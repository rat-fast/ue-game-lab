// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabAnimInstance.h"

#include "Animation/AnimInstance.h"
#include "KismetAnimationLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SkillLabAnimationDefinitionProvider.h"

void USkillLabAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Character = Cast<ACharacter>(TryGetPawnOwner());
	MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	Velocity = FVector::ZeroVector;
	GroundSpeed = 0.0;
	Direction = 0.0;
	ShouldMove = false;
	IsFalling = false;
	IsMovingOnGround = false;
	ShouldEnterJump = false;
	ShouldDoIKTrace = false;
}

void USkillLabAnimInstance::NativeUpdateAnimation(const float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!Character)
	{
		Character = Cast<ACharacter>(TryGetPawnOwner());
	}
	if (!MovementComponent && Character)
	{
		MovementComponent = Character->GetCharacterMovement();
	}

	const FSkillLabAnimationDefinition& Definition = FSkillLabAnimationDefinitionProvider::Get();
	if (!Character || !MovementComponent)
	{
		Velocity = FVector::ZeroVector;
		GroundSpeed = 0.0;
		Direction = 0.0;
		ShouldMove = false;
		IsFalling = false;
		IsMovingOnGround = false;
		ShouldEnterJump = false;
		ShouldDoIKTrace = false;
		return;
	}

	Velocity = MovementComponent->Velocity;
	GroundSpeed = static_cast<double>(Velocity.Size2D());
	ShouldMove = GroundSpeed > static_cast<double>(Definition.MovementThreshold)
		&& !MovementComponent->GetCurrentAcceleration().Equals(FVector::ZeroVector, 1.0e-4f);
	IsFalling = MovementComponent->IsFalling();
	IsMovingOnGround = !IsFalling && ShouldMove;
	ShouldEnterJump = IsFalling && Velocity.Z > 100.0;
	ShouldDoIKTrace = !IsFalling;

	const float RawDirection = UKismetAnimationLibrary::CalculateDirection(Velocity, Character->GetActorRotation());
	const float SelectedDirection = MovementComponent->bOrientRotationToMovement
		? FMath::Clamp(RawDirection, -Definition.DirectionClampDegrees, Definition.DirectionClampDegrees)
		: RawDirection;
	Direction = static_cast<double>(FMath::Clamp(SelectedDirection, Definition.DirectionMin, Definition.DirectionMax));
}
