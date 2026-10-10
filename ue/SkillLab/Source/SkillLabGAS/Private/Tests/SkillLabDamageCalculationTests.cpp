#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "SkillLabDamageCalculation.h"

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

#endif
