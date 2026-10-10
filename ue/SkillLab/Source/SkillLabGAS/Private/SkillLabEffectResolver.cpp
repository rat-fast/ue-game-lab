#include "SkillLabEffectResolver.h"
#include "SkillLabCombatAttributeSet.h"
#include "GameFramework/Actor.h"

FSkillLabEffectResult USkillLabEffectResolver::Apply(const FSkillLabEffectRequest& Request)
{
	FSkillLabEffectResult Result;
	Result.RequestedAmount = Request.Magnitude;
	if (!IsValid(Request.SourceASC) || !IsValid(Request.TargetASC) || !FMath::IsFinite(Request.Magnitude)
		|| !FMath::IsFinite(Request.PowerCoefficient) || Request.Magnitude <= 0.0f)
	{
		Result.Reason = TEXT("EffectRequestInvalid");
		return Result;
	}
	Result.TargetActor = Request.TargetASC->GetAvatarActor();
	if (!Result.TargetActor) Result.TargetActor = Request.TargetASC->GetOwner();
	Result.TargetLocation = Result.TargetActor ? Result.TargetActor->GetActorLocation() : FVector::ZeroVector;
	const USkillLabCombatAttributeSet* Attributes = Request.TargetASC->GetCombatAttributeSet();
	if (!Attributes)
	{
		Result.Reason = TEXT("EffectTargetMissingAttributes");
		return Result;
	}
	Result.HealthBefore = Attributes->GetHealth();
	FSkillLabDamageRequest DamageRequest;
	DamageRequest.SourceASC = Request.SourceASC;
	DamageRequest.TargetASC = Request.TargetASC;
	DamageRequest.BaseDamage = Request.Magnitude;
	DamageRequest.PowerCoefficient = Request.PowerCoefficient;
	DamageRequest.DamageType = Request.DamageType;
	DamageRequest.SourceObject = Request.SourceObject;
	switch (Request.Operation)
	{
	case ESkillLabEffectOperation::Healing:
		Result.bApplied = Request.SourceASC->ApplyHealingToTarget(Request.TargetASC, Request.Magnitude, Request.SourceObject);
		break;
	case ESkillLabEffectOperation::DamageOverTime:
	{
		if (!FMath::IsFinite(Request.Duration) || !FMath::IsFinite(Request.Period) || Request.Duration <= 0.0f || Request.Period <= 0.0f)
		{
			Result.Reason = TEXT("EffectDurationInvalid");
			break;
		}
		const FSkillLabDamageResult DamageResult = Request.SourceASC->ApplyDamageOverTimeToTarget(DamageRequest, Request.Duration, Request.Period);
		Result.ActiveEffectHandle = DamageResult.ActiveEffectHandle;
		Result.bApplied = DamageResult.bApplied;
		Result.bDurationEffect = Result.bApplied;
		break;
	}
	case ESkillLabEffectOperation::Damage:
		Result.bApplied = Request.SourceASC->ResolveDamage(DamageRequest).bApplied;
		break;
	default:
		Result.Reason = TEXT("EffectOperationUnsupported");
		break;
	}
	Result.HealthAfter = Attributes->GetHealth();
	Result.ActualAmount = FMath::Max(0.0f, Request.Operation == ESkillLabEffectOperation::Healing
		? Result.HealthAfter - Result.HealthBefore : Result.HealthBefore - Result.HealthAfter);
	if (!Result.bApplied && Result.Reason.IsNone()) Result.Reason = TEXT("GameplayEffectFailed");
	return Result;
}
