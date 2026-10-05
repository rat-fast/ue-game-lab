#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "SkillLabAbilitySystemComponent.generated.h"

UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	USkillLabAbilitySystemComponent();
};
