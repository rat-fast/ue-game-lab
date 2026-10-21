// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabCharacterAnimationStateComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	const TCHAR* StateName(const ESkillLabCharacterAnimationState State)
	{
		switch (State)
		{
		case ESkillLabCharacterAnimationState::Idle: return TEXT("Idle");
		case ESkillLabCharacterAnimationState::Move: return TEXT("Move");
		case ESkillLabCharacterAnimationState::Jump: return TEXT("Jump");
		case ESkillLabCharacterAnimationState::Fall: return TEXT("Fall");
		case ESkillLabCharacterAnimationState::Land: return TEXT("Land");
		default: return TEXT("Unknown");
		}
	}
}

USkillLabCharacterAnimationStateComponent::USkillLabCharacterAnimationStateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);
}

void USkillLabCharacterAnimationStateComponent::BeginPlay()
{
	Super::BeginPlay();
	PreviousState = Snapshot.State;
	UpdateSnapshot(0.0f);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] %s initialized state=%s serial=%d"),
		*GetNameSafe(GetOwner()), StateName(Snapshot.State), Snapshot.TransitionSerial);
	OnStateChanged.Broadcast(Snapshot.State, Snapshot.TransitionSerial);
}

void USkillLabCharacterAnimationStateComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateSnapshot(DeltaTime);
}

void USkillLabCharacterAnimationStateComponent::UpdateSnapshot(float DeltaTime)
{
	bool bStateChanged = false;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!Movement)
	{
		return;
	}

	const FVector Velocity = Character->GetVelocity();
	const FVector GroundVelocity(Velocity.X, Velocity.Y, 0.0f);
	const float GroundSpeed = GroundVelocity.Size();
	const float RawDirection = GroundSpeed > KINDA_SMALL_NUMBER
		? FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(Character->GetActorRightVector(), GroundVelocity),
			FVector::DotProduct(Character->GetActorForwardVector(), GroundVelocity)))
		: 0.0f;
	const float Direction = Movement->bOrientRotationToMovement
		? FMath::Clamp(RawDirection, -45.0f, 45.0f)
		: RawDirection;
	const bool bHasMoveInput = !Movement->GetCurrentAcceleration().IsNearlyZero();
	if (bHasMoveInput)
	{
		MoveInputGraceRemaining = MoveInputGraceDuration;
	}
	else
	{
		MoveInputGraceRemaining = FMath::Max(0.0f, MoveInputGraceRemaining - DeltaTime);
	}
	// CharacterMovement can report acceleration intermittently between input ticks.
	// Keep Move through that sampling gap, then release it when both input grace and
	// residual velocity have ended.
	// Right-click navigation is driven by the path-following controller. During
	// that path, CharacterMovement can have a real ground velocity while its
	// current acceleration remains zero. Velocity is therefore a sufficient
	// locomotion signal; acceleration is retained for immediate input and the
	// short sampling-gap grace period.
	const bool bShouldMove = bHasMoveInput
		|| GroundSpeed > MoveSpeedThreshold
		|| (Snapshot.bShouldMove && MoveInputGraceRemaining > 0.0f);

	const bool bIsFalling = Movement->IsFalling();
	const bool bHasJustLanded = bWasFalling && !bIsFalling && Movement->IsMovingOnGround();
	const bool bKeepLandState = Snapshot.State == ESkillLabCharacterAnimationState::Land
		&& Snapshot.StateTime < LandStateDuration
		&& Movement->IsMovingOnGround();
	const ESkillLabCharacterAnimationState NewState = (bHasJustLanded || bKeepLandState)
		? ESkillLabCharacterAnimationState::Land
		: SelectState(Character, bShouldMove);

	if (NewState != PreviousState)
	{
		const ESkillLabCharacterAnimationState OldState = PreviousState;
		PreviousState = NewState;
		++Snapshot.TransitionSerial;
		Snapshot.StateTime = 0.0f;
		UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] %s %s -> %s serial=%d"),
			*GetNameSafe(GetOwner()), StateName(OldState), StateName(NewState), Snapshot.TransitionSerial);
		bStateChanged = true;
	}
	else
	{
		Snapshot.StateTime = FMath::Max(0.0f, Snapshot.StateTime + DeltaTime);
	}

	Snapshot.State = NewState;
	Snapshot.GroundSpeed = GroundSpeed;
	Snapshot.Velocity = Velocity;
	Snapshot.bShouldMove = bShouldMove;
	Snapshot.Direction = Direction;
	Snapshot.VerticalSpeed = Velocity.Z;
	Snapshot.bIsFalling = bIsFalling;
	bWasFalling = bIsFalling;

	// Land is a short, explicit semantic state. Once its observation window has
	// elapsed, resolve back to the current grounded movement state.
	if (Snapshot.State == ESkillLabCharacterAnimationState::Land && Snapshot.StateTime >= LandStateDuration)
	{
		const ESkillLabCharacterAnimationState GroundedState = bShouldMove || GroundSpeed > MoveSpeedThreshold
			? ESkillLabCharacterAnimationState::Move
			: ESkillLabCharacterAnimationState::Idle;
		if (GroundedState != Snapshot.State)
		{
			const ESkillLabCharacterAnimationState OldState = Snapshot.State;
			PreviousState = GroundedState;
			Snapshot.State = GroundedState;
			Snapshot.StateTime = 0.0f;
			++Snapshot.TransitionSerial;
			UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] %s %s -> %s serial=%d"),
				*GetNameSafe(GetOwner()), StateName(OldState), StateName(GroundedState), Snapshot.TransitionSerial);
			bStateChanged = true;
		}
	}
	if (bStateChanged)
	{
		OnStateChanged.Broadcast(Snapshot.State, Snapshot.TransitionSerial);
	}
}

ESkillLabCharacterAnimationState USkillLabCharacterAnimationStateComponent::SelectState(const ACharacter* Character, const bool bShouldMove) const
{
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return ESkillLabCharacterAnimationState::Idle;
	}

	if (Movement->IsFalling())
	{
		return Movement->Velocity.Z > 0.0f
			? ESkillLabCharacterAnimationState::Jump
			: ESkillLabCharacterAnimationState::Fall;
	}

	return bShouldMove
		? ESkillLabCharacterAnimationState::Move
		: ESkillLabCharacterAnimationState::Idle;
}
