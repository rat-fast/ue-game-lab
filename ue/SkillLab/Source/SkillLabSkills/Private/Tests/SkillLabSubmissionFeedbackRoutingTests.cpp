#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabProductionFixture.h"
#include "SkillLabSubmissionFeedbackProbe.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	bool RequestWithSettlementDeferred(FSkillLabProductionFixture& Fixture, FSkillLabProductionFixture::FUnit& Unit,
		const bool bAccept)
	{
		if (!Fixture.Start(Unit).bSuccess) return false;
		// 在真实 GE 查询内暂挂现有 reservation；正式动作及 Resolve 均运行，不丢弃返回或注入最终结果。
		FGameplayEffectApplicationQuery Query;
		Query.BindLambda([&](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&)
		{
			Unit.Production = Unit.Runtime->GetProductionHandle(Unit.Behavior, Unit.Cast);
			Fixture.Test->TestTrue(TEXT("Real GE query temporarily defers untouched reservation"),
				Unit.ASC->MarkEnergyEscrowIndeterminate(Unit.Production.Key.Transaction));
			return bAccept;
		});
		Unit.ASC->GameplayEffectApplicationQueries.Add(Query);
		Fixture.Test->AddExpectedError(TEXT("Submission escrow did not close"), EAutomationExpectedErrorFlags::Contains, 1);
		const bool bReturned = Unit.Behavior->RequestDeclaredAction();
		Unit.ASC->GameplayEffectApplicationQueries.Reset();
		Fixture.Test->TestFalse(TEXT("Formal request reports unresolved settlement, not complete acceptance"), bReturned);
		Fixture.Test->TestTrue(TEXT("Formal Resolve retains pending result"), Unit.Behavior->IsActionPending());
		return Unit.Production.Key.IsSet() && Unit.Session->HasUnconfirmed();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSubmissionFeedbackRoutingTest, "SkillLab.Skills.SubmissionFeedbackRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabSubmissionFeedbackRoutingTest::RunTest(const FString&)
{
	for (const bool bAccept : {true, false})
	{
		// 原等待、原 Runtime 新 Cast、替换 Runtime 新 Cast，以及重建间隙先结算后交付。
		for (const int32 Mode : {0, 1, 2, 3})
		{
			FSkillLabProductionFixture Fixture(this);
			if (!Fixture.bReady) return false;
			auto Unit = Fixture.MakeUnit();
			Unit.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 50.0f);
			if (!RequestWithSettlementDeferred(Fixture, Unit, bAccept)) return false;
			const auto Original = Unit.Production.Key;
			TestEqual(TEXT("Real producer supplies final action fact before delayed settlement"), Unit.Session->GetProductionResults().Query(Original).State,
				bAccept ? ESkillLabProductionState::ConfirmedAccepted : ESkillLabProductionState::ConfirmedRejected);
			int32 OldReports = 0;
			auto* OldRuntime = Unit.Runtime;
			const auto OldHandle = OldRuntime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
			{ if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) ++OldReports; });
			if (Mode >= 2)
			{
				Unit.Runtime->DestroyComponent();
				if (Mode == 3)
				{
					Unit.Session->ReconcileProductionResults();
					Unit.Session->ReconcileProductionResults();
					TestTrue(TEXT("Settlement can close in Runtime replacement gap"), Unit.Session->GetRecords().Last().bResolved);
					TestFalse(TEXT("Absent feedback receiver does not consume notification"), Unit.Session->GetRecords().Last().bNotified);
				}
				Fixture.RebuildRuntime(Unit);
			}
			else if (Mode == 1) Unit.Runtime->CancelActiveCast(TEXT("FeedbackTestNextCast"));
			if (Mode != 0) TestTrue(TEXT("Current Runtime starts unrelated formal Cast"), Fixture.Start(Unit, TEXT("Receipt_Other")).bSuccess);
			const int32 NewCast = Fixture.ActiveCast(Unit);
			TStrongObjectPtr<USkillLabSubmissionFeedbackProbe> Probe(NewObject<USkillLabSubmissionFeedbackProbe>());
			Unit.Runtime->OnSkillEvent.AddDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
			Probe->OnResolved = [&] { Unit.Session->ReconcileProductionResults(); };
			int32 NativeReports = 0, SessionReports = 0, Debits = 0;
			FSkillLabSkillResult LastNative;
			const auto NativeHandle = Unit.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
			{
				if (Result.Event != ESkillLabSkillEvent::SubmissionResolved) return;
				++NativeReports;
				LastNative = Result;
				Unit.Session->ReconcileProductionResults();
			});
			const auto SessionHandle = Unit.Session->OnSubmissionConfirmed.AddLambda([&](const FSkillLabSubmissionConfirmation& Confirmation)
			{
				++SessionReports;
				TestTrue(TEXT("Feedback retains complete original production identity after rebuild"), Confirmation.Key == Original);
				TestEqual(TEXT("Feedback has actual action acceptance"), Confirmation.bActionSucceeded, bAccept);
				TestTrue(TEXT("Feedback has closed resource fact"), Confirmation.bResourceResolved);
				TestEqual(TEXT("Feedback has actual cooldown fact"), Confirmation.bCooldownRecorded, bAccept);
				Unit.Session->ReconcileProductionResults();
			});
			const auto DebitHandle = Unit.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
				[&](const FOnAttributeChangeData& Data)
				{ if (Data.NewValue < Data.OldValue) { ++Debits; Unit.Session->ReconcileProductionResults(); } });
			Unit.Session->ReconcileProductionResults();
			Unit.Session->ReconcileProductionResults();
			TestEqual(TEXT("Blueprint feedback channel receives original result exactly once"), Probe->Count, 1);
			TestEqual(TEXT("Native feedback channel receives original result exactly once"), NativeReports, 1);
			TestEqual(TEXT("Session observer receives original result exactly once"), SessionReports, 1);
			TestTrue(TEXT("Dynamic Runtime feedback carries complete original key"), Probe->Last.SubmissionKey == Original);
			TestTrue(TEXT("Native Runtime feedback carries complete original key"), LastNative.SubmissionKey == Original);
			TestEqual(TEXT("Runtime feedback carries original real request id"), LastNative.RequestId, Original.RequestId);
			TestEqual(TEXT("Public feedback retains original Cast id even on replacement Cast collision"), LastNative.CastId, static_cast<int32>(Original.Cast));
			TestEqual(TEXT("Public feedback retains original Skill id"), LastNative.SkillId, Original.SkillId);
			TestEqual(TEXT("Public feedback retains original point"), LastNative.SubmissionPointId, static_cast<int64>(Original.Point));
			TestEqual(TEXT("Public feedback reports actual result"), LastNative.bSuccess, bAccept);
			TestEqual(TEXT("Public feedback reports actual fee"), LastNative.bCostCommitted, bAccept);
			TestEqual(TEXT("Public feedback supplies distinct final reason"), LastNative.Reason,
				FName(bAccept ? TEXT("SubmissionConfirmedAccepted") : TEXT("SubmissionConfirmedRejected")));
			TestEqual(TEXT("Matching original wait ends; unrelated and replacement Casts survive"), Fixture.ActiveCast(Unit), Mode == 0 ? 0 : NewCast);
			TestEqual(TEXT("Current unrelated Skill identity survives supplemental old result"), Unit.Runtime->GetActiveSkillId(),
				Mode == 0 ? NAME_None : FName(TEXT("Receipt_Other")));
			TestEqual(TEXT("Destroyed old Runtime never receives late supplement"), OldReports, Mode >= 2 ? 0 : 1);
			TestEqual(TEXT("Settled actual energy matches real action"), Unit.ASC->GetActualEnergy(), bAccept ? 98.0f : 100.0f);
			TestEqual(TEXT("Only one debit across repeated queries and feedback reentry"), Debits, bAccept && Mode != 3 ? 1 : 0);
			TestEqual(TEXT("Real GE is never replayed by feedback routing"), Unit.ASC->GetCombatAttributeSet()->GetHealth(), bAccept ? 51.0f : 50.0f);
			TestFalse(TEXT("Final result lifts same-skill isolation"), Unit.Session->HasUnconfirmed());
			TestTrue(TEXT("Notification marked only after actual feedback dispatch"), Unit.Session->GetRecords().Last().bNotified);
			Unit.Runtime->OnSkillEvent.RemoveDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
			Unit.Runtime->OnNativeSkillEvent.Remove(NativeHandle);
			OldRuntime->OnNativeSkillEvent.Remove(OldHandle);
			Unit.Session->OnSubmissionConfirmed.Remove(SessionHandle);
			Unit.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DebitHandle);
			UE_LOG(LogTemp, Log, TEXT("[SkillLabReviewRegression] FormalFeedback Accepted=%d Mode=%d OriginalCast=%llu CurrentCast=%d Dynamic=%d Native=%d Session=%d"),
				bAccept, Mode, Original.Cast, Fixture.ActiveCast(Unit), Probe->Count, NativeReports, SessionReports);
		}
	}

	// 结束原等待所发出的取消事件销毁 Runtime：最终补报尚未发出，保留至有效替代入口。
	{
		FSkillLabProductionFixture PendingFixture(this);
		if (!PendingFixture.bReady) return false;
		auto PendingUnit = PendingFixture.MakeUnit();
		if (!RequestWithSettlementDeferred(PendingFixture, PendingUnit, true)) return false;
		int32 PrematureReports = 0;
		auto* EndedRuntime = PendingUnit.Runtime;
		const auto EndHandle = EndedRuntime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
		{
			if (Result.Event == ESkillLabSkillEvent::Cancelled) EndedRuntime->DestroyComponent();
			if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) ++PrematureReports;
		});
		PendingUnit.Session->ReconcileProductionResults();
		TestEqual(TEXT("Receiver lost during original Cast cleanup gets no final supplement"), PrematureReports, 0);
		TestFalse(TEXT("Undelivered final supplement remains pending after cleanup callback"), PendingUnit.Session->GetRecords().Last().bNotified);
		EndedRuntime->OnNativeSkillEvent.Remove(EndHandle);
		PendingFixture.RebuildRuntime(PendingUnit);
		TestTrue(TEXT("New receiver starts new Cast before pending feedback"), PendingFixture.Start(PendingUnit, TEXT("Receipt_Other")).bSuccess);
		const int32 NewCast = PendingFixture.ActiveCast(PendingUnit);
		int32 FinalReports = 0;
		const auto FinalHandle = PendingUnit.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
		{ if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) ++FinalReports; });
		PendingUnit.Session->ReconcileProductionResults();
		PendingUnit.Session->ReconcileProductionResults();
		TestEqual(TEXT("Replacement receives exactly one previously undelivered final supplement"), FinalReports, 1);
		TestEqual(TEXT("Pending feedback does not terminate replacement Cast"), PendingFixture.ActiveCast(PendingUnit), NewCast);
		TestEqual(TEXT("Pending feedback retries never repeat fee"), PendingUnit.ASC->GetActualEnergy(), 98.0f);
		PendingUnit.Runtime->OnNativeSkillEvent.Remove(FinalHandle);
	}

	// 动态反馈先到后销毁 Runtime：不再调用该 Runtime 的原生接收方，也不重发已送达的结果。
	FSkillLabProductionFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	if (!RequestWithSettlementDeferred(Fixture, Unit, true)) return false;
	TStrongObjectPtr<USkillLabSubmissionFeedbackProbe> Probe(NewObject<USkillLabSubmissionFeedbackProbe>());
	Unit.Runtime->OnSkillEvent.AddDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
	Probe->OnResolved = [&] { Unit.Runtime->DestroyComponent(); Unit.Session->ReconcileProductionResults(); };
	int32 InvalidNativeReports = 0;
	const auto Handle = Unit.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{ if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) ++InvalidNativeReports; });
	Unit.Session->ReconcileProductionResults();
	Unit.Session->ReconcileProductionResults();
	TestEqual(TEXT("Dynamic feedback is delivered before receiver destruction"), Probe->Count, 1);
	TestEqual(TEXT("Runtime destroyed by dynamic feedback never receives native supplement"), InvalidNativeReports, 0);
	Unit.Runtime->OnNativeSkillEvent.Remove(Handle);
	Unit.Runtime->OnSkillEvent.RemoveDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
	Fixture.RebuildRuntime(Unit);
	Unit.Session->ReconcileProductionResults();
	TestTrue(TEXT("Delivered feedback stays consumed after callback destroys and rebuilds Runtime"), Unit.Session->GetRecords().Last().bNotified);
	TestEqual(TEXT("Feedback destruction never compensates committed fee"), Unit.ASC->GetActualEnergy(), 98.0f);
	return true;
}
#endif
