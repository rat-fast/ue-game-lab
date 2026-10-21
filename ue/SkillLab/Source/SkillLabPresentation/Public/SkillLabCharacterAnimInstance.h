// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SkillLabCharacterAnimationStateComponent.h"
#include "SkillLabCharacterAnimInstance.generated.h"

/** Native data bridge for ABP_Unarmed's existing pose state machine. */
UCLASS(Transient, Blueprintable)
class SKILLLABPRESENTATION_API USkillLabCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	FSkillLabCharacterAnimationSnapshot GetCharacterAnimationSnapshot() const { return Snapshot; }

	/** Receives Lua's locomotion mapping; movement facts remain in the native snapshot. */
	void SetLuaLocomotionIntent(bool bShouldMove, bool bIsFalling);

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
	bool bHasLuaLocomotionIntent = false;
	bool bLuaShouldMove = false;
	bool bLuaIsFalling = false;

};
