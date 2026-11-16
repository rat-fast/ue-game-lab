#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"
#include "GameplayEffect.h"
#include "UObject/UObjectIterator.h"
#include "Components/StaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSubmittedActionLifetimeTest, "SkillLab.Skills.SubmittedActionLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabSubmittedActionLifetimeTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[
 {"id":"Shot","luaScript":"shot.lua","cost":7,"cooldown":0,"magnitude":3,"range":5000,"active":4,"projectileSpeed":500,"projectileCollisionRadius":18,"projectileLifetime":2,"projectileVisualScale":0.62,"projectileInterceptOtherUnits":true,"completionMode":"AllSubmissionPointsResolved","submissionPoints":[{"id":1,"trigger":"Release","target":"RequestTarget"},{"id":2,"trigger":"Timer","delay":0.05,"target":"RequestTarget"}]},
 {"id":"ShortShot","luaScript":"shot.lua","cost":7,"cooldown":0,"magnitude":3,"range":5000,"active":4,"projectileSpeed":1,"projectileCollisionRadius":18,"projectileLifetime":0.1,"projectileVisualScale":0.62,"projectileInterceptOtherUnits":true},
 {"id":"SpatialShot","luaScript":"spatial.lua","cost":7,"cooldown":0,"magnitude":3,"range":5000,"active":4,"projectileSpeed":500,"projectileCollisionRadius":18,"projectileLifetime":2,"projectileVisualScale":0.62,"projectileInterceptOtherUnits":true,"spatial":{"muzzle":{"offset":[0,50,0],"visualScale":2}}},
 {"id":"RelatedShot","luaScript":"related.lua","cost":7,"cooldown":0,"magnitude":3,"range":5000,"active":4,"projectileSpeed":500,"projectileCollisionRadius":18,"projectileLifetime":2,"projectileVisualScale":0.62,"projectileInterceptOtherUnits":true,"spatial":{"muzzle":{"offset":[0,25,0]}}},
 {"id":"Burn","luaScript":"burn.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"active":3,"duration":0.5,"period":0.1,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})");
 const FString Rules = TEXT(",\"combatRules\",\"Open\",\"recoveryRules\",\"Open\"");
 const FString Shot = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"Projectile\"") + Rules + TEXT(")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 const FString Burn = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"DamageOverTime\",\"execution\",\"InstantTarget\"") + Rules + TEXT(")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 const FString SpatialShot = Shot + TEXT("\nSkillLabSkill_Spatial(\"muzzle\",\"Projectile\",\"Release\",\"Source\",\"World\",\"ExecutionDirection\",\"Once\",\"KeepRotation\")");
 const FString RelatedShot = Shot + TEXT("\nSkillLabSkill_Spatial(\"muzzle\",\"Projectile\",\"Release\",\"Related\",\"World\",\"ExecutionDirection\",\"Once\",\"KeepRotation\")");
 if (!F.LoadCatalog(Json, {{TEXT("shot.lua"),Shot},{TEXT("spatial.lua"),SpatialShot},{TEXT("related.lua"),RelatedShot},{TEXT("burn.lua"),Burn}})) return false;
 auto Source = F.MakeUnit(), Target = F.MakeUnit(FVector(600,0,0)); Target.Runtime->TeamId = 2;
 auto BeginShot = [&](FName Id)
 {
  const auto R = F.Submit(Source, Id, Target.Actor);
  const auto* E = Source.Runtime->FindExecution(R.CastToken);
  auto* B = E ? Cast<USkillLabLuaSkillBehavior>(E->ActiveBehavior) : nullptr;
  return TPair<FSkillLabSkillResult,USkillLabLuaSkillBehavior*>(R,B);
 };
 auto Spatial = BeginShot(TEXT("SpatialShot"));
 if (!TestNotNull(TEXT("Spatial fixture behavior"), Spatial.Value)) return false;
 auto* SpatialProjectile = Spatial.Value->GetPointProjectile(1);
 if (!TestNotNull(TEXT("Spatial fixture delivers projectile"), SpatialProjectile)) return false;
 TestTrue(TEXT("Real spawn consumes configured world offset"), SpatialProjectile->GetActorLocation().Equals(Source.Actor->GetActorLocation() + FVector(0,50,0), 0.1f));
 const auto* Sphere = Cast<USphereComponent>(SpatialProjectile->GetRootComponent());
 TestTrue(TEXT("Visual scaling leaves collision radius unchanged"), Sphere && FMath::IsNearlyEqual(Sphere->GetUnscaledSphereRadius(), 18.f));
 const auto* Mesh = SpatialProjectile->FindComponentByClass<UStaticMeshComponent>();
 TestTrue(TEXT("Visual mesh receives scale"), Mesh && FMath::IsNearlyEqual(Mesh->GetRelativeScale3D().X, 1.24f));
 SpatialProjectile->Destroy(); Source.Runtime->ResetSkills();
 auto Related = F.MakeUnit(FVector(0,200,0));
 Related.Actor->SetActorEnableCollision(false);
 FSkillLabSkillRequest RelatedRequest;
 RelatedRequest.Caster = Source.Actor; RelatedRequest.Target = Target.Actor; RelatedRequest.RelatedActor = Related.Actor;
 RelatedRequest.SkillId = TEXT("RelatedShot"); RelatedRequest.TargetLocation = Target.Actor->GetActorLocation(); RelatedRequest.bHasLocation = true;
 TArray<FSkillLabSkillResult> RelatedEvents;
 const auto RelatedHandle = Source.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& Event) { RelatedEvents.Add(Event); });
 const auto RelatedCast = Source.Runtime->SubmitSkillRequest(RelatedRequest);
 const auto* RelatedExecution = Source.Runtime->FindExecution(RelatedCast.CastToken);
 auto* RelatedBehavior = RelatedExecution ? Cast<USkillLabLuaSkillBehavior>(RelatedExecution->ActiveBehavior) : nullptr;
 auto* RelatedProjectile = RelatedBehavior ? RelatedBehavior->GetPointProjectile(1) : nullptr;
 if (!TestNotNull(TEXT("Related request delivers a projectile"), RelatedProjectile)) return false;
 TestTrue(TEXT("actual spawn uses distinct related actor"), RelatedProjectile->GetActorLocation().Equals(FVector(0,225,0), 0.1f));
 TestTrue(TEXT("formal target event preserves distinct related actor"), RelatedEvents.ContainsByPredicate([&](const auto& Event)
  { return Event.Event == ESkillLabSkillEvent::TargetSelected && Event.TargetActor == Target.Actor && Event.RelatedActor.Get() == Related.Actor; }));
 RelatedProjectile->Destroy(); Source.Runtime->OnNativeSkillEvent.Remove(RelatedHandle); Source.Runtime->ResetSkills();

 FSkillLabSpatialDefinition Reference; Reference.OffsetSpace = ESkillLabSpatialOffsetSpace::World;
 FTransform ReferenceResult; FString ReferenceError; const FVector Ground(10,20,30);
 for (const auto& Case : {TPair<ESkillLabSpatialSource,FVector>(ESkillLabSpatialSource::Source, Source.Actor->GetActorLocation()),
  TPair<ESkillLabSpatialSource,FVector>(ESkillLabSpatialSource::Target, Target.Actor->GetActorLocation()),
  TPair<ESkillLabSpatialSource,FVector>(ESkillLabSpatialSource::Ground, Ground),
  TPair<ESkillLabSpatialSource,FVector>(ESkillLabSpatialSource::Related, Related.Actor->GetActorLocation())})
 {
  Reference.Source = Case.Key;
  TestTrue(TEXT("reference resolves"), FSkillLabSpatialTransform::Resolve(Reference, Source.Actor, Target.Actor, Related.Actor, Ground, FVector::ForwardVector, ReferenceResult, ReferenceError));
  TestTrue(TEXT("reference selects expected actor or ground"), ReferenceResult.GetLocation().Equals(Case.Value, 0.1f));
 }
 Reference.Source = ESkillLabSpatialSource::Related;
 TestFalse(TEXT("missing related actor never falls back to target"), FSkillLabSpatialTransform::Resolve(Reference, Source.Actor, Target.Actor, nullptr, Ground, FVector::ForwardVector, ReferenceResult, ReferenceError));
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
	TArray<FSkillLabSkillResult> RemovedResults;
 Source.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& R)
	{
		if (R.Event == ESkillLabSkillEvent::DamageTick) { ++Ticks; Observed.Add(R); }
		if (R.Event == ESkillLabSkillEvent::BuffRemoved) { ++Removed; RemovedResults.Add(R); }
	});
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

 // Active target specs own their transient definitions; distinct applications retain distinct stacking identity.
 auto EffectSourceA = F.MakeUnit(FVector(0,1200,0));
 auto EffectSourceB = F.MakeUnit(FVector(0,1500,0));
 auto EffectTarget = F.MakeUnit(FVector(600,1200,0));
 TArray<TPair<FActiveGameplayEffectHandle,FSkillLabDamageResult>> PeriodicResults;
 EffectTarget.ASC->OnPeriodicDamageResolved.AddLambda([&](const FActiveGameplayEffectHandle Handle, const FSkillLabDamageResult& Result)
 {
  PeriodicResults.Emplace(Handle,Result);
 });
 FSkillLabDamageRequest RequestA;
 RequestA.SourceASC = EffectSourceA.ASC;
 RequestA.TargetASC = EffectTarget.ASC;
 RequestA.BaseDamage = 11.0f;
 FSkillLabDamageRequest RequestB = RequestA;
 RequestB.SourceASC = EffectSourceB.ASC;
 RequestB.BaseDamage = 23.0f;
 const FSkillLabDamageResult AppliedA = EffectSourceA.ASC->ApplyDamageOverTimeToTarget(RequestA,0.45f,0.1f);
 const FSkillLabDamageResult AppliedB = EffectSourceB.ASC->ApplyDamageOverTimeToTarget(RequestB,0.65f,0.2f);
 TestTrue(TEXT("First independent periodic effect is accepted"),AppliedA.bApplied && AppliedA.ActiveEffectHandle.IsValid());
 TestTrue(TEXT("Second independent periodic effect is accepted"),AppliedB.bApplied && AppliedB.ActiveEffectHandle.IsValid());
 TestTrue(TEXT("Different applications retain different active handles"),AppliedA.ActiveEffectHandle != AppliedB.ActiveEffectHandle);
 const FActiveGameplayEffect* ActiveA = EffectTarget.ASC->GetActiveGameplayEffect(AppliedA.ActiveEffectHandle);
 const FActiveGameplayEffect* ActiveB = EffectTarget.ASC->GetActiveGameplayEffect(AppliedB.ActiveEffectHandle);
 TestNotNull(TEXT("First active effect retains its spec"),ActiveA);
 TestNotNull(TEXT("Second active effect retains its spec"),ActiveB);
 if (!ActiveA || !ActiveB) return false;
 TWeakObjectPtr<const UGameplayEffect> WeakDefinitionA(ActiveA->Spec.Def);
 TWeakObjectPtr<const UGameplayEffect> WeakDefinitionB(ActiveB->Spec.Def);
 TestTrue(TEXT("Stacking identity remains definition-specific"),ActiveA->Spec.Def != ActiveB->Spec.Def
  && ActiveA->Spec.GetContext().GetInstigatorAbilitySystemComponent() == EffectSourceA.ASC
  && ActiveB->Spec.GetContext().GetInstigatorAbilitySystemComponent() == EffectSourceB.ASC);
 CollectGarbage(RF_NoFlags,true);
 TestTrue(TEXT("Active target spec keeps the first transient definition alive"),WeakDefinitionA.IsValid());
 TestTrue(TEXT("Active target spec keeps the second transient definition alive"),WeakDefinitionB.IsValid());
 F.Tick(0.25f);
 int32 TicksA = 0, TicksB = 0;
 for (const auto& Entry : PeriodicResults)
 {
  if (Entry.Key == AppliedA.ActiveEffectHandle)
  {
   ++TicksA;
   TestEqual(TEXT("First spec reports its own periodic damage"),Entry.Value.RawDamage,11.0f);
  }
  else if (Entry.Key == AppliedB.ActiveEffectHandle)
  {
   ++TicksB;
   TestEqual(TEXT("Second spec reports its own periodic damage"),Entry.Value.RawDamage,23.0f);
  }
 }
 TestTrue(TEXT("First periodic handle produced observed damage results"),TicksA >= 2);
 TestTrue(TEXT("Second periodic handle produced observed damage results"),TicksB >= 2);
 EffectTarget.ASC->RemoveActiveGameplayEffect(AppliedA.ActiveEffectHandle);
 EffectTarget.ASC->RemoveActiveGameplayEffect(AppliedB.ActiveEffectHandle);
 CollectGarbage(RF_NoFlags,true);
 TestFalse(TEXT("Removed first effect definition is collectible"),WeakDefinitionA.IsValid());
 TestFalse(TEXT("Removed second effect definition is collectible"),WeakDefinitionB.IsValid());

 // The application query runs before GAS copies the spec into its active container.
 TWeakObjectPtr<const UGameplayEffect> WeakAllowedDefinition;
 bool bAllowedQueryRan = false;
 bool bAllowedDefinitionSurvivedQueryGC = false;
 FGameplayEffectApplicationQuery AllowAfterGC;
 AllowAfterGC.BindLambda([&](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
 {
  bAllowedQueryRan = true;
  WeakAllowedDefinition = Spec.Def;
  CollectGarbage(RF_NoFlags,true);
  bAllowedDefinitionSurvivedQueryGC = WeakAllowedDefinition.IsValid();
  return true;
 });
 const int32 AllowQueryIndex = EffectTarget.ASC->GameplayEffectApplicationQueries.Add(AllowAfterGC);
 const FSkillLabDamageResult AllowedAfterGC = EffectSourceA.ASC->ApplyDamageOverTimeToTarget(RequestA,0.45f,0.1f);
 EffectTarget.ASC->GameplayEffectApplicationQueries.RemoveAt(AllowQueryIndex);
 TestTrue(TEXT("Allowing application query ran its in-callback GC"),bAllowedQueryRan);
 TestTrue(TEXT("Scope strong reference keeps definition alive during allowing query GC"),bAllowedDefinitionSurvivedQueryGC);
 TestTrue(TEXT("Effect is accepted after application query GC"),AllowedAfterGC.bApplied && AllowedAfterGC.ActiveEffectHandle.IsValid());
 TestTrue(TEXT("Allowed effect remains active after query returns"),
  EffectTarget.ASC->GetActiveGameplayEffect(AllowedAfterGC.ActiveEffectHandle) != nullptr && WeakAllowedDefinition.IsValid());
 EffectTarget.ASC->RemoveActiveGameplayEffect(AllowedAfterGC.ActiveEffectHandle);
 CollectGarbage(RF_NoFlags,true);
 TestFalse(TEXT("Allowed effect definition is collectible after active removal"),WeakAllowedDefinition.IsValid());

 auto CountTransientDefinitions = [](USkillLabAbilitySystemComponent* Outer)
 {
  int32 Count = 0;
  for (TObjectIterator<UGameplayEffect> It; It; ++It)
   if (It->GetOuter() == Outer && !It->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)) ++Count;
  return Count;
 };
 TWeakObjectPtr<const UGameplayEffect> WeakRejectedDefinition;
 bool bRejectedQueryRan = false;
 bool bRejectedDefinitionSurvivedQueryGC = false;
 FGameplayEffectApplicationQuery RejectEffect;
 RejectEffect.BindLambda([&](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec& Spec)
 {
  bRejectedQueryRan = true;
  WeakRejectedDefinition = Spec.Def;
  CollectGarbage(RF_NoFlags,true);
  bRejectedDefinitionSurvivedQueryGC = WeakRejectedDefinition.IsValid();
  return false;
 });
 const int32 RejectQueryIndex = EffectTarget.ASC->GameplayEffectApplicationQueries.Add(RejectEffect);
 const int32 DefinitionsBeforeRejectedApply = CountTransientDefinitions(EffectSourceA.ASC);
 const FSkillLabDamageResult Rejected = EffectSourceA.ASC->ApplyDamageOverTimeToTarget(RequestA,0.45f,0.1f);
 TestFalse(TEXT("Application query rejection returns no accepted effect"),Rejected.bApplied);
 EffectTarget.ASC->GameplayEffectApplicationQueries.RemoveAt(RejectQueryIndex);
 TestTrue(TEXT("Rejecting application query ran its in-callback GC"),bRejectedQueryRan);
 TestTrue(TEXT("Scope strong reference keeps definition alive during rejecting query GC"),bRejectedDefinitionSurvivedQueryGC);
 CollectGarbage(RF_NoFlags,true);
 TestFalse(TEXT("Rejected effect definition is collectible after application returns"),WeakRejectedDefinition.IsValid());
 TestEqual(TEXT("Rejected application leaves no source-owned transient definition"),
  CountTransientDefinitions(EffectSourceA.ASC),DefinitionsBeforeRejectedApply);

 auto DestroyedDoTSource = F.MakeUnit(FVector(0,1800,0));
 auto DestroyedDoTTarget = F.MakeUnit(FVector(600,1800,0));
 FSkillLabDamageRequest DestroyedSourceRequest;
 DestroyedSourceRequest.SourceASC = DestroyedDoTSource.ASC;
 DestroyedSourceRequest.TargetASC = DestroyedDoTTarget.ASC;
 DestroyedSourceRequest.BaseDamage = 7.0f;
 const FSkillLabDamageResult BeforeSourceDestruction = DestroyedDoTSource.ASC->ApplyDamageOverTimeToTarget(DestroyedSourceRequest,0.5f,0.1f);
 TestTrue(TEXT("DoT is active before source destruction"),BeforeSourceDestruction.bApplied);
 const float HealthAfterInitialTick = DestroyedDoTTarget.ASC->GetCombatAttributeSet()->GetHealth();
 DestroyedDoTSource.Actor->Destroy();
 F.Tick(0.15f);
 TestTrue(TEXT("Target-owned active DoT continues after source Actor and ASC destruction"),
  DestroyedDoTTarget.ASC->GetCombatAttributeSet()->GetHealth() < HealthAfterInitialTick);

 auto ResetBurn = F.Submit(Source,TEXT("Burn"),Target.Actor);
 Source.Runtime->ResetSkills();
	TestTrue(TEXT("Reset emits a removal event with live source and target actors"), RemovedResults.Num() >= 3
		&& RemovedResults.Last().SourceActor == Source.Actor && RemovedResults.Last().TargetActor == Target.Actor);
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
