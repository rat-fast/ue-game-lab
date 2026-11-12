#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SkillLabSubmissionTestWorld.h"
#include "Components/SphereComponent.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillGameplayAbility.h"
#include "UObject/StrongObjectPtr.h"

// 目录是进程全局状态；拥有该对象的测试必须串行，latent 命令持有 fixture 到结束。
class FSkillLabScopedTestCatalog final
{
public:
	FSkillLabScopedTestCatalog() : bPreviousLoad(FSkillLabSkillDefinitionProvider::CopyDefinitionsForAutomation(Previous)) {}
	~FSkillLabScopedTestCatalog() { FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(Previous, bPreviousLoad); }
	FSkillLabScopedTestCatalog(const FSkillLabScopedTestCatalog&) = delete;
	FSkillLabScopedTestCatalog& operator=(const FSkillLabScopedTestCatalog&) = delete;
private:
	TMap<FName, FSkillLabSkillDefinition> Previous;
	bool bPreviousLoad;
};

// 仅组装真实 World/ASC/Runtime；不拥有另一份生命周期、提交或资源账本。
class FSkillLabCombatTestFixture
{
public:
	struct FUnit
	{
		AActor* Actor = nullptr;
		USkillLabAbilitySystemComponent* ASC = nullptr;
		USkillLabSkillRuntimeComponent* Runtime = nullptr;
	};
	explicit FSkillLabCombatTestFixture(FAutomationTestBase* InTest) : Test(InTest)
	{
		World = CreateSkillLabSubmissionTestWorld(true);
		bReady = Test->TestNotNull(TEXT("Independent combat World exists"), World);
	}
	virtual ~FSkillLabCombatTestFixture()
	{
		for (const auto& Actor : Actors) if (Actor.IsValid()) Actor->Destroy();
		if (World) { World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); }
		// Catalog 成员最后析构，确保 EndPlay/Timer 清理完成后才恢复全局目录。
	}
	FSkillLabCombatTestFixture(const FSkillLabCombatTestFixture&) = delete;
	FSkillLabCombatTestFixture& operator=(const FSkillLabCombatTestFixture&) = delete;
	bool LoadCatalog(const FString& Json, const TMap<FString, FString>& Sources)
	{
		TMap<FName, FSkillLabSkillDefinition> Definitions;
		FString Error;
		const bool bParsed = FSkillLabSkillDefinitionProvider::ParseCatalogText(Json, Sources, Definitions, Error);
		Test->TestTrue(TEXT("Combat catalog parses through production provider"), bParsed);
		if (!bParsed) { Test->AddError(Error); return false; }
		FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(Definitions);
		return true;
	}
	FUnit MakeUnit(const FVector& Location = FVector::ZeroVector)
	{
		FUnit Unit;
		if (!World) return Unit;
		Unit.Actor = World->SpawnActor<AActor>();
		Actors.Add(Unit.Actor);
		auto* Sphere = NewObject<USphereComponent>(Unit.Actor);
		Sphere->InitSphereRadius(24.0f);
		Sphere->SetCollisionObjectType(ECC_Pawn);
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Sphere->SetCollisionResponseToAllChannels(ECR_Overlap);
		Sphere->SetGenerateOverlapEvents(true);
		Unit.Actor->SetRootComponent(Sphere);
		Unit.Actor->AddInstanceComponent(Sphere);
		Sphere->RegisterComponent();
		Unit.Actor->SetActorLocation(Location);
		Unit.ASC = NewObject<USkillLabAbilitySystemComponent>(Unit.Actor);
		Unit.Actor->AddInstanceComponent(Unit.ASC);
		Unit.ASC->RegisterComponent();
		if (!Unit.ASC->HasBegunPlay()) Unit.ASC->BeginPlay();
		RebuildRuntime(Unit);
		return Unit;
	}
	void RebuildRuntime(FUnit& Unit)
	{
		if (Unit.Runtime) Unit.Runtime->DestroyComponent();
		Unit.Runtime = NewObject<USkillLabSkillRuntimeComponent>(Unit.Actor);
		Unit.Actor->AddInstanceComponent(Unit.Runtime);
		Unit.Runtime->RegisterComponent();
		if (!Unit.Runtime->HasBegunPlay()) Unit.Runtime->BeginPlay();
		Unit.Runtime->InitializeSkills();
	}
	FSkillLabSkillResult Submit(const FUnit& Unit, FName Skill, AActor* Target = nullptr,
		const FVector& Center = FVector::ZeroVector, int32 RequestId = 0)
	{
		FSkillLabSkillRequest Request;
		Request.Caster = Unit.Actor; Request.SkillId = Skill; Request.Target = Target;
		Request.TargetLocation = Center; Request.bHasLocation = true; Request.RequestId = RequestId;
		return Unit.Runtime->SubmitSkillRequest(Request);
	}
	bool Notify(const FUnit& Unit, int32 CastId, FGameplayTag Event) { return Unit.Runtime->ForwardValidatedNotify(CastId, Event); }
	void Tick(float Seconds, float Step = 1.0f / 60.0f)
	{
		while (Seconds > KINDA_SMALL_NUMBER)
		{
			const float Delta = FMath::Min(Step, Seconds);
			World->Tick(LEVELTICK_All, Delta);
			// 同一 automation Update 内的显式 World Tick 也必须推进 TimerManager 帧号。
			++GFrameCounter;
			Seconds -= Delta;
		}
	}
	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	bool bReady = false;
private:
	FSkillLabScopedTestCatalog Catalog;
	TArray<TWeakObjectPtr<AActor>> Actors;
};

inline USkillLabSkillBehavior* SkillLabObservedBehavior(const USkillLabSkillRuntimeComponent* Runtime)
{ const auto* E = Runtime->GetSingleExecution(); return E ? E->ActiveBehavior.Get() : nullptr; }
inline TWeakObjectPtr<USkillLabSkillGameplayAbility> SkillLabObservedAbility(const USkillLabSkillRuntimeComponent* Runtime)
{ const auto* E = Runtime->GetSingleExecution(); return E ? E->ActiveAbility : TWeakObjectPtr<USkillLabSkillGameplayAbility>(); }
inline FTimerHandle SkillLabObservedTimer(const USkillLabSkillRuntimeComponent* Runtime, const bool bIndeterminate)
{ const auto* E = Runtime->GetSingleExecution(); return E ? (bIndeterminate ? E->IndeterminateTimer : E->PhaseTimer) : FTimerHandle(); }

inline TWeakObjectPtr<USkillLabSkillGameplayAbility> SkillLabObservedAbility(TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime)
{ return SkillLabObservedAbility(Runtime.Get()); }
#endif
