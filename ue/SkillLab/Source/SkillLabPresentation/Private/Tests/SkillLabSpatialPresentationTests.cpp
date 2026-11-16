#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "SkillLabSkillPresentationRouter.h"
#include "SkillLabSkillFeedbackActor.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSpatialPresentationTest, "SkillLab.Presentation.SpatialLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabSpatialPresentationTest::RunTest(const FString&)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("test world"), World)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
	ON_SCOPE_EXIT { World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); };
	AActor* Source = World->SpawnActor<AActor>(); AActor* Target = World->SpawnActor<AActor>();
	Source->SetRootComponent(NewObject<USceneComponent>(Source)); Source->GetRootComponent()->RegisterComponent();
	Target->SetRootComponent(NewObject<USceneComponent>(Target)); Target->GetRootComponent()->RegisterComponent();
	Target->SetActorLocation(FVector(100,0,0));
	AActor* Related = World->SpawnActor<AActor>();
	Related->SetRootComponent(NewObject<USceneComponent>(Related)); Related->GetRootComponent()->RegisterComponent();
	Related->SetActorLocation(FVector(0,300,0));
	auto* Runtime = NewObject<USkillLabSkillRuntimeComponent>(Source); Source->AddInstanceComponent(Runtime); Runtime->RegisterComponent();
	TStrongObjectPtr<USkillLabSkillPresentationRouter> Router(NewObject<USkillLabSkillPresentationRouter>()); Router->Bind(Runtime);
	FSkillLabSpatialDefinition D; D.Name = TEXT("aura"); D.Event = TEXT("BuffApplied"); D.Source = ESkillLabSpatialSource::Target;
	D.OffsetSpace = ESkillLabSpatialOffsetSpace::World; D.Offset = FVector(0,0,20); D.bFollow = true;
	FSkillLabSkillResult Result; Result.SkillId = TEXT("Fixture"); Result.bSuccess = true; Result.SourceActor = Source;
	Result.TargetActor = Target; Result.Event = ESkillLabSkillEvent::BuffApplied; Result.SpatialDefinitions.Add(D);
	Result.RelatedActor = Related;
	FSkillLabSkillResult RelatedResult = Result;
	RelatedResult.Event = ESkillLabSkillEvent::TargetSelected;
	RelatedResult.SpatialDefinitions[0].Event = TEXT("TargetSelected");
	RelatedResult.SpatialDefinitions[0].Source = ESkillLabSpatialSource::Related;
	RelatedResult.SpatialDefinitions[0].bFollow = false;
	Router->ShowSpatial(RelatedResult);
	bool bFoundRelatedMarker = false;
	for (TActorIterator<ASkillLabSkillFeedbackActor> It(World); It; ++It)
		bFoundRelatedMarker |= It->GetActorLocation().Equals(FVector(0,300,20), 0.1f);
	TestTrue(TEXT("presentation uses related actor distinct from target"), bFoundRelatedMarker);
	Result.EffectInstanceId = TEXT("effectA"); Router->ShowSpatial(Result);
	Result.EffectInstanceId = TEXT("effectB"); Router->ShowSpatial(Result);
	TestEqual(TEXT("two effect identities have separate visuals"), Router->PersistentMarkers.Num(), 2);
	const auto A = Router->PersistentMarkers.FindRef(TEXT("effectA:aura"));
	const auto B = Router->PersistentMarkers.FindRef(TEXT("effectB:aura"));
	TestTrue(TEXT("both markers created"), A.IsValid() && B.IsValid());
	Target->SetActorLocation(FVector(200,0,0)); World->Tick(LEVELTICK_All, 0.05f);
	if (A.IsValid()) TestTrue(TEXT("marker follows target with offset"), A->GetActorLocation().Equals(FVector(200,0,20), 0.1f));
	Result.Event = ESkillLabSkillEvent::BuffRemoved; Result.EffectInstanceId = TEXT("effectA"); Router->HandleEvent(Result);
	TestFalse(TEXT("removed effect visual destroyed"), A.IsValid() && !A->IsActorBeingDestroyed());
	TestTrue(TEXT("other effect remains"), B.IsValid() && !B->IsActorBeingDestroyed());
	Router->Bind(nullptr);
	TestTrue(TEXT("unbind clears tracked visuals"), Router->PersistentMarkers.IsEmpty());
	TestFalse(TEXT("unbind destroys remaining visual"), B.IsValid() && !B->IsActorBeingDestroyed());
	return true;
}
#endif
