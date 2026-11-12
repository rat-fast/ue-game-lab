#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "GameFramework/Actor.h"

void USkillLabAbilitySystemComponent::OnResourceChanged(const FOnAttributeChangeData& Data)
{
	if (ResourceWriteDepth > 0)
	{
		if (Data.Attribute == ExpectedResourceNotification
			&& FMath::IsNearlyEqual(Data.OldValue, ExpectedResourceOldValue)
			&& FMath::IsNearlyEqual(Data.NewValue, ExpectedResourceNewValue))
		{
			bAwaitingResourceNotification = false;
			if (Data.Attribute == USkillLabCombatAttributeSet::GetEnergyAttribute() && SettlingEnergyTransactionId != 0)
			{
				if (FEnergyReservation* Reservation = EnergyReservations.Find(SettlingEnergyTransactionId))
				{
					if (Reservation->bDebitAttempted) Reservation->bDebitObserved = true;
				}
			}
			return;
		}
		bUnsupportedResourceWrite = true;
		++ResourceBypassGeneration;
		UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Unsupported reentrant Energy/MaxEnergy write during coordinated resource change"));
		return;
	}
	if (!EnergyReservations.IsEmpty() && !FMath::IsNearlyEqual(Data.OldValue, Data.NewValue))
	{
		bUnsupportedResourceWrite = true;
		++ResourceBypassGeneration;
		UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Energy/MaxEnergy changed outside the resource coordinator while reservations are active"));
	}
}

bool USkillLabAbilitySystemComponent::ApplyResourceChange(const FPendingResourceChange& Change)
{
	const uint64 BypassGenerationBefore = ResourceBypassGeneration;
	if (!CanWriteResources() || HasResourceAggregation()) return false;
	USkillLabCombatAttributeSet* Attributes = GetCombatAttributeSet();
	if (!Attributes) return false;
	const FGameplayAttribute Attribute = Change.bMaxEnergy ? USkillLabCombatAttributeSet::GetMaxEnergyAttribute() : USkillLabCombatAttributeSet::GetEnergyAttribute();
	const float OldValue = Change.bMaxEnergy ? Attributes->GetMaxEnergy() : Attributes->GetEnergy();
	const float Limit = Change.bMaxEnergy ? TNumericLimits<float>::Max() : Attributes->GetMaxEnergy();
	const float ProposedValue = Change.Operation == EGameplayModOp::Additive ? OldValue + Change.Value : Change.Value;
	const float NewValue = Change.bMaxEnergy ? FMath::Max(0.0f, ProposedValue) : FMath::Clamp(ProposedValue, 0.0f, Limit);
	if (!FMath::IsFinite(ProposedValue) || !FMath::IsFinite(NewValue)) return false;

	ExpectedResourceNotification = Attribute;
	ExpectedResourceOldValue = OldValue;
	ExpectedResourceNewValue = NewValue;
	bAwaitingResourceNotification = !FMath::IsNearlyEqual(OldValue, NewValue);
	{
		TGuardValue<uint32> WriteDepth(ResourceWriteDepth, ResourceWriteDepth + 1);
		// 写入钳制后的权威目标，不能把原始越界值留在 GAS base。
		SetNumericAttributeBase(Attribute, NewValue);
	}
	bAwaitingResourceNotification = false;
	// 属性委托可结束本会话；禁止继续夹紧另一属性或消费队列。
	if (!CanWriteResources()) return false;
	bool bApplied = FMath::IsNearlyEqual(Change.bMaxEnergy ? Attributes->GetMaxEnergy() : Attributes->GetEnergy(), NewValue);
	bApplied &= FMath::IsNearlyEqual(GetNumericAttributeBase(Attribute), NewValue);
	if (Change.bMaxEnergy && Attributes->GetEnergy() > Attributes->GetMaxEnergy())
	{
		bApplied &= ApplyResourceChange({false, EGameplayModOp::Override, Attributes->GetMaxEnergy()});
	}
	float OtherReservations = GetReservedEnergy();
	if (SettlingEnergyTransactionId != 0)
	{
		if (const FEnergyReservation* SettlingReservation = EnergyReservations.Find(SettlingEnergyTransactionId))
		{
			OtherReservations = FMath::Max(0.0f, OtherReservations - SettlingReservation->Amount);
		}
	}
	if (Attributes->GetEnergy() + UE_KINDA_SMALL_NUMBER < OtherReservations) bApplied = false;
	if (!bApplied || ResourceBypassGeneration != BypassGenerationBefore)
	{
		bUnsupportedResourceWrite = true;
		++ResourceBypassGeneration;
		UE_LOG(LogTemp, Error, TEXT("[SkillLabResource] Coordinated Energy/MaxEnergy change violated the expected GAS value or reservation invariant"));
		return false;
	}
	return true;
}

bool USkillLabAbilitySystemComponent::RequestResourceChange(const bool bMaxEnergy, const EGameplayModOp::Type Operation, const float Value)
{
	return RequestResourceChangeDetailed(bMaxEnergy, Operation, Value) != ESkillLabResourceChangeResult::Rejected;
}

ESkillLabResourceChangeResult USkillLabAbilitySystemComponent::RequestResourceChangeDetailed(
	const bool bMaxEnergy, const EGameplayModOp::Type Operation, const float Value)
{
	return SubmitResourceChange({bMaxEnergy, Operation, Value});
}

ESkillLabResourceChangeResult USkillLabAbilitySystemComponent::RequestResourceChangeTracked(
	bool bMaxEnergy, EGameplayModOp::Type Operation, float Value, uint64 AttemptId, TWeakObjectPtr<UObject> Owner,
	TFunction<FName()> BeforeApply, TFunction<void(const FSkillLabResourceReceipt&)> OnFinished)
{
	FPendingResourceChange Change{bMaxEnergy, Operation, Value};
	Change.AttemptId = AttemptId;
	Change.Owner = Owner;
	Change.BeforeApply = MoveTemp(BeforeApply);
	Change.OnFinished = MoveTemp(OnFinished);
	return SubmitResourceChange(MoveTemp(Change));
}

void USkillLabAbilitySystemComponent::FinishResourceChange(FPendingResourceChange& Change, ESkillLabResourceCompletion Result, FName Reason)
{
	auto Callback = MoveTemp(Change.OnFinished);
	if (!Callback) return;
	if (Change.Owner.IsValid()) Callback({Change.AttemptId, FObjectKey(this), Result, Reason});
	else UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] Receipt owner unavailable: attempt=%llu result=%d reason=%s"), Change.AttemptId, static_cast<int32>(Result), *Reason.ToString());
}

ESkillLabResourceChangeResult USkillLabAbilitySystemComponent::SubmitResourceChange(FPendingResourceChange Change)
{
	if (ResourceWriteDepth > 0)
	{
		PendingResourceChanges.Add(MoveTemp(Change));
		return ESkillLabResourceChangeResult::Queued;
	}
	FName Reason;
	const auto Result = ExecuteResourceChange(Change, Reason);
	FinishResourceChange(Change, Result, Reason);
	DrainPendingResourceChanges();
	return Result == ESkillLabResourceCompletion::Applied ? ESkillLabResourceChangeResult::Applied : ESkillLabResourceChangeResult::Rejected;
}

ESkillLabResourceCompletion USkillLabAbilitySystemComponent::ExecuteResourceChange(FPendingResourceChange& Change, FName& Reason)
{
	Reason = TEXT("ResourceUnavailable");
	if (!CanWriteResources()) return ESkillLabResourceCompletion::Unavailable;
	if (Change.OnFinished && (!Change.AttemptId || !Change.Owner.IsValid())) return ESkillLabResourceCompletion::Unavailable;
	if (Change.BeforeApply)
	{
		Reason = Change.BeforeApply();
		if (!Reason.IsNone()) return ESkillLabResourceCompletion::Rejected;
		if (!CanWriteResources() || (Change.OnFinished && !Change.Owner.IsValid())) { Reason = TEXT("ResourceUnavailable"); return ESkillLabResourceCompletion::Unavailable; }
	}
	Reason = TEXT("ResourceChangeRejected");
	const bool bMaxEnergy = Change.bMaxEnergy;
	const auto Operation = Change.Operation;
	const float Value = Change.Value;
	if (!FMath::IsFinite(Value) || (Operation != EGameplayModOp::Additive && Operation != EGameplayModOp::Override)) return ESkillLabResourceCompletion::Rejected;
	const USkillLabCombatAttributeSet* Attributes = GetCombatAttributeSet();
	if (!Attributes || HasResourceAggregation()) return ESkillLabResourceCompletion::Rejected;
	if (!FMath::IsFinite((Operation == EGameplayModOp::Additive
		? (bMaxEnergy ? Attributes->GetMaxEnergy() : Attributes->GetEnergy()) : 0.0f) + Value)) return ESkillLabResourceCompletion::Rejected;
	const float Reserved = GetReservedEnergy();
	float ProposedEnergy = Attributes->GetEnergy();
	float ProposedMaximum = Attributes->GetMaxEnergy();
	if (bMaxEnergy)
	{
		ProposedMaximum = FMath::Max(0.0f, Operation == EGameplayModOp::Additive ? ProposedMaximum + Value : Value);
		ProposedEnergy = FMath::Min(ProposedEnergy, ProposedMaximum);
	}
	else
	{
		ProposedEnergy = FMath::Clamp(Operation == EGameplayModOp::Additive ? ProposedEnergy + Value : Value, 0.0f, ProposedMaximum);
	}
	if (ProposedEnergy + UE_KINDA_SMALL_NUMBER < Reserved)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] Resource change rejected because it would invalidate active Energy reservations"));
		return ESkillLabResourceCompletion::Rejected;
	}
	const bool bApplied = ApplyResourceChange(Change);
	Reason = bApplied ? TEXT("ResourceChangeApplied") : TEXT("ResourceChangeFailed");
	return bApplied ? ESkillLabResourceCompletion::Applied : ESkillLabResourceCompletion::Failed;
}

float USkillLabAbilitySystemComponent::GetActualEnergy() const
{
	const USkillLabCombatAttributeSet* Attributes = GetCombatAttributeSet();
	return Attributes ? Attributes->GetEnergy() : 0.0f;
}

float USkillLabAbilitySystemComponent::GetReservedEnergy() const
{
	float Total = 0.0f;
	for (const TPair<uint64, FEnergyReservation>& Pair : EnergyReservations) Total += Pair.Value.Amount;
	return Total;
}

float USkillLabAbilitySystemComponent::GetAvailableEnergy() const
{
	return FMath::Max(0.0f, GetActualEnergy() - GetReservedEnergy());
}

void USkillLabAbilitySystemComponent::DrainPendingResourceChanges()
{
	if (ResourceWriteDepth > 0 || bDrainingResourceChanges) return;
	TGuardValue<bool> DrainGuard(bDrainingResourceChanges, true);
	if (!CanWriteResources()) { ClosePendingResourceChanges(); return; }
	while (ResourceWriteDepth == 0 && !HasSettlingEnergyReservation() && !PendingResourceChanges.IsEmpty())
	{
		FPendingResourceChange Change = MoveTemp(PendingResourceChanges[0]);
		PendingResourceChanges.RemoveAt(0);
		FName Reason;
		const auto Result = ExecuteResourceChange(Change, Reason);
		if (Result != ESkillLabResourceCompletion::Applied)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] Reentrant coordinated resource change was rejected after revalidation"));
		}
		FinishResourceChange(Change, Result, Reason);
		if (!CanWriteResources()) { ClosePendingResourceChanges(); return; }
	}
}

void USkillLabAbilitySystemComponent::ClosePendingResourceChanges()
{
	if (ResourceWriteDepth > 0) return;
	TArray<FPendingResourceChange> Closing = MoveTemp(PendingResourceChanges);
	PendingResourceChanges.Reset();
	for (auto& Change : Closing) FinishResourceChange(Change, ESkillLabResourceCompletion::Unavailable, TEXT("ResourceUnavailable"));
}

