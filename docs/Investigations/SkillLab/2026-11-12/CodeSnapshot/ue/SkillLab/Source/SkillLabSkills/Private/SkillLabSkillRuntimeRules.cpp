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
void USkillLabSkillRuntimeComponent::ListenForInterrupts()
{
 if (!ASC.IsValid()) return;
 const auto Register = [this](const FGameplayTag Tag)
 {
  if (!Tag.IsValid() || InterruptHandles.Contains(Tag)) return;
  InterruptHandles.Add(Tag, ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
   .AddUObject(this, &USkillLabSkillRuntimeComponent::OnBlockingTagChanged));
 };
 Register(SkillLabGASTags::State_Dead); Register(SkillLabGASTags::State_Stunned); Register(SkillLabGASTags::State_Silenced);
 for (const auto& Pair : Executions)
 {
  if (!Pair.Value) continue;
  const auto& Definition = Pair.Value->ActiveDefinition;
  for (auto Tag : Definition.InterruptedByTags) Register(Tag);
  const auto RegisterRules = [&Register](const FSkillLabOperationRules& Rules) { for (auto Tag : Rules.InterruptBy) Register(Tag); };
  RegisterRules(Definition.OperationRules); RegisterRules(Definition.RecoveryRules);
  for (const auto& Window : Definition.RuleWindows) RegisterRules(Window.Rules);
 }
}

void USkillLabSkillRuntimeComponent::ClearInterruptListeners()
{
	if (ASC.IsValid())
	{
		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : InterruptHandles)
		{
			ASC->RegisterGameplayTagEvent(Pair.Key, EGameplayTagEventType::AnyCountChange).Remove(Pair.Value);
		}
	}
	InterruptHandles.Reset();
}

void USkillLabSkillRuntimeComponent::OnBlockingTagChanged(const FGameplayTag Tag, const int32 Count)
{
 if (Count > 0) ReevaluateInterruptStates();
 SchedulePendingSkillIntentConsumption();
}

void USkillLabSkillRuntimeComponent::ReevaluateInterruptStates()
{
 if (!ASC.IsValid()) return;
 const bool bDead = !IsAlive() || ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead);
 const bool bControl = ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Stunned) || ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Silenced);
 if (bDead || bControl)
 {
  if (bDead) ClearPendingSkillIntent(TEXT("CasterDead"));
  CancelActiveCast(bDead ? TEXT("CasterDead") : TEXT("BlockedByControlState"));
  return;
 }
 for (const auto& Token : GetExecutionTokens())
 {
  const auto* Execution = FindExecution(Token);
  if (!Execution || Execution->bFinishing) continue;
  FName Reason;
  const auto* Rules = ResolveOperationRules(*Execution, Reason);
  if (!Rules)
  {
   UE_LOG(LogTemp, Warning, TEXT("[SkillLabCombatRules] Cast=%d StateEvaluation=%s"), Token.CastId, *Reason.ToString());
   continue;
  }
  const auto& Tags = Rules->bOverrideInterruptBy ? Rules->InterruptBy : Execution->ActiveDefinition.InterruptedByTags;
  if (ASC.IsValid() && ASC->HasAnyMatchingGameplayTags(Tags)) CancelCast(Token, TEXT("BlockedByState"));
  if (!ASC.IsValid() || bShuttingDown) return;
 }
}

bool USkillLabSkillRuntimeComponent::IsRuleWindowOpen(const FName WindowId) const
{
 const auto* Execution = GetSingleExecution();
 if (!Execution) return false;
 for (int32 Index = 0; Index < Execution->ActiveDefinition.RuleWindows.Num(); ++Index)
  if (Execution->ActiveDefinition.RuleWindows[Index].Id == WindowId)
   return Execution->Lifecycle.IsWindowOpen(Execution->Lifecycle.GetCastToken(), static_cast<uint64>(Index) + 1);
 return false;
}

const FSkillLabOperationRules* USkillLabSkillRuntimeComponent::ResolveOperationRules(const USkillLabCastExecution& Execution, FName& OutReason) const
{
	const FSkillLabOperationRules* Rules = Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Recovery
		? &Execution.ActiveDefinition.RecoveryRules : &Execution.ActiveDefinition.OperationRules;
	int32 OpenCount = 0;
	for (int32 Index = 0; Index < Execution.ActiveDefinition.RuleWindows.Num(); ++Index)
	{
		if (Execution.Lifecycle.IsWindowOpen(Execution.Lifecycle.GetCastToken(), static_cast<uint64>(Index) + 1))
		{
			Rules = &Execution.ActiveDefinition.RuleWindows[Index].Rules;
			++OpenCount;
		}
	}
	if (OpenCount > 1)
	{
		OutReason = TEXT("RuleWindowConflict");
		return nullptr;
	}
	OutReason = Rules->Id.IsNone() ? FName(TEXT("LegacyCombatRules")) : Rules->Id;
	return Rules;
}

ESkillLabRuleDecision USkillLabSkillRuntimeComponent::EvaluateOperation(const USkillLabCastExecution& Execution, const ESkillLabOperationCategory Category, FName& OutReason) const
{
	const FSkillLabOperationRules* Rules = ResolveOperationRules(Execution, OutReason);
	if (!Rules)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabCombatRules] Cast=%d Operation=%d Reason=%s Decision=Reject"), Execution.Token.CastId, static_cast<int32>(Category), *OutReason.ToString());
		return ESkillLabRuleDecision::Reject;
	}
	ESkillLabRuleDecision Decision = ESkillLabRuleDecision::Reject;
	switch (Category)
	{
	case ESkillLabOperationCategory::Skill: Decision = Rules->Skill; break;
	case ESkillLabOperationCategory::Dodge: Decision = Rules->Dodge; break;
	case ESkillLabOperationCategory::Movement: Decision = Rules->Movement; break;
	case ESkillLabOperationCategory::UserCancel: Decision = Rules->UserCancel; break;
	case ESkillLabOperationCategory::NormalHit: Decision = Rules->NormalHit; break;
	case ESkillLabOperationCategory::HeavyHit: Decision = Rules->HeavyHit; break;
	case ESkillLabOperationCategory::UnknownHit: Decision = Rules->UnknownHit; break;
	}
	if (Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Recovery
		&& (Decision == ESkillLabRuleDecision::CancelAndActivate || Decision == ESkillLabRuleDecision::Allow))
	{
		OutReason = TEXT("RecoveryBlocksActivation");
		Decision = ESkillLabRuleDecision::Buffer;
	}
	UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] Cast=%d Operation=%d Rule=%s Decision=%d"), Execution.Token.CastId,
		static_cast<int32>(Category), *OutReason.ToString(), static_cast<int32>(Decision));
	return Decision;
}

ESkillLabRuleDecision USkillLabSkillRuntimeComponent::EvaluateAbilityCategory(const USkillLabCastExecution& Execution, const FGameplayTag Category, FName& OutReason) const
{
	const FSkillLabOperationRules* Rules = ResolveOperationRules(Execution, OutReason);
	if (!Rules) return ESkillLabRuleDecision::Reject;
	const ESkillLabRuleDecision* Configured = Rules->AbilityCategories.Find(Category);
	ESkillLabRuleDecision Decision = ESkillLabRuleDecision::Reject;
	if (Configured) Decision = *Configured;
	else if (Category == SkillLabGASTags::Ability_Skill) Decision = Rules->Skill;
	else if (Category == SkillLabGASTags::Ability_Dodge) Decision = Rules->Dodge;
	else OutReason = TEXT("UnknownAbilityCategory");
	if (Execution.Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Recovery && (Decision == ESkillLabRuleDecision::CancelAndActivate || Decision == ESkillLabRuleDecision::Allow))
	{
		OutReason = TEXT("RecoveryBlocksActivation");
		Decision = ESkillLabRuleDecision::Buffer;
	}
	UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] Cast=%d AbilityCategory=%s Rule=%s Decision=%d"), Execution.Token.CastId,
		*Category.ToString(), *OutReason.ToString(), static_cast<int32>(Decision));
	return Decision;
}

bool USkillLabSkillRuntimeComponent::RequestNonSkillOperation(const ESkillLabOperationCategory Category, FName& OutReason)
{
 if (Category != ESkillLabOperationCategory::Movement && Category != ESkillLabOperationCategory::UserCancel)
 { OutReason = TEXT("InvalidOperation"); return false; }
 const auto Tokens = GetExecutionTokens();
 TArray<FSkillLabCastExecutionToken> Interrupt;
 bool bAllowed = true;
 OutReason = TEXT("NoActiveCast");
 for (const auto& Token : Tokens)
 {
  const auto* E = FindExecution(Token);
  if (!E || E->bFinishing) continue;
  FName Reason;
  const auto Decision = EvaluateOperation(*E, Category, Reason);
  if (Decision == ESkillLabRuleDecision::Interrupt) Interrupt.Add(Token);
  else if (Decision != ESkillLabRuleDecision::Continue && Decision != ESkillLabRuleDecision::Allow)
  { bAllowed = false; OutReason = Reason; }
 }
 if (!bAllowed) return false;
 for (const auto& Token : Interrupt) CancelCast(Token, Category == ESkillLabOperationCategory::Movement ? TEXT("Movement") : TEXT("UserCancel"));
 return true;
}

void USkillLabSkillRuntimeComponent::NotifyNonInterruptingOperation(const ESkillLabOperationCategory Category)
{
	FName Reason;
	RequestNonSkillOperation(Category, Reason);
}

void USkillLabSkillRuntimeComponent::NotifyDamageTaken(const FGameplayTag AttackCategory, const FSkillLabDamageResult& DamageResult)
{
 const auto Category = AttackCategory == SkillLabGASTags::Event_Attack_Heavy ? ESkillLabOperationCategory::HeavyHit
  : AttackCategory == SkillLabGASTags::Event_Attack_Normal ? ESkillLabOperationCategory::NormalHit : ESkillLabOperationCategory::UnknownHit;
 for (const auto& Token : GetExecutionTokens())
 {
  const auto* E = FindExecution(Token);
  FName Reason;
  if (E && !E->bFinishing && EvaluateOperation(*E, Category, Reason) == ESkillLabRuleDecision::Interrupt)
   CancelCast(Token, Category == ESkillLabOperationCategory::HeavyHit ? TEXT("HeavyHit") : TEXT("DamageInterrupt"));
 }
}


ESkillLabRuleDecision USkillLabSkillRuntimeComponent::BuildActivationPlan(const FGameplayTag Category,
 TArray<FSkillLabCastExecutionToken>& OutCancel, FName& OutReason) const
{
 OutCancel.Reset();
 bool bWait = !ActivationBindings.IsEmpty();
 OutReason = NAME_None;
 for (const auto& Token : GetExecutionTokens())
 {
  const auto* E = FindExecution(Token);
  if (!E || E->bFinishing) { bWait = true; continue; }
  FName Reason;
  const auto Decision = EvaluateAbilityCategory(*E, Category, Reason);
  if (Decision == ESkillLabRuleDecision::Reject || Decision == ESkillLabRuleDecision::Interrupt)
  { OutCancel.Reset(); OutReason = Reason; return ESkillLabRuleDecision::Reject; }
  if (Decision == ESkillLabRuleDecision::CancelAndActivate) OutCancel.Add(Token);
  else if (Decision != ESkillLabRuleDecision::Allow) { bWait = true; OutReason = Reason; }
 }
 if (bWait) { OutCancel.Reset(); return ESkillLabRuleDecision::Buffer; }
 return ESkillLabRuleDecision::Allow;
}
