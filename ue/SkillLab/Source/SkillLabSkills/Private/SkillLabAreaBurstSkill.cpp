#include "SkillLabAreaBurstSkill.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillBehaviorRegistry.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabTargetingService.h"

namespace
{
	void Configure(FSkillLabSkillDefinition& Definition)
	{
		Definition.TargetShape = ESkillLabTargetShape::Ground;
		Definition.Relation = ESkillLabTargetRelation::Enemy;
		Definition.Execution = ESkillLabExecutionTemplate::GroundArea;
		Definition.Effect = ESkillLabEffectKind::Damage;
		Definition.Range = 1000.0f;
		Definition.Radius = 350.0f;
		Definition.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		Definition.BehaviorClass = USkillLabLuaSkillBehavior::StaticClass();
	}
}

void USkillLabAreaBurstBehavior::Execute()
{
	if (!IsCurrent() || !Runtime) return;
	TArray<AActor*> Targets;
	FSkillLabTargetingService::FindAreaTargets(*Runtime, Request.TargetLocation, Definition, Targets);
	for (AActor* Target : Targets) ApplyEffect(Target);
	if (Targets.Num() == 0) ReportMiss(TEXT("AreaEmpty"));
	Complete(Targets.Num() > 0 ? TEXT("AreaHit") : TEXT("AreaEmpty"));
}

FSkillLabSkillBehaviorRegistration RegisterSkillLabAreaBurstBehavior()
{
	return {TEXT("Skill_AreaBurst"), &Configure};
}
