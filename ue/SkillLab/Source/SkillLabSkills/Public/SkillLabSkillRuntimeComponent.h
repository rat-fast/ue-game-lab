#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabCastLifecycle.h"
#include "SkillLabCastExecution.h"
#include "SkillLabSubmissionResults.h"
#include "SkillLabSkillRuntimeComponent.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabSkillGameplayAbility;
class USkillLabSkillBehavior;
class USkillLabSkillEffectEventAdapter;
class USkillLabSkillEffectExecutor;
class USkillLabSkillLoadout;
class USkillLabSkillRuntimeComponent;
class USkillLabUnconfirmedSubmissions;
struct FSkillLabDamageResult;
struct FSkillLabResourceReceipt;

/**
 * Stable, read-only context exposed to effect execution.
 *
 * The runtime owns cast state and the executor only borrows the context while
 * applying a synchronous effect.  Keeping this boundary separate prevents the
 * executor from reaching into lifecycle implementation details and gives the
 * runtime an explicit shutdown point for any future asynchronous executor.
 */
class SKILLLABSKILLS_API ISkillLabSkillCastContext
{
public:
	virtual ~ISkillLabSkillCastContext() = default;
	virtual bool IsBehaviorCurrent(const USkillLabSkillBehavior* Behavior, int32 CastId) const = 0;
	virtual USkillLabAbilitySystemComponent* GetSourceAbilitySystemComponent() const = 0;
	virtual AActor* GetSourceActor() const = 0;
	virtual int32 GetSourceUnitId() const = 0;
	virtual bool MatchesTargetRelation(const USkillLabSkillRuntimeComponent* Target,
		ESkillLabTargetRelation Relation) const = 0;
};

/** Shared orchestration. No concrete skill identity or delivery implementation belongs here. */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLABSKILLS_API USkillLabSkillRuntimeComponent : public UActorComponent, public ISkillLabSkillCastContext
{
	GENERATED_BODY()
public:
	USkillLabSkillRuntimeComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillLab|Unit") int32 TeamId = 1;
	UPROPERTY(BlueprintAssignable, Category="SkillLab|Skill") FSkillLabSkillEventDelegate OnSkillEvent;
	FSkillLabNativeSkillEvent OnNativeSkillEvent;

	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void InitializeSkills();
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult SubmitSkillRequest(const FSkillLabSkillRequest& Request);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult SubmitSkillOperationRequest(const FSkillLabSkillRequest& Request, ESkillLabOperationCategory Category);
	void NotifyNonInterruptingOperation(ESkillLabOperationCategory Category);
	bool RequestNonSkillOperation(ESkillLabOperationCategory Category, FName& OutReason);
	bool IsRuleWindowOpen(FName WindowId) const;
	void NotifyDamageTaken(FGameplayTag AttackCategory, const FSkillLabDamageResult& DamageResult);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult CastSkill(FName SkillId, AActor* Target, FVector TargetLocation);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void CancelActiveCast(FName Reason);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void ResetSkills();
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") int32 GetUnitId() const { return UnitId; }
	uint64 GetRuntimeGeneration() const { return RuntimeGeneration; }
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") bool IsInitialized() const { return bInitialized; }
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") bool IsAlive() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") bool IsEnemy(const USkillLabSkillRuntimeComponent* Other) const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetCooldownRemaining(FName SkillId) const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetEnergyValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetReservedEnergyValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetAvailableEnergyValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetMaxEnergyValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetHealthValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetMaxHealthValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") ESkillLabCastStage GetCastStage() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetStageRemaining() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") FName GetActiveSkillId() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") int32 GetActiveCastId() const;
	void GetGrantedSkillIds(TArray<FName>& OutIds) const;

	void NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability);
	void NotifyAbilityEnded(USkillLabSkillGameplayAbility* Ability);
	bool ForwardValidatedNotify(int32 CastId, FGameplayTag EventTag);
	bool ForwardValidatedNotifyStateEnd(int32 CastId, FGameplayTag BeginEventTag);
	void HandleAbilityGameplayEvent(USkillLabSkillGameplayAbility* Ability, int32 CastId, FGameplayTag EventTag);
	virtual bool IsBehaviorCurrent(const USkillLabSkillBehavior* Behavior, int32 CastId) const override;
	virtual USkillLabAbilitySystemComponent* GetSourceAbilitySystemComponent() const override { return ASC.Get(); }
	virtual AActor* GetSourceActor() const override { return GetOwner(); }
	virtual int32 GetSourceUnitId() const override { return UnitId; }
	virtual bool MatchesTargetRelation(const USkillLabSkillRuntimeComponent* Target,
		ESkillLabTargetRelation Relation) const override;
	FSkillLabSkillResult ApplyBehaviorEffect(const USkillLabSkillBehavior* Behavior, int32 CastId,
		AActor* Target, const FSkillLabSkillDefinition& Definition, const FSkillLabProductionHandle& Production = {});
	FSkillLabProductionHandle GetProductionHandle(const USkillLabSkillBehavior* Behavior, int32 CastId) const;
	void ReportBehaviorMiss(const USkillLabSkillBehavior* Behavior, int32 CastId, AActor* Target, FName Reason);
	void CompleteBehavior(const USkillLabSkillBehavior* Behavior, int32 CastId, FName Reason = NAME_None);
	void CancelBehavior(const USkillLabSkillBehavior* Behavior, int32 CastId, FName Reason);
	bool BeginBehaviorSubmission(const USkillLabSkillBehavior* Behavior, int32 CastId, uint64 SubmissionPointId, FName& OutReason);
	bool ResolveBehaviorSubmission(const USkillLabSkillBehavior* Behavior, int32 CastId, uint64 SubmissionPointId, bool bSucceeded, bool* bOutIndeterminate = nullptr, FName* OutFailureReason = nullptr);
	void AbortBehaviorSubmission(const USkillLabSkillBehavior* Behavior, int32 CastId, uint64 SubmissionPointId);
	bool IsSubmissionIndeterminate(const USkillLabSkillBehavior* Behavior, int32 CastId) const;

	/** 无参兼容查询仅在恰有一项执行时返回该项；多执行不隐选最近/最严格。 */
	const USkillLabCastExecution* GetSingleExecution() const;
	const USkillLabCastExecution* FindExecutionByCastId(int32 CastId) const;
	const USkillLabCastExecution* FindExecution(const FSkillLabCastExecutionToken& Token) const;
	TArray<FSkillLabCastExecutionToken> GetExecutionTokens() const;
	void CancelCast(const FSkillLabCastExecutionToken& Token, FName Reason);
	bool ForwardValidatedNotify(const FSkillLabCastExecutionToken& Token, FGameplayTag EventTag);
	bool ForwardValidatedNotifyStateEnd(const FSkillLabCastExecutionToken& Token, FGameplayTag BeginEventTag);


private:
	friend class USkillLabUnconfirmedSubmissions;
	friend class ASkillLabFireballProjectile;
	friend class FSkillLabCastScenarioFixture;
	friend class FSkillLabUnconfirmedSubmissionTest;
	friend class FSkillLabUnconfirmedWaitCommand;
	friend class FSkillLabProductionFixture;
	UPROPERTY(VisibleAnywhere) int32 UnitId = 0;
	UPROPERTY(VisibleAnywhere) bool bInitialized = false;
	/** Borrowed from the owning Actor; Runtime never keeps the ASC alive. */
	UPROPERTY(Transient) TWeakObjectPtr<USkillLabAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillEffectEventAdapter> EffectEventAdapter;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillEffectExecutor> EffectExecutor;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillLoadout> Loadout;
	UPROPERTY(Transient) TWeakObjectPtr<USkillLabUnconfirmedSubmissions> UnconfirmedSubmissions;
	UPROPERTY(Transient) TMap<int32, TObjectPtr<USkillLabCastExecution>> Executions;
	TMap<FGameplayTag, FDelegateHandle> InterruptHandles;
	FDelegateHandle DamageTakenHandle;
	FTimerHandle PendingIntentTimer;
	int32 NextRequestId = 1;
	int32 NextCastId = 1;
	int32 CancellationBarrierDepth = 0;
	bool bShuttingDown = false;
	bool bDeferredEndPlay = false;
	bool bResetting = false;
	bool bResetRequested = false;
	uint64 RuntimeGeneration = 1;
	enum class EResetPhase : uint8 { Cancelling, Cleaning, Waiting };
	struct FResetAttempt
	{
		uint64 Id = 0;
		uint64 Generation = 0;
		TWeakObjectPtr<USkillLabAbilitySystemComponent> SourceASC;
		FObjectKey SourceASCKey;
		EResetPhase Phase = EResetPhase::Cancelling;
	};
	uint64 NextResetAttemptId = 1;
	TOptional<FResetAttempt> ResetAttempt;
	struct FActivationBinding
	{
		FGameplayAbilitySpecHandle Spec;
		int32 CastId = 0;
		uint64 Generation = 0;
	};
	TArray<FActivationBinding> ActivationBindings;
	TOptional<FSkillLabSkillInputIntent> PendingSkillIntent;
	TSet<int32> AcceptedRequestIds;
	TArray<int32> AcceptedRequestOrder;
	static constexpr int32 MaxAcceptedRequestHistory = 128;

	using ESubmissionState = USkillLabCastExecution::ESubmissionState;
	using FSubmissionContext = USkillLabCastExecution::FSubmissionContext;
	USkillLabCastExecution* FindMutableExecution(int32 CastId) const;
	USkillLabCastExecution* FindExecution(const USkillLabSkillBehavior* Behavior, int32 CastId) const;
	bool IsExecutionCurrent(const USkillLabCastExecution& Execution) const;
	bool AnySubmitting() const;
	ESkillLabRuleDecision BuildActivationPlan(FGameplayTag AbilityCategory, TArray<FSkillLabCastExecutionToken>& OutCancel, FName& OutReason) const;
	void CancelExecution(USkillLabCastExecution& Execution, FName Reason);
	void CompleteReset();
	bool IsResetAttemptCurrent(uint64 AttemptId, uint64 Generation) const;
	void FinishResetAttempt(uint64 AttemptId, uint64 Generation, FName Reason);
	void OnResetResourceFinished(uint64 Generation, const FSkillLabResourceReceipt& Receipt);
	FSkillLabSkillResult TryActivateRequest(FSkillLabSkillRequest Request, const FSkillLabSkillDefinition& Definition);
	FSkillLabSkillResult Reject(const FSkillLabSkillRequest& Request, FName Reason);
	FSkillLabSkillResult MakeResult(const USkillLabCastExecution* Execution = nullptr) const;
	void Emit(ESkillLabSkillEvent Event, const FSkillLabSkillResult& Result, FName Reason = NAME_None);
	void ReleaseCast(USkillLabSkillGameplayAbility* Ability, int32 CastId);
	void FinishWindup(USkillLabCastExecution& Execution, FSkillLabCastPhaseToken Token);
	void BeginRecovery(USkillLabCastExecution& Execution);
	void FinishRecovery(USkillLabCastExecution& Execution, FSkillLabCastPhaseToken Token);
	void FinishCast(USkillLabCastExecution& Execution, bool bCancelled, FName Reason);
	void FinishEndPlayCleanup();
	void FinishSubmissionTermination(USkillLabCastExecution& Execution);
	void StartIndeterminateWait(USkillLabCastExecution& Execution);
	bool CanReceiveSubmissionConfirmation(const USkillLabUnconfirmedSubmissions* Session) const;
	bool OnSubmissionConfirmed(const struct FSkillLabSubmissionConfirmation& Confirmation);
	void FinishUnavailableSubmission(USkillLabCastExecution& Execution, const FSkillLabSkillResult& Identity, TWeakObjectPtr<UWorld> WorldBefore,
		bool bActionSucceeded, bool bResourceResolved, bool bCooldownRecorded);
	void ListenForInterrupts();
	void ClearInterruptListeners();
	void OnBlockingTagChanged(FGameplayTag Tag, int32 Count);
	void ReevaluateInterruptStates();
	const FSkillLabOperationRules* ResolveOperationRules(const USkillLabCastExecution& Execution, FName& OutReason) const;
	bool CheckGates(const FSkillLabSkillDefinition& Definition, FName& OutReason) const;
	void RememberRequestId(int32 RequestId);
	void ForgetRequestId(int32 RequestId);
	void ClearPendingCast(USkillLabCastExecution& Execution);
	void ForwardEffectEvent(const FSkillLabSkillResult& Result);
	void ClearPendingSkillIntent(FName Reason);
	void StorePendingSkillIntent(const FSkillLabSkillRequest& Request, ESkillLabOperationCategory Category, FGameplayTag AbilityCategory);
	FSkillLabSkillResult SubmitSkillOperation(const FSkillLabSkillRequest& Request, ESkillLabOperationCategory Category, FGameplayTag AbilityCategory);
	ESkillLabRuleDecision EvaluateAbilityCategory(const USkillLabCastExecution& Execution, FGameplayTag Category, FName& OutReason) const;
	void SchedulePendingSkillIntentConsumption();
	void ConsumePendingSkillIntent();
	ESkillLabRuleDecision EvaluateOperation(const USkillLabCastExecution& Execution, ESkillLabOperationCategory Category, FName& OutReason) const;
};
