#include "SkillLabSkillDefinitionProvider.h"

#include "SkillLabPulseAbility.h"
#include "SkillLabPulseEffect.h"
#include "SkillLabRecoverEnergyAbility.h"
#include "SkillLabRecoverEnergyEffect.h"

namespace
{
	const FName TestPulseSkillId(TEXT("Skill_TestPulse"));
	const FName RecoverEnergySkillId(TEXT("Skill_RecoverEnergy"));
}

FName FSkillLabSkillDefinitionProvider::GetDefaultSkillId()
{
	return TestPulseSkillId;
}

FName FSkillLabSkillDefinitionProvider::GetRecoverEnergySkillId()
{
	return RecoverEnergySkillId;
}

void FSkillLabSkillDefinitionProvider::GetDefaultSkillIds(TArray<FName>& OutSkillIds)
{
	OutSkillIds.Reset();
	OutSkillIds.Add(TestPulseSkillId);
	OutSkillIds.Add(RecoverEnergySkillId);
}

ESkillLabDefinitionStatus FSkillLabSkillDefinitionProvider::ResolveDefinition(const FName SkillId, FSkillLabSkillDefinition& OutDefinition)
{
	OutDefinition = FSkillLabSkillDefinition();
	if (SkillId != TestPulseSkillId && SkillId != RecoverEnergySkillId)
	{
		if (SkillId == FName(TEXT("Skill_DisabledSample")))
		{
			OutDefinition.SkillId = SkillId;
			OutDefinition.Version = 1;
			OutDefinition.DefinitionSource = TEXT("CppTypeProvider");
			OutDefinition.bEnabled = false;
			return ESkillLabDefinitionStatus::Disabled;
		}

		if (SkillId == FName(TEXT("Skill_InvalidSample")))
		{
			OutDefinition.SkillId = SkillId;
			OutDefinition.Version = 0;
			OutDefinition.DefinitionSource = TEXT("CppTypeProvider");
			OutDefinition.bEnabled = true;
			return ESkillLabDefinitionStatus::Invalid;
		}

		return ESkillLabDefinitionStatus::Missing;
	}

	OutDefinition.SkillId = SkillId;
	OutDefinition.Version = 1;
	OutDefinition.DefinitionSource = TEXT("CppTypeProvider");
	OutDefinition.ExecutionTemplate = TEXT("InstantGameplayEffect");
	OutDefinition.PresentationBinding = SkillId == TestPulseSkillId ? TEXT("Skill.TestPulse") : TEXT("Skill.RecoverEnergy");
	OutDefinition.TargetType = ESkillLabTargetType::Self;
	if (SkillId == TestPulseSkillId)
	{
		OutDefinition.ResourceCost = 10.0f;
		OutDefinition.CooldownSeconds = 0.5f;
		OutDefinition.EffectType = TEXT("TestValueAdd");
		OutDefinition.EffectMagnitude = 1.0f;
		OutDefinition.AbilityClass = USkillLabPulseAbility::StaticClass();
		OutDefinition.EffectClass = USkillLabPulseEffect::StaticClass();
	}
	else
	{
		OutDefinition.ResourceCost = 5.0f;
		OutDefinition.CooldownSeconds = 0.25f;
		OutDefinition.EffectType = TEXT("SkillEnergyAdd");
		OutDefinition.EffectMagnitude = 20.0f;
		OutDefinition.AbilityClass = USkillLabRecoverEnergyAbility::StaticClass();
		OutDefinition.EffectClass = USkillLabRecoverEnergyEffect::StaticClass();
	}
	OutDefinition.bEnabled = true;

	if (!OutDefinition.AbilityClass || !OutDefinition.EffectClass || OutDefinition.Version <= 0 || OutDefinition.EffectType.IsNone())
	{
		return ESkillLabDefinitionStatus::Invalid;
	}
	if (!OutDefinition.bEnabled)
	{
		return ESkillLabDefinitionStatus::Disabled;
	}
	return ESkillLabDefinitionStatus::Available;
}
