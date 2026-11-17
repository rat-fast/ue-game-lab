#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class ASkillLabFireballProjectile;

struct SKILLLABSKILLS_API FSkillLabProjectileObservation
{
	TWeakObjectPtr<ASkillLabFireballProjectile> Projectile;
	TWeakObjectPtr<AActor> Caster;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	TWeakObjectPtr<UWorld> World;
	FName SkillId = NAME_None;
	FSkillLabCastExecutionToken CastToken;
	FSkillLabSubmissionKey SubmissionKey;
	FString ActorPath;
	int32 RequestId = 0;
	int32 CastId = 0;
	uint64 SubmissionPointId = 0;
	FTransform SpawnTransform = FTransform::Identity;
	double ObservationWorldTime = 0.0;
	double ObservationWallTime = 0.0;
	bool bDelivered = false;
	bool bDeliveryAccepted = false;
	bool bRemoved = false;
	FName Reason = NAME_None;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSkillLabProjectileObserved, const FSkillLabProjectileObservation&);

class SKILLLABSKILLS_API FSkillLabSkillObservationBus
{
public:
	static FSkillLabProjectileObserved& OnProjectileObserved();
	static void BroadcastProjectile(const FSkillLabProjectileObservation& Observation);
};
