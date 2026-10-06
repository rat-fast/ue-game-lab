#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillAbility.h"
#include "SkillLabPulseAbility.generated.h"

UCLASS()
class SKILLLAB_API USkillLabPulseAbility : public USkillLabSkillAbility
{
	GENERATED_BODY()

public:
	virtual bool ApplyEffect(
		const FSkillLabSkillDefinition& Definition,
		const FSkillLabSkillRequest& Request,
		int32 CastId,
		int32 GrantedAbilityId,
		FSkillLabEffectApplicationResult& OutResult) override;
};
