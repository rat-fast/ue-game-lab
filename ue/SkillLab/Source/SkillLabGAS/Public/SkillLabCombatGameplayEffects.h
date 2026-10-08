#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "SkillLabCombatGameplayEffects.generated.h"

UCLASS()
class SKILLLABGAS_API USkillLabDamageGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USkillLabDamageGameplayEffect();
};

UCLASS()
class SKILLLABGAS_API USkillLabHealingGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	USkillLabHealingGameplayEffect();
};
