#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "Misc/AutomationTest.h"
#include "SkillLabSubmissionTestWorld.h"
#include "Components/SphereComponent.h"
#include "GameFramework/WorldSettings.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "TimerManager.h"

// 只提供正式请求、生产调用与观察；丢失的是同步返回交付，不注入动作成功 bool。
class FSkillLabProductionFixture
{
public:
	struct FUnit
	{
		AActor* Actor = nullptr;
		USkillLabAbilitySystemComponent* ASC = nullptr;
		USkillLabSkillRuntimeComponent* Runtime = nullptr;
		USkillLabLuaSkillBehavior* Behavior = nullptr;
		USkillLabUnconfirmedSubmissions* Session = nullptr;
		FSkillLabProductionHandle Production;
		int32 Cast = 0;
		TStrongObjectPtr<USkillLabCastExecution> Execution;
	};
	explicit FSkillLabProductionFixture(FAutomationTestBase* InTest) : Test(InTest)
	{
		World = CreateSkillLabSubmissionTestWorld(true);
		// 无 GameMode 的独立世界需要正式分发 BeginPlay，FinishSpawning 才会运行 Actor BeginPlay。
		if (World) World->GetWorldSettings()->NotifyBeginPlay();
		TMap<FString, FString> Sources;
		Sources.Add(TEXT("Hold.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) end"));
		Sources.Add(TEXT("Area.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Ground\",\"relation\",\"Any\",\"effect\",\"Damage\",\"execution\",\"GroundArea\")\nfunction OnSkillCast(context) end"));
		Sources.Add(TEXT("Projectile.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Any\",\"effect\",\"Damage\",\"execution\",\"Projectile\")\nfunction OnSkillCast(context) end"));
		FString Error;
		Sources.Add(TEXT("Any.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Any\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) end"));
		bReady = World && FSkillLabSkillDefinitionProvider::ParseCatalogText(TEXT(R"({"skills":[
			{"id":"Receipt_Instant","luaScript":"Hold.lua","cost":2,"cooldown":20,"magnitude":1,"range":5000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},
			{"id":"Receipt_Other","luaScript":"Hold.lua","cost":3,"cooldown":0,"magnitude":1,"range":5000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},
			{"id":"Receipt_Any","luaScript":"Any.lua","cost":2,"cooldown":20,"magnitude":1,"range":5000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},
			{"id":"Receipt_Area","luaScript":"Area.lua","cost":2,"cooldown":20,"magnitude":1,"range":5000,"radius":100,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},
			{"id":"Receipt_ShortArea","luaScript":"Area.lua","cost":2,"cooldown":0.1,"magnitude":1,"range":5000,"radius":100,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},
			{"id":"Receipt_Projectile","luaScript":"Projectile.lua","cost":2,"cooldown":20,"magnitude":1,"range":5000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete","projectileSpeed":1200,"projectileCollisionRadius":18,"projectileLifetime":5,"projectileVisualScale":0.5,"projectileInterceptOtherUnits":false}
		]})"), Sources, Definitions, Error);
		Test->TestTrue(TEXT("Production fixture catalog and World available"), bReady);
		if (!bReady) Test->AddError(Error);
		else FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(Definitions);
	}
	~FSkillLabProductionFixture()
	{
		for (const TWeakObjectPtr<AActor>& Actor : Actors) if (Actor.IsValid()) Actor->Destroy();
		if (World) { World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); }
	}
	FUnit MakeUnit(const FVector& Location = FVector::ZeroVector)
	{
		FUnit Unit;
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
		Unit.Session = Unit.Actor->FindComponentByClass<USkillLabUnconfirmedSubmissions>();
		if (Unit.Session && !Unit.Session->HasBegunPlay()) Unit.Session->BeginPlay();
	}
	FSkillLabSkillResult Start(FUnit& Unit, const FName Skill = TEXT("Receipt_Instant"), AActor* Target = nullptr,
		const FVector& Center = FVector::ZeroVector, const int32 RequestId = 0)
	{
		FSkillLabSkillRequest Request;
		Request.Caster = Unit.Actor;
		Request.SkillId = Skill;
		Request.RequestId = RequestId;
		Request.Target = Target;
		Request.bHasLocation = true;
		Request.TargetLocation = Center;
		const auto Result = Unit.Runtime->SubmitSkillRequest(Request);
		if (Result.bSuccess)
		{
			Unit.Cast = Result.CastId;
			Unit.Execution.Reset(Unit.Runtime->FindMutableExecution(Result.CastId));
			Unit.Behavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Unit.Runtime));
		}
		return Result;
	}
	bool Begin(FUnit& Unit, const uint64 Point = 1)
	{
		FName Reason;
		const bool bBegan = Unit.Runtime->BeginBehaviorSubmission(Unit.Behavior, Unit.Cast, Point, Reason);
		Unit.Production = Unit.Runtime->GetProductionHandle(Unit.Behavior, Unit.Cast);
		return Test->TestTrue(TEXT("Production begins with real reservation and operation key"), bBegan && Unit.Production.Key.IsSet());
	}
	bool Execute(FUnit& Unit)
	{
		// 仅丢弃同步回执交付，动作仍调用生产重载；适配只在测试编译边界内存在。
		auto* Behavior = Unit.Behavior;
		Behavior->EnsurePoints();
		const auto Production = Unit.Runtime->GetProductionHandle(Behavior, Unit.Cast);
		auto* Point = Behavior->Points.Find(static_cast<int32>(Production.Key.Point));
		if (!Point) return false;
		Point->Production = Production;
		Point->Result.SubmissionKey = Production.Key;
		Behavior->LastPointId = Point->Declaration.Id;
		const bool bAccepted = Behavior->ExecuteDeclaredAction(*Point);
		Behavior->LastResponse = Point->Result;
		return bAccepted;
	}
	FSkillLabSkillResult Apply(FUnit& Unit, AActor* Target)
	{
		return Unit.Runtime->ApplyBehaviorEffect(Unit.Behavior, Unit.Cast, Target, Unit.Runtime->GetSingleExecution()->ActiveDefinition);
	}
	void LoseReturn(FUnit& Unit)
	{
		Test->AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		Unit.Runtime->AbortBehaviorSubmission(Unit.Behavior, Unit.Cast, Unit.Production.Key.Point);
	}
	void Retain(const FUnit& Unit, const FSkillLabProductionHandle& Operation)
	{
		const auto Key = Operation.Key;
		FSkillLabUnconfirmedSubmission Record;
		Record.SkillId = Key.SkillId;
		Record.RuntimeIdentity = Key.Runtime;
		Record.RuntimeGeneration = Key.Generation;
		Record.CastId = Key.Cast;
		Record.PointId = Key.Point;
		Record.TransactionId = Key.Transaction;
		Record.ProductionKey = Key;
		Record.ResultSourceId = TEXT("SkillLabProduction");
		Record.bAlreadyCommitted = Unit.Session->GetProductionResults().Query(Key).bAlreadyCommitted;
		Unit.Session->Record(Record);
	}
	int32 ActiveCast(const FUnit& Unit) const { return Unit.Runtime->GetActiveCastId(); }
	bool IsWaiting(const FUnit& Unit) const { return World->GetTimerManager().IsTimerActive(Unit.Execution->IndeterminateTimer); }
	FAutomationTestBase* Test;
	UWorld* World = nullptr;
	bool bReady = false;
private:
	FSkillLabScopedTestCatalog Catalog;
	TArray<TWeakObjectPtr<AActor>> Actors;
	TMap<FName, FSkillLabSkillDefinition> Definitions;
};
#endif
