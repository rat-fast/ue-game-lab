#include "SkillLabPulseEffect.h"

#include "SkillLabAttributeSet.h"

USkillLabPulseEffect::USkillLabPulseEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USkillLabAttributeSet::GetTestValueAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCallerMagnitude;
	SetByCallerMagnitude.DataName = FName(TEXT("SkillLab.EffectMagnitude"));
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerMagnitude);
	Modifiers.Add(Modifier);
}
