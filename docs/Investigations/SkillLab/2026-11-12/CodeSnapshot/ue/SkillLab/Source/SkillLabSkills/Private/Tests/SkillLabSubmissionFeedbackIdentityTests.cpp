#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabProductionFixture.h"
#include "SkillLabSubmissionFeedbackProbe.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

class FSkillLabSubmissionFeedbackIdentityCommand final : public IAutomationLatentCommand
{
public:
	explicit FSkillLabSubmissionFeedbackIdentityCommand(FAutomationTestBase* InTest) : Test(InTest) {}
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
		if (Now - Started < 0.16) return false;
		Finish();
		return true;
	}
private:
	bool Setup()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			auto& Unit = Units[Index];
			Unit = Fixture->MakeUnit(FVector(Index * 2000, 0, 0));
			Targets[Index] = Fixture->MakeUnit(FVector(Index * 2000 + 1000, 0, 0));
			const bool bAccept = Index == 1;
			if (!Fixture->Start(Unit, TEXT("Receipt_ShortArea"), nullptr, Targets[Index].Actor->GetActorLocation(), 701 + Index).bSuccess) return false;
			FGameplayEffectApplicationQuery Query;
			Query.BindLambda([&](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&)
			{
				Unit.Production = Unit.Runtime->GetProductionHandle(Unit.Behavior, Unit.Cast);
				Test->TestTrue(TEXT("Actual GE query defers untouched reservation before formal settlement"),
					Unit.ASC->MarkEnergyEscrowIndeterminate(Unit.Production.Key.Transaction));
				return bAccept;
			});
			Targets[Index].ASC->GameplayEffectApplicationQueries.Add(Query);
			Test->AddExpectedError(TEXT("Submission escrow did not close"), EAutomationExpectedErrorFlags::Contains, 1);
			Test->TestFalse(TEXT("Formal area action returns pending settlement"), Unit.Behavior->RequestDeclaredAction());
			Targets[Index].ASC->GameplayEffectApplicationQueries.Reset();
			if (!Test->TestTrue(TEXT("Formal Resolve retains original submission"), Unit.Behavior->IsActionPending() && Unit.Session->HasUnconfirmed())) return false;
			Original[Index] = Unit.Production.Key;
			FSkillLabSkillResult OriginalTerminal;
			const auto Handle = Unit.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
			{ if (Result.Event == ESkillLabSkillEvent::Cancelled) OriginalTerminal = Result; });
			Unit.Runtime->DestroyComponent();
			Unit.Runtime->OnNativeSkillEvent.Remove(Handle);
			Test->TestTrue(TEXT("Original terminal already exposes the stable original operation identity"), OriginalTerminal.SubmissionKey == Original[Index]);
			Test->TestEqual(TEXT("Original operation snapshots the actual nondefault request"), Original[Index].RequestId, 701 + Index);
			Unit.Session->SetComponentTickEnabled(false);
			Unit.Session->ReconcileProductionResults();
			Unit.Session->ReconcileProductionResults();
			Test->TestTrue(TEXT("Real result settles in Runtime gap"), Unit.Session->GetRecords().Last().bResolved);
			Test->TestFalse(TEXT("Final result stays undelivered with no Runtime"), Unit.Session->GetRecords().Last().bNotified);
			Test->TestFalse(TEXT("Settlement lifts same-skill isolation before feedback dispatch"), Unit.Session->BlocksSkill(Original[Index].SkillId));
			Test->TestEqual(TEXT("Gap settlement reflects actual success or failure"), Unit.ASC->GetActualEnergy(), bAccept ? 98.0f : 100.0f);
		}
		return true;
	}
	void Finish()
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			auto& Unit = Units[Index];
			const auto Old = Original[Index];
			const bool bAccept = Index == 1;
			Test->TestEqual(TEXT("Original accepted cooldown really expires before same-skill reactivation"), Unit.Session->GetCooldownRemaining(Old.SkillId), 0.0f);
			Fixture->RebuildRuntime(Unit);
			const auto NewRequest = Fixture->Start(Unit, Old.SkillId, nullptr, Targets[Index].Actor->GetActorLocation());
			if (!Test->TestTrue(TEXT("Replacement formally activates identical SkillId after gap settlement"), NewRequest.bSuccess) || !Fixture->Begin(Unit)) return;
			const auto Current = Unit.Production.Key;
			Test->TestEqual(TEXT("Old and new SkillId truly collide"), Current.SkillId, Old.SkillId);
			Test->TestEqual(TEXT("Old and new numeric Cast truly collide"), Current.Cast, Old.Cast);
			Test->TestEqual(TEXT("Old and new submission point truly collide"), Current.Point, Old.Point);
			Test->TestTrue(TEXT("Stable operation identities differ despite numeric collision"), Old.Id.IsValid() && Current.Id.IsValid() && Old.Id != Current.Id);
			Test->TestTrue(TEXT("New Runtime identity is genuinely different"), Current.Runtime != Old.Runtime);
			Test->TestTrue(TEXT("New operation and request are not substituted for original"), Current.Operation != Old.Operation && Current.RequestId != Old.RequestId);
			const float Energy = Unit.ASC->GetActualEnergy(), Reserved = Unit.ASC->GetReservedEnergy();
			const float Health = Targets[Index].ASC->GetCombatAttributeSet()->GetHealth();
			const auto Stage = Unit.Runtime->GetCastStage();
			TStrongObjectPtr<USkillLabSubmissionFeedbackProbe> Probe(NewObject<USkillLabSubmissionFeedbackProbe>());
			Unit.Runtime->OnSkillEvent.AddDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
			int32 NativeReports = 0;
			const auto CheckActualReceiver = [&](const FSkillLabSkillResult& Result, const TCHAR* Channel)
			{
				Test->TestTrue(FString::Printf(TEXT("%s receives the actual original full key"), Channel), Result.SubmissionKey == Old);
				Test->TestTrue(FString::Printf(TEXT("%s can distinguish original operation without current-Runtime lookup"), Channel),
					Result.SubmissionKey.Id == Old.Id && Result.SubmissionKey.Id != Current.Id);
				Test->TestEqual(FString::Printf(TEXT("%s reports original actual RequestId"), Channel), Result.RequestId, Old.RequestId);
				Test->TestEqual(FString::Printf(TEXT("%s reports actual final success"), Channel), Result.bSuccess, bAccept);
				FSkillLabSkillResult ReflectedCopy;
				FSkillLabSkillResult::StaticStruct()->CopyScriptStruct(&ReflectedCopy, &Result);
				Test->TestTrue(FString::Printf(TEXT("%s reflected value copy preserves native identity too"), Channel), ReflectedCopy.SubmissionKey == Old);
				Unit.Session->ReconcileProductionResults();
			};
			Probe->OnResolved = [&] { CheckActualReceiver(Probe->Last, TEXT("OnSkillEvent")); };
			const auto Handle = Unit.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
			{
				if (Result.Event != ESkillLabSkillEvent::SubmissionResolved) return;
				++NativeReports;
				CheckActualReceiver(Result, TEXT("OnNativeSkillEvent"));
			});
			Unit.Session->ReconcileProductionResults();
			Unit.Session->ReconcileProductionResults();
			Test->TestEqual(TEXT("Dynamic actual receiver gets old result once"), Probe->Count, 1);
			Test->TestEqual(TEXT("Native actual receiver gets old result once"), NativeReports, 1);
			Test->TestEqual(TEXT("Old feedback never cancels same-skill replacement Cast"), Fixture->ActiveCast(Unit), NewRequest.CastId);
			Test->TestEqual(TEXT("Old feedback never advances new Cast phase"), Unit.Runtime->GetCastStage(), Stage);
			Test->TestTrue(TEXT("New submission still owns its original executing key"), Unit.Runtime->GetProductionHandle(Unit.Behavior, Unit.Cast).Key == Current);
			Test->TestEqual(TEXT("New producer remains unexecuted after old supplement"), Unit.Session->GetProductionResults().Query(Current).State, ESkillLabProductionState::InProgress);
			Test->TestEqual(TEXT("Old supplement does not repeat fee"), Unit.ASC->GetActualEnergy(), Energy);
			Test->TestEqual(TEXT("Old supplement does not alter new reservation"), Unit.ASC->GetReservedEnergy(), Reserved);
			Test->TestEqual(TEXT("Old supplement does not restart expired cooldown"), Unit.Session->GetCooldownRemaining(Old.SkillId), 0.0f);
			Test->TestEqual(TEXT("Old supplement does not replay any real GE"), Targets[Index].ASC->GetCombatAttributeSet()->GetHealth(), Health);
			// 新操作随后仍能正常执行和结算；迟到反馈没有消费其点、费用或生命周期。
			const bool bExecuted = Fixture->Execute(Unit);
			Test->TestTrue(TEXT("New operation still executes actual area GE"), bExecuted);
			Test->TestTrue(TEXT("New operation independently resolves through formal settlement"), Unit.Runtime->ResolveBehaviorSubmission(Unit.Behavior, Unit.Cast, Current.Point, bExecuted));
			Test->TestEqual(TEXT("Only new operation incurs its own new fee"), Unit.ASC->GetActualEnergy(), Energy - 2.0f);
			Unit.Session->ReconcileProductionResults();
			Test->TestEqual(TEXT("Later new execution cannot reissue old supplement"), Probe->Count, 1);
			Unit.Runtime->OnSkillEvent.RemoveDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
			Unit.Runtime->OnNativeSkillEvent.Remove(Handle);
			UE_LOG(LogTemp, Log, TEXT("[SkillLabFeedbackIdentity] SameSkillCollision Accepted=%d Skill=%s Cast=%llu Point=%llu OldRequest=%d NewRequest=%d OldId=%s NewId=%s Dynamic=%d Native=%d"),
				bAccept, *Old.SkillId.ToString(), Old.Cast, Old.Point, Old.RequestId, Current.RequestId,
				*Old.Id.ToString(), *Current.Id.ToString(), Probe->Count, NativeReports);
		}
	}
	FAutomationTestBase* Test;
	TUniquePtr<FSkillLabProductionFixture> Fixture;
	FSkillLabProductionFixture::FUnit Units[2], Targets[2];
	FSkillLabSubmissionKey Original[2];
	double Started = 0.0, LastTick = 0.0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSubmissionFeedbackIdentityTest, "SkillLab.Skills.SubmissionFeedbackIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabSubmissionFeedbackIdentityTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FSkillLabSubmissionFeedbackIdentityCommand(this));
	return true;
}
#endif
