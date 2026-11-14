#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillGameplayAbility.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabAbilityOwnershipTest, "SkillLab.Skills.AbilityOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabAbilityOwnershipTest::RunTest(const FString&)
{
	FSkillLabCombatTestFixture Fixture(this);
	const FString Json = TEXT(R"({"skills":[{"id":"Hold","luaScript":"hold.lua","cost":0,"cooldown":0,"magnitude":1,"range":500,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})");
	const FString Lua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) end");
	if (!Fixture.LoadCatalog(Json, {{TEXT("hold.lua"), Lua}})) return false;

	auto Unit = Fixture.MakeUnit();
	const FGameplayAbilitySpecHandle ExternalHandle = Unit.ASC->GiveAbility(FGameplayAbilitySpec(USkillLabSkillGameplayAbility::StaticClass(), 1));
	TestTrue(TEXT("A separate system grant is valid"), ExternalHandle.IsValid());
	TestEqual(TEXT("Initial Runtime and separate grant are both present"), Unit.ASC->GetActivatableAbilities().Num(), 2);
	FGameplayAbilitySpecHandle InitialRuntimeHandle;
	for (const FGameplayAbilitySpec& Spec : Unit.ASC->GetActivatableAbilities())
	{
		if (Spec.Handle != ExternalHandle) InitialRuntimeHandle = Spec.Handle;
	}
	TestTrue(TEXT("The Runtime grant has its own exact handle"), InitialRuntimeHandle.IsValid());

	FGameplayAbilitySpecHandle RuntimeHandle = InitialRuntimeHandle;
	for (int32 ReplacementIndex = 0; ReplacementIndex < 2; ++ReplacementIndex)
	{
		const FSkillLabSkillResult CastResult = Fixture.Submit(Unit, TEXT("Hold"));
		if (!TestTrue(TEXT("Formal request starts the Cast before replacement"), CastResult.bSuccess)) return false;
		const USkillLabCastExecution* Execution = Unit.Runtime->FindExecution(CastResult.CastToken);
		if (!TestNotNull(TEXT("Cast remains waiting for its completion event"), Execution)) return false;
		TStrongObjectPtr<USkillLabSkillGameplayAbility> OldAbility(Execution->ActiveAbility.Get());
		if (!TestNotNull(TEXT("Active Cast owns a live Ability instance"), OldAbility.Get())) return false;
		TestTrue(TEXT("Ability is active in both Runtime and GAS before replacement"), OldAbility->IsSkillAbilityActive() && OldAbility->IsActive());
		TestTrue(TEXT("Active Ability uses the exact Runtime grant, not the same-class external grant"), OldAbility->GetCurrentAbilitySpecHandle() == RuntimeHandle);
		const FGameplayAbilitySpec* OldSpec = Unit.ASC->FindAbilitySpecFromHandle(RuntimeHandle);
		if (!TestNotNull(TEXT("Runtime's exact spec exists before active replacement"), OldSpec)) return false;
		TestTrue(TEXT("GAS spec is active before replacement"), OldSpec->IsActive());
		TStrongObjectPtr<USkillLabSkillRuntimeComponent> OldRuntime(Unit.Runtime);

		Fixture.RebuildRuntime(Unit);
		TestFalse(TEXT("Strongly held old Ability has ended its skill lifecycle"), OldAbility->IsSkillAbilityActive());
		TestFalse(TEXT("Strongly held old Ability has ended its GAS lifecycle"), OldAbility->IsActive());
		TestTrue(TEXT("Old Runtime has no remaining Casts"), OldRuntime->GetExecutionTokens().IsEmpty());
		TestNull(TEXT("Active replacement removes the prior Runtime's exact handle"), Unit.ASC->FindAbilitySpecFromHandle(RuntimeHandle));
		TestTrue(TEXT("Active replacement preserves the same-class external exact handle"), Unit.ASC->FindAbilitySpecFromHandle(ExternalHandle) != nullptr);
		TestEqual(TEXT("Replacement has only one Runtime grant plus the external grant"), Unit.ASC->GetActivatableAbilities().Num(), 2);

		FGameplayAbilitySpecHandle ReplacementHandle;
		for (const FGameplayAbilitySpec& Spec : Unit.ASC->GetActivatableAbilities())
		{
			if (Spec.Handle != ExternalHandle) ReplacementHandle = Spec.Handle;
		}
		TestTrue(TEXT("Replacement Runtime uses a new exact handle"), ReplacementHandle.IsValid()
			&& ReplacementHandle != RuntimeHandle && ReplacementHandle != InitialRuntimeHandle && ReplacementHandle != ExternalHandle);
		OldRuntime->DestroyComponent();
		TestNotNull(TEXT("Repeated old cleanup preserves the new Runtime spec"), Unit.ASC->FindAbilitySpecFromHandle(ReplacementHandle));
		TestNotNull(TEXT("Repeated old cleanup preserves the external spec"), Unit.ASC->FindAbilitySpecFromHandle(ExternalHandle));
		TestEqual(TEXT("Repeated old cleanup does not change the spec count"), Unit.ASC->GetActivatableAbilities().Num(), 2);
		RuntimeHandle = ReplacementHandle;
	}

	Unit.Runtime->DestroyComponent();
	TestEqual(TEXT("Repeated Runtime replacement leaves only the separate system grant"), Unit.ASC->GetActivatableAbilities().Num(), 1);
	TestTrue(TEXT("Repeated cleanup preserves the same separate handle"), Unit.ASC->FindAbilitySpecFromHandle(ExternalHandle) != nullptr);
	return true;
}
#endif
