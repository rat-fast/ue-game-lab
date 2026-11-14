#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"
class FSkillLabBufferedIntentConsumeCommand final : public IAutomationLatentCommand
{
public:
	FSkillLabBufferedIntentConsumeCommand(FAutomationTestBase* InTest, USkillLabSkillRuntimeComponent* InRuntime,
		const int32 InOldCastId, const int32 InBufferedRequestId, const float InInitialEnergy,
		TSharedPtr<FSkillLabCastScenarioFixture> InFixture)
		: Test(InTest), Runtime(InRuntime), OldCastId(InOldCastId), BufferedRequestId(InBufferedRequestId),
		InitialEnergy(InInitialEnergy), Fixture(MoveTemp(InFixture))
	{
		if (Runtime.IsValid())
		{
			RejectHandle = Runtime->OnNativeSkillEvent.AddLambda([this](const FSkillLabSkillResult& Result)
			{
				if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("InsufficientEnergy"))
				{
					bObservedGateReject = true;
					ObservedGateRejectReason = Result.Reason;
				}
			});
		}
	}

	virtual ~FSkillLabBufferedIntentConsumeCommand() override
	{
		if (Runtime.IsValid() && RejectHandle.IsValid()) Runtime->OnNativeSkillEvent.Remove(RejectHandle);
	}

	virtual bool Update() override
	{
		if (!Runtime.IsValid())
		{
			Test->TestNotNull(TEXT("Runtime remains valid while consuming buffered intent"), Runtime.Get());
			return true;
		}
		if (UWorld* World = Runtime->GetWorld())
		{
			World->GetTimerManager().Tick(1.0f / 60.0f);
		}
		if (bAwaitingInitialConsumption && (Runtime->GetActiveCastId() == OldCastId || FSkillLabCastScenarioFixture::HasIntent(Runtime.Get())))
		{
			if (++WaitFrames <= 120) return false;
			Test->TestTrue(TEXT("Buffered intent consumption completes within the latent timeout"), false);
			return true;
		}

		if (bAwaitingInitialConsumption)
		{
			if (!Test->TestNotNull(TEXT("Deferred activation has an execution"), Runtime->GetSingleExecution())) return true;
			Test->TestFalse(TEXT("Natural-completion buffer ID is not reused as a new RequestId"),
				Runtime->GetSingleExecution()->ActiveRequest.RequestId == BufferedRequestId);
			Test->TestTrue(TEXT("Natural completion starts a fresh Cast after clearing the old Cast"), Runtime->GetActiveCastId() > OldCastId);
			Test->TestEqual(TEXT("Natural completion activates the buffered skill definition"), Runtime->GetSingleExecution()->ActiveRequest.SkillId,
				FName(TEXT("Skill_BufferedSuccess")));
			Test->TestTrue(TEXT("Fresh Cast has a live GAS Ability only after deferred consumption"), SkillLabObservedAbility(Runtime).IsValid()
				&& SkillLabObservedAbility(Runtime)->bSkillActive && SkillLabObservedAbility(Runtime)->BoundCastToken.CastId == Runtime->GetActiveCastId());
			Test->TestFalse(TEXT("Old Cast Notify cannot advance the consumed Cast"), Runtime->ForwardValidatedNotify(OldCastId, SkillLabGASTags::Event_Cast));
			FName RuleReason;
			Test->TestFalse(TEXT("Another skill selects a distinct movement policy"), Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, RuleReason));
			Test->TestEqual(TEXT("Distinct policy reports its content identity"), RuleReason, FName(TEXT("Restricted")));
			bAwaitingInitialConsumption = false;
			WaitFrames = 0;

			Runtime->CancelActiveCast(TEXT("BufferedConsumptionTestCleanup"));
			FSkillLabSkillRequest FailedVictimRequest;
			FailedVictimRequest.Caster = Runtime->GetOwner();
			FailedVictimRequest.SkillId = TEXT("Skill_BufferedVictim");
			const FSkillLabSkillResult FailedVictim = Runtime->SubmitSkillRequest(FailedVictimRequest);
			Test->TestTrue(TEXT("Second natural-completion gate fixture starts formally"), FailedVictim.bSuccess);
			FSkillLabSkillRequest FailedBufferedRequest = FailedVictimRequest;
			FailedBufferedRequest.SkillId = TEXT("Skill_BufferedFail");
			const FSkillLabSkillResult FailedBuffered = Runtime->SubmitSkillRequest(FailedBufferedRequest);
			Test->TestEqual(TEXT("Second intent is buffered during Cast"), FailedBuffered.Reason, FName(TEXT("InputBufferedLatestWins")));
			Test->TestTrue(TEXT("Second fixture releases through the current Ability"),
				Runtime->ForwardValidatedNotify(FailedVictim.CastId, SkillLabGASTags::Event_Cast));
			Runtime->GetOwner()->FindComponentByClass<USkillLabAbilitySystemComponent>()->RequestResourceChange(false, EGameplayModOp::Override, 0.0f);
			FailedOldCastId = FailedVictim.CastId;
			FSkillLabCastScenarioFixture::FinishRecovery(Runtime.Get());
			Test->TestEqual(TEXT("Buffered request does not activate while Recovery is ending"), Runtime->GetActiveCastId(), 0);
			Test->TestTrue(TEXT("Intent remains until deferred formal gates run"), FSkillLabCastScenarioFixture::HasIntent(Runtime.Get()));
		}
		return CompleteAfterGateFailure();
	}

private:
	bool CompleteAfterGateFailure()
	{
		if (Runtime->GetActiveCastId() == FailedOldCastId || FSkillLabCastScenarioFixture::HasIntent(Runtime.Get()))
		{
			if (++WaitFrames <= 120) return false;
			Test->TestTrue(TEXT("Buffered resource rejection completes within the latent timeout"), false);
			return true;
		}
		Test->TestEqual(TEXT("Failed buffered activation does not revive old Cast"), Runtime->GetActiveCastId(), 0);
		Test->TestFalse(TEXT("Failed buffered activation clears the slot"), FSkillLabCastScenarioFixture::HasIntent(Runtime.Get()));
		Test->TestTrue(TEXT("Failed buffered activation reports the formal resource gate"), bObservedGateReject);
		Test->TestEqual(TEXT("Failed buffered activation exposes the exact resource rejection reason"),
			ObservedGateRejectReason, FName(TEXT("InsufficientEnergy")));
		Runtime->ResetSkills();
		Runtime->GetOwner()->FindComponentByClass<USkillLabAbilitySystemComponent>()->RequestResourceChange(false, EGameplayModOp::Override,
			InitialEnergy);
		Test->TestEqual(TEXT("Reset restores the original energy state"), Runtime->GetEnergyValue(), InitialEnergy);
		return true;
	}

	FAutomationTestBase* Test;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	int32 OldCastId;
	int32 BufferedRequestId;
	float InitialEnergy;
	int32 WaitFrames = 0;
	FDelegateHandle RejectHandle;
	bool bObservedGateReject = false;
	FName ObservedGateRejectReason = NAME_None;
	bool bAwaitingInitialConsumption = true;
	int32 FailedOldCastId = 0;
	TSharedPtr<FSkillLabCastScenarioFixture> Fixture;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastDeferredIntentTest, "SkillLab.Skills.CastDeferredIntent",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastDeferredIntentTest::RunTest(const FString&)
{
	TSharedPtr<FSkillLabCastScenarioFixture> Fixture = MakeShared<FSkillLabCastScenarioFixture>(this);
	if (!Fixture->bReady) return false;
	auto Unit = Fixture->MakeUnit();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	const auto Definition = Fixture->FixtureDefinitions.FindChecked(TEXT("Skill_Mend"));
	auto Request = [&]() { return Fixture->Submit(Unit, Definition.SkillId); };
	const float InitialEnergy = Runtime->GetEnergyValue();
	int32 StartedCount = 0;
	const auto StartedHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{ if (Result.Event == ESkillLabSkillEvent::Started) ++StartedCount; });
	ON_SCOPE_EXIT { Runtime->OnNativeSkillEvent.Remove(StartedHandle); };
	const FSkillLabSkillResult CacheVictim = Request();
	Runtime->NotifyNonInterruptingOperation(ESkillLabOperationCategory::Movement);
	TestEqual(TEXT("Movement category leaves current Cast intact"), Runtime->GetActiveCastId(), CacheVictim.CastId);
	Runtime->NotifyNonInterruptingOperation(ESkillLabOperationCategory::UserCancel);
	TestEqual(TEXT("User cancellation category leaves current Cast intact"), Runtime->GetActiveCastId(), CacheVictim.CastId);
	FSkillLabSkillRequest BufferedRequest;
	BufferedRequest.Caster = Owner;
	BufferedRequest.SkillId = Definition.SkillId;
	const FSkillLabSkillResult BufferedResult = Runtime->SubmitSkillRequest(BufferedRequest);
	TestEqual(TEXT("Busy skill intent occupies latest-wins slot"), BufferedResult.Reason, FName(TEXT("InputBufferedLatestWins")));
	FSkillLabSkillRequest LatestRequest = BufferedRequest;
	const FSkillLabSkillResult LatestBufferedResult = Runtime->SubmitSkillRequest(LatestRequest);
	TestEqual(TEXT("Latest skill intent replaces prior buffered intent"), LatestBufferedResult.Reason, FName(TEXT("InputBufferedLatestWins")));
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	TestEqual(TEXT("Cancelled Cast clears buffered intent without auto activation"), Runtime->GetActiveCastId(), 0);
	TestTrue(TEXT("Buffered Cast victim was live"), CacheVictim.bSuccess);
	Runtime->ResetSkills();
	TestFalse(TEXT("Reset clears any pending input slot"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	const FSkillLabSkillResult ResetVictim = Request();
	FSkillLabSkillRequest ResetBufferedRequest;
	ResetBufferedRequest.Caster = Owner;
	ResetBufferedRequest.SkillId = Definition.SkillId;
	Runtime->SubmitSkillRequest(ResetBufferedRequest);
	TestTrue(TEXT("Reset fixture populated the pending input slot"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	Runtime->ResetSkills();
	TestFalse(TEXT("Reset actively clears a pending intent"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	TestTrue(TEXT("Reset fixture cast was active"), ResetVictim.bSuccess);

	const FSkillLabSkillResult DeathVictim = Request();
	FSkillLabSkillRequest DeathBufferedRequest;
	DeathBufferedRequest.Caster = Owner;
	DeathBufferedRequest.SkillId = Definition.SkillId;
	Runtime->SubmitSkillRequest(DeathBufferedRequest);
	TestTrue(TEXT("Death fixture populated the pending input slot"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Dead);
	TestEqual(TEXT("Death cancels the current Cast"), Runtime->GetActiveCastId(), 0);
	TestFalse(TEXT("Death clears the cached intent"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Dead);
	TestTrue(TEXT("Death cleanup fixture started a Cast"), DeathVictim.bSuccess);
	Runtime->ResetSkills();

	const FSkillLabSkillResult TerminalVictim = Request();
	FSkillLabSkillRequest TerminalBufferedRequest;
	TerminalBufferedRequest.Caster = Owner;
	TerminalBufferedRequest.SkillId = TEXT("Skill_BufferedSuccess");
	const FSkillLabSkillResult TerminalBuffered = Runtime->SubmitSkillRequest(TerminalBufferedRequest);
	TestEqual(TEXT("Natural-completion input is accepted into the single slot"), TerminalBuffered.Reason, FName(TEXT("InputBufferedLatestWins")));
	Runtime->ForwardValidatedNotify(TerminalVictim.CastId, Definition.ReleaseEventTag);
	const int32 BeforeFinishStartedCount = StartedCount;
	TestEqual(TEXT("Buffered intent cannot activate in Recovery"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	FSkillLabCastScenarioFixture::FinishRecovery(Runtime);
	TestEqual(TEXT("Natural completion clears old Cast before scheduling buffered activation"), Runtime->GetActiveCastId(), 0);
	TestFalse(TEXT("Old Ability has ended before deferred buffered activation"), SkillLabObservedAbility(Runtime).IsValid());
	TestTrue(TEXT("Natural completion preserves buffered intent until next tick"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	TestEqual(TEXT("No Cast starts synchronously inside old Ability EndAbility"), StartedCount, BeforeFinishStartedCount);
	ADD_LATENT_AUTOMATION_COMMAND(FSkillLabBufferedIntentConsumeCommand(this, Runtime, TerminalVictim.CastId,
		TerminalBuffered.RequestId, InitialEnergy, Fixture));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastFixtureCleanupTest, "SkillLab.Skills.CastFixtureCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastFixtureCleanupTest::RunTest(const FString&)
{
	// 目录是进程全局状态：与其他目录测试沿单 worker 队列串行执行。
	FSkillLabScopedTestCatalog OriginalCatalog;
	FSkillLabSkillDefinition Sentinel;
	Sentinel.SkillId = TEXT("FixtureRestoreSentinel");
	Sentinel.Cost = 37.0f;
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation({{Sentinel.SkillId, Sentinel}}, false);
	const auto CheckCatalog = [&]()
	{
		TMap<FName, FSkillLabSkillDefinition> Restored;
		TestFalse(TEXT("Cleanup restores the prior load-attempt flag"), FSkillLabSkillDefinitionProvider::CopyDefinitionsForAutomation(Restored));
		TestEqual(TEXT("Cleanup restores the prior catalog size"), Restored.Num(), 1);
		const auto* Entry = Restored.Find(Sentinel.SkillId);
		TestTrue(TEXT("Cleanup restores prior content, not the scenario catalog"), Entry && Entry->Cost == Sentinel.Cost);
	};
	UWorld* EarlyWorld = nullptr;
	TWeakObjectPtr<AActor> EarlyOwner;
	const auto EarlyReturn = [&]()
	{
		FSkillLabCastScenarioFixture Fixture(this);
		if (!Fixture.bReady) return false;
		const auto Unit = Fixture.MakeUnit();
		EarlyWorld = Fixture.World;
		EarlyOwner = Unit.Actor;
		TestTrue(TEXT("Early-return cleanup begins with a live Cast"), Fixture.Submit(Unit, TEXT("Skill_Hold")).bSuccess);
		return false;
	};
	TestFalse(TEXT("Exercise an early-return exit without manual cleanup"), EarlyReturn());
	TestTrue(TEXT("Early return removes the World context"), EarlyWorld && !GEngine->GetWorldContextFromWorld(EarlyWorld));
	TestTrue(TEXT("Early return destroys the owner"), !EarlyOwner.IsValid() || EarlyOwner->IsActorBeingDestroyed());
	CheckCatalog();

	TSharedPtr<FSkillLabCastScenarioFixture> Fixture = MakeShared<FSkillLabCastScenarioFixture>(this);
	if (!Fixture->bReady) return false;
	const auto Unit = Fixture->MakeUnit();
	TestTrue(TEXT("Latent abandonment begins with a live Cast"), Fixture->Submit(Unit, TEXT("Skill_Hold")).bSuccess);
	UWorld* LatentWorld = Fixture->World;
	TWeakPtr<FSkillLabCastScenarioFixture> WeakFixture = Fixture;
	TSharedPtr<FSkillLabBufferedIntentConsumeCommand> Command = MakeShared<FSkillLabBufferedIntentConsumeCommand>(this, Unit.Runtime, 1, 2, 100.0f, Fixture);
	Fixture.Reset();
	TestTrue(TEXT("Latent command owns the fixture after RunTest releases it"), WeakFixture.IsValid());
	Command.Reset();
	TestFalse(TEXT("Abandoning latent work releases its fixture"), WeakFixture.IsValid());
	TestNull(TEXT("Latent abandonment removes the World context"), GEngine->GetWorldContextFromWorld(LatentWorld));
	CheckCatalog();
	return true;
}
#endif
