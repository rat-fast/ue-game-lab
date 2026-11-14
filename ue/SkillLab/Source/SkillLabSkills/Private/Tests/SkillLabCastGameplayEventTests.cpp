#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastGameplayEventTest, "SkillLab.Skills.CastGameplayEvent",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastGameplayEventTest::RunTest(const FString&)
{
	FSkillLabCastScenarioFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	auto* World = Unit.Actor->GetWorld();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	FSkillLabSkillDefinition Definition;
	if (!TestTrue(TEXT("Self-heal fixture resolves"), FSkillLabSkillDefinitionProvider::Resolve(TEXT("Skill_Mend"), Definition))) return false;
	auto Request = [&]() { return Fixture.Submit(Unit, Definition.SkillId); };
	int32 ReleaseCount = 0, EffectCount = 0;
	const auto Handle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{
		if (Result.Event == ESkillLabSkillEvent::PhaseChanged && Result.Stage == ESkillLabCastStage::Active) ++ReleaseCount;
		if (Result.Event == ESkillLabSkillEvent::EffectApplied) ++EffectCount;
	});
	ON_SCOPE_EXIT { Runtime->OnNativeSkillEvent.Remove(Handle); };
	const float InitialEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult First = Request();
	TestTrue(TEXT("GAS Ability activation accepts the cast"), First.bSuccess);
	TestEqual(TEXT("Two skill definitions share a rules profile"), Fixture.FixtureDefinitions.FindChecked(TEXT("Skill_Mend")).OperationRules.Id,
		Fixture.FixtureDefinitions.FindChecked(TEXT("Skill_BufferedVictim")).OperationRules.Id);
	TestEqual(TEXT("No release before valid event"), ReleaseCount, 0);
	TestEqual(TEXT("No cost before valid event"), Runtime->GetEnergyValue(), InitialEnergy);
	FGameplayEventData BarePayload;
	BarePayload.EventTag = Definition.ReleaseEventTag;
	BarePayload.Instigator = Owner;
	BarePayload.EventMagnitude = static_cast<float>(First.CastId);
	ASC->HandleGameplayEvent(Definition.ReleaseEventTag, &BarePayload);
	TestEqual(TEXT("Bare ASC event cannot borrow current cast identity"), ReleaseCount, 0);
	TestFalse(TEXT("Wrong tag is rejected"), Runtime->ForwardValidatedNotify(First.CastId, FGameplayTag()));
	TestFalse(TEXT("Wrong cast is rejected"), Runtime->ForwardValidatedNotify(First.CastId + 1, Definition.ReleaseEventTag));
	TestTrue(TEXT("Validated Notify traverses ASC and current Ability"), Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag));
	TestEqual(TEXT("Current Ability advances exactly one release"), ReleaseCount, 1);
	TestEqual(TEXT("Authoritative effect is submitted"), EffectCount, 1);
	TestEqual(TEXT("Cost is committed once"), Runtime->GetEnergyValue(), InitialEnergy - Definition.Cost);
	Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag);
	TestEqual(TEXT("Duplicate event cannot release twice"), ReleaseCount, 1);
	TestEqual(TEXT("Duplicate event cannot submit effect twice"), EffectCount, 1);
	TestEqual(TEXT("Cast enters Recovery after successful behavior"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	const FSkillLabSkillResult RecoveryRequest = Request();
	TestFalse(TEXT("Recovery buffers but does not activate new skill"), RecoveryRequest.bSuccess);
	TestEqual(TEXT("Recovery request remains latest-wins buffered"), RecoveryRequest.Reason, FName(TEXT("InputBufferedLatestWins")));
	TestEqual(TEXT("Recovery buffer does not start another Cast"), Runtime->GetActiveCastId(), First.CastId);

	FActorSpawnParameters AttackerParameters;
	AttackerParameters.Name = TEXT("SkillLabCombatRulesAttacker");
	AActor* Attacker = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, AttackerParameters);
	USkillLabAbilitySystemComponent* AttackerASC = NewObject<USkillLabAbilitySystemComponent>(Attacker);
	Attacker->AddInstanceComponent(AttackerASC);
	AttackerASC->RegisterComponent();
	AttackerASC->InitializeSkillLabAbilityActorInfo(Attacker, Attacker);
	AttackerASC->ApplyDamageToTarget(ASC, 1.0f, nullptr, SkillLabGASTags::Event_Attack_Normal);
	TestEqual(TEXT("Normal categorized hit does not interrupt"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	AttackerASC->ApplyDamageToTarget(ASC, 1.0f, nullptr, FGameplayTag());
	TestEqual(TEXT("Unclassified hit does not interrupt"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	AttackerASC->ApplyDamageToTarget(ASC, 1.0f, nullptr, SkillLabGASTags::Event_Attack_Heavy);
	TestEqual(TEXT("Heavy categorized hit interrupts Recovery Cast"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Post-commit hit interruption does not refund energy"), Runtime->GetEnergyValue(), InitialEnergy - Definition.Cost);

	Runtime->ResetSkills();
	TestFalse(TEXT("Reset clears cached intent"), FSkillLabCastScenarioFixture::HasIntent(Runtime));
	const FSkillLabSkillResult Second = Request();
	TestTrue(TEXT("New cast activates after Reset"), Second.bSuccess);
	TestTrue(TEXT("Cast identity is monotonic across Reset"), Second.CastId > First.CastId);
	TestFalse(TEXT("Old Notify cannot advance new cast"), Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag));
	TestEqual(TEXT("New cast still waits"), Runtime->GetCastStage(), ESkillLabCastStage::Windup);
	TestTrue(TEXT("New cast can open its own window"), Runtime->ForwardValidatedNotify(Second.CastId, SkillLabGASTags::Event_Window_Open));
	TestFalse(TEXT("Old cast cannot close the new cast window"), Runtime->ForwardValidatedNotifyStateEnd(First.CastId, SkillLabGASTags::Event_Window_Open));
	TestTrue(TEXT("New cast window remains open after stale end"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	TestFalse(TEXT("Cancellation clears the active window"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	TestFalse(TEXT("Cancelled cast rejects Notify"), Runtime->ForwardValidatedNotify(Second.CastId, Definition.ReleaseEventTag));
	Runtime->ResetSkills();
	return true;
}
#endif
