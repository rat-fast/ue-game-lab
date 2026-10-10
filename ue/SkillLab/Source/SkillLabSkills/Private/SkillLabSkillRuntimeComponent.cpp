#include "SkillLabSkillRuntimeComponent.h"

#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabEffectResolver.h"
#include "SkillLabGASTags.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillEffectEventAdapter.h"
#include "SkillLabSkillPresentationRouter.h"
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
	if (EffectEventAdapter) EffectEventAdapter->Clear(true);
	if (ActiveCastId != 0)
	{
		const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
		if (Lifecycle.Cancel(Token, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0))
		{
			FinishCast(true, TEXT("EndPlay"));
		}
	}
	ClearInterruptListeners();
	if (PresentationRouter)
	{
		PresentationRouter->Bind(nullptr);
	}
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
	if (!ASC)
	{
		return;
	}
	ASC->InitializeSkillLabAbilityActorInfo(Owner, Owner);
	PresentationRouter = NewObject<USkillLabSkillPresentationRouter>(this);
	if (PresentationRouter)
	{
		PresentationRouter->Bind(this);
	}
	EffectEventAdapter = NewObject<USkillLabSkillEffectEventAdapter>(this);
	if (EffectEventAdapter)
	{
		EffectEventAdapter->OnEffectEvent.AddUObject(this, &USkillLabSkillRuntimeComponent::ForwardEffectEvent);
	}
	UnitId = Owner ? Owner->GetUniqueID() : 0;

	TArray<FName> SkillIds;
	FSkillLabSkillDefinitionProvider::GetDefaultSkillIds(SkillIds);
	for (const FName SkillId : SkillIds)
	{
		FSkillLabSkillDefinition Definition;
		if (FSkillLabSkillDefinitionProvider::Resolve(SkillId, Definition) && Definition.AbilityClass)
		{
			GrantedSkills.Add(SkillId, ASC->GiveAbility(FGameplayAbilitySpec(Definition.AbilityClass, 1)));
		}
	}
	bInitialized = GrantedSkills.Num() > 0;
	if (bInitialized)
	{
		FSkillLabSkillResult Result = MakeResult();
		Result.bSuccess = true;
		Emit(ESkillLabSkillEvent::Completed, Result, TEXT("Initialized"));
	}
}

bool USkillLabSkillRuntimeComponent::IsAlive() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
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
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetEnergy() : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetMaxEnergyValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetMaxEnergy() : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetHealthValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetHealth() : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetMaxHealthValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
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
	GrantedSkills.GetKeys(OutIds);
	OutIds.Sort(FNameLexicalLess());
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
	if (!bInitialized || !ASC)
	{
		OutReason = TEXT("NotInitialized");
	}
	else if (!IsAlive() || ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead))
	{
		OutReason = TEXT("CasterDead");
	}
	else if (ASC->HasAnySkillLabBlockTag(ActiveDefinition.BlockedByTags))
	{
		OutReason = TEXT("BlockedByState");
	}
	return OutReason.IsNone();
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillRequest(const FSkillLabSkillRequest& InRequest)
{
	FSkillLabSkillRequest Request = InRequest;
	Request.RequestId = Request.RequestId > 0 ? Request.RequestId : NextRequestId++;
	FSkillLabSkillDefinition Definition;
	FName Reason;
	if (bShuttingDown || !GetWorld() || !bInitialized || !ASC)
	{
		return Reject(Request, TEXT("NotInitialized"));
	}
	if (bFinishing || ActiveCastId != 0)
	{
		return Reject(Request, TEXT("SkillBusy"));
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
	if (!GrantedSkills.Contains(Request.SkillId))
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

	ActiveRequest = Request;
	ActiveCastId = NextCastId++;
	bCommitted = false;
	bFinishing = false;
	OutcomeReason = NAME_None;
	FSkillLabCastLifecycle::FConfig LifecycleConfig;
	LifecycleConfig.WindupSeconds = Definition.Phases.WindupSeconds;
	LifecycleConfig.ActiveSeconds = Definition.Phases.ActiveSeconds;
	LifecycleConfig.RecoverySeconds = Definition.Phases.RecoverySeconds;
	LifecycleConfig.bUseActiveTimeout = Definition.Phases.ActiveSeconds > 0.0f;
	if (!Lifecycle.Begin(static_cast<uint64>(ActiveCastId), LifecycleConfig, Now))
	{
		ActiveCastId = 0;
		ActiveRequest = FSkillLabSkillRequest();
		ActiveDefinition = FSkillLabSkillDefinition();
		return Reject(Request, TEXT("LifecycleStartFailed"));
	}
	if (!ASC->TryActivateAbility(GrantedSkills[Request.SkillId]) || !ActiveAbility)
	{
		const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
		Lifecycle.Cancel(Token, Now);
		FinishCast(true, TEXT("ActivationFailed"));
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
			ReleaseDelegate.BindLambda([WeakThis = TWeakObjectPtr<USkillLabSkillRuntimeComponent>(this), WindupToken]()
			{
				if (WeakThis.IsValid()) WeakThis->ReleaseCast(WindupToken);
			});
			GetWorld()->GetTimerManager().SetTimer(PhaseTimer, ReleaseDelegate, Definition.Phases.WindupSeconds, false);
		}
		else ReleaseCast(WindupToken);
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

void USkillLabSkillRuntimeComponent::ReceiveCastEvent(const int32 CastId, const FGameplayTag EventTag)
{
	if (bShuttingDown || bFinishing || CastId <= 0 || ActiveCastId != CastId || !EventTag.IsValid()) return;
	if (Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Windup
		&& ActiveDefinition.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
		&& EventTag == ActiveDefinition.ReleaseEventTag)
		ReleaseCast(Lifecycle.GetPhaseToken());
	else if (Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active && ActiveBehavior
		&& ActiveDefinition.BehaviorEventTags.HasTag(EventTag))
		ActiveBehavior->ReceiveEvent(EventTag);
}

void USkillLabSkillRuntimeComponent::ReleaseCast(const FSkillLabCastPhaseToken Token)
{
	if (bShuttingDown || bFinishing || bCommitted || ActiveCastId == 0 || !Lifecycle.IsTokenCurrent(Token)
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
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	if (!Attributes || Attributes->GetEnergy() < ActiveDefinition.Cost || !Lifecycle.TryMarkRelease(ActiveToken))
	{
		CancelActiveCast(TEXT("ReleaseGateFailed"));
		return;
	}
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
			ActiveBehavior->Initialize(this, ActiveDefinition, ActiveRequest, ActiveCastId);
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
	if (ActiveAbility)
	{
		USkillLabSkillGameplayAbility* Ability = ActiveAbility;
		ActiveAbility = nullptr;
		Ability->FinishSkillAbility(bCancelled);
	}
	FSkillLabSkillResult Result = MakeResult();
	Result.bSuccess = !bCancelled;
	Result.Stage = bCancelled ? ESkillLabCastStage::Cancelled : ESkillLabCastStage::Completed;
	Emit(bCancelled ? ESkillLabSkillEvent::Cancelled : ESkillLabSkillEvent::Completed, Result, Reason);
	ActiveBehavior = nullptr;
	ActiveRequest = FSkillLabSkillRequest();
	ActiveDefinition = FSkillLabSkillDefinition();
	ActiveCastId = 0;
	StageEndTime = 0.0;
	bCommitted = false;
	bFinishing = false;
}

void USkillLabSkillRuntimeComponent::NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability)
{
	if (bShuttingDown || ActiveCastId == 0 || ActiveAbility)
	{
		return;
	}
	ActiveAbility = Ability;
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
		&& Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active;
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::ApplyBehaviorEffect(const USkillLabSkillBehavior* Behavior,
	const int32 CastId, AActor* Target, const FSkillLabSkillDefinition& Definition)
{
	FSkillLabSkillResult Result = MakeResult();
	Result.SkillId = Definition.SkillId;
	Result.CastId = CastId;
	Result.TargetActor = Target;
	Result.Effect = Definition.Effect;
	if (!IsBehaviorCurrent(Behavior, CastId))
	{
		Result.Reason = TEXT("CastNotActive");
		return Result;
	}
	USkillLabSkillRuntimeComponent* TargetRuntime = Target ? Target->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	USkillLabAbilitySystemComponent* TargetASC = Target ? Target->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!TargetRuntime || !TargetASC || !TargetRuntime->IsAlive())
	{
		Result.Reason = TEXT("HitTargetInvalid");
		Emit(ESkillLabSkillEvent::Hit, Result, Result.Reason);
		return Result;
	}
	if (!FSkillLabTargetingService::MatchesRelation(*this, *TargetRuntime, Definition.Relation))
	{
		Result.Reason = TEXT("TargetRelationRejected");
		Emit(ESkillLabSkillEvent::Hit, Result, Result.Reason);
		return Result;
	}
	const USkillLabCombatAttributeSet* Before = TargetASC->GetCombatAttributeSet();
	Result.TargetUnitId = TargetRuntime->GetUnitId();
	Result.HealthBefore = Before ? Before->GetHealth() : 0.0f;
	Result.RequestedAmount = Definition.Magnitude;
	FSkillLabEffectRequest EffectRequest;
	EffectRequest.SourceASC = ASC;
	EffectRequest.TargetASC = TargetASC;
	EffectRequest.Magnitude = Definition.Magnitude;
	EffectRequest.PowerCoefficient = Definition.PowerCoefficient;
	EffectRequest.Duration = Definition.Duration;
	EffectRequest.Period = Definition.Period;
	EffectRequest.SourceObject = GetOwner();
	EffectRequest.Operation = Definition.Effect == ESkillLabEffectKind::Healing ? ESkillLabEffectOperation::Healing
		: (Definition.Effect == ESkillLabEffectKind::DamageOverTime ? ESkillLabEffectOperation::DamageOverTime : ESkillLabEffectOperation::Damage);
	const FSkillLabEffectResult EffectResult = USkillLabEffectResolver::Apply(EffectRequest);
	Result.bSuccess = EffectResult.bApplied;
	Result.ActualAmount = EffectResult.ActualAmount;
	Result.HealthAfter = EffectResult.HealthAfter;
	Result.Reason = EffectResult.Reason;
	if (!Result.bSuccess)
	{
		Emit(ESkillLabSkillEvent::Hit, Result, Result.Reason);
		return Result;
	}
	if (Definition.Effect == ESkillLabEffectKind::DamageOverTime && EffectResult.ActiveEffectHandle.IsValid() && EffectEventAdapter)
	{
		Result.EffectInstanceId = FName(*EffectResult.ActiveEffectHandle.ToString());
		Result.EffectDuration = Definition.Duration;
		EffectEventAdapter->Track(TargetASC, EffectResult.ActiveEffectHandle, Result);
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
	if (!ASC)
	{
		return;
	}
	TSet<FGameplayTag> RegisteredTags;
	RegisteredTags.Add(SkillLabGASTags::State_Dead);
	FDelegateHandle DeathHandle = ASC->RegisterGameplayTagEvent(SkillLabGASTags::State_Dead, EGameplayTagEventType::AnyCountChange)
		.AddUObject(this, &USkillLabSkillRuntimeComponent::OnBlockingTagChanged);
	InterruptHandles.Add(SkillLabGASTags::State_Dead, DeathHandle);
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
	if (ASC)
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
	if (Count > 0 && ActiveCastId != 0 && (Tag == SkillLabGASTags::State_Dead || ActiveDefinition.BlockedByTags.HasTag(Tag)))
	{
		CancelActiveCast(Tag == SkillLabGASTags::State_Dead ? TEXT("CasterDead") : TEXT("BlockedByState"));
	}
}

void USkillLabSkillRuntimeComponent::ResetSkills()
{
	CancelActiveCast(TEXT("Reset"));
	if (EffectEventAdapter) EffectEventAdapter->Clear(true);
	CooldownEnd.Reset();
	if (ASC && ASC->GetCombatAttributeSet())
	{
		const USkillLabCombatAttributeSet* Attributes = ASC->GetCombatAttributeSet();
		ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), Attributes->GetMaxHealth());
		ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), Attributes->GetMaxEnergy());
	}
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
