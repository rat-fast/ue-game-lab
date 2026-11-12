#include "SkillLabSubmissionResults.h"
#include "Engine/World.h"

bool FSkillLabSubmissionKey::operator==(const FSkillLabSubmissionKey& Other) const
{
	return Id == Other.Id && RequestId == Other.RequestId && Session == Other.Session && ResourceOwner == Other.ResourceOwner && Runtime == Other.Runtime
		&& SkillId == Other.SkillId && Generation == Other.Generation && Cast == Other.Cast && Point == Other.Point
		&& Transaction == Other.Transaction && Operation == Other.Operation;
}

FSkillLabProductionResult FSkillLabSubmissionResults::Query(const FSkillLabSubmissionKey& Key) const
{
	const FSkillLabProductionResult* Result = Results.Find(Key.Operation);
	return Result && Result->Key == Key ? *Result : FSkillLabProductionResult();
}

FSkillLabSubmissionKey FSkillLabSubmissionResults::Begin(FSkillLabSubmissionKey Key, const ESkillLabProductionKind Kind,
	const float Cost, const float Cooldown, const bool bAlreadyCommitted, UWorld* Clock)
{
	if (NextOperation == 0) return {};
	Key.Operation = NextOperation++;
	Key.Id = FGuid::NewGuid();
	FSkillLabProductionResult Result;
	Result.Key = Key;
	Result.Kind = Kind;
	Result.State = ESkillLabProductionState::InProgress;
	Result.Version = 1;
	Result.Cost = Cost;
	Result.Cooldown = Cooldown;
	Result.bAlreadyCommitted = bAlreadyCommitted;
	Result.Clock = Clock;
	Results.Add(Key.Operation, Result);
	return Key;
}

bool FSkillLabSubmissionResults::Confirm(const FSkillLabSubmissionKey& Key, const bool bAccepted,
	const FName Reason, TOptional<double> AcceptedAt)
{
	FSkillLabProductionResult* Result = Results.Find(Key.Operation);
	if (!Result || !(Result->Key == Key)) return false;
	const ESkillLabProductionState State = bAccepted ? ESkillLabProductionState::ConfirmedAccepted : ESkillLabProductionState::ConfirmedRejected;
	if (Result->State != ESkillLabProductionState::InProgress)
	{
		if (Result->State == State) return true;
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabProduction] Conflicting or unavailable result ignored: Operation=%llu"), Key.Operation);
		return false;
	}
	Result->State = State;
	Result->Reason = Reason;
	Result->AcceptedAt = bAccepted ? AcceptedAt : TOptional<double>();
	++Result->Version;
	UE_LOG(LogTemp, Log, TEXT("[SkillLabProduction] Operation=%llu Cast=%llu Point=%llu Kind=%d Accepted=%d Version=%llu TimeKnown=%d Reason=%s"),
		Key.Operation, Key.Cast, Key.Point, static_cast<int32>(Result->Kind), bAccepted, Result->Version, Result->AcceptedAt.IsSet(), *Reason.ToString());
	if (Result->ParentOperation) Aggregate(Result->ParentOperation);
	return true;
}

TArray<FSkillLabSubmissionKey> FSkillLabSubmissionResults::PlanArea(const FSkillLabSubmissionKey& Key,
	const int32 Count, const bool bQueryComplete)
{
	FSkillLabProductionResult Parent = Query(Key);
	if (Parent.Kind != ESkillLabProductionKind::Area || Parent.State != ESkillLabProductionState::InProgress
		|| !Parent.Children.IsEmpty()) return {};
	TArray<FSkillLabSubmissionKey> Children;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FSkillLabSubmissionKey Child = Begin(Key, ESkillLabProductionKind::Effect,
			Parent.Cost, Parent.Cooldown, Parent.bAlreadyCommitted, Parent.Clock.Get());
		Results.FindChecked(Child.Operation).ParentOperation = Key.Operation;
		Children.Add(Child);
	}
	FSkillLabProductionResult& Stored = Results.FindChecked(Key.Operation);
	Stored.Children = Children;
	Stored.bAreaQueryComplete = bQueryComplete;
	Aggregate(Key.Operation);
	return Children;
}

void FSkillLabSubmissionResults::Aggregate(const uint64 Operation)
{
	const FSkillLabProductionResult Parent = Results.FindChecked(Operation);
	if (Parent.State != ESkillLabProductionState::InProgress || !Parent.bAreaQueryComplete) return;
	if (Parent.Children.IsEmpty())
	{
		Confirm(Parent.Key, true, TEXT("AreaEmpty"), Parent.Clock.IsValid()
			? TOptional<double>(Parent.Clock->GetTimeSeconds()) : TOptional<double>());
		return;
	}
	bool bAllRejected = true;
	for (const FSkillLabSubmissionKey& Child : Parent.Children)
	{
		const FSkillLabProductionResult Result = Query(Child);
		if (Result.State == ESkillLabProductionState::ConfirmedAccepted)
		{
			Confirm(Parent.Key, true, TEXT("AreaAnyApplied"), Result.AcceptedAt);
			return;
		}
		bAllRejected &= Result.State == ESkillLabProductionState::ConfirmedRejected;
	}
	if (bAllRejected) Confirm(Parent.Key, false, TEXT("AreaAllRejected"));
}

void FSkillLabSubmissionResults::EndSession()
{
	for (auto& Pair : Results)
	{
		if (Pair.Value.State != ESkillLabProductionState::InProgress) continue;
		Pair.Value.State = ESkillLabProductionState::Unavailable;
		Pair.Value.Reason = TEXT("ProductionSessionEnded");
		++Pair.Value.Version;
	}
}
