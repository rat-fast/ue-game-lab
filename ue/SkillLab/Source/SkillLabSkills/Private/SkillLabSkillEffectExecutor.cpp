#include "SkillLabSkillEffectExecutor.h"

#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabEffectResolver.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillEffectEventAdapter.h"
#include "SkillLabSkillRuntimeComponent.h"

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
	AActor* Target, const FSkillLabSkillDefinition& Definition, FSkillLabSkillResult Result) const
{
	Result.SkillId = Definition.SkillId;
	Result.CastId = CastId;
	Result.TargetActor = Target;
	Result.Effect = Definition.Effect;
	if (!ContextOwner.IsValid() || !Context || !Context->IsBehaviorCurrent(Behavior, CastId))
	{
		Result.Reason = TEXT("CastNotActive");
		return Result;
	}

	USkillLabSkillRuntimeComponent* TargetRuntime = Target ? Target->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	USkillLabAbilitySystemComponent* TargetASC = Target ? Target->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!TargetRuntime || !TargetASC || !TargetRuntime->IsAlive())
	{
		Result.Reason = TEXT("HitTargetInvalid");
		return Result;
	}
	if (!Context->MatchesTargetRelation(TargetRuntime, Definition.Relation))
	{
		Result.Reason = TEXT("TargetRelationRejected");
		return Result;
	}

	const USkillLabCombatAttributeSet* Before = TargetASC->GetCombatAttributeSet();
	Result.TargetUnitId = TargetRuntime->GetUnitId();
	Result.HealthBefore = Before ? Before->GetHealth() : 0.0f;
	Result.RequestedAmount = Definition.Magnitude;
	FSkillLabEffectRequest EffectRequest;
	EffectRequest.SourceASC = Context->GetSourceAbilitySystemComponent();
	EffectRequest.TargetASC = TargetASC;
	EffectRequest.Magnitude = Definition.Magnitude;
	EffectRequest.PowerCoefficient = Definition.PowerCoefficient;
	EffectRequest.Duration = Definition.Duration;
	EffectRequest.Period = Definition.Period;
	EffectRequest.SourceObject = Context->GetSourceActor();
	EffectRequest.Operation = Definition.Effect == ESkillLabEffectKind::Healing ? ESkillLabEffectOperation::Healing
		: (Definition.Effect == ESkillLabEffectKind::DamageOverTime ? ESkillLabEffectOperation::DamageOverTime : ESkillLabEffectOperation::Damage);
	const FSkillLabEffectResult EffectResult = USkillLabEffectResolver::Apply(EffectRequest);
	Result.bSuccess = EffectResult.bApplied;
	Result.bEffectApplied = EffectResult.bApplied;
	Result.ActualAmount = EffectResult.ActualAmount;
	Result.HealthAfter = EffectResult.HealthAfter;
	Result.Reason = EffectResult.Reason;
	if (Result.bSuccess && Definition.Effect == ESkillLabEffectKind::DamageOverTime
		&& EffectResult.ActiveEffectHandle.IsValid() && EffectEventAdapter.IsValid())
	{
		Result.EffectInstanceId = FName(*EffectResult.ActiveEffectHandle.ToString());
		Result.EffectDuration = Definition.Duration;
		EffectEventAdapter->Track(TargetASC, EffectResult.ActiveEffectHandle, Result);
	}
	return Result;
}
