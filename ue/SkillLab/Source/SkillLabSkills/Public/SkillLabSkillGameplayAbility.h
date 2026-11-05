#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SkillLabSkillGameplayAbility.generated.h"

class UAbilityTask_WaitGameplayEvent;
class USkillLabSkillRuntimeComponent;
class USkillLabSkillGameplayAbility;

/** Exact identity carried by the local ASC event; never encoded in float magnitude. */
UCLASS()
class SKILLLABSKILLS_API USkillLabCastGameplayEventData : public UObject
{
	GENERATED_BODY()
public:
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	TWeakObjectPtr<USkillLabSkillGameplayAbility> Ability;
	int32 CastId = 0;
};

UCLASS()
class SKILLLABSKILLS_API USkillLabSkillGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()
public:
	USkillLabSkillGameplayAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	void FinishSkillAbility(bool bWasCancelled);
	void BindSkillCast(USkillLabSkillRuntimeComponent* Runtime, int32 CastId, const FGameplayTagContainer& EventTags);
	bool IsSkillAbilityActive() const { return bSkillActive; }

private:
	friend class USkillLabSkillRuntimeComponent;
	friend class FSkillLabCastGameplayEventTest;
	friend class FSkillLabBufferedIntentConsumeCommand;
	UFUNCTION()
	void HandleSkillGameplayEvent(FGameplayEventData Payload);

	FGameplayAbilitySpecHandle ActiveHandle;
	const FGameplayAbilityActorInfo* ActiveActorInfo = nullptr;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	UPROPERTY()
	TArray<TObjectPtr<UAbilityTask_WaitGameplayEvent>> EventTasks;
	int32 BoundCastId = 0;
	bool bSkillActive = false;
};
