// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SkillLabCharacter.generated.h"

UCLASS()
class SKILLLAB_API ASkillLabCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASkillLabCharacter();
	void AddCameraZoomInput(float Value);
	void AddCameraRotationInput(const FVector2D& Value);

protected:

	UPROPERTY(VisibleAnywhere, Category = "SkillLab|Camera")
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "SkillLab|Camera")
	TObjectPtr<class UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "SkillLab|Camera")
	TObjectPtr<class USkillLabCameraRigComponent> CameraRig;

};
