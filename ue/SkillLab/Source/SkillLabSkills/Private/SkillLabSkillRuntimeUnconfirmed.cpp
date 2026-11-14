#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillBehavior.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillEffectExecutor.h"
#include "SkillLabSkillEffectEventAdapter.h"

#include "UObject/StrongObjectPtr.h"
void USkillLabSkillRuntimeComponent::StartIndeterminateWait(USkillLabCastExecution& Execution)
{
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(&Execution);
	// Handoff precedes every deferred Reset/EndPlay or observable termination.
	if (UnconfirmedSubmissions.IsValid())
	{
		FSkillLabUnconfirmedSubmission Record;
		Record.SkillId = Execution.ActiveDefinition.SkillId;
		Record.RuntimeIdentity = FObjectKey(this);
		Record.RuntimeGeneration = Execution.Submission.RuntimeGeneration;
		Record.CastId = Execution.Submission.CastToken.CastId;
		Record.PointId = Execution.Submission.PointId;
		Record.TransactionId = Execution.Submission.TransactionId;
		Record.bAlreadyCommitted = Execution.bCommitted;
		Record.ProductionKey = Execution.Submission.Production.Key;
		if (Record.ProductionKey.IsSet())
		{
			Record.ResultSourceId = TEXT("SkillLabProduction");
			Record.bAlreadyCommitted = UnconfirmedSubmissions->GetProductionResults().Query(Record.ProductionKey).bAlreadyCommitted;
		}
		UnconfirmedSubmissions->Record(Record);
	}
	else UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] Session cannot retain unconfirmed submission: no valid owner; Transaction=%llu"), Execution.Submission.TransactionId);

	if (IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
		&& !bShuttingDown && !Execution.bDeferredSubmissionReset && Execution.DeferredSubmissionCancel.IsNone() && GetWorld())
	{
		const auto CastToken = Execution.Token;
		GetWorld()->GetTimerManager().ClearTimer(Execution.ActiveTimeoutTimer);
		GetWorld()->GetTimerManager().ClearTimer(Execution.PhaseTimer);
		GetWorld()->GetTimerManager().SetTimer(Execution.IndeterminateTimer, FTimerDelegate::CreateWeakLambda(this, [this, CastToken]()
		{
			if (FindExecution(CastToken)) CancelCast(CastToken, TEXT("SubmissionUnconfirmed"));
		}), 1.0f, false);
	}
}

void USkillLabSkillRuntimeComponent::FinishUnavailableSubmission(USkillLabCastExecution& Execution, const FSkillLabSkillResult& Identity,
	const TWeakObjectPtr<UWorld> WorldBefore, const bool bActionSucceeded, const bool bResourceResolved, const bool bCooldownRecorded)
{
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(&Execution);
	UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] Execution.Submission delivery unavailable: Skill=%s Cast=%d Transaction=%llu ActionSucceeded=%d ResourceResolved=%d CooldownRecorded=%d; no retry or compensation"),
		*Identity.SkillId.ToString(), Identity.CastId, Execution.Submission.TransactionId, bActionSucceeded, bResourceResolved, bCooldownRecorded);
	Execution.Submission.Outcome = ESubmissionState::DeliveryUnavailable;
	Execution.Submission.State = ESubmissionState::Closed;
	Execution.bSubmitting = false;
	Execution.bDeferredSubmissionFinish = false;
	Execution.DeferredSubmissionCancel = NAME_None;
	if (WorldBefore.IsValid())
	{
		WorldBefore->GetTimerManager().ClearTimer(Execution.PhaseTimer);
		WorldBefore->GetTimerManager().ClearTimer(Execution.ActiveTimeoutTimer);
		WorldBefore->GetTimerManager().ClearTimer(Execution.IndeterminateTimer);
		WorldBefore->GetTimerManager().ClearTimer(PendingIntentTimer);
	}
	if (IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed() && !bShuttingDown)
	{
		CancelExecution(Execution, TEXT("SubmissionDeliveryUnavailable"));
		FinishSubmissionTermination(Execution);
		return;
	}
	// 销毁链已开始：只清理本地状态，不调用失效 Actor、ASC 或组件。
	const TWeakObjectPtr<USkillLabSkillGameplayAbility> Ability = Execution.ActiveAbility;
	Execution.Lifecycle.Cancel(Execution.Lifecycle.GetPhaseToken(), WorldBefore.IsValid() ? WorldBefore->GetTimeSeconds() : 0.0);
	if (IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed())
	{
		ClearInterruptListeners();
		if (ASC.IsValid() && !ASC->IsBeingDestroyed() && DamageTakenHandle.IsValid()) ASC->OnSkillLabDamageTaken.Remove(DamageTakenHandle);
		if (IsValid(EffectEventAdapter)) EffectEventAdapter->Clear(false);
		if (IsValid(EffectExecutor)) EffectExecutor->Shutdown();
	}
	Execution.ActiveAbility.Reset();
	Execution.ActiveBehavior = nullptr;
	Executions.Remove(Execution.Token.CastId);
	Execution.bFinishing = false;
	Execution.StageEndTime = 0.0;
	PendingSkillIntent.Reset();
	InterruptHandles.Reset();
	DamageTakenHandle.Reset();
	OnNativeSkillEvent.Clear();
	OnSkillEvent.Clear();
	FinishSubmissionTermination(Execution);
	if (Ability.IsValid() && ASC.IsValid() && !ASC->IsBeingDestroyed()
		&& IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()) Ability->FinishSkillAbility(true);
}

bool USkillLabSkillRuntimeComponent::IsSubmissionIndeterminate(const USkillLabSkillBehavior* Behavior, const int32 CastId) const
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return false; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	return Execution.Submission.Outcome == ESubmissionState::Indeterminate && Execution.Submission.Behavior.Get() == Behavior
		&& Execution.Submission.CastToken.CastId == static_cast<uint64>(CastId) && Execution.Submission.RuntimeGeneration == Execution.Token.RuntimeGeneration;
}
