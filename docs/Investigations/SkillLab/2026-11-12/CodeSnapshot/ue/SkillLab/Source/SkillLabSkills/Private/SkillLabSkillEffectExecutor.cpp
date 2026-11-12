#include "SkillLabSkillEffectExecutor.h"

#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "GameFramework/Actor.h"
#include "SkillLabEffectResolver.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillEffectEventAdapter.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "Misc/ScopeExit.h"

void USkillLabSkillEffectExecutor::Initialize(UObject* InContextOwner, ISkillLabSkillCastContext* InContext,
	USkillLabSkillEffectEventAdapter* InEffectEventAdapter)
{
	ContextOwner = InContextOwner;
	Context = InContext;
	EffectEventAdapter = InEffectEventAdapter;
}

void USkillLabSkillEffectExecutor::Shutdown()
{
	ContextOwner.Reset();
	Context = nullptr;
	EffectEventAdapter.Reset();
}

FSkillLabSkillResult USkillLabSkillEffectExecutor::Apply(const USkillLabSkillBehavior* Behavior, const int32 CastId,
 AActor* Target, const FSkillLabSkillDefinition& Definition, FSkillLabSkillResult Result, const FSkillLabProductionHandle& Production) const
{
 if (!ContextOwner.IsValid() || !Context || !Context->IsBehaviorCurrent(Behavior, CastId))
 { Result.Reason = TEXT("CastNotActive"); return Result; }
 auto* Runtime = Cast<USkillLabSkillRuntimeComponent>(ContextOwner.Get());
 return ApplySelected(Context->GetSourceAbilitySystemComponent(), Context->GetSourceActor(), Runtime ? Runtime->TeamId : 0,
  Target, Definition, Result, Production, EffectEventAdapter.Get());
}

FSkillLabSkillResult USkillLabSkillEffectExecutor::ApplySelected(USkillLabAbilitySystemComponent* SourceASC, AActor* Source,
 int32 SourceTeam, AActor* Target, const FSkillLabSkillDefinition& Definition, FSkillLabSkillResult Result,
 const FSkillLabProductionHandle& Production, USkillLabSkillEffectEventAdapter* Observer)
{
	bool bEnteredResolver = false;
	const auto RecordReturned = [Production](const bool bAccepted, const FName Reason)
	{
		if (Production.Owner.IsValid() && Production.Owner->CanAcceptConfirmedCooldown(
			Production.Owner->ResourceOwner.Get()))
		{
			// 同步 overlap 仍可处在创建提交栈内；命中 GE 不能改写投射物交付记录。
			const auto Record = Production.Owner->ProductionResults.Query(Production.Key);
			if (Record.Key.IsSet() && Record.Kind == ESkillLabProductionKind::Effect)
				Production.Owner->ProductionResults.Confirm(Production.Key, bAccepted, Reason);
		}
	};
	ON_SCOPE_EXIT { if (!bEnteredResolver) RecordReturned(false, Result.Reason); };
	Result.SkillId = Definition.SkillId;
	Result.TargetActor = Target;
	Result.Effect = Definition.Effect;

	if (!IsValid(Source) || Source->IsActorBeingDestroyed() || !IsValid(SourceASC) || SourceASC->IsBeingDestroyed())
	{ Result.Reason = TEXT("SourceUnavailable"); return Result; }
	USkillLabSkillRuntimeComponent* TargetRuntime = Target ? Target->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	USkillLabAbilitySystemComponent* TargetASC = Target ? Target->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!TargetRuntime || !TargetASC || !TargetRuntime->IsAlive())
	{
		Result.Reason = TEXT("HitTargetInvalid");
		return Result;
	}
	if (!((Definition.Relation == ESkillLabTargetRelation::Any)
		|| (Definition.Relation == ESkillLabTargetRelation::Self && Target == Source)
		|| (Definition.Relation == ESkillLabTargetRelation::Enemy && Target != Source && TargetRuntime->TeamId != SourceTeam)
		|| (Definition.Relation == ESkillLabTargetRelation::Ally && (Target == Source || TargetRuntime->TeamId == SourceTeam))))
	{
		Result.Reason = TEXT("TargetRelationRejected");
		return Result;
	}

	const USkillLabCombatAttributeSet* Before = TargetASC->GetCombatAttributeSet();
	Result.TargetUnitId = TargetRuntime->GetUnitId();
	Result.HealthBefore = Before ? Before->GetHealth() : 0.0f;
	Result.RequestedAmount = Definition.Magnitude;
	FSkillLabEffectRequest EffectRequest;
	EffectRequest.SourceASC = SourceASC;
	EffectRequest.TargetASC = TargetASC;
	EffectRequest.Magnitude = Definition.Magnitude;
	EffectRequest.PowerCoefficient = Definition.PowerCoefficient;
	EffectRequest.Duration = Definition.Duration;
	EffectRequest.Period = Definition.Period;
	EffectRequest.SourceObject = Source;
	EffectRequest.Operation = Definition.Effect == ESkillLabEffectKind::Healing ? ESkillLabEffectOperation::Healing
		: (Definition.Effect == ESkillLabEffectKind::DamageOverTime ? ESkillLabEffectOperation::DamageOverTime : ESkillLabEffectOperation::Damage);
	EffectRequest.OnApplicationReturned = RecordReturned;
	bEnteredResolver = true;
	const FSkillLabEffectResult EffectResult = USkillLabEffectResolver::Apply(EffectRequest);
	Result.bSuccess = EffectResult.bApplied;
	Result.bEffectApplied = EffectResult.bApplied;
	Result.ActualAmount = EffectResult.ActualAmount;
	Result.HealthAfter = EffectResult.HealthAfter;
	Result.Reason = EffectResult.Reason;
	if (Result.bSuccess && Definition.Effect == ESkillLabEffectKind::DamageOverTime
		&& EffectResult.ActiveEffectHandle.IsValid() && IsValid(Observer))
	{
		Result.EffectInstanceId = FName(*EffectResult.ActiveEffectHandle.ToString());
		Result.EffectDuration = Definition.Duration;
		Observer->Track(TargetASC, EffectResult.ActiveEffectHandle, Result);
	}
	return Result;
}
