#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SkillLabPulseAbility.generated.h"

UCLASS()
class SKILLLAB_API USkillLabPulseAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USkillLabPulseAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	bool ExecutePulseEffect();
};
