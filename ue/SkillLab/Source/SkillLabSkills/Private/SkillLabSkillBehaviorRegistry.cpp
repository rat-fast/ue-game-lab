#include "SkillLabSkillBehaviorRegistry.h"

#include "SkillLabSkillTypes.h"

FSkillLabSkillBehaviorRegistration RegisterSkillLabFireballBehavior();
FSkillLabSkillBehaviorRegistration RegisterSkillLabAreaBurstBehavior();
FSkillLabSkillBehaviorRegistration RegisterSkillLabMendBehavior();
FSkillLabSkillBehaviorRegistration RegisterSkillLabBurningBehavior();

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
	if (SkillId.IsNone() || !Builder)
	{
		return;
	}
	// Repeated module wiring is harmless; a different builder cannot silently replace
	// an already registered skill identity.
	if (const FSkillLabSkillBehaviorBuilder* Existing = GetBuilders().Find(SkillId))
	{
		return;
	}
	GetBuilders().Add(SkillId, Builder);
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
	static bool bBuiltInsRegistered = false;
	if (bBuiltInsRegistered)
	{
		return;
	}
	// These calls are wiring only. Each skill owns its ID and behavior in its own file.
	FSkillLabSkillBehaviorRegistry Registry;
	const TArray<FSkillLabSkillBehaviorRegistrar> Registrars = {
		&RegisterSkillLabFireballBehavior,
		&RegisterSkillLabAreaBurstBehavior,
		&RegisterSkillLabMendBehavior,
		&RegisterSkillLabBurningBehavior
	};
	for (const auto Registrar : Registrars)
	{
		if (Registrar)
		{
			const FSkillLabSkillBehaviorRegistration Registration = Registrar();
			Registry.Register(Registration.SkillId, Registration.Builder);
		}
	}
	bBuiltInsRegistered = true;
}

bool FSkillLabSkillBehaviorRegistry::IsRegistered(const FName SkillId)
{
	return !SkillId.IsNone() && GetBuilders().Contains(SkillId);
}

void FSkillLabSkillBehaviorRegistry::GetRegisteredSkillIds(TArray<FName>& OutIds)
{
	OutIds.Reset();
	GetBuilders().GetKeys(OutIds);
	OutIds.Sort(FNameLexicalLess());
}
