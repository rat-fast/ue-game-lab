#include "SkillLabSkillBehaviorRegistry.h"

#include "SkillLabSkillTypes.h"

void RegisterSkillLabFireballBehavior(FSkillLabSkillBehaviorRegistry& Registry);
void RegisterSkillLabAreaBurstBehavior(FSkillLabSkillBehaviorRegistry& Registry);
void RegisterSkillLabMendBehavior(FSkillLabSkillBehaviorRegistry& Registry);
void RegisterSkillLabBurningBehavior(FSkillLabSkillBehaviorRegistry& Registry);

namespace
{
	TMap<FName, FSkillLabSkillBehaviorBuilder>& GetBuilders()
	{
		static TMap<FName, FSkillLabSkillBehaviorBuilder> Builders;
		return Builders;
	}
}

void FSkillLabSkillBehaviorRegistry::Register(const FName SkillId, const FSkillLabSkillBehaviorBuilder Builder)
{
	if (!SkillId.IsNone() && Builder)
	{
		GetBuilders().Add(SkillId, Builder);
	}
}

bool FSkillLabSkillBehaviorRegistry::Apply(const FName SkillId, FSkillLabSkillDefinition& OutDefinition)
{
	if (const FSkillLabSkillBehaviorBuilder* Builder = GetBuilders().Find(SkillId))
	{
		(*Builder)(OutDefinition);
		return true;
	}
	return false;
}

void FSkillLabSkillBehaviorRegistry::RegisterBuiltInBehaviors()
{
	// These calls are wiring only. Each skill owns its ID and behavior in its own file.
	FSkillLabSkillBehaviorRegistry Registry;
	RegisterSkillLabFireballBehavior(Registry);
	RegisterSkillLabAreaBurstBehavior(Registry);
	RegisterSkillLabMendBehavior(Registry);
	RegisterSkillLabBurningBehavior(Registry);
}
