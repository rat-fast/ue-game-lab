#include "SkillLabRecoverEnergyAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabAttributeSet.h"
#include "SkillLabRecoverEnergyEffect.h"
#include "SkillLabSkillComponent.h"

bool USkillLabRecoverEnergyAbility::ApplyEffect(
	const FSkillLabSkillDefinition& Definition,
	const FSkillLabSkillRequest& Request,
	const int32 CastId,
	const int32 GrantedAbilityId,
	FSkillLabEffectApplicationResult& OutResult)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();
	USkillLabAbilitySystemComponent* SourceASC = ActorInfo ? Cast<USkillLabAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	USkillLabSkillComponent* SourceComponent = SourceASC && SourceASC->GetOwner()
		? SourceASC->GetOwner()->FindComponentByClass<USkillLabSkillComponent>()
		: nullptr;
	AActor* TargetActor = Request.TargetObject ? Request.TargetObject.Get() : (Request.TargetType == ESkillLabTargetType::Self ? SourceASC ? SourceASC->GetOwner() : nullptr : nullptr);
	UAbilitySystemComponent* TargetASC = TargetActor ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor) : nullptr;

	OutResult.EffectApplicationId = SourceComponent ? SourceComponent->AllocateEffectApplicationId() : 0;
	OutResult.RequestId = Request.RequestId;
	OutResult.CastId = CastId;
	OutResult.GrantedAbilityId = GrantedAbilityId;
	OutResult.SourceUnitId = SourceComponent ? SourceComponent->GetUnitId() : 0;
	OutResult.SourceObject = SourceASC ? SourceASC->GetOwner() : nullptr;
	OutResult.TargetObject = TargetActor;
	OutResult.TargetUnitId = Request.TargetUnitId;
	OutResult.TargetLocation = Request.TargetLocation;
	OutResult.TargetDirection = Request.TargetDirection;
	OutResult.SkillId = Definition.SkillId;
	OutResult.EffectType = Definition.EffectType;
	OutResult.Magnitude = Definition.EffectMagnitude;
	OutResult.WorldTime = SourceASC && SourceASC->GetWorld() ? SourceASC->GetWorld()->GetTimeSeconds() : 0.0f;

	if (!SourceASC || !TargetASC || !Definition.EffectClass)
	{
		OutResult.Status = TargetASC ? ESkillLabEffectResultStatus::Failed : ESkillLabEffectResultStatus::InvalidTarget;
		OutResult.Reason = TEXT("EffectContextUnavailable");
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return false;
	}

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(this);
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(Definition.EffectClass, 1.0f, EffectContext);
	if (!SpecHandle.IsValid())
	{
		OutResult.Status = ESkillLabEffectResultStatus::Failed;
		OutResult.Reason = TEXT("EffectSpecCreationFailed");
		EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FName(TEXT("SkillLab.EffectMagnitude")), Definition.EffectMagnitude);
	const FGameplayAttribute EffectAttribute = USkillLabAttributeSet::GetSkillEnergyAttribute();
	const float ValueBefore = TargetASC->GetNumericAttribute(EffectAttribute);
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	const float ValueAfter = TargetASC->GetNumericAttribute(EffectAttribute);
	OutResult.AppliedMagnitude = ValueAfter - ValueBefore;
	const bool bApplied = !FMath::IsNearlyZero(OutResult.AppliedMagnitude);
	OutResult.Status = bApplied ? ESkillLabEffectResultStatus::Applied : ESkillLabEffectResultStatus::Failed;
	OutResult.Reason = bApplied ? FString() : TEXT("EffectApplicationFailed");

	EndAbility(GetCurrentAbilitySpecHandle(), ActorInfo, GetCurrentActivationInfo(), true, false);
	return bApplied;
}
