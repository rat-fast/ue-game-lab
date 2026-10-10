// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "SkillLabCameraRigComponent.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillAnimationComponent.h"
#include "AbilitySystemComponent.h"

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

	SkillAbilitySystemComponent = CreateDefaultSubobject<USkillLabAbilitySystemComponent>(TEXT("SkillAbilitySystem"));
	SkillComponent = CreateDefaultSubobject<USkillLabSkillRuntimeComponent>(TEXT("SkillComponent"));
	SkillAnimationComponent = CreateDefaultSubobject<USkillLabSkillAnimationComponent>(TEXT("SkillAnimation"));
}

UAbilitySystemComponent* ASkillLabCharacter::GetAbilitySystemComponent() const
{
	return SkillAbilitySystemComponent.Get();
}

void ASkillLabCharacter::AddCameraZoomInput(float Value)
{
	CameraRig->AddZoomInput(Value);
}

void ASkillLabCharacter::AddCameraRotationInput(const FVector2D& Value)
{
	CameraRig->AddRotationInput(Value);
}

void ASkillLabCharacter::ConfigureAsTrainingDummy()
{
	if (TrainingDummyBody)
	{
		return;
	}

	if (GetMesh())
	{
		GetMesh()->SetVisibility(false, true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);

	UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (!CylinderMesh || !SphereMesh)
	{
		return;
	}

	TrainingDummyBody = NewObject<UStaticMeshComponent>(this, TEXT("TrainingDummyBody"));
	TrainingDummyBody->SetStaticMesh(CylinderMesh);
	TrainingDummyBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TrainingDummyBody->SetCollisionResponseToAllChannels(ECR_Ignore);
	TrainingDummyBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	TrainingDummyBody->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// ACharacter's root/capsule origin is at the capsule center, not at the feet.
	// The default capsule half height is 88 cm, so offset the 135 cm tall post
	// down by 20.5 cm to put its base on the floor.
	TrainingDummyBody->SetRelativeLocation(FVector(0.0f, 0.0f, -20.5f));
	TrainingDummyBody->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.35f));
	TrainingDummyBody->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(TrainingDummyBody);
	TrainingDummyBody->RegisterComponent();

	TrainingDummyHead = NewObject<UStaticMeshComponent>(this, TEXT("TrainingDummyHead"));
	TrainingDummyHead->SetStaticMesh(SphereMesh);
	TrainingDummyHead->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TrainingDummyHead->SetCollisionResponseToAllChannels(ECR_Ignore);
	TrainingDummyHead->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	TrainingDummyHead->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	TrainingDummyHead->SetRelativeLocation(FVector(0.0f, 0.0f, 67.5f));
	TrainingDummyHead->SetRelativeScale3D(FVector(0.78f, 0.78f, 0.45f));
	TrainingDummyHead->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	AddInstanceComponent(TrainingDummyHead);
	TrainingDummyHead->RegisterComponent();

	if (UMaterialInstanceDynamic* BodyMaterial = TrainingDummyBody->CreateAndSetMaterialInstanceDynamic(0))
	{
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.32f, 0.10f, 0.03f, 1.0f));
		BodyMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.32f, 0.10f, 0.03f, 1.0f));
	}
	if (UMaterialInstanceDynamic* HeadMaterial = TrainingDummyHead->CreateAndSetMaterialInstanceDynamic(0))
	{
		HeadMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.05f, 0.02f, 1.0f));
		HeadMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.8f, 0.05f, 0.02f, 1.0f));
	}
}
