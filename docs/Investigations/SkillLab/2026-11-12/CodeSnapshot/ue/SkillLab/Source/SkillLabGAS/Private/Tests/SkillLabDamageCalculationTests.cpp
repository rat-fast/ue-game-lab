#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabDamageCalculation.h"
#include "SkillLabCombatAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabDamageCalculationTest,
	"SkillLab.GAS.DamageCalculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabDamageCalculationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const SkillLabDamageCalculation::FResult Result = SkillLabDamageCalculation::Resolve(100.0f, 100.0f, 5.0f, 30.0f);
	TestEqual(TEXT("Armor is applied once"), Result.MitigatedDamage, 25.0f);
	TestEqual(TEXT("Health is reduced by the actual settled amount"), Result.HealthAfter, 75.0f);
	TestEqual(TEXT("Applied damage is independently reported"), Result.AppliedDamage, 25.0f);

	const SkillLabDamageCalculation::FResult Clamped = SkillLabDamageCalculation::Resolve(10.0f, 100.0f, 0.0f, 50.0f);
	TestEqual(TEXT("Damage cannot reduce health below zero"), Clamped.HealthAfter, 0.0f);
	TestEqual(TEXT("Applied damage is capped by remaining health"), Clamped.AppliedDamage, 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabEnergyLedgerTest,
	"SkillLab.GAS.EnergyLedger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabEnergyLedgerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Editor || Context.WorldType == EWorldType::Game
				|| Context.WorldType == EWorldType::PIE))
			{
				World = Context.World();
				break;
			}
		}
	}
	if (!TestNotNull(TEXT("Automation world exists"), World)) return false;

	AActor* Owner = World->SpawnActor<AActor>();
	if (!TestNotNull(TEXT("Resource owner spawns"), Owner)) return false;
	USkillLabAbilitySystemComponent* ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
	Owner->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitializeSkillLabAbilityActorInfo(Owner, Owner);
	TestEqual(TEXT("Initial GAS Energy is confirmed actual"), ASC->GetActualEnergy(), 100.0f);
	TestTrue(TEXT("Energy same-value override is accepted"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	TestTrue(TEXT("Energy zero additive is accepted"), ASC->RequestResourceChange(false, EGameplayModOp::Additive, 0.0f));
	TestTrue(TEXT("MaxEnergy same-value override is accepted"), ASC->RequestResourceChange(true, EGameplayModOp::Override, 100.0f));
	TestTrue(TEXT("MaxEnergy zero additive is accepted"), ASC->RequestResourceChange(true, EGameplayModOp::Additive, 0.0f));
	TestFalse(TEXT("No-op writes do not poison future reservations"), ASC->HasUnsupportedResourceWrite());

	TestTrue(TEXT("First reservation is accepted"), ASC->BeginEnergyEscrow(101, 30.0f));
	TestTrue(TEXT("Second reservation can coexist"), ASC->BeginEnergyEscrow(102, 40.0f));
	TestEqual(TEXT("Reservation does not change visible GAS Energy"), ASC->GetActualEnergy(), 100.0f);
	TestEqual(TEXT("Reservations are summed"), ASC->GetReservedEnergy(), 70.0f);
	TestEqual(TEXT("Available Energy excludes all reservations"), ASC->GetAvailableEnergy(), 30.0f);
	TestFalse(TEXT("A reservation cannot overbook available Energy"), ASC->BeginEnergyEscrow(103, 31.0f));
	bool bReentrantBeginRejected = false;
	const FDelegateHandle MaxEnergyHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData&)
		{
			bReentrantBeginRejected = !ASC->BeginEnergyEscrow(201, 1.0f);
		});
	TestTrue(TEXT("MaxEnergy reduction that invalidates reservations is rejected"),
		!ASC->RequestResourceChange(true, EGameplayModOp::Override, 50.0f));
	TestEqual(TEXT("Rejected MaxEnergy change leaves actual Energy unchanged"), ASC->GetActualEnergy(), 100.0f);
	TestFalse(TEXT("Rejected MaxEnergy write emits no delegate"), bReentrantBeginRejected);
	TestTrue(TEXT("Compatible MaxEnergy increase applies with reservations"),
		ASC->RequestResourceChange(true, EGameplayModOp::Override, 120.0f));
	TestTrue(TEXT("Begin during MaxEnergy notification is rejected"), bReentrantBeginRejected);
	TestEqual(TEXT("Rejected reentrant reservation does not overbook"), ASC->GetReservedEnergy(), 70.0f);
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()).Remove(MaxEnergyHandle);

	TestTrue(TEXT("Compatible coordinated resource change applies with reservations"),
		ASC->RequestResourceChange(false, EGameplayModOp::Additive, 5.0f));
	TestEqual(TEXT("Compatible change updates actual without consuming reservations"), ASC->GetActualEnergy(), 105.0f);
	TestTrue(TEXT("Successful resolution commits the first cost"), ASC->ResolveEnergyEscrow(101, true));
	TestEqual(TEXT("Success deducts exactly once"), ASC->GetActualEnergy(), 75.0f);
	TestEqual(TEXT("Other reservation remains available"), ASC->GetAvailableEnergy(), 35.0f);
	TestFalse(TEXT("Closed transaction ID cannot be reused"), ASC->BeginEnergyEscrow(101, 1.0f));
	TestTrue(TEXT("Explicit failure releases the second reservation"), ASC->ResolveEnergyEscrow(102, false));
	TestEqual(TEXT("Failed reservation leaves actual Energy unchanged"), ASC->GetActualEnergy(), 75.0f);
	TestFalse(TEXT("Closed transaction is idempotently rejected"), ASC->ResolveEnergyEscrow(102, false));

	TestTrue(TEXT("Uncertain result reserves Energy"), ASC->BeginEnergyEscrow(104, 10.0f));
	TestTrue(TEXT("Uncertain result retains its reservation"), ASC->MarkEnergyEscrowIndeterminate(104));
	TestEqual(TEXT("Indeterminate result is not reported as a debit"), ASC->GetActualEnergy(), 75.0f);
	TestEqual(TEXT("Indeterminate reservation reduces available Energy"), ASC->GetAvailableEnergy(), 65.0f);
	TestTrue(TEXT("Compatible resource changes remain available while indeterminate"),
		ASC->RequestResourceChange(false, EGameplayModOp::Additive, 5.0f));
	TestEqual(TEXT("Indeterminate reservation does not block compatible writes"), ASC->GetActualEnergy(), 80.0f);
	TestTrue(TEXT("Another reservation can start while one result is indeterminate"), ASC->BeginEnergyEscrow(105, 20.0f));
	TestEqual(TEXT("Available Energy includes both reservations"), ASC->GetAvailableEnergy(), 50.0f);
	TestTrue(TEXT("Authoritative success resolves the indeterminate reservation"), ASC->ResolveIndeterminateEnergyEscrow(104, true));
	TestEqual(TEXT("Authoritative success debits once"), ASC->GetActualEnergy(), 70.0f);
	TestFalse(TEXT("Resolved indeterminate transaction cannot be applied twice"), ASC->ResolveIndeterminateEnergyEscrow(104, true));
	TestTrue(TEXT("Other reservation remains independently releasable"), ASC->ResolveEnergyEscrow(105, false));

	TestTrue(TEXT("Energy can be restored before large reservation cases"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	TestTrue(TEXT("Large reservation can cover most of actual Energy"), ASC->BeginEnergyEscrow(106, 80.0f));
	TestTrue(TEXT("Large reservation commits without counting itself as unpaid"), ASC->ResolveEnergyEscrow(106, true));
	TestEqual(TEXT("Large reservation debits to the exact remaining balance"), ASC->GetActualEnergy(), 20.0f);
	TestTrue(TEXT("Energy can be restored after large settlement"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	TestTrue(TEXT("Near-full reservations coexist"), ASC->BeginEnergyEscrow(107, 55.0f) && ASC->BeginEnergyEscrow(108, 40.0f));
	TestTrue(TEXT("First near-full reservation commits"), ASC->ResolveEnergyEscrow(107, true));
	TestEqual(TEXT("First near-full settlement preserves the second reservation"), ASC->GetActualEnergy(), 45.0f);
	TestTrue(TEXT("Second near-full reservation commits from the remaining balance"), ASC->ResolveEnergyEscrow(108, true));
	TestEqual(TEXT("Combined reservations debit at most the confirmed actual"), ASC->GetActualEnergy(), 5.0f);

	TestTrue(TEXT("Energy can be restored before verifying MaxEnergy clamp behavior"),
		ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	TestTrue(TEXT("MaxEnergy can still be changed through the coordinator"),
		ASC->RequestResourceChange(true, EGameplayModOp::Override, 50.0f));
	TestEqual(TEXT("Lowering MaxEnergy retains existing clamp semantics"), ASC->GetActualEnergy(), 50.0f);
	TestTrue(TEXT("MaxEnergy can be restored through the coordinator"),
		ASC->RequestResourceChange(true, EGameplayModOp::Override, 100.0f));
	TestTrue(TEXT("Energy can be restored through the coordinator"),
		ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	TestEqual(TEXT("Ordinary resource override behavior is retained"), ASC->GetActualEnergy(), 100.0f);

	const uint64 ReentrantId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Allocated reservation starts"), ASC->BeginEnergyEscrow(ReentrantId, 5.0f));
	bool bNestedResolveRejected = false;
	const FDelegateHandle EnergyHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (Change.NewValue < Change.OldValue) bNestedResolveRejected = !ASC->ResolveEnergyEscrow(ReentrantId, true);
		});
	TestTrue(TEXT("Outer successful resolve charges once"), ASC->ResolveEnergyEscrow(ReentrantId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(EnergyHandle);
	TestTrue(TEXT("Same-transaction resolve is rejected while settling"), bNestedResolveRejected);
	TestEqual(TEXT("Synchronous callback cannot double charge"), ASC->GetActualEnergy(), 95.0f);
	TestFalse(TEXT("Closed ID remains unavailable after late retry"), ASC->BeginEnergyEscrow(ReentrantId, 1.0f));
	TestFalse(TEXT("Late result cannot affect a closed transaction"), ASC->ResolveEnergyEscrow(ReentrantId, false));
	TestTrue(TEXT("Energy reset for cross-transaction settlement test"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	const uint64 FirstNestedId = ASC->AllocateEnergyTransactionId();
	const uint64 SecondNestedId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("First nested-settlement reservation starts"), ASC->BeginEnergyEscrow(FirstNestedId, 5.0f));
	TestTrue(TEXT("Second nested-settlement reservation starts"), ASC->BeginEnergyEscrow(SecondNestedId, 5.0f));
	bool bCrossTransactionResolveRejected = false;
	const FDelegateHandle CrossSettlementHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (Change.NewValue < Change.OldValue)
				bCrossTransactionResolveRejected = !ASC->ResolveEnergyEscrow(SecondNestedId, true);
		});
	TestTrue(TEXT("First transaction resolves while nested second resolution is rejected"), ASC->ResolveEnergyEscrow(FirstNestedId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(CrossSettlementHandle);
	TestTrue(TEXT("ASC settlement lock rejects another transaction in the debit callback"), bCrossTransactionResolveRejected);
	TestFalse(TEXT("Nested transaction attempt does not report an unsupported write"), ASC->HasUnsupportedResourceWrite());
	TestEqual(TEXT("First transaction is charged once"), ASC->GetActualEnergy(), 95.0f);
	TestTrue(TEXT("Second transaction can settle once the first exits"), ASC->ResolveEnergyEscrow(SecondNestedId, true));
	TestEqual(TEXT("Both independent transactions charge once"), ASC->GetActualEnergy(), 90.0f);

	const uint64 IndeterminateReentrantId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Indeterminate reentry reservation starts"), ASC->BeginEnergyEscrow(IndeterminateReentrantId, 5.0f));
	TestTrue(TEXT("Reservation enters indeterminate state"), ASC->MarkEnergyEscrowIndeterminate(IndeterminateReentrantId));
	bNestedResolveRejected = false;
	const FDelegateHandle IndeterminateHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (Change.NewValue < Change.OldValue)
				bNestedResolveRejected = !ASC->ResolveIndeterminateEnergyEscrow(IndeterminateReentrantId, true);
		});
	TestTrue(TEXT("Indeterminate authoritative result settles once"), ASC->ResolveIndeterminateEnergyEscrow(IndeterminateReentrantId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(IndeterminateHandle);
	TestTrue(TEXT("Reentrant indeterminate result is rejected while settling"), bNestedResolveRejected);
	TestEqual(TEXT("Indeterminate reentry cannot double charge"), ASC->GetActualEnergy(), 85.0f);

	TestTrue(TEXT("Energy reset before ordinary write-stack reentry"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 80.0f));
	const uint64 WriteStackId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Pending reservation starts before ordinary resource notification"), ASC->BeginEnergyEscrow(WriteStackId, 5.0f));
	bool bWriteStackResolveAttempted = false;
	bool bWriteStackResolveRejected = false;
	const FDelegateHandle WriteStackHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bWriteStackResolveAttempted && Change.NewValue > Change.OldValue)
			{
				bWriteStackResolveAttempted = true;
				bWriteStackResolveRejected = !ASC->ResolveEnergyEscrow(WriteStackId, true);
			}
		});
	TestTrue(TEXT("Outer coordinated Energy increase succeeds around a nested Resolve attempt"),
		ASC->RequestResourceChange(false, EGameplayModOp::Additive, 10.0f));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(WriteStackHandle);
	TestTrue(TEXT("Resolve is rejected during an ordinary resource write callback"), bWriteStackResolveRejected);
	TestEqual(TEXT("Outer resource write retains its intended actual value"), ASC->GetActualEnergy(), 90.0f);
	TestEqual(TEXT("Rejected nested Resolve leaves its reservation pending"), ASC->GetReservedEnergy(), 5.0f);
	TestFalse(TEXT("Rejected nested Resolve does not poison the ASC"), ASC->HasUnsupportedResourceWrite());
	TestTrue(TEXT("Pending transaction resolves once after the ordinary write exits"), ASC->ResolveEnergyEscrow(WriteStackId, true));
	TestEqual(TEXT("Deferred transaction charges exactly once"), ASC->GetActualEnergy(), 85.0f);

	TestTrue(TEXT("Energy reset before mixed pending and indeterminate reentry"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 100.0f));
	const uint64 SettlingId = ASC->AllocateEnergyTransactionId();
	const uint64 IndeterminateId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Settling transaction reservation starts"), ASC->BeginEnergyEscrow(SettlingId, 5.0f));
	TestTrue(TEXT("Indeterminate transaction reservation starts"), ASC->BeginEnergyEscrow(IndeterminateId, 5.0f));
	TestTrue(TEXT("Second reservation enters indeterminate state"), ASC->MarkEnergyEscrowIndeterminate(IndeterminateId));
	bool bNestedIndeterminateSuccessRejected = false;
	bool bNestedIndeterminateFailureRejected = false;
	const FDelegateHandle MixedStateHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (Change.NewValue < Change.OldValue)
			{
				bNestedIndeterminateSuccessRejected = !ASC->ResolveIndeterminateEnergyEscrow(IndeterminateId, true);
				bNestedIndeterminateFailureRejected = !ASC->ResolveIndeterminateEnergyEscrow(IndeterminateId, false);
			}
		});
	TestTrue(TEXT("Pending transaction commits while both uncertain callbacks are rejected"), ASC->ResolveEnergyEscrow(SettlingId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(MixedStateHandle);
	TestTrue(TEXT("Nested indeterminate success callback is rejected"), bNestedIndeterminateSuccessRejected);
	TestTrue(TEXT("Nested indeterminate failure callback is rejected"), bNestedIndeterminateFailureRejected);
	TestTrue(TEXT("Rejected callbacks leave the second transaction indeterminate"), ASC->HasIndeterminateEnergyEscrow(IndeterminateId));
	TestEqual(TEXT("Only the first reservation is released after its debit"), ASC->GetReservedEnergy(), 5.0f);
	TestFalse(TEXT("Rejected indeterminate callbacks do not poison the outer commit"), ASC->HasUnsupportedResourceWrite());
	TestTrue(TEXT("Indeterminate success remains usable after the settlement stack exits"),
		ASC->ResolveIndeterminateEnergyEscrow(IndeterminateId, true));
	TestEqual(TEXT("Both transactions settle once after callback reentry"), ASC->GetActualEnergy(), 90.0f);

	TestTrue(TEXT("Energy reset before conflicting queued-write case"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 80.0f));
	const uint64 ConflictReservationId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reservation starts before queued conflict"), ASC->BeginEnergyEscrow(ConflictReservationId, 20.0f));
	bool bConflictingWriteAcceptedForRevalidation = false;
	const FDelegateHandle ConflictWriteHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (Change.NewValue > Change.OldValue)
			{
				bConflictingWriteAcceptedForRevalidation = ASC->RequestResourceChange(false, EGameplayModOp::Override, 0.0f);
			}
		});
	AddExpectedMessage(TEXT("[SkillLabResource] Resource change rejected because it would invalidate active Energy reservations"),
		ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1, false);
	TestTrue(TEXT("Outer write succeeds while conflicting callback write is queued"),
		ASC->RequestResourceChange(false, EGameplayModOp::Additive, 10.0f));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(ConflictWriteHandle);
	TestTrue(TEXT("Conflicting callback write was accepted for deferred revalidation"), bConflictingWriteAcceptedForRevalidation);
	TestEqual(TEXT("Rejected conflict cannot alter outer actual Energy"), ASC->GetActualEnergy(), 90.0f);
	TestEqual(TEXT("Rejected conflict preserves its reservation"), ASC->GetReservedEnergy(), 20.0f);
	TestFalse(TEXT("Revalidated conflicting write is removed from the pending queue"), ASC->HasPendingResourceChanges());
	TestTrue(TEXT("Conflict reservation remains explicitly closable"), ASC->ResolveEnergyEscrow(ConflictReservationId, false));

	const uint64 BypassId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Bypass reservation starts"), ASC->BeginEnergyEscrow(BypassId, 5.0f));
	bool bBypassInjected = false;
	const FDelegateHandle BypassHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bBypassInjected && Change.NewValue < Change.OldValue)
			{
				bBypassInjected = true;
				TestTrue(TEXT("Resource change from debit callback is accepted for revalidation"),
					ASC->RequestResourceChange(false, EGameplayModOp::Additive, 5.0f));
				ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 50.0f);
			}
		});
	AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write during coordinated resource change"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated the expected GAS value or reservation invariant"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Native bypass during debit is not reported as a confirmed commit"), ASC->ResolveEnergyEscrow(BypassId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(BypassHandle);
	TestTrue(TEXT("Native bypass is detected during a coordinated write"), ASC->HasUnsupportedResourceWrite());
	TestTrue(TEXT("Bypassed charge remains explicitly indeterminate"), ASC->HasIndeterminateEnergyEscrow(BypassId));
	TestFalse(TEXT("Failed settlement drains its queued resource change"), ASC->HasPendingResourceChanges());
	TestEqual(TEXT("Reentrant change is revalidated and applied after the indeterminate exit"), ASC->GetActualEnergy(), 55.0f);
	TestFalse(TEXT("Repeated success cannot retry an already attempted debit"), ASC->ResolveIndeterminateEnergyEscrow(BypassId, true));
	TestFalse(TEXT("Repeated authoritative success remains idempotent"), ASC->ResolveIndeterminateEnergyEscrow(BypassId, true));
	TestEqual(TEXT("Repeated success does not debit after an uncertain applied attempt"), ASC->GetActualEnergy(), 55.0f);
	TestFalse(TEXT("Unconfirmed attempted debit cannot be released as a failed action"), ASC->ResolveIndeterminateEnergyEscrow(BypassId, false));
	TestTrue(TEXT("Unconfirmed attempted debit retains reservation isolation"), ASC->HasIndeterminateEnergyEscrow(BypassId));
	TestEqual(TEXT("Bypass side effect remains visible and is not falsely restored"), ASC->GetActualEnergy(), 55.0f);

	Owner->Destroy();
	return true;
}

#endif
