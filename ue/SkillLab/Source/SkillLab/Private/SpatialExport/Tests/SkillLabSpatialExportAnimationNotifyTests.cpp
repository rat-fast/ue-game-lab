#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SkillLabCastExecution.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSpatialExportAnimationNotifyGenerationTest,
	"SkillLab.SpatialExport.AnimationNotifyGeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabSpatialExportAnimationNotifyGenerationTest::RunTest(const FString&)
{
	const FString OutputDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
		TEXT("SpatialExportNotifyGeneration"), FGuid::NewGuid().ToString(EGuidFormats::Digits));
	const FString EventsPath = FPaths::Combine(OutputDirectory, TEXT("events.jsonl"));
	TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
	if (!TestNotNull(TEXT("Test world exists"), World.Get())) return false;

	TStrongObjectPtr<USkillLabSpatialExportSubsystem> Subsystem(NewObject<USkillLabSpatialExportSubsystem>(World.Get()));
	TStrongObjectPtr<USkillLabSkillRuntimeComponent> Runtime(NewObject<USkillLabSkillRuntimeComponent>(World.Get()));
	Subsystem->Runtime = Runtime.Get();
	Subsystem->OutputDirectory = OutputDirectory;
	Subsystem->bRequestAccepted = true;
	Subsystem->CapturedRuntimeGeneration = 23;
	Subsystem->CapturedCastId = 7;

	FSkillLabCastExecutionToken OldGenerationToken;
	OldGenerationToken.Runtime = Runtime.Get();
	OldGenerationToken.RuntimeGeneration = 22;
	OldGenerationToken.CastId = 7;
	Subsystem->RecordAnimationNotify(OldGenerationToken, nullptr, FGameplayTag(), false, true);
	TestFalse(TEXT("Notify from an earlier Runtime generation is ignored"), IFileManager::Get().FileExists(*EventsPath));

	FSkillLabCastExecutionToken CurrentGenerationToken = OldGenerationToken;
	CurrentGenerationToken.RuntimeGeneration = 23;
	Subsystem->RecordAnimationNotify(CurrentGenerationToken, nullptr, FGameplayTag(), false, true);

	FString Events;
	TestTrue(TEXT("Matching-generation Notify is written"), FFileHelper::LoadFileToString(Events, *EventsPath));
	TArray<FString> Lines;
	Events.ParseIntoArrayLines(Lines);
	TestEqual(TEXT("Only the matching Notify is recorded"), Lines.Num(), 1);
	TestTrue(TEXT("Recorded Notify keeps the captured generation"), Events.Contains(TEXT("\"runtime_generation\":23")));
	TestTrue(TEXT("Recorded Notify keeps the captured CastId"), Events.Contains(TEXT("\"cast_id\":7")));

	IFileManager::Get().DeleteDirectory(*OutputDirectory, false, true);
	World->DestroyWorld(false);
	return true;
}
#endif
