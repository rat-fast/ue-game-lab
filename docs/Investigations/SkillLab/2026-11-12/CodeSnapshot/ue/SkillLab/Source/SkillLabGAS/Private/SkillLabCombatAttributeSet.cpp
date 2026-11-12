#include "SkillLabCombatAttributeSet.h"

#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemComponent.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabDamageCalculation.h"
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

bool USkillLabCombatAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data)
{
	if (Data.EvaluatedData.Attribute == GetEnergyAttribute() || Data.EvaluatedData.Attribute == GetMaxEnergyAttribute())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] GE resource output rejected before modification: %s"), *Data.EvaluatedData.Attribute.GetName());
		return false;
	}
	return Super::PreGameplayEffectExecute(Data);
}

void USkillLabCombatAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float RawDamage = FMath::Max(0.0f, GetIncomingDamage());
		SetIncomingDamage(0.0f);
		const SkillLabDamageCalculation::FResult Settlement = SkillLabDamageCalculation::Resolve(GetHealth(), GetMaxHealth(), GetArmor(), RawDamage);
		if (USkillLabAbilitySystemComponent* ASC = Cast<USkillLabAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			FSkillLabDamageResult Result;
			Result.bApplied = true;
			Result.HealthBefore = Settlement.HealthBefore;
			Result.HealthAfter = Settlement.HealthAfter;
			Result.RawDamage = Settlement.RawDamage;
			Result.MitigatedDamage = Settlement.MitigatedDamage;
			Result.AppliedDamage = Settlement.AppliedDamage;
			ASC->RecordPeriodicDamage(Data.EffectSpec, Result);
		}
		SetHealth(Settlement.HealthAfter);
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
	ClampVitals();
}

void USkillLabCombatAttributeSet::ClampVitals()
{
	SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));
	const float ClampedEnergy = FMath::Clamp(GetEnergy(), 0.0f, GetMaxEnergy());
	if (!FMath::IsNearlyEqual(GetEnergy(), ClampedEnergy))
	{
		if (USkillLabAbilitySystemComponent* SkillASC = Cast<USkillLabAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			const ESkillLabResourceChangeResult Result = SkillASC->RequestResourceChangeDetailed(false, EGameplayModOp::Override, ClampedEnergy);
			if (Result != ESkillLabResourceChangeResult::Applied)
				UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] ClampVitals resource correction not completed: Result=%d"), static_cast<int32>(Result));
		}
		else SetEnergy(ClampedEnergy);
	}

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
