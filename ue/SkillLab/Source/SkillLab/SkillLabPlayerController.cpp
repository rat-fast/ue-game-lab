// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "SkillLabCharacter.h"
#include "SkillLabHUDManagerComponent.h"
#include "SkillLabSkillInputComponent.h"
#include "SkillLabSkillShortcutComponent.h"
#include "SkillLabTestCommandsComponent.h"

ASkillLabPlayerController::ASkillLabPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	HUDManager = CreateDefaultSubobject<USkillLabHUDManagerComponent>(TEXT("HUDManager"));
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
	if (TestCommands) TestCommands->InitializeForPawn(GetPawn());

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (HUDManager) HUDManager->InitializeForPlayer(this, GetPawn(), SkillShortcuts);

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
		if (SkillShortcuts) SkillShortcuts->BindInput(InputComponent);
	}
}

void ASkillLabPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasActorBegunPlay()) return;
	if (HUDManager) HUDManager->InitializeForPlayer(this, InPawn, SkillShortcuts);
	if (TestCommands) TestCommands->InitializeForPawn(InPawn);
}

void ASkillLabPlayerController::OnUnPossess()
{
	if (SkillInput) SkillInput->CancelSkill();
	if (HUDManager) HUDManager->SetSkillRuntime(nullptr);
	Super::OnUnPossess();
}

void ASkillLabPlayerController::HandlePointerPressed()
{
	bPointerHeld = GetMousePosition(PointerPressPosition.X, PointerPressPosition.Y);
	bCameraDragging = false;
}

void ASkillLabPlayerController::HandlePointerReleased()
{
	if (SkillInput && SkillInput->IsTargetingGround())
	{
		bPointerHeld = false;
		bCameraDragging = false;
		return;
	}
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
