#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponent.h"
#include "GameFramework/Actor.h"
#include <limits>

namespace
{
	bool IsResourceAttribute(const FGameplayAttribute& Attribute)
	{
		return Attribute == USkillLabCombatAttributeSet::GetEnergyAttribute()
			|| Attribute == USkillLabCombatAttributeSet::GetMaxEnergyAttribute();
	}
}

bool USkillLabAbilitySystemComponent::CanWriteResources() const
{
	return IsValid(this) && !IsBeingDestroyed() && !bEnergySessionEnded
		&& IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed() && GetOwner()->HasAuthority()
		&& IsValid(GetCombatAttributeSet());
}

bool USkillLabAbilitySystemComponent::HasResourceAggregation() const
{
	const auto* Attributes = GetCombatAttributeSet();
	if (!Attributes) return true;
	if (!FMath::IsFinite(Attributes->GetEnergy()) || !FMath::IsFinite(Attributes->GetMaxEnergy())
		|| !FMath::IsNearlyEqual(GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()), Attributes->GetEnergy())
		|| !FMath::IsNearlyEqual(GetNumericAttributeBase(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()), Attributes->GetMaxEnergy())) return true;
	// 即使多个聚合的净差恰好为零，也不能覆写 base 后让移除/过期破坏预留。
	for (const auto Handle : GetActiveEffects(FGameplayEffectQuery()))
	{
		const auto* Effect = GetActiveGameplayEffect(Handle);
		if (!Effect || !Effect->Spec.Def) continue;
		for (const auto& Modifier : Effect->Spec.Def->Modifiers)
			if (IsResourceAttribute(Modifier.Attribute)) return true;
	}
	return false;
}

bool USkillLabAbilitySystemComponent::CanApplyResourceEffect(const FActiveGameplayEffectsContainer&,
	const FGameplayEffectSpec& Spec) const
{
	if (!Spec.Def) return false;
	// 这是引擎真实 ApplySpec 的前置查询，覆盖 Self/Target/BP 路径；不隐藏非 virtual 原生函数。
	for (const auto& Modifier : Spec.Def->Modifiers)
	{
		if (IsResourceAttribute(Modifier.Attribute))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] Resource GE rejected before application; use RequestResourceEffect for a pure instant numeric spec, or RequestResourceChange: %s"), *GetNameSafe(Spec.Def));
			return false;
		}
	}
	return true;
}

ESkillLabResourceChangeResult USkillLabAbilitySystemComponent::RequestResourceEffect(const FGameplayEffectSpec& Spec, FName& OutReason)
{
	OutReason = TEXT("ResourceEffectUnsupported");
	const UGameplayEffect* Definition = Spec.Def;
	FGameplayTagContainer GrantedTags;
	Spec.GetAllGrantedTags(GrantedTags);
	if (!Definition || Definition->DurationPolicy != EGameplayEffectDurationType::Instant || Spec.GetPeriod() != 0.0f
		|| Spec.GetStackCount() != 1 || Definition->Modifiers.Num() != 1 || !Definition->Executions.IsEmpty()
		|| Definition->FindComponent(UGameplayEffectComponent::StaticClass()) || !GrantedTags.IsEmpty()
		|| !Definition->ConditionalGameplayEffects.IsEmpty() || !Definition->OverflowEffects.IsEmpty()) return ESkillLabResourceChangeResult::Rejected;
	const auto& Modifier = Definition->Modifiers[0];
	const auto Calculation = Modifier.ModifierMagnitude.GetMagnitudeCalculationType();
	if (!IsResourceAttribute(Modifier.Attribute)
		|| (Modifier.ModifierOp != EGameplayModOp::Additive && Modifier.ModifierOp != EGameplayModOp::Override)
		|| (Calculation != EGameplayEffectMagnitudeCalculation::ScalableFloat && Calculation != EGameplayEffectMagnitudeCalculation::SetByCaller)
		|| !Modifier.SourceTags.IsEmpty() || !Modifier.TargetTags.IsEmpty()) return ESkillLabResourceChangeResult::Rejected;
	float Value = 0.0f;
	if (!Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Value, false, std::numeric_limits<float>::quiet_NaN()) || !FMath::IsFinite(Value))
	{
		OutReason = TEXT("ResourceEffectMagnitudeInvalid");
		return ESkillLabResourceChangeResult::Rejected;
	}
	const auto Result = RequestResourceChangeDetailed(Modifier.Attribute == USkillLabCombatAttributeSet::GetMaxEnergyAttribute(), Modifier.ModifierOp, Value);
	OutReason = Result == ESkillLabResourceChangeResult::Applied ? TEXT("ResourceChangeApplied")
		: (Result == ESkillLabResourceChangeResult::Queued ? TEXT("ResourceChangeQueued") : TEXT("ResourceChangeRejected"));
	return Result;
}
