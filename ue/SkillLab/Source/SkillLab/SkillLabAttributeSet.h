#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SkillLabAttributeSet.generated.h"

UCLASS()
class SKILLLAB_API USkillLabAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USkillLabAttributeSet();

	static float GetDefaultTestValue();
	static float GetDefaultSkillEnergy();

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Attributes")
	FGameplayAttributeData TestValue;
	ATTRIBUTE_ACCESSORS_BASIC(USkillLabAttributeSet, TestValue)

	UPROPERTY(BlueprintReadOnly, Category = "SkillLab|Attributes")
	FGameplayAttributeData SkillEnergy;
	ATTRIBUTE_ACCESSORS_BASIC(USkillLabAttributeSet, SkillEnergy)
};
