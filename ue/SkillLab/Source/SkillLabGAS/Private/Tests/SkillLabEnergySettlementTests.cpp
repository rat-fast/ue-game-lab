#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabEnergySettlementTest, "SkillLab.GAS.EnergySettlementHistory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabEnergySettlementTest::RunTest(const FString&)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Independent resource world created"), World)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	AActor* Actor = World->SpawnActor<AActor>();
	auto* ASC = NewObject<USkillLabAbilitySystemComponent>(Actor);
	Actor->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	ASC->InitializeSkillLabAbilityActorInfo(Actor, Actor);
	if (!ASC->HasBegunPlay()) ASC->BeginPlay();
	const uint64 Transaction = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Real reservation starts"), ASC->BeginEnergyEscrow(Transaction, 2.0f, 7, 11));
	TestTrue(TEXT("Operation binds once before execution"), ASC->BindEnergyOperation(Transaction, Actor, Actor, 21));
	TestFalse(TEXT("Operation cannot be rebound to another producer"), ASC->BindEnergyOperation(Transaction, Actor, Actor, 22));
	const auto Reserved = ASC->QueryEnergySettlement(Transaction);
	TestEqual(TEXT("Query distinguishes never-debited reservation"), Reserved.State, ESkillLabEnergySettlementState::ReservedNoDebit);
	TestEqual(TEXT("Query retains original cost"), Reserved.Amount, 2.0f);
	TestEqual(TEXT("Query retains original operation"), Reserved.Operation, uint64(21));
	bool bSettlingObserved = false, bReentryRejected = false;
	int32 Debits = 0;
	const auto Handle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (Data.NewValue >= Data.OldValue) return;
			++Debits;
			bSettlingObserved = ASC->QueryEnergySettlement(Transaction).State == ESkillLabEnergySettlementState::SettlementInProgress;
			bReentryRejected = !ASC->ResolveEnergyEscrow(Transaction, true) && !ASC->ResolveEnergyEscrow(Transaction, false);
			ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
		});
	TestTrue(TEXT("Existing debit algorithm closes normally"), ASC->ResolveEnergyEscrow(Transaction, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Handle);
	const auto Committed = ASC->QueryEnergySettlement(Transaction);
	TestTrue(TEXT("Delegate query cannot announce a not-yet-validated debit"), bSettlingObserved && bReentryRejected);
	TestEqual(TEXT("Exactly one debit notification"), Debits, 1);
	TestEqual(TEXT("Closed history survives queued resource change"), Committed.State, ESkillLabEnergySettlementState::ConfirmedCommitted);
	TestEqual(TEXT("Queue has applied independently of original debit"), ASC->GetActualEnergy(), 99.0f);
	TestEqual(TEXT("History identity is immutable"), Committed.Generation, uint64(7));
	TestFalse(TEXT("No second debit after closure"), ASC->ResolveEnergyEscrow(Transaction, true));
	TestEqual(TEXT("Queries do not change confirmed version"), ASC->QueryEnergySettlement(Transaction).Version, Committed.Version);
	TestEqual(TEXT("Missing transaction is unavailable, not committed"), ASC->QueryEnergySettlement(9999).State, ESkillLabEnergySettlementState::Unavailable);
	const uint64 ReleasedId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Failure fixture reserves without debit"), ASC->BeginEnergyEscrow(ReleasedId, 2.0f));
	TestTrue(TEXT("Explicit failure releases through existing algorithm"), ASC->ResolveEnergyEscrow(ReleasedId, false));
	TestEqual(TEXT("Release has explicit history"), ASC->QueryEnergySettlement(ReleasedId).State, ESkillLabEnergySettlementState::ConfirmedReleased);
	TestEqual(TEXT("Release did not change actual energy"), ASC->GetActualEnergy(), 99.0f);

	const uint64 UnknownId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Unconfirmed debit fixture reserves"), ASC->BeginEnergyEscrow(UnknownId, 2.0f));
	bool bInjected = false;
	AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
	const auto Bypass = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Data)
		{
			if (bInjected || Data.NewValue >= Data.OldValue) return;
			bInjected = true;
			ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 50.0f);
		});
	TestFalse(TEXT("Bypass injection does not fake closure"), ASC->ResolveEnergyEscrow(UnknownId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Bypass);
	TestEqual(TEXT("Observed debit is not confirmed settlement"), ASC->QueryEnergySettlement(UnknownId).State,
		ESkillLabEnergySettlementState::DebitAttemptUnconfirmed);
	TestFalse(TEXT("Unknown attempted debit cannot be retried"), ASC->ResolveIndeterminateEnergyEscrow(UnknownId, true));
	TestFalse(TEXT("Unknown attempted debit cannot be released as failed"), ASC->ResolveIndeterminateEnergyEscrow(UnknownId, false));
	TestEqual(TEXT("Unknown reservation is retained"), ASC->GetReservedEnergy(), 2.0f);
	TestEqual(TEXT("No guessed refund or second debit"), ASC->GetActualEnergy(), 50.0f);
	ASC->EndPlay(EEndPlayReason::RemovedFromWorld);
	TestEqual(TEXT("Ended but valid resource owner cannot confirm retained history"), ASC->QueryEnergySettlement(Transaction).State,
		ESkillLabEnergySettlementState::Unavailable);
	ASC->DestroyComponent();
	TestEqual(TEXT("Destroyed resource owner cannot confirm retained history"), ASC->QueryEnergySettlement(Transaction).State,
		ESkillLabEnergySettlementState::Unavailable);
	Actor->Destroy();
	World->DestroyWorld(false);
	if (GEngine) GEngine->DestroyWorldContext(World);
	return true;
}
#endif
