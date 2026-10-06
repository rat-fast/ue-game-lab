#include "SkillLabSkillAbility.h"

#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillComponent.h"

USkillLabSkillAbility::USkillLabSkillAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

void USkillLabSkillAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	USkillLabAbilitySystemComponent* ASC = ActorInfo ? Cast<USkillLabAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	USkillLabSkillComponent* SkillComponent = ASC && ASC->GetOwner()
		? ASC->GetOwner()->FindComponentByClass<USkillLabSkillComponent>()
		: nullptr;
	if (!SkillComponent)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	SkillComponent->NotifySkillAbilityActivationPending(this);
}

