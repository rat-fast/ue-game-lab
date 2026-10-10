#pragma once

#include "CoreMinimal.h"

struct FSkillLabSkillDefinition;

/** A skill-owned behavior descriptor applied after content values are loaded. */
using FSkillLabSkillBehaviorBuilder = void(*)(FSkillLabSkillDefinition&);

/** Registry that combines JSON content values with stable C++ skill behavior. */
class SKILLLABSKILLS_API FSkillLabSkillBehaviorRegistry
{
public:
	static void Register(FName SkillId, FSkillLabSkillBehaviorBuilder Builder);
	static bool Apply(FName SkillId, FSkillLabSkillDefinition& OutDefinition);
	static void RegisterBuiltInBehaviors();
};
