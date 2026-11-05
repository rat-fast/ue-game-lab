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
	bResetting = true;
	ClearPendingSkillIntent(TEXT("EndPlay"));
	++RuntimeGeneration;
	if (EffectEventAdapter) EffectEventAdapter->Clear(true);
	if (EffectExecutor) EffectExecutor->Shutdown();
	if (ActiveCastId != 0)
	{
		const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
		if (Lifecycle.Cancel(Token, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0))
		{
			FinishCast(true, TEXT("EndPlay"));
		}
	}
	ClearInterruptListeners();
	if (ASC.IsValid() && DamageTakenHandle.IsValid()) ASC->OnSkillLabDamageTaken.Remove(DamageTakenHandle);
	DamageTakenHandle.Reset();
	Super::EndPlay(EndPlayReason);
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
	const double* End = CooldownEnd.Find(SkillId);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return End ? FMath::Max(0.0f, static_cast<float>(*End - Now)) : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetEnergyValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetEnergy() : 0.0f;
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
	return ToPublicStage(Lifecycle.GetStage());
}

float USkillLabSkillRuntimeComponent::GetStageRemaining() const
{
	if (ActiveCastId == 0 || StageEndTime <= 0.0)
	{
		return 0.0f;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : StageEndTime;
	return FMath::Max(0.0f, static_cast<float>(StageEndTime - Now));
}

FName USkillLabSkillRuntimeComponent::GetActiveSkillId() const
{
	return ActiveCastId != 0 ? ActiveDefinition.SkillId : NAME_None;
}

void USkillLabSkillRuntimeComponent::GetGrantedSkillIds(TArray<FName>& OutIds) const
{
	if (Loadout)
	{
		Loadout->GetGrantedSkillIds(OutIds);
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

FSkillLabSkillResult USkillLabSkillRuntimeComponent::MakeResult() const
{
	FSkillLabSkillResult Result;
	Result.SkillId = ActiveDefinition.SkillId;
	Result.CastId = ActiveCastId;
	Result.SourceUnitId = UnitId;
	Result.SourceActor = GetOwner();
	Result.Stage = ToPublicStage(Lifecycle.GetStage());
	Result.Effect = ActiveDefinition.Effect;
	Result.RequestId = ActiveRequest.RequestId;
	return Result;
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

bool USkillLabSkillRuntimeComponent::CheckGates(FName& OutReason) const
{
	OutReason = NAME_None;
	if (!bInitialized || !ASC.IsValid())
	{
		OutReason = TEXT("NotInitialized");
	}
	else if (!IsAlive() || ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead))
	{
		OutReason = TEXT("CasterDead");
	}
	else if (ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Stunned)
		|| ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Silenced))
	{
		OutReason = TEXT("BlockedByControlState");
	}
	else if (ASC->HasAnySkillLabBlockTag(ActiveDefinition.BlockedByTags))
	{
		OutReason = TEXT("BlockedByState");
	}
	return OutReason.IsNone();
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillRequest(const FSkillLabSkillRequest& InRequest)
{
	return SubmitSkillOperationRequest(InRequest, ESkillLabOperationCategory::Skill);
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillOperationRequest(const FSkillLabSkillRequest& InRequest,
	const ESkillLabOperationCategory Category)
{
	FSkillLabSkillRequest Request = InRequest;
	if (Request.RequestId > 0)
	{
		if (AcceptedRequestIds.Contains(Request.RequestId))
		{
			return Reject(Request, TEXT("RequestDuplicate"));
		}
	}
	else
	{
		do { Request.RequestId = NextRequestId++; } while (AcceptedRequestIds.Contains(Request.RequestId));
	}
	FSkillLabSkillDefinition Definition;
	FName Reason;
	if (bShuttingDown || bResetting || !GetWorld() || !bInitialized || !ASC.IsValid())
	{
		return Reject(Request, bResetting ? TEXT("Resetting") : TEXT("NotInitialized"));
	}
	if (Category != ESkillLabOperationCategory::Skill && Category != ESkillLabOperationCategory::Dodge)
	{
		return Reject(Request, TEXT("OperationHasNoSkillRequest"));
	}
	if (Category == ESkillLabOperationCategory::Dodge && ActiveCastId != 0
		&& Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Recovery)
	{
		ClearPendingSkillIntent(TEXT("DodgeReplacedInput"));
		CancelActiveCast(TEXT("DodgeRequest"));
	}
	if (bFinishing || ActiveCastId != 0 || PendingSkillIntent.IsSet())
	{
		StorePendingSkillIntent(Request, Category);
		return Reject(Request, TEXT("InputBufferedLatestWins"));
	}
	if (!FSkillLabSkillDefinitionProvider::Resolve(Request.SkillId, Definition))
	{
		return Reject(Request, TEXT("SkillUnknown"));
	}
	ActiveDefinition = Definition;
	if (!CheckGates(Reason))
	{
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, Reason);
	}
	if (!IsValid(Request.Caster) || Request.Caster != GetOwner())
	{
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, Request.Caster ? TEXT("CasterMismatch") : TEXT("CasterRequired"));
	}
	if (Loadout.Get() == nullptr || !Loadout->IsGranted(Request.SkillId))
	{
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, TEXT("SkillNotGranted"));
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (const double* Cooldown = CooldownEnd.Find(Request.SkillId); Cooldown && Now < *Cooldown)
	{
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, TEXT("Cooldown"));
	}
	const USkillLabCombatAttributeSet* Attributes = ASC->GetCombatAttributeSet();
	if (!Attributes || Attributes->GetEnergy() < Definition.Cost)
	{
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, TEXT("InsufficientEnergy"));
	}
	AActor* ResolvedTarget = nullptr;
	if (!FSkillLabTargetingService::ValidateRequest(*this, Request, Definition, ResolvedTarget, Reason))
	{
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, Reason);
	}
	Request.Target = ResolvedTarget;
	RememberRequestId(Request.RequestId);

	ActiveRequest = Request;
	ActiveCastId = NextCastId++;
	ActiveCastGeneration = RuntimeGeneration;
	bCommitted = false;
	bFinishing = false;
	bEffectSubmitted = false;
	ActiveEffectOperationId = NAME_None;
	ActiveEffectResult = FSkillLabSkillResult();
	ActiveEffectResults.Reset();
	OutcomeReason = NAME_None;
	FSkillLabCastLifecycle::FConfig LifecycleConfig;
	LifecycleConfig.WindupSeconds = Definition.Phases.WindupSeconds;
	LifecycleConfig.ActiveSeconds = Definition.Phases.ActiveSeconds;
	LifecycleConfig.RecoverySeconds = Definition.Phases.RecoverySeconds;
	LifecycleConfig.bUseActiveTimeout = Definition.Phases.ActiveSeconds > 0.0f;
	if (!Lifecycle.Begin(static_cast<uint64>(ActiveCastId), LifecycleConfig, Now))
	{
		ForgetRequestId(Request.RequestId);
		ActiveCastId = 0;
		ActiveRequest = FSkillLabSkillRequest();
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, TEXT("LifecycleStartFailed"));
	}
	if (!ASC->TryActivateAbility(Loadout->FindGrantedHandle(Request.SkillId)) || !ActiveAbility.IsValid())
	{
		const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
		Lifecycle.Cancel(Token, Now);
		ClearPendingCast();
		ForgetRequestId(Request.RequestId);
		return Reject(Request, TEXT("ActivationFailed"));
	}

	ListenForInterrupts();
	const int32 StartedCastId = ActiveCastId;
	const FSkillLabCastPhaseToken WindupToken = Lifecycle.GetPhaseToken();
	StageEndTime = Definition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
		? Now + Definition.ReleaseEventTimeout : Now + Definition.Phases.WindupSeconds;
	FSkillLabSkillResult Started = MakeResult();
	Started.bSuccess = true;
	Started.Stage = ToPublicStage(Lifecycle.GetStage());
	Started.TargetActor = Request.Target;
	Started.TargetLocation = Definition.TargetShape == ESkillLabTargetShape::Ground
		? Request.TargetLocation
		: (Request.Target ? Request.Target->GetActorLocation() : FVector::ZeroVector);
	Started.VisualRadius = Definition.Radius;
	Started.StageDuration = Definition.Phases.WindupSeconds;
	Emit(ESkillLabSkillEvent::Started, Started);
	// Events may synchronously cancel, release, or even start another cast.
	if (ActiveCastId != StartedCastId || !Lifecycle.IsTokenCurrent(WindupToken)) return Started;
	if (Definition.ReleaseTrigger == ESkillLabReleaseTrigger::Timer)
	{
		if (Definition.Phases.WindupSeconds > 0.0f)
		{
			FTimerDelegate ReleaseDelegate;
			ReleaseDelegate.BindLambda([WeakThis = TWeakObjectPtr<USkillLabSkillRuntimeComponent>(this), StartedCastId]()
			{
				if (WeakThis.IsValid()) WeakThis->ReleaseCast(WeakThis->ActiveAbility.Get(), StartedCastId);
			});
			GetWorld()->GetTimerManager().SetTimer(PhaseTimer, ReleaseDelegate, Definition.Phases.WindupSeconds, false);
		}
		else ReleaseCast(ActiveAbility.Get(), StartedCastId);
	}
	else
	{
		FTimerDelegate TimeoutDelegate;
		TimeoutDelegate.BindLambda([WeakThis = TWeakObjectPtr<USkillLabSkillRuntimeComponent>(this), StartedCastId, WindupToken]()
		{
			if (WeakThis.IsValid() && WeakThis->ActiveCastId == StartedCastId && WeakThis->Lifecycle.IsTokenCurrent(WindupToken))
				WeakThis->CancelActiveCast(TEXT("ReleaseEventTimeout"));
		});
		GetWorld()->GetTimerManager().SetTimer(ActiveTimeoutTimer, TimeoutDelegate, Definition.ReleaseEventTimeout, false);
	}
	return Started;
}

bool USkillLabSkillRuntimeComponent::ForwardValidatedNotify(const int32 CastId, const FGameplayTag EventTag)
{
	if (bShuttingDown || bFinishing || CastId <= 0 || ActiveCastId != CastId || !EventTag.IsValid()
		|| !ActiveAbility.IsValid() || !ASC.IsValid())
	{
		return false;
	}
	const bool bIsReleaseEvent = ActiveDefinition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
		&& EventTag == ActiveDefinition.ReleaseEventTag;
	const bool bIsBehaviorEvent = ActiveDefinition.BehaviorEventTags.HasTagExact(EventTag);
	if (!bIsReleaseEvent && !bIsBehaviorEvent)
	{
		return false;
	}
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetOwner();
	Payload.Target = ActiveRequest.Target;
	USkillLabCastGameplayEventData* Identity = NewObject<USkillLabCastGameplayEventData>(this);
	Identity->Runtime = this;
	Identity->Ability = ActiveAbility;
	Identity->CastId = CastId;
	Payload.OptionalObject = Identity;
	ASC->HandleGameplayEvent(EventTag, &Payload);
	return true;
}

void USkillLabSkillRuntimeComponent::HandleAbilityGameplayEvent(USkillLabSkillGameplayAbility* Ability,
	const int32 CastId, const FGameplayTag EventTag)
{
	if (bShuttingDown || bFinishing || !Ability || ActiveAbility.Get() != Ability || CastId <= 0
		|| ActiveCastId != CastId || !EventTag.IsValid())
	{
		return;
	}
	if (Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Windup
		&& ActiveDefinition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
		&& EventTag == ActiveDefinition.ReleaseEventTag)
	{
		ReleaseCast(Ability, CastId);
	}
	else if (Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active && ActiveBehavior
		&& ActiveDefinition.BehaviorEventTags.HasTagExact(EventTag))
	{
		ActiveBehavior->ReceiveEvent(EventTag);
	}
}

void USkillLabSkillRuntimeComponent::ReleaseCast(USkillLabSkillGameplayAbility* Ability, const int32 CastId)
{
	const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
	if (bShuttingDown || bFinishing || bCommitted || ActiveCastId == 0 || !Lifecycle.IsTokenCurrent(Token)
		|| !Ability || ActiveAbility.Get() != Ability || ActiveCastId != CastId
		|| Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Windup)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	FName GateReason;
	AActor* ValidatedTarget = nullptr;
	if (!CheckGates(GateReason)
		|| !FSkillLabTargetingService::ValidateRequest(*this, ActiveRequest, ActiveDefinition, ValidatedTarget, GateReason))
	{
		CancelActiveCast(GateReason);
		return;
	}
	ActiveRequest.Target = ValidatedTarget;
	if (Lifecycle.AdvancePhase(Token, Now) != FSkillLabCastLifecycle::EAdvanceResult::EnteredActive)
	{
		CancelActiveCast(TEXT("ReleasePhaseFailed"));
		return;
	}
	const FSkillLabCastPhaseToken ActiveToken = Lifecycle.GetPhaseToken();
	const int32 ReleasedCastId = ActiveCastId;
	const USkillLabCombatAttributeSet* Attributes = ASC.IsValid() ? ASC->GetCombatAttributeSet() : nullptr;
	if (!Attributes || Attributes->GetEnergy() < ActiveDefinition.Cost || !Lifecycle.TryMarkRelease(ActiveToken))
	{
		CancelActiveCast(TEXT("ReleaseGateFailed"));
		return;
	}
	if (!Lifecycle.TryMarkSubmissionPoint(ActiveToken, 1)) return;
	bCommitted = true;
	CooldownEnd.Add(ActiveDefinition.SkillId, Now + ActiveDefinition.Cooldown);
	const float Cost = ActiveDefinition.Cost;
	ASC->ApplyModToAttribute(USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Additive, -Cost);
	if (ActiveCastId != ReleasedCastId || !Lifecycle.IsTokenCurrent(ActiveToken)) return;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
		GetWorld()->GetTimerManager().ClearTimer(ActiveTimeoutTimer);
	}
	FSkillLabSkillResult Phase = MakeResult();
	Phase.bSuccess = true;
	Phase.Stage = ESkillLabCastStage::Active;
	Phase.TargetActor = ActiveRequest.Target;
	Phase.TargetLocation = ActiveDefinition.TargetShape == ESkillLabTargetShape::Ground
		? ActiveRequest.TargetLocation
		: (ActiveRequest.Target ? ActiveRequest.Target->GetActorLocation() : FVector::ZeroVector);
	Phase.VisualRadius = ActiveDefinition.Radius;
	Phase.StageDuration = ActiveDefinition.Phases.ActiveSeconds;
	Emit(ESkillLabSkillEvent::PhaseChanged, Phase);
	if (ActiveCastId != ReleasedCastId || !Lifecycle.IsTokenCurrent(ActiveToken)) return;
	FSkillLabSkillResult TargetSelected = Phase;
	TargetSelected.Event = ESkillLabSkillEvent::TargetSelected;
	Emit(ESkillLabSkillEvent::TargetSelected, TargetSelected);
	if (ActiveCastId != ReleasedCastId || !Lifecycle.IsTokenCurrent(ActiveToken)) return;

	StageEndTime = ActiveDefinition.Phases.ActiveSeconds > 0.0f ? Now + ActiveDefinition.Phases.ActiveSeconds : 0.0;
	if (ActiveDefinition.Phases.ActiveSeconds > 0.0f)
	{
		FTimerDelegate ActiveDelegate;
		ActiveDelegate.BindLambda([WeakThis = TWeakObjectPtr<USkillLabSkillRuntimeComponent>(this), ReleasedCastId, ActiveToken]()
		{
			if (WeakThis.IsValid() && WeakThis->ActiveCastId == ReleasedCastId && WeakThis->Lifecycle.IsTokenCurrent(ActiveToken))
				WeakThis->CancelActiveCast(TEXT("ActiveTimeout"));
		});
		GetWorld()->GetTimerManager().SetTimer(ActiveTimeoutTimer, ActiveDelegate, ActiveDefinition.Phases.ActiveSeconds, false);
	}
	if (ActiveDefinition.BehaviorClass)
	{
		ActiveBehavior = NewObject<USkillLabSkillBehavior>(this, ActiveDefinition.BehaviorClass);
		if (ActiveBehavior)
		{
			ActiveBehavior->Initialize(this, ActiveDefinition, ActiveRequest, ActiveCastId, ActiveCastGeneration);
			ActiveBehavior->Execute();
			return;
		}
	}
	CancelActiveCast(TEXT("BehaviorCreateFailed"));
}

void USkillLabSkillRuntimeComponent::CancelActiveCast(const FName Reason)
{
	if (ActiveCastId == 0)
	{
		return;
	}
	const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
	if (Lifecycle.Cancel(Token, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0))
	{
		FinishCast(true, Reason);
	}
}

void USkillLabSkillRuntimeComponent::BeginRecovery()
{
	if (ActiveCastId == 0 || Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Active)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
	if (Lifecycle.AdvancePhase(Token, Now) != FSkillLabCastLifecycle::EAdvanceResult::EnteredRecovery)
	{
		return;
	}
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ActiveTimeoutTimer);
	}
	StageEndTime = ActiveDefinition.Phases.RecoverySeconds > 0.0f ? Now + ActiveDefinition.Phases.RecoverySeconds : Now;
	const int32 RecoveringCastId = ActiveCastId;
	const FSkillLabCastPhaseToken RecoveryToken = Lifecycle.GetPhaseToken();
	FSkillLabSkillResult Phase = MakeResult();
	Phase.bSuccess = true;
	Phase.Stage = ESkillLabCastStage::Recovery;
	Phase.StageDuration = ActiveDefinition.Phases.RecoverySeconds;
	Emit(ESkillLabSkillEvent::PhaseChanged, Phase);
	// A listener may cancel this cast and start another one during the broadcast.
	// Never schedule the previous cast's recovery against the new active definition.
	if (ActiveCastId != RecoveringCastId || !Lifecycle.IsTokenCurrent(RecoveryToken)) return;
	if (ActiveDefinition.Phases.RecoverySeconds > 0.0f)
	{
		FTimerDelegate RecoveryDelegate;
		RecoveryDelegate.BindLambda([WeakThis = TWeakObjectPtr<USkillLabSkillRuntimeComponent>(this), RecoveryToken]()
		{
			if (WeakThis.IsValid()) WeakThis->FinishRecovery(RecoveryToken);
		});
		GetWorld()->GetTimerManager().SetTimer(PhaseTimer, RecoveryDelegate, ActiveDefinition.Phases.RecoverySeconds, false);
	}
	else
	{
		FinishRecovery(RecoveryToken);
	}
}

void USkillLabSkillRuntimeComponent::FinishRecovery(const FSkillLabCastPhaseToken Token)
{
	if (ActiveCastId == 0 || !Lifecycle.IsTokenCurrent(Token) || Lifecycle.GetStage() != FSkillLabCastLifecycle::EStage::Recovery)
	{
		return;
	}
	if (Lifecycle.Finish(Token, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0))
	{
		FinishCast(false, OutcomeReason.IsNone() ? TEXT("Completed") : OutcomeReason);
	}
}

void USkillLabSkillRuntimeComponent::FinishCast(const bool bCancelled, const FName Reason)
{
	if (bFinishing || ActiveCastId == 0)
	{
		return;
	}
	bFinishing = true;
	OutcomeReason = Reason;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
		GetWorld()->GetTimerManager().ClearTimer(ActiveTimeoutTimer);
	}
	ClearInterruptListeners();
	if (ActiveBehavior)
	{
		ActiveBehavior->Cleanup();
	}
	if (ActiveAbility.IsValid())
	{
		USkillLabSkillGameplayAbility* Ability = ActiveAbility.Get();
		ActiveAbility = nullptr;
		if (Ability)
		{
			Ability->FinishSkillAbility(bCancelled);
		}
	}
	FSkillLabSkillResult Result = MakeResult();
	Result.bSuccess = !bCancelled;
	Result.Stage = bCancelled ? ESkillLabCastStage::Cancelled : ESkillLabCastStage::Completed;
	Emit(bCancelled ? ESkillLabSkillEvent::Cancelled : ESkillLabSkillEvent::Completed, Result, Reason);
	ActiveBehavior = nullptr;
	bEffectSubmitted = false;
	ActiveEffectOperationId = NAME_None;
	ActiveEffectResult = FSkillLabSkillResult();
	ActiveEffectResults.Reset();
	ActiveRequest = FSkillLabSkillRequest();
	ActiveDefinition = FSkillLabSkillDefinition();
	ActiveCastId = 0;
	ActiveCastGeneration = 0;
	StageEndTime = 0.0;
	bCommitted = false;
	bFinishing = false;
	if (bCancelled)
	{
		ClearPendingSkillIntent(TEXT("CastCancelled"));
	}
	else
	{
		SchedulePendingSkillIntentConsumption();
	}
}

void USkillLabSkillRuntimeComponent::ClearPendingCast()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(PhaseTimer);
		GetWorld()->GetTimerManager().ClearTimer(ActiveTimeoutTimer);
	}
	ClearInterruptListeners();
	if (ActiveAbility.IsValid())
	{
		USkillLabSkillGameplayAbility* Ability = ActiveAbility.Get();
		ActiveAbility = nullptr;
		if (Ability) Ability->FinishSkillAbility(true);
	}
	ActiveBehavior = nullptr;
	ActiveRequest = FSkillLabSkillRequest();
	ActiveDefinition = FSkillLabSkillDefinition();
	ActiveCastId = 0;
	ActiveCastGeneration = 0;
	StageEndTime = 0.0;
	bCommitted = false;
	bFinishing = false;
	bEffectSubmitted = false;
	ActiveEffectOperationId = NAME_None;
	ActiveEffectResult = FSkillLabSkillResult();
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

void USkillLabSkillRuntimeComponent::NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability)
{
	if (bShuttingDown || ActiveCastId == 0 || ActiveAbility.IsValid() || !Ability || !Loadout
		|| Ability->GetCurrentAbilitySpecHandle() != Loadout->FindGrantedHandle(ActiveRequest.SkillId))
	{
		return;
	}
	ActiveAbility = Ability;
	FGameplayTagContainer EventTags = ActiveDefinition.BehaviorEventTags;
	if (ActiveDefinition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent)
	{
		EventTags.AddTag(ActiveDefinition.ReleaseEventTag);
	}
	Ability->BindSkillCast(this, ActiveCastId, EventTags);
}

void USkillLabSkillRuntimeComponent::NotifyAbilityEnded(USkillLabSkillGameplayAbility* Ability)
{
	if (ActiveAbility != Ability)
	{
		return;
	}
	ActiveAbility = nullptr;
	if (!bFinishing && ActiveCastId != 0)
	{
		CancelActiveCast(TEXT("AbilityEnded"));
	}
}

bool USkillLabSkillRuntimeComponent::IsBehaviorCurrent(const USkillLabSkillBehavior* Behavior, const int32 CastId) const
{
	return !bShuttingDown && Behavior && ActiveBehavior == Behavior && ActiveCastId == CastId
		&& Behavior->RuntimeGeneration == RuntimeGeneration
		&& !bResetting && !bFinishing
		&& Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active;
}

bool USkillLabSkillRuntimeComponent::MatchesTargetRelation(const USkillLabSkillRuntimeComponent* Target,
	const ESkillLabTargetRelation Relation) const
{
	return Target && FSkillLabTargetingService::MatchesRelation(*this, *Target, Relation);
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::ApplyBehaviorEffect(const USkillLabSkillBehavior* Behavior,
	const int32 CastId, AActor* Target, const FSkillLabSkillDefinition& Definition)
{
	FSkillLabSkillResult Result = MakeResult();
	if (!IsBehaviorCurrent(Behavior, CastId))
	{
		Result.Reason = TEXT("CastNotActive");
		return Result;
	}
	for (const FSkillLabSkillResult& Existing : ActiveEffectResults)
	{
		if (Existing.TargetActor == Target)
		{
			Result = Existing;
			Result.Reason = TEXT("EffectAlreadySubmitted");
			return Result;
		}
	}
	ActiveEffectOperationId = FName(*FString::Printf(TEXT("SkillEffect_%d_%d"), CastId, ActiveEffectResults.Num() + 1));
	const uint64 TargetIdentity = IsValid(Target) ? static_cast<uint64>(Target->GetUniqueID()) : 0;
	const uint64 SubmissionPointId = (TargetIdentity << 1) + 2;
	if (!Lifecycle.TryMarkSubmissionPoint(Lifecycle.GetPhaseToken(), SubmissionPointId))
	{
		Result.Reason = TEXT("SubmissionPointAlreadyCommitted");
		return Result;
	}
	bEffectSubmitted = true;
	Result.EffectOperationId = ActiveEffectOperationId;
	if (EffectExecutor)
	{
		Result = EffectExecutor->Apply(Behavior, CastId, Target, Definition, Result);
	}
	else
	{
		Result.Reason = TEXT("EffectExecutorUnavailable");
	}
	Result.EffectOperationId = ActiveEffectOperationId;
	ActiveEffectResult = Result;
	ActiveEffectResults.Add(Result);
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
	if (!Behavior || !IsBehaviorCurrent(Behavior, CastId))
	{
		return;
	}
	FSkillLabSkillResult Result = MakeResult();
	Result.bSuccess = false;
	Result.TargetActor = Target;
	Emit(ESkillLabSkillEvent::Hit, Result, Reason);
}

void USkillLabSkillRuntimeComponent::CompleteBehavior(const USkillLabSkillBehavior* Behavior, const int32 CastId, const FName Reason)
{
	if (!Behavior || !IsBehaviorCurrent(Behavior, CastId))
	{
		return;
	}
	OutcomeReason = Reason;
	BeginRecovery();
}

void USkillLabSkillRuntimeComponent::ListenForInterrupts()
{
	ClearInterruptListeners();
	if (!ASC.IsValid())
	{
		return;
	}
	TSet<FGameplayTag> RegisteredTags;
	const auto RegisterInterruptTag = [this, &RegisteredTags](const FGameplayTag& Tag)
	{
		RegisteredTags.Add(Tag);
		const FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &USkillLabSkillRuntimeComponent::OnBlockingTagChanged);
		InterruptHandles.Add(Tag, Handle);
	};
	RegisterInterruptTag(SkillLabGASTags::State_Dead);
	RegisterInterruptTag(SkillLabGASTags::State_Stunned);
	RegisterInterruptTag(SkillLabGASTags::State_Silenced);
	for (const FGameplayTag Tag : ActiveDefinition.BlockedByTags)
	{
		if (!Tag.IsValid() || RegisteredTags.Contains(Tag)) continue;
		RegisteredTags.Add(Tag);
		FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(Tag, EGameplayTagEventType::AnyCountChange)
			.AddUObject(this, &USkillLabSkillRuntimeComponent::OnBlockingTagChanged);
		InterruptHandles.Add(Tag, Handle);
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
	if (Count > 0 && ActiveCastId != 0 && (Tag == SkillLabGASTags::State_Dead
		|| Tag == SkillLabGASTags::State_Stunned || Tag == SkillLabGASTags::State_Silenced
		|| ActiveDefinition.BlockedByTags.HasTag(Tag)))
	{
		CancelActiveCast(Tag == SkillLabGASTags::State_Dead ? TEXT("CasterDead")
			: (Tag == SkillLabGASTags::State_Stunned || Tag == SkillLabGASTags::State_Silenced)
				? TEXT("BlockedByControlState") : TEXT("BlockedByState"));
	}
}

void USkillLabSkillRuntimeComponent::NotifyNonInterruptingOperation(const ESkillLabOperationCategory Category)
{
	if (Category == ESkillLabOperationCategory::Movement || Category == ESkillLabOperationCategory::UserCancel)
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SkillLabCombatRules] Operation=%d Cast=%d Decision=Continue"), static_cast<int32>(Category), ActiveCastId);
	}
}

void USkillLabSkillRuntimeComponent::NotifyDamageTaken(const FGameplayTag AttackCategory, const FSkillLabDamageResult& DamageResult)
{
	(void)DamageResult;
	if (ActiveCastId == 0) return;
	if (AttackCategory == SkillLabGASTags::Event_Attack_Heavy)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] DamageCategory=%s Cast=%d Decision=Interrupt"), *AttackCategory.ToString(), ActiveCastId);
		CancelActiveCast(TEXT("HeavyHit"));
	}
	else if (AttackCategory == SkillLabGASTags::Event_Attack_Normal)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] DamageCategory=%s Cast=%d Decision=Continue"), *AttackCategory.ToString(), ActiveCastId);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] DamageCategory=%s Cast=%d Decision=Continue Reason=UnknownCategory"), *AttackCategory.ToString(), ActiveCastId);
	}
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
	const ESkillLabOperationCategory Category)
{
	if (PendingSkillIntent.IsSet())
	{
		Reject(PendingSkillIntent->Request, TEXT("BufferedIntentReplaced"));
	}
	FSkillLabSkillInputIntent Intent;
	Intent.Request = Request;
	Intent.Category = Category;
	PendingSkillIntent = MoveTemp(Intent);
}

void USkillLabSkillRuntimeComponent::SchedulePendingSkillIntentConsumption()
{
	if (!PendingSkillIntent.IsSet() || ActiveCastId != 0 || bFinishing || bResetting || bShuttingDown || !GetWorld())
	{
		return;
	}
	FTimerDelegate ConsumeDelegate;
	ConsumeDelegate.BindWeakLambda(this, [this]()
	{
		PendingIntentTimer.Invalidate();
		ConsumePendingSkillIntent();
	});
	PendingIntentTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(ConsumeDelegate);
}

void USkillLabSkillRuntimeComponent::ConsumePendingSkillIntent()
{
	if (!PendingSkillIntent.IsSet() || ActiveCastId != 0 || bFinishing || bResetting || bShuttingDown || !bInitialized)
	{
		return;
	}
	if (!IsAlive() || (ASC.IsValid() && ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead)))
	{
		ClearPendingSkillIntent(TEXT("CasterDead"));
		return;
	}

	FSkillLabSkillInputIntent Intent = MoveTemp(PendingSkillIntent.GetValue());
	PendingSkillIntent.Reset();
	Intent.Request.RequestId = 0;
	UE_LOG(LogTemp, Log, TEXT("[SkillLabCombatRules] ConsumeBufferedIntent OldCastCleared=true ActiveCast=%d AbilityValid=%s Skill=%s"),
		ActiveCastId, ActiveAbility.IsValid() ? TEXT("true") : TEXT("false"), *Intent.Request.SkillId.ToString());
	SubmitSkillOperationRequest(Intent.Request, Intent.Category);
}

void USkillLabSkillRuntimeComponent::ResetSkills()
{
	if (bShuttingDown) return;
	bResetting = true;
	++RuntimeGeneration;
	ClearPendingSkillIntent(TEXT("Reset"));
	CancelActiveCast(TEXT("Reset"));
	if (EffectEventAdapter) EffectEventAdapter->Clear(true);
	bEffectSubmitted = false;
	ActiveEffectOperationId = NAME_None;
	ActiveEffectResult = FSkillLabSkillResult();
	ActiveEffectResults.Reset();
	CooldownEnd.Reset();
	AcceptedRequestIds.Reset();
	AcceptedRequestOrder.Reset();
	if (ASC.IsValid() && ASC->GetCombatAttributeSet())
	{
		const USkillLabCombatAttributeSet* Attributes = ASC->GetCombatAttributeSet();
		ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
		ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), Attributes->GetMaxEnergy());
	}
	bResetting = false;
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
	OnNativeSkillEvent.Broadcast(EventResult);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabSkills] Event=%d Skill=%s Request=%d Cast=%d Source=%d Target=%d Success=%s Reason=%s Amount=%.2f"),
		static_cast<int32>(EventType), *EventResult.SkillId.ToString(), EventResult.RequestId, EventResult.CastId,
		EventResult.SourceUnitId, EventResult.TargetUnitId, EventResult.bSuccess ? TEXT("true") : TEXT("false"),
		*EventResult.Reason.ToString(), EventResult.ActualAmount);
}
