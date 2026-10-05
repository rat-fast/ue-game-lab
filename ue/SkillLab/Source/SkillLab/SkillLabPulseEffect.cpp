#include "SkillLabPulseEffect.h"

#include "SkillLabAttributeSet.h"

USkillLabPulseEffect::USkillLabPulseEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = USkillLabAttributeSet::GetTestValueAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FScalableFloat(1.0f);
	Modifiers.Add(Modifier);
}
