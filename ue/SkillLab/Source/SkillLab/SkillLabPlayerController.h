// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "SkillLabPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class APawn;
class UNiagaraSystem;
class USkillLabHUDManagerComponent;
class USkillLabPlayerInputComponent;
class USkillLabSkillInputComponent;
class USkillLabSkillShortcutComponent;
class USkillLabTestCommandsComponent;
struct FInputActionValue;

UCLASS()
class SKILLLAB_API ASkillLabPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASkillLabPlayerController();
	UFUNCTION(Exec)
	void SkillTest();

	UFUNCTION(Exec)
	void SkillTestRecover();

	UFUNCTION(Exec)
	void SkillArea();

	UFUNCTION(Exec)
	void SkillBurn();

	UFUNCTION(Exec)
	void SkillSpawnTargets();

	UFUNCTION(Exec)
	void SkillReset();

	/** Reloads external skill numbers without recompiling or restarting the editor. */
	UFUNCTION(Exec)
	void SkillReloadDefinitions();

	UFUNCTION(Exec)
	void SkillAnimationSmoke();

	/** Runs all four skills through the public runtime request API without cursor targeting. */
	UFUNCTION(Exec)
	void SkillPresentationSmoke();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputAction> MoveOrRotateAction;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputAction> CameraLookAction;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Input", meta = (ClampMin = "0.0"))
	float DragThresholdPixels = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Feedback")
	TObjectPtr<UNiagaraSystem> MoveSuccessEffect;

	UPROPERTY(EditDefaultsOnly, Category = "SkillLab|Feedback")
	TObjectPtr<UNiagaraSystem> MoveFailureEffect;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkillLabHUDManagerComponent> HUDManager;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkillLabPlayerInputComponent> InputAdapter;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkillLabSkillInputComponent> SkillInput;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkillLabSkillShortcutComponent> SkillShortcuts;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkillLabTestCommandsComponent> TestCommands;

};
