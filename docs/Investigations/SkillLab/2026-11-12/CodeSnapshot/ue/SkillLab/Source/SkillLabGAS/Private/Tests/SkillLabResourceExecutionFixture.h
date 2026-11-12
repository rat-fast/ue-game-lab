#pragma once

#include "GameplayEffectExecutionCalculation.h"
#include "SkillLabResourceExecutionFixture.generated.h"

UCLASS()
class USkillLabResourceExecutionFixture : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()
public:
	static int32 ExecutionCount;
	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters, FGameplayEffectCustomExecutionOutput& Output) const override;
};
