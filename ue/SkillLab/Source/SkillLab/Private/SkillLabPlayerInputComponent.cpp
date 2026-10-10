#include "SkillLabPlayerInputComponent.h"

#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "../SkillLabCharacter.h"
#include "SkillLabSkillInputComponent.h"

USkillLabPlayerInputComponent::USkillLabPlayerInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabPlayerInputComponent::Configure(
	UInputAction* InMoveOrRotateAction,
	UInputAction* InCameraLookAction,
	UInputAction* InCameraZoomAction,
	UNiagaraSystem* InMoveSuccessEffect,
	UNiagaraSystem* InMoveFailureEffect,
	const float InDragThresholdPixels,
	USkillLabSkillInputComponent* InSkillInput)
{
	MoveOrRotateAction = InMoveOrRotateAction;
	CameraLookAction = InCameraLookAction;
	CameraZoomAction = InCameraZoomAction;
	MoveSuccessEffect = InMoveSuccessEffect;
	MoveFailureEffect = InMoveFailureEffect;
	DragThresholdPixels = FMath::Max(0.0f, InDragThresholdPixels);
	SkillInput = InSkillInput;
}

void USkillLabPlayerInputComponent::BindInput(UInputComponent* Input)
{
	if (!Input || BoundInput.Get() == Input)
	{
		return;
	}

	BoundInput = Input;
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(Input))
	{
		if (MoveOrRotateAction)
		{
			EnhancedInputComponent->BindAction(MoveOrRotateAction, ETriggerEvent::Started, this,
				&USkillLabPlayerInputComponent::HandlePointerPressed);
			EnhancedInputComponent->BindAction(MoveOrRotateAction, ETriggerEvent::Completed, this,
				&USkillLabPlayerInputComponent::HandlePointerReleased);
		}
		if (CameraLookAction)
		{
			EnhancedInputComponent->BindAction(CameraLookAction, ETriggerEvent::Triggered, this,
				&USkillLabPlayerInputComponent::HandleCameraLook);
		}
		if (CameraZoomAction)
		{
			EnhancedInputComponent->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this,
				&USkillLabPlayerInputComponent::HandleCameraZoom);
		}
	}
}

void USkillLabPlayerInputComponent::ActivateInputMapping()
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (!InputSubsystem)
	{
		return;
	}

	RemoveInputMapping();
	RuntimeMappingContext = NewObject<UInputMappingContext>(this);
	if (!RuntimeMappingContext)
	{
		return;
	}
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

void USkillLabPlayerInputComponent::ResetPointerState()
{
	bPointerHeld = false;
	bCameraDragging = false;
	PointerPressPosition = FVector2D::ZeroVector;
}

void USkillLabPlayerInputComponent::RemoveInputMapping()
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()
		: nullptr;
	if (InputSubsystem && RuntimeMappingContext)
	{
		InputSubsystem->RemoveMappingContext(RuntimeMappingContext);
	}
	RuntimeMappingContext = nullptr;
}

void USkillLabPlayerInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveInputMapping();
	ResetPointerState();
	SkillInput.Reset();
	BoundInput.Reset();
	Super::EndPlay(EndPlayReason);
}

void USkillLabPlayerInputComponent::HandlePointerPressed()
{
	if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		bPointerHeld = Controller->GetMousePosition(PointerPressPosition.X, PointerPressPosition.Y);
	}
	bCameraDragging = false;
}

void USkillLabPlayerInputComponent::HandlePointerReleased()
{
	if (SkillInput.IsValid() && SkillInput->IsTargetingGround())
	{
		ResetPointerState();
		return;
	}
	if (bPointerHeld && !bCameraDragging)
	{
		MoveToCursor();
	}
	ResetPointerState();
}

void USkillLabPlayerInputComponent::HandleCameraLook(const FInputActionValue& Value)
{
	if (!bPointerHeld)
	{
		return;
	}

	if (APlayerController* Controller = Cast<APlayerController>(GetOwner()))
	{
		float MouseX = 0.0f;
		float MouseY = 0.0f;
		if (Controller->GetMousePosition(MouseX, MouseY))
		{
			bCameraDragging |= FVector2D::Distance(FVector2D(MouseX, MouseY), PointerPressPosition) >= DragThresholdPixels;
		}
	}

	if (bCameraDragging)
	{
		APlayerController* Controller = Cast<APlayerController>(GetOwner());
		if (ASkillLabCharacter* Character = Controller ? Cast<ASkillLabCharacter>(Controller->GetPawn()) : nullptr)
		{
			Character->AddCameraRotationInput(Value.Get<FVector2D>());
		}
	}
}

void USkillLabPlayerInputComponent::HandleCameraZoom(const FInputActionValue& Value)
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (ASkillLabCharacter* Character = Controller ? Cast<ASkillLabCharacter>(Controller->GetPawn()) : nullptr)
	{
		Character->AddCameraZoomInput(Value.Get<float>());
	}
}

void USkillLabPlayerInputComponent::MoveToCursor()
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller)
	{
		return;
	}

	FHitResult HitResult;
	if (!Controller->GetHitResultUnderCursor(ECC_Visibility, false, HitResult))
	{
		return;
	}

	APawn* ControlledPawn = Controller->GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
		Controller, ControlledPawn->GetActorLocation(), HitResult.Location, ControlledPawn);
	const bool bHasUsablePath = Path && Path->IsValid() && !Path->IsPartial();
	UNiagaraSystem* FeedbackEffect = bHasUsablePath ? MoveSuccessEffect.Get() : MoveFailureEffect.Get();
	if (FeedbackEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), FeedbackEffect, HitResult.Location + FVector(0.0f, 0.0f, 2.0f),
			FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::None, true);
	}
	if (bHasUsablePath)
	{
		UAIBlueprintHelperLibrary::SimpleMoveToLocation(Controller, HitResult.Location);
	}
}
