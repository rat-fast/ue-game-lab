#include "SkillLabProjectileTypes.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

FVector GetSkillLabProjectileAimPoint(const AActor* Actor)
{
	if (!IsValid(Actor)) return FVector::ZeroVector;
	if (const UPrimitiveComponent* RootCollision = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		if (RootCollision->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return RootCollision->CalcBounds(RootCollision->GetComponentTransform()).Origin;
		}
	}
	FVector Center, Extent;
	Actor->GetActorBounds(true, Center, Extent);
	return Extent.IsNearlyZero() ? Actor->GetActorLocation() : Center;
}
