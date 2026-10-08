#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "SkillLabAbilitySystemComponent.generated.h"

class USkillLabCombatAttributeSet;
class UGameplayEffect;

UENUM(BlueprintType)
enum class ESkillLabDamageType : uint8
{
	Physical,
	Magical,
	Pure
};

USTRUCT(BlueprintType)
struct SKILLLABGAS_API FSkillLabDamageRequest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) TObjectPtr<UAbilitySystemComponent> SourceASC = nullptr;
	UPROPERTY(BlueprintReadWrite) TObjectPtr<UAbilitySystemComponent> TargetASC = nullptr;
	UPROPERTY(BlueprintReadWrite) float BaseDamage = 0.0f;
	UPROPERTY(BlueprintReadWrite) float PowerCoefficient = 0.0f;
	UPROPERTY(BlueprintReadWrite) ESkillLabDamageType DamageType = ESkillLabDamageType::Physical;
	UPROPERTY(BlueprintReadWrite) TObjectPtr<UObject> SourceObject = nullptr;
};

USTRUCT(BlueprintType)
struct SKILLLABGAS_API FSkillLabDamageResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bApplied = false;
	UPROPERTY(BlueprintReadOnly) float HealthBefore = 0.0f;
	UPROPERTY(BlueprintReadOnly) float HealthAfter = 0.0f;
	UPROPERTY(BlueprintReadOnly) float RawDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly) float MitigatedDamage = 0.0f;
	UPROPERTY(BlueprintReadOnly) float AppliedDamage = 0.0f;
};

UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLABGAS_API USkillLabAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	USkillLabAbilitySystemComponent();

	/** Initializes owner/avatar information and ensures a combat attribute set exists. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	void InitializeSkillLabAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

	UFUNCTION(BlueprintPure, Category = "SkillLab|GAS")
	USkillLabCombatAttributeSet* GetCombatAttributeSet() const;

	/** Applies a damage/healing meta effect to another ASC. The target's AttributeSet resolves mitigation and clamping. */
	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	bool ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, float Amount, UObject* SourceObject = nullptr);
	FSkillLabDamageResult ResolveDamage(const FSkillLabDamageRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	bool ApplyHealingToTarget(UAbilitySystemComponent* TargetASC, float Amount, UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	bool ApplyDamageOverTimeToTarget(UAbilitySystemComponent* TargetASC, float AmountPerTick, float Duration, float Period, UObject* SourceObject = nullptr);
	FSkillLabDamageResult ApplyDamageOverTimeToTarget(const FSkillLabDamageRequest& Request, float Duration, float Period);

	UFUNCTION(BlueprintPure, Category = "SkillLab|GAS")
	bool HasAnySkillLabBlockTag(const FGameplayTagContainer& Tags) const;

private:
	bool ApplyMetaEffect(UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> EffectClass, float Amount, UObject* SourceObject);
	UPROPERTY(Transient)
	TObjectPtr<USkillLabCombatAttributeSet> CombatAttributeSet;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameplayEffect>> RuntimeEffects;
};
