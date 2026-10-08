#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.generated.h"

#define SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class SKILLLABGAS_API USkillLabCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	USkillLabCombatAttributeSet();

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals")
	FGameplayAttributeData Health;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, Health)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Vitals")
	FGameplayAttributeData MaxHealth;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, MaxHealth)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Resources")
	FGameplayAttributeData Energy;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, Energy)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Resources")
	FGameplayAttributeData MaxEnergy;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, MaxEnergy)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Armor;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, Armor)

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Power;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, Power)

	UPROPERTY()
	FGameplayAttributeData IncomingDamage;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, IncomingDamage)

	UPROPERTY()
	FGameplayAttributeData IncomingHealing;
	SKILLLAB_GAS_ATTRIBUTE_ACCESSORS(USkillLabCombatAttributeSet, IncomingHealing)

private:
	void ClampVitals();
};

#undef SKILLLAB_GAS_ATTRIBUTE_ACCESSORS
