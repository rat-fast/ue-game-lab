#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabSkillBehavior.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

#include "UObject/StrongObjectPtr.h"
bool USkillLabSkillRuntimeComponent::ResolveBehaviorSubmission(const USkillLabSkillBehavior* Behavior, const int32 CastId,
	const uint64 SubmissionPointId, const bool bSucceeded, bool* bOutIndeterminate, FName* OutFailureReason)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return false; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (bOutIndeterminate) *bOutIndeterminate = false;
	if (OutFailureReason) *OutFailureReason = NAME_None;
	if (!Execution.bSubmitting || Execution.Submission.PointId != SubmissionPointId || Execution.Submission.CastToken.CastId != static_cast<uint64>(CastId)
		|| Execution.Submission.Behavior.Get() != Behavior || Execution.Submission.RuntimeGeneration != Execution.Token.RuntimeGeneration
		|| (Execution.Submission.State != ESubmissionState::Executing && Execution.Submission.State != ESubmissionState::Escrowed)) return false;
	if (Execution.Submission.State == ESubmissionState::Executing && Execution.Submission.bHasEscrow && ASC.IsValid() && ASC->HasUnsupportedResourceWrite())
	{
		if (bOutIndeterminate) *bOutIndeterminate = true;
		AbortBehaviorSubmission(Behavior, CastId, SubmissionPointId);
		return false;
	}
	// 动作的 false 可能只是范围执行被中断；缺失子结果不能授权释放预留。
	if (Execution.Submission.State == ESubmissionState::Executing && !bSucceeded)
	{
		const ESkillLabProductionState State = UnconfirmedSubmissions.IsValid()
			? UnconfirmedSubmissions->GetProductionResults().Query(Execution.Submission.Production.Key).State
			: ESkillLabProductionState::Unavailable;
		if (State != ESkillLabProductionState::ConfirmedRejected)
		{
			if (bOutIndeterminate) *bOutIndeterminate = true;
			AbortBehaviorSubmission(Behavior, CastId, SubmissionPointId);
			return false;
		}
	}
	// 保存调用前身份；属性委托可能销毁 Actor 或结束会话所有者。
	const FSkillLabSkillResult Identity = MakeResult(&Execution);
	const TWeakObjectPtr<AActor> ActorBefore = GetOwner();
	const TWeakObjectPtr<UWorld> WorldBefore = GetWorld();
	const float Cooldown = Execution.ActiveDefinition.Cooldown;
	const bool bWasCommitted = Execution.bCommitted;
	Execution.Submission.State = ESubmissionState::Resolving;
	const bool bResourceResolved = !Execution.Submission.bHasEscrow || (ASC.IsValid() && ASC->ResolveEnergyEscrow(Execution.Submission.TransactionId, bSucceeded));
	const bool bReceiversAlive = ActorBefore.IsValid() && !ActorBefore->IsActorBeingDestroyed()
		&& ASC.IsValid() && !ASC->IsBeingDestroyed() && UnconfirmedSubmissions.IsValid()
		&& UnconfirmedSubmissions->CanAcceptConfirmedCooldown(ASC.Get()) && WorldBefore.IsValid();
	bool bCooldownRecorded = bWasCommitted;
	if (bSucceeded && bResourceResolved && !bWasCommitted)
	{
		// 已确认扣费不因交付对象消失而回滚或再扣一次。
		Execution.bCommitted = true;
		bCooldownRecorded = bReceiversAlive && UnconfirmedSubmissions->StartConfirmedCooldown(
			Identity.SkillId, WorldBefore->GetTimeSeconds() + Cooldown);
		if (bCooldownRecorded)
		{
			if (auto* Production = UnconfirmedSubmissions->ProductionResults.Results.Find(Execution.Submission.Production.Key.Operation))
				Production->bCooldownRecorded = true;
		}
	}
	if (!IsValid(this) || bShuttingDown || !bReceiversAlive || (bSucceeded && bResourceResolved && !bCooldownRecorded))
	{
		if (OutFailureReason) *OutFailureReason = TEXT("SubmissionDeliveryUnavailable");
		FinishUnavailableSubmission(Execution, Identity, WorldBefore, bSucceeded, bResourceResolved, bCooldownRecorded);
		return false;
	}
	if (!bResourceResolved)
	{
		if (Execution.Submission.bHasEscrow && ASC.IsValid() && !ASC->HasIndeterminateEnergyEscrow(Execution.Submission.TransactionId)
			&& !ASC->MarkEnergyEscrowIndeterminate(Execution.Submission.TransactionId))
			UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Could not retain unresolved escrow: Transaction=%llu"), Execution.Submission.TransactionId);
		UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Execution.Submission escrow did not close: Cast=%d Transaction=%llu"), CastId, Execution.Submission.TransactionId);
	}
	const bool bPointResolved = Execution.Lifecycle.ResolveSubmission(Execution.Submission.CastToken, SubmissionPointId, bSucceeded && bResourceResolved);
	Execution.Submission.State = !bResourceResolved || !bPointResolved ? ESubmissionState::Indeterminate
		: (bSucceeded ? ESubmissionState::Committed : ESubmissionState::Failed);
	Execution.Submission.Outcome = Execution.Submission.State;
	if (bOutIndeterminate) *bOutIndeterminate = Execution.Submission.Outcome == ESubmissionState::Indeterminate;
	if (bSucceeded && bResourceResolved && bPointResolved)
	{
		FSkillLabSkillResult SubmissionResult = MakeResult(&Execution);
		SubmissionResult.bSuccess = true;
		SubmissionResult.SubmissionPointId = static_cast<int64>(SubmissionPointId);
		Emit(ESkillLabSkillEvent::SubmissionCommitted, SubmissionResult, TEXT("SubmissionCommitted"));
	}
	// 结果事件也允许同步终止；不得继续向已失效的对象交接或排计时器。
	if (!IsValid(this) || bShuttingDown || !ActorBefore.IsValid() || ActorBefore->IsActorBeingDestroyed()
		|| !ASC.IsValid() || ASC->IsBeingDestroyed() || !UnconfirmedSubmissions.IsValid()
		|| !UnconfirmedSubmissions->CanAcceptConfirmedCooldown(ASC.Get()))
	{
		if (OutFailureReason) *OutFailureReason = TEXT("SubmissionDeliveryUnavailable");
		FinishUnavailableSubmission(Execution, Identity, WorldBefore, bSucceeded, bResourceResolved, bCooldownRecorded);
		return false;
	}
	Execution.Submission.State = ESubmissionState::Closed;
	Execution.bSubmitting = false;
	Execution.PendingSubmissionId = 0;
	if (Execution.Submission.Outcome == ESubmissionState::Indeterminate) StartIndeterminateWait(Execution);
	FinishSubmissionTermination(Execution);
	return bResourceResolved && bPointResolved;
}

bool USkillLabSkillRuntimeComponent::BeginBehaviorSubmission(const USkillLabSkillBehavior* Behavior, const int32 CastId,
	const uint64 SubmissionPointId, FName& OutReason)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return false; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (IsSubmissionIndeterminate(Behavior, CastId)) { OutReason = TEXT("SubmissionUnconfirmed"); return false; }
	if (Execution.bSubmitting || !IsBehaviorCurrent(Behavior, CastId)) { OutReason = TEXT("SubmissionNotAvailable"); return false; }
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	if (!Attributes || (!Execution.bCommitted && ASC->GetAvailableEnergy() < Execution.ActiveDefinition.Cost))
	{
		OutReason = TEXT("InsufficientEnergy"); return false;
	}
	if (!Execution.Lifecycle.BeginSubmission(Execution.Lifecycle.GetCastToken(), SubmissionPointId))
	{
		OutReason = TEXT("SubmissionPointUnavailable"); return false;
	}
	Execution.bSubmitting = true;
	Execution.PendingSubmissionId = SubmissionPointId;
	Execution.Submission = FSubmissionContext();
	Execution.Submission.CastToken = Execution.Lifecycle.GetCastToken();
	Execution.Submission.RuntimeGeneration = Execution.Token.RuntimeGeneration;
	Execution.Submission.PointId = SubmissionPointId;
	Execution.Submission.Behavior = Behavior;
	Execution.Submission.TransactionId = Execution.bCommitted ? 0 : ASC->AllocateEnergyTransactionId();
	if (!Execution.bCommitted && (!Execution.Submission.TransactionId || !ASC->BeginEnergyEscrow(Execution.Submission.TransactionId, Execution.ActiveDefinition.Cost, Execution.Submission.RuntimeGeneration, static_cast<uint64>(CastId))))
	{
		Execution.Lifecycle.ResolveSubmission(Execution.Submission.CastToken, SubmissionPointId, false);
		Execution.Submission.Outcome = ASC.IsValid() && ASC->HasIndeterminateEnergyEscrow(Execution.Submission.TransactionId)
			? ESubmissionState::Indeterminate : ESubmissionState::Failed;
		Execution.Submission.State = ESubmissionState::Closed;
		Execution.bSubmitting = false;
		Execution.PendingSubmissionId = 0;
		OutReason = Execution.Submission.Outcome == ESubmissionState::Indeterminate ? TEXT("SubmissionUnconfirmed") : TEXT("ResourceEscrowFailed");
		if (Execution.Submission.Outcome == ESubmissionState::Indeterminate) StartIndeterminateWait(Execution);
		FinishSubmissionTermination(Execution);
		return false;
	}
	Execution.Submission.bHasEscrow = !Execution.bCommitted;
	Execution.Submission.State = ESubmissionState::Escrowed;
	Execution.Submission.Production = UnconfirmedSubmissions.IsValid() ? UnconfirmedSubmissions->BeginProduction(this,
		Execution.Token.RuntimeGeneration, static_cast<uint64>(CastId), SubmissionPointId, Execution.Submission.TransactionId,
		Execution.ActiveRequest.RequestId, Execution.ActiveDefinition, Execution.bCommitted) : FSkillLabProductionHandle();
	if (!Execution.Submission.Production.Key.IsSet() || (Execution.Submission.bHasEscrow && !ASC->BindEnergyOperation(Execution.Submission.TransactionId,
		UnconfirmedSubmissions.Get(), this, Execution.Submission.Production.Key.Operation)))
	{
		ResolveBehaviorSubmission(Behavior, CastId, SubmissionPointId, false);
		OutReason = TEXT("ProductionIdentityUnavailable");
		return false;
	}
	if (bShuttingDown || Execution.bDeferredSubmissionReset || !Execution.DeferredSubmissionCancel.IsNone())
	{
		bool bIndeterminate = false;
		ResolveBehaviorSubmission(Behavior, CastId, SubmissionPointId, false, &bIndeterminate);
		OutReason = bIndeterminate ? TEXT("SubmissionUnconfirmed") : TEXT("SubmissionInterruptedBeforeExecution");
		return false;
	}
	Execution.Submission.State = ESubmissionState::Executing;
	OutReason = NAME_None;
	return true;
}

void USkillLabSkillRuntimeComponent::AbortBehaviorSubmission(const USkillLabSkillBehavior* Behavior, const int32 CastId, const uint64 SubmissionPointId)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (!Execution.bSubmitting || Execution.Submission.PointId != SubmissionPointId || Execution.Submission.CastToken.CastId != static_cast<uint64>(CastId)
		|| Execution.Submission.Behavior.Get() != Behavior || Execution.Submission.State != ESubmissionState::Executing) return;
	Execution.Lifecycle.ResolveSubmission(Execution.Submission.CastToken, SubmissionPointId, false);
	Execution.Submission.State = ESubmissionState::Indeterminate;
	Execution.Submission.Outcome = Execution.Submission.State;
	UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Execution.Submission result indeterminate: Cast=%d Point=%llu"), CastId, SubmissionPointId);
	if (Execution.Submission.bHasEscrow && (!ASC.IsValid() || !ASC->MarkEnergyEscrowIndeterminate(Execution.Submission.TransactionId)))
		UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Indeterminate submission escrow did not close: Cast=%d"), CastId);
	Execution.Submission.State = ESubmissionState::Closed;
	Execution.bSubmitting = false;
	Execution.PendingSubmissionId = 0;
	StartIndeterminateWait(Execution);
	FinishSubmissionTermination(Execution);
}
