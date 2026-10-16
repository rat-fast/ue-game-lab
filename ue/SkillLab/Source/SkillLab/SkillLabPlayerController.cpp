// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabPlayerController.h"

#include "SkillLabCharacter.h"
#include "SkillLabAnimInstance.h"
#include "SkillLabHUDManagerComponent.h"
#include "SkillLabPlayerInputComponent.h"
#include "SkillLabSkillInputComponent.h"
#include "SkillLabSkillShortcutComponent.h"
#include "SkillLabTestCommandsComponent.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"
#include "Components/SkeletalMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Parse.h"

ASkillLabPlayerController::ASkillLabPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	HUDManager = CreateDefaultSubobject<USkillLabHUDManagerComponent>(TEXT("HUDManager"));
	InputAdapter = CreateDefaultSubobject<USkillLabPlayerInputComponent>(TEXT("InputAdapter"));
	SkillInput = CreateDefaultSubobject<USkillLabSkillInputComponent>(TEXT("SkillInput"));
	SkillShortcuts = CreateDefaultSubobject<USkillLabSkillShortcutComponent>(TEXT("SkillShortcuts"));
	TestCommands = CreateDefaultSubobject<USkillLabTestCommandsComponent>(TEXT("TestCommands"));
}

void ASkillLabPlayerController::SkillTest()
{
	if (TestCommands) TestCommands->SkillTest();
}

void ASkillLabPlayerController::SkillTestRecover()
{
	if (TestCommands) TestCommands->SkillTestRecover();
}

void ASkillLabPlayerController::SkillArea() { if (TestCommands) TestCommands->SkillArea(); }
void ASkillLabPlayerController::SkillBurn() { if (TestCommands) TestCommands->SkillBurn(); }

void ASkillLabPlayerController::SkillSpawnTargets()
{
	if (TestCommands) TestCommands->SpawnTargets();
}

void ASkillLabPlayerController::SkillReset()
{
	if (TestCommands) TestCommands->ResetSkills();
}

void ASkillLabPlayerController::SkillReloadDefinitions()
{
	if (TestCommands) TestCommands->ReloadDefinitions();
}

void ASkillLabPlayerController::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] PlayerController BeginPlay controller=%s pawn=%s input=%s"),
		*GetNameSafe(this), *GetNameSafe(GetPawn()), *GetNameSafe(InputComponent));
	if (SkillInput) SkillInput->InitializeForPlayer(this, GetPawn());
	if (TestCommands) TestCommands->InitializeForPawn(GetPawn(), SkillShortcuts);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (HUDManager) HUDManager->InitializeForPlayer(this, GetPawn(), SkillShortcuts);
	if (InputAdapter) InputAdapter->ActivateInputMapping();
	if (FParse::Param(FCommandLine::Get(), TEXT("SkillLabRuntimeProbe"))) StartRuntimeProbe();
}

void ASkillLabPlayerController::StartRuntimeProbe()
{
	if (bRuntimeProbeStarted || !GetWorld() || !GetPawn()) return;
	bRuntimeProbeStarted = true;
	for (TActorIterator<ASkillLabCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != GetPawn())
		{
			RuntimeProbeTarget = *It;
			RuntimeProbeTarget->SetActorLocation(GetPawn()->GetActorLocation() + FVector(300.0f, 0.0f, 0.0f));
			break;
		}
	}
	if (USkillLabSkillRuntimeComponent* Runtime = GetPawn()->FindComponentByClass<USkillLabSkillRuntimeComponent>())
		Runtime->OnNativeSkillEvent.AddUObject(this, &ASkillLabPlayerController::HandleRuntimeProbeSkillEvent);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabRuntimeProbe] START target=%s"), *GetNameSafe(RuntimeProbeTarget.Get()));
	LogRuntimeProbeSample(TEXT("Idle"));
	GetWorld()->GetTimerManager().SetTimer(RuntimeProbeTimer, this, &ASkillLabPlayerController::TickRuntimeProbe, 0.10f, true, 0.10f);
}

void ASkillLabPlayerController::LogRuntimeProbeSample(const TCHAR* Label)
{
	ASkillLabCharacter* ProbeCharacter = Cast<ASkillLabCharacter>(GetPawn());
	USkillLabAnimInstance* Anim = ProbeCharacter && ProbeCharacter->GetMesh() ? Cast<USkillLabAnimInstance>(ProbeCharacter->GetMesh()->GetAnimInstance()) : nullptr;
	if (!ProbeCharacter || !Anim) { UE_LOG(LogTemp, Warning, TEXT("[SkillLabRuntimeProbe] SAMPLE label=%s anim=null"), Label); return; }
	const int32 PelvisIndex = ProbeCharacter->GetMesh()->GetBoneIndex(TEXT("pelvis"));
	const FVector Pelvis = PelvisIndex != INDEX_NONE ? ProbeCharacter->GetMesh()->GetBoneTransform(PelvisIndex).GetLocation() : FVector::ZeroVector;
	const FVector Root = ProbeCharacter->GetMesh()->GetSocketTransform(TEXT("root"), RTS_World).GetLocation();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabRuntimeProbe] SAMPLE label=%s t=%.2f vel=(%.1f,%.1f,%.1f) accel=(%.1f,%.1f,%.1f) speed=%.3f dir=%.3f move=%s falling=%s jump=%s ik=%s pelvis=(%.1f,%.1f,%.1f) root=(%.1f,%.1f,%.1f)"),
		Label, RuntimeProbeElapsed, Anim->Velocity.X, Anim->Velocity.Y, Anim->Velocity.Z, ProbeCharacter->GetCharacterMovement()->GetCurrentAcceleration().X, ProbeCharacter->GetCharacterMovement()->GetCurrentAcceleration().Y, ProbeCharacter->GetCharacterMovement()->GetCurrentAcceleration().Z, Anim->GroundSpeed, Anim->Direction,
		Anim->ShouldMove ? TEXT("true") : TEXT("false"), Anim->IsFalling ? TEXT("true") : TEXT("false"),
		Anim->ShouldEnterJump ? TEXT("true") : TEXT("false"), Anim->ShouldDoIKTrace ? TEXT("true") : TEXT("false"),
		Pelvis.X, Pelvis.Y, Pelvis.Z, Root.X, Root.Y, Root.Z);
}

void ASkillLabPlayerController::SubmitRuntimeProbeSkill(FName SkillId, AActor* Target, const FVector& TargetLocation, bool bHasLocation)
{
	USkillLabSkillRuntimeComponent* Runtime = GetPawn() ? GetPawn()->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	if (!Runtime) return;
	if (Target && Target != GetPawn()) Target->SetActorLocation(GetPawn()->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f));
	FSkillLabSkillRequest Request;
	Request.SkillId = SkillId; Request.Caster = GetPawn(); Request.Target = Target; Request.TargetLocation = TargetLocation; Request.bHasLocation = bHasLocation;
	const FSkillLabSkillResult Result = Runtime->SubmitSkillRequest(Request);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabRuntimeProbe] REQUEST skill=%s success=%s reason=%s request=%d cast=%d energy=%.1f cooldown=%.2f stage=%d"),
		*SkillId.ToString(), Result.bSuccess ? TEXT("true") : TEXT("false"), *Result.Reason.ToString(), Result.RequestId, Result.CastId,
		Runtime->GetEnergyValue(), Runtime->GetCooldownRemaining(SkillId), static_cast<int32>(Runtime->GetCastStage()));
}

void ASkillLabPlayerController::HandleRuntimeProbeSkillEvent(const FSkillLabSkillResult& Result)
{
	USkillLabSkillRuntimeComponent* Runtime = GetPawn() ? GetPawn()->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("[SkillLabRuntimeProbe] EVENT skill=%s event=%d success=%s reason=%s cast=%d requested=%.1f actual=%.1f health=%.1f->%.1f energy=%.1f cooldown=%.2f stage=%d"),
		*Result.SkillId.ToString(), static_cast<int32>(Result.Event), Result.bSuccess ? TEXT("true") : TEXT("false"), *Result.Reason.ToString(), Result.CastId,
		Result.RequestedAmount, Result.ActualAmount, Result.HealthBefore, Result.HealthAfter, Runtime ? Runtime->GetEnergyValue() : -1.0f,
		Runtime ? Runtime->GetCooldownRemaining(Result.SkillId) : -1.0f, Runtime ? static_cast<int32>(Runtime->GetCastStage()) : -1);
}

void ASkillLabPlayerController::TickRuntimeProbe()
{
	RuntimeProbeElapsed += 0.10f;
	ASkillLabCharacter* ProbeCharacter = Cast<ASkillLabCharacter>(GetPawn());
	if (!ProbeCharacter) return;
	UCharacterMovementComponent* Movement = ProbeCharacter->GetCharacterMovement();
	if (Movement)
	{
		Movement->SetMovementMode(MOVE_Walking);
		Movement->bForceMaxAccel = true;
		if (RuntimeProbeElapsed < 1.8f) { ProbeCharacter->bUseControllerRotationYaw = false; Movement->bOrientRotationToMovement = true; Movement->Velocity = FVector(600.0f, 0.0f, 0.0f); Movement->AddInputVector(FVector(1.0f, 0.0f, 0.0f), true); }
		else if (RuntimeProbeElapsed < 2.95f) { Movement->bOrientRotationToMovement = false; Movement->Velocity = FVector(0.0f, 600.0f, 0.0f); Movement->AddInputVector(FVector(0.0f, 1.0f, 0.0f), true); }
		else if (RuntimeProbeElapsed < 4.1f) { Movement->SetMovementMode(MOVE_None); Movement->Velocity = FVector::ZeroVector; }
		else if (RuntimeProbeElapsed < 5.2f)
		{
			Movement->bOrientRotationToMovement = false;
			Movement->SetMovementMode(MOVE_Falling);
			Movement->Velocity = FVector(0.0f, 0.0f, 300.0f);
		}
		else Movement->Velocity = FVector::ZeroVector;
		if (USkillLabAnimInstance* Anim = ProbeCharacter->GetMesh() ? Cast<USkillLabAnimInstance>(ProbeCharacter->GetMesh()->GetAnimInstance()) : nullptr)
			Anim->NativeUpdateAnimation(0.10f);
	}
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 1.0f, 0.051f)) LogRuntimeProbeSample(TEXT("MovingForward"));
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 2.2f, 0.051f)) LogRuntimeProbeSample(TEXT("MovingRight"));
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 3.0f, 0.051f)) LogRuntimeProbeSample(TEXT("Stopped"));
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 3.8f, 0.051f)) LogRuntimeProbeSample(TEXT("StoppedRecovered"));
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 4.2f, 0.051f)) { ProbeCharacter->Jump(); LogRuntimeProbeSample(TEXT("JumpRequested")); }
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 4.7f, 0.051f)) LogRuntimeProbeSample(TEXT("Airborne"));
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 6.0f, 0.051f)) LogRuntimeProbeSample(TEXT("Landed"));
	AActor* Target = RuntimeProbeTarget.Get();
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 6.5f, 0.051f)) SubmitRuntimeProbeSkill(TEXT("Skill_Fireball"), Target, Target ? Target->GetActorLocation() : FVector::ZeroVector, false);
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 8.5f, 0.051f)) SubmitRuntimeProbeSkill(TEXT("Skill_AreaBurst"), Target, Target ? Target->GetActorLocation() : ProbeCharacter->GetActorLocation(), true);
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 10.5f, 0.051f)) SubmitRuntimeProbeSkill(TEXT("Skill_Mend"), ProbeCharacter, ProbeCharacter->GetActorLocation(), false);
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 12.5f, 0.051f)) SubmitRuntimeProbeSkill(TEXT("Skill_BurningBuff"), Target, Target ? Target->GetActorLocation() : FVector::ZeroVector, false);
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 14.0f, 0.051f))
	{
		SubmitRuntimeProbeSkill(TEXT("Skill_Fireball"), Target, Target ? Target->GetActorLocation() : FVector::ZeroVector, false);
		if (USkillLabSkillRuntimeComponent* Runtime = ProbeCharacter->FindComponentByClass<USkillLabSkillRuntimeComponent>()) Runtime->CancelActiveCast(TEXT("RuntimeProbeCancel"));
	}
	if (FMath::IsNearlyEqual(RuntimeProbeElapsed, 15.5f, 0.051f))
	{
		if (USkillLabSkillRuntimeComponent* Runtime = ProbeCharacter->FindComponentByClass<USkillLabSkillRuntimeComponent>())
		{
			Runtime->ResetSkills();
			UE_LOG(LogTemp, Log, TEXT("[SkillLabRuntimeProbe] RESET energy=%.1f stage=%d fireballCooldown=%.2f"), Runtime->GetEnergyValue(), static_cast<int32>(Runtime->GetCastStage()), Runtime->GetCooldownRemaining(TEXT("Skill_Fireball")));
		}
	}
	if (RuntimeProbeElapsed >= 17.0f)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabRuntimeProbe] COMPLETE"));
		GetWorld()->GetTimerManager().ClearTimer(RuntimeProbeTimer);
		FPlatformMisc::RequestExit(false);
	}
}

void ASkillLabPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] SetupInputComponent controller=%s input=%s"),
		*GetNameSafe(this), *GetNameSafe(InputComponent));

	if (InputAdapter)
	{
		InputAdapter->Configure(MoveOrRotateAction, CameraLookAction, CameraZoomAction,
			MoveSuccessEffect, MoveFailureEffect, DragThresholdPixels, SkillInput);
		InputAdapter->BindInput(InputComponent);
	}
	if (InputComponent)
	{
		if (SkillShortcuts) SkillShortcuts->BindInput(InputComponent, SkillInput);
		UE_LOG(LogTemp, Log, TEXT("[SkillLabInput] SetupInputComponent complete keyBindings=%d"), InputComponent->KeyBindings.Num());
	}
}

void ASkillLabPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasActorBegunPlay()) return;
	if (HUDManager) HUDManager->InitializeForPlayer(this, InPawn, SkillShortcuts);
	if (SkillInput) SkillInput->InitializeForPlayer(this, InPawn);
	if (TestCommands) TestCommands->InitializeForPawn(InPawn, SkillShortcuts);
}

void ASkillLabPlayerController::OnUnPossess()
{
	if (SkillInput) SkillInput->ClearPlayerContext();
	if (InputAdapter) InputAdapter->ResetPointerState();
	if (HUDManager) HUDManager->SetControlledPawn(nullptr);
	if (TestCommands) TestCommands->InitializeForPawn(nullptr, SkillShortcuts);
	Super::OnUnPossess();
}
