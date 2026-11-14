#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastTimingAndReservationsTest, "SkillLab.Skills.CastTimingAndReservations",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastTimingAndReservationsTest::RunTest(const FString&)
{
	FSkillLabCastScenarioFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	FSkillLabSkillRequest ImmediateRequest;
	ImmediateRequest.Caster = Owner;
	ImmediateRequest.SkillId = TEXT("Skill_Immediate");
	const uint64 BlockingReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reserve energy through the ASC ledger"), ASC->BeginEnergyEscrow(BlockingReservation, Runtime->GetEnergyValue() - 1.0f));
	TestEqual(TEXT("Runtime exposes actual energy"), Runtime->GetEnergyValue(), ASC->GetActualEnergy());
	TestEqual(TEXT("Runtime exposes reserved energy"), Runtime->GetReservedEnergyValue(), ASC->GetReservedEnergy());
	TestEqual(TEXT("Runtime exposes available energy"), Runtime->GetAvailableEnergyValue(), 1.0f);
	TestFalse(TEXT("Formal request rejects insufficient available energy"), Runtime->SubmitSkillRequest(ImmediateRequest).bSuccess);
	TestTrue(TEXT("Release blocking reservation"), ASC->ResolveEnergyEscrow(BlockingReservation, false));
	const float BeforeImmediateEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult Immediate = Runtime->SubmitSkillRequest(ImmediateRequest);
	TestTrue(TEXT("Immediate timing uses formal activation"), Immediate.bSuccess);
	TestEqual(TEXT("Immediate timing completes without a configured recovery"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Immediate timing commits cost once"), Runtime->GetEnergyValue(), BeforeImmediateEnergy - 2.0f);
	Runtime->ResetSkills();
	FSkillLabSkillRequest HoldRequest;
	FSkillLabSkillRequest IndependentRequest;
	IndependentRequest.Caster = Owner;
	IndependentRequest.SkillId = TEXT("Skill_Independent");
	const FSkillLabSkillResult BlockedRelease = Runtime->SubmitSkillRequest(IndependentRequest);
	TestTrue(TEXT("Release gate fixture activates before competing reservation"), BlockedRelease.bSuccess);
	const uint64 ReleaseReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reserve funds before release"), ASC->BeginEnergyEscrow(ReleaseReservation, Runtime->GetAvailableEnergyValue() - 1.0f));
	Runtime->ForwardValidatedNotify(BlockedRelease.CastId, SkillLabGASTags::Event_Cast);
	TestEqual(TEXT("Release rejects insufficient available funds"), Runtime->GetActiveCastId(), 0);
	TestTrue(TEXT("Release competing reservation"), ASC->ResolveEnergyEscrow(ReleaseReservation, false));
	Runtime->ResetSkills();
	const float BeforeIndependentEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult Independent = Runtime->SubmitSkillRequest(IndependentRequest);
	TestTrue(TEXT("Independent preparation activates through GAS"), Independent.bSuccess);
	TestEqual(TEXT("Preparation can end while release is still pending"), Runtime->GetCastStage(), ESkillLabCastStage::Active);
	TestFalse(TEXT("Preparation ending does not mark release"), Runtime->GetSingleExecution()->Lifecycle.GetSnapshot().bHasReleased);
	TestEqual(TEXT("Preparation ending does not charge cost"), Runtime->GetEnergyValue(), BeforeIndependentEnergy);
	TestTrue(TEXT("Release event after preparation submits normally"), Runtime->ForwardValidatedNotify(Independent.CastId, SkillLabGASTags::Event_Cast));
	TestEqual(TEXT("Independent release completes without recovery delay"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Independent release charges once"), Runtime->GetEnergyValue(), BeforeIndependentEnergy - 2.0f);
	Runtime->ResetSkills();
	HoldRequest.Caster = Owner;
	HoldRequest.SkillId = TEXT("Skill_Hold");
	const FSkillLabSkillResult Hold = Runtime->SubmitSkillRequest(HoldRequest);
	TestTrue(TEXT("Hold fixture enters a formal Cast"), Hold.bSuccess);
	TestEqual(TEXT("Effect and completion are independently timed"), Runtime->GetCastStage(), ESkillLabCastStage::Active);
	USkillLabLuaSkillBehavior* HoldBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	if (!TestNotNull(TEXT("Hold fixture has a public behavior endpoint"), HoldBehavior)) return false;
	const uint64 SubmissionReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reserve funds before first submission"), ASC->BeginEnergyEscrow(SubmissionReservation, Runtime->GetAvailableEnergyValue() - 1.0f));
	const float BeforeRejectedSubmission = Runtime->GetEnergyValue();
	int32 RejectedSubmissionEffects = 0;
	const FDelegateHandle RejectedSubmissionHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.CastId == Hold.CastId && Event.Event == ESkillLabSkillEvent::EffectApplied) ++RejectedSubmissionEffects;
	});
	TestFalse(TEXT("First submission rejects insufficient available funds"), HoldBehavior->RequestDeclaredAction());
	Runtime->OnNativeSkillEvent.Remove(RejectedSubmissionHandle);
	TestEqual(TEXT("Rejected submission does not apply an effect"), RejectedSubmissionEffects, 0);
	TestEqual(TEXT("Rejected submission does not debit actual energy"), Runtime->GetEnergyValue(), BeforeRejectedSubmission);
	TestEqual(TEXT("Rejected submission does not start cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), 0.0f);
	TestTrue(TEXT("Release competing submission reservation"), ASC->ResolveEnergyEscrow(SubmissionReservation, false));
	const float BeforeHoldEnergy = Runtime->GetEnergyValue();
	auto* SavedExecutor = FSkillLabCastScenarioFixture::Executor(Runtime);
	FSkillLabCastScenarioFixture::SetExecutor(Runtime, nullptr);
	TestFalse(TEXT("Failed effect is not a successful submission"), HoldBehavior->RequestDeclaredAction());
	TestEqual(TEXT("Failed submission does not charge energy"), Runtime->GetEnergyValue(), BeforeHoldEnergy);
	TestEqual(TEXT("Failed submission does not start cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), 0.0f);
	FSkillLabCastScenarioFixture::SetExecutor(Runtime, SavedExecutor);
	bool bNestedSubmissionRejected = false;
	const FDelegateHandle SubmissionHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.Event != ESkillLabSkillEvent::EffectApplied || Event.CastId != Hold.CastId) return;
		FName Reason;
		bNestedSubmissionRejected = !Runtime->BeginBehaviorSubmission(HoldBehavior, Hold.CastId, 2, Reason);
	});
	bool bReentrantResolveAttempted = false;
	bool bReentrantResolveAccepted = false;
	const FDelegateHandle ReentrantResolveHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bReentrantResolveAttempted && Change.NewValue < Change.OldValue)
			{
				bReentrantResolveAttempted = true;
				bReentrantResolveAccepted = Runtime->ResolveBehaviorSubmission(HoldBehavior, Hold.CastId, 1, true);
			}
		});
	TestTrue(TEXT("Retry after failure applies through the real executor"), HoldBehavior->RequestDeclaredAction());
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(ReentrantResolveHandle);
	Runtime->OnNativeSkillEvent.Remove(SubmissionHandle);
	TestTrue(TEXT("Synchronous reentry cannot submit while an attempt is pending"), bNestedSubmissionRejected);
	TestTrue(TEXT("Settlement callback attempted reentrant resolution"), bReentrantResolveAttempted);
	TestFalse(TEXT("Reentrant resolution is rejected"), bReentrantResolveAccepted);
	TestEqual(TEXT("First successful submission charges exactly once"), Runtime->GetEnergyValue(), BeforeHoldEnergy - 2.0f);
	const float FirstCooldown = Runtime->GetCooldownRemaining(HoldRequest.SkillId);
	TestTrue(TEXT("Outer resolution starts cooldown"), FirstCooldown > 0.0f);
	TestEqual(TEXT("Outer resolution clears reservation"), Runtime->GetReservedEnergyValue(), 0.0f);
	FName SubmissionReason;
	TestTrue(TEXT("Shared API accepts another independent point"), Runtime->BeginBehaviorSubmission(HoldBehavior, Hold.CastId, 2, SubmissionReason));
	TestTrue(TEXT("Shared API resolves the second accepted point"), Runtime->ResolveBehaviorSubmission(HoldBehavior, Hold.CastId, 2, true));
	TestEqual(TEXT("Second point does not charge again"), Runtime->GetEnergyValue(), BeforeHoldEnergy - 2.0f);
	TestEqual(TEXT("Second point does not restart cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), FirstCooldown);
	TestTrue(TEXT("Completion event terminates submitted behavior independently"), Runtime->ForwardValidatedNotify(Hold.CastId, SkillLabGASTags::Event_Cast_Complete));
	TestEqual(TEXT("Completion event leaves no active Cast"), Runtime->GetActiveCastId(), 0);
	Runtime->ResetSkills();
	const FSkillLabSkillResult EscrowCast = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* EscrowBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	bool bSawFullEscrow = false;
	const float BeforeEscrow = Runtime->GetEnergyValue();
	const FDelegateHandle EscrowHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.CastId != EscrowCast.CastId || Event.Event != ESkillLabSkillEvent::EffectApplied) return;
		bSawFullEscrow = FMath::IsNearlyEqual(Runtime->GetEnergyValue(), BeforeEscrow)
			&& FMath::IsNearlyEqual(Runtime->GetReservedEnergyValue(), 2.0f)
			&& FMath::IsNearlyEqual(Runtime->GetAvailableEnergyValue(), BeforeEscrow - 2.0f);
		TestFalse(TEXT("Write cannot invalidate a reservation"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 1.0f));
		TestTrue(TEXT("Valid energy write applies while reserved"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 90.0f));
		ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
	});
	TestTrue(TEXT("Escrow action submits successfully"), EscrowBehavior && EscrowBehavior->RequestDeclaredAction());
	Runtime->OnNativeSkillEvent.Remove(EscrowHandle);
	TestTrue(TEXT("Effect callback observes full cost already escrowed"), bSawFullEscrow);
	TestEqual(TEXT("Valid writes and subsequent settlement preserve the balance"), Runtime->GetEnergyValue(), 89.0f);
	TestEqual(TEXT("Successful settlement clears reservation"), Runtime->GetReservedEnergyValue(), 0.0f);
	Runtime->ResetSkills();
	const uint64 FailedReservation = ASC->AllocateEnergyTransactionId();
	const uint64 OtherReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Resource coordinator reserves for failed attempt"), ASC->BeginEnergyEscrow(FailedReservation, 2.0f));
	TestTrue(TEXT("Another transaction can reserve available energy"), ASC->BeginEnergyEscrow(OtherReservation, 2.0f));
	TestTrue(TEXT("Energy override respecting reservations applies"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 10.0f));
	TestTrue(TEXT("Failed attempt releases its reservation"), ASC->ResolveEnergyEscrow(FailedReservation, false));
	TestEqual(TEXT("Failed attempt does not debit actual energy"), Runtime->GetEnergyValue(), 10.0f);
	TestEqual(TEXT("Other reservation remains"), Runtime->GetReservedEnergyValue(), 2.0f);
	TestTrue(TEXT("Other reservation can be released"), ASC->ResolveEnergyEscrow(OtherReservation, false));
	Runtime->ResetSkills();
	return true;
}
#endif
