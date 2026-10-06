#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillAbility.generated.h"

UCLASS(Abstract)
class SKILLLAB_API USkillLabSkillAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	USkillLabSkillAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual bool ApplyEffect(
		const FSkillLabSkillDefinition& Definition,
		const FSkillLabSkillRequest& Request,
		int32 CastId,
		int32 GrantedAbilityId,
		FSkillLabEffectApplicationResult& OutResult) PURE_VIRTUAL(USkillLabSkillAbility::ApplyEffect, return false;);
};
