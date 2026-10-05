#include "SkillLabSkillComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabAttributeSet.h"
#include "SkillLabPulseAbility.h"

namespace
{
	const FName TestPulseSkillId(TEXT("Skill_TestPulse"));

	const TCHAR* RequestSourceToString(const ESkillLabRequestSource Source)
	{
		switch (Source)
		{
		case ESkillLabRequestSource::Player: return TEXT("Player");
		case ESkillLabRequestSource::AI: return TEXT("AI");
		default: return TEXT("Test");
		}
	}

	const TCHAR* TargetTypeToString(const ESkillLabTargetType TargetType)
	{
		return TargetType == ESkillLabTargetType::Unit ? TEXT("Unit") : TEXT("Self");
	}
}

USkillLabSkillComponent::USkillLabSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeUnit();
}

void USkillLabSkillComponent::InitializeUnit()
{
	if (bInitialized)
	{
		LogEvent(TEXT("InitializeSkipped"), TEXT("AlreadyInitialized"));
		return;
	}

	AActor* OwnerActor = GetOwner();
	AbilitySystemComponent = OwnerActor ? OwnerActor->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!AbilitySystemComponent)
	{
		LogEvent(TEXT("InitializeFailed"), TEXT("MissingASC"));
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, OwnerActor);
	AttributeSet = const_cast<USkillLabAttributeSet*>(AbilitySystemComponent->GetSet<USkillLabAttributeSet>());
	if (!AttributeSet)
	{
		LogEvent(TEXT("InitializeFailed"), TEXT("MissingAttributeSet"));
		return;
	}

	if (UnitId == 0)
	{
		UnitId = GetUniqueID();
	}
	AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetTestValueAttribute(), InitialTestValue);
	AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetSkillEnergyAttribute(), InitialEnergy);

	if (!bSkillGranted)
	{
		AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(USkillLabPulseAbility::StaticClass(), 1));
		bSkillGranted = true;
		LogEvent(TEXT("Granted"));
	}

	bInitialized = true;
	LogEvent(TEXT("Initialized"));
}

FSkillLabRequestResult USkillLabSkillComponent::RequestTestPulse(AActor* Target)
{
	FSkillLabRequestResult Result;
	Result.RequestId = NextRequestId++;
	Result.UnitId = UnitId;
	Result.SkillId = TestPulseSkillId;
	Result.Source = ESkillLabRequestSource::Test;
	Result.TargetType = ESkillLabTargetType::Self;
	CurrentRequestId = Result.RequestId;
	CurrentSkillId = Result.SkillId;
	CurrentSource = Result.Source;
	CurrentTargetType = Result.TargetType;
	PendingPulseAbility = nullptr;

	if (!bInitialized || !AbilitySystemComponent || !AttributeSet)
	{
		Result.Reason = TEXT("UnitNotInitialized");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}
	if (!IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed())
	{
		Result.Reason = TEXT("InvalidCaster");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}
	if (!bSkillGranted)
	{
		Result.Reason = TEXT("SkillNotGranted");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}
	if (Target && Target != GetOwner())
	{
		Result.Reason = TEXT("TargetUnsupportedInFirstSlice");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}
	if (GetWorld()->GetTimeSeconds() < CooldownEndTime)
	{
		Result.Reason = TEXT("Cooldown");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}
	if (GetSkillEnergy() < PulseCost)
	{
		Result.Reason = TEXT("InsufficientEnergy");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}

	FGameplayAbilitySpecHandle AbilityHandle;
	for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA(USkillLabPulseAbility::StaticClass()))
		{
			AbilityHandle = Spec.Handle;
			break;
		}
	}
	if (!AbilityHandle.IsValid())
	{
		Result.Reason = TEXT("AbilitySpecMissing");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}

	LogEvent(TEXT("ActivationChecked"));

	if (!AbilitySystemComponent->TryActivateAbility(AbilityHandle))
	{
		PendingPulseAbility = nullptr;
		Result.Status = ESkillLabRequestStatus::Rejected;
		Result.Reason = TEXT("ActivationFailed");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}

	if (!PendingPulseAbility)
	{
		Result.Status = ESkillLabRequestStatus::Rejected;
		Result.Reason = TEXT("ActivationContextMissing");
		LogEvent(TEXT("Rejected"), Result.Reason);
		return Result;
	}

	ActiveCastId = NextCastId++;
	Result.Status = ESkillLabRequestStatus::Started;
	Result.CastId = ActiveCastId;
	AbilitySystemComponent->ApplyModToAttribute(USkillLabAttributeSet::GetSkillEnergyAttribute(), EGameplayModOp::Additive, -PulseCost);
	CooldownEndTime = GetWorld()->GetTimeSeconds() + PulseCooldown;
	LogEvent(TEXT("RequestStarted"));

	USkillLabPulseAbility* ActivatedAbility = PendingPulseAbility;
	PendingPulseAbility = nullptr;
	if (!ExecutePulseActivation(ActivatedAbility, Result))
	{
		Result.Reason = TEXT("EffectApplyFailed");
		LogEvent(TEXT("EffectApplyFailed"), Result.Reason);
	}

	Result.Status = ESkillLabRequestStatus::Completed;
	LogEvent(TEXT("Completed"));
	ActiveCastId = 0;
	return Result;
}

void USkillLabSkillComponent::NotifyPulseAbilityActivationPending(USkillLabPulseAbility* Ability)
{
	PendingPulseAbility = Ability;
}

bool USkillLabSkillComponent::ExecutePulseActivation(USkillLabPulseAbility* Ability, FSkillLabRequestResult& Result)
{
	if (!Ability || Result.CastId != ActiveCastId)
	{
		return false;
	}

	return Ability->ExecutePulseEffect();
}

void USkillLabSkillComponent::NotifyPulseEffectApplied()
{
	LogEvent(TEXT("TargetResolved"));
	LogEvent(TEXT("EffectApplied"), FString::Printf(TEXT("TestValue=%.2f"), GetTestValue()));
}

void USkillLabSkillComponent::ResetUnit()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	AbilitySystemComponent->CancelAllAbilities();
	// The only first-slice effect is Instant, so it modifies the base value and is reset below.
	AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetTestValueAttribute(), InitialTestValue);
	AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetSkillEnergyAttribute(), InitialEnergy);
	CooldownEndTime = 0.0f;
	ActiveCastId = 0;
	LogEvent(TEXT("ResetCompleted"));
}

float USkillLabSkillComponent::GetTestValue() const
{
	return AbilitySystemComponent ? AbilitySystemComponent->GetNumericAttribute(USkillLabAttributeSet::GetTestValueAttribute()) : 0.0f;
}

float USkillLabSkillComponent::GetSkillEnergy() const
{
	return AbilitySystemComponent ? AbilitySystemComponent->GetNumericAttribute(USkillLabAttributeSet::GetSkillEnergyAttribute()) : 0.0f;
}

void USkillLabSkillComponent::LogEvent(const TCHAR* EventName, const FString& Detail) const
{
	const FString Context = BuildTraceContext();
	UE_LOG(LogTemp, Log, TEXT("[SkillSystem] %s Event=%s%s"), *Context, EventName, Detail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Detail=%s"), *Detail));
}

FString USkillLabSkillComponent::BuildTraceContext() const
{
	return FString::Printf(
		TEXT("UnitId=%d SkillId=%s RequestId=%d CastId=%d Source=%s Target=%s"),
		UnitId,
		*CurrentSkillId.ToString(),
		CurrentRequestId,
		ActiveCastId,
		RequestSourceToString(CurrentSource),
		TargetTypeToString(CurrentTargetType));
}
