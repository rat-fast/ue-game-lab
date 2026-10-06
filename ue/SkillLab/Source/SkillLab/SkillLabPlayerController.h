// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SkillLabPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UNiagaraSystem;
struct FInputActionValue;

UCLASS()
class SKILLLAB_API ASkillLabPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Exec)
	void SkillTest();

	UFUNCTION(Exec)
	void SkillTestRecover();

	UFUNCTION(Exec)
	void SkillReset();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void HandlePointerPressed();
	void HandlePointerReleased();
	void HandleCameraLook(const FInputActionValue& Value);
	void HandleCameraZoom(const FInputActionValue& Value);
	void MoveToCursor();

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputAction> MoveOrRotateAction;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputAction> CameraLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input", meta = (ClampMin = "0.0"))
	float DragThresholdPixels = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Feedback")
	TObjectPtr<UNiagaraSystem> MoveSuccessEffect;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Feedback")
	TObjectPtr<UNiagaraSystem> MoveFailureEffect;

	FVector2D PointerPressPosition = FVector2D::ZeroVector;
	bool bPointerHeld = false;
	bool bCameraDragging = false;
};
