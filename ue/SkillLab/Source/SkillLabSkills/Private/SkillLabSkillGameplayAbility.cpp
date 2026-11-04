#include "SkillLabSkillGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
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

void USkillLabSkillGameplayAbility::BindSkillCast(USkillLabSkillRuntimeComponent* Runtime, const int32 CastId,
	const FGameplayTagContainer& EventTags)
{
	if (!bSkillActive || !Runtime || CastId <= 0)
	{
		return;
	}

	SkillRuntime = Runtime;
	BoundCastId = CastId;
	EventTasks.Reset();
	for (const FGameplayTag EventTag : EventTags)
	{
		if (!EventTag.IsValid())
		{
			continue;
		}
		UAbilityTask_WaitGameplayEvent* WaitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, EventTag, nullptr, false, true);
		if (!WaitTask)
		{
			continue;
		}
		WaitTask->EventReceived.AddDynamic(this, &USkillLabSkillGameplayAbility::HandleSkillGameplayEvent);
		EventTasks.Add(WaitTask);
		WaitTask->ReadyForActivation();
	}
}

void USkillLabSkillGameplayAbility::HandleSkillGameplayEvent(const FGameplayEventData Payload)
{
	const USkillLabCastGameplayEventData* Identity = Cast<USkillLabCastGameplayEventData>(Payload.OptionalObject);
	if (!bSkillActive || BoundCastId <= 0 || !Identity || Identity->CastId != BoundCastId
		|| Identity->Ability.Get() != this || Identity->Runtime != SkillRuntime || !SkillRuntime.IsValid()
		|| Payload.Instigator != GetOwningActorFromActorInfo())
	{
		return;
	}
	SkillRuntime->HandleAbilityGameplayEvent(this, BoundCastId, Payload.EventTag);
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
	BoundCastId = 0;
	SkillRuntime = nullptr;
	EventTasks.Reset();
	Super::EndAbility(Handle, Info, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
