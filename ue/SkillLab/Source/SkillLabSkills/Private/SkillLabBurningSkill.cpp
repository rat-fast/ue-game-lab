#include "SkillLabBurningSkill.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillBehaviorRegistry.h"
#include "SkillLabSkillRuntimeComponent.h"

namespace
{
	void Configure(FSkillLabSkillDefinition& Definition)
	{
		Definition.TargetShape = ESkillLabTargetShape::Unit;
		Definition.Relation = ESkillLabTargetRelation::Enemy;
		Definition.Execution = ESkillLabExecutionTemplate::InstantTarget;
		Definition.Effect = ESkillLabEffectKind::DamageOverTime;
		Definition.Range = 900.0f;
		Definition.Duration = 4.0f;
		Definition.Period = 1.0f;
		Definition.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		Definition.BehaviorClass = USkillLabBurningBehavior::StaticClass();
	}
}

void USkillLabBurningBehavior::Execute()
{
	if (!IsCurrent()) return;
	ApplyEffect(Request.Target);
	Complete(TEXT("BurningStarted"));
}

FSkillLabSkillBehaviorRegistration RegisterSkillLabBurningBehavior()
{
	return {TEXT("Skill_BurningBuff"), &Configure};
}
