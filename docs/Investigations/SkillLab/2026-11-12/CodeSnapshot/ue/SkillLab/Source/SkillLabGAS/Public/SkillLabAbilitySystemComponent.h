#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "SkillLabEnergySettlement.h"
#include "SkillLabAbilitySystemComponent.generated.h"

class USkillLabCombatAttributeSet;
class UGameplayEffect;
struct FGameplayEffectSpec;

// Queued 只代表排队受理，不能用于宣称 Reset/恢复已经完成。
enum class ESkillLabResourceChangeResult : uint8 { Rejected, Queued, Applied };
enum class ESkillLabResourceCompletion : uint8 { Applied, Rejected, Failed, Unavailable };
struct FSkillLabResourceReceipt
{
	uint64 AttemptId = 0;
	FObjectKey ASC;
	ESkillLabResourceCompletion Result = ESkillLabResourceCompletion::Rejected;
	FName Reason;
};

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
	UPROPERTY(BlueprintReadWrite) FGameplayTag AttackCategory;
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
	UPROPERTY(BlueprintReadOnly) FActiveGameplayEffectHandle ActiveEffectHandle;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FSkillLabDamageTaken, FGameplayTag, const FSkillLabDamageResult&);

DECLARE_MULTICAST_DELEGATE_TwoParams(FSkillLabPeriodicDamageResolved, FActiveGameplayEffectHandle, const FSkillLabDamageResult&);

UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLABGAS_API USkillLabAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	USkillLabAbilitySystemComponent();
	FSkillLabDamageTaken OnSkillLabDamageTaken;
	FSkillLabPeriodicDamageResolved OnPeriodicDamageResolved;
	/** AttributeSet supplies the atomic result; GAS supplies the periodic effect identity. */
	void RecordPeriodicDamage(const FGameplayEffectSpec& Spec, const FSkillLabDamageResult& Result);

	/** Initializes owner/avatar information and ensures a combat attribute set exists. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	void InitializeSkillLabAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor);

	UFUNCTION(BlueprintPure, Category = "SkillLab|GAS")
	USkillLabCombatAttributeSet* GetCombatAttributeSet() const;

	/** Applies a damage/healing meta effect to another ASC. The target's AttributeSet resolves mitigation and clamping. */
	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	bool ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, float Amount, UObject* SourceObject = nullptr, FGameplayTag AttackCategory = FGameplayTag());
	FSkillLabDamageResult ResolveDamage(const FSkillLabDamageRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	bool ApplyHealingToTarget(UAbilitySystemComponent* TargetASC, float Amount, UObject* SourceObject = nullptr);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|GAS")
	bool ApplyDamageOverTimeToTarget(UAbilitySystemComponent* TargetASC, float AmountPerTick, float Duration, float Period, UObject* SourceObject = nullptr);
	FSkillLabDamageResult ApplyDamageOverTimeToTarget(const FSkillLabDamageRequest& Request, float Duration, float Period);

	UFUNCTION(BlueprintPure, Category = "SkillLab|GAS")
	bool HasAnySkillLabBlockTag(const FGameplayTagContainer& Tags) const;

	/** Controlled resource writes are applied when they preserve all active reservations. */
	bool RequestResourceChange(bool bMaxEnergy, EGameplayModOp::Type Operation, float Value);
	ESkillLabResourceChangeResult RequestResourceChangeDetailed(bool bMaxEnergy, EGameplayModOp::Type Operation, float Value);
	ESkillLabResourceChangeResult RequestResourceChangeTracked(bool bMaxEnergy, EGameplayModOp::Type Operation, float Value,
		uint64 AttemptId, TWeakObjectPtr<UObject> Owner, TFunction<FName()> BeforeApply,
		TFunction<void(const FSkillLabResourceReceipt&)> OnFinished);
	// 将单个纯数值即时资源 GE spec 转换为协调请求，不授予 Tag/句柄或执行其他 GE 行为。
	// 不支持的持续、混合、计算/组件配置在任何资源变化前拒绝。
	ESkillLabResourceChangeResult RequestResourceEffect(const FGameplayEffectSpec& Spec, FName& OutReason);
	uint64 AllocateEnergyTransactionId();
	/** Energy remains the confirmed GAS value; reservations only reduce the available view. */
	float GetActualEnergy() const;
	float GetReservedEnergy() const;
	float GetAvailableEnergy() const;
	bool BeginEnergyEscrow(uint64 TransactionId, float Cost, uint64 RuntimeGeneration = 0, uint64 CastId = 0);
	bool ResolveEnergyEscrow(uint64 TransactionId, bool bSuccess);
	bool MarkEnergyEscrowIndeterminate(uint64 TransactionId);
	bool ResolveIndeterminateEnergyEscrow(uint64 TransactionId, bool bSucceeded);
	bool IsEnergyEscrowActive() const { return !EnergyReservations.IsEmpty(); }
	bool HasPendingResourceChanges() const { return !PendingResourceChanges.IsEmpty(); }
	bool HasIndeterminateEnergyEscrow(uint64 TransactionId) const;
	bool HasUnsupportedResourceWrite() const { return bUnsupportedResourceWrite; }
	FSkillLabEnergySettlement QueryEnergySettlement(uint64 TransactionId) const;
	bool HasEndedEnergySession() const { return bEnergySessionEnded; }
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;
	bool BindEnergyOperation(uint64 TransactionId, UObject* OperationOwner, UObject* Runtime, uint64 OperationId);


private:
	struct FPendingResourceChange
	{
		bool bMaxEnergy; EGameplayModOp::Type Operation; float Value;
		uint64 AttemptId = 0;
		TWeakObjectPtr<UObject> Owner;
		TFunction<FName()> BeforeApply;
		TFunction<void(const FSkillLabResourceReceipt&)> OnFinished;
	};
	enum class EEnergyReservationState : uint8 { Pending, Settling, Indeterminate };
	struct FEnergyReservation
	{
		float Amount;
		uint64 RuntimeGeneration;
		uint64 CastId;
		EEnergyReservationState State;
		bool bDebitAttempted = false;
		bool bDebitObserved = false;
	};
	bool ApplyResourceChange(const FPendingResourceChange& Change);
	bool ResolveEnergyReservation(uint64 TransactionId, bool bSuccess);
	bool HasSettlingEnergyReservation() const;
	void DrainPendingResourceChanges();
	ESkillLabResourceChangeResult SubmitResourceChange(FPendingResourceChange Change);
	ESkillLabResourceCompletion ExecuteResourceChange(FPendingResourceChange& Change, FName& Reason);
	void FinishResourceChange(FPendingResourceChange& Change, ESkillLabResourceCompletion Result, FName Reason);
	void ClosePendingResourceChanges();
	void OnResourceChanged(const FOnAttributeChangeData& Data);
	bool CanWriteResources() const;
	bool HasResourceAggregation() const;
	bool CanApplyResourceEffect(const FActiveGameplayEffectsContainer& Effects, const FGameplayEffectSpec& Spec) const;
	uint64 NextEnergyTransaction = 1;
	uint64 SettlingEnergyTransactionId = 0;
	uint64 ResourceBypassGeneration = 0;
	uint32 ResourceWriteDepth = 0;
	FGameplayAttribute ExpectedResourceNotification;
	float ExpectedResourceOldValue = 0.0f;
	float ExpectedResourceNewValue = 0.0f;
	bool bAwaitingResourceNotification = false;
	// Sticky for the lifetime of this ASC after any detected bypass write.
	bool bUnsupportedResourceWrite = false;
	TArray<FPendingResourceChange> PendingResourceChanges;
	TMap<uint64, FEnergyReservation> EnergyReservations;
	TMap<uint64, FSkillLabEnergySettlement> EnergySettlementHistory;
	bool bEnergySessionEnded = false;
	bool bDrainingResourceChanges = false;
	TSet<uint64> UsedEnergyTransactionIds;
	TSet<uint64> AllocatedEnergyTransactionIds;
	void HandlePeriodicExecution(UAbilitySystemComponent* Source, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);
	TMap<const FGameplayEffectSpec*, FSkillLabDamageResult> PendingPeriodicDamage;
	bool ApplyMetaEffect(UAbilitySystemComponent* TargetASC, TSubclassOf<UGameplayEffect> EffectClass, float Amount, UObject* SourceObject);
	UPROPERTY(Transient)
	TObjectPtr<USkillLabCombatAttributeSet> CombatAttributeSet;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameplayEffect>> RuntimeEffects;
};
