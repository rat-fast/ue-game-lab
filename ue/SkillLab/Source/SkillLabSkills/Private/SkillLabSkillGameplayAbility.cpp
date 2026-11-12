#include "SkillLabSkillGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "SkillLabSkillRuntimeComponent.h"

USkillLabSkillGameplayAbility::USkillLabSkillGameplayAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerExecution;
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

void USkillLabSkillGameplayAbility::BindSkillCast(USkillLabSkillRuntimeComponent* Runtime, const FSkillLabCastExecutionToken& Token,
	const FGameplayTagContainer& EventTags)
{
	if (!bSkillActive || !Runtime || !Token.IsValid())
	{
		return;
	}

	SkillRuntime = Runtime;
	BoundCastToken = Token;
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
	const auto* Identity = Cast<USkillLabCastGameplayEventData>(Payload.OptionalObject);
	if (!bSkillActive || !BoundCastToken.IsValid() || !Identity || Identity->CastId != BoundCastToken.CastId
		|| !(Identity->CastToken == BoundCastToken) || Identity->Ability.Get() != this || Identity->Runtime != SkillRuntime
		|| !SkillRuntime.IsValid() || !SkillRuntime->FindExecution(BoundCastToken)
		|| Payload.Instigator != GetOwningActorFromActorInfo()) return;
	SkillRuntime->HandleAbilityGameplayEvent(this, BoundCastToken.CastId, Payload.EventTag);
}

void USkillLabSkillGameplayAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
 // 在通知 Runtime 前封闭 task/End 重入；通知始终发给最初绑定的 Runtime。
 bSkillActive = false;
 for (const auto& Task : EventTasks) if (Task) Task->EndTask();
 EventTasks.Reset();
 if (SkillRuntime.IsValid()) SkillRuntime->NotifyAbilityEnded(this);
 BoundCastToken = {};
 SkillRuntime.Reset();
 Super::EndAbility(Handle, Info, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
