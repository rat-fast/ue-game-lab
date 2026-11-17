#include "SkillLabSkillPresentationRouter.h"

#include "Engine/Engine.h"
#include "SkillLabSkillFeedbackActor.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabProjectileTypes.h"
#include "Engine/StaticMesh.h"

UWorld* USkillLabSkillPresentationRouter::GetWorld() const
{
	return Source ? Source->GetWorld() : nullptr;
}

void USkillLabSkillPresentationRouter::Bind(USkillLabSkillRuntimeComponent* InSource)
{
	if (Source == InSource) return;
	for (const auto& Pair : PersistentMarkers) if (Pair.Value.IsValid()) Pair.Value->Destroy();
	PersistentMarkers.Reset();
	if (Source) Source->OnNativeSkillEvent.RemoveAll(this);
	Source = InSource;
	if (Source) Source->OnNativeSkillEvent.AddUObject(this, &USkillLabSkillPresentationRouter::HandleEvent);
}

void USkillLabSkillPresentationRouter::HandleEvent(const FSkillLabSkillResult& Result)
{
	if (Result.Event == ESkillLabSkillEvent::BuffRemoved && !Result.EffectInstanceId.IsNone())
	{
		const FString Prefix = Result.EffectInstanceId.ToString() + TEXT(":");
		for (auto It = PersistentMarkers.CreateIterator(); It; ++It)
			if (It.Key().StartsWith(Prefix)) { if (It.Value().IsValid()) It.Value()->Destroy(); It.RemoveCurrent(); }
	}
	TGuardValue<bool> ProducedGuard(bSpatialProduced, false);
	OnPresentationEvent.Broadcast(Result);
	UWorld* World = GetWorld();
	if (!World) return;
	ShowScreenMessage(Result);
	if (bSpatialProduced) return;
	if (Result.Event == ESkillLabSkillEvent::TargetSelected && Result.VisualRadius > 0.0f)
	{
		SpawnWorldMarker(Result.TargetLocation + FVector(0.0f, 0.0f, 3.0f), Result.VisualRadius, FColor::Orange, 1.5f, true, Result);
	}
	else if ((Result.Event == ESkillLabSkillEvent::Hit || Result.Event == ESkillLabSkillEvent::DamageTick)
		&& Result.bSuccess && Result.SourceActor && Result.TargetActor)
	{
		const FColor Color = Result.Effect == ESkillLabEffectKind::Healing ? FColor::Green : FColor::Red;
		// A short-lived white target frame makes the final hit location legible even with no assets.
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 76.0f, FColor::White, 0.28f, false, Result);
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 70.0f, Color, 0.65f, false, Result);
		const FString Label = Result.Effect == ESkillLabEffectKind::Healing
			? (Result.ActualAmount > 0.0f
				? FString::Printf(TEXT("HEAL +%.0f"), Result.ActualAmount)
				: FString(TEXT("HEAL FULL")))
			: FString::Printf(TEXT("DAMAGE -%.0f"), Result.ActualAmount);
		SpawnTextMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 190.0f), Label, Color,
			Result.Event == ESkillLabSkillEvent::DamageTick ? 0.9f : 1.5f, Result);
	}
	else if (Result.Event == ESkillLabSkillEvent::BuffApplied && Result.TargetActor)
	{
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 90.0f, FColor::Purple, 4.0f, false, Result);
		SpawnTextMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 205.0f), TEXT("BURNING"), FColor::Purple, 2.0f, Result);
	}
}

void USkillLabSkillPresentationRouter::ShowSpatial(const FSkillLabSkillResult& Result)
{
	const FName Event = Result.Event == ESkillLabSkillEvent::TargetSelected ? FName(TEXT("TargetSelected"))
		: Result.Event == ESkillLabSkillEvent::Hit ? FName(TEXT("Hit"))
		: Result.Event == ESkillLabSkillEvent::BuffApplied ? FName(TEXT("BuffApplied")) : NAME_None;
	if (Event.IsNone() || !Result.bSuccess || !GetWorld()) return;
	for (const auto& D : Result.SpatialDefinitions)
	{
		if (D.Object != ESkillLabSpatialObject::Marker || D.Event != Event) continue;
		if (D.bFollow && Result.EffectInstanceId.IsNone())
		{ UE_LOG(LogTemp, Warning, TEXT("[SkillLabSpatial] Skill=%s Object=%s: missing effect identity"), *Result.SkillId.ToString(), *D.Name.ToString()); continue; }
		const FVector Destination = IsValid(Result.TargetActor) ? GetSkillLabProjectileAimPoint(Result.TargetActor) : Result.TargetLocation;
		const FVector Direction = Destination - GetSkillLabProjectileAimPoint(Result.SourceActor);
		FTransform Transform; FString Error;
		if (!FSkillLabSpatialTransform::Resolve(D, Result.SourceActor, Result.TargetActor, Result.RelatedActor.Get(), Result.TargetLocation, Direction, Transform, Error))
		{ UE_LOG(LogTemp, Warning, TEXT("[SkillLabSpatial] Skill=%s Object=%s: %s"), *Result.SkillId.ToString(), *D.Name.ToString(), *Error); continue; }
		FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UStaticMesh* Mesh = D.VisualMesh.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *D.VisualMesh);
		if (!D.VisualMesh.IsEmpty() && !Mesh)
		{ UE_LOG(LogTemp, Warning, TEXT("[SkillLabSpatial] Skill=%s Object=%s: mesh %s unavailable"), *Result.SkillId.ToString(), *D.Name.ToString(), *D.VisualMesh); continue; }
		const FQuat Rotation = Transform.GetRotation() * D.VisualRotation.Quaternion();
		auto* Marker = GetWorld()->SpawnActor<ASkillLabSkillFeedbackActor>(ASkillLabSkillFeedbackActor::StaticClass(), Transform.GetLocation(), Rotation.Rotator(), Params);
		if (!Marker) { UE_LOG(LogTemp, Warning, TEXT("[SkillLabSpatial] Skill=%s Object=%s: spawn failed"), *Result.SkillId.ToString(), *D.Name.ToString()); continue; }
		Marker->InitializeShape(Event == TEXT("TargetSelected") ? ESkillLabFeedbackShape::Ring : ESkillLabFeedbackShape::TargetFrame,
			(Result.VisualRadius > 0 ? Result.VisualRadius : 70.0f) * D.VisualScale, FColor::Purple, 1.5f);
		Marker->SetObservationSource(Result, D.Name, Result.VisualRadius > 0 ? Result.VisualRadius : 70.0f, D.VisualScale);
		if (Mesh) Marker->SetSpatialMesh(Mesh);
		bSpatialProduced = true;
		if (D.bFollow)
		{
			const FString Key = Result.EffectInstanceId.ToString() + TEXT(":") + D.Name.ToString();
			if (auto* Existing = PersistentMarkers.Find(Key); Existing && Existing->IsValid())
			{
				(*Existing)->SetObservedRemovalCause(TEXT("ReplacedByNewEffect"));
				(*Existing)->Destroy();
			}
			Marker->StartSpatialFollow(D, Result.SourceActor, Result.TargetActor, Result.TargetLocation, Rotation);
			PersistentMarkers.Add(Key, Marker);
		}
		UE_LOG(LogTemp, Log, TEXT("[SkillLabSpatial] Skill=%s Object=%s Event=%s Location=%s Scale=%.2f"), *Result.SkillId.ToString(), *D.Name.ToString(), *Event.ToString(), *Transform.GetLocation().ToString(), D.VisualScale);
	}
}

void USkillLabSkillPresentationRouter::ShowScreenMessage(const FSkillLabSkillResult& Result)
{
	if (!GEngine) return;
	FColor Color = FColor::White;
	FString Message;
	switch (Result.Event)
	{
	case ESkillLabSkillEvent::Started:
		Color = FColor::Yellow;
		Message = FString::Printf(TEXT("CAST  %s"), *Result.SkillId.ToString());
		break;
	case ESkillLabSkillEvent::Hit:
		Color = Result.bSuccess ? FColor::Green : FColor::Red;
		Message = Result.bSuccess
			? FString::Printf(TEXT("HIT  %s  %s%.0f"), *Result.SkillId.ToString(), Result.Effect == ESkillLabEffectKind::Healing ? TEXT("+") : TEXT("-"), Result.ActualAmount)
			: FString::Printf(TEXT("MISS  %s  %s"), *Result.SkillId.ToString(), *Result.Reason.ToString());
		break;
	case ESkillLabSkillEvent::DamageTick:
		Color = Result.bSuccess ? FColor::Orange : FColor::Red;
		Message = Result.bSuccess
			? FString::Printf(TEXT("TICK  %s  -%.0f"), *Result.SkillId.ToString(), Result.ActualAmount)
			: FString::Printf(TEXT("TICK  %s  %s"), *Result.SkillId.ToString(), *Result.Reason.ToString());
		break;
	case ESkillLabSkillEvent::BuffApplied:
		Color = FColor::Purple;
		Message = FString::Printf(TEXT("BUFF  %s"), *Result.SkillId.ToString());
		break;
	case ESkillLabSkillEvent::Rejected:
		Color = FColor::Red;
		Message = FString::Printf(TEXT("REJECTED  %s"), *Result.Reason.ToString());
		break;
	default:
		break;
	}
	if (!Message.IsEmpty()) GEngine->AddOnScreenDebugMessage(-1, 2.0f, Color, Message);
}

void USkillLabSkillPresentationRouter::SpawnWorldMarker(const FVector& Location, const float Radius, const FColor& Color, const float LifeSeconds, const bool bFlat, const FSkillLabSkillResult& Context)
{
	UWorld* World = GetWorld();
	if (!World) return;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkillLabSkillFeedbackActor* Marker = World->SpawnActor<ASkillLabSkillFeedbackActor>(ASkillLabSkillFeedbackActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
	if (Marker)
	{
		const ESkillLabFeedbackShape Shape = bFlat
			? ESkillLabFeedbackShape::Ring
			: (Color == FColor::White ? ESkillLabFeedbackShape::TargetFrame : ESkillLabFeedbackShape::Target);
		Marker->InitializeShape(Shape, Radius, Color, LifeSeconds);
		Marker->SetObservationSource(Context, NAME_None, Radius);
	}
}

void USkillLabSkillPresentationRouter::SpawnTextMarker(const FVector& Location, const FString& Text, const FColor& Color, const float LifeSeconds, const FSkillLabSkillResult& Context)
{
	UWorld* World = GetWorld();
	if (!World) return;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkillLabSkillFeedbackActor* Marker = World->SpawnActor<ASkillLabSkillFeedbackActor>(ASkillLabSkillFeedbackActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
	if (Marker) { Marker->InitializeText(Text, Color, LifeSeconds); Marker->SetObservationSource(Context); }
}
