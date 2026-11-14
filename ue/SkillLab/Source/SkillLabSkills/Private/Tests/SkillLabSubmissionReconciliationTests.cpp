#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabProductionFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSubmissionReconciliationTest, "SkillLab.Skills.SubmissionReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabSubmissionReconciliationTest::RunTest(const FString&)
{
	FSkillLabProductionFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Accepted = Fixture.MakeUnit();
	Accepted.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 50.0f);
	TestTrue(TEXT("Accepted delayed result starts through Ability"), Fixture.Start(Accepted).bSuccess);
	if (!Fixture.Begin(Accepted)) return false;
	TestTrue(TEXT("Real accepted GE executes before reply loss"), Fixture.Apply(Accepted, Accepted.Actor).bEffectApplied);
	Fixture.LoseReturn(Accepted);
	TestEqual(TEXT("Unknown first reply retains actual energy"), Accepted.ASC->GetActualEnergy(), 100.0f);
	TestEqual(TEXT("Unknown first reply retains reservation"), Accepted.ASC->GetReservedEnergy(), 2.0f);
	int32 Reports = 0, ResourceCallbacks = 0, SupplementalEvents = 0;
	const auto EventHandle = Accepted.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{ if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) ++SupplementalEvents; });
	const auto ReportHandle = Accepted.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation& Result)
	{
		++Reports;
		TestTrue(TEXT("Confirmation reports three independent facts"), Result.bActionSucceeded && Result.bResourceResolved && Result.bCooldownRecorded);
		Accepted.Session->ReconcileProductionResults(); // 同步通知重入不得二次消费。
		Fixture.Retain(Accepted, Accepted.Production); // 乱序重复交接也不得重开。
	});
	const auto DebitHandle = Accepted.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (Data.NewValue >= Data.OldValue) return;
			++ResourceCallbacks;
			Accepted.Session->ReconcileProductionResults();
		});
	Accepted.Session->ReconcileProductionResults();
	Accepted.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DebitHandle);
	const double Deadline = Fixture.World->GetTimeSeconds() + Accepted.Session->GetCooldownRemaining(TEXT("Receipt_Instant"));
	Accepted.Session->ReconcileProductionResults();
	TestEqual(TEXT("Accepted first submission charges once"), Accepted.ASC->GetActualEnergy(), 98.0f);
	TestEqual(TEXT("Reservation closes explicitly"), Accepted.ASC->GetReservedEnergy(), 0.0f);
	TestEqual(TEXT("One real debit callback despite two reentries"), ResourceCallbacks, 1);
	TestEqual(TEXT("One supplemental session report"), Reports, 1);
	TestEqual(TEXT("Existing Runtime channel receives one distinct supplement"), SupplementalEvents, 1);
	TestEqual(TEXT("Waiting original Cast ends without replay"), Fixture.ActiveCast(Accepted), 0);
	TestFalse(TEXT("Confirmed reply clears finite waiting timer"), Fixture.IsWaiting(Accepted));
	TestFalse(TEXT("Confirmed reply lifts same-skill isolation"), Accepted.Session->BlocksSkill(TEXT("Receipt_Instant")));
	TestEqual(TEXT("Real healing was never replayed"), Accepted.ASC->GetCombatAttributeSet()->GetHealth(), 51.0f);
	TestTrue(TEXT("Unknown acceptance time starts cooldown at confirmation"), FMath::IsNearlyEqual(Deadline, Fixture.World->GetTimeSeconds() + 20.0));
	TestEqual(TEXT("New same-skill request rechecks cooldown"), Fixture.Start(Accepted).Reason, FName(TEXT("Cooldown")));
	Accepted.Session->OnSubmissionConfirmed.Remove(ReportHandle);
	Accepted.Runtime->OnNativeSkillEvent.Remove(EventHandle);

	auto Rejected = Fixture.MakeUnit(FVector(200, 0, 0));
	TestTrue(TEXT("Rejected late reply formally activates"), Fixture.Start(Rejected).bSuccess);
	if (!Fixture.Begin(Rejected)) return false;
	TestFalse(TEXT("Actual executor rejects target"), Fixture.Apply(Rejected, nullptr).bEffectApplied);
	Fixture.LoseReturn(Rejected);
	bool bReportedFailure = false;
	const auto RejectedReport = Rejected.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation& Result)
	{ bReportedFailure = !Result.bActionSucceeded && Result.bResourceResolved && !Result.bCooldownRecorded && !Result.bCostCommitted
		&& Result.Reason == TEXT("SubmissionConfirmedRejected"); });
	Rejected.Session->ReconcileProductionResults();
	TestTrue(TEXT("Actual rejected result has a distinct supplement"), bReportedFailure);
	TestEqual(TEXT("Never-debited rejection releases reservation once"), Rejected.ASC->GetReservedEnergy(), 0.0f);
	TestEqual(TEXT("No synthetic refund or debit on rejection"), Rejected.ASC->GetActualEnergy(), 100.0f);
	TestEqual(TEXT("Rejected first point starts no cooldown"), Rejected.Session->GetCooldownRemaining(TEXT("Receipt_Instant")), 0.0f);
	TestFalse(TEXT("Explicit failure removes isolation"), Rejected.Session->HasUnconfirmed());
	Rejected.Session->OnSubmissionConfirmed.Remove(RejectedReport);

	auto History = Fixture.MakeUnit(FVector(400, 0, 0));
	TestTrue(TEXT("History fixture starts"), Fixture.Start(History).bSuccess);
	if (!Fixture.Begin(History)) return false;
	TestTrue(TEXT("History has real accepted producer"), Fixture.Apply(History, History.Actor).bEffectApplied);
	TestTrue(TEXT("Normal synchronous resolution closes resource history"), History.Runtime->ResolveBehaviorSubmission(History.Behavior, History.Cast, 1, true));
	const float Before = History.Session->GetCooldownRemaining(TEXT("Receipt_Instant"));
	Fixture.Retain(History, History.Production); // 模拟资源已闭合、交付记录延后消费。
	History.Session->ReconcileProductionResults();
	TestEqual(TEXT("Confirmed history is reused without another debit"), History.ASC->GetActualEnergy(), 98.0f);
	TestEqual(TEXT("Already-delivered cooldown is not restarted"), History.Session->GetCooldownRemaining(TEXT("Receipt_Instant")), Before);

	auto Paid = Fixture.MakeUnit(FVector(600, 0, 0));
	auto TargetA = Fixture.MakeUnit(FVector(650, 0, 0));
	auto TargetB = Fixture.MakeUnit(FVector(700, 0, 0));
	TestTrue(TEXT("Paid fixture activates through normal target request"), Fixture.Start(Paid, TEXT("Receipt_Any"), TargetA.Actor).bSuccess);
	if (!Fixture.Begin(Paid)) return false;
	TestTrue(TEXT("Paid first real GE accepted"), Fixture.Apply(Paid, TargetA.Actor).bEffectApplied);
	TestTrue(TEXT("First payment closes"), Paid.Runtime->ResolveBehaviorSubmission(Paid.Behavior, Paid.Cast, 1, true));
	const auto FirstOperation = Paid.Production;
	const float PaidDeadline = Paid.Session->GetCooldownRemaining(TEXT("Receipt_Any"));
	if (!Fixture.Begin(Paid, 2)) return false;
	TestEqual(TEXT("Later paid point has no transaction"), Paid.Production.Key.Transaction, uint64(0));
	TestTrue(TEXT("Later point still has independent nonzero operation"), Paid.Production.Key.Operation != FirstOperation.Key.Operation);
	TestTrue(TEXT("Different target real GE accepted for API-boundary coverage"), Fixture.Apply(Paid, TargetB.Actor).bEffectApplied);
	Fixture.LoseReturn(Paid);
	Fixture.Retain(Paid, FirstOperation);
	bool bStillBlockedBeforeAll = false;
	const auto PaidReport = Paid.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation& Result)
	{ if (Result.Key.Operation == Paid.Production.Key.Operation) bStillBlockedBeforeAll = Paid.Session->BlocksSkill(TEXT("Receipt_Any")); });
	Paid.Session->ReconcileProductionResults();
	TestTrue(TEXT("Other unresolved original point still holds isolation during first report"), bStillBlockedBeforeAll);
	TestFalse(TEXT("Isolation ends only after all retained points converge"), Paid.Session->BlocksSkill(TEXT("Receipt_Any")));
	TestEqual(TEXT("Transaction-zero point never recharges"), Paid.ASC->GetActualEnergy(), 98.0f);
	TestEqual(TEXT("No second cooldown for already-paid point"), Paid.Session->GetCooldownRemaining(TEXT("Receipt_Any")), PaidDeadline);
	Paid.Session->OnSubmissionConfirmed.Remove(PaidReport);

	// 真实扣费委托中的原生旁路故障注入，仅证明未知状态不被误消费。
	auto Unknown = Fixture.MakeUnit(FVector(900, 0, 0));
	TestTrue(TEXT("Resource unknown fixture starts"), Fixture.Start(Unknown).bSuccess);
	if (!Fixture.Begin(Unknown)) return false;
	TestTrue(TEXT("Resource unknown has known real action"), Fixture.Apply(Unknown, Unknown.Actor).bEffectApplied);
	Fixture.LoseReturn(Unknown);
	bool bInjected = false;
	AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
	const auto UnknownHandle = Unknown.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (bInjected || Data.NewValue >= Data.OldValue) return;
			bInjected = true;
			Unknown.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 50.0f);
		});
	int32 UnknownReports = 0;
	const auto UnknownReport = Unknown.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation&) { ++UnknownReports; });
	Unknown.Session->ReconcileProductionResults();
	Unknown.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(UnknownHandle);
	Unknown.Session->ReconcileProductionResults();
	TestEqual(TEXT("Known action cannot confirm uncertain debit"), Unknown.ASC->QueryEnergySettlement(Unknown.Production.Key.Transaction).State,
		ESkillLabEnergySettlementState::DebitAttemptUnconfirmed);
	TestTrue(TEXT("Resource unknown keeps same-skill isolation"), Unknown.Session->HasUnconfirmed());
	TestEqual(TEXT("Unknown debit keeps reservation"), Unknown.ASC->GetReservedEnergy(), 2.0f);
	TestEqual(TEXT("Unknown debit is not repeated or compensated"), Unknown.ASC->GetActualEnergy(), 50.0f);
	TestEqual(TEXT("No success report from action fact alone"), UnknownReports, 0);
	TestEqual(TEXT("No cooldown from action fact alone"), Unknown.Session->GetCooldownRemaining(TEXT("Receipt_Instant")), 0.0f);
	Unknown.Session->OnSubmissionConfirmed.Remove(UnknownReport);

	for (const bool bDestroyActor : {true, false})
	{
		auto LostReceiver = Fixture.MakeUnit(FVector(1500, 0, 0));
		TestTrue(TEXT("Late lifetime fixture activates"), Fixture.Start(LostReceiver).bSuccess);
		if (!Fixture.Begin(LostReceiver)) return false;
		TestTrue(TEXT("Late lifetime fixture has a real accepted GE"), Fixture.Apply(LostReceiver, LostReceiver.Actor).bEffectApplied);
		Fixture.LoseReturn(LostReceiver);
		int32 LifetimeReports = 0, LifetimeDebits = 0;
		const auto LifetimeReport = LostReceiver.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation&) { ++LifetimeReports; });
		FDelegateHandle LifetimeHandle;
		LifetimeHandle = LostReceiver.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
			[&](const FOnAttributeChangeData& Data)
			{
				if (Data.NewValue >= Data.OldValue) return;
				++LifetimeDebits;
				LostReceiver.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(LifetimeHandle);
				if (bDestroyActor) LostReceiver.Actor->Destroy();
				else LostReceiver.Session->EndPlay(EEndPlayReason::RemovedFromWorld);
			});
		AddExpectedMessagePlain(FString::Printf(TEXT("SubmissionDeliveryUnavailable Operation=1 ActionSucceeded=1 ResourceResolved=%d CooldownRecorded=0"), bDestroyActor ? 0 : 1),
			ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
		LostReceiver.Session->ReconcileProductionResults();
		TestEqual(TEXT("Late destruction never supplements complete success"), LifetimeReports, 0);
		TestEqual(TEXT("Late destruction attempts exactly one real debit"), LifetimeDebits, 1);
		if (!bDestroyActor)
		{
			TestEqual(TEXT("Ended late receiver retains confirmed resource fact"), LostReceiver.ASC->GetActualEnergy(), 98.0f);
			TestEqual(TEXT("Ended late receiver does not fabricate cooldown"), LostReceiver.Session->GetCooldownRemaining(TEXT("Receipt_Instant")), 0.0f);
			TestFalse(TEXT("Ended late receiver does not reopen closed reservation"), LostReceiver.ASC->IsEnergyEscrowActive());
		}
		LostReceiver.Session->OnSubmissionConfirmed.Remove(LifetimeReport);
	}

	auto NotificationEnd = Fixture.MakeUnit(FVector(1800, 0, 0));
	TestTrue(TEXT("Notification lifetime fixture activates"), Fixture.Start(NotificationEnd).bSuccess);
	if (!Fixture.Begin(NotificationEnd)) return false;
	Fixture.Apply(NotificationEnd, NotificationEnd.Actor);
	Fixture.LoseReturn(NotificationEnd);
	int32 FinalReports = 0;
	const auto EndOnReport = NotificationEnd.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation&)
	{ ++FinalReports; NotificationEnd.Session->EndPlay(EEndPlayReason::RemovedFromWorld); });
	NotificationEnd.Session->ReconcileProductionResults();
	NotificationEnd.Session->ReconcileProductionResults();
	TestEqual(TEXT("Receiver ending during notification cannot cause a second supplement"), FinalReports, 1);
	TestEqual(TEXT("Notification lifetime does not compensate closed cost"), NotificationEnd.ASC->GetActualEnergy(), 98.0f);
	NotificationEnd.Session->OnSubmissionConfirmed.Remove(EndOnReport);

	// Closed producer history remains queryable while only unresolved/undelivered records stay active.
	auto LongHistory = Fixture.MakeUnit(FVector(2100, 0, 0));
	TArray<FSkillLabSubmissionKey> ClosedKeys;
	auto MakeRecord = [&](const uint64 PointId, const bool bConfirmNow)
	{
		FSkillLabSubmissionKey Seed;
		Seed.Session = FObjectKey(LongHistory.Session);
		Seed.ResourceOwner = FObjectKey(LongHistory.ASC);
		Seed.Runtime = FObjectKey(LongHistory.Runtime);
		Seed.Generation = 9001;
		Seed.Cast = 7001;
		Seed.Point = PointId;
		Seed.SkillId = TEXT("Receipt_Any");
		const FSkillLabSubmissionKey Key = LongHistory.Session->ProductionResults.Begin(
			Seed, ESkillLabProductionKind::Effect, 0.0f, 0.0f, true, Fixture.World);
		if (bConfirmNow) LongHistory.Session->ProductionResults.Confirm(Key, true, TEXT("HistoryFixture"), Fixture.World->GetTimeSeconds());
		FSkillLabUnconfirmedSubmission Record;
		Record.SkillId = Key.SkillId;
		Record.RuntimeIdentity = Key.Runtime;
		Record.RuntimeGeneration = Key.Generation;
		Record.CastId = Key.Cast;
		Record.PointId = Key.Point;
		Record.ResultSourceId = TEXT("SkillLabProduction");
		Record.bAlreadyCommitted = true;
		Record.ProductionKey = Key;
		LongHistory.Session->Record(Record);
		return Key;
	};
	for (uint64 Point = 1; Point <= 128; ++Point) ClosedKeys.Add(MakeRecord(Point, true));
	bool bAddedDuringFeedback = false;
	const auto AddDuringFeedback = LongHistory.Session->OnSubmissionConfirmed.AddLambda(
		[&](const FSkillLabSubmissionConfirmation&)
		{
			if (bAddedDuringFeedback) return;
			bAddedDuringFeedback = true;
			MakeRecord(1000, false);
		});
	LongHistory.Session->ReconcileProductionResults();
	LongHistory.Session->OnSubmissionConfirmed.Remove(AddDuringFeedback);
	TestTrue(TEXT("Reentrant record is appended outside the current active-index snapshot"), bAddedDuringFeedback);
	TestEqual(TEXT("Long closed history leaves only the reentrant unresolved operation active"), LongHistory.Session->ActiveRecordIndices.Num(), 1);
	TestEqual(TEXT("Closed producer history remains available by full identity"),
		LongHistory.Session->GetProductionResults().Query(ClosedKeys.Last()).State, ESkillLabProductionState::ConfirmedAccepted);
	TestEqual(TEXT("Closed records remain available to existing indexed readers"), LongHistory.Session->GetRecords().Num(), 129);
	TArray<FSkillLabSubmissionKey> PendingKeys;
	PendingKeys.Add(LongHistory.Session->GetRecords().Last().ProductionKey);
	for (uint64 Point = 1001; Point <= 1127; ++Point) PendingKeys.Add(MakeRecord(Point, false));
	LongHistory.Session->ReconcileProductionResults();
	TestEqual(TEXT("Long unresolved set is retained while no producer result exists"), LongHistory.Session->ActiveRecordIndices.Num(), 128);
	TestTrue(TEXT("Unresolved records continue to isolate their skill"), LongHistory.Session->BlocksSkill(TEXT("Receipt_Any")));
	TestTrue(TEXT("Unresolved records remain visible during convergence"), LongHistory.Session->HasUnconfirmed());
	TestEqual(TEXT("Full record history is not compacted while entries are active"), LongHistory.Session->GetRecords().Num(), 256);
	for (const FSkillLabSubmissionKey& Key : PendingKeys)
		LongHistory.Session->ProductionResults.Confirm(Key, true, TEXT("ReentrantResolved"), Fixture.World->GetTimeSeconds());
	LongHistory.Session->ReconcileProductionResults();
	TestEqual(TEXT("Confirmed records leave the active scan set together"), LongHistory.Session->ActiveRecordIndices.Num(), 0);
	TestFalse(TEXT("Converged records no longer block skill requests"), LongHistory.Session->HasUnconfirmed());
	TestEqual(TEXT("Reentrant producer result remains queryable after convergence"),
		LongHistory.Session->GetProductionResults().Query(PendingKeys.Last()).State, ESkillLabProductionState::ConfirmedAccepted);
	return true;
}
#endif
