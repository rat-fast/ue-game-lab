#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastRuleWindowTransitionsTest, "SkillLab.Skills.CastRuleWindowTransitions",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastRuleWindowTransitionsTest::RunTest(const FString&)
{
	FSkillLabCastScenarioFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	const auto Definition = Fixture.FixtureDefinitions.FindChecked(TEXT("Skill_Mend"));
	auto Request = [&]() { return Fixture.Submit(Unit, Definition.SkillId); };
	auto& StateFixtureJson = Fixture.StateFixtureJson;
	auto& LuaSources = Fixture.LuaSources;
	auto& FixtureDefinitions = Fixture.FixtureDefinitions;
	auto& FixtureError = Fixture.FixtureError;
	auto& ImmediateDefinitions = Fixture.ImmediateDefinitions;
	auto& IndependentDefinitions = Fixture.IndependentDefinitions;
	auto& HoldDefinitions = Fixture.HoldDefinitions;
	auto& UtilityDefinitions = Fixture.UtilityDefinitions;
	{
		TMap<FString, FString> MissingConflictSources = LuaSources;
		MissingConflictSources[TEXT("Skill_Mend.lua")] = Fixture.LuaSources.FindChecked(TEXT("Skill_Mend.lua")).Replace(TEXT(",\"windowConflict\",\"Reject\""), TEXT(""));
		TMap<FName, FSkillLabSkillDefinition> Invalid;
		FString Error;
		TestFalse(TEXT("Overlapping-capable windows cannot use an implicit conflict policy"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
			StateFixtureJson, MissingConflictSources, Invalid, Error));
		TestTrue(TEXT("Missing conflict policy identifies the declaration"), Error.Contains(TEXT("windowConflict")));
		Error.Reset();
		TestFalse(TEXT("Duplicate window identities are rejected at load"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
			StateFixtureJson.Replace(TEXT("\"id\":\"OtherWindow\""), TEXT("\"id\":\"RestrictedWindow\"")), LuaSources, Invalid, Error));
	}
	FSkillLabSkillRequest StateRequest;
	StateRequest.Caster = Owner;
	StateRequest.SkillId = TEXT("Skill_Mend");
	const FSkillLabSkillResult StateCast = Runtime->SubmitSkillRequest(StateRequest);
	FSkillLabSkillRequest UtilityRequest = StateRequest;
	UtilityRequest.SkillId = TEXT("Skill_Utility");
	TestEqual(TEXT("Unspecified registered category is rejected explicitly"), Runtime->SubmitSkillRequest(UtilityRequest).Reason, FName(TEXT("UnknownAbilityCategory")));
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Marked, 2);
	TestEqual(TEXT("A state outside the current profile does not interrupt"), Runtime->GetActiveCastId(), StateCast.CastId);
	TestTrue(TEXT("Window event is accepted for state reevaluation"), Runtime->ForwardValidatedNotify(StateCast.CastId, SkillLabGASTags::Event_Window_Open));
	TestEqual(TEXT("Opening an interrupting window reevaluates an existing state"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Marked, 1);
	const FSkillLabSkillResult StateCastTwo = Runtime->SubmitSkillRequest(StateRequest);
	TestTrue(TEXT("State remains noninterrupting outside the window"), StateCastTwo.bSuccess);
	Runtime->ForwardValidatedNotify(StateCastTwo.CastId, SkillLabGASTags::Event_Window_Open);
	TestEqual(TEXT("Remaining state count still interrupts in the window"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Marked, 1);
	const FSkillLabSkillResult StateCastThree = Runtime->SubmitSkillRequest(StateRequest);
	Runtime->ForwardValidatedNotify(StateCastThree.CastId, SkillLabGASTags::Event_Window_Open);
	TestEqual(TEXT("Removing the last state count restores the window"), Runtime->GetActiveCastId(), StateCastThree.CastId);
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Marked);
	TestEqual(TEXT("A state gained inside the window interrupts"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Marked);
	Runtime->ResetSkills();
	TMap<FName, FSkillLabSkillDefinition> ModifiedProfiles;
	TestTrue(TEXT("Shared profile modification parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		StateFixtureJson.Replace(TEXT("\"movement\":\"Continue\""), TEXT("\"movement\":\"Reject\"")), LuaSources, ModifiedProfiles, FixtureError));
	// Keep unrelated fixture declarations in the complete candidate catalog.
	ModifiedProfiles.Append(ImmediateDefinitions);
	ModifiedProfiles.Append(IndependentDefinitions);
	ModifiedProfiles.Append(HoldDefinitions);
	ModifiedProfiles.Append(UtilityDefinitions);
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(ModifiedProfiles);
	for (const FName Skill : {FName(TEXT("Skill_Mend")), FName(TEXT("Skill_BufferedVictim")), FName(TEXT("Skill_BufferedSuccess"))})
	{
		FSkillLabSkillRequest ProfileRequest;
		ProfileRequest.Caster = Owner;
		ProfileRequest.SkillId = Skill;
		TestTrue(TEXT("Profile fixture activates formally"), Runtime->SubmitSkillRequest(ProfileRequest).bSuccess);
		FName RuleReason;
		TestFalse(TEXT("Selected profile controls movement"), Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, RuleReason));
		TestEqual(TEXT("Shared modification preserves the unrelated rule identity"), RuleReason,
			Skill == TEXT("Skill_BufferedSuccess") ? FName(TEXT("Restricted")) : FName(TEXT("Shared")));
		Runtime->CancelActiveCast(TEXT("ProfileFixtureCleanup"));
	}
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(FixtureDefinitions);
	const auto WindowCast = Request();
	TestTrue(TEXT("Window begin uses the current Ability"), Runtime->ForwardValidatedNotify(WindowCast.CastId, SkillLabGASTags::Event_Window_Open));
	TestTrue(TEXT("Window is observable"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	TestEqual(TEXT("Window profile matches the declared ability category"), Runtime->SubmitSkillRequest(UtilityRequest).Reason, FName(TEXT("Restricted")));
	FName MovementReason;
	TestFalse(TEXT("Window profile rejects movement"), Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, MovementReason));
	TestEqual(TEXT("Rejected movement identifies the selected rule"), MovementReason, FName(TEXT("Restricted")));
	TestEqual(TEXT("Window profile rejects a skill without buffering"), Request().Reason, FName(TEXT("Restricted")));
	TestTrue(TEXT("Second window can open"), Runtime->ForwardValidatedNotify(WindowCast.CastId, SkillLabGASTags::Event_Window_AlternateOpen));
	TestEqual(TEXT("Overlapping rules report conflict"), Request().Reason, FName(TEXT("RuleWindowConflict")));
	TestTrue(TEXT("Second window closes"), Runtime->ForwardValidatedNotifyStateEnd(WindowCast.CastId, SkillLabGASTags::Event_Window_AlternateOpen));
	TestTrue(TEXT("Notify state end closes first window"), Runtime->ForwardValidatedNotifyStateEnd(WindowCast.CastId, SkillLabGASTags::Event_Window_Open));
	TestFalse(TEXT("Closed window is observable"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	Runtime->ForwardValidatedNotify(WindowCast.CastId, SkillLabGASTags::Event_Window_Open);
	TestFalse(TEXT("Duplicate begin does not reopen window"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	return true;
}
#endif
