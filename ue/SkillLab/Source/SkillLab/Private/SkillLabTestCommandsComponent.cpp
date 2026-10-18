#include "SkillLabTestCommandsComponent.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/PlatformMisc.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "SkillLabCharacterAnimationStateComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealClient.h"
#include "SkillLabSkillShortcutComponent.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabTestArenaSubsystem.h"
#include "../SkillLabCharacter.h"
#include "SkillLabSkillTypes.h"
#include "TimerManager.h"

USkillLabTestCommandsComponent::USkillLabTestCommandsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APawn* USkillLabTestCommandsComponent::GetControlledPawn() const
{
	return ControlledPawn.Get();
}

void USkillLabTestCommandsComponent::InitializeForPawn(APawn* Pawn, USkillLabSkillShortcutComponent* InShortcuts)
{
	ControlledPawn = Pawn;
	ShortcutComponent = InShortcuts;
	SkillRuntime = Pawn ? Pawn->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	if (!bEnableTestCommands || !bInitializeTrainingArena || !Pawn) return;
	if (USkillLabTestArenaSubsystem* Arena = GetWorld() ? GetWorld()->GetSubsystem<USkillLabTestArenaSubsystem>() : nullptr)
	{
		Arena->PrepareForSkillTesting(Pawn);
	}
}

void USkillLabTestCommandsComponent::RequestSlot(const int32 SlotIndex)
{
	if (!bEnableTestCommands) return;
	if (ShortcutComponent.IsValid())
	{
		ShortcutComponent->ActivateSlot(SlotIndex);
	}
}

void USkillLabTestCommandsComponent::SkillTest() { RequestSlot(0); }
void USkillLabTestCommandsComponent::SkillTestRecover() { RequestSlot(2); }
void USkillLabTestCommandsComponent::SkillArea() { RequestSlot(1); }
void USkillLabTestCommandsComponent::SkillBurn() { RequestSlot(3); }

void USkillLabTestCommandsComponent::SpawnTargets()
{
	if (!bEnableTestCommands) return;
	if (USkillLabTestArenaSubsystem* Arena = GetWorld() ? GetWorld()->GetSubsystem<USkillLabTestArenaSubsystem>() : nullptr)
	{
		Arena->SpawnTrainingTargets(GetControlledPawn());
	}
}

void USkillLabTestCommandsComponent::ResetSkills()
{
	if (!bEnableTestCommands) return;
	if (SkillRuntime.IsValid()) SkillRuntime->ResetSkills();
}

void USkillLabTestCommandsComponent::ReloadDefinitions()
{
	if (!bEnableTestCommands) return;
	const bool bReloaded = FSkillLabSkillDefinitionProvider::ReloadDefinitions();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabTesting] SkillDefinitions reload %s"), bReloaded ? TEXT("succeeded") : TEXT("failed; previous definitions retained"));
}

void USkillLabTestCommandsComponent::SkillPresentationSmoke()
{
	if (!bEnableTestCommands || !SkillRuntime.IsValid() || !GetWorld()) return;
	if (SkillPresentationSmokeTimer.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(SkillPresentationSmokeTimer);
	}
	SkillRuntime->OnNativeSkillEvent.RemoveAll(this);
	SkillRuntime->OnNativeSkillEvent.AddUObject(this, &USkillLabTestCommandsComponent::HandleSkillPresentationSmokeEvent);

	ASkillLabCharacter* Caster = Cast<ASkillLabCharacter>(GetControlledPawn());
	if (!Caster)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabPresentationSmoke] requires ASkillLabCharacter"));
		SkillRuntime->OnNativeSkillEvent.RemoveAll(this);
		return;
	}

	const FVector Forward = FVector(Caster->GetActorForwardVector().X, Caster->GetActorForwardVector().Y, 0.0f).GetSafeNormal();
	const FVector TargetLocation = Caster->GetActorLocation() + Forward * 650.0f;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	ASkillLabCharacter* Target = GetWorld()->SpawnActor<ASkillLabCharacter>(
		ASkillLabCharacter::StaticClass(), TargetLocation, Caster->GetActorRotation(), SpawnParameters);
	if (!Target)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillLabPresentationSmoke] failed to spawn temporary target"));
		SkillRuntime->OnNativeSkillEvent.RemoveAll(this);
		return;
	}
	Target->ConfigureAsTrainingDummy();
	if (USkillLabSkillRuntimeComponent* TargetRuntime = Target->FindComponentByClass<USkillLabSkillRuntimeComponent>())
	{
		TargetRuntime->TeamId = 2;
	}
	Target->Tags.AddUnique(TEXT("SkillLabPresentationSmokeTarget"));
	SkillPresentationSmokeTarget = Target;
	SkillPresentationSmokeStep = 0;
	UE_LOG(LogTemp, Log, TEXT("[SkillLabPresentationSmoke] started target=%s distance=%.0f groundAim=%s"),
		*GetNameSafe(Target), FVector::Dist2D(Caster->GetActorLocation(), TargetLocation),
		*(Caster->GetActorLocation() + Forward * 800.0f).ToCompactString());
	GetWorld()->GetTimerManager().SetTimer(SkillPresentationSmokeTimer, this,
		&USkillLabTestCommandsComponent::RunSkillPresentationSmokeStep, 0.5f, false);
}

void USkillLabTestCommandsComponent::RunSkillPresentationSmokeStep()
{
	if (!SkillRuntime.IsValid() || !GetWorld()) return;
	ASkillLabCharacter* Caster = Cast<ASkillLabCharacter>(GetControlledPawn());
	AActor* Target = SkillPresentationSmokeTarget.Get();
	if (!Caster || !Target)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillLabPresentationSmoke] caster or temporary target disappeared at step=%d"), SkillPresentationSmokeStep);
		SkillRuntime->OnNativeSkillEvent.RemoveAll(this);
		return;
	}

	FName SkillId = NAME_None;
	AActor* SkillTarget = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	float NextDelay = 1.5f;
	switch (SkillPresentationSmokeStep)
	{
	case 0:
		SkillId = TEXT("Skill_Fireball");
		SkillTarget = Target;
		NextDelay = 6.0f; // Allows the projectile's configured lifetime to resolve before the next cast.
		break;
	case 1:
		SkillId = TEXT("Skill_AreaBurst");
		TargetLocation = Caster->GetActorLocation() + Caster->GetActorForwardVector() * 800.0f;
		break;
	case 2:
		SkillId = TEXT("Skill_Mend");
		break;
	case 3:
		SkillId = TEXT("Skill_BurningBuff");
		SkillTarget = Target;
		NextDelay = 5.0f; // Observe periodic ticks after the cast has ended.
		break;
	default:
		UE_LOG(LogTemp, Log, TEXT("[SkillLabPresentationSmoke] sequence finished; persistent effects are left to their own lifecycle"));
		SkillRuntime->OnNativeSkillEvent.RemoveAll(this);
		SkillPresentationSmokeTarget = nullptr;
		Target->Destroy();
		GetWorld()->GetTimerManager().SetTimer(SkillPresentationSmokeTimer, []()
		{
			FPlatformMisc::RequestExit(false);
		}, 0.5f, false);
		return;
	}

	const FSkillLabSkillResult RequestResult = SkillRuntime->CastSkill(SkillId, SkillTarget, TargetLocation);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabPresentationSmoke] request Skill=%s accepted=%s Cast=%d Reason=%s"),
		*SkillId.ToString(), RequestResult.bSuccess ? TEXT("true") : TEXT("false"), RequestResult.CastId, *RequestResult.Reason.ToString());
	++SkillPresentationSmokeStep;
	GetWorld()->GetTimerManager().SetTimer(SkillPresentationSmokeTimer, this,
		&USkillLabTestCommandsComponent::RunSkillPresentationSmokeStep, NextDelay, false);
}

void USkillLabTestCommandsComponent::HandleSkillPresentationSmokeEvent(const FSkillLabSkillResult& Result)
{
	if (Result.SkillId.IsNone()) return;
	UE_LOG(LogTemp, Log, TEXT("[SkillLabPresentationSmoke] event=%d Skill=%s Cast=%d Stage=%d Success=%s Reason=%s Effect=%d Amount=%.2f Target=%s"),
		static_cast<int32>(Result.Event), *Result.SkillId.ToString(), Result.CastId, static_cast<int32>(Result.Stage),
		Result.bSuccess ? TEXT("true") : TEXT("false"), *Result.Reason.ToString(), static_cast<int32>(Result.Effect),
		Result.ActualAmount, *GetNameSafe(Result.TargetActor));
}

void USkillLabTestCommandsComponent::AnimationStateSmoke()
{
	if (!bEnableTestCommands) return;
	ACharacter* Character = Cast<ACharacter>(GetControlledPawn());
	if (!Character || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimationState] smoke requires a controlled Character"));
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeMoveTimer);
	GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeJumpTimer);
	GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeStopTimer);
	GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeCaptureTimer);
	GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeCaptureEndTimer);
	LastCapturedAnimationState = INDEX_NONE;
	AnimationSmokeCaptureIndex = 0;
	if (ASkillLabCharacter* SkillLabCharacter = Cast<ASkillLabCharacter>(Character))
	{
		SkillLabCharacter->AddCameraZoomInput(3.0f);
	}
	CaptureAnimationSmokeState();
	GetWorld()->GetTimerManager().SetTimer(AnimationSmokeCaptureTimer, this,
		&USkillLabTestCommandsComponent::CaptureAnimationSmokeState, 0.05f, true);
	GetWorld()->GetTimerManager().SetTimer(AnimationSmokeCaptureEndTimer, this,
		&USkillLabTestCommandsComponent::StopAnimationSmokeCapture, 4.5f, false);
	GetWorld()->GetTimerManager().SetTimer(AnimationSmokeMoveTimer, this,
		&USkillLabTestCommandsComponent::ApplyAnimationSmokeMovement, 0.02f, true);
	GetWorld()->GetTimerManager().SetTimer(AnimationSmokeJumpTimer, this,
		&USkillLabTestCommandsComponent::TriggerAnimationSmokeJump, 0.8f, false);
	GetWorld()->GetTimerManager().SetTimer(AnimationSmokeStopTimer, this,
		&USkillLabTestCommandsComponent::StopAnimationSmokeMovement, 1.8f, false);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] smoke started: move, jump, fall, land"));
}

void USkillLabTestCommandsComponent::ApplyAnimationSmokeMovement()
{
	ACharacter* Character = Cast<ACharacter>(GetControlledPawn());
	if (Character)
	{
		Character->AddMovementInput(Character->GetActorForwardVector(), 1.0f);
	}
}

void USkillLabTestCommandsComponent::TriggerAnimationSmokeJump()
{
	ACharacter* Character = Cast<ACharacter>(GetControlledPawn());
	if (!Character) return;
	if (Character->CanJump())
	{
		Character->Jump();
		UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] smoke issued Character::Jump"));
	}
	else
	{
		Character->LaunchCharacter(FVector(0.0f, 0.0f, 450.0f), false, true);
		UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] smoke launched Character because CanJump was false"));
	}
}

void USkillLabTestCommandsComponent::StopAnimationSmokeMovement()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeMoveTimer);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] smoke movement input stopped"));
}

void USkillLabTestCommandsComponent::CaptureAnimationSmokeState()
{
	ACharacter* Character = Cast<ACharacter>(GetControlledPawn());
	const USkillLabCharacterAnimationStateComponent* StateComponent = Character
		? Character->FindComponentByClass<USkillLabCharacterAnimationStateComponent>()
		: nullptr;
	if (!Character || !StateComponent) return;

	const ESkillLabCharacterAnimationState State = StateComponent->GetAnimationState();
	const int32 StateValue = static_cast<int32>(State);
	if (StateValue == LastCapturedAnimationState || FScreenshotRequest::IsScreenshotRequested()) return;
	LastCapturedAnimationState = StateValue;

	const TCHAR* StateName = TEXT("Unknown");
	switch (State)
	{
	case ESkillLabCharacterAnimationState::Idle: StateName = TEXT("Idle"); break;
	case ESkillLabCharacterAnimationState::Move: StateName = TEXT("Move"); break;
	case ESkillLabCharacterAnimationState::Jump: StateName = TEXT("Jump"); break;
	case ESkillLabCharacterAnimationState::Fall: StateName = TEXT("Fall"); break;
	case ESkillLabCharacterAnimationState::Land: StateName = TEXT("Land"); break;
	default: break;
	}

	const FString CaptureDirectory = FPaths::ProjectSavedDir() / TEXT("AnimationStateVisual");
	IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
	const FString CapturePath = CaptureDirectory / FString::Printf(
		TEXT("%02d_%s.png"), AnimationSmokeCaptureIndex++, StateName);
	FScreenshotRequest::RequestScreenshot(CapturePath, false, false, false, FIntRect(), true);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] visual capture requested state=%s path=%s"),
		StateName, *CapturePath);
}

void USkillLabTestCommandsComponent::StopAnimationSmokeCapture()
{
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(AnimationSmokeCaptureTimer);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] visual capture stopped"));
}
