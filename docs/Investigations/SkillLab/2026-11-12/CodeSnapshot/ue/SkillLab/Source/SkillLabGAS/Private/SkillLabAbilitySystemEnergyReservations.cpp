#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"

bool USkillLabAbilitySystemComponent::HasSettlingEnergyReservation() const
{
	for (const TPair<uint64, FEnergyReservation>& Pair : EnergyReservations)
	{
		if (Pair.Value.State == EEnergyReservationState::Settling) return true;
	}
	return false;
}

uint64 USkillLabAbilitySystemComponent::AllocateEnergyTransactionId()
{
	while (NextEnergyTransaction != 0 && UsedEnergyTransactionIds.Contains(NextEnergyTransaction)) ++NextEnergyTransaction;
	if (NextEnergyTransaction == 0) return 0;
	const uint64 Id = NextEnergyTransaction++;
	UsedEnergyTransactionIds.Add(Id);
	AllocatedEnergyTransactionIds.Add(Id);
	return Id;
}

bool USkillLabAbilitySystemComponent::BeginEnergyEscrow(const uint64 TransactionId, const float Cost, const uint64 RuntimeGeneration, const uint64 CastId)
{
	if (!TransactionId || (UsedEnergyTransactionIds.Contains(TransactionId) && !AllocatedEnergyTransactionIds.Contains(TransactionId))) return false;
	UsedEnergyTransactionIds.Add(TransactionId);
	AllocatedEnergyTransactionIds.Remove(TransactionId);
	const USkillLabCombatAttributeSet* Attributes = GetCombatAttributeSet();
	if (!CanWriteResources() || HasResourceAggregation() || ResourceWriteDepth > 0 || !Attributes || !FMath::IsFinite(Cost) || Cost < 0.0f || !GetOwner() || !GetOwner()->HasAuthority()
		|| !PendingResourceChanges.IsEmpty() || bUnsupportedResourceWrite
		|| !FMath::IsNearlyEqual(GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()), Attributes->GetEnergy())
		|| !FMath::IsNearlyEqual(GetNumericAttributeBase(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()), Attributes->GetMaxEnergy())
		|| GetAvailableEnergy() < Cost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabResource] Energy reservation rejected by current GAS state or active resource write"));
		return false;
	}
	EnergyReservations.Add(TransactionId, {Cost, RuntimeGeneration, CastId, EEnergyReservationState::Pending});
	FSkillLabEnergySettlement Snapshot;
	Snapshot.Owner = FObjectKey(this);
	Snapshot.Transaction = TransactionId;
	Snapshot.Generation = RuntimeGeneration;
	Snapshot.Cast = CastId;
	Snapshot.Amount = Cost;
	Snapshot.State = ESkillLabEnergySettlementState::ReservedNoDebit;
	Snapshot.Version = 1;
	EnergySettlementHistory.Add(TransactionId, Snapshot);
	return true;
}

bool USkillLabAbilitySystemComponent::ResolveEnergyEscrow(const uint64 TransactionId, const bool bSuccess)
{
	return ResolveEnergyReservation(TransactionId, bSuccess);
}

bool USkillLabAbilitySystemComponent::HasIndeterminateEnergyEscrow(const uint64 TransactionId) const
{
	const FEnergyReservation* Reservation = EnergyReservations.Find(TransactionId);
	return Reservation && Reservation->State == EEnergyReservationState::Indeterminate;
}

bool USkillLabAbilitySystemComponent::ResolveEnergyReservation(const uint64 TransactionId, const bool bSuccess)
{
	FEnergyReservation* Reservation = EnergyReservations.Find(TransactionId);
	if (bEnergySessionEnded || !Reservation || Reservation->State != EEnergyReservationState::Pending || SettlingEnergyTransactionId != 0
		|| ResourceWriteDepth > 0) return false;
	// 扣款尝试不明时，失败也不能释放 reservation；观察到委托不足以证明未扣款。
	if (Reservation->bDebitAttempted) return false;
	bool bResolved = false;
	{
		TGuardValue<uint64> SettlementLock(SettlingEnergyTransactionId, TransactionId);
		Reservation->State = EEnergyReservationState::Settling;
		if (bSuccess && Reservation->Amount > 0.0f)
		{
			if (Reservation->bDebitAttempted)
			{
				Reservation->State = EEnergyReservationState::Indeterminate;
			}
			else
			{
				const float ExpectedEnergy = GetActualEnergy() - Reservation->Amount;
				const float OtherReservations = FMath::Max(0.0f, GetReservedEnergy() - Reservation->Amount);
				if (ExpectedEnergy + UE_KINDA_SMALL_NUMBER < OtherReservations)
				{
					Reservation->State = EEnergyReservationState::Indeterminate;
				}
				else
				{
					const float BeforeMaximum = GetCombatAttributeSet()->GetMaxEnergy();
					Reservation->bDebitAttempted = true;
					const bool bApplied = ApplyResourceChange({false, EGameplayModOp::Additive, -Reservation->Amount});
					if (bApplied && Reservation->bDebitObserved && FMath::IsNearlyEqual(GetActualEnergy(), ExpectedEnergy)
						&& FMath::IsNearlyEqual(GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()), ExpectedEnergy)
						&& FMath::IsNearlyEqual(GetCombatAttributeSet()->GetMaxEnergy(), BeforeMaximum)
						&& !bUnsupportedResourceWrite)
					{
						FSkillLabEnergySettlement& History = EnergySettlementHistory.FindChecked(TransactionId);
						History.State = ESkillLabEnergySettlementState::ConfirmedCommitted;
						++History.Version;
						EnergyReservations.Remove(TransactionId);
						bResolved = true;
					}
					else Reservation->State = EEnergyReservationState::Indeterminate;
				}
			}
		}
		else
		{
			FSkillLabEnergySettlement& History = EnergySettlementHistory.FindChecked(TransactionId);
			History.State = bSuccess ? ESkillLabEnergySettlementState::ConfirmedCommitted : ESkillLabEnergySettlementState::ConfirmedReleased;
			++History.Version;
			EnergyReservations.Remove(TransactionId);
			bResolved = true;
		}
	}
	DrainPendingResourceChanges();
	return bResolved;
}

bool USkillLabAbilitySystemComponent::MarkEnergyEscrowIndeterminate(const uint64 TransactionId)
{
	FEnergyReservation* Reservation = EnergyReservations.Find(TransactionId);
	if (!Reservation || Reservation->State != EEnergyReservationState::Pending) return false;
	Reservation->State = EEnergyReservationState::Indeterminate;
	return true;
}

bool USkillLabAbilitySystemComponent::ResolveIndeterminateEnergyEscrow(const uint64 TransactionId, const bool bSucceeded)
{
	FEnergyReservation* Reservation = EnergyReservations.Find(TransactionId);
	if (bEnergySessionEnded || !Reservation || Reservation->State != EEnergyReservationState::Indeterminate || SettlingEnergyTransactionId != 0
		|| ResourceWriteDepth > 0) return false;
	if (Reservation->bDebitAttempted) return false;
	Reservation->State = EEnergyReservationState::Pending;
	return ResolveEnergyReservation(TransactionId, bSucceeded);
}
