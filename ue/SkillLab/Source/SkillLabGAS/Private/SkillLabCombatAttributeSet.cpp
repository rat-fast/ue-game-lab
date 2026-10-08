#include "SkillLabCombatAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "SkillLabGASTags.h"

USkillLabCombatAttributeSet::USkillLabCombatAttributeSet()
	: Health(100.0f)
	, MaxHealth(100.0f)
	, Energy(100.0f)
	, MaxEnergy(100.0f)
	, Armor(0.0f)
	, Power(10.0f)
{
}

void USkillLabCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxEnergyAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
	else if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetEnergyAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxEnergy());
	}
	else if (Attribute == GetArmorAttribute() || Attribute == GetPowerAttribute())
	{
		NewValue = FMath::Max(0.0f, NewValue);
	}
}

void USkillLabCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float RawDamage = FMath::Max(0.0f, GetIncomingDamage());
		SetIncomingDamage(0.0f);
		const float MitigatedDamage = FMath::Max(0.0f, RawDamage - GetArmor());
		SetHealth(FMath::Clamp(GetHealth() - MitigatedDamage, 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		const float Healing = FMath::Max(0.0f, GetIncomingHealing());
		SetIncomingHealing(0.0f);
		SetHealth(FMath::Clamp(GetHealth() + Healing, 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	}
	else if (Data.EvaluatedData.Attribute == GetEnergyAttribute())
	{
		SetEnergy(FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy()));
	}
	ClampVitals();
}

void USkillLabCombatAttributeSet::ClampVitals()
{
	SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	SetEnergy(FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy()));

	if (UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent())
	{
		const bool bDead = GetHealth() <= 0.0f;
		const bool bHasDeadTag = ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead);
		if (bDead && !bHasDeadTag)
		{
			ASC->AddLooseGameplayTag(SkillLabGASTags::State_Dead);
		}
		else if (!bDead && bHasDeadTag)
		{
			ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Dead);
		}
	}
}
