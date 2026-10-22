#include "SkillLabMendSkill.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillBehaviorRegistry.h"
#include "SkillLabSkillRuntimeComponent.h"

namespace
{
	void Configure(FSkillLabSkillDefinition& Definition)
	{
		Definition.TargetShape = ESkillLabTargetShape::Self;
		Definition.Relation = ESkillLabTargetRelation::Self;
		Definition.Execution = ESkillLabExecutionTemplate::InstantTarget;
		Definition.Effect = ESkillLabEffectKind::Healing;
		Definition.Range = 800.0f;
		Definition.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		Definition.BehaviorClass = USkillLabLuaSkillBehavior::StaticClass();
	}
}

void USkillLabMendBehavior::Execute()
{
	if (!IsCurrent()) return;
	ApplyEffect(Runtime ? Runtime->GetOwner() : nullptr);
	Complete(TEXT("HealingApplied"));
}

FSkillLabSkillBehaviorRegistration RegisterSkillLabMendBehavior()
{
	return {TEXT("Skill_Mend"), &Configure};
}
