#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"

enum class ESkillLabEnergySettlementState : uint8
{
	Unavailable, ReservedNoDebit, SettlementInProgress, ConfirmedCommitted, ConfirmedReleased, DebitAttemptUnconfirmed
};

// 资源事实独立于技能接受事实；不以 reservation 消失或余额推测结算。
struct SKILLLABGAS_API FSkillLabEnergySettlement
{
	FObjectKey Owner, OperationOwner, Runtime;
	uint64 Transaction = 0, Generation = 0, Cast = 0, Operation = 0, Version = 0;
	float Amount = 0.0f;
	ESkillLabEnergySettlementState State = ESkillLabEnergySettlementState::Unavailable;
};
