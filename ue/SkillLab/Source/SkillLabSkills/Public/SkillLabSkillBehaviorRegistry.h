#pragma once

#include "CoreMinimal.h"

struct FSkillLabSkillDefinition;

/** Registry that supplies only stable shared runtime classes, never skill content. */
class SKILLLABSKILLS_API FSkillLabSkillBehaviorRegistry
{
public:
	static void ApplySharedRuntimeClasses(FSkillLabSkillDefinition& OutDefinition);
};
