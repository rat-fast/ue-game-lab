// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "SkillLabCharacter.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabHUDWidget.h"
#include "SkillLabHUDManagerComponent.h"
#include "Components/CapsuleComponent.h"

ASkillLabPlayerController::ASkillLabPlayerController()
{
	HUDManager = CreateDefaultSubobject<USkillLabHUDManagerComponent>(TEXT("HUDManager"));
}

void ASkillLabPlayerController::SkillTest()
{
	CastAtCursor(TEXT("Skill_Fireball"), false, true);
}

void ASkillLabPlayerController::SkillTestRecover()
{
	CastAtCursor(TEXT("Skill_Mend"), false);
}

void ASkillLabPlayerController::SkillArea() { HandleSkillArea(); }
void ASkillLabPlayerController::SkillBurn() { CastAtCursor(TEXT("Skill_BurningBuff"), false, true); }

void ASkillLabPlayerController::SkillSpawnTargets()
{
	if (!GetWorld() || bSkillTestTargetsSpawned) return;
	bSkillTestTargetsSpawned = true;
	const FVector Origin = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		ASkillLabCharacter* Target = GetWorld()->SpawnActor<ASkillLabCharacter>(
			ASkillLabCharacter::StaticClass(), Origin + FVector(500.0f, (Index - 1) * 180.0f, 0.0f), FRotator::ZeroRotator, Params);
		if (Target)
		{
			Target->ConfigureAsTrainingDummy();
			if (USkillLabSkillRuntimeComponent* Skills = Target->FindComponentByClass<USkillLabSkillRuntimeComponent>()) Skills->TeamId = 2;
			Target->Tags.AddUnique(TEXT("SkillLabDummy"));
		}
	}
}

void ASkillLabPlayerController::CastAtCursor(const FName SkillId, const bool bGroundTarget, const bool bAutoSelectNearestDummy)
{
	ASkillLabCharacter* Caster = Cast<ASkillLabCharacter>(GetPawn());
	USkillLabSkillRuntimeComponent* Skills = Caster ? Caster->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	if (!Skills) return;
	FHitResult Hit;
	GetHitResultUnderCursor(ECC_Visibility, false, Hit);
	AActor* Target = Hit.GetActor();
	const bool bHitSkillDummy = Target && Target->ActorHasTag(TEXT("SkillLabDummy"));
	if (!bHitSkillDummy)
	{
		Target = nullptr;
	}
	if (!Target && SkillId == TEXT("Skill_Mend")) Target = Caster;
	if (!Target && bAutoSelectNearestDummy && (SkillId == TEXT("Skill_Fireball") || SkillId == TEXT("Skill_BurningBuff")))
	{
		float BestDistance = TNumericLimits<float>::Max();
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (!It->ActorHasTag(TEXT("SkillLabDummy"))) continue;
			const float Distance = FVector::DistSquared(Caster->GetActorLocation(), It->GetActorLocation());
			if (Distance < BestDistance) { BestDistance = Distance; Target = *It; }
		}
	}
	FVector Location = Hit.bBlockingHit ? Hit.Location : Caster->GetActorLocation() + Caster->GetActorForwardVector() * 500.0f;
	if (bGroundTarget)
	{
		FVector WorldOrigin;
		FVector WorldDirection;
		if (DeprojectMousePositionToWorld(WorldOrigin, WorldDirection))
		{
			const float GroundZ = Caster->GetActorLocation().Z - Caster->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			const FPlane GroundPlane(FVector(0.0f, 0.0f, GroundZ), FVector::UpVector);
			const float Denominator = FVector::DotProduct(WorldDirection, GroundPlane.GetNormal());
			if (FMath::Abs(Denominator) > KINDA_SMALL_NUMBER)
			{
				Location = FMath::LinePlaneIntersection(WorldOrigin, WorldOrigin + WorldDirection * 100000.0f, GroundPlane);
			}
		}
	}
	const FSkillLabSkillResult Result = Skills->CastSkill(SkillId, bGroundTarget ? nullptr : Target, Location);
	if (!Result.bSuccess) UE_LOG(LogTemp, Warning, TEXT("Skill %s rejected: %s"), *SkillId.ToString(), *Result.Reason.ToString());
}

void ASkillLabPlayerController::HandleSkillFireball() { CastAtCursor(TEXT("Skill_Fireball"), false); }
void ASkillLabPlayerController::HandleSkillArea() { CastAtCursor(TEXT("Skill_AreaBurst"), true); }
void ASkillLabPlayerController::HandleSkillMend() { CastAtCursor(TEXT("Skill_Mend"), false); }
void ASkillLabPlayerController::HandleSkillBurn() { CastAtCursor(TEXT("Skill_BurningBuff"), false); }

void ASkillLabPlayerController::SkillReset()
{
	if (ASkillLabCharacter* SkillLabCharacter = Cast<ASkillLabCharacter>(GetPawn()))
	{
		if (USkillLabSkillRuntimeComponent* SkillComponent = SkillLabCharacter->FindComponentByClass<USkillLabSkillRuntimeComponent>())
		{
			SkillComponent->ResetSkills();
		}
	}
}

void ASkillLabPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SkillSpawnTargets();

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (!HUDWidget)
	{
		HUDWidget = CreateWidget<USkillLabHUDWidget>(this, USkillLabHUDWidget::StaticClass());
		if (HUDWidget)
		{
			USkillLabSkillRuntimeComponent* Runtime = Cast<ASkillLabCharacter>(GetPawn()) ? Cast<ASkillLabCharacter>(GetPawn())->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
			if (HUDManager) HUDManager->SetSkillRuntime(Runtime);
			HUDWidget->SetHUDManager(HUDManager);
			HUDWidget->AddToViewport(20);
			UE_LOG(LogTemp, Log, TEXT("[SkillLabHUD] Created HUD widget and connected HUD manager"));
		}
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
			LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			RuntimeMappingContext = NewObject<UInputMappingContext>(this);
			if (RuntimeMappingContext)
			{
				if (MoveOrRotateAction)
				{
					RuntimeMappingContext->MapKey(MoveOrRotateAction, EKeys::RightMouseButton);
				}
				if (CameraLookAction)
				{
					RuntimeMappingContext->MapKey(CameraLookAction, EKeys::Mouse2D);
				}
				if (CameraZoomAction)
				{
					RuntimeMappingContext->MapKey(CameraZoomAction, EKeys::MouseWheelAxis);
				}
				InputSubsystem->AddMappingContext(RuntimeMappingContext, 0);
			}
		}
	}
}

void ASkillLabPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (MoveOrRotateAction)
		{
			EnhancedInputComponent->BindAction(
				MoveOrRotateAction,
				ETriggerEvent::Started,
				this,
				&ASkillLabPlayerController::HandlePointerPressed);
			EnhancedInputComponent->BindAction(
				MoveOrRotateAction,
				ETriggerEvent::Completed,
				this,
				&ASkillLabPlayerController::HandlePointerReleased);
		}

		if (CameraLookAction)
		{
			EnhancedInputComponent->BindAction(
				CameraLookAction,
				ETriggerEvent::Triggered,
				this,
				&ASkillLabPlayerController::HandleCameraLook);
		}

		if (CameraZoomAction)
		{
			EnhancedInputComponent->BindAction(
				CameraZoomAction,
				ETriggerEvent::Triggered,
				this,
				&ASkillLabPlayerController::HandleCameraZoom);
		}
	}
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ASkillLabPlayerController::HandleSkillFireball);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ASkillLabPlayerController::HandleSkillArea);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ASkillLabPlayerController::HandleSkillMend);
		InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ASkillLabPlayerController::HandleSkillBurn);
	}
}

void ASkillLabPlayerController::HandlePointerPressed()
{
	bPointerHeld = GetMousePosition(PointerPressPosition.X, PointerPressPosition.Y);
	bCameraDragging = false;
}

void ASkillLabPlayerController::HandlePointerReleased()
{
	if (bPointerHeld && !bCameraDragging)
	{
		MoveToCursor();
	}

	bPointerHeld = false;
	bCameraDragging = false;
}

void ASkillLabPlayerController::HandleCameraLook(const FInputActionValue& Value)
{
	if (!bPointerHeld)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (GetMousePosition(MouseX, MouseY))
	{
		const FVector2D CurrentPosition(MouseX, MouseY);
		bCameraDragging |= FVector2D::Distance(CurrentPosition, PointerPressPosition) >= DragThresholdPixels;
	}

	if (bCameraDragging)
	{
		if (ASkillLabCharacter* SkillLabCharacter = Cast<ASkillLabCharacter>(GetPawn()))
		{
			SkillLabCharacter->AddCameraRotationInput(Value.Get<FVector2D>());
		}
	}
}

void ASkillLabPlayerController::HandleCameraZoom(const FInputActionValue& Value)
{
	if (ASkillLabCharacter* SkillLabCharacter = Cast<ASkillLabCharacter>(GetPawn()))
	{
		SkillLabCharacter->AddCameraZoomInput(Value.Get<float>());
	}
}

void ASkillLabPlayerController::MoveToCursor()
{
	FHitResult HitResult;
	if (GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
				this,
				ControlledPawn->GetActorLocation(),
				HitResult.Location,
				ControlledPawn);
			const bool bHasUsablePath = Path && Path->IsValid() && !Path->IsPartial();
			UNiagaraSystem* FeedbackEffect = bHasUsablePath ? MoveSuccessEffect : MoveFailureEffect;
			if (FeedbackEffect)
			{
				UNiagaraFunctionLibrary::SpawnSystemAtLocation(
					GetWorld(),
					FeedbackEffect,
					HitResult.Location + FVector(0.0f, 0.0f, 2.0f),
					FRotator::ZeroRotator,
					FVector::OneVector,
					true,
					true,
					ENCPoolMethod::None,
					true);
			}

			if (bHasUsablePath)
			{
				UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, HitResult.Location);
			}
		}
	}
}
