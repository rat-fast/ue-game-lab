// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SkillLabAnimInstance.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/** Native source of locomotion facts consumed by ABP_Unarmed's pose graph. */
UCLASS(Blueprintable, BlueprintType)
class SKILLLABPRESENTATION_API USkillLabAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	double GroundSpeed = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	double Direction = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	bool ShouldMove = false;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	bool IsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	bool IsMovingOnGround = false;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	bool ShouldEnterJump = false;

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Animation")
	bool ShouldDoIKTrace = false;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACharacter> Character;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> MovementComponent;
};
