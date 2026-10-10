#include "SkillLabAbilitySystemComponent.h"

#include "GameplayEffect.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabCombatGameplayEffects.h"
#include "SkillLabGASTags.h"

USkillLabAbilitySystemComponent::USkillLabAbilitySystemComponent()
{
	SetIsReplicatedByDefault(true);
	OnPeriodicGameplayEffectExecuteDelegateOnSelf.AddUObject(this, &USkillLabAbilitySystemComponent::HandlePeriodicExecution);
}

void USkillLabAbilitySystemComponent::RecordPeriodicDamage(const FGameplayEffectSpec& Spec, const FSkillLabDamageResult& Result)
{
	if (Spec.GetPeriod() <= 0.0f) return;
	if (FSkillLabDamageResult* Previous = PendingPeriodicDamage.Find(&Spec))
	{
		Previous->HealthAfter = Result.HealthAfter;
		Previous->RawDamage += Result.RawDamage;
		Previous->MitigatedDamage += Result.MitigatedDamage;
		Previous->AppliedDamage += Result.AppliedDamage;
	}
	else PendingPeriodicDamage.Add(&Spec, Result);
}

void USkillLabAbilitySystemComponent::HandlePeriodicExecution(UAbilitySystemComponent* Source,
	const FGameplayEffectSpec& Spec, const FActiveGameplayEffectHandle Handle)
{
	(void)Source;
	FSkillLabDamageResult Result;
	// UE executes ActiveEffect.Spec and then sends the same spec through this native callback.
	// Remove before broadcasting so nested effects cannot overwrite this execution's result.
	if (!PendingPeriodicDamage.RemoveAndCopyValue(&Spec, Result))
	{
		// The engine normally forwards the same spec reference. A few UE paths
		// materialize a transient copy; with one modifier/effect in this module,
		// the single pending result is still unambiguous.
		if (PendingPeriodicDamage.Num() != 1) return;
		for (const TPair<const FGameplayEffectSpec*, FSkillLabDamageResult>& Pair : PendingPeriodicDamage)
		{
			Result = Pair.Value;
			break;
		}
		PendingPeriodicDamage.Reset();
	}
	Result.ActiveEffectHandle = Handle;
	OnPeriodicDamageResolved.Broadcast(Handle, Result);
}

void USkillLabAbilitySystemComponent::InitializeSkillLabAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (InOwnerActor || InAvatarActor)
	{
		InitAbilityActorInfo(InOwnerActor, InAvatarActor ? InAvatarActor : InOwnerActor);
	}

	if (!CombatAttributeSet)
	{
		CombatAttributeSet = const_cast<USkillLabCombatAttributeSet*>(GetSet<USkillLabCombatAttributeSet>());
	}
	if (!CombatAttributeSet)
	{
		CombatAttributeSet = NewObject<USkillLabCombatAttributeSet>(GetOwner(), TEXT("SkillLabCombatAttributes"));
		if (CombatAttributeSet)
		{
			AddAttributeSetSubobject(CombatAttributeSet.Get());
		}
	}
}

USkillLabCombatAttributeSet* USkillLabAbilitySystemComponent::GetCombatAttributeSet() const
{
	return CombatAttributeSet ? CombatAttributeSet.Get() : const_cast<USkillLabCombatAttributeSet*>(GetSet<USkillLabCombatAttributeSet>());
}

bool USkillLabAbilitySystemComponent::ApplyDamageToTarget(UAbilitySystemComponent* TargetASC, const float Amount, UObject* SourceObject)
{
	return ApplyMetaEffect(TargetASC, USkillLabDamageGameplayEffect::StaticClass(), Amount, SourceObject);
}

FSkillLabDamageResult USkillLabAbilitySystemComponent::ResolveDamage(const FSkillLabDamageRequest& Request)
{
	FSkillLabDamageResult Result;
	UAbilitySystemComponent* Target = Request.TargetASC;
	const USkillLabCombatAttributeSet* TargetAttributes = Target ? Target->GetSet<USkillLabCombatAttributeSet>() : nullptr;
	const USkillLabCombatAttributeSet* SourceAttributes = Request.SourceASC ? Request.SourceASC->GetSet<USkillLabCombatAttributeSet>() : nullptr;
	if (!Target || !TargetAttributes || Request.BaseDamage <= 0.0f)
	{
		return Result;
	}
	Result.HealthBefore = TargetAttributes->GetHealth();
	Result.RawDamage = FMath::Max(0.0f, Request.BaseDamage + (SourceAttributes ? SourceAttributes->GetPower() * Request.PowerCoefficient : 0.0f));
	// The AttributeSet is the single authority for mitigation. Pass raw damage once;
	// the result below reports the amount that survived armor and health clamping.
	Result.bApplied = ApplyDamageToTarget(Target, Result.RawDamage, Request.SourceObject);
	const USkillLabCombatAttributeSet* AfterAttributes = Target->GetSet<USkillLabCombatAttributeSet>();
	Result.HealthAfter = AfterAttributes ? AfterAttributes->GetHealth() : Result.HealthBefore;
	Result.AppliedDamage = FMath::Max(0.0f, Result.HealthBefore - Result.HealthAfter);
	Result.MitigatedDamage = Result.AppliedDamage;
	return Result;
}

bool USkillLabAbilitySystemComponent::ApplyHealingToTarget(UAbilitySystemComponent* TargetASC, const float Amount, UObject* SourceObject)
{
	return ApplyMetaEffect(TargetASC, USkillLabHealingGameplayEffect::StaticClass(), Amount, SourceObject);
}

bool USkillLabAbilitySystemComponent::ApplyDamageOverTimeToTarget(UAbilitySystemComponent* TargetASC, const float AmountPerTick, const float Duration, const float Period, UObject* SourceObject)
{
	FSkillLabDamageRequest Request;
	Request.SourceASC = this;
	Request.TargetASC = TargetASC;
	Request.BaseDamage = AmountPerTick;
	Request.SourceObject = SourceObject;
	return ApplyDamageOverTimeToTarget(Request, Duration, Period).bApplied;
}

FSkillLabDamageResult USkillLabAbilitySystemComponent::ApplyDamageOverTimeToTarget(const FSkillLabDamageRequest& Request, const float Duration, const float Period)
{
	FSkillLabDamageResult Result;
	UAbilitySystemComponent* TargetASC = Request.TargetASC;
	const USkillLabCombatAttributeSet* TargetAttributes = TargetASC ? TargetASC->GetSet<USkillLabCombatAttributeSet>() : nullptr;
	const USkillLabCombatAttributeSet* SourceAttributes = Request.SourceASC ? Request.SourceASC->GetSet<USkillLabCombatAttributeSet>() : nullptr;
	if (!TargetASC || !TargetAttributes || Request.BaseDamage <= 0.0f || Duration <= 0.0f || Period <= 0.0f)
	{
		return Result;
	}

	UGameplayEffect* Effect = NewObject<UGameplayEffect>(this);
	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Effect->Period = Period;
	Effect->bExecutePeriodicEffectOnApplication = true;
	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = USkillLabCombatAttributeSet::GetIncomingDamageAttribute();
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataTag = SkillLabGASTags::Data_Magnitude;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	Effect->Modifiers.Add(DamageModifier);
	Effect->StackingType = EGameplayEffectStackingType::AggregateByTarget;
	Effect->StackLimitCount = 1;
	Effect->StackDurationRefreshPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
	Effect->Period = Period;
	RuntimeEffects.Add(Effect);

	FGameplayEffectContextHandle Context = MakeEffectContext();
	if (Request.SourceObject) Context.AddSourceObject(Request.SourceObject);
	FGameplayEffectSpecHandle SpecHandle(new FGameplayEffectSpec(Effect, Context, 1.0f));
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid()) return Result;
	SpecHandle.Data->SetDuration(Duration, true);
	const float RawDamage = FMath::Max(0.0f, Request.BaseDamage + (SourceAttributes ? SourceAttributes->GetPower() * Request.PowerCoefficient : 0.0f));
	SpecHandle.Data->SetSetByCallerMagnitude(SkillLabGASTags::Data_Magnitude, RawDamage);
	Result.HealthBefore = TargetAttributes->GetHealth();
	Result.RawDamage = RawDamage;
	// Duration/periodic effects return a live valid handle. Use the semantic
	// success flag so this remains correct if the GE is changed to instant.
	const FActiveGameplayEffectHandle AppliedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	Result.ActiveEffectHandle = AppliedHandle;
	Result.bApplied = AppliedHandle.WasSuccessfullyApplied();
	Result.HealthAfter = TargetAttributes->GetHealth();
	Result.AppliedDamage = FMath::Max(0.0f, Result.HealthBefore - Result.HealthAfter);
	Result.MitigatedDamage = Result.AppliedDamage;
	return Result;
}

bool USkillLabAbilitySystemComponent::HasAnySkillLabBlockTag(const FGameplayTagContainer& Tags) const
{
	return HasAnyMatchingGameplayTags(Tags);
}

bool USkillLabAbilitySystemComponent::ApplyMetaEffect(UAbilitySystemComponent* TargetASC, const TSubclassOf<UGameplayEffect> EffectClass, const float Amount, UObject* SourceObject)
{
	if (!TargetASC || !EffectClass || Amount <= 0.0f || !TargetASC->GetSet<USkillLabCombatAttributeSet>())
	{
		return false;
	}

	FGameplayEffectContextHandle Context = MakeEffectContext();
	if (SourceObject)
	{
		Context.AddSourceObject(SourceObject);
	}
	const FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(EffectClass, 1.0f, Context);
	if (!SpecHandle.IsValid())
	{
		return false;
	}
	SpecHandle.Data->SetSetByCallerMagnitude(SkillLabGASTags::Data_Magnitude, Amount);
	const FActiveGameplayEffectHandle AppliedHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	// Instant GEs intentionally return an invalid sentinel handle in UE 5.8;
	// WasSuccessfullyApplied() is the authoritative result for both instant and duration GEs.
	return AppliedHandle.WasSuccessfullyApplied();
}
