#include "SkillLabRecoverEnergyEffect.h"

#include "SkillLabAttributeSet.h"

USkillLabRecoverEnergyEffect::USkillLabRecoverEnergyEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USkillLabAttributeSet::GetSkillEnergyAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCallerMagnitude;
	SetByCallerMagnitude.DataName = FName(TEXT("SkillLab.EffectMagnitude"));
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);
	Modifiers.Add(Modifier);
}
