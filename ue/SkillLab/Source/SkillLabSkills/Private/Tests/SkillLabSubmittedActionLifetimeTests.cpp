#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSubmittedActionLifetimeTest, "SkillLab.Skills.SubmittedActionLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabSubmittedActionLifetimeTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[
 {"id":"Shot","luaScript":"shot.lua","cost":7,"cooldown":0,"magnitude":3,"range":5000,"active":4,"projectileSpeed":500,"projectileCollisionRadius":18,"projectileLifetime":2,"projectileVisualScale":0.62,"projectileInterceptOtherUnits":true,"completionMode":"AllSubmissionPointsResolved","submissionPoints":[{"id":1,"trigger":"Release","target":"RequestTarget"},{"id":2,"trigger":"Timer","delay":0.05,"target":"RequestTarget"}]},
 {"id":"ShortShot","luaScript":"shot.lua","cost":7,"cooldown":0,"magnitude":3,"range":5000,"active":4,"projectileSpeed":1,"projectileCollisionRadius":18,"projectileLifetime":0.1,"projectileVisualScale":0.62,"projectileInterceptOtherUnits":true},
 {"id":"Burn","luaScript":"burn.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"active":3,"duration":0.5,"period":0.1,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})");
 const FString Rules = TEXT(",\"combatRules\",\"Open\",\"recoveryRules\",\"Open\"");
 const FString Shot = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"Projectile\"") + Rules + TEXT(")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 const FString Burn = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"DamageOverTime\",\"execution\",\"InstantTarget\"") + Rules + TEXT(")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 if (!F.LoadCatalog(Json, {{TEXT("shot.lua"),Shot},{TEXT("burn.lua"),Burn}})) return false;
 auto Source = F.MakeUnit(), Target = F.MakeUnit(FVector(600,0,0)); Target.Runtime->TeamId = 2;
 auto BeginShot = [&](FName Id)
 {
  const auto R = F.Submit(Source, Id, Target.Actor);
  const auto* E = Source.Runtime->FindExecution(R.CastToken);
  auto* B = E ? Cast<USkillLabLuaSkillBehavior>(E->ActiveBehavior) : nullptr;
  return TPair<FSkillLabSkillResult,USkillLabLuaSkillBehavior*>(R,B);
 };
 auto First = BeginShot(TEXT("Shot"));
 if (!TestNotNull(TEXT("Formal projectile behavior exists"), First.Value)) return false;
 auto* Projectile = First.Value->GetPointProjectile(1);
 if (!TestNotNull(TEXT("Real projectile is delivered"), Projectile)) return false;
 TestTrue(TEXT("Collision is enabled only after delivery"), Projectile->IsDeliveryConfirmed());
 const auto Delivery = First.Value->GetPointProduction(1);
 const auto Receipt = Delivery.Owner->GetProductionResults().Query(Delivery.Key);
 F.Tick(0.1f);
 TestNotNull(TEXT("Second declared point owns a distinct real projectile"),First.Value->GetPointProjectile(2));
 TestTrue(TEXT("Second projectile has independent operation and no second fee"),First.Value->GetPointProduction(2).Key.IsSet() && First.Value->GetPointProduction(2).Key.Operation != Delivery.Key.Operation && First.Value->GetPointProduction(2).Key.Transaction == 0);
 Source.Runtime->CancelCast(First.Key.CastToken,TEXT("LifetimeTestCancel"));
 TestFalse(TEXT("Cancel preserves delivered projectile Actor"), Projectile->IsActorBeingDestroyed());
 const float Health = Target.ASC->GetCombatAttributeSet()->GetHealth();
 auto Second = BeginShot(TEXT("ShortShot"));
 F.Tick(1.4f);
 TestTrue(TEXT("Both cancelled Cast projectiles still apply real damage"), Target.ASC->GetCombatAttributeSet()->GetHealth() <= Health - 6.0f);
 TestEqual(TEXT("Delivery receipt version is not rewritten by hit"), Delivery.Owner->GetProductionResults().Query(Delivery.Key).Version, Receipt.Version);
 TestEqual(TEXT("Two deliveries charge once each, hit adds no cost"), Source.ASC->GetActualEnergy(), 86.0f);
 TestNull(TEXT("Expired action resolves its own Cast"), Source.Runtime->FindExecution(Second.Key.CastToken));

 // A component replacement must not replace the original submitted capability.
 Source.Runtime->ResetSkills();
 auto Old = BeginShot(TEXT("Shot"));
 if (!TestNotNull(TEXT("Replacement fixture projectile is alive"), Old.Value)) return false;
 const auto OldDelivery = Old.Value->GetPointProduction(1);
 const float BeforeReplacementHit = Target.ASC->GetCombatAttributeSet()->GetHealth();
 F.RebuildRuntime(Source);
 const float AfterRebuildEnergy = Source.ASC->GetActualEnergy();
 F.Tick(1.4f);
 TestTrue(TEXT("Original delivered action survives Runtime replacement"), Target.ASC->GetCombatAttributeSet()->GetHealth() < BeforeReplacementHit);
 TestEqual(TEXT("Replacement does not recharge old hit"), Source.ASC->GetActualEnergy(), AfterRebuildEnergy);
 TestEqual(TEXT("Original delivery stays accepted"), OldDelivery.Owner->GetProductionResults().Query(OldDelivery.Key).State, ESkillLabProductionState::ConfirmedAccepted);

 Source.Runtime->ResetSkills();
 Target.Actor->SetActorLocation(Source.Actor->GetActorLocation());
 const float BeforeOverlap = Target.ASC->GetCombatAttributeSet()->GetHealth();
 const auto Overlap = F.Submit(Source,TEXT("ShortShot"),Target.Actor);
 TestTrue(TEXT("Creation-time synchronous overlap is a successful delivery"),Overlap.bSuccess);
 TestTrue(TEXT("Opening collision applies immediate real damage"),Target.ASC->GetCombatAttributeSet()->GetHealth() < BeforeOverlap);
 TestEqual(TEXT("Creation-time self-destruction charges once"),Source.ASC->GetActualEnergy(),93.0f);
 TestTrue(TEXT("Synchronous resolved action leaves no stuck Cast"),Source.Runtime->GetExecutionTokens().IsEmpty());
 Target.Actor->SetActorLocation(FVector(600,0,0));
 Source.Runtime->ResetSkills();
 auto Destroyed = BeginShot(TEXT("ShortShot"));
 if (!TestNotNull(TEXT("External destruction fixture has behavior"),Destroyed.Value)) return false;
 TStrongObjectPtr<USkillLabLuaSkillBehavior> DestroyedBehavior(Destroyed.Value);
 const auto DestroyedDelivery = DestroyedBehavior->GetPointProduction(1);
 DestroyedBehavior->GetPointProjectile(1)->Destroy();
 TestEqual(TEXT("External destruction reports the actual action reason"),DestroyedBehavior->GetPointResult(1)->Reason,FName(TEXT("Destroyed")));
 TestEqual(TEXT("External destruction cannot retract delivery"),DestroyedDelivery.Owner->GetProductionResults().Query(DestroyedDelivery.Key).State,ESkillLabProductionState::ConfirmedAccepted);

 Source.Runtime->ResetSkills();
 auto* Obstacle = F.World->SpawnActor<AActor>();
 auto* Wall = NewObject<USphereComponent>(Obstacle); Obstacle->SetRootComponent(Wall); Obstacle->AddInstanceComponent(Wall);
 Wall->InitSphereRadius(40); Wall->SetCollisionObjectType(ECC_WorldStatic); Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics); Wall->SetCollisionResponseToAllChannels(ECR_Block); Wall->RegisterComponent();
 Obstacle->SetActorLocation(FVector(180,0,0));
 int32 Blocked = 0;
 const auto BlockHandle = Source.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& R) { Blocked += R.Event == ESkillLabSkillEvent::Hit && R.Reason == TEXT("Blocked"); });
 const float BeforeBlocked = Target.ASC->GetCombatAttributeSet()->GetHealth();
 F.Submit(Source,TEXT("Shot"),Target.Actor); F.Tick(0.6f);
 Source.Runtime->OnNativeSkillEvent.Remove(BlockHandle); Obstacle->Destroy();
 TestEqual(TEXT("Two delivered projectiles resolve against real world blocker"),Blocked,2);
 TestEqual(TEXT("Blocked delivery applies no damage"),Target.ASC->GetCombatAttributeSet()->GetHealth(),BeforeBlocked);
 TestTrue(TEXT("Both blocked action waits finish"),Source.Runtime->GetExecutionTokens().IsEmpty());

 auto Disposable = F.MakeUnit(FVector(0,800,0)), Survivor = F.MakeUnit(FVector(600,800,0)); Survivor.Runtime->TeamId = 2;
 F.Submit(Disposable,TEXT("Shot"),Survivor.Actor);
 const float BeforeSourceDestroyed = Survivor.ASC->GetCombatAttributeSet()->GetHealth();
 Disposable.Actor->Destroy(); F.Tick(1.4f);
 TestEqual(TEXT("Destroyed source cannot apply late projectile effect"),Survivor.ASC->GetCombatAttributeSet()->GetHealth(),BeforeSourceDestroyed);

 // Real periodic GE continues after cancellation, completes naturally, and Reset removes it.
 Source.Runtime->ResetSkills();
 int32 Ticks = 0, Removed = 0;
 TArray<FSkillLabSkillResult> Observed;
 Source.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& R)
 { if (R.Event == ESkillLabSkillEvent::DamageTick) { ++Ticks; Observed.Add(R); } Removed += R.Event == ESkillLabSkillEvent::BuffRemoved; });
 auto Burning = F.Submit(Source,TEXT("Burn"),Target.Actor);
 Source.Runtime->CancelCast(Burning.CastToken,TEXT("KeepIndependentGE"));
 F.Tick(0.25f);
 TestTrue(TEXT("Real periodic damage continues after Cast cancel"), Ticks > 0);
 for (const auto& R : Observed) TestTrue(TEXT("Periodic facts retain original complete token and point"), R.CastToken == Burning.CastToken && R.SubmissionPointId == 1 && R.SubmissionKey.IsSet());
 F.Tick(0.4f);
 TestEqual(TEXT("Independent GE naturally reports one removal"), Removed,1);
 const auto CompleteBurn = F.Submit(Source,TEXT("Burn"),Target.Actor);
 F.Notify(Source,CompleteBurn.CastId,SkillLabGASTags::Event_Cast_Complete);
 TestTrue(TEXT("Ordinary Cast completion leaves independent effect running"),Source.Runtime->GetExecutionTokens().IsEmpty());
 const int32 BeforeCompletedTicks = Ticks; F.Tick(0.25f);
 TestTrue(TEXT("Periodic GE ticks after normal completion"),Ticks > BeforeCompletedTicks);
 F.Tick(0.4f);
 TestEqual(TEXT("Normally completed Cast effect also removes naturally"),Removed,2);
 auto ResetBurn = F.Submit(Source,TEXT("Burn"),Target.Actor);
 Source.Runtime->ResetSkills();
 const int32 AtReset = Ticks; F.Tick(0.3f);
 TestEqual(TEXT("Explicit Reset removes tracked periodic effect"),Ticks,AtReset);
 auto EndBurn = F.Submit(Source,TEXT("Burn"),Target.Actor);
 const float BeforeEnd = Target.ASC->GetCombatAttributeSet()->GetHealth();
 const int32 BeforeRemoval = Removed;
 Source.Runtime->DestroyComponent(); Source.Runtime = nullptr;
 F.Tick(0.25f);
 TestTrue(TEXT("Runtime EndPlay detaches observation but keeps independent GE"),Target.ASC->GetCombatAttributeSet()->GetHealth() < BeforeEnd);
 TestEqual(TEXT("Detached observer does not fabricate BuffRemoved"),Removed,BeforeRemoval);
 return true;
}
#endif
