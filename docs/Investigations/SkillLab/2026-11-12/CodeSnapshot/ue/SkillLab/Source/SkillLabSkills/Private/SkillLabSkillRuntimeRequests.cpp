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
#include "UObject/StrongObjectPtr.h"

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

FSkillLabSkillResult USkillLabSkillRuntimeComponent::CastSkill(const FName SkillId, AActor* Target, const FVector TargetLocation)
{
	FSkillLabSkillRequest Request;
	Request.SkillId = SkillId;
	Request.Caster = GetOwner();
	Request.Target = Target;
	Request.TargetLocation = TargetLocation;
	Request.bHasLocation = true;
	return SubmitSkillRequest(Request);
}


FSkillLabSkillResult USkillLabSkillRuntimeComponent::Reject(const FSkillLabSkillRequest& Request, const FName Reason)
{
	FSkillLabSkillResult Result;
	Result.SkillId = Request.SkillId;
	Result.RequestId = Request.RequestId;
	Result.SourceUnitId = UnitId;
	Result.SourceActor = GetOwner();
	Result.Reason = Reason;
	Emit(ESkillLabSkillEvent::Rejected, Result, Reason);
	return Result;
}


bool USkillLabSkillRuntimeComponent::CheckGates(const FSkillLabSkillDefinition& Definition, FName& OutReason) const
{
 OutReason = NAME_None;
 if (!bInitialized || !ASC.IsValid()) OutReason = TEXT("NotInitialized");
 else if (!IsAlive() || ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead)) OutReason = TEXT("CasterDead");
 else if (ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Stunned)
  || ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Silenced)) OutReason = TEXT("BlockedByControlState");
 else if (ASC->HasAnySkillLabBlockTag(Definition.BlockedByTags)) OutReason = TEXT("BlockedByState");
 return OutReason.IsNone();
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillRequest(const FSkillLabSkillRequest& InRequest)
{
	FSkillLabSkillDefinition Definition;
	const ESkillLabOperationCategory Category = FSkillLabSkillDefinitionProvider::Resolve(InRequest.SkillId, Definition)
		? Definition.Category : ESkillLabOperationCategory::Skill;
	const FGameplayTag AbilityCategory = Definition.AbilityCategory.IsValid() ? Definition.AbilityCategory : SkillLabGASTags::Ability_Skill;
	return SubmitSkillOperation(InRequest, Category, AbilityCategory);
}


FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillOperationRequest(const FSkillLabSkillRequest& InRequest,
	const ESkillLabOperationCategory Category)
{
	return SubmitSkillOperation(InRequest, Category, Category == ESkillLabOperationCategory::Dodge
		? SkillLabGASTags::Ability_Dodge : SkillLabGASTags::Ability_Skill);
}


FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillOperation(const FSkillLabSkillRequest& InRequest,
 const ESkillLabOperationCategory Category, const FGameplayTag AbilityCategory)
{
 FSkillLabSkillRequest Request = InRequest;
 const uint64 RequestGeneration = RuntimeGeneration;
 const auto IsRequestCurrent = [this, RequestGeneration]()
 {
  return IsValid(this) && !IsBeingDestroyed() && !bShuttingDown && !bResetting && !bResetRequested
   && RuntimeGeneration == RequestGeneration && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
   && GetWorld() && bInitialized && ASC.IsValid() && !ASC->IsBeingDestroyed();
 };
 if (Request.RequestId > 0)
 {
  if (AcceptedRequestIds.Contains(Request.RequestId)) return Reject(Request, TEXT("RequestDuplicate"));
 }
 else { do { Request.RequestId = NextRequestId++; } while (AcceptedRequestIds.Contains(Request.RequestId)); }
 if (CancellationBarrierDepth > 0) return Reject(Request, TEXT("OperationReentered"));
 if (bShuttingDown || bResetting || !GetWorld() || !bInitialized || !ASC.IsValid())
  return Reject(Request, bResetting ? TEXT("Resetting") : TEXT("NotInitialized"));
 if (Category != ESkillLabOperationCategory::Skill && Category != ESkillLabOperationCategory::Dodge)
  return Reject(Request, TEXT("OperationHasNoSkillRequest"));
 if (Category == ESkillLabOperationCategory::Dodge)
 {
  // 8.2：闪避先取消全体，再尝试门控；提交中的 Cast 封闭并延迟收束，不能覆盖它。
  ClearPendingSkillIntent(TEXT("DodgeReplacedInput"));
  if (!IsRequestCurrent()) return Reject(Request, TEXT("OperationReentered"));
  CancelActiveCast(TEXT("DodgeRequest"));
  if (!Executions.IsEmpty()) return Reject(Request, TEXT("OperationReentered"));
 }
 if (!IsRequestCurrent()) return Reject(Request, TEXT("OperationReentered"));
 if (!UnconfirmedSubmissions.IsValid() || !UnconfirmedSubmissions->CanAcceptConfirmedCooldown(ASC.Get()))
  return Reject(Request, TEXT("SessionOwnerUnavailable"));
 if (UnconfirmedSubmissions->BlocksSkill(Request.SkillId)) return Reject(Request, TEXT("SubmissionUnconfirmed"));
 FSkillLabSkillDefinition Definition;
 if (!FSkillLabSkillDefinitionProvider::Resolve(Request.SkillId, Definition)) return Reject(Request, TEXT("SkillUnknown"));
 FName Reason;
 if (!CheckGates(Definition, Reason)) return Reject(Request, Reason);
 TArray<FSkillLabCastExecutionToken> Cancel;
 auto Decision = BuildActivationPlan(AbilityCategory, Cancel, Reason);
 if (Decision == ESkillLabRuleDecision::Reject) return Reject(Request, Reason);
 if (Decision == ESkillLabRuleDecision::Buffer)
 {
  StorePendingSkillIntent(Request, Category, AbilityCategory);
  return Reject(Request, TEXT("InputBufferedLatestWins"));
 }
 if (PendingSkillIntent.IsSet())
 {
  ClearPendingSkillIntent(Cancel.IsEmpty() ? FName(TEXT("BufferedIntentReplaced")) : FName(TEXT("OperationReplacedInput")));
  if (!IsRequestCurrent()) return Reject(Request, TEXT("OperationReentered"));
  // 回调写入的新意图比本请求更新；保留它，不重复清槽或递归重试。
  if (PendingSkillIntent.IsSet()) return Reject(Request, TEXT("OperationReentered"));
  Decision = BuildActivationPlan(AbilityCategory, Cancel, Reason);
  if (Decision == ESkillLabRuleDecision::Reject) return Reject(Request, Reason);
  if (Decision == ESkillLabRuleDecision::Buffer)
  {
   StorePendingSkillIntent(Request, Category, AbilityCategory);
   return Reject(Request, TEXT("InputBufferedLatestWins"));
  }
 }
 // 使用通知后的新计划，仅取消仍明确授权 CancelAndActivate 的来源。
 if (!Cancel.IsEmpty())
 {
  for (const auto& Token : Cancel) CancelCast(Token, TEXT("OperationRequest"));
  if (!IsRequestCurrent() || PendingSkillIntent.IsSet()) return Reject(Request, TEXT("OperationReentered"));
  Cancel.Reset();
  Decision = BuildActivationPlan(AbilityCategory, Cancel, Reason);
  if (Decision == ESkillLabRuleDecision::Reject) return Reject(Request, Reason);
  if (Decision == ESkillLabRuleDecision::Buffer)
  {
   StorePendingSkillIntent(Request, Category, AbilityCategory);
   return Reject(Request, TEXT("InputBufferedLatestWins"));
  }
  // 取消通知再产生新的取消来源时，不无限追逐并取消回调创建的执行。
  if (!Cancel.IsEmpty()) return Reject(Request, TEXT("OperationReentered"));
 }
 if (!UnconfirmedSubmissions.IsValid() || !UnconfirmedSubmissions->CanAcceptConfirmedCooldown(ASC.Get()))
  return Reject(Request, TEXT("SessionOwnerUnavailable"));
 if (UnconfirmedSubmissions->BlocksSkill(Request.SkillId)) return Reject(Request, TEXT("SubmissionUnconfirmed"));
 if (AcceptedRequestIds.Contains(Request.RequestId)) return Reject(Request, TEXT("RequestDuplicate"));
 return TryActivateRequest(Request, Definition);
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::TryActivateRequest(FSkillLabSkillRequest Request, const FSkillLabSkillDefinition& Definition)
{
 FName Reason;
 if (!CheckGates(Definition, Reason)) return Reject(Request, Reason);
 if (!IsValid(Request.Caster) || Request.Caster != GetOwner())
  return Reject(Request, Request.Caster ? TEXT("CasterMismatch") : TEXT("CasterRequired"));
 if (!Loadout || !Loadout->IsGranted(Request.SkillId)) return Reject(Request, TEXT("SkillNotGranted"));
 if (GetCooldownRemaining(Request.SkillId) > 0.0f) return Reject(Request, TEXT("Cooldown"));
 const auto* Attributes = ASC->GetCombatAttributeSet();
 if (!Attributes || ASC->GetAvailableEnergy() < Definition.Cost) return Reject(Request, TEXT("InsufficientEnergy"));
 AActor* ResolvedTarget = nullptr;
 if (!FSkillLabTargetingService::ValidateRequest(*this, Request, Definition, ResolvedTarget, Reason)) return Reject(Request, Reason);
 Request.Target = ResolvedTarget;
 TStrongObjectPtr<USkillLabCastExecution> Guard(NewObject<USkillLabCastExecution>(this));
 auto& Execution = *Guard;
 Execution.Token.Runtime = this;
 Execution.Token.RuntimeGeneration = RuntimeGeneration;
 Execution.Token.CastId = NextCastId++;
 Execution.ActiveDefinition = Definition;
 Execution.ActiveRequest = Request;
 const double Now = GetWorld()->GetTimeSeconds();
 FSkillLabCastLifecycle::FConfig Config;
 Config.WindupSeconds = Definition.Phases.WindupSeconds;
 Config.ActiveSeconds = Definition.Phases.ActiveSeconds;
 Config.RecoverySeconds = Definition.Phases.RecoverySeconds;
 Config.bUseActiveTimeout = Definition.Phases.ActiveSeconds > 0.0f;
 if (!Execution.Lifecycle.Begin(static_cast<uint64>(Execution.Token.CastId), Config, Now)) return Reject(Request, TEXT("LifecycleStartFailed"));
 Executions.Add(Execution.Token.CastId, &Execution);
 RememberRequestId(Request.RequestId);
 const auto Spec = Loadout->FindGrantedHandle(Request.SkillId);
 FActivationBinding Binding;
 Binding.Spec = Spec; Binding.CastId = Execution.Token.CastId; Binding.Generation = RuntimeGeneration;
 ActivationBindings.Add(Binding);
 const bool bActivated = ASC->TryActivateAbility(Spec);
 ActivationBindings.Pop(EAllowShrinking::No);
 if (!bActivated || !IsExecutionCurrent(Execution) || !Execution.ActiveAbility.IsValid())
 {
  ClearPendingCast(Execution);
  ForgetRequestId(Request.RequestId);
  return Reject(Request, TEXT("ActivationFailed"));
 }
 ListenForInterrupts();
 const auto CastToken = Execution.Token;
 const auto WindupToken = Execution.Lifecycle.GetPhaseToken();
 Execution.StageEndTime = !Definition.bIndependentWindup && Definition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
  ? Now + Definition.ReleaseEventTimeout : Now + Definition.Phases.WindupSeconds;
 FSkillLabSkillResult Started = MakeResult(&Execution);
 Started.bSuccess = true; Started.TargetActor = Request.Target;
 Started.TargetLocation = Definition.TargetShape == ESkillLabTargetShape::Ground ? Request.TargetLocation
  : (Request.Target ? Request.Target->GetActorLocation() : FVector::ZeroVector);
 Started.VisualRadius = Definition.Radius; Started.StageDuration = Definition.Phases.WindupSeconds;
 Emit(ESkillLabSkillEvent::Started, Started);
 if (!IsExecutionCurrent(Execution) || !Execution.Lifecycle.IsTokenCurrent(WindupToken)) return Started;
 ReevaluateInterruptStates();
 if (!IsExecutionCurrent(Execution) || !Execution.Lifecycle.IsTokenCurrent(WindupToken)) return Started;
 if (Definition.ReleaseTrigger == ESkillLabReleaseTrigger::Timer)
 {
  if (Definition.Phases.WindupSeconds <= 0.0f) ReleaseCast(Execution.ActiveAbility.Get(), CastToken.CastId);
  else GetWorld()->GetTimerManager().SetTimer(Execution.PhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, CastToken]()
  {
   if (const auto* E = FindExecution(CastToken)) ReleaseCast(E->ActiveAbility.Get(), CastToken.CastId);
  }), Definition.Phases.WindupSeconds, false);
 }
 else
 {
  GetWorld()->GetTimerManager().SetTimer(Execution.ActiveTimeoutTimer, FTimerDelegate::CreateWeakLambda(this, [this, CastToken]()
  {
   if (const auto* E = FindExecution(CastToken); E && !E->Lifecycle.GetSnapshot().bHasReleased)
    CancelCast(CastToken, TEXT("ReleaseEventTimeout"));
  }), Definition.ReleaseEventTimeout, false);
  if (Definition.bIndependentWindup)
  {
   if (Definition.Phases.WindupSeconds <= 0.0f) FinishWindup(Execution, WindupToken);
   else GetWorld()->GetTimerManager().SetTimer(Execution.PhaseTimer, FTimerDelegate::CreateWeakLambda(this, [this, CastToken, WindupToken]()
   {
    if (FindExecution(CastToken)) FinishWindup(*FindMutableExecution(CastToken.CastId), WindupToken);
   }), Definition.Phases.WindupSeconds, false);
  }
 }
 return Started;
}

void USkillLabSkillRuntimeComponent::RememberRequestId(const int32 RequestId)
{
	if (RequestId <= 0 || AcceptedRequestIds.Contains(RequestId)) return;
	AcceptedRequestIds.Add(RequestId);
	AcceptedRequestOrder.Add(RequestId);
	while (AcceptedRequestOrder.Num() > MaxAcceptedRequestHistory)
	{
		AcceptedRequestIds.Remove(AcceptedRequestOrder[0]);
		AcceptedRequestOrder.RemoveAt(0);
	}
}


void USkillLabSkillRuntimeComponent::ForgetRequestId(const int32 RequestId)
{
	AcceptedRequestIds.Remove(RequestId);
	AcceptedRequestOrder.Remove(RequestId);
}


void USkillLabSkillRuntimeComponent::ClearPendingSkillIntent(const FName Reason)
{
	if (!PendingSkillIntent.IsSet()) return;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PendingIntentTimer);
	}
	const FSkillLabSkillRequest Request = PendingSkillIntent->Request;
	PendingSkillIntent.Reset();
	Reject(Request, Reason);
}


void USkillLabSkillRuntimeComponent::StorePendingSkillIntent(const FSkillLabSkillRequest& Request,
 const ESkillLabOperationCategory Category, const FGameplayTag AbilityCategory)
{
 const auto Previous = PendingSkillIntent;
 FSkillLabSkillInputIntent Intent;
 Intent.Request = Request; Intent.Category = Category; Intent.AbilityCategory = AbilityCategory;
 PendingSkillIntent = MoveTemp(Intent);
 if (Previous.IsSet()) Reject(Previous->Request, TEXT("BufferedIntentReplaced"));
}

void USkillLabSkillRuntimeComponent::SchedulePendingSkillIntentConsumption()
{
 if (!PendingSkillIntent.IsSet() || bResetting || bShuttingDown || !GetWorld()) return;
 if (GetWorld()->GetTimerManager().IsTimerActive(PendingIntentTimer)) return;
 PendingIntentTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
 {
  PendingIntentTimer.Invalidate();
  ConsumePendingSkillIntent();
 }));
}

void USkillLabSkillRuntimeComponent::ConsumePendingSkillIntent()
{
 if (!PendingSkillIntent.IsSet() || bResetting || bShuttingDown || !bInitialized) return;
 if (!IsAlive() || (ASC.IsValid() && ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead)))
 { ClearPendingSkillIntent(TEXT("CasterDead")); return; }
 TArray<FSkillLabCastExecutionToken> Cancel;
 FName Reason;
 const auto Category = PendingSkillIntent->AbilityCategory.IsValid() ? PendingSkillIntent->AbilityCategory : SkillLabGASTags::Ability_Skill;
 const auto Decision = BuildActivationPlan(Category, Cancel, Reason);
 if (Decision == ESkillLabRuleDecision::Buffer) return;
 if (Decision == ESkillLabRuleDecision::Reject) { ClearPendingSkillIntent(Reason); return; }
 auto Intent = MoveTemp(PendingSkillIntent.GetValue());
 PendingSkillIntent.Reset();
 Intent.Request.RequestId = 0;
 SubmitSkillOperation(Intent.Request, Intent.Category, Category);
}

