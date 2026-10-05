#include "SkillLabPulseAbility.h"

#include "AbilitySystemComponent.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabPulseEffect.h"
#include "SkillLabSkillComponent.h"

USkillLabPulseAbility::USkillLabPulseAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void USkillLabPulseAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	USkillLabAbilitySystemComponent* ASC = ActorInfo ? Cast<USkillLabAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	if (!ASC)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	USkillLabSkillComponent* SkillComponent = ASC->GetOwner() ? ASC->GetOwner()->FindComponentByClass<USkillLabSkillComponent>() : nullptr;
	if (!SkillComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// GAS has accepted the activation path; the skill component finalizes identity,
	// cost and cooldown only after TryActivateAbility returns successfully.
	SkillComponent->NotifyPulseAbilityActivationPending(this);
}

bool USkillLabPulseAbility::ExecutePulseEffect()
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	USkillLabAbilitySystemComponent* ASC = ActorInfo ? Cast<USkillLabAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	if (!ASC)
	{
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, true);
		return false;
	}

	FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(USkillLabPulseEffect::StaticClass(), 1.0f, EffectContext);
	bool bApplied = false;
	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		bApplied = true;
		if (USkillLabSkillComponent* SkillComponent = ASC->GetOwner() ? ASC->GetOwner()->FindComponentByClass<USkillLabSkillComponent>() : nullptr)
		{
			SkillComponent->NotifyPulseEffectApplied();
		}
	}

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
	return bApplied;
}
