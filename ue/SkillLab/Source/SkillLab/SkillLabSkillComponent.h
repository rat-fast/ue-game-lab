#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabSkillComponent.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabAttributeSet;
class USkillLabPulseAbility;

UENUM(BlueprintType)
enum class ESkillLabRequestSource : uint8
{
	Test,
	Player,
	AI
};

UENUM(BlueprintType)
enum class ESkillLabTargetType : uint8
{
	Self,
	Unit
};

UENUM(BlueprintType)
enum class ESkillLabRequestStatus : uint8
{
	Rejected,
	Started,
	Completed
};

USTRUCT(BlueprintType)
struct FSkillLabRequestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ESkillLabRequestStatus Status = ESkillLabRequestStatus::Rejected;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CastId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 UnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabRequestSource Source = ESkillLabRequestSource::Test;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabTargetType TargetType = ESkillLabTargetType::Self;
};

UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabSkillComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "SkillLab|Skill")
	void InitializeUnit();

	UFUNCTION(BlueprintCallable, Category = "SkillLab|Skill")
	FSkillLabRequestResult RequestTestPulse(AActor* Target = nullptr);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|Skill")
	void ResetUnit();

	UFUNCTION(BlueprintPure, Category = "SkillLab|Skill")
	float GetTestValue() const;

	UFUNCTION(BlueprintPure, Category = "SkillLab|Skill")
	float GetSkillEnergy() const;

	void NotifyPulseAbilityActivationPending(USkillLabPulseAbility* Ability);
	bool ExecutePulseActivation(USkillLabPulseAbility* Ability, FSkillLabRequestResult& Result);
	void NotifyPulseEffectApplied();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 UnitId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	bool bSkillGranted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 NextRequestId = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 NextCastId = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 ActiveCastId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 CurrentRequestId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	FName CurrentSkillId = TEXT("Skill_TestPulse");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	ESkillLabRequestSource CurrentSource = ESkillLabRequestSource::Test;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	ESkillLabTargetType CurrentTargetType = ESkillLabTargetType::Self;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabPulseAbility> PendingPulseAbility;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabAttributeSet> AttributeSet;

	float CooldownEndTime = 0.0f;
	float InitialEnergy = 100.0f;
	float InitialTestValue = 0.0f;
	float PulseCost = 10.0f;
	float PulseCooldown = 0.5f;

	void LogEvent(const TCHAR* EventName, const FString& Detail = FString()) const;
	FString BuildTraceContext() const;
};
