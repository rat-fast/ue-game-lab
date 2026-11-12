#include "SkillLabEnergySettlement.h"
#include "SkillLabAbilitySystemComponent.h"
#include "GameFramework/Actor.h"

FSkillLabEnergySettlement USkillLabAbilitySystemComponent::QueryEnergySettlement(const uint64 TransactionId) const
{
	if (!IsValid(this) || bEnergySessionEnded || IsBeingDestroyed() || !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) return {};
	const FSkillLabEnergySettlement* History = EnergySettlementHistory.Find(TransactionId);
	if (!History) return {};
	FSkillLabEnergySettlement Result = *History;
	if (const FEnergyReservation* Reservation = EnergyReservations.Find(TransactionId))
	{
		Result.State = Reservation->State == EEnergyReservationState::Settling
			? ESkillLabEnergySettlementState::SettlementInProgress
			: (Reservation->bDebitAttempted ? ESkillLabEnergySettlementState::DebitAttemptUnconfirmed
				: ESkillLabEnergySettlementState::ReservedNoDebit);
	}
	else if (Result.State != ESkillLabEnergySettlementState::ConfirmedCommitted
		&& Result.State != ESkillLabEnergySettlementState::ConfirmedReleased)
	{
		Result.State = ESkillLabEnergySettlementState::Unavailable;
	}
	return Result;
}

void USkillLabAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnergySessionEnded = true;
	ClosePendingResourceChanges();
	Super::EndPlay(EndPlayReason);
}

void USkillLabAbilitySystemComponent::BeginDestroy()
{
	bEnergySessionEnded = true;
	ClosePendingResourceChanges();
	Super::BeginDestroy();
}

bool USkillLabAbilitySystemComponent::BindEnergyOperation(const uint64 TransactionId, UObject* OperationOwner,
	UObject* Runtime, const uint64 OperationId)
{
	FSkillLabEnergySettlement* History = EnergySettlementHistory.Find(TransactionId);
	if (!History || !OperationId || !IsValid(OperationOwner) || !IsValid(Runtime)
		|| QueryEnergySettlement(TransactionId).State != ESkillLabEnergySettlementState::ReservedNoDebit) return false;
	if (History->Operation)
		return History->Operation == OperationId && History->OperationOwner == FObjectKey(OperationOwner)
			&& History->Runtime == FObjectKey(Runtime);
	History->Operation = OperationId;
	History->OperationOwner = FObjectKey(OperationOwner);
	History->Runtime = FObjectKey(Runtime);
	return true;
}
