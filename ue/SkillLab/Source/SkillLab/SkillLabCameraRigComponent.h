// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabCameraRigComponent.generated.h"

class USpringArmComponent;

UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabCameraRigComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabCameraRigComponent();

	void InitializeCameraBoom(USpringArmComponent* InCameraBoom);
	void AddZoomInput(float Value);
	void AddRotationInput(const FVector2D& Value);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Zoom", meta = (ClampMin = "0.0"))
	float MinArmLength = 500.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Zoom", meta = (ClampMin = "0.0"))
	float MaxArmLength = 1400.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Zoom", meta = (ClampMin = "0.0"))
	float ZoomStep = 120.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Rotation")
	float MinPitch = -80.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Rotation")
	float MaxPitch = -35.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Rotation")
	float YawSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera|Rotation")
	float PitchSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "SkillLab|Camera", meta = (ClampMin = "0.0"))
	float InterpolationSpeed = 12.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoom;

	float TargetArmLength = 900.0f;
	FRotator TargetRotation = FRotator(-65.0f, 0.0f, 0.0f);
};
