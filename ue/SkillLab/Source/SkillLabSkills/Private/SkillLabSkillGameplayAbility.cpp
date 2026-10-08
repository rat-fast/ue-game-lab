#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillRuntimeComponent.h"

USkillLabSkillGameplayAbility::USkillLabSkillGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
}

void USkillLabSkillGameplayAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData*)
{
	AActor* Owner = Info ? Info->OwnerActor.Get() : nullptr;
	if (USkillLabSkillRuntimeComponent* Skills = Owner ? Owner->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr)
	{
		Skills->NotifyAbilityActivated(this);
		EndAbility(Handle, Info, ActivationInfo, true, false);
	}
	else EndAbility(Handle, Info, ActivationInfo, true, true);
}
