// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabCharacterAnimInstance.h"

#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

void USkillLabCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	bHasLuaLocomotionIntent = false;
	bLuaShouldMove = false;
	bLuaIsFalling = false;
	ResolveStateComponent();
}

void USkillLabCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (!StateComponent)
	{
		ResolveStateComponent();
	}
	if (!StateComponent)
	{
		return;
	}

	Snapshot = StateComponent->GetSnapshot();
	PushLegacyAnimBPInputs();
}

void USkillLabCharacterAnimInstance::SetLuaLocomotionIntent(const bool bShouldMove, const bool bIsFalling)
{
	bHasLuaLocomotionIntent = true;
	bLuaShouldMove = bShouldMove;
	bLuaIsFalling = bIsFalling;
	PushLegacyAnimBPInputs();
}

void USkillLabCharacterAnimInstance::ResolveStateComponent()
{
	AActor* Owner = GetOwningActor();
	StateComponent = Owner ? Owner->FindComponentByClass<USkillLabCharacterAnimationStateComponent>() : nullptr;
	if (StateComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimationState] AnimInstance %s consumes %s"),
			*GetNameSafe(this), *GetNameSafe(StateComponent));
	}
}

void USkillLabCharacterAnimInstance::PushLegacyAnimBPInputs()
{
	// ABP_Unarmed keeps its existing locomotion state-machine variables and pose
	// graph. The native parent maps the character-owned snapshot into those
	// animation inputs so the asset does not recalculate movement state.
	if (FNumericProperty* GroundSpeedProperty = CastField<FNumericProperty>(GetClass()->FindPropertyByName(TEXT("GroundSpeed"))))
	{
		GroundSpeedProperty->SetFloatingPointPropertyValue(GroundSpeedProperty->ContainerPtrToValuePtr<void>(this), Snapshot.GroundSpeed);
	}
	if (FNumericProperty* DirectionProperty = CastField<FNumericProperty>(GetClass()->FindPropertyByName(TEXT("Direction"))))
	{
		DirectionProperty->SetFloatingPointPropertyValue(DirectionProperty->ContainerPtrToValuePtr<void>(this), Snapshot.Direction);
	}
	if (FBoolProperty* ShouldMoveProperty = CastField<FBoolProperty>(GetClass()->FindPropertyByName(TEXT("ShouldMove"))))
	{
		ShouldMoveProperty->SetPropertyValue_InContainer(this,
			bHasLuaLocomotionIntent ? bLuaShouldMove : Snapshot.bShouldMove);
	}
	if (FBoolProperty* IsFallingProperty = CastField<FBoolProperty>(GetClass()->FindPropertyByName(TEXT("IsFalling"))))
	{
		IsFallingProperty->SetPropertyValue_InContainer(this,
			bHasLuaLocomotionIntent ? bLuaIsFalling : Snapshot.bIsFalling);
	}
	if (FStructProperty* VelocityProperty = CastField<FStructProperty>(GetClass()->FindPropertyByName(TEXT("Velocity"))))
	{
		if (VelocityProperty->Struct == TBaseStructure<FVector>::Get())
		{
			*VelocityProperty->ContainerPtrToValuePtr<FVector>(this) = Snapshot.Velocity;
		}
	}
}
