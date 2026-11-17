#include "SkillLabSkillObservation.h"

FSkillLabProjectileObserved& FSkillLabSkillObservationBus::OnProjectileObserved()
{
	static FSkillLabProjectileObserved Delegate;
	return Delegate;
}

void FSkillLabSkillObservationBus::BroadcastProjectile(const FSkillLabProjectileObservation& Observation)
{
	OnProjectileObserved().Broadcast(Observation);
}
