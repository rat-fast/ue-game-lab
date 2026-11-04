#if WITH_DEV_AUTOMATION_TESTS

#include "Abilities/GameplayAbility.h"
#include "Misc/AutomationTest.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillDefinitionProvider.h"

namespace
{
	FString MakeJson(const FString& SkillId = TEXT("Skill_Test"), const FString& ExtraFields = FString())
	{
		return FString::Printf(TEXT(R"({"skills":[{"id":"%s","luaScript":"fixture.lua","cost":10,"cooldown":1,"magnitude":20,"powerCoefficient":0.5,"range":900,"windup":0.25,"recovery":0.2%s}]})"),
			*SkillId, *ExtraFields);
	}

	TMap<FString, FString> MakeLua(const FString& Declaration = TEXT(""))
	{
		TMap<FString, FString> Sources;
		Sources.Add(TEXT("fixture.lua"), Declaration.IsEmpty()
			? TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end")
			: Declaration);
		return Sources;
	}

	bool ParseFixture(const FString& Json, const TMap<FString, FString>& Sources,
		TMap<FName, FSkillLabSkillDefinition>& OutDefinitions, FString& OutError)
	{
		return FSkillLabSkillDefinitionProvider::ParseCatalogText(Json, Sources, OutDefinitions, OutError);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabDefinitionProviderValidationTest,
	"SkillLab.Skills.DefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabDefinitionProviderValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Error;
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	const FString ValidLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Ground\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"GroundArea\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
	const FString GroundJson = MakeJson(TEXT("Skill_AreaFixture"), TEXT(",\"radius\":350"));
	TestTrue(TEXT("Lua declaration and JSON numbers form one normalized definition"), ParseFixture(GroundJson, MakeLua(ValidLua), Definitions, Error));
	const FSkillLabSkillDefinition* Ground = Definitions.Find(TEXT("Skill_AreaFixture"));
	TestNotNull(TEXT("Normalized definition is present"), Ground);
	if (Ground)
	{
		TestEqual(TEXT("Lua owns target shape"), Ground->TargetShape, ESkillLabTargetShape::Ground);
		TestEqual(TEXT("Lua owns relation"), Ground->Relation, ESkillLabTargetRelation::Enemy);
		TestEqual(TEXT("Lua owns effect category"), Ground->Effect, ESkillLabEffectKind::Damage);
		TestEqual(TEXT("Lua selects an allowlisted execution template"), Ground->Execution, ESkillLabExecutionTemplate::GroundArea);
		TestEqual(TEXT("JSON range reaches normalized definition"), Ground->Range, 900.0f);
		TestEqual(TEXT("JSON radius reaches normalized definition"), Ground->Radius, 350.0f);
		TestTrue(TEXT("Only shared runtime classes are assigned"), Ground->AbilityClass && Ground->BehaviorClass);
	}

	const FString ProjectileLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"Projectile\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
	const FString ProjectileJson = MakeJson(TEXT("Skill_ProjectileFixture"), TEXT(",\"projectileSpeed\":1250,\"projectileCollisionRadius\":18,\"projectileLifetime\":5,\"projectileVisualScale\":0.62,\"projectileInterceptOtherUnits\":true"));
	Error.Reset();
	Definitions.Reset();
	TestTrue(TEXT("Projectile JSON tuning is accepted"), ParseFixture(ProjectileJson, MakeLua(ProjectileLua), Definitions, Error));
	if (const FSkillLabSkillDefinition* Projectile = Definitions.Find(TEXT("Skill_ProjectileFixture")))
	{
		TestEqual(TEXT("Projectile speed is normalized"), Projectile->ProjectileParameters.Speed, 1250.0f);
		TestEqual(TEXT("Projectile collision radius is normalized"), Projectile->ProjectileParameters.CollisionRadius, 18.0f);
		TestEqual(TEXT("Projectile lifetime is normalized"), Projectile->ProjectileParameters.Lifetime, 5.0f);
		TestEqual(TEXT("Projectile visual scale is normalized"), Projectile->ProjectileParameters.VisualScale, 0.62f);
		TestTrue(TEXT("Projectile interception choice is normalized"), Projectile->ProjectileParameters.bInterceptOtherUnits);
	}

	auto ExpectRejected = [&](const TCHAR* Label, const FString& Json, const FString& Lua, const FString& ErrorFragment = FString())
	{
		TMap<FName, FSkillLabSkillDefinition> Candidate;
		FString LocalError;
		const bool bAccepted = ParseFixture(Json, MakeLua(Lua), Candidate, LocalError);
		TestFalse(Label, bAccepted);
		TestTrue(FString::Printf(TEXT("%s reports a diagnostic"), Label), !LocalError.IsEmpty());
		if (!ErrorFragment.IsEmpty()) TestTrue(FString::Printf(TEXT("%s identifies the failure"), Label), LocalError.Contains(ErrorFragment));
	};

	const FString UnitLua = MakeLua()[TEXT("fixture.lua")];
	ExpectRejected(TEXT("Unknown JSON fields are rejected"), MakeJson(TEXT("Skill_Test"), TEXT(",\"unexpected\":1")), UnitLua, TEXT("unknown JSON field"));
	ExpectRejected(TEXT("Missing required range is rejected"), TEXT(R"({"skills":[{"id":"Skill_Test","luaScript":"fixture.lua","cost":1,"cooldown":0,"magnitude":1,"windup":0,"recovery":0}]})"), UnitLua, TEXT("range"));
	ExpectRejected(TEXT("JSON cannot duplicate Lua-owned classification"), MakeJson(TEXT("Skill_Test"), TEXT(",\"targetShape\":\"Unit\"")), UnitLua, TEXT("unknown JSON field"));
	ExpectRejected(TEXT("Duplicate Lua declaration keys are rejected"), MakeJson(), MakeLua(TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"targetShape\",\"Ground\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast() end"))[TEXT("fixture.lua")], TEXT("duplicate Lua declaration field"));
	ExpectRejected(TEXT("Unknown Lua declaration fields are rejected"), MakeJson(), MakeLua(TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\",\"abilityClass\",\"Anything\")\nfunction OnSkillCast() end"))[TEXT("fixture.lua")], TEXT("unknown Lua declaration field"));
	ExpectRejected(TEXT("Missing Lua declarations are rejected"), MakeJson(), TEXT("function OnSkillCast() end"), TEXT("missing SkillLabSkill_Define"));
	ExpectRejected(TEXT("Lua evaluation errors are rejected"), MakeJson(), TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\")\nerror(\"fixture error\")\nfunction OnSkillCast() end"), TEXT("evaluation failed"));
	ExpectRejected(TEXT("Unknown Lua classification is rejected"), MakeJson(), MakeLua(TEXT("SkillLabSkill_Define(\"targetShape\",\"Actor\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast() end"))[TEXT("fixture.lua")], TEXT("unknown targetShape"));
	ExpectRejected(TEXT("Wrong Lua declaration value types are rejected"), MakeJson(), TEXT("SkillLabSkill_Define(\"targetShape\",42,\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast() end"), TEXT("must contain strings"));
	ExpectRejected(TEXT("Invalid Lua target/execution combinations are rejected"), MakeJson(TEXT("Skill_Test"), TEXT(",\"radius\":350")), MakeLua(TEXT("SkillLabSkill_Define(\"targetShape\",\"Ground\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast() end"))[TEXT("fixture.lua")], TEXT("GroundArea"));
	ExpectRejected(TEXT("Nonfinite JSON values are rejected"), MakeJson().Replace(TEXT("\"powerCoefficient\":0.5"), TEXT("\"powerCoefficient\":1e400")), UnitLua, TEXT("finite"));
	ExpectRejected(TEXT("Out-of-range JSON values are rejected"), MakeJson().Replace(TEXT("\"range\":900"), TEXT("\"range\":100001")), UnitLua, TEXT("finite number"));
	ExpectRejected(TEXT("Wrong JSON number types are rejected"), MakeJson().Replace(TEXT("\"range\":900"), TEXT("\"range\":\"900\"")), UnitLua, TEXT("finite number"));
	ExpectRejected(TEXT("Duplicate JSON fields are rejected"),
		TEXT(R"({"skills":[{"id":"Skill_Test","luaScript":"fixture.lua","cost":10,"cooldown":1,"magnitude":20,"powerCoefficient":0.5,"range":900,"range":901,"windup":0.25,"recovery":0.2}]})"),
		UnitLua, TEXT("duplicate JSON field"));
	ExpectRejected(TEXT("Duplicate skill IDs are rejected"), TEXT(R"({"skills":[{"id":"Skill_Test","luaScript":"fixture.lua","cost":10,"cooldown":1,"magnitude":20,"powerCoefficient":0.5,"range":900,"windup":0.25,"recovery":0.2},{"id":"Skill_Test","luaScript":"fixture.lua","cost":10,"cooldown":1,"magnitude":20,"powerCoefficient":0.5,"range":900,"windup":0.25,"recovery":0.2}]})"), UnitLua, TEXT("duplicate skill id"));

	TMap<FName, FSkillLabSkillDefinition> Previous;
	Error.Reset();
	TestTrue(TEXT("Previous candidate catalog fixture parses"), ParseFixture(MakeJson(), MakeLua(), Previous, Error));
	const FString PreviousId = TEXT("Skill_Test");
	TMap<FName, FSkillLabSkillDefinition> Replacement = Previous;
	Error.Reset();
	TestFalse(TEXT("Invalid candidate does not parse"), ParseFixture(MakeJson(TEXT("Skill_Bad")).Replace(TEXT("\"range\":900"), TEXT("\"range\":-1")), MakeLua(), Replacement, Error));
	TestTrue(TEXT("Caller retains its prior complete catalog after candidate rejection"), Replacement.Contains(FName(*PreviousId)));
	TestFalse(TEXT("Rejected candidate was not partially exposed"), Replacement.Contains(TEXT("Skill_Bad")));
	return true;
}

#endif
