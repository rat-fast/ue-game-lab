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

USkillLabSkillRuntimeComponent::USkillLabSkillRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void USkillLabSkillRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeSkills();
}


void USkillLabSkillRuntimeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
 bShuttingDown = true;
 ResetAttempt.Reset();
 bResetRequested = false;
 bDeferredEndPlay = true;
 if (UnconfirmedSubmissions.IsValid() && UnconfirmedSubmissions->FeedbackRuntime.Get() == this)
  UnconfirmedSubmissions->FeedbackRuntime.Reset();
 ClearPendingSkillIntent(TEXT("EndPlay"));
 CancelActiveCast(TEXT("EndPlay"));
 if (!AnySubmitting()) FinishEndPlayCleanup();
 Super::EndPlay(EndPlayReason);
}

void USkillLabSkillRuntimeComponent::FinishEndPlayCleanup()
{
 if (AnySubmitting()) return;
 bDeferredEndPlay = false;
 bResetting = true;
 CancelActiveCast(TEXT("EndPlay"));
 ++RuntimeGeneration;
 // 独立持续 GE 的寿命由 GAS 持有；销毁 Runtime 只解除观察。
 if (EffectEventAdapter) EffectEventAdapter->Clear(false);
 if (EffectExecutor) EffectExecutor->Shutdown();
 ClearInterruptListeners();
 if (ASC.IsValid() && DamageTakenHandle.IsValid()) ASC->OnSkillLabDamageTaken.Remove(DamageTakenHandle);
 DamageTakenHandle.Reset();
 if (Loadout) Loadout->RevokeGrantedAbilities();
}

void USkillLabSkillRuntimeComponent::InitializeSkills()
{
	if (bInitialized || bShuttingDown)
	{
		return;
	}
	AActor* Owner = GetOwner();
	ASC = Owner ? Owner->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!ASC.IsValid())
	{
		return;
	}
	ASC->InitializeSkillLabAbilityActorInfo(Owner, Owner);
	UnconfirmedSubmissions = USkillLabUnconfirmedSubmissions::FindOrCreate(Owner, ASC.Get());
	if (!UnconfirmedSubmissions.IsValid()) return;
	DamageTakenHandle = ASC->OnSkillLabDamageTaken.AddUObject(this, &USkillLabSkillRuntimeComponent::NotifyDamageTaken);
	EffectEventAdapter = NewObject<USkillLabSkillEffectEventAdapter>(this);
	if (EffectEventAdapter)
	{
		EffectEventAdapter->OnEffectEvent.AddUObject(this, &USkillLabSkillRuntimeComponent::ForwardEffectEvent);
	}
	EffectExecutor = NewObject<USkillLabSkillEffectExecutor>(this);
	if (EffectExecutor)
	{
		EffectExecutor->Initialize(this, this, EffectEventAdapter);
	}
	UnitId = Owner ? Owner->GetUniqueID() : 0;

	Loadout = NewObject<USkillLabSkillLoadout>(this);
	bInitialized = Loadout.Get() != nullptr && Loadout->Initialize(ASC.Get());
	if (bInitialized)
	{
		UnconfirmedSubmissions->FeedbackRuntime = this;
		FSkillLabSkillResult Result = MakeResult();
		Result.bSuccess = true;
		Emit(ESkillLabSkillEvent::Completed, Result, TEXT("Initialized"));
	}
}


bool USkillLabSkillRuntimeComponent::IsAlive() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	return bInitialized && Attributes && Attributes->GetHealth() > 0.0f;
}


bool USkillLabSkillRuntimeComponent::IsEnemy(const USkillLabSkillRuntimeComponent* Other) const
{
	return Other && Other != this && TeamId != Other->TeamId;
}


float USkillLabSkillRuntimeComponent::GetCooldownRemaining(const FName SkillId) const
{
	return UnconfirmedSubmissions.IsValid() ? UnconfirmedSubmissions->GetCooldownRemaining(SkillId) : 0.0f;
}


float USkillLabSkillRuntimeComponent::GetEnergyValue() const
{
	return ASC.IsValid() ? ASC->GetActualEnergy() : 0.0f;
}


float USkillLabSkillRuntimeComponent::GetReservedEnergyValue() const
{
	return ASC.IsValid() ? ASC->GetReservedEnergy() : 0.0f;
}


float USkillLabSkillRuntimeComponent::GetAvailableEnergyValue() const
{
	return ASC.IsValid() ? ASC->GetAvailableEnergy() : 0.0f;
}


float USkillLabSkillRuntimeComponent::GetMaxEnergyValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetMaxEnergy() : 0.0f;
}


float USkillLabSkillRuntimeComponent::GetHealthValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetHealth() : 0.0f;
}


float USkillLabSkillRuntimeComponent::GetMaxHealthValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetMaxHealth() : 0.0f;
}


ESkillLabCastStage USkillLabSkillRuntimeComponent::GetCastStage() const
{
 const auto* Execution = GetSingleExecution();
 return Execution ? ToPublicStage(Execution->Lifecycle.GetStage()) : ESkillLabCastStage::Idle;
}

float USkillLabSkillRuntimeComponent::GetStageRemaining() const
{
 const auto* Execution = GetSingleExecution();
 if (!Execution || Execution->StageEndTime <= 0.0) return 0.0f;
 const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : Execution->StageEndTime;
 return FMath::Max(0.0f, static_cast<float>(Execution->StageEndTime - Now));
}

FName USkillLabSkillRuntimeComponent::GetActiveSkillId() const
{
 const auto* Execution = GetSingleExecution();
 return Execution ? Execution->ActiveDefinition.SkillId : NAME_None;
}

void USkillLabSkillRuntimeComponent::GetGrantedSkillIds(TArray<FName>& OutIds) const
{
	if (Loadout)
	{
		Loadout->GetGrantedSkillIds(OutIds);
	}
}


void USkillLabSkillRuntimeComponent::ResetSkills()
{
 if (bShuttingDown || bResetting) return;
 bResetting = true;
 bResetRequested = true;
 FResetAttempt Attempt;
 Attempt.Id = NextResetAttemptId++;
 if (!Attempt.Id) Attempt.Id = NextResetAttemptId++;
 Attempt.Generation = RuntimeGeneration;
 Attempt.SourceASC = ASC;
 Attempt.SourceASCKey = FObjectKey(ASC.Get());
 ResetAttempt = Attempt;
 const uint64 AttemptId = Attempt.Id;
 const uint64 Generation = Attempt.Generation;
 ClearPendingSkillIntent(TEXT("Reset"));
 if (!IsResetAttemptCurrent(AttemptId, Generation)) return;
 for (const auto& Token : GetExecutionTokens())
 {
  if (auto* Execution = FindMutableExecution(Token.CastId))
  {
   Execution->bDeferredSubmissionReset = true;
   CancelExecution(*Execution, TEXT("Reset"));
   if (!IsResetAttemptCurrent(AttemptId, Generation)) return;
  }
 }
 ResetAttempt->Phase = EResetPhase::Cleaning;
 if (!AnySubmitting()) CompleteReset();
}

bool USkillLabSkillRuntimeComponent::IsResetAttemptCurrent(uint64 AttemptId, uint64 Generation) const
{
 const AActor* Owner = GetOwner();
 return IsValid(this) && !IsBeingDestroyed() && !bShuttingDown && bResetting && ResetAttempt.IsSet()
  && ResetAttempt->Id == AttemptId && ResetAttempt->Generation == Generation && RuntimeGeneration == Generation
  && IsValid(Owner) && !Owner->IsActorBeingDestroyed() && IsValid(GetWorld());
}

void USkillLabSkillRuntimeComponent::FinishResetAttempt(uint64 AttemptId, uint64 Generation, FName Reason)
{
 if (!IsResetAttemptCurrent(AttemptId, Generation)) return;
 ResetAttempt.Reset();
 bResetRequested = false;
 bResetting = false;
 UE_LOG(LogTemp, Log, TEXT("[SkillLabReset] attempt=%llu generation=%llu result=%s"), AttemptId, Generation,
  Reason.IsNone() ? TEXT("ResetCompleted") : *Reason.ToString());
 if (!Reason.IsNone()) Emit(ESkillLabSkillEvent::Rejected, MakeResult(), Reason);
}

void USkillLabSkillRuntimeComponent::OnResetResourceFinished(uint64 Generation, const FSkillLabResourceReceipt& Receipt)
{
 if (!IsResetAttemptCurrent(Receipt.AttemptId, Generation) || ResetAttempt->Phase != EResetPhase::Waiting
  || Receipt.ASC != ResetAttempt->SourceASCKey) return;
 FName Reason = NAME_None;
 if (Receipt.Reason == TEXT("ResetBlockedByUnconfirmedSubmission")
  || (UnconfirmedSubmissions.IsValid() && UnconfirmedSubmissions->HasUnconfirmed()))
  Reason = TEXT("ResetBlockedByUnconfirmedSubmission");
 else if (Receipt.Result != ESkillLabResourceCompletion::Applied)
 {
  switch (Receipt.Result)
  {
  case ESkillLabResourceCompletion::Rejected: Reason = TEXT("ResetResourceChangeRejected"); break;
  case ESkillLabResourceCompletion::Failed: Reason = TEXT("ResetResourceChangeFailed"); break;
  default: Reason = TEXT("ResetResourceChangeUnavailable"); break;
  }
 }
 else if (UnconfirmedSubmissions.IsValid()) UnconfirmedSubmissions->ResetConfirmedCooldowns();
 UE_LOG(LogTemp, Log, TEXT("[SkillLabReset] attempt=%llu generation=%llu resource=%d reason=%s"), Receipt.AttemptId,
  Generation, static_cast<int32>(Receipt.Result), *Receipt.Reason.ToString());
 FinishResetAttempt(Receipt.AttemptId, Generation, Reason);
}

void USkillLabSkillRuntimeComponent::CompleteReset()
{
 if (!bResetRequested || AnySubmitting() || bShuttingDown || !ResetAttempt.IsSet()
  || ResetAttempt->Phase != EResetPhase::Cleaning) return;
 const uint64 AttemptId = ResetAttempt->Id;
 uint64 Generation = ResetAttempt->Generation;
 if (!IsResetAttemptCurrent(AttemptId, Generation)) return;
 bResetRequested = false;
 ++RuntimeGeneration;
 Generation = RuntimeGeneration;
 ResetAttempt->Generation = Generation;
 if (EffectEventAdapter) EffectEventAdapter->Clear(true);
 if (!IsResetAttemptCurrent(AttemptId, Generation)) return;
 AcceptedRequestIds.Reset();
 AcceptedRequestOrder.Reset();
 if (UnconfirmedSubmissions.IsValid() && UnconfirmedSubmissions->HasUnconfirmed())
 {
  FinishResetAttempt(AttemptId, Generation, TEXT("ResetBlockedByUnconfirmedSubmission"));
  return;
 }
 auto* SourceASC = ResetAttempt->SourceASC.Get();
 if (!IsValid(SourceASC) || SourceASC != ASC.Get() || SourceASC->IsBeingDestroyed() || !SourceASC->GetCombatAttributeSet())
 {
  FinishResetAttempt(AttemptId, Generation, TEXT("ResetResourceChangeUnavailable"));
  return;
 }
 const float ResetEnergy = SourceASC->GetCombatAttributeSet()->GetMaxEnergy();
 SourceASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), SourceASC->GetCombatAttributeSet()->GetMaxHealth());
 if (!IsResetAttemptCurrent(AttemptId, Generation)) return;
 if (UnconfirmedSubmissions.IsValid() && UnconfirmedSubmissions->HasUnconfirmed())
 {
  FinishResetAttempt(AttemptId, Generation, TEXT("ResetBlockedByUnconfirmedSubmission"));
  return;
 }
 if (!IsValid(SourceASC) || SourceASC != ASC.Get() || SourceASC->IsBeingDestroyed())
 {
  FinishResetAttempt(AttemptId, Generation, TEXT("ResetResourceChangeUnavailable"));
  return;
 }
 ResetAttempt->Phase = EResetPhase::Waiting;
 TWeakObjectPtr<USkillLabSkillRuntimeComponent> WeakThis(this);
 TWeakObjectPtr<USkillLabAbilitySystemComponent> WeakASC(SourceASC);
 SourceASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, ResetEnergy, AttemptId, this,
  [WeakThis, WeakASC, AttemptId, Generation]() -> FName
  {
   const auto* Runtime = WeakThis.Get();
   if (!Runtime || !Runtime->IsResetAttemptCurrent(AttemptId, Generation) || !WeakASC.IsValid()
    || Runtime->ASC != WeakASC || Runtime->ResetAttempt->SourceASC != WeakASC
    || Runtime->ResetAttempt->Phase != EResetPhase::Waiting) return TEXT("ResetResourceChangeUnavailable");
   if (Runtime->UnconfirmedSubmissions.IsValid() && Runtime->UnconfirmedSubmissions->HasUnconfirmed())
    return TEXT("ResetBlockedByUnconfirmedSubmission");
   return NAME_None;
  },
  [WeakThis, Generation](const FSkillLabResourceReceipt& Receipt)
  {
   if (auto* Runtime = WeakThis.Get()) Runtime->OnResetResourceFinished(Generation, Receipt);
  });
}

void USkillLabSkillRuntimeComponent::ForwardEffectEvent(const FSkillLabSkillResult& Result)
{
	Emit(Result.Event, Result, Result.Reason);
}


void USkillLabSkillRuntimeComponent::Emit(const ESkillLabSkillEvent EventType, const FSkillLabSkillResult& Result, const FName Reason)
{
	FSkillLabSkillResult EventResult = Result;
	EventResult.Event = EventType;
	if (!Reason.IsNone())
	{
		EventResult.Reason = Reason;
	}
	OnSkillEvent.Broadcast(EventResult);
	if (IsValid(this) && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
		&& (EventType != ESkillLabSkillEvent::SubmissionResolved || CanReceiveSubmissionConfirmation(UnconfirmedSubmissions.Get())))
		OnNativeSkillEvent.Broadcast(EventResult);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabSkills] Event=%d Skill=%s Request=%d Cast=%d Source=%d Target=%d Success=%s Reason=%s Amount=%.2f"),
		static_cast<int32>(EventType), *EventResult.SkillId.ToString(), EventResult.RequestId, EventResult.CastId,
		EventResult.SourceUnitId, EventResult.TargetUnitId, EventResult.bSuccess ? TEXT("true") : TEXT("false"),
		*EventResult.Reason.ToString(), EventResult.ActualAmount);
}
