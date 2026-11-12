#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabProductionFixture.h"
#include "SkillLabFireballProjectile.h"
#include "EngineUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabProductionRecordsTest, "SkillLab.Skills.ProductionRecords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabProductionRecordsTest::RunTest(const FString&)
{
	FSkillLabProductionFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Instant = Fixture.MakeUnit();
	Instant.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 50.0f);
	TestTrue(TEXT("Formal instant request activates"), Fixture.Start(Instant).bSuccess);
	if (!Fixture.Begin(Instant)) return false;
	const auto Key = Instant.Production.Key;
	bool bUnknownInsideGE = false, bRecordedBeforeHit = false;
	const auto HealthHandle = Instant.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData&)
		{ bUnknownInsideGE = Instant.Session->GetProductionResults().Query(Key).State == ESkillLabProductionState::InProgress; });
	const auto EventHandle = Instant.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{
		if (Result.Event == ESkillLabSkillEvent::Hit)
			bRecordedBeforeHit = Instant.Session->GetProductionResults().Query(Key).State == ESkillLabProductionState::ConfirmedAccepted;
	});
	TestTrue(TEXT("Real Executor/Resolver accepts GE"), Fixture.Apply(Instant, Instant.Actor).bEffectApplied);
	Instant.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
	Instant.Runtime->OnNativeSkillEvent.Remove(EventHandle);
	TestTrue(TEXT("Attribute side effect does not predeclare GE return"), bUnknownInsideGE);
	TestTrue(TEXT("GE return is recorded before outer Hit"), bRecordedBeforeHit);
	const auto Known = Instant.Session->GetProductionResults().Query(Key);
	TestFalse(TEXT("GE return time is not invented as its earlier acceptance time"), Known.AcceptedAt.IsSet());
	Fixture.LoseReturn(Instant);
	Fixture.RebuildRuntime(Instant);
	TestEqual(TEXT("Real accepted record survives Runtime cleanup/rebuild"), Instant.Session->GetProductionResults().Query(Key).State,
		ESkillLabProductionState::ConfirmedAccepted);
	auto Wrong = Key;
	++Wrong.Generation;
	TestEqual(TEXT("Wrong generation cannot read original result"), Instant.Session->GetProductionResults().Query(Wrong).State,
		ESkillLabProductionState::Unknown);
	Wrong = Key; ++Wrong.Operation;
	TestEqual(TEXT("Missing operation is unknown, not rejection"), Instant.Session->GetProductionResults().Query(Wrong).State,
		ESkillLabProductionState::Unknown);
	AddExpectedError(TEXT("Conflicting or unavailable result ignored"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Contradictory duplicate cannot overwrite real GE acceptance"), Instant.Session->ProductionResults.Confirm(Key, false, TEXT("InvalidDuplicate")));
	TestEqual(TEXT("Duplicate did not change version"), Instant.Session->GetProductionResults().Query(Key).Version, Known.Version);

	auto Rejected = Fixture.MakeUnit(FVector(200, 0, 0));
	TestTrue(TEXT("Rejection fixture activates"), Fixture.Start(Rejected).bSuccess);
	if (!Fixture.Begin(Rejected)) return false;
	TestFalse(TEXT("Real executor rejects missing effect target"), Fixture.Apply(Rejected, nullptr).bEffectApplied);
	TestEqual(TEXT("Rejected operation has an explicit production record"), Rejected.Session->GetProductionResults().Query(Rejected.Production.Key).State,
		ESkillLabProductionState::ConfirmedRejected);
	Fixture.LoseReturn(Rejected);

	auto Empty = Fixture.MakeUnit();
	TestTrue(TEXT("Empty area formally selects ground"), Fixture.Start(Empty, TEXT("Receipt_Area"), nullptr, FVector(2500, 0, 0)).bSuccess);
	if (!Fixture.Begin(Empty)) return false;
	TestTrue(TEXT("Real empty query accepts area action"), Fixture.Execute(Empty));
	const auto EmptyResult = Empty.Session->GetProductionResults().Query(Empty.Production.Key);
	TestTrue(TEXT("Empty success is based on completed real query"), EmptyResult.bAreaQueryComplete && EmptyResult.Children.IsEmpty());
	TestEqual(TEXT("Empty area is explicitly accepted"), EmptyResult.State, ESkillLabProductionState::ConfirmedAccepted);
	TestTrue(TEXT("Empty query has a real acceptance clock"), EmptyResult.AcceptedAt.IsSet());
	Fixture.LoseReturn(Empty);

	auto Area = Fixture.MakeUnit();
	auto TargetA = Fixture.MakeUnit(FVector(1000, 0, 0));
	auto TargetB = Fixture.MakeUnit(FVector(1020, 0, 0));
	TestTrue(TEXT("Partial area formally activates"), Fixture.Start(Area, TEXT("Receipt_Area"), nullptr, FVector(1000, 0, 0)).bSuccess);
	if (!Fixture.Begin(Area)) return false;
	bool bStopped = false;
	const auto StopAfterFirst = [&](const FOnAttributeChangeData&)
	{ if (!bStopped) { bStopped = true; Area.Runtime->EndPlay(EEndPlayReason::RemovedFromWorld); } };
	const auto AHandle = TargetA.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).AddLambda(StopAfterFirst);
	const auto BHandle = TargetB.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).AddLambda(StopAfterFirst);
	TestTrue(TEXT("Partial action accepts actual first GE despite Runtime EndPlay"), Fixture.Execute(Area));
	TargetA.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).Remove(AHandle);
	TargetB.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).Remove(BHandle);
	const auto Partial = Area.Session->GetProductionResults().Query(Area.Production.Key);
	int32 AcceptedChildren = 0, PendingChildren = 0;
	for (const auto& Child : Partial.Children)
	{
		const auto State = Area.Session->GetProductionResults().Query(Child).State;
		AcceptedChildren += State == ESkillLabProductionState::ConfirmedAccepted;
		PendingChildren += State == ESkillLabProductionState::InProgress;
	}
	TestEqual(TEXT("Partial aggregate persists independently of Behavior"), Partial.State, ESkillLabProductionState::ConfirmedAccepted);
	TestEqual(TEXT("Only first actual GE accepted"), AcceptedChildren, 1);
	TestEqual(TEXT("Interrupted planned child is not fabricated as failed"), PendingChildren, 1);
	Fixture.LoseReturn(Area);

	// 正式 GAS 应用查询拒绝 GE，生产返回必须沿 WasSuccessfullyApplied 记录。
	FGameplayEffectApplicationQuery RejectGE;
	RejectGE.BindLambda([](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&) { return false; });
	TargetA.ASC->GameplayEffectApplicationQueries.Add(RejectGE);
	TargetB.ASC->GameplayEffectApplicationQueries.Add(RejectGE);
	auto AllRejected = Fixture.MakeUnit();
	TestTrue(TEXT("All-rejected area formally activates"), Fixture.Start(AllRejected, TEXT("Receipt_Area"), nullptr, FVector(1000, 0, 0)).bSuccess);
	if (!Fixture.Begin(AllRejected)) return false;
	TestFalse(TEXT("Actual application queries reject all planned GEs"), Fixture.Execute(AllRejected));
	TestEqual(TEXT("All explicit child rejections permit aggregate rejection"), AllRejected.Session->GetProductionResults().Query(AllRejected.Production.Key).State,
		ESkillLabProductionState::ConfirmedRejected);
	Fixture.LoseReturn(AllRejected);
	TargetA.ASC->GameplayEffectApplicationQueries.Reset();
	TargetB.ASC->GameplayEffectApplicationQueries.Reset();

	auto Interrupted = Fixture.MakeUnit();
	TestTrue(TEXT("Interrupted unknown area formally activates"), Fixture.Start(Interrupted, TEXT("Receipt_Area"), nullptr, FVector(1000, 0, 0)).bSuccess);
	if (!Fixture.Begin(Interrupted)) return false;
	FGameplayEffectApplicationQuery InterruptGE;
	InterruptGE.BindLambda([&](const FActiveGameplayEffectsContainer&, const FGameplayEffectSpec&)
	{ if (Interrupted.Runtime->HasBegunPlay()) Interrupted.Runtime->EndPlay(EEndPlayReason::RemovedFromWorld); return false; });
	TargetA.ASC->GameplayEffectApplicationQueries.Add(InterruptGE);
	TargetB.ASC->GameplayEffectApplicationQueries.Add(InterruptGE);
	TestFalse(TEXT("First real GE rejection interrupts the producer"), Fixture.Execute(Interrupted));
	TestEqual(TEXT("Missing planned child is unknown rather than aggregate failure"), Interrupted.Session->GetProductionResults().Query(Interrupted.Production.Key).State,
		ESkillLabProductionState::InProgress);
	Fixture.LoseReturn(Interrupted);
	TargetA.ASC->GameplayEffectApplicationQueries.Reset();
	TargetB.ASC->GameplayEffectApplicationQueries.Reset();

	auto Lost = Fixture.MakeUnit(FVector(300, 0, 0));
	Lost.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 50.0f);
	TestTrue(TEXT("Lost producer session activates"), Fixture.Start(Lost).bSuccess);
	if (!Fixture.Begin(Lost)) return false;
	const auto LostHandle = Lost.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).AddLambda(
		[&](const FOnAttributeChangeData&) { if (Lost.Session->HasBegunPlay()) Lost.Session->EndPlay(EEndPlayReason::RemovedFromWorld); });
	TestTrue(TEXT("GE can return applied after owner was ended inside delegate"), Fixture.Apply(Lost, Lost.Actor).bEffectApplied);
	Lost.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).Remove(LostHandle);
	TestEqual(TEXT("No invented acceptance in side-effect-to-return loss window"), Lost.Session->GetProductionResults().Query(Lost.Production.Key).State,
		ESkillLabProductionState::Unavailable);
	Fixture.LoseReturn(Lost);

	// 与其他单位隔开，目标与发射点重叠；必须实际由引擎 collision 触发。
	auto Shooter = Fixture.MakeUnit(FVector(4000, 0, 0));
	auto OverlapTarget = Fixture.MakeUnit(FVector(4000, 0, 0));
	TestTrue(TEXT("Projectile formal request activates"), Fixture.Start(Shooter, TEXT("Receipt_Projectile"), OverlapTarget.Actor).bSuccess);
	if (!Fixture.Begin(Shooter)) return false;
	bool bDeliveryBeforeImpact = false;
	const auto HitHandle = Shooter.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{
		if (Result.Event == ESkillLabSkillEvent::EffectApplied)
			bDeliveryBeforeImpact = Shooter.Session->GetProductionResults().Query(Shooter.Production.Key).State == ESkillLabProductionState::ConfirmedAccepted;
	});
	TestTrue(TEXT("Real FinishSpawning and handshake deliver projectile"), Fixture.Execute(Shooter));
	Shooter.Runtime->OnNativeSkillEvent.Remove(HitHandle);
	TestTrue(TEXT("Actual synchronous overlap observes delivery before damage"), bDeliveryBeforeImpact);
	TestEqual(TEXT("Overlap applies actual GE once"), OverlapTarget.ASC->GetCombatAttributeSet()->GetHealth(), 99.0f);
	const auto Delivered = Shooter.Session->GetProductionResults().Query(Shooter.Production.Key);
	TestTrue(TEXT("Delivery has trustworthy time before activation"), Delivered.AcceptedAt.IsSet());
	TestEqual(TEXT("Synchronous impact/self-destruction does not change delivery"), Delivered.State, ESkillLabProductionState::ConfirmedAccepted);
	Fixture.LoseReturn(Shooter);

	auto SecondShooter = Fixture.MakeUnit(FVector(4500, 0, 0));
	auto FarTarget = Fixture.MakeUnit(FVector(4900, 0, 0));
	TestTrue(TEXT("Unhit projectile formally activates"), Fixture.Start(SecondShooter, TEXT("Receipt_Projectile"), FarTarget.Actor).bSuccess);
	if (!Fixture.Begin(SecondShooter)) return false;
	TestTrue(TEXT("Unhit projectile is really delivered"), Fixture.Execute(SecondShooter));
	const auto BeforeDestroy = SecondShooter.Session->GetProductionResults().Query(SecondShooter.Production.Key);
	int32 DestroyedProjectiles = 0;
	for (TActorIterator<ASkillLabFireballProjectile> It(Fixture.World); It; ++It)
		if (It->GetOwner() == SecondShooter.Actor && !It->IsActorBeingDestroyed()) { ++DestroyedProjectiles; It->Destroy(); }
	TestEqual(TEXT("External destruction exercised real post-delivery actor"), DestroyedProjectiles, 1);
	TestEqual(TEXT("Destroyed detection cannot overwrite delivery version"),
		SecondShooter.Session->GetProductionResults().Query(SecondShooter.Production.Key).Version, BeforeDestroy.Version);
	Fixture.LoseReturn(SecondShooter);
	return true;
}
#endif
