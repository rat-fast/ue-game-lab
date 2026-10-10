#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SkillLabSkillDefinitionProvider.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabDefinitionProviderValidationTest,
	"SkillLab.Skills.DefinitionValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabDefinitionProviderValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Error;
	const FString Valid = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20}]})");
	TestTrue(TEXT("Registered skill with valid numeric content is accepted"),
		FSkillLabSkillDefinitionProvider::ValidateJsonText(Valid, Error));
	TestTrue(TEXT("Valid catalog has no error"), Error.IsEmpty());

	const FString UnknownField = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"unexpected":1}]})");
	Error.Reset();
	TestFalse(TEXT("Unknown content fields are rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(UnknownField, Error));
	TestFalse(TEXT("Unknown field reports a reason"), Error.IsEmpty());

	const FString Duplicate = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20},{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20}]})");
	Error.Reset();
	TestFalse(TEXT("Duplicate skill IDs are rejected transactionally"), FSkillLabSkillDefinitionProvider::ValidateJsonText(Duplicate, Error));

	const FString Negative = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":-1,"cooldown":1,"magnitude":20}]})");
	Error.Reset();
	TestFalse(TEXT("Negative resource costs are rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(Negative, Error));
	return true;
}

#endif
