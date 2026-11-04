#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillRuntimeComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastGameplayEventTest, "SkillLab.Skills.CastGameplayEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabCastGameplayEventTest::RunTest(const FString&)
{
	TMap<FName, FSkillLabSkillDefinition> PreviousDefinitions;
	const bool bPreviousLoadAttempted = FSkillLabSkillDefinitionProvider::CopyDefinitionsForAutomation(PreviousDefinitions);
	const FString FixtureJson = TEXT(R"({"skills":[{"id":"Skill_Mend","luaScript":"Skill_Mend.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3}]})");
	const FString FixtureLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
	TMap<FName, FSkillLabSkillDefinition> FixtureDefinitions;
	FString FixtureError;
	TMap<FString, FString> LuaSources;
	LuaSources.Add(TEXT("Skill_Mend.lua"), FixtureLua);
	if (!TestTrue(TEXT("Gameplay Event test definition fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		FixtureJson, LuaSources, FixtureDefinitions, FixtureError)))
	{
		AddError(FixtureError);
		return false;
	}
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(FixtureDefinitions);
	ON_SCOPE_EXIT
	{
		FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(PreviousDefinitions, bPreviousLoadAttempted);
	};

	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Editor || Context.WorldType == EWorldType::Game
				|| Context.WorldType == EWorldType::PIE))
			{
				World = Context.World();
				break;
			}
		}
	}
	if (!TestNotNull(TEXT("Automation world exists"), World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("SkillLabEventTestOwner");
	AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	USkillLabAbilitySystemComponent* ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
	Owner->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	USkillLabSkillRuntimeComponent* Runtime = NewObject<USkillLabSkillRuntimeComponent>(Owner);
	Owner->AddInstanceComponent(Runtime);
	Runtime->RegisterComponent();
	Runtime->InitializeSkills();
	FSkillLabSkillDefinition Definition;
	if (!TestTrue(TEXT("Self-heal fixture resolves"), FSkillLabSkillDefinitionProvider::Resolve(TEXT("Skill_Mend"), Definition)))
	{
		Owner->Destroy();
		return false;
	}
	int32 ReleaseCount = 0;
	int32 EffectCount = 0;
	Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{
		if (Result.Event == ESkillLabSkillEvent::PhaseChanged && Result.Stage == ESkillLabCastStage::Active) ++ReleaseCount;
		if (Result.Event == ESkillLabSkillEvent::EffectApplied) ++EffectCount;
	});
	auto Request = [&]()
	{
		FSkillLabSkillRequest Value;
		Value.Caster = Owner;
		Value.SkillId = Definition.SkillId;
		return Runtime->SubmitSkillRequest(Value);
	};
	const float InitialEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult First = Request();
	TestTrue(TEXT("GAS Ability activation accepts the cast"), First.bSuccess);
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
	Runtime->ResetSkills();
	const FSkillLabSkillResult Second = Request();
	TestTrue(TEXT("New cast activates after Reset"), Second.bSuccess);
	TestTrue(TEXT("Cast identity is monotonic across Reset"), Second.CastId > First.CastId);
	TestFalse(TEXT("Old Notify cannot advance new cast"), Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag));
	TestEqual(TEXT("New cast still waits"), Runtime->GetCastStage(), ESkillLabCastStage::Windup);
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	TestFalse(TEXT("Cancelled cast rejects Notify"), Runtime->ForwardValidatedNotify(Second.CastId, Definition.ReleaseEventTag));
	Runtime->ResetSkills();
	const FSkillLabSkillResult Third = Request();
	TestTrue(TEXT("Cast can restart after cancellation"), Third.bSuccess);
	Runtime->ResetSkills();
	TestFalse(TEXT("Reset cast rejects Notify"), Runtime->ForwardValidatedNotify(Third.CastId, Definition.ReleaseEventTag));
	TestEqual(TEXT("No stale effect after terminal casts"), EffectCount, 1);
	TestEqual(TEXT("Reset restores energy"), Runtime->GetEnergyValue(), InitialEnergy);
	Owner->Destroy();
	return true;
}
#endif
