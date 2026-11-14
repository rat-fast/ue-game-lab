#pragma once
#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabCastLifecycle.h"
#include "SkillLabSubmissionResults.h"
#include "SkillLabCastExecution.generated.h"

class USkillLabSkillGameplayAbility;
class USkillLabSkillBehavior;

/** 一次 Cast 的唯一可变执行状态；Runtime 集合负责 GC，异步回调使用完整身份重新查找。 */
UCLASS()
class SKILLLABSKILLS_API USkillLabCastExecution : public UObject
{
 GENERATED_BODY()
public:
	/** GAS owns the ability instance; Runtime only observes the active instance. */
	UPROPERTY(Transient) TWeakObjectPtr<USkillLabSkillGameplayAbility> ActiveAbility;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillBehavior> ActiveBehavior;
	UPROPERTY(Transient) FSkillLabSkillRequest ActiveRequest;
	UPROPERTY(Transient) FSkillLabSkillDefinition ActiveDefinition;
	FTimerHandle PhaseTimer;
	FTimerHandle ActiveTimeoutTimer;
	FTimerHandle IndeterminateTimer;
	FSkillLabCastLifecycle Lifecycle;
	FSkillLabCastExecutionToken Token;
	double StageEndTime = 0.0;
	bool bCommitted = false;
	bool bSubmitting = false;
	enum class ESubmissionState : uint8 { None, Escrowed, Executing, Resolving, Committed, Failed, Indeterminate, DeliveryUnavailable, Closed };
	struct FSubmissionContext
	{
		ESubmissionState State = ESubmissionState::None;
		ESubmissionState Outcome = ESubmissionState::None;
		FSkillLabCastToken CastToken;
		uint64 RuntimeGeneration = 0;
		uint64 TransactionId = 0;
		uint64 PointId = 0;
		TWeakObjectPtr<const USkillLabSkillBehavior> Behavior;
		bool bHasEscrow = false;
		FSkillLabProductionHandle Production;
	};
	FSubmissionContext Submission;
	bool bApplyingEffect = false;
	FName DeferredSubmissionCancel;
	bool bDeferredSubmissionReset = false;
	bool bDeferredSubmissionFinish = false;
	FName DeferredSubmissionFinishReason;
	bool bFinishing = false;
	FName ActiveEffectOperationId = NAME_None;
	// 操作编号仅在效果实际生效后递增；失败和重复目标不占用编号。
	int32 SuccessfulEffectCount = 0;
	TSet<FGameplayTag> ConsumedWindowEvents;
	FName OutcomeReason;
	TMap<uint64, TArray<FSkillLabSkillResult>> SuccessfulPointEffects;
};
