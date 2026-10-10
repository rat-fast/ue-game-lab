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
	USkillLabSkillRuntimeComponent* Skills = Owner ? Owner->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	if (!Skills) { EndAbility(Handle, Info, ActivationInfo, true, true); return; }
	ActiveHandle = Handle;
	ActiveActorInfo = Info;
	ActiveActivationInfo = ActivationInfo;
	bSkillActive = true;
	Skills->NotifyAbilityActivated(this);
}

void USkillLabSkillGameplayAbility::FinishSkillAbility(bool bWasCancelled)
{
	if (bSkillActive) EndAbility(ActiveHandle, ActiveActorInfo, ActiveActivationInfo, true, bWasCancelled);
}

void USkillLabSkillGameplayAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (Info)
	{
		if (AActor* Owner = Info->OwnerActor.Get())
		{
			if (USkillLabSkillRuntimeComponent* Skills = Owner->FindComponentByClass<USkillLabSkillRuntimeComponent>()) Skills->NotifyAbilityEnded(this);
		}
	}
	bSkillActive = false;
	Super::EndAbility(Handle, Info, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
