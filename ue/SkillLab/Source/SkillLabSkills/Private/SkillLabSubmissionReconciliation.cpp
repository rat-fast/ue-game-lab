#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "Engine/World.h"

FSkillLabProductionHandle USkillLabUnconfirmedSubmissions::BeginProduction(UObject* Runtime, const uint64 Generation,
	const uint64 Cast, const uint64 Point, const uint64 Transaction, const int32 RequestId,
	const FSkillLabSkillDefinition& Definition, const bool bAlreadyCommitted)
{
	if (!CanAcceptConfirmedCooldown(ResourceOwner.Get()) || !IsValid(Runtime)) return {};
	FSkillLabSubmissionKey Key;
	Key.Session = FObjectKey(this);
	Key.ResourceOwner = FObjectKey(ResourceOwner.Get());
	Key.Runtime = FObjectKey(Runtime);
	Key.Generation = Generation;
	Key.Cast = Cast;
	Key.Point = Point;
	Key.Transaction = Transaction;
	Key.SkillId = Definition.SkillId;
	Key.RequestId = RequestId;
	const ESkillLabProductionKind Kind = Definition.Execution == ESkillLabExecutionTemplate::GroundArea ? ESkillLabProductionKind::Area
		: (Definition.Execution == ESkillLabExecutionTemplate::Projectile ? ESkillLabProductionKind::Projectile : ESkillLabProductionKind::Effect);
	Key = ProductionResults.Begin(Key, Kind, Definition.Cost, Definition.Cooldown, bAlreadyCommitted, GetWorld());
	return {this, Key};
}

void USkillLabUnconfirmedSubmissions::ReconcileProductionResults()
{
	if (!CanAcceptConfirmedCooldown(ResourceOwner.Get())) { ReportLostOwner(); return; }
	// 同步回调可以追加或完成记录；快照只处理本轮开始时仍活跃的稳定下标。
	const TArray<int32> PendingIndices = ActiveRecordIndices.Array();
	for (const int32 Index : PendingIndices)
	{
		if (!CanAcceptConfirmedCooldown(ResourceOwner.Get())) break;
		if (ActiveRecordIndices.Contains(Index) && Records.IsValidIndex(Index)) ReconcileRecord(Index);
	}
}

void USkillLabUnconfirmedSubmissions::ReconcileRecord(const int32 Index)
{
	if (Records[Index].bResolving) return;
	if (Records[Index].bResolved) { DeliverConfirmation(Index); return; }
	const FSkillLabUnconfirmedSubmission Record = Records[Index];
	const FSkillLabSubmissionKey Key = Record.ProductionKey;
	if (!Key.IsSet() || Record.ResultSourceId != TEXT("SkillLabProduction") || Key.Session != FObjectKey(this)
		|| Key.ResourceOwner != FObjectKey(ResourceOwner.Get()) || Key.Runtime != Record.RuntimeIdentity
		|| Key.Generation != Record.RuntimeGeneration || Key.Cast != Record.CastId || Key.Point != Record.PointId
		|| Key.Transaction != Record.TransactionId || Key.SkillId != Record.SkillId) return;
	const FSkillLabProductionResult Production = ProductionResults.Query(Key);
	const bool bAccepted = Production.State == ESkillLabProductionState::ConfirmedAccepted;
	if (!bAccepted && Production.State != ESkillLabProductionState::ConfirmedRejected) return;
	if (Production.bAlreadyCommitted != Record.bAlreadyCommitted || Production.Clock.Get() != GetWorld()) return;
	Records[Index].bResolving = true;
	FSkillLabSubmissionConfirmation Confirmation;
	Confirmation.Key = Key;
	Confirmation.bActionSucceeded = bAccepted;
	Confirmation.ProductionVersion = Production.Version;
	if (Record.bAlreadyCommitted)
	{
		// 原费用、冷却事实已在同 Cast 的先前成功点确认；本点不能重新启动或清除。
		Confirmation.bResourceResolved = Key.Transaction == 0;
		Confirmation.bCooldownRecorded = Confirmation.bResourceResolved;
	}
	else
	{
		FSkillLabEnergySettlement Resource = ResourceOwner->QueryEnergySettlement(Key.Transaction);
		const bool bIdentityMatches = Resource.Owner == Key.ResourceOwner && Resource.OperationOwner == Key.Session
			&& Resource.Runtime == Key.Runtime && Resource.Operation == Key.Operation && Resource.Transaction == Key.Transaction
			&& Resource.Generation == Key.Generation && Resource.Cast == Key.Cast && FMath::IsNearlyEqual(Resource.Amount, Production.Cost);
		if (!bIdentityMatches) { Records[Index].bResolving = false; return; }
		if (Resource.State == ESkillLabEnergySettlementState::ReservedNoDebit)
		{
			// 只在从未尝试扣费时允许一次结转/释放；所有后续调用重新查询历史。
			if (ResourceOwner->HasIndeterminateEnergyEscrow(Key.Transaction)) ResourceOwner->ResolveIndeterminateEnergyEscrow(Key.Transaction, bAccepted);
			else ResourceOwner->ResolveEnergyEscrow(Key.Transaction, bAccepted);
			if (!CanAcceptConfirmedCooldown(ResourceOwner.Get()))
			{
				Records[Index].bResolving = false;
				const bool bResourceClosed = ResourceOwner.IsValid() && ResourceOwner->QueryEnergySettlement(Key.Transaction).State
					== (bAccepted ? ESkillLabEnergySettlementState::ConfirmedCommitted : ESkillLabEnergySettlementState::ConfirmedReleased);
				UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] SubmissionDeliveryUnavailable Operation=%llu ActionSucceeded=%d ResourceResolved=%d CooldownRecorded=0; no retry or compensation"),
					Key.Operation, bAccepted, bResourceClosed);
				ReportLostOwner();
				return;
			}
			Resource = ResourceOwner->QueryEnergySettlement(Key.Transaction);
		}
		Confirmation.ResourceVersion = Resource.Version;
		Confirmation.bResourceResolved = Resource.State == (bAccepted ? ESkillLabEnergySettlementState::ConfirmedCommitted
			: ESkillLabEnergySettlementState::ConfirmedReleased);
		if (!Confirmation.bResourceResolved)
		{
			Records[Index].bResolving = false;
			if (!Records[Index].bReportedResourceUnknown)
			{
				Records[Index].bReportedResourceUnknown = true;
				UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] Resource fact unresolved: Operation=%llu Transaction=%llu ActionSucceeded=%d ResourceResolved=0 CooldownRecorded=0 State=%d; no retry or compensation"),
					Key.Operation, Key.Transaction, bAccepted, static_cast<int32>(Resource.State));
			}
			return; // 已尝试但未闭合、失效或矛盾：继续隔离，不扣第二次、不释放。
		}
		Confirmation.bCooldownRecorded = bAccepted && (Production.bCooldownRecorded || StartConfirmedCooldown(Key.SkillId,
			Production.AcceptedAt.Get(GetWorld()->GetTimeSeconds()) + Production.Cooldown));
		if (bAccepted && Confirmation.bCooldownRecorded)
			ProductionResults.Results.FindChecked(Key.Operation).bCooldownRecorded = true;
	}
	if (!CanAcceptConfirmedCooldown(ResourceOwner.Get()) || (bAccepted && !Confirmation.bCooldownRecorded))
	{
		Records[Index].bResolving = false;
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] SubmissionDeliveryUnavailable Operation=%llu ActionSucceeded=%d ResourceResolved=%d CooldownRecorded=%d; no retry or compensation"),
			Key.Operation, bAccepted, Confirmation.bResourceResolved, Confirmation.bCooldownRecorded);
		ReportLostOwner();
		return;
	}
	Records[Index].bResolved = true;
	Records[Index].bResolving = false;
	Records[Index].bReportedLostOwner = false;
	Confirmation.Reason = bAccepted ? TEXT("SubmissionConfirmedAccepted") : TEXT("SubmissionConfirmedRejected");
	Confirmation.bCostCommitted = (bAccepted || Record.bAlreadyCommitted) && Confirmation.bResourceResolved;
	Confirmation.CooldownRemaining = GetCooldownRemaining(Key.SkillId);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabUnconfirmed] Resolved Operation=%llu Skill=%s Cast=%llu Accepted=%d ResourceVersion=%llu CooldownRemaining=%.3f"),
		Key.Operation, *Key.SkillId.ToString(), Key.Cast, bAccepted, Confirmation.ResourceVersion, GetCooldownRemaining(Key.SkillId));
	PendingFeedback.Add(Index, Confirmation);
	DeliverConfirmation(Index);
	if (!CanAcceptConfirmedCooldown(ResourceOwner.Get())) ReportLostOwner();
}

void USkillLabUnconfirmedSubmissions::DeliverConfirmation(const int32 Index)
{
	if (Records[Index].bNotified || !CanAcceptConfirmedCooldown(ResourceOwner.Get()) || !FeedbackRuntime.IsValid()
		|| !FeedbackRuntime->CanReceiveSubmissionConfirmation(this)) return;
	const auto* Pending = PendingFeedback.Find(Index);
	if (!Pending) return;
	const FSkillLabSubmissionConfirmation Confirmation = *Pending;
	Records[Index].bNotified = true; // 先锁定；反馈中的查询或 Runtime 重建不得重复广播。
	if (!FeedbackRuntime->OnSubmissionConfirmed(Confirmation))
	{
		Records[Index].bNotified = false;
		return; // 原 Cast 清理时接收方失效；由本会话下一个有效反馈入口继续交付。
	}
	PendingFeedback.Remove(Index);
	ActiveRecordIndices.Remove(Index);
	if (CanAcceptConfirmedCooldown(ResourceOwner.Get())) OnSubmissionConfirmed.Broadcast(Confirmation);
}
