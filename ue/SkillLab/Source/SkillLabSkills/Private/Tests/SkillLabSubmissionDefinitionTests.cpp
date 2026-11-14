#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillRuntimeComponent.h"

namespace
{
	const TCHAR* SubmissionLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_Finish() end");

	FString MakeSubmissionCatalog(const FString& ExtraFields = FString())
	{
		return FString::Printf(TEXT(R"({"skills":[{"id":"Skill_SubmissionFixture","luaScript":"fixture.lua","cost":1,"cooldown":0,"magnitude":1,"range":500%s}]})"), *ExtraFields);
	}

	bool ParseSubmissionCatalog(const FString& Json, TMap<FName, FSkillLabSkillDefinition>& Out, FString& Error)
	{
		return FSkillLabSkillDefinitionProvider::ParseCatalogText(Json, {{TEXT("fixture.lua"), SubmissionLua}}, Out, Error);
	}

	bool ParseSubmissionCatalog(const FString& Json, const FString& Lua, TMap<FName, FSkillLabSkillDefinition>& Out, FString& Error)
	{
		return FSkillLabSkillDefinitionProvider::ParseCatalogText(Json, {{TEXT("fixture.lua"), Lua}}, Out, Error);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabSubmissionDefinitionTest,
	"SkillLab.Skills.SubmissionDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabSubmissionDefinitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Error;
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	FSkillLabCastExecutionToken OriginalToken;
	USkillLabSkillRuntimeComponent* TokenRuntime = NewObject<USkillLabSkillRuntimeComponent>();
	OriginalToken.Runtime = TokenRuntime;
	OriginalToken.RuntimeGeneration = 73;
	OriginalToken.CastId = 29;
	FSkillLabCastExecutionToken CopiedToken;
	FSkillLabCastExecutionToken::StaticStruct()->CopyScriptStruct(&CopiedToken, &OriginalToken);
	TestEqual(TEXT("Reflected token copies preserve runtime generation"), CopiedToken.RuntimeGeneration, uint64(73));
	TestEqual(TEXT("Reflected token copies preserve Cast id"), CopiedToken.CastId, 29);
	TestTrue(TEXT("Reflected token copies preserve Runtime identity"), CopiedToken.Runtime == TokenRuntime);
	FSkillLabSkillResult OriginalResult;
	OriginalResult.CastToken = OriginalToken;
	FSkillLabSkillResult CopiedResult;
	FSkillLabSkillResult::StaticStruct()->CopyScriptStruct(&CopiedResult, &OriginalResult);
	TestEqual(TEXT("Reflected result copies preserve token generation"), CopiedResult.CastToken.RuntimeGeneration, uint64(73));
	TestEqual(TEXT("Reflected result copies preserve token Cast id"), CopiedResult.CastToken.CastId, 29);
	TestTrue(TEXT("Reflected result copies preserve token Runtime identity"), CopiedResult.CastToken.Runtime == TokenRuntime);

	FString ProductionCatalog;
	const FString CatalogPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SkillDefinitions.json"));
	TestTrue(TEXT("Existing production catalog is readable"), FFileHelper::LoadFileToString(ProductionCatalog, *CatalogPath));
	TestTrue(TEXT("The four existing production skill definitions remain parseable without edits"),
		FSkillLabSkillDefinitionProvider::ParseCatalogText(ProductionCatalog, {}, Definitions, Error));
	if (Definitions.Num() == 4)
	{
		for (const FName SkillId : { FName(TEXT("Skill_Fireball")), FName(TEXT("Skill_AreaBurst")), FName(TEXT("Skill_Mend")), FName(TEXT("Skill_BurningBuff")) })
		{
			const FSkillLabSkillDefinition* Definition = Definitions.Find(SkillId);
			TestNotNull(FString::Printf(TEXT("Legacy definition %s exists"), *SkillId.ToString()), Definition);
			if (Definition)
			{
				TestFalse(FString::Printf(TEXT("%s uses implicit compatibility point"), *SkillId.ToString()), Definition->bHasExplicitSubmissionPoints);
				TestEqual(FString::Printf(TEXT("%s has one compatibility point"), *SkillId.ToString()), Definition->SubmissionPoints.Num(), 1);
				if (Definition->SubmissionPoints.Num() == 1)
				{
					TestEqual(FString::Printf(TEXT("%s compatibility point id"), *SkillId.ToString()), Definition->SubmissionPoints[0].Id, 1);
					TestEqual(FString::Printf(TEXT("%s compatibility trigger"), *SkillId.ToString()), Definition->SubmissionPoints[0].Trigger, ESkillLabSubmissionTrigger::Script);
				}
			}
		}
	}

	const FString MultiPoint = MakeSubmissionCatalog(TEXT(",\"active\":2,\"submissionPoints\":[{\"id\":17,\"trigger\":\"Release\",\"target\":\"RequestTarget\"},{\"id\":42,\"trigger\":\"Timer\",\"delay\":0.25,\"target\":\"RequestTarget\"},{\"id\":99,\"trigger\":\"GameplayEvent\",\"eventTag\":\"SkillLab.Event.Attack.Heavy\",\"target\":\"RequestTarget\"}],\"completionMode\":\"AllSubmissionPointsResolved\""));
	Definitions.Reset(); Error.Reset();
	TestTrue(TEXT("Explicit multi-point catalog parses through the public entry"), ParseSubmissionCatalog(MultiPoint, Definitions, Error));
	if (const FSkillLabSkillDefinition* Definition = Definitions.Find(TEXT("Skill_SubmissionFixture")))
	{
		TestTrue(TEXT("Explicit declaration is distinguished from legacy content"), Definition->bHasExplicitSubmissionPoints);
		TestEqual(TEXT("All point declarations are retained"), Definition->SubmissionPoints.Num(), 3);
		TestEqual(TEXT("Stable point ids are not renumbered"), Definition->SubmissionPoints[0].Id, 17);
		TestEqual(TEXT("Timer point id is retained"), Definition->SubmissionPoints[1].Id, 42);
		TestEqual(TEXT("Timer delay is normalized"), Definition->SubmissionPoints[1].DelaySeconds, 0.25f);
		TestTrue(TEXT("GameplayEvent submission tag enters the Ability event set"), Definition->BehaviorEventTags.HasTagExact(FGameplayTag::RequestGameplayTag(FName(TEXT("SkillLab.Event.Attack.Heavy")), false)));
		TestEqual(TEXT("Completion mode is normalized"), Definition->CompletionMode, ESkillLabCompletionMode::AllSubmissionPointsResolved);
	}

	const FString RuleProfile = TEXT(R"({"id":"AllowFixture","skill":"Allow","dodge":"Buffer","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue","abilityCategories":{"Ability.SkillLab.Utility":"Allow"}})");
	const FString LuaWithRules = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\",\"combatRules\",\"AllowFixture\",\"recoveryRules\",\"AllowFixture\")\nfunction OnSkillCast(context) SkillLabSkill_Finish() end");
	const FString RulesCatalog = FString(TEXT("{\"combatRules\":[")) + RuleProfile + TEXT("],\"skills\":[{\"id\":\"Skill_SubmissionFixture\",\"luaScript\":\"fixture.lua\",\"cost\":1,\"cooldown\":0,\"magnitude\":1,\"range\":500}]}");
	Definitions.Reset(); Error.Reset();
	TestTrue(TEXT("Allow is accepted for skill requests and explicit ability categories"), ParseSubmissionCatalog(RulesCatalog, LuaWithRules, Definitions, Error));
	if (const FSkillLabSkillDefinition* Definition = Definitions.Find(TEXT("Skill_SubmissionFixture")))
	{
		TestEqual(TEXT("Skill request decision normalizes to Allow"), Definition->OperationRules.Skill, ESkillLabRuleDecision::Allow);
		TestEqual(TEXT("Ability category decision normalizes to Allow"), Definition->OperationRules.AbilityCategories.FindRef(FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.SkillLab.Utility")), false)), ESkillLabRuleDecision::Allow);
	}

	auto ExpectRejected = [&](const TCHAR* Description, const FString& Json, const TCHAR* ErrorText)
	{
		TMap<FName, FSkillLabSkillDefinition> Candidate;
		FString LocalError;
		TestFalse(Description, ParseSubmissionCatalog(Json, Candidate, LocalError));
		TestTrue(FString::Printf(TEXT("%s has a specific diagnostic"), Description), LocalError.Contains(ErrorText));
	};
	ExpectRejected(TEXT("Empty point lists are rejected"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[]")), TEXT("nonempty array"));
	ExpectRejected(TEXT("Fractional point ids are rejected"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1.5,\"trigger\":\"Script\",\"target\":\"RequestTarget\"}]")), TEXT("positive int32"));
	ExpectRejected(TEXT("Overflow point ids are rejected"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":2147483648,\"trigger\":\"Script\",\"target\":\"RequestTarget\"}]")), TEXT("positive int32"));
	ExpectRejected(TEXT("Duplicate point ids are rejected"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":7,\"trigger\":\"Script\",\"target\":\"RequestTarget\"},{\"id\":7,\"trigger\":\"Release\",\"target\":\"RequestTarget\"}]")), TEXT("duplicate submission point id"));
	ExpectRejected(TEXT("Unknown triggers are rejected"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1,\"trigger\":\"Notify\",\"target\":\"RequestTarget\"}]")), TEXT("trigger must be"));
	ExpectRejected(TEXT("Timer points require a bounded delay"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1,\"trigger\":\"Timer\",\"target\":\"RequestTarget\"}]")), TEXT("delay must be"));
	ExpectRejected(TEXT("Timer delay is rejected for other trigger types"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1,\"trigger\":\"Release\",\"delay\":0.1,\"target\":\"RequestTarget\"}]")), TEXT("delay is only valid for Timer"));
	ExpectRejected(TEXT("Gameplay event points require an event tag"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1,\"trigger\":\"GameplayEvent\",\"target\":\"RequestTarget\"}]")), TEXT("submissionPoints[0].eventTag is required for GameplayEvent trigger"));
	ExpectRejected(TEXT("Unregistered gameplay event tags are rejected"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1,\"trigger\":\"GameplayEvent\",\"eventTag\":\"SkillLab.Event.Unknown\",\"target\":\"RequestTarget\"}]")), TEXT("registered SkillLab.Event child tag"));
	ExpectRejected(TEXT("Submission event tags cannot reuse the release event role"), MakeSubmissionCatalog(TEXT(",\"releaseTrigger\":\"GameplayEvent\",\"releaseEventTag\":\"SkillLab.Event.Cast\",\"submissionPoints\":[{\"id\":1,\"trigger\":\"GameplayEvent\",\"eventTag\":\"SkillLab.Event.Cast\",\"target\":\"RequestTarget\"}]")), TEXT("conflicts with releaseEventTag"));
	ExpectRejected(TEXT("Submission event tags cannot reuse a rule window event"), FString(TEXT("{\"combatRules\":[")) + RuleProfile + TEXT("],\"skills\":[{\"id\":\"Skill_SubmissionFixture\",\"luaScript\":\"fixture.lua\",\"cost\":1,\"cooldown\":0,\"magnitude\":1,\"range\":500,\"active\":1,\"ruleWindows\":[{\"id\":\"Window\",\"beginEvent\":\"SkillLab.Event.Attack.Normal\",\"endEvent\":\"SkillLab.Event.Window.Close\",\"rules\":\"AllowFixture\"}],\"submissionPoints\":[{\"id\":1,\"trigger\":\"GameplayEvent\",\"eventTag\":\"SkillLab.Event.Attack.Normal\",\"target\":\"RequestTarget\"}]}]}"), TEXT("ruleWindows require unique ids"));
	ExpectRejected(TEXT("Target binding must match the declared shape"), MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[{\"id\":1,\"trigger\":\"Script\",\"target\":\"RequestLocation\"}]")), TEXT("incompatible with the skill target shape"));
	ExpectRejected(TEXT("Completion by all points requires explicit points"), MakeSubmissionCatalog(TEXT(",\"completionMode\":\"AllSubmissionPointsResolved\"")), TEXT("requires explicit submissionPoints"));
	ExpectRejected(TEXT("Completion event cannot bypass all-points completion"), MakeSubmissionCatalog(TEXT(",\"active\":1,\"completionEventTag\":\"SkillLab.Event.Cast.Complete\",\"completionMode\":\"AllSubmissionPointsResolved\",\"submissionPoints\":[{\"id\":1,\"trigger\":\"Script\",\"target\":\"RequestTarget\"}]")), TEXT("cannot use completionEventTag"));
	ExpectRejected(TEXT("Allow cannot leak into movement decisions"), FString(TEXT("{\"combatRules\":[")) + RuleProfile.Replace(TEXT("\"movement\":\"Continue\""), TEXT("\"movement\":\"Allow\"")) + TEXT("],\"skills\":[{\"id\":\"Skill_SubmissionFixture\",\"luaScript\":\"fixture.lua\",\"cost\":1,\"cooldown\":0,\"magnitude\":1,\"range\":500}]}"), TEXT("invalid decision"));

	TMap<FName, FSkillLabSkillDefinition> Retained;
	Error.Reset();
	TestTrue(TEXT("A valid candidate parses before rejection testing"), ParseSubmissionCatalog(MakeSubmissionCatalog(), Retained, Error));
	const TMap<FName, FSkillLabSkillDefinition> BeforeRejectedCandidate = Retained;
	Error.Reset();
	TestFalse(TEXT("Bad candidate catalog is rejected"), ParseSubmissionCatalog(MakeSubmissionCatalog(TEXT(",\"submissionPoints\":[]")), Retained, Error));
	TestEqual(TEXT("Failed parse leaves caller catalog unchanged"), Retained.Num(), BeforeRejectedCandidate.Num());
	TestTrue(TEXT("Failed parse retains the prior definition"), Retained.Contains(TEXT("Skill_SubmissionFixture")));
	return true;
}

#endif
