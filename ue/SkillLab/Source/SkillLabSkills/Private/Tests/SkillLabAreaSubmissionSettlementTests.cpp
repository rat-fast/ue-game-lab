#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabProductionFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabAreaSubmissionSettlementTest, "SkillLab.Skills.AreaSubmissionSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabAreaSubmissionSettlementTest::RunTest(const FString&)
{
	// 各用例独立 World；真实查询、GE、动作返回和正式 Resolve，不手工 Abort 或丢弃返回。
	for (const int32 Mode : {0, 1, 2, 3})
	{
		FSkillLabProductionFixture Fixture(this);
		if (!Fixture.bReady) return false;
		auto Source = Fixture.MakeUnit();
		auto TargetA = Fixture.MakeUnit(FVector(1000, 0, 0));
		auto TargetB = Fixture.MakeUnit(FVector(1020, 0, 0));
		if (Mode == 2) TargetB.Actor->SetActorLocation(FVector(2000, 0, 0));
		TestTrue(TEXT("Area request activates through formal Ability"), Fixture.Start(Source, TEXT("Receipt_Area"), nullptr, FVector(1000, 0, 0)).bSuccess);
		bool bInterrupted = false;
		int32 GECalls = 0;
		FGameplayEffectApplicationQuery Query;
		Query.BindLambda([&](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&)
		{
			++GECalls;
			Source.Production = Source.Runtime->GetProductionHandle(Source.Behavior, Source.Cast);
			if (Mode != 1 && !bInterrupted)
			{
				bInterrupted = true;
				Source.Runtime->EndPlay(EEndPlayReason::RemovedFromWorld);
			}
			return Mode == 3;
		});
		TargetA.ASC->GameplayEffectApplicationQueries.Add(Query);
		TargetB.ASC->GameplayEffectApplicationQueries.Add(Query);
		if (Mode == 0) AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		const bool bReturned = Source.Behavior->RequestDeclaredAction();
		TargetA.ASC->GameplayEffectApplicationQueries.Reset();
		TargetB.ASC->GameplayEffectApplicationQueries.Reset();
		TestFalse(TEXT("Interrupted or rejected formal action never reports complete success"), bReturned);
		const auto Key = Source.Production.Key;
		const auto Production = Source.Session->GetProductionResults().Query(Key);
		const auto Resource = Source.ASC->QueryEnergySettlement(Key.Transaction);
		if (Mode == 0)
		{
			TestEqual(TEXT("Interrupted real query executes only first planned GE"), GECalls, 1);
			TestEqual(TEXT("Missing second child keeps authoritative aggregate pending"), Production.State, ESkillLabProductionState::InProgress);
			int32 Rejected = 0, Pending = 0;
			for (const auto& Child : Production.Children)
			{
				const auto State = Source.Session->GetProductionResults().Query(Child).State;
				Rejected += State == ESkillLabProductionState::ConfirmedRejected;
				Pending += State == ESkillLabProductionState::InProgress;
			}
			TestEqual(TEXT("Actual first GE rejection remains recorded"), Rejected, 1);
			TestEqual(TEXT("Unexecuted child is never fabricated as failure"), Pending, 1);
			TestEqual(TEXT("Formal Resolve retains never-debited history"), Resource.State, ESkillLabEnergySettlementState::ReservedNoDebit);
			TestTrue(TEXT("Formal Resolve transfers reservation to unresolved ownership"), Source.ASC->HasIndeterminateEnergyEscrow(Key.Transaction));
			TestTrue(TEXT("Formal action reports pending after Runtime EndPlay"), Source.Behavior->IsActionPending());
			TestFalse(TEXT("Pending formal action cannot finish"), Source.Behavior->IsActionCompleted());
			TestEqual(TEXT("Unknown aggregate preserves actual energy"), Source.ASC->GetActualEnergy(), 100.0f);
			TestEqual(TEXT("Unknown aggregate preserves reserved energy"), Source.ASC->GetReservedEnergy(), 2.0f);
			TestEqual(TEXT("Unknown aggregate exposes available energy"), Source.ASC->GetAvailableEnergy(), 98.0f);
			TestTrue(TEXT("Unknown formal submission isolates same skill"), Source.Session->BlocksSkill(Key.SkillId));
			Fixture.RebuildRuntime(Source);
			TestEqual(TEXT("Replacement Runtime rejects same unknown skill"), Fixture.Start(Source, Key.SkillId, nullptr, FVector(1000, 0, 0)).Reason,
				FName(TEXT("SubmissionUnconfirmed")));
			TestTrue(TEXT("Replacement Runtime accepts other skill"), Fixture.Start(Source, TEXT("Receipt_Other")).bSuccess);
			const int32 NewCast = Fixture.ActiveCast(Source);
			Source.Session->ReconcileProductionResults();
			Source.Session->ReconcileProductionResults();
			TestEqual(TEXT("Unknown query never terminates replacement Cast"), Fixture.ActiveCast(Source), NewCast);
			TestTrue(TEXT("Unknown remains isolated after reconstruction and queries"), Source.Session->HasUnconfirmed());
			TestEqual(TEXT("Unknown never releases original reservation"), Source.ASC->GetReservedEnergy(), 2.0f);
			TestEqual(TEXT("Unknown starts no cooldown"), Source.Session->GetCooldownRemaining(Key.SkillId), 0.0f);
		}
		else
		{
			const bool bAccepted = Mode == 3;
			TestEqual(TEXT("Real aggregate has an explicit final production fact"), Production.State,
				bAccepted ? ESkillLabProductionState::ConfirmedAccepted : ESkillLabProductionState::ConfirmedRejected);
			TestEqual(TEXT("Formal settlement uses matching closed resource history"), Resource.State,
				bAccepted ? ESkillLabEnergySettlementState::ConfirmedCommitted : ESkillLabEnergySettlementState::ConfirmedReleased);
			TestEqual(TEXT("Only real partial acceptance incurs cost"), Source.ASC->GetActualEnergy(), bAccepted ? 98.0f : 100.0f);
			TestEqual(TEXT("Explicit final result releases reservation"), Source.ASC->GetReservedEnergy(), 0.0f);
			TestFalse(TEXT("Explicit final result does not manufacture unresolved isolation"), Source.Session->HasUnconfirmed());
			TestEqual(TEXT("All children rejected must execute both real queries"), GECalls, Mode == 1 ? 2 : 1);
			TestEqual(TEXT("Cooldown tracks real acceptance only"), Source.Session->GetCooldownRemaining(Key.SkillId) > 0.0f, bAccepted);
		}
		UE_LOG(LogTemp, Log, TEXT("[SkillLabReviewRegression] FormalArea Mode=%d Calls=%d Production=%d Resource=%d Actual=%.1f Reserved=%.1f Isolated=%d"),
			Mode, GECalls, static_cast<int32>(Production.State), static_cast<int32>(Resource.State),
			Source.ASC->GetActualEnergy(), Source.ASC->GetReservedEnergy(), Source.Session->HasUnconfirmed());
	}
	return true;
}
#endif
