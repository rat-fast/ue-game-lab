#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SkillLabSkillGameplayAbility.generated.h"

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
	bool IsSkillAbilityActive() const { return bSkillActive; }

private:
	FGameplayAbilitySpecHandle ActiveHandle;
	const FGameplayAbilityActorInfo* ActiveActorInfo = nullptr;
	FGameplayAbilityActivationInfo ActiveActivationInfo;
	bool bSkillActive = false;
};
