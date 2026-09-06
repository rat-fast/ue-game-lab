// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "SkillLabCameraRigComponent.h"

ASkillLabCharacter::ASkillLabCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	bUseControllerRotationYaw = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SetRelativeRotation(FRotator(-65.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritRoll = false;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	CameraRig = CreateDefaultSubobject<USkillLabCameraRigComponent>(TEXT("CameraRig"));
	CameraRig->InitializeCameraBoom(CameraBoom);
}

void ASkillLabCharacter::AddCameraZoomInput(float Value)
{
	CameraRig->AddZoomInput(Value);
}

void ASkillLabCharacter::AddCameraRotationInput(const FVector2D& Value)
{
	CameraRig->AddRotationInput(Value);
}
