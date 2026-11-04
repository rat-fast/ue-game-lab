#include "SkillLabSkillBehaviorRegistry.h"

#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillBehavior.h"

void FSkillLabSkillBehaviorRegistry::ApplySharedRuntimeClasses(FSkillLabSkillDefinition& OutDefinition)
{
	OutDefinition.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
	OutDefinition.BehaviorClass = USkillLabLuaSkillBehavior::StaticClass();
}
