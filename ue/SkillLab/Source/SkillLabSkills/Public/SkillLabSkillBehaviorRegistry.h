#pragma once

#include "CoreMinimal.h"

struct FSkillLabSkillDefinition;

/** A skill-owned behavior descriptor applied after content values are loaded. */
using FSkillLabSkillBehaviorBuilder = void(*)(FSkillLabSkillDefinition&);

/** One self-contained registration entry supplied by a skill behavior module. */
struct SKILLLABSKILLS_API FSkillLabSkillBehaviorRegistration
{
	FName SkillId = NAME_None;
	FSkillLabSkillBehaviorBuilder Builder = nullptr;
};
using FSkillLabSkillBehaviorRegistrar = FSkillLabSkillBehaviorRegistration(*)();

/** Registry that combines JSON content values with stable C++ skill behavior. */
class SKILLLABSKILLS_API FSkillLabSkillBehaviorRegistry
{
public:
	static void Register(FName SkillId, FSkillLabSkillBehaviorBuilder Builder);
	static bool Apply(FName SkillId, FSkillLabSkillDefinition& OutDefinition);
	static void RegisterBuiltInBehaviors();
	static bool IsRegistered(FName SkillId);
	static void GetRegisteredSkillIds(TArray<FName>& OutIds);
};
