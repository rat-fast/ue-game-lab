#include "SkillLabSpatialExportSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UnrealClient.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillObservation.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillAnimationComponent.h"
#include "SkillLabSkillFeedbackActor.h"
#include "SkillLabFireballProjectile.h"

#include "SkillLabSpatialExportJson.h"
using namespace SkillLabSpatialExport;

void USkillLabSpatialExportSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GetWorld() && GetWorld()->WorldType == EWorldType::Game
		&& FParse::Value(FCommandLine::Get(), TEXT("SkillLabSpatialExportRequest="), RequestPath)
		&& FParse::Value(FCommandLine::Get(), TEXT("SkillLabSpatialExportOutput="), OutputDirectory)
		&& FParse::Value(FCommandLine::Get(), TEXT("SkillLabSpatialExportMode="), Mode))
	{
		State = EState::WaitingWorld;
		SessionStartWall = FPlatformTime::Seconds();
		SessionStartedAt = UtcNow();
		SetPhase(TEXT("request_loading"));
		FParse::Value(FCommandLine::Get(), TEXT("SkillLabSpatialExportRunId="), RunId);
		if (!LoadRequest()) Fail(LastError);
		else SetPhase(TEXT("waiting_for_world"));
	}
}

void USkillLabSpatialExportSubsystem::Deinitialize()
{
	if (Runtime.IsValid() && SkillEventHandle.IsValid()) Runtime->OnNativeSkillEvent.Remove(SkillEventHandle);
	if (ProjectileHandle.IsValid()) FSkillLabSkillObservationBus::OnProjectileObserved().Remove(ProjectileHandle);
	if (FeedbackHandle.IsValid()) FSkillLabFeedbackObservationBus::OnObserved().Remove(FeedbackHandle);
	FWorldDelegates::OnWorldTickEnd.Remove(WorldTickHandle);
	if (AnimationComponent.IsValid() && AnimationNotifyHandle.IsValid()) AnimationComponent->OnNotifyObserved.Remove(AnimationNotifyHandle);
	ReleaseCaptureResources();
	Super::Deinitialize();
}

void USkillLabSpatialExportSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (State != EState::Disabled) WorldTickHandle = FWorldDelegates::OnWorldTickEnd.AddLambda([this](UWorld* World, ELevelTick, float) { if (World == GetWorld() && IsTickable()) AdvanceSession(); });
	if (State == EState::WaitingWorld) { State = EState::Warmup; SetPhase(TEXT("warmup")); }
}

bool USkillLabSpatialExportSubsystem::IsTickable() const
{
	return State != EState::Disabled && State != EState::Finished;
}

TStatId USkillLabSpatialExportSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USkillLabSpatialExportSubsystem, STATGROUP_Tickables);
}

bool USkillLabSpatialExportSubsystem::LoadRequest()
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *RequestPath)) { LastError = FString::Printf(TEXT("request file missing: %s"), *RequestPath); return false; }
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Request) || !Request.IsValid())
	{ LastError = TEXT("request JSON is invalid"); return false; }
	Request->TryGetNumberField(TEXT("warmup_seconds"), WarmupSeconds);
	Request->TryGetNumberField(TEXT("pre_request_seconds"), PreRequestSeconds);
	Request->TryGetNumberField(TEXT("duration_seconds"), DurationSeconds);
	Request->TryGetNumberField(TEXT("timeout_seconds"), TimeoutSeconds);
	double SampleHz = 30.0;
	Request->TryGetNumberField(TEXT("sample_hz"), SampleHz);
	if (SampleHz > 0.0) SampleInterval = 1.0 / SampleHz;
	Request->TryGetStringField(TEXT("run_id"), RunId);
	return true;
}

void USkillLabSpatialExportSubsystem::Tick(float DeltaTime)
{
	if (State == EState::WaitingWorld || State == EState::Disabled || State == EState::Finished) return;
	if (FPlatformTime::Seconds() - SessionStartWall > TimeoutSeconds)
	{
		Fail(TEXT("UE session safety deadline exceeded"));
		return;
	}
	// 会话在 World tick 末尾推进，等待原 Actor/动画更新完成。
}

void USkillLabSpatialExportSubsystem::AdvanceSession()
{
	if (State == EState::Warmup)
	{
		ResolveActorsAndRuntime();
		if (State == EState::Finished) return;
		if (!Runtime.IsValid() || !Runtime->IsInitialized()) return;
		if (!bRuntimeReadyObserved)
		{
			bRuntimeReadyObserved = true;
			WarmupStartWorldTime = GetWorld()->GetTimeSeconds();
		}
		if (GetWorld()->GetTimeSeconds() - WarmupStartWorldTime < WarmupSeconds) return;
		SetPhase(TEXT("configuration"));
		WriteConfigurationAndReferences();
		if (bConfigurationWriteFailed) { Fail(TEXT("failed to write configuration or references")); return; }
		if (Mode == TEXT("inspect")) { Finish(true); return; }
		State = EState::PreRequest;
		SetPhase(TEXT("pre_request_sampling"));
		PreRequestStartWall = FPlatformTime::Seconds();
		NextSampleWall = PreRequestStartWall;
		SkillEventHandle = Runtime->OnNativeSkillEvent.AddLambda([this](const FSkillLabSkillResult& Event) { RecordSkillEvent(Event); });
		ProjectileHandle = FSkillLabSkillObservationBus::OnProjectileObserved().AddUObject(this, &USkillLabSpatialExportSubsystem::RecordProjectile);
		FeedbackHandle = FSkillLabFeedbackObservationBus::OnObserved().AddUObject(this, &USkillLabSpatialExportSubsystem::RecordFeedbackObservation);
		AnimationComponent = Caster->FindComponentByClass<USkillLabSkillAnimationComponent>();
		if (AnimationComponent.IsValid()) AnimationNotifyHandle = AnimationComponent->OnNotifyObserved.AddUObject(this, &USkillLabSpatialExportSubsystem::RecordAnimationNotify);
	}
	if (State == EState::PreRequest)
	{
		if (FPlatformTime::Seconds() >= NextSampleWall)
		{
			CaptureSample();
			NextSampleWall += SampleInterval;
		}
		if (FPlatformTime::Seconds() - PreRequestStartWall >= PreRequestSeconds) SubmitRequest();
		return;
	}
	if (State == EState::Capturing)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now >= NextSampleWall)
		{
			CaptureSample();
			NextSampleWall = Now + SampleInterval;
		}
		if (GetWorld()->GetTimeSeconds() - RequestWorldTime >= DurationSeconds) Finish(true);
	}
}

AActor* USkillLabSpatialExportSubsystem::ResolveActor(const TSharedPtr<FJsonObject>& Locator, const FString& Label, const bool bRequired)
{
	if (!Locator.IsValid()) return bRequired ? nullptr : nullptr;
	double PlayerIndex = -1.0;
	FString Path;
	if (Locator->TryGetNumberField(TEXT("player_index"), PlayerIndex))
	{
		if (PlayerIndex >= 0.0 && PlayerIndex <= MAX_int32)
			if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), static_cast<int32>(PlayerIndex))) return Pawn;
		Errors.Add(FString::Printf(TEXT("%s player_index has no Pawn"), *Label));
		return nullptr;
	}
	if (!Locator->TryGetStringField(TEXT("level_actor_path"), Path))
	{
		if (bRequired) Errors.Add(FString::Printf(TEXT("%s locator is missing"), *Label));
		return nullptr;
	}
	TArray<AActor*> Matches;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		if (It->GetPathName() == Path) Matches.Add(*It);
	if (Matches.Num() != 1)
	{
		TArray<FString> Candidates;
		const FString RequestedName = FPaths::GetBaseFilename(Path);
		for (TActorIterator<AActor> It(GetWorld()); It && Candidates.Num() < 12; ++It)
			if (It->GetName().Contains(RequestedName, ESearchCase::IgnoreCase)) Candidates.Add(It->GetPathName());
		Errors.Add(FString::Printf(TEXT("%s exact path matched %d actors: %s; candidate_actor_paths=[%s]"),
			*Label, Matches.Num(), *Path, *FString::Join(Candidates, TEXT(", "))));
		return nullptr;
	}
	return Matches[0];
}

void USkillLabSpatialExportSubsystem::ResolveActorsAndRuntime()
{
	if (Caster.IsValid() && Runtime.IsValid()) return;
	if (!Request.IsValid()) { Fail(TEXT("request was not loaded")); return; }
	const TSharedPtr<FJsonObject>* CasterLocator = nullptr;
	if (!Request->TryGetObjectField(TEXT("caster"), CasterLocator) || !CasterLocator)
	{ Fail(TEXT("caster locator is missing")); return; }
	Caster = ResolveActor(*CasterLocator, TEXT("caster"), true);
	if (!Caster.IsValid()) { Fail(Errors.IsEmpty() ? TEXT("caster could not be resolved") : Errors.Last()); return; }
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Request->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		const TSharedPtr<FJsonObject>* ActorLocator = nullptr;
		if ((*TargetObject)->TryGetObjectField(TEXT("actor"), ActorLocator) && ActorLocator)
		{
			Target = ResolveActor(*ActorLocator, TEXT("target.actor"), false);
			if (!Target.IsValid()) { Fail(Errors.IsEmpty() ? TEXT("target.actor could not be resolved") : Errors.Last()); return; }
		}
	}
	const TSharedPtr<FJsonObject>* RelatedObject = nullptr;
	if (Request->TryGetObjectField(TEXT("related_actor"), RelatedObject) && RelatedObject && RelatedObject->IsValid())
	{
		RelatedActor = ResolveActor(*RelatedObject, TEXT("related_actor"), false);
		if (!RelatedActor.IsValid()) { Fail(Errors.IsEmpty() ? TEXT("related_actor could not be resolved") : Errors.Last()); return; }
	}
	TArray<USkillLabSkillRuntimeComponent*> Runtimes;
	Caster->GetComponents<USkillLabSkillRuntimeComponent>(Runtimes);
	if (Runtimes.Num() != 1)
	{
		Fail(FString::Printf(TEXT("caster must have exactly one Skill Runtime; found %d"), Runtimes.Num()));
		return;
	}
	Runtime = Runtimes[0];
	FString SelectedSkillId;
	if (Request->TryGetStringField(TEXT("skill_id"), SelectedSkillId) && !SelectedSkillId.IsEmpty())
	{
		TArray<FName> Granted;
		Runtime->GetGrantedSkillIds(Granted);
		FSkillLabSkillDefinition Definition;
		if (!Granted.Contains(FName(*SelectedSkillId)) || !FSkillLabSkillDefinitionProvider::Resolve(FName(*SelectedSkillId), Definition))
		{
			Fail(FString::Printf(TEXT("selected skill is not granted with an active definition: %s"), *SelectedSkillId));
			return;
		}
		bProjectileExpected = Definition.Execution == ESkillLabExecutionTemplate::Projectile;
		if (Definition.TargetShape == ESkillLabTargetShape::Self && !Target.IsValid()) Target = Caster;
	}
	for (const auto& Weak : {Caster, Target, RelatedActor}) if (AActor* Actor = Weak.Get())
	{
		TArray<USkeletalMeshComponent*> Meshes; Actor->GetComponents(Meshes);
		for (auto* Mesh : Meshes) if (!PoseHandles.Contains(Mesh))
			PoseHandles.Add(Mesh, Mesh->RegisterOnBoneTransformsFinalizedDelegate(FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateLambda([this, WeakMesh = TWeakObjectPtr<USkeletalMeshComponent>(Mesh)]() { PoseFinalizedFrames.Add(WeakMesh, GFrameCounter); })));
	}
}

void USkillLabSpatialExportSubsystem::SubmitRequest()
{
	SetPhase(TEXT("request_submission"));
	RequestWallTime = FPlatformTime::Seconds();
	RequestWorldTime = GetWorld()->GetTimeSeconds();
	RequestStartSampleIndex = SampleIndex;
	FSkillLabSkillRequest SkillRequest;
	SkillRequest.SkillId = FName(*Request->GetStringField(TEXT("skill_id")));
	SkillRequest.Caster = Caster.Get();
	SkillRequest.Target = Target.Get();
	SkillRequest.RelatedActor = RelatedActor;
	const TSharedPtr<FJsonObject>* TargetObject = nullptr;
	if (Request->TryGetObjectField(TEXT("target"), TargetObject) && TargetObject && TargetObject->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Ground = nullptr;
		if ((*TargetObject)->TryGetArrayField(TEXT("ground_location"), Ground) && Ground && Ground->Num() == 3)
		{
			SkillRequest.bHasLocation = true;
			SkillRequest.TargetLocation = FVector((*Ground)[0]->AsNumber(), (*Ground)[1]->AsNumber(), (*Ground)[2]->AsNumber());
		}
	}
	bSubmittingRequest = true;
	const FSkillLabSkillResult Result = Runtime->SubmitSkillRequest(SkillRequest);
	bSubmittingRequest = false;
	RequestId = Result.RequestId;
	CapturedCastId = Result.CastId;
	bRequestAccepted = Result.bSuccess || Result.CastId > 0;
	TSharedRef<FJsonObject> Return = MakeShared<FJsonObject>();
	Return->SetStringField(TEXT("event"), TEXT("request_return"));
	Return->SetNumberField(TEXT("request_id"), RequestId);
	Return->SetStringField(TEXT("skill_id"), SkillRequest.SkillId.ToString());
	Return->SetBoolField(TEXT("accepted"), bRequestAccepted);
	Return->SetBoolField(TEXT("released"), Result.bHasReleased);
	Return->SetNumberField(TEXT("cast_id"), Result.CastId);
	Return->SetStringField(TEXT("reason"), Result.Reason.ToString());
	Return->SetNumberField(TEXT("wall_seconds"), FPlatformTime::Seconds() - SessionStartWall);
	AppendJsonLine(TEXT("events.jsonl"), Return);
	if (!bRequestAccepted) { Fail(FString::Printf(TEXT("skill request rejected: %s"), *Result.Reason.ToString())); return; }
	State = EState::Capturing;
	SetPhase(TEXT("capture_sampling"));
	NextSampleWall = FPlatformTime::Seconds();
}

void USkillLabSpatialExportSubsystem::AppendJsonLine(const FString& RelativePath, const TSharedRef<FJsonObject>& Object)
{
	const FString Path = FPaths::Combine(OutputDirectory, RelativePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(JsonLineText(Object) + TEXT("\n"), *Path,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		Errors.AddUnique(FString::Printf(TEXT("failed to append JSONL record: %s"), *RelativePath));
	}
}

void USkillLabSpatialExportSubsystem::Fail(const FString& Error)
{
	if (FailurePhase.IsEmpty()) FailurePhase = CurrentPhase;
	Errors.AddUnique(Error);
	LastError = Error;
	Finish(false);
}

void USkillLabSpatialExportSubsystem::Finish(const bool bComplete)
{
	SetPhase(TEXT("finalizing"));
	if (Mode == TEXT("capture")) FinalizeObservedObjectStates();
	if (Runtime.IsValid() && SkillEventHandle.IsValid()) Runtime->OnNativeSkillEvent.Remove(SkillEventHandle);
	if (ProjectileHandle.IsValid()) FSkillLabSkillObservationBus::OnProjectileObserved().Remove(ProjectileHandle);
	if (FeedbackHandle.IsValid()) FSkillLabFeedbackObservationBus::OnObserved().Remove(FeedbackHandle);
	WriteReceipt(bComplete);
	State = EState::Finished;
	FPlatformMisc::RequestExit(false);
}

void USkillLabSpatialExportSubsystem::SetPhase(const FString& Phase)
{
	CurrentPhase = Phase;
	if (OutputDirectory.IsEmpty()) return;
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("phase"), CurrentPhase);
	Json->SetNumberField(TEXT("wall_seconds_since_start"), FPlatformTime::Seconds() - SessionStartWall);
	Json->SetStringField(TEXT("updated_at"), UtcNow());
	if (!FFileHelper::SaveStringToFile(JsonText(Json), *FPaths::Combine(OutputDirectory, TEXT("ue_phase.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		Errors.AddUnique(TEXT("failed to write ue_phase.json"));
		UE_LOG(LogTemp, Error, TEXT("[SpatialExport] Failed to write phase checkpoint"));
	}
}

void USkillLabSpatialExportSubsystem::FinalizeObservedObjectStates()
{
	if (bProjectileExpected && ObservedProjectiles.IsEmpty())
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("event"), TEXT("projectile_not_observed_at_capture_end"));
		Json->SetStringField(TEXT("status"), TEXT("not_observed"));
		Json->SetNumberField(TEXT("request_id"), RequestId);
		Json->SetNumberField(TEXT("cast_id"), CapturedCastId);
		Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(CapturedRuntimeGeneration));
		AppendJsonLine(TEXT("events.jsonl"), Json);
	}
	for (const auto& Pair : ObservedProjectilePaths)
	{
		if (RemovedProjectilePaths.Contains(Pair.Value)) continue;
		ASkillLabFireballProjectile* Projectile = Pair.Key.Get();
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("event"), TEXT("projectile_capture_cutoff"));
		Json->SetStringField(TEXT("actor_path"), Pair.Value);
		Json->SetStringField(TEXT("status"), IsValid(Projectile) ? TEXT("alive_at_capture_end") : TEXT("observation_lost"));
		Json->SetNumberField(TEXT("request_id"), RequestId);
		Json->SetNumberField(TEXT("cast_id"), CapturedCastId);
		Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(CapturedRuntimeGeneration));
		if (const FGuid* Id = ObservedProjectileSubmissionIds.Find(Pair.Key)) Json->SetStringField(TEXT("submission_key"), Id->ToString());
		if (IsValid(Projectile)) Json->SetObjectField(TEXT("transform"), TransformJson(Projectile->GetActorTransform()));
		AppendJsonLine(TEXT("events.jsonl"), Json);
	}
	for (const auto& Pair : ObservedFeedbackPaths)
	{
		if (RemovedFeedbackPaths.Contains(Pair.Value)) continue;
		ASkillLabSkillFeedbackActor* Actor = Pair.Key.Get();
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		Json->SetStringField(TEXT("event"), TEXT("feedback_capture_cutoff"));
		Json->SetStringField(TEXT("actor_path"), Pair.Value);
		Json->SetStringField(TEXT("status"), IsValid(Actor) ? TEXT("alive_at_capture_end") : TEXT("observation_lost"));
		Json->SetNumberField(TEXT("request_id"), RequestId);
		Json->SetNumberField(TEXT("cast_id"), CapturedCastId);
		Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(CapturedRuntimeGeneration));
		if (const FGuid* Id = ObservedFeedbackSubmissionIds.Find(Pair.Key)) Json->SetStringField(TEXT("submission_key"), Id->ToString());
		if (IsValid(Actor)) Json->SetObjectField(TEXT("transform"), TransformJson(Actor->GetActorTransform()));
		AppendJsonLine(TEXT("events.jsonl"), Json);
	}
}

void USkillLabSpatialExportSubsystem::WriteReceipt(const bool bComplete)
{
	if (Mode == TEXT("capture"))
	{

	}
	TSharedRef<FJsonObject> Receipt = MakeShared<FJsonObject>();
	Receipt->SetBoolField(TEXT("complete"), bComplete && Errors.IsEmpty());
	Receipt->SetBoolField(TEXT("request_accepted"), bRequestAccepted);
	Receipt->SetStringField(TEXT("started_at"), SessionStartedAt);
	Receipt->SetStringField(TEXT("ended_at"), UtcNow());
	Receipt->SetStringField(TEXT("phase"), CurrentPhase);
	if (!FailurePhase.IsEmpty()) Receipt->SetStringField(TEXT("failure_phase"), FailurePhase);
	Receipt->SetNumberField(TEXT("sample_count"), SampleIndex);
	Receipt->SetNumberField(TEXT("request_id"), RequestId);
	Receipt->SetArrayField(TEXT("errors"), [&]() { TArray<TSharedPtr<FJsonValue>> Values; for (const FString& Error : Errors) Values.Add(MakeShared<FJsonValueString>(Error)); return Values; }());
	TArray<TSharedPtr<FJsonValue>> Limitations;
	if (Mode == TEXT("capture"))
	{

	}
	Receipt->SetArrayField(TEXT("limitations"), Limitations);
	TSharedRef<FJsonObject> Coverage = MakeShared<FJsonObject>();
	Coverage->SetNumberField(TEXT("samples"), SampleIndex);
	Coverage->SetNumberField(TEXT("capture_samples"), FMath::Max(0, SampleIndex - RequestStartSampleIndex));
	Coverage->SetNumberField(TEXT("duration_seconds"), DurationSeconds);
	if (SampleIndex > 0)
	{
		Coverage->SetNumberField(TEXT("sample_coverage_start_world_seconds"), FirstSampleWorldTime);
		Coverage->SetNumberField(TEXT("sample_coverage_end_world_seconds"), LastSampleWorldTime);
		Coverage->SetNumberField(TEXT("sample_coverage_world_duration_seconds"), FMath::Max(0.0, LastSampleWorldTime - FirstSampleWorldTime));
		Coverage->SetNumberField(TEXT("sample_coverage_start_wall_seconds"), FirstSampleWallTime - SessionStartWall);
		Coverage->SetNumberField(TEXT("sample_coverage_end_wall_seconds"), LastSampleWallTime - SessionStartWall);
		Coverage->SetNumberField(TEXT("sample_coverage_wall_duration_seconds"), FMath::Max(0.0, LastSampleWallTime - FirstSampleWallTime));
	}
	else
	{
		for (const TCHAR* Field : {TEXT("sample_coverage_start_world_seconds"), TEXT("sample_coverage_end_world_seconds"),
			TEXT("sample_coverage_world_duration_seconds"), TEXT("sample_coverage_start_wall_seconds"),
			TEXT("sample_coverage_end_wall_seconds"), TEXT("sample_coverage_wall_duration_seconds")})
			Coverage->SetField(Field, MakeShared<FJsonValueNull>());
	}
	if (bRequestAccepted && SampleIndex > RequestStartSampleIndex)
	{
		Coverage->SetNumberField(TEXT("request_coverage_start_world_seconds"), RequestWorldTime);
		Coverage->SetNumberField(TEXT("request_coverage_end_world_seconds"), LastSampleWorldTime);
		Coverage->SetNumberField(TEXT("request_coverage_world_duration_seconds"), FMath::Max(0.0, LastSampleWorldTime - RequestWorldTime));
	}
	else
	{
		Coverage->SetField(TEXT("request_coverage_start_world_seconds"), MakeShared<FJsonValueNull>());
		Coverage->SetField(TEXT("request_coverage_end_world_seconds"), MakeShared<FJsonValueNull>());
		Coverage->SetField(TEXT("request_coverage_world_duration_seconds"), MakeShared<FJsonValueNull>());
	}
	Coverage->SetNumberField(TEXT("missed_expected_samples"), MissedExpectedSamples);
	const double ObservedCoverage = FMath::Max(0.0, LastSampleWorldTime - RequestWorldTime);
	Coverage->SetNumberField(TEXT("actual_sample_hz"), ObservedCoverage > 0.0 ? (SampleIndex - RequestStartSampleIndex) / ObservedCoverage : 0.0);
	Receipt->SetObjectField(TEXT("coverage"), Coverage);
	const FString Path = FPaths::Combine(OutputDirectory, TEXT("ue_receipt.json"));
	const FString Temporary = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(JsonText(Receipt), *Temporary)
		|| !IFileManager::Get().Move(*Path, *Temporary, true, true, false, true))
		UE_LOG(LogTemp, Error, TEXT("[SpatialExport] Failed to publish receipt at %s"), *Path);
	if (!Errors.IsEmpty())
	{
		FString Diagnostics;
		for (const FString& Error : Errors) Diagnostics += Error + LINE_TERMINATOR;
		FFileHelper::SaveStringToFile(Diagnostics, *FPaths::Combine(OutputDirectory, TEXT("diagnostics.txt")));
	}
}
