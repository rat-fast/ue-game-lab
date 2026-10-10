#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabPlayerInputComponent.generated.h"

class UInputAction;
class UInputComponent;
class UInputMappingContext;
class UNiagaraSystem;
class USkillLabSkillInputComponent;
struct FInputActionValue;

/**
 * Owns the mouse gesture adapter used by the SkillLab controller.
 *
 * The controller remains the lifecycle owner, while click-vs-drag, camera
 * forwarding and navigation feedback stay outside the gameplay framework
 * class. Skill targeting is injected so this adapter does not discover or
 * activate skill components through the controller's component list.
 */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabPlayerInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabPlayerInputComponent();

	void Configure(
		UInputAction* InMoveOrRotateAction,
		UInputAction* InCameraLookAction,
		UInputAction* InCameraZoomAction,
		UNiagaraSystem* InMoveSuccessEffect,
		UNiagaraSystem* InMoveFailureEffect,
		float InDragThresholdPixels,
		USkillLabSkillInputComponent* InSkillInput);
	void BindInput(UInputComponent* Input);
	void ActivateInputMapping();
	void ResetPointerState();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandlePointerPressed();
	void HandlePointerReleased();
	void HandleCameraLook(const FInputActionValue& Value);
	void HandleCameraZoom(const FInputActionValue& Value);
	void MoveToCursor();
	void RemoveInputMapping();

	TWeakObjectPtr<UInputComponent> BoundInput;
	TWeakObjectPtr<USkillLabSkillInputComponent> SkillInput;
	TObjectPtr<UInputAction> MoveOrRotateAction;
	TObjectPtr<UInputAction> CameraLookAction;
	TObjectPtr<UInputAction> CameraZoomAction;
	TObjectPtr<UNiagaraSystem> MoveSuccessEffect;
	TObjectPtr<UNiagaraSystem> MoveFailureEffect;
	TObjectPtr<UInputMappingContext> RuntimeMappingContext;

	float DragThresholdPixels = 6.0f;
	FVector2D PointerPressPosition = FVector2D::ZeroVector;
	bool bPointerHeld = false;
	bool bCameraDragging = false;
};
