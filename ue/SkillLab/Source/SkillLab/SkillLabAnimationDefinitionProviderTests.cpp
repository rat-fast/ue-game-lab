// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SkillLabAnimationDefinitionProvider.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabAnimationDefinitionProviderTest,
	"SkillLab.Animation.DefinitionProvider",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabAnimationDefinitionProviderTest::RunTest(const FString& Parameters)
{
	const FString ValidJson = TEXT(R"({"schemaVersion":1,"locomotion":{"movementThreshold":0.01,"directionClampDegrees":45,"directionMin":-180,"directionMax":180}})");
	const FString InvalidBlendSpace = TEXT(R"({"schemaVersion":1,"locomotion":{"movementThreshold":0.01,"directionClampDegrees":45,"directionMin":-180,"directionMax":180,"blendSpace":"/Game/Missing.Missing"}})");
	FString Error;
	TestTrue(TEXT("Current schema is accepted"), FSkillLabAnimationDefinitionProvider::ValidateJsonText(ValidJson, Error));
	TestFalse(TEXT("Unsupported resource field is rejected instead of silently ignored"), FSkillLabAnimationDefinitionProvider::ValidateJsonText(InvalidBlendSpace, Error));

	const FString ConfigPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("AnimationDefinitions.json"));
	FString OriginalContents;
	if (!FFileHelper::LoadFileToString(OriginalContents, *ConfigPath))
	{
		AddError(FString::Printf(TEXT("Could not read %s for retention test"), *ConfigPath));
		return false;
	}

	TestTrue(TEXT("Valid project definition loads"), FSkillLabAnimationDefinitionProvider::Reload(&Error));
	const float PreviousThreshold = FSkillLabAnimationDefinitionProvider::Get().MovementThreshold;
	const FString InvalidContents = TEXT(R"({"schemaVersion":1,"locomotion":{"movementThreshold":-1,"directionClampDegrees":45,"directionMin":-180,"directionMax":180}})");
	if (!FFileHelper::SaveStringToFile(InvalidContents, *ConfigPath))
	{
		AddError(TEXT("Could not write temporary invalid config for retention test"));
		return false;
	}

	const bool bRejected = !FSkillLabAnimationDefinitionProvider::Reload(&Error);
	const bool bRetained = FMath::IsNearlyEqual(FSkillLabAnimationDefinitionProvider::Get().MovementThreshold, PreviousThreshold);
	const bool bRestored = FFileHelper::SaveStringToFile(OriginalContents, *ConfigPath);
	const bool bReloaded = bRestored && FSkillLabAnimationDefinitionProvider::Reload(&Error);
	TestTrue(TEXT("Invalid config is rejected"), bRejected);
	TestTrue(TEXT("Rejected config retains the previous valid definition"), bRetained);
	TestTrue(TEXT("Original config is restored and reloads"), bReloaded);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
