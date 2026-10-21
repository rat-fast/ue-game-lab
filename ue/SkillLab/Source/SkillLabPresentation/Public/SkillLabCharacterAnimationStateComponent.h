// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabCharacterAnimationStateComponent.generated.h"

UENUM(BlueprintType)
enum class ESkillLabCharacterAnimationState : uint8
{
	Idle UMETA(DisplayName = "Idle"),
	Move UMETA(DisplayName = "Move"),
	Jump UMETA(DisplayName = "Jump"),
	Fall UMETA(DisplayName = "Fall"),
	Land UMETA(DisplayName = "Land")
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSkillLabCharacterAnimationStateChanged, ESkillLabCharacterAnimationState, int32);

USTRUCT(BlueprintType)
struct SKILLLABPRESENTATION_API FSkillLabCharacterAnimationSnapshot
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	ESkillLabCharacterAnimationState State = ESkillLabCharacterAnimationState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	float GroundSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	bool bShouldMove = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	float Direction = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	float VerticalSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	bool bIsFalling = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	float StateTime = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Animation")
	int32 TransitionSerial = 0;
};

/**
 * Character-owned source of truth for basic animation inputs and state semantics.
 * It reads ACharacter movement facts and leaves pose evaluation and animation
 * resource selection to the AnimInstance/AnimBP.
 */
UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLABPRESENTATION_API USkillLabCharacterAnimationStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabCharacterAnimationStateComponent();

	const FSkillLabCharacterAnimationSnapshot& GetSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	FSkillLabCharacterAnimationSnapshot GetAnimationSnapshot() const { return Snapshot; }

	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	ESkillLabCharacterAnimationState GetAnimationState() const { return Snapshot.State; }

	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	bool IsInLandState() const { return Snapshot.State == ESkillLabCharacterAnimationState::Land; }

	UFUNCTION(BlueprintPure, Category = "SkillLab|Animation")
	int32 GetTransitionSerial() const { return Snapshot.TransitionSerial; }
	FSkillLabCharacterAnimationStateChanged OnStateChanged;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Tuning")
	float MoveSpeedThreshold = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Tuning")
	float MoveInputGraceDuration = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation|Tuning")
	float LandStateDuration = 0.20f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Animation|Runtime")
	FSkillLabCharacterAnimationSnapshot Snapshot;

private:
	void UpdateSnapshot(float DeltaTime);
	ESkillLabCharacterAnimationState SelectState(const class ACharacter* Character, bool bShouldMove) const;

	ESkillLabCharacterAnimationState PreviousState = ESkillLabCharacterAnimationState::Idle;
	bool bWasFalling = false;
	float MoveInputGraceRemaining = 0.0f;
};
