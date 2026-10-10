#pragma once

#include "CoreMinimal.h"

class AActor;

/** Uses collision geometry to aim at a unit rather than an actor pivot or visual mesh origin. */
SKILLLABSKILLS_API FVector GetSkillLabProjectileAimPoint(const AActor* Actor);

/** C++ tuning owned by projectile skill behavior, kept outside reflected content data. */
struct SKILLLABSKILLS_API FSkillLabProjectileParameters
{
	float Speed = 1250.0f;
	float CollisionRadius = 18.0f;
	float Lifetime = 5.0f;
	bool bInterceptOtherUnits = true;
	float VisualScale = 0.62f;
};
