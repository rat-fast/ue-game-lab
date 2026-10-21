// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SkillLabCharacterAnimationStateComponent.h"
#include "SkillLabCharacterAnimInstance.generated.h"

/** Native data bridge for ABP_Unarmed's existing pose state machine. */
UCLASS(Transient, Blueprintable)
class SKILLLAB_API USkillLabCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	FSkillLabCharacterAnimationSnapshot GetCharacterAnimationSnapshot() const { return Snapshot; }

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "SkillLab|Animation")
	FSkillLabCharacterAnimationSnapshot Snapshot;

private:
	void ResolveStateComponent();
	void PushLegacyAnimBPInputs();

	UPROPERTY(Transient)
	TObjectPtr<USkillLabCharacterAnimationStateComponent> StateComponent;

};
