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

	const FString AnimationBinding = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"castAnimation":"/Game/SkillLab/Animations/AS_FireballCast.AS_FireballCast"}]})");
	Error.Reset();
	TestTrue(TEXT("Sequence presentation binding is accepted by the shared definition contract"),
		FSkillLabSkillDefinitionProvider::ValidateJsonText(AnimationBinding, Error));

	const FString ConflictingBindings = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"castMontage":"/Game/SkillLab/Animations/SomeMontage.SomeMontage","castAnimation":"/Game/SkillLab/Animations/AS_FireballCast.AS_FireballCast"}]})");
	Error.Reset();
	TestFalse(TEXT("Montage and sequence bindings remain mutually exclusive"),
		FSkillLabSkillDefinitionProvider::ValidateJsonText(ConflictingBindings, Error));

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

	const FString EventRelease = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":1.5}]})");
	Error.Reset();
	const bool bEventReleaseValid = FSkillLabSkillDefinitionProvider::ValidateJsonText(EventRelease, Error);
	TestTrue(FString::Printf(TEXT("GameplayEvent release fields are parsed (Error: %s)"), *Error), bEventReleaseValid);

	const FString LegacyTimer = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"windup":0.25}]})");
	Error.Reset();
	TestTrue(TEXT("Legacy Timer definitions remain valid without release fields"),
		FSkillLabSkillDefinitionProvider::ValidateJsonText(LegacyTimer, Error));

	FSkillLabSkillDefinition ResolvedLegacyTimer;
	const bool bDefinitionsReloaded = FSkillLabSkillDefinitionProvider::ReloadDefinitions();
	const bool bResolvedLegacyTimer = bDefinitionsReloaded
		&& FSkillLabSkillDefinitionProvider::Resolve(TEXT("Skill_AreaBurst"), ResolvedLegacyTimer);
	TestTrue(TEXT("Existing Timer definition resolves from the catalog"), bResolvedLegacyTimer);
	if (bResolvedLegacyTimer)
	{
		TestTrue(TEXT("Existing Timer definition keeps Timer release"), ResolvedLegacyTimer.ReleaseTrigger == ESkillLabReleaseTrigger::Timer);
		TestTrue(TEXT("Existing Timer definition keeps the 3 second default timeout"), FMath::IsNearlyEqual(ResolvedLegacyTimer.ReleaseEventTimeout, 3.0f));
		TestFalse(TEXT("Existing Timer definition has no release event tag"), ResolvedLegacyTimer.ReleaseEventTag.IsValid());
	}

	const FString MissingEventTag = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTimeout":1.5}]})");
	Error.Reset();
	TestFalse(TEXT("GameplayEvent release requires an event tag"),
		FSkillLabSkillDefinitionProvider::ValidateJsonText(MissingEventTag, Error));

	const FString TimerWithEventTag = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"Timer","releaseEventTag":"SkillLab.Event.Cast"}]})");
	Error.Reset();
	TestFalse(TEXT("Timer release rejects an unused event tag"),
		FSkillLabSkillDefinitionProvider::ValidateJsonText(TimerWithEventTag, Error));

	const FString UnknownReleaseTrigger = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"Notify"}]})");
	Error.Reset();
	TestFalse(TEXT("Unknown releaseTrigger is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(UnknownReleaseTrigger, Error));

	const FString UnknownReleaseTag = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Unknown","releaseEventTimeout":1.0}]})");
	Error.Reset();
	TestFalse(TEXT("Unknown releaseEventTag is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(UnknownReleaseTag, Error));

	const FString ZeroEventTimeout = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":0}]})");
	Error.Reset();
	TestFalse(TEXT("Zero GameplayEvent timeout is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(ZeroEventTimeout, Error));

	const FString NegativeEventTimeout = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":-1}]})");
	Error.Reset();
	TestFalse(TEXT("Negative GameplayEvent timeout is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(NegativeEventTimeout, Error));

	const FString NonFiniteEventTimeout = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":1e400}]})");
	Error.Reset();
	TestFalse(TEXT("Nonfinite GameplayEvent timeout is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(NonFiniteEventTimeout, Error));

	const FString WrongTimeoutType = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":"1.0"}]})");
	Error.Reset();
	TestFalse(TEXT("String releaseEventTimeout is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(WrongTimeoutType, Error));

	const FString WhitespaceTrigger = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":" GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":1.0}]})");
	Error.Reset();
	TestFalse(TEXT("Whitespace around releaseTrigger is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(WhitespaceTrigger, Error));

	const FString WhitespaceTag = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"GameplayEvent","releaseEventTag":" SkillLab.Event.Cast","releaseEventTimeout":1.0}]})");
	Error.Reset();
	TestFalse(TEXT("Whitespace around releaseEventTag is rejected"), FSkillLabSkillDefinitionProvider::ValidateJsonText(WhitespaceTag, Error));

	const FString TimerZeroTimeout = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"Timer","releaseEventTimeout":0}]})");
	Error.Reset();
	TestTrue(TEXT("Timer ignores a zero releaseEventTimeout"), FSkillLabSkillDefinitionProvider::ValidateJsonText(TimerZeroTimeout, Error));

	const FString TimerNegativeTimeout = TEXT(R"({"skills":[{"id":"Skill_Fireball","cost":10,"cooldown":1,"magnitude":20,"releaseTrigger":"Timer","releaseEventTimeout":-1}]})");
	Error.Reset();
	TestTrue(TEXT("Timer ignores a negative releaseEventTimeout"), FSkillLabSkillDefinitionProvider::ValidateJsonText(TimerNegativeTimeout, Error));
	return true;
}

#endif
