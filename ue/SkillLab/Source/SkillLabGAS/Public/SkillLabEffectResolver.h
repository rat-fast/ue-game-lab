#pragma once

#include "CoreMinimal.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabEffectResolver.generated.h"

/** The effect operation is deliberately independent of the skill module. Skills describe
 * intent; this module remains the single gateway to attribute/effect application. */
UENUM(BlueprintType)
enum class ESkillLabEffectOperation : uint8
{
	Damage,
	Healing,
	DamageOverTime
};

USTRUCT(BlueprintType)
struct SKILLLABGAS_API FSkillLabEffectRequest
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite) TObjectPtr<USkillLabAbilitySystemComponent> SourceASC = nullptr;
	UPROPERTY(BlueprintReadWrite) TObjectPtr<USkillLabAbilitySystemComponent> TargetASC = nullptr;
	UPROPERTY(BlueprintReadWrite) ESkillLabEffectOperation Operation = ESkillLabEffectOperation::Damage;
	UPROPERTY(BlueprintReadWrite) float Magnitude = 0.0f;
	UPROPERTY(BlueprintReadWrite) float PowerCoefficient = 0.0f;
	UPROPERTY(BlueprintReadWrite) float Duration = 0.0f;
	UPROPERTY(BlueprintReadWrite) float Period = 0.0f;
	UPROPERTY(BlueprintReadWrite) ESkillLabDamageType DamageType = ESkillLabDamageType::Physical;
	UPROPERTY(BlueprintReadWrite) TObjectPtr<UObject> SourceObject = nullptr;
};

USTRUCT(BlueprintType)
struct SKILLLABGAS_API FSkillLabEffectResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bApplied = false;
	UPROPERTY(BlueprintReadOnly) bool bDurationEffect = false;
	UPROPERTY(BlueprintReadOnly) FName Reason = NAME_None;
	UPROPERTY(BlueprintReadOnly) float RequestedAmount = 0.0f;
	UPROPERTY(BlueprintReadOnly) float ActualAmount = 0.0f;
	UPROPERTY(BlueprintReadOnly) float HealthBefore = 0.0f;
	UPROPERTY(BlueprintReadOnly) float HealthAfter = 0.0f;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> TargetActor = nullptr;
	UPROPERTY(BlueprintReadOnly) FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) FActiveGameplayEffectHandle ActiveEffectHandle;
};

UCLASS()
class SKILLLABGAS_API USkillLabEffectResolver : public UObject
{
	GENERATED_BODY()
public:
	/** Applies one effect intent through the GAS component. No skill-specific branching belongs here. */
	UFUNCTION(BlueprintCallable, Category="SkillLab|GAS")
	static FSkillLabEffectResult Apply(const FSkillLabEffectRequest& Request);
};
