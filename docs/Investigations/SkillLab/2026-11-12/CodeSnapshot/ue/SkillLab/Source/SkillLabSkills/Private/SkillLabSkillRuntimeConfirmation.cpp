#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

#include "UObject/StrongObjectPtr.h"
FSkillLabProductionHandle USkillLabSkillRuntimeComponent::GetProductionHandle(const USkillLabSkillBehavior* Behavior, const int32 CastId) const
{
	USkillLabCastExecution* Found = FindExecution(Behavior, CastId);
	if (!Found) { return {}; }
	TStrongObjectPtr<USkillLabCastExecution> ExecutionGuard(Found);
	USkillLabCastExecution& Execution = *Found;
	return Execution.bSubmitting && Execution.Submission.Behavior.Get() == Behavior && Execution.Submission.CastToken.CastId == static_cast<uint64>(CastId)
		&& Execution.Submission.RuntimeGeneration == Execution.Token.RuntimeGeneration ? Execution.Submission.Production : FSkillLabProductionHandle();
}

bool USkillLabSkillRuntimeComponent::CanReceiveSubmissionConfirmation(const USkillLabUnconfirmedSubmissions* Session) const
{
	return IsValid(this) && !IsBeingDestroyed() && bInitialized && !bShuttingDown
		&& IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed() && UnconfirmedSubmissions.Get() == Session
		&& IsValid(Session) && Session->CanAcceptConfirmedCooldown(ASC.Get());
}

bool USkillLabSkillRuntimeComponent::OnSubmissionConfirmed(const FSkillLabSubmissionConfirmation& Confirmation)
{
	if (!CanReceiveSubmissionConfirmation(UnconfirmedSubmissions.Get())
		|| Confirmation.Key.Session != FObjectKey(UnconfirmedSubmissions.Get())
		|| Confirmation.Key.ResourceOwner != FObjectKey(ASC.Get())) return false;
	const auto* Original = Confirmation.Key.Runtime == FObjectKey(this) ? FindMutableExecution(static_cast<int32>(Confirmation.Key.Cast)) : nullptr;
	const bool bOriginalCast = Original && Original->Token.RuntimeGeneration == Confirmation.Key.Generation
		&& Original->Submission.Production.Key == Confirmation.Key;
	FSkillLabSkillResult Result;
	if (bOriginalCast) Result.CastToken = Original->Token;
	Result.SkillId = Confirmation.Key.SkillId;
	Result.CastId = static_cast<int32>(Confirmation.Key.Cast);
	Result.SubmissionPointId = static_cast<int64>(Confirmation.Key.Point);
	Result.SubmissionKey = Confirmation.Key;
	Result.RequestId = Confirmation.Key.RequestId;
	Result.bSuccess = Confirmation.bActionSucceeded && Confirmation.bResourceResolved && Confirmation.bCooldownRecorded;
	Result.bCostCommitted = Confirmation.bCostCommitted;
	Result.SourceActor = GetOwner();
	Result.SourceUnitId = UnitId;
	// 先终止原未决等待；迟到确认不能重新执行动作或推进新 Cast。
	if (bOriginalCast && Original->Submission.Outcome == ESubmissionState::Indeterminate)
		CancelCast(Original->Token, Confirmation.Reason);
	if (!CanReceiveSubmissionConfirmation(UnconfirmedSubmissions.Get())) return false;
	Emit(ESkillLabSkillEvent::SubmissionResolved, Result, Confirmation.Reason);
	return true;
}
