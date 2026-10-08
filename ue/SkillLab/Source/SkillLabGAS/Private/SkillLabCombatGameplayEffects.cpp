#include "SkillLabCombatGameplayEffects.h"

#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"

namespace
{
	void ConfigureMetaEffect(UGameplayEffect& Effect, const FGameplayAttribute& Attribute)
	{
		Effect.DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataTag = SkillLabGASTags::Data_Magnitude;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
		Effect.Modifiers.Add(Modifier);
	}
}

USkillLabDamageGameplayEffect::USkillLabDamageGameplayEffect()
{
	ConfigureMetaEffect(*this, USkillLabCombatAttributeSet::GetIncomingDamageAttribute());
}

USkillLabHealingGameplayEffect::USkillLabHealingGameplayEffect()
{
	ConfigureMetaEffect(*this, USkillLabCombatAttributeSet::GetIncomingHealingAttribute());
}
