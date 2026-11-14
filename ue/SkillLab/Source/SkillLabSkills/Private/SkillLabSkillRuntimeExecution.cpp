#include "SkillLabSkillRuntimeComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillEffectEventAdapter.h"
#include "SkillLabSkillEffectExecutor.h"
#include "SkillLabSkillLoadout.h"
#include "SkillLabTargetingService.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "TimerManager.h"

namespace
{
	ESkillLabCastStage ToPublicStage(const FSkillLabCastLifecycle::EStage Stage)
	{
		switch (Stage)
		{
		case FSkillLabCastLifecycle::EStage::Windup: return ESkillLabCastStage::Windup;
		case FSkillLabCastLifecycle::EStage::Active: return ESkillLabCastStage::Active;
		case FSkillLabCastLifecycle::EStage::Recovery: return ESkillLabCastStage::Recovery;
		case FSkillLabCastLifecycle::EStage::Completed: return ESkillLabCastStage::Completed;
		case FSkillLabCastLifecycle::EStage::Cancelled: return ESkillLabCastStage::Cancelled;
		default: return ESkillLabCastStage::Idle;
		}
	}

	FName EffectReason(const ESkillLabEffectKind Kind)
	{
		switch (Kind)
		{
		case ESkillLabEffectKind::Healing: return TEXT("HealingApplied");
		case ESkillLabEffectKind::DamageOverTime: return TEXT("DamageOverTimeApplied");
		default: return TEXT("DamageApplied");
		}
	}
}

#include "UObject/StrongObjectPtr.h"
FSkillLabSkillResult USkillLabSkillRuntimeComponent::MakeResult(const USkillLabCastExecution* Execution) const
{
 FSkillLabSkillResult Result;
 Result.SourceUnitId = UnitId; Result.SourceActor = GetOwner();
 if (!Execution) return Result;
 Result.CastToken = Execution->Token;
 Result.SkillId = Execution->ActiveDefinition.SkillId;
 Result.CastId = Execution->Token.CastId;
 Result.Stage = ToPublicStage(Execution->Lifecycle.GetStage());
 Result.Effect = Execution->ActiveDefinition.Effect;
 Result.RequestId = Execution->ActiveRequest.RequestId;
 const auto& Submission = Execution->Submission;
 if (Submission.Production.Key.IsSet() && Submission.Production.Key.Runtime == FObjectKey(this)
  && Submission.RuntimeGeneration == Execution->Token.RuntimeGeneration && Submission.CastToken.CastId == static_cast<uint64>(Result.CastId))
 {
  Result.SubmissionKey = Submission.Production.Key;
  Result.SubmissionPointId = static_cast<int64>(Submission.PointId);
 }
 Result.bHasReleased = Execution->Lifecycle.GetSnapshot().bHasReleased;
 Result.bCostCommitted = Execution->bCommitted;
 return Result;
}

bool USkillLabSkillRuntimeComponent::ForwardValidatedNotifyStateEnd(const int32 CastId, const FGameplayTag BeginEventTag)
{
	USkillLabCastExecution* Found = FindMutableExecution(CastId);
	if (!Found) { return false; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (CastId != Execution.Token.CastId || CastId <= 0) return false;
	for (const FSkillLabRuleWindow& Window : Execution.ActiveDefinition.RuleWindows)
		if (Window.BeginEvent == BeginEventTag) return ForwardValidatedNotify(CastId, Window.EndEvent);
	return false;
}

bool USkillLabSkillRuntimeComponent::ForwardValidatedNotify(const int32 CastId, const FGameplayTag EventTag)
{
	USkillLabCastExecution* Found = FindMutableExecution(CastId);
	if (!Found) { return false; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (bShuttingDown || bResetting || !Execution.DeferredSubmissionCancel.IsNone() || Execution.bFinishing || CastId <= 0 || Execution.Token.CastId != CastId || !EventTag.IsValid()
		|| !Execution.ActiveAbility.IsValid() || !ASC.IsValid())
	{
		return false;
	}
	const bool bIsReleaseEvent = Execution.ActiveDefinition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
		&& EventTag == Execution.ActiveDefinition.ReleaseEventTag;
	const bool bIsBehaviorEvent = Execution.ActiveDefinition.BehaviorEventTags.HasTagExact(EventTag) || EventTag == Execution.ActiveDefinition.CompletionEventTag;
	bool bIsPointEvent = false;
	for (const auto& Point : Execution.ActiveDefinition.SubmissionPoints)
		bIsPointEvent |= Point.Trigger == ESkillLabSubmissionTrigger::GameplayEvent && Point.EventTag == EventTag;
	bool bIsWindowEvent = false;
	for (const FSkillLabRuleWindow& Window : Execution.ActiveDefinition.RuleWindows)
		bIsWindowEvent |= EventTag == Window.BeginEvent || EventTag == Window.EndEvent;
	if (!bIsReleaseEvent && !bIsBehaviorEvent && !bIsWindowEvent && !bIsPointEvent)
	{
		return false;
	}
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	Payload.Target = Execution.ActiveRequest.Target;
	USkillLabCastGameplayEventData* Identity = NewObject<USkillLabCastGameplayEventData>(this);
	Identity->Runtime = this;
	Identity->Ability = Execution.ActiveAbility;
	Identity->CastId = CastId;
	Identity->CastToken = Execution.Token;
	Payload.OptionalObject = Identity;
	ASC->HandleGameplayEvent(EventTag, &Payload);
	return true;
}

void USkillLabSkillRuntimeComponent::HandleAbilityGameplayEvent(USkillLabSkillGameplayAbility* Ability,
	const int32 CastId, const FGameplayTag EventTag)
{
	USkillLabCastExecution* Found = FindMutableExecution(CastId);
	if (!Found) { return; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (bShuttingDown || bResetting || !Execution.DeferredSubmissionCancel.IsNone() || Execution.bFinishing || !Ability || Execution.ActiveAbility.Get() != Ability || CastId <= 0
		|| Execution.Token.CastId != CastId || !EventTag.IsValid())
	{
		return;
	}
	for (int32 Index = 0; Index < Execution.ActiveDefinition.RuleWindows.Num(); ++Index)
	{
		const FSkillLabRuleWindow& Window = Execution.ActiveDefinition.RuleWindows[Index];
		if (EventTag != Window.BeginEvent && EventTag != Window.EndEvent) continue;
		if (Execution.ConsumedWindowEvents.Contains(EventTag)) return;
		const FSkillLabCastToken CastToken = Execution.Lifecycle.GetCastToken();
		const uint64 WindowId = static_cast<uint64>(Index) + 1;
		const bool bChanged = EventTag == Window.BeginEvent ? Execution.Lifecycle.OpenWindow(CastToken, WindowId)
			: Execution.Lifecycle.CloseWindow(CastToken, WindowId);
		// A premature end must not consume the eventual matching end.
		if (bChanged) Execution.ConsumedWindowEvents.Add(EventTag);
		UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] Cast=%d Window=%s Event=%s Changed=%d"), CastId,
			*Window.Id.ToString(), *EventTag.ToString(), bChanged);
		if (bChanged) { ReevaluateInterruptStates(); SchedulePendingSkillIntentConsumption(); }
		return;
	}
	if (!Execution.Lifecycle.GetSnapshot().bHasReleased
		&& Execution.ActiveDefinition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
		&& EventTag == Execution.ActiveDefinition.ReleaseEventTag)
	{
		ReleaseCast(Ability, CastId);
	}
	else if (Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active && Execution.ActiveBehavior)
	{
		Execution.ActiveBehavior->ReceiveEvent(EventTag);
	}
}

void USkillLabSkillRuntimeComponent::FinishWindup(USkillLabCastExecution& Execution, const FSkillLabCastPhaseToken Token)
{
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(&Execution);
	if (!Execution.ActiveDefinition.bIndependentWindup || Execution.bFinishing || Execution.Token.CastId <= 0
		|| Execution.Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Windup || !Execution.Lifecycle.IsTokenCurrent(Token)) return;
	if (Execution.Lifecycle.AdvancePhase(Token, GetWorld()->GetTimeSeconds()) != FSkillLabCastLifecycle::EAdvanceResult::EnteredActive) return;
	const int32 PreparedCastId = Execution.Token.CastId;
	const FSkillLabCastPhaseToken ActiveToken = Execution.Lifecycle.GetPhaseToken();
	FSkillLabSkillResult Phase = MakeResult(&Execution);
	Phase.bSuccess = true;
	Execution.StageEndTime = 0.0;
	Emit(ESkillLabSkillEvent::PhaseChanged, Phase, TEXT("WindupCompleted"));
	SchedulePendingSkillIntentConsumption();
	if (IsExecutionCurrent(Execution) && Execution.Token.CastId == PreparedCastId && Execution.Lifecycle.IsTokenCurrent(ActiveToken)) ReevaluateInterruptStates();
}

void USkillLabSkillRuntimeComponent::ReleaseCast(USkillLabSkillGameplayAbility* Ability, const int32 CastId)
{
	USkillLabCastExecution* Found = FindMutableExecution(CastId);
	if (!Found) { return; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (Execution.ActiveDefinition.bIndependentWindup && Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Windup) return;
	const FSkillLabCastPhaseToken Token = Execution.Lifecycle.GetPhaseToken();
	if (bShuttingDown || bResetting || !Execution.DeferredSubmissionCancel.IsNone() || Execution.bFinishing || Execution.Token.CastId == 0 || !Execution.Lifecycle.IsTokenCurrent(Token)
		|| !Ability || Execution.ActiveAbility.Get() != Ability || Execution.Token.CastId != CastId
		|| Execution.Lifecycle.GetSnapshot().bHasReleased
		|| (Execution.Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Windup && Execution.Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Active))
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	FName GateReason;
	AActor* ValidatedTarget = nullptr;
	if (!CheckGates(Execution.ActiveDefinition, GateReason)
		|| !FSkillLabTargetingService::ValidateRequest(*this, Execution.ActiveRequest, Execution.ActiveDefinition, ValidatedTarget, GateReason))
	{
		CancelExecution(Execution, GateReason);
		return;
	}
	Execution.ActiveRequest.Target = ValidatedTarget;
	if (Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Windup
		&& Execution.Lifecycle.AdvancePhase(Token, Now) != FSkillLabCastLifecycle::EAdvanceResult::EnteredActive)
	{
		CancelExecution(Execution, TEXT("ReleasePhaseFailed"));
		return;
	}
	const FSkillLabCastPhaseToken ActiveToken = Execution.Lifecycle.GetPhaseToken();
	const int32 ReleasedCastId = Execution.Token.CastId;
	ReevaluateInterruptStates();
	if (!IsExecutionCurrent(Execution) || Execution.Token.CastId != ReleasedCastId || !Execution.Lifecycle.IsTokenCurrent(ActiveToken)) return;
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	if (!Attributes || (!Execution.bCommitted && ASC->GetAvailableEnergy() < Execution.ActiveDefinition.Cost) || !Execution.Lifecycle.TryMarkRelease(ActiveToken))
	{
		CancelExecution(Execution, TEXT("ReleaseGateFailed"));
		return;
	}
	if (!IsExecutionCurrent(Execution) || Execution.Token.CastId != ReleasedCastId || !Execution.Lifecycle.IsTokenCurrent(ActiveToken)) return;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(Execution.PhaseTimer);
		GetWorld()->GetTimerManager().ClearTimer(Execution.ActiveTimeoutTimer);
		GetWorld()->GetTimerManager().ClearTimer(Execution.IndeterminateTimer);
	}
	FSkillLabSkillResult Phase = MakeResult(&Execution);
	Phase.bSuccess = true;
	Phase.Stage = ESkillLabCastStage::Active;
	Phase.TargetActor = Execution.ActiveRequest.Target;
	Phase.TargetLocation = Execution.ActiveDefinition.TargetShape == ESkillLabTargetShape::Ground
		? Execution.ActiveRequest.TargetLocation
		: (Execution.ActiveRequest.Target ? Execution.ActiveRequest.Target->GetActorLocation() : FVector::ZeroVector);
	Phase.VisualRadius = Execution.ActiveDefinition.Radius;
	Phase.StageDuration = Execution.ActiveDefinition.Phases.ActiveSeconds;
	Emit(ESkillLabSkillEvent::PhaseChanged, Phase);
	if (!IsExecutionCurrent(Execution) || Execution.Token.CastId != ReleasedCastId || !Execution.Lifecycle.IsTokenCurrent(ActiveToken)) return;
	FSkillLabSkillResult TargetSelected = Phase;
	TargetSelected.Event = ESkillLabSkillEvent::TargetSelected;
	Emit(ESkillLabSkillEvent::TargetSelected, TargetSelected);
	if (!IsExecutionCurrent(Execution) || Execution.Token.CastId != ReleasedCastId || !Execution.Lifecycle.IsTokenCurrent(ActiveToken)) return;

	Execution.StageEndTime = Execution.ActiveDefinition.Phases.ActiveSeconds > 0.0f ? Now + Execution.ActiveDefinition.Phases.ActiveSeconds : 0.0;
	if (Execution.ActiveDefinition.Phases.ActiveSeconds > 0.0f)
	{
		const auto CastToken = Execution.Token;
		GetWorld()->GetTimerManager().SetTimer(Execution.ActiveTimeoutTimer, FTimerDelegate::CreateWeakLambda(this, [this, CastToken, ActiveToken]()
		{
			if (const auto* E = FindExecution(CastToken); E && E->Lifecycle.IsTokenCurrent(ActiveToken)) CancelCast(CastToken, TEXT("ActiveTimeout"));
		}), Execution.ActiveDefinition.Phases.ActiveSeconds, false);
	}
	if (Execution.ActiveDefinition.BehaviorClass)
	{
		Execution.ActiveBehavior = NewObject<USkillLabSkillBehavior>(this, Execution.ActiveDefinition.BehaviorClass);
		if (Execution.ActiveBehavior)
		{
			Execution.ActiveBehavior->Initialize(this, Execution.ActiveDefinition, Execution.ActiveRequest, Execution.Token.CastId, Execution.Token.RuntimeGeneration);
			Execution.ActiveBehavior->Execute();
			return;
		}
	}
	CancelExecution(Execution, TEXT("BehaviorCreateFailed"));
}

void USkillLabSkillRuntimeComponent::CancelActiveCast(const FName Reason)
{
 // 兼容角色级明确取消入口：取消调用时的全部施法；业务内部使用 token。
 TGuardValue<int32> Barrier(CancellationBarrierDepth, CancellationBarrierDepth + 1);
 for (const auto& Token : GetExecutionTokens()) CancelCast(Token, Reason);
}

void USkillLabSkillRuntimeComponent::CancelExecution(USkillLabCastExecution& Execution, const FName Reason)
{
 TStrongObjectPtr<USkillLabCastExecution> Guard(&Execution);
 if (!IsExecutionCurrent(Execution) || Execution.bFinishing) return;
 if (Execution.bSubmitting)
 {
  if (Execution.DeferredSubmissionCancel.IsNone()) Execution.DeferredSubmissionCancel = Reason;
  return;
 }
 if (Execution.Lifecycle.Cancel(Execution.Lifecycle.GetPhaseToken(), GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0))
  FinishCast(Execution, true, Reason);
}

void USkillLabSkillRuntimeComponent::BeginRecovery(USkillLabCastExecution& Execution)
{
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(&Execution);
	if (Execution.Token.CastId == 0 || Execution.Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Active)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FSkillLabCastPhaseToken Token = Execution.Lifecycle.GetPhaseToken();
	if (Execution.Lifecycle.AdvancePhase(Token, Now) != FSkillLabCastLifecycle::EAdvanceResult::EnteredRecovery)
	{
		return;
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(Execution.ActiveTimeoutTimer);
	}
	Execution.StageEndTime = Execution.ActiveDefinition.Phases.RecoverySeconds > 0.0f ? Now + Execution.ActiveDefinition.Phases.RecoverySeconds : Now;
	const int32 RecoveringCastId = Execution.Token.CastId;
	const FSkillLabCastPhaseToken RecoveryToken = Execution.Lifecycle.GetPhaseToken();
	FSkillLabSkillResult Phase = MakeResult(&Execution);
	Phase.bSuccess = true;
	Phase.Stage = ESkillLabCastStage::Recovery;
	Phase.StageDuration = Execution.ActiveDefinition.Phases.RecoverySeconds;
	Emit(ESkillLabSkillEvent::PhaseChanged, Phase);
	SchedulePendingSkillIntentConsumption();
	// A listener may cancel this cast and start another one during the broadcast.
	// Never schedule the previous cast's recovery against the new active definition.
	if (!IsExecutionCurrent(Execution) || Execution.Token.CastId != RecoveringCastId || !Execution.Lifecycle.IsTokenCurrent(RecoveryToken)) return;
	ReevaluateInterruptStates();
	if (!IsExecutionCurrent(Execution) || Execution.Token.CastId != RecoveringCastId || !Execution.Lifecycle.IsTokenCurrent(RecoveryToken)) return;
	if (Execution.ActiveDefinition.Phases.RecoverySeconds > 0.0f)
	{
		const auto CastToken = Execution.Token;
		GetWorld()->GetTimerManager().SetTimer(Execution.PhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, CastToken, RecoveryToken]()
		{
			if (FindExecution(CastToken)) FinishRecovery(*FindMutableExecution(CastToken.CastId), RecoveryToken);
		}), Execution.ActiveDefinition.Phases.RecoverySeconds, false);
	}
	else
	{
		FinishRecovery(Execution, RecoveryToken);
	}
}

void USkillLabSkillRuntimeComponent::FinishRecovery(USkillLabCastExecution& Execution, const FSkillLabCastPhaseToken Token)
{
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(&Execution);
	if (Execution.Token.CastId == 0 || !Execution.Lifecycle.IsTokenCurrent(Token) || Execution.Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Recovery)
	{
		return;
	}
	if (Execution.Lifecycle.Finish(Token, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0))
	{
		FinishCast(Execution, false, Execution.OutcomeReason.IsNone() ? TEXT("Completed") : Execution.OutcomeReason);
	}
}

void USkillLabSkillRuntimeComponent::FinishCast(USkillLabCastExecution& Execution, const bool bCancelled, const FName Reason)
{
 TStrongObjectPtr<USkillLabCastExecution> Guard(&Execution);
 if (!IsExecutionCurrent(Execution) || Execution.bFinishing) return;
 Execution.bFinishing = true;
 Execution.OutcomeReason = Reason;
 if (auto* World = GetWorld())
 {
  World->GetTimerManager().ClearTimer(Execution.PhaseTimer);
  World->GetTimerManager().ClearTimer(Execution.ActiveTimeoutTimer);
  World->GetTimerManager().ClearTimer(Execution.IndeterminateTimer);
 }
 if (Execution.ActiveBehavior) Execution.ActiveBehavior->Cleanup();
 const auto Ability = Execution.ActiveAbility;
 Execution.ActiveAbility.Reset();
 if (Ability.IsValid() && ASC.IsValid() && !ASC->IsBeingDestroyed()) Ability->FinishSkillAbility(bCancelled);
 auto Result = MakeResult(&Execution);
 Result.bSuccess = !bCancelled;
 Result.Stage = bCancelled ? ESkillLabCastStage::Cancelled : ESkillLabCastStage::Completed;
 // 先退出执行集合，终态监听者再请求时不会继承旧限制/Ability。
 Executions.Remove(Execution.Token.CastId);
 Execution.ActiveBehavior = nullptr;
 if (bCancelled) ClearPendingSkillIntent(TEXT("CastCancelled"));
 Emit(bCancelled ? ESkillLabSkillEvent::Cancelled : ESkillLabSkillEvent::Completed, Result, Reason);
 if (!bCancelled) SchedulePendingSkillIntentConsumption();
}

void USkillLabSkillRuntimeComponent::ClearPendingCast(USkillLabCastExecution& Execution)
{
 TStrongObjectPtr<USkillLabCastExecution> Guard(&Execution);
 Execution.bFinishing = true;
 if (auto* World = GetWorld())
 {
  World->GetTimerManager().ClearTimer(Execution.PhaseTimer);
  World->GetTimerManager().ClearTimer(Execution.ActiveTimeoutTimer);
  World->GetTimerManager().ClearTimer(Execution.IndeterminateTimer);
 }
 const auto Ability = Execution.ActiveAbility;
 Execution.ActiveAbility.Reset();
 if (Ability.IsValid()) Ability->FinishSkillAbility(true);
 if (Execution.ActiveBehavior) Execution.ActiveBehavior->Cleanup();
 if (FindMutableExecution(Execution.Token.CastId) == &Execution) Executions.Remove(Execution.Token.CastId);
}

void USkillLabSkillRuntimeComponent::NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability)
{
 if (!Ability || ActivationBindings.IsEmpty() || bShuttingDown || bResetting) return;
 const auto Binding = ActivationBindings.Last();
 auto* Execution = FindMutableExecution(Binding.CastId);
 if (!Execution || !IsExecutionCurrent(*Execution) || Binding.Generation != RuntimeGeneration
  || Binding.Spec != Ability->ActiveHandle || Execution->ActiveAbility.IsValid()) return;
 Execution->ActiveAbility = Ability;
 FGameplayTagContainer Events = Execution->ActiveDefinition.BehaviorEventTags;
 const auto& Definition = Execution->ActiveDefinition;
 Events.AddTag(Definition.CompletionEventTag);
 for (const auto& Point : Definition.SubmissionPoints) if (Point.Trigger == ESkillLabSubmissionTrigger::GameplayEvent) Events.AddTag(Point.EventTag);
 for (const auto& Window : Definition.RuleWindows) { Events.AddTag(Window.BeginEvent); Events.AddTag(Window.EndEvent); }
 if (Definition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent) Events.AddTag(Definition.ReleaseEventTag);
 Ability->BindSkillCast(this, Execution->Token, Events);
}

void USkillLabSkillRuntimeComponent::NotifyAbilityEnded(USkillLabSkillGameplayAbility* Ability)
{
 if (!Ability || !FindExecution(Ability->BoundCastToken)) return;
 auto* Execution = FindMutableExecution(Ability->BoundCastToken.CastId);
 if (Execution->ActiveAbility.Get() != Ability) return;
 Execution->ActiveAbility.Reset();
 if (!Execution->bFinishing) CancelExecution(*Execution, TEXT("AbilityEnded"));
}

bool USkillLabSkillRuntimeComponent::IsBehaviorCurrent(const USkillLabSkillBehavior* Behavior, const int32 CastId) const
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return false; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	return !bShuttingDown && Behavior && Execution.ActiveBehavior == Behavior && Execution.Token.CastId == CastId
		&& Behavior->RuntimeGeneration == RuntimeGeneration
		&& !bResetting && !Execution.bFinishing
		&& Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active;
}

bool USkillLabSkillRuntimeComponent::MatchesTargetRelation(const USkillLabSkillRuntimeComponent* Target,
	const ESkillLabTargetRelation Relation) const
{
	return Target && FSkillLabTargetingService::MatchesRelation(*this, *Target, Relation);
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::ApplyBehaviorEffect(const USkillLabSkillBehavior* Behavior,
	const int32 CastId, AActor* Target, const FSkillLabSkillDefinition& Definition, const FSkillLabProductionHandle& Production)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { FSkillLabSkillResult Missing; Missing.Reason = TEXT("CastNotActive"); return Missing; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	FSkillLabSkillResult Result = MakeResult(&Execution);
	if (Execution.bApplyingEffect || !IsBehaviorCurrent(Behavior, CastId) || IsSubmissionIndeterminate(Behavior, CastId))
	{
		Result.Reason = TEXT("CastNotActive");
		return Result;
	}
	const auto Handle = Production.Key.IsSet() ? Production : GetProductionHandle(Behavior, CastId);
	if (!Handle.Key.IsSet() || Handle.Key.Runtime != FObjectKey(this) || Handle.Key.Generation != Execution.Token.RuntimeGeneration
		|| Handle.Key.Cast != static_cast<uint64>(CastId)) { Result.Reason = TEXT("SubmissionRequired"); return Result; }
	const uint64 PointId = Handle.Key.Point;
	for (const FSkillLabSkillResult& Existing : Execution.SuccessfulPointEffects.FindOrAdd(PointId))
	{
		if (Existing.TargetActor == Target)
		{
			Result = Existing;
			Result.Reason = TEXT("EffectAlreadySubmitted");
			return Result;
		}
	}
	Execution.ActiveEffectOperationId = FName(*FString::Printf(TEXT("SkillEffect_%d_%d"), CastId, Execution.SuccessfulEffectCount + 1));
	if (!Execution.bSubmitting && !Execution.bCommitted) { Result.Reason = TEXT("SubmissionRequired"); return Result; }
	TGuardValue<bool> ApplyingEffect(Execution.bApplyingEffect, true);
	Result.EffectOperationId = Execution.ActiveEffectOperationId;
	if (EffectExecutor)
	{
		Result = EffectExecutor->Apply(Behavior, CastId, Target, Definition, Result,
			Handle);
	}
	else
	{
		Result.Reason = TEXT("EffectExecutorUnavailable");
		// 未进入执行器，明确没有副作用；正式失败仍需要对应生产记录。
		if (Handle.Owner.IsValid() && Handle.Owner->CanAcceptConfirmedCooldown(ASC.Get())
			&& Handle.Owner->GetProductionResults().Query(Handle.Key).Kind == ESkillLabProductionKind::Effect)
			Handle.Owner->ProductionResults.Confirm(Handle.Key, false, Result.Reason);
	}
	if (!IsBehaviorCurrent(Behavior, CastId)) return Result;
	Result.EffectOperationId = Execution.ActiveEffectOperationId;
	if (Result.bEffectApplied)
	{
		++Execution.SuccessfulEffectCount;
		Execution.SuccessfulPointEffects.FindOrAdd(PointId).Add(Result);
	}
	if (!Result.bSuccess)
	{
		Emit(ESkillLabSkillEvent::Hit, Result, Result.Reason);
		return Result;
	}
	Emit(ESkillLabSkillEvent::Hit, Result);
	Emit(ESkillLabSkillEvent::EffectApplied, Result, EffectReason(Definition.Effect));
	if (Definition.Effect == ESkillLabEffectKind::DamageOverTime)
	{
		Emit(ESkillLabSkillEvent::BuffApplied, Result, TEXT("BurningStarted"));
	}
	return Result;
}

void USkillLabSkillRuntimeComponent::ReportBehaviorMiss(const USkillLabSkillBehavior* Behavior, const int32 CastId,
	AActor* Target, const FName Reason)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (!Behavior || !IsBehaviorCurrent(Behavior, CastId))
	{
		return;
	}
	FSkillLabSkillResult Result = MakeResult(&Execution);
	Result.bSuccess = false;
	Result.TargetActor = Target;
	Emit(ESkillLabSkillEvent::Hit, Result, Reason);
}

void USkillLabSkillRuntimeComponent::CompleteBehavior(const USkillLabSkillBehavior* Behavior, const int32 CastId, const FName Reason)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (!Behavior || !IsBehaviorCurrent(Behavior, CastId) || IsSubmissionIndeterminate(Behavior, CastId))
	{
		return;
	}
	if (Execution.bSubmitting) { Execution.bDeferredSubmissionFinish = true; Execution.DeferredSubmissionFinishReason = Reason; return; }
	Execution.OutcomeReason = Reason;
	BeginRecovery(Execution);
}

void USkillLabSkillRuntimeComponent::CancelBehavior(const USkillLabSkillBehavior* Behavior, const int32 CastId, const FName Reason)
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	if (IsBehaviorCurrent(Behavior, CastId)) CancelExecution(Execution, Reason);
}

void USkillLabSkillRuntimeComponent::FinishSubmissionTermination(USkillLabCastExecution& Execution)
{
 TStrongObjectPtr<USkillLabCastExecution> Guard(&Execution);
 if (Execution.bSubmitting) return;
 if (bDeferredEndPlay)
 {
  CancelExecution(Execution, TEXT("EndPlay"));
  if (!AnySubmitting()) FinishEndPlayCleanup();
  return;
 }
 if (bResetRequested)
 {
  CancelExecution(Execution, TEXT("Reset"));
  if (!AnySubmitting()) CompleteReset();
  return;
 }
 const auto CancelReason = Execution.DeferredSubmissionCancel;
 Execution.DeferredSubmissionCancel = NAME_None;
 if (!CancelReason.IsNone()) CancelExecution(Execution, CancelReason);
 else if (Execution.bDeferredSubmissionFinish && Execution.Submission.Outcome != ESubmissionState::Indeterminate)
 {
  Execution.bDeferredSubmissionFinish = false;
  CompleteBehavior(Execution.Submission.Behavior.Get(), Execution.Token.CastId, Execution.DeferredSubmissionFinishReason);
 }
 else if (Execution.Submission.Outcome == ESubmissionState::Indeterminate) Execution.bDeferredSubmissionFinish = false;
}

