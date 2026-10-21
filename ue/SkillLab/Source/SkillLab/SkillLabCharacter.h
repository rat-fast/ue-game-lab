// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "SkillLabCharacter.generated.h"

UCLASS()
class SKILLLAB_API ASkillLabCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ASkillLabCharacter();
	virtual void BeginPlay() override;
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	void AddCameraZoomInput(float Value);
	void AddCameraRotationInput(const FVector2D& Value);
	void ConfigureAsTrainingDummy();

	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	class USkillLabCharacterAnimationStateComponent* GetCharacterAnimationStateComponent() const { return CharacterAnimationStateComponent; }

protected:

	UPROPERTY(VisibleAnywhere, Category = "SkillLab|Camera")
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "SkillLab|Camera")
	TObjectPtr<class UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, Category = "SkillLab|Camera")
	TObjectPtr<class USkillLabCameraRigComponent> CameraRig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkillLabAbilitySystemComponent> SkillAbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkillLabSkillRuntimeComponent> SkillComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Abilities", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkillLabSkillAnimationComponent> SkillAnimationComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkillLabCharacterAnimationStateComponent> CharacterAnimationStateComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USkillLabCharacterPresentationLuaComponent> CharacterPresentationLuaComponent;

	UPROPERTY(Transient)
	TObjectPtr<class UStaticMeshComponent> TrainingDummyBody;

	UPROPERTY(Transient)
	TObjectPtr<class UStaticMeshComponent> TrainingDummyHead;

};
