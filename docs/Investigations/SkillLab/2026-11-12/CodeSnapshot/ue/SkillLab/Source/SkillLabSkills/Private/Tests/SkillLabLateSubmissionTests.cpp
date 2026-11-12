#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabProductionFixture.h"
#include "HAL/PlatformTime.h"

class FSkillLabLateSubmissionCommand final : public IAutomationLatentCommand
{
public:
	explicit FSkillLabLateSubmissionCommand(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (!Fixture)
		{
			Fixture = MakeUnique<FSkillLabProductionFixture>(Test);
			if (!Fixture->bReady || !Setup()) return true;
			Started = LastTick = FPlatformTime::Seconds();
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		Fixture->World->Tick(LEVELTICK_All, static_cast<float>(Now - LastTick));
		LastTick = Now;
		if (Now - Started < 1.1) return false;
		Finish();
		return true;
	}
private:
	bool Setup()
	{
		Accepted = Fixture->MakeUnit();
		Rejected = Fixture->MakeUnit(FVector(200, 0, 0));
		Empty = Fixture->MakeUnit(FVector(400, 0, 0));
		Expired = Fixture->MakeUnit(FVector(600, 0, 0));
		Unknown = Fixture->MakeUnit(FVector(800, 0, 0));
		Reset = Fixture->MakeUnit(FVector(1000, 0, 0));
		for (auto* Unit : {&Accepted, &Rejected, &Unknown, &Reset})
		{
			if (!Fixture->Start(*Unit).bSuccess || !Fixture->Begin(*Unit)) return false;
			if (Unit != &Unknown)
				Fixture->Apply(*Unit, Unit == &Rejected ? nullptr : Unit->Actor);
			Fixture->LoseReturn(*Unit);
			// 故障注入只延迟生产结果的查询/交付；真实执行及来源记录早已完成。
			Unit->Session->SetComponentTickEnabled(false);
		}
		for (auto* Unit : {&Empty, &Expired})
		{
			if (!Fixture->Start(*Unit, Unit == &Empty ? TEXT("Receipt_Area") : TEXT("Receipt_ShortArea"), nullptr,
				FVector(2500, 0, 0)).bSuccess || !Fixture->Begin(*Unit)) return false;
			if (!Fixture->Execute(*Unit)) return false;
			Fixture->LoseReturn(*Unit);
			Unit->Session->SetComponentTickEnabled(false);
		}
		EmptyAcceptance = Empty.Session->GetProductionResults().Query(Empty.Production.Key).AcceptedAt.GetValue();
		Reset.Runtime->ResetSkills();
		Test->TestTrue(TEXT("Reset cannot discard pending real producer record"), Reset.Session->HasUnconfirmed());
		Test->TestEqual(TEXT("Blocked Reset does not restore over reservation"), Reset.ASC->GetReservedEnergy(), 2.0f);
		return true;
	}
	void Finish()
	{
		for (auto* Unit : {&Accepted, &Rejected, &Empty, &Expired, &Unknown})
		{
			Test->TestEqual(TEXT("True finite timeout ends original Cast before delayed query"), Fixture->ActiveCast(*Unit), 0);
			Test->TestFalse(TEXT("True finite timeout releases wait timer"), Fixture->IsWaiting(*Unit));
			Test->TestTrue(TEXT("Unqueried result remains isolated after abnormal terminal"), Unit->Session->HasUnconfirmed());
		}
		Test->TestEqual(TEXT("Same skill explicitly rejects while delayed"), Fixture->Start(Accepted).Reason, FName(TEXT("SubmissionUnconfirmed")));
		Test->TestTrue(TEXT("Other skill can activate on unreserved balance"), Fixture->Start(Accepted, TEXT("Receipt_Other")).bSuccess);
		const int32 NewCast = Fixture->ActiveCast(Accepted);
		const double ConfirmationTime = Fixture->World->GetTimeSeconds();
		int32 AcceptedReports = 0;
		const auto AcceptedReport = Accepted.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
		{ if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) { ++AcceptedReports; Test->TestTrue(TEXT("Late original success is reported"), Result.bSuccess); } });
		Accepted.Session->ReconcileProductionResults();
		Accepted.Session->ReconcileProductionResults();
		Test->TestEqual(TEXT("After real timeout original feedback receives one supplement"), AcceptedReports, 1);
		Accepted.Runtime->OnNativeSkillEvent.Remove(AcceptedReport);
		Test->TestEqual(TEXT("Late original receipt cannot terminate current other Cast"), Fixture->ActiveCast(Accepted), NewCast);
		Test->TestEqual(TEXT("Late first acceptance charges once"), Accepted.ASC->GetActualEnergy(), 98.0f);
		Test->TestTrue(TEXT("Unavailable acceptance time uses later confirmation clock"), FMath::IsNearlyEqual(
			ConfirmationTime + Accepted.Session->GetCooldownRemaining(TEXT("Receipt_Instant")), ConfirmationTime + 20.0, 0.01));
		Fixture->RebuildRuntime(Rejected);
		Test->TestTrue(TEXT("Replacement Runtime can activate unrelated skill"), Fixture->Start(Rejected, TEXT("Receipt_Other")).bSuccess);
		const int32 ReplacementCast = Fixture->ActiveCast(Rejected);
		int32 RejectedReports = 0;
		const auto RejectedReport = Rejected.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
		{ if (Result.Event == ESkillLabSkillEvent::SubmissionResolved) { ++RejectedReports; Test->TestFalse(TEXT("Late replacement failure is reported"), Result.bSuccess); } });
		Rejected.Session->ReconcileProductionResults();
		Rejected.Session->ReconcileProductionResults();
		Test->TestEqual(TEXT("After real timeout replacement feedback receives one supplement"), RejectedReports, 1);
		Rejected.Runtime->OnNativeSkillEvent.Remove(RejectedReport);
		Test->TestEqual(TEXT("Original receipt cannot affect replacement Runtime Cast"), Fixture->ActiveCast(Rejected), ReplacementCast);
		Test->TestEqual(TEXT("Late real rejection releases original reservation"), Rejected.ASC->GetReservedEnergy(), 0.0f);
		Test->TestEqual(TEXT("Late rejection does not alter actual energy"), Rejected.ASC->GetActualEnergy(), 100.0f);
		Empty.Session->ReconcileProductionResults();
		const double OriginalDeadline = EmptyAcceptance + 20.0;
		Test->TestTrue(TEXT("Trusted acceptance time retains original deadline after abnormal terminal"), FMath::IsNearlyEqual(
			Fixture->World->GetTimeSeconds() + Empty.Session->GetCooldownRemaining(TEXT("Receipt_Area")), OriginalDeadline, 0.01));
		Empty.Session->ReconcileProductionResults();
		Test->TestEqual(TEXT("Repeated late query does not recharge"), Empty.ASC->GetActualEnergy(), 98.0f);
		Expired.Session->ReconcileProductionResults();
		Test->TestEqual(TEXT("Already elapsed original cooldown is not restarted"), Expired.Session->GetCooldownRemaining(TEXT("Receipt_ShortArea")), 0.0f);
		Test->TestFalse(TEXT("Elapsed cooldown still lifts original isolation"), Expired.Session->HasUnconfirmed());
		Test->TestTrue(TEXT("New request re-enters full gate after elapsed cooldown"), Fixture->Start(Expired, TEXT("Receipt_ShortArea"), nullptr, FVector(2500, 0, 0)).bSuccess);
		Unknown.Session->ReconcileProductionResults();
		Test->TestTrue(TEXT("No production return remains unknown after timeout"), Unknown.Session->HasUnconfirmed());
		Test->TestEqual(TEXT("No-result reservation remains, without fabricated fee"), Unknown.ASC->GetActualEnergy(), 100.0f);
		Test->TestEqual(TEXT("No-result reservation survives"), Unknown.ASC->GetReservedEnergy(), 2.0f);
		Reset.Session->ReconcileProductionResults();
		Test->TestEqual(TEXT("Same-ASC receipt converges after blocked Reset"), Reset.ASC->GetActualEnergy(), 98.0f);
		Test->TestFalse(TEXT("Reset does not permanently block confirmed result"), Reset.Session->HasUnconfirmed());
		UE_LOG(LogTemp, Log, TEXT("[SkillLabLateSubmission] RealProducerLateQuery NewCast=%d ReplacementCast=%d OriginalDeadline=%.3f ConfirmedAt=%.3f UnknownStillIsolated=1"),
			NewCast, ReplacementCast, OriginalDeadline, ConfirmationTime);
	}
	FAutomationTestBase* Test;
	TUniquePtr<FSkillLabProductionFixture> Fixture;
	FSkillLabProductionFixture::FUnit Accepted, Rejected, Empty, Expired, Unknown, Reset;
	double Started = 0.0, LastTick = 0.0, EmptyAcceptance = 0.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabLateSubmissionTest, "SkillLab.Skills.LateSubmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabLateSubmissionTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSkillLabLateSubmissionCommand(this));
	return true;
}
#endif
