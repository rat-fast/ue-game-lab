// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabCameraRigComponent.h"

#include "GameFramework/SpringArmComponent.h"

USkillLabCameraRigComponent::USkillLabCameraRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USkillLabCameraRigComponent::InitializeCameraBoom(USpringArmComponent* InCameraBoom)
{
	CameraBoom = InCameraBoom;

	if (CameraBoom)
	{
		TargetArmLength = CameraBoom->TargetArmLength;
		TargetRotation = CameraBoom->GetRelativeRotation();
	}
}

void USkillLabCameraRigComponent::BeginPlay()
{
	Super::BeginPlay();

	if (CameraBoom)
	{
		TargetArmLength = CameraBoom->TargetArmLength;
		TargetRotation = CameraBoom->GetRelativeRotation();
	}
}

void USkillLabCameraRigComponent::AddZoomInput(float Value)
{
	TargetArmLength = FMath::Clamp(
		TargetArmLength - (Value * ZoomStep),
		MinArmLength,
		MaxArmLength);
}

void USkillLabCameraRigComponent::AddRotationInput(const FVector2D& Value)
{
	TargetRotation.Yaw += Value.X * YawSensitivity;
	TargetRotation.Pitch = FMath::Clamp(
		TargetRotation.Pitch + (Value.Y * PitchSensitivity),
		MinPitch,
		MaxPitch);
	TargetRotation.Roll = 0.0f;
}

void USkillLabCameraRigComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!CameraBoom)
	{
		return;
	}

	CameraBoom->TargetArmLength = FMath::FInterpTo(
		CameraBoom->TargetArmLength,
		TargetArmLength,
		DeltaTime,
		InterpolationSpeed);
	CameraBoom->SetRelativeRotation(FMath::RInterpTo(
		CameraBoom->GetRelativeRotation(),
		TargetRotation,
		DeltaTime,
		InterpolationSpeed));
}
