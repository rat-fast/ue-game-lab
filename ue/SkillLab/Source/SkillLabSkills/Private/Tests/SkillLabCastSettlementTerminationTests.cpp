#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastSettlementTerminationTest, "SkillLab.Skills.CastSettlementTermination",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastSettlementTerminationTest::RunTest(const FString&)
{
	FSkillLabCastScenarioFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	auto* World = Unit.Actor->GetWorld();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	FSkillLabSkillRequest HoldRequest; HoldRequest.Caster = Owner; HoldRequest.SkillId = TEXT("Skill_Hold");
	const auto FreshFixture = [&]()
	{
		Owner->Destroy();
		Owner = World->SpawnActor<AActor>();
		ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		Runtime = NewObject<USkillLabSkillRuntimeComponent>(Owner);
		Owner->AddInstanceComponent(Runtime);
		Runtime->RegisterComponent();
		Runtime->InitializeSkills();
		HoldRequest.Caster = Owner;
	};
	FreshFixture();
	bool bDebitCallbackRan = false;
	const FDelegateHandle DebitHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bDebitCallbackRan && Change.NewValue < Change.OldValue && ASC->IsEnergyEscrowActive())
			{
				bDebitCallbackRan = true;
				ASC->RequestResourceChange(false, EGameplayModOp::Additive, 5.0f);
				ASC->RequestResourceChange(true, EGameplayModOp::Override, 99.0f);
			}
		});
	const FSkillLabSkillResult DebitCast = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* DebitBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	TestTrue(TEXT("Debit callback fixture is active"), DebitCast.bSuccess && DebitBehavior);
	TestTrue(TEXT("Debit callback action succeeds"), DebitBehavior && DebitBehavior->RequestDeclaredAction());
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DebitHandle);
	TestTrue(TEXT("Escrow debit callback can submit controlled changes"), bDebitCallbackRan);
	TestEqual(TEXT("Queued debit callback resource change is applied"), Runtime->GetEnergyValue(), 99.0f);
	ASC->RequestResourceChange(true, EGameplayModOp::Override, 100.0f);
	Runtime->ResetSkills();
	FreshFixture();
	bool bDirectDebitCallbackRan = false;
	const FDelegateHandle DirectDebitHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bDirectDebitCallbackRan && Change.NewValue < Change.OldValue && ASC->IsEnergyEscrowActive())
			{
				bDirectDebitCallbackRan = true;
				ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 50.0f);
			}
		});
	const uint64 DirectDebitId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Direct debit fixture reserves without changing actual"), ASC->BeginEnergyEscrow(DirectDebitId, 2.0f));
	TestFalse(TEXT("Reservation does not trigger debit callback"), bDirectDebitCallbackRan);
	AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Direct debit callback makes settlement unconfirmed"), ASC->ResolveEnergyEscrow(DirectDebitId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DirectDebitHandle);
	TestTrue(TEXT("Direct debit callback was exercised"), bDirectDebitCallbackRan);
	TestTrue(TEXT("Direct debit callback is reported"), ASC->HasUnsupportedResourceWrite());
	TestTrue(TEXT("Unconfirmed debit retains reservation"), ASC->HasIndeterminateEnergyEscrow(DirectDebitId));
	FreshFixture();
	const FSkillLabSkillResult CancelDuringSubmit = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* CancelBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	const float BeforeCancelEnergy = Runtime->GetEnergyValue();
	const FDelegateHandle CancelSubmitHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.Event == ESkillLabSkillEvent::EffectApplied && Event.CastId == CancelDuringSubmit.CastId)
			Runtime->CancelActiveCast(TEXT("SynchronousCancel"));
	});
	TestTrue(TEXT("Accepted effect survives synchronous cancellation"), CancelBehavior && CancelBehavior->RequestDeclaredAction());
	Runtime->OnNativeSkillEvent.Remove(CancelSubmitHandle);
	TestEqual(TEXT("Deferred cancellation terminates the same Cast"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Accepted submission charges once before cancellation"), Runtime->GetEnergyValue(), BeforeCancelEnergy - 2.0f);
	TestTrue(TEXT("Accepted submission retains cooldown after cancellation"), Runtime->GetCooldownRemaining(HoldRequest.SkillId) > 0.0f);
	Runtime->ResetSkills();
	const auto TestSubmissionTermination = [&](const int32 Mode)
	{
		AActor* TestOwner = World->SpawnActor<AActor>();
		USkillLabAbilitySystemComponent* TestASC = NewObject<USkillLabAbilitySystemComponent>(TestOwner);
		TestOwner->AddInstanceComponent(TestASC);
		TestASC->RegisterComponent();
		USkillLabSkillRuntimeComponent* TestRuntime = NewObject<USkillLabSkillRuntimeComponent>(TestOwner);
		TestOwner->AddInstanceComponent(TestRuntime);
		TestRuntime->RegisterComponent();
		if (!TestRuntime->HasBegunPlay()) TestRuntime->BeginPlay();
		TestRuntime->InitializeSkills();
		FSkillLabSkillRequest TestRequest;
		TestRequest.Caster = TestOwner;
		TestRequest.SkillId = TEXT("Skill_Hold");
		const FSkillLabSkillResult TestCast = TestRuntime->SubmitSkillRequest(TestRequest);
		USkillLabLuaSkillBehavior* Behavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(TestRuntime));
		if (!TestTrue(TEXT("Termination fixture activates"), TestCast.bSuccess && Behavior)) { TestOwner->Destroy(); return; }
		if (Mode == 2)
		{
			FName Reason;
			TestTrue(TEXT("Indeterminate fixture begins submission"), TestRuntime->BeginBehaviorSubmission(Behavior, TestCast.CastId, 1, Reason));
			const uint64 TransactionId = TestRuntime->GetSingleExecution()->Submission.TransactionId;
			TestASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
			AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
			TestRuntime->AbortBehaviorSubmission(Behavior, TestCast.CastId, 1);
			TestTrue(TEXT("Uncertain escrow retains an explicit ledger"), TestASC->HasIndeterminateEnergyEscrow(TransactionId));
			TestEqual(TEXT("Uncertain submission does not start cooldown"), TestRuntime->GetCooldownRemaining(TestRequest.SkillId), 0.0f);
		}
		else
		{
			bool bEnded = false;
			FDelegateHandle Handle;
			if (Mode == 0 || Mode == 3)
			{
				Handle = TestASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
					[&](const FOnAttributeChangeData& Change)
					{
						if (!bEnded && Change.NewValue < Change.OldValue && TestASC->IsEnergyEscrowActive())
						{
							bEnded = true;
							TestASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
							if (Mode == 0) TestRuntime->EndPlay(EEndPlayReason::RemovedFromWorld);
							else TestRuntime->ResetSkills();
						}
					});
			}
			else
			{
				Handle = TestRuntime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
				{
					if (!bEnded && Event.Event == ESkillLabSkillEvent::EffectApplied)
					{
						bEnded = true;
						TestASC->RequestResourceChange(false, EGameplayModOp::Additive, -1.0f);
						if (Mode == 1) TestRuntime->EndPlay(EEndPlayReason::RemovedFromWorld);
						else TestRuntime->ResetSkills();
					}
				});
			}
			const bool bSucceeded = Behavior->RequestDeclaredAction();
			if (Mode == 0 || Mode == 3) TestASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Handle);
			else TestRuntime->OnNativeSkillEvent.Remove(Handle);
			TestTrue(TEXT("Termination occurs inside submission"), bEnded);
			TestEqual(TEXT("Only a still-live Runtime reports complete submission delivery"), bSucceeded, Mode == 3 || Mode == 4);
			TestEqual(TEXT("Termination clears the Cast"), TestRuntime->GetActiveCastId(), 0);
			const float ExpectedEnergy = Mode == 0 ? 99.0f : (Mode == 1 ? 97.0f : 100.0f);
			TestEqual(TEXT("Termination retains coordinated resource changes"), TestRuntime->GetEnergyValue(), ExpectedEnergy);
		}
		TestEqual(TEXT("Only the explicitly unconfirmed reservation remains active"), TestASC->IsEnergyEscrowActive(), Mode == 2);
		TestFalse(TEXT("Termination drains resource queue"), TestASC->HasPendingResourceChanges());
		TestOwner->Destroy();
	};
	TestSubmissionTermination(0);
	TestSubmissionTermination(1);
	TestSubmissionTermination(2);
	TestSubmissionTermination(3);
	TestSubmissionTermination(4);
	return true;
}
#endif
