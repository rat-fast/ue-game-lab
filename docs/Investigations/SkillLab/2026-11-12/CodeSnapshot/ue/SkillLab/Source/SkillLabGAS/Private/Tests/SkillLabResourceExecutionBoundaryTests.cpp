#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabResourceExecutionFixture.h"

class FSkillLabPeriodicExecutionCheck final : public IAutomationLatentCommand
{
public:
	FSkillLabPeriodicExecutionCheck(FAutomationTestBase* InTest, UWorld* InWorld, USkillLabAbilitySystemComponent* InASC, FActiveGameplayEffectHandle InHandle, uint64 InId, int32 InBefore)
		: Test(InTest), World(InWorld), ASC(InASC), Handle(InHandle), Id(InId), Before(InBefore) {}
	virtual bool Update() override
	{
		World->Tick(LEVELTICK_All, 0.1f);
		if (++Frames < 10) return false;
		Test->TestTrue(TEXT("Periodic execution ticked repeatedly"), USkillLabResourceExecutionFixture::ExecutionCount > Before + 1);
		Test->TestEqual(TEXT("Periodic Energy remains protected"), ASC->GetActualEnergy(), 100.0f);
		Test->TestEqual(TEXT("Periodic reservation preserved"), ASC->GetReservedEnergy(), 80.0f);
		Test->TestTrue(TEXT("Periodic non-resource output applied"), ASC->GetCombatAttributeSet()->GetHealth() < 100.0f);
		Test->TestFalse(TEXT("Periodic GE expires naturally"), ASC->GetActiveGameplayEffect(Handle) != nullptr);
		Test->TestTrue(TEXT("Release periodic reservation"), ASC->ResolveEnergyEscrow(Id, false));
		World->DestroyWorld(false);
		if (GEngine) GEngine->DestroyWorldContext(World);
		return true;
	}
private:
	FAutomationTestBase* Test; UWorld* World; USkillLabAbilitySystemComponent* ASC;
	FActiveGameplayEffectHandle Handle; uint64 Id; int32 Before; int32 Frames = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabResourceExecutionBoundaryTest, "SkillLab.GAS.ResourceExecutionBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabResourceExecutionBoundaryTest::RunTest(const FString&)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Execution world"), World)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	for (int32 Mode = 1; Mode <= 6; ++Mode)
	{
		auto* Owner = World->SpawnActor<AActor>();
		auto* ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->InitializeSkillLabAbilityActorInfo(Owner, Owner);
		if (!ASC->HasBegunPlay()) ASC->BeginPlay();
		const uint64 Id = ASC->AllocateEnergyTransactionId();
		if (Mode != 2) TestTrue(TEXT("Reserve for execution"), ASC->BeginEnergyEscrow(Id, 80.0f));
		auto* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = Mode == 6 ? EGameplayEffectDurationType::HasDuration : EGameplayEffectDurationType::Instant;
		if (Mode == 6) { Effect->DurationMagnitude = FScalableFloat(0.5f); Effect->Period = FScalableFloat(0.1f); }
		FGameplayEffectExecutionDefinition Execution;
		Execution.CalculationClass = USkillLabResourceExecutionFixture::StaticClass();
		Effect->Executions.Add(Execution);
		FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), static_cast<float>(Mode));
		const int32 Before = USkillLabResourceExecutionFixture::ExecutionCount;
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectSpecToSelf(Spec);
		if (Mode != 6) TestTrue(TEXT("Real calculation executed"), USkillLabResourceExecutionFixture::ExecutionCount > Before);
		TestEqual(TEXT("Execution cannot modify Energy base"), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()), 100.0f);
		TestEqual(TEXT("Execution cannot modify Energy current"), ASC->GetActualEnergy(), 100.0f);
		TestEqual(TEXT("Execution cannot modify MaxEnergy base"), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()), 100.0f);
		TestEqual(TEXT("Reservation preserved"), ASC->GetReservedEnergy(), Mode == 2 ? 0.0f : 80.0f);
		TestEqual(TEXT("Available preserved"), ASC->GetAvailableEnergy(), Mode == 2 ? 100.0f : 20.0f);
		if (Mode == 1 || Mode == 3 || Mode == 4) TestTrue(TEXT("Non-resource output applied"), ASC->GetCombatAttributeSet()->GetHealth() < 100.0f);
		if (Mode == 6)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FSkillLabPeriodicExecutionCheck(this, World, ASC, Handle, Id, Before));
			return true;
		}
		if (Mode != 2) TestTrue(TEXT("Release execution reservation"), ASC->ResolveEnergyEscrow(Id, false));
		Owner->Destroy();
	}
	return true;
}
#endif
