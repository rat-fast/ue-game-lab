#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabProductionFixture.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabResetResourceCompletionTest, "SkillLab.Skills.ResetResourceCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabResetResourceCompletionTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"skills":[{"id":"ResetProbe","luaScript":"probe.lua","cost":1,"cooldown":20,"magnitude":1,"range":500,"active":3},{"id":"WaitingProbe","luaScript":"probe.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":2}]})");
 const FString Lua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 if (!F.bReady || !F.LoadCatalog(Json, {{TEXT("probe.lua"), Lua}})) return false;

 auto Immediate = F.MakeUnit();
 TestTrue(TEXT("Immediate setup changes energy"), Immediate.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f));
 Immediate.Runtime->ResetSkills();
 TestEqual(TEXT("Immediate Reset restores energy"), Immediate.ASC->GetActualEnergy(), Immediate.Runtime->GetMaxEnergyValue());
 TestTrue(TEXT("Immediate Reset releases request barrier"), F.Submit(Immediate, TEXT("ResetProbe")).bSuccess);

 auto Health = F.MakeUnit(FVector(500, 0, 0));
 Health.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 40.0f);
 int32 HealthCallbacks = 0; FSkillLabSkillResult HealthRequest;
 const auto HealthHandle = Health.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   ++HealthCallbacks;
   Health.Runtime->ResetSkills();
   HealthRequest = F.Submit(Health, TEXT("ResetProbe"));
  });
 Health.Runtime->ResetSkills();
 Health.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
 TestEqual(TEXT("Health restore occurs once despite reentrant Reset"), HealthCallbacks, 1);
 TestEqual(TEXT("Health callback observes Reset barrier"), HealthRequest.Reason, FName(TEXT("Resetting")));

 auto Rejected = F.MakeUnit(FVector(600, 0, 0));
 TestTrue(TEXT("Real skill establishes confirmed cooldown"), F.Submit(Rejected, TEXT("ResetProbe")).bSuccess);
 const float CooldownBefore = Rejected.Runtime->GetCooldownRemaining(TEXT("ResetProbe"));
 TestTrue(TEXT("Confirmed cooldown exists before Reset"), CooldownBefore > 0.0f);
 Rejected.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 40.0f);
 const uint64 Reservation = Rejected.ASC->AllocateEnergyTransactionId();
 int32 HealthChanges = 0, EnergyChanges = 0, Rejections = 0;
 const auto RejectEvent = Rejected.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
  { if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("ResetResourceChangeRejected")) ++Rejections; });
 const auto EnergyHandle = Rejected.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&) { ++EnergyChanges; });
 const auto RejectHealthHandle = Rejected.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   ++HealthChanges;
   TestTrue(TEXT("Health callback legally increases maximum"), Rejected.ASC->RequestResourceChange(true, EGameplayModOp::Override, 200.0f));
   TestTrue(TEXT("Health callback legally increases actual energy"), Rejected.ASC->RequestResourceChange(false, EGameplayModOp::Override, 200.0f));
   TestTrue(TEXT("Health callback establishes independent reservation"), Rejected.ASC->BeginEnergyEscrow(Reservation, 150.0f));
  });
 Rejected.Runtime->ResetSkills();
 Rejected.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetHealthAttribute()).Remove(RejectHealthHandle);
 Rejected.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(EnergyHandle);
 Rejected.Runtime->OnNativeSkillEvent.Remove(RejectEvent);
 TestEqual(TEXT("Health changed once during formal Reset"), HealthChanges, 1);
 TestEqual(TEXT("Resource rejection reported exactly once"), Rejections, 1);
 TestEqual(TEXT("Rejected Reset does not write or compensate Energy"), EnergyChanges, 1);
 TestEqual(TEXT("Actual Energy retains legal callback change"), Rejected.ASC->GetActualEnergy(), 200.0f);
 TestEqual(TEXT("Maximum retains legal callback change"), Rejected.Runtime->GetMaxEnergyValue(), 200.0f);
 TestEqual(TEXT("Independent reservation remains"), Rejected.ASC->GetReservedEnergy(), 150.0f);
 TestEqual(TEXT("Available energy reflects retained reservation"), Rejected.ASC->GetAvailableEnergy(), 50.0f);
 TestEqual(TEXT("Failed Reset does not clear confirmed cooldown"), Rejected.Runtime->GetCooldownRemaining(TEXT("ResetProbe")), CooldownBefore);
 TestTrue(TEXT("Rejected Reset releases barrier for another skill"), F.Submit(Rejected, TEXT("WaitingProbe")).bSuccess);
 TestTrue(TEXT("Independent reservation remains releasable"), Rejected.ASC->ResolveEnergyEscrow(Reservation, false));

 auto Applied = F.MakeUnit(FVector(750, 0, 0));
 Applied.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f);
 int32 AppliedNotifications = 0; FSkillLabSkillResult AppliedRequest;
 const auto AppliedHandle = Applied.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   ++AppliedNotifications;
   Applied.Runtime->ResetSkills();
   AppliedRequest = F.Submit(Applied, TEXT("WaitingProbe"));
  });
 Applied.Runtime->ResetSkills();
 Applied.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(AppliedHandle);
 TestEqual(TEXT("Successful Reset writes Energy once despite delegate reentry"), AppliedNotifications, 1);
 TestEqual(TEXT("Reset resource delegate still observes the barrier"), AppliedRequest.Reason, FName(TEXT("Resetting")));
 TestEqual(TEXT("Successful Reset resource write reaches max"), Applied.ASC->GetActualEnergy(), 100.0f);
 TestTrue(TEXT("Request succeeds after final receipt"), F.Submit(Applied, TEXT("WaitingProbe")).bSuccess);

 auto DestroyOnApply = F.MakeUnit(FVector(800, 0, 0));
 DestroyOnApply.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f);
 int32 DestroyNotifications = 0;
 const auto DestroyOnApplyHandle = DestroyOnApply.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   ++DestroyNotifications;
   DestroyOnApply.Runtime->DestroyComponent();
  });
 DestroyOnApply.Runtime->ResetSkills();
 DestroyOnApply.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DestroyOnApplyHandle);
 TestEqual(TEXT("Runtime destroyed once during actual Reset resource write"), DestroyNotifications, 1);
 TestEqual(TEXT("Applied resource fact remains after Runtime destruction"), DestroyOnApply.ASC->GetActualEnergy(), 100.0f);
 F.RebuildRuntime(DestroyOnApply);
 TestTrue(TEXT("Replacement accepts a new request after old receipt"), F.Submit(DestroyOnApply, TEXT("WaitingProbe")).bSuccess);

 auto Queued = F.MakeUnit(FVector(1000, 0, 0));
 TestTrue(TEXT("Queued setup changes energy"), Queued.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f));
 bool bEntered = false;
 FSkillLabSkillResult DuringReset;
 const FDelegateHandle Handle = Queued.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   if (bEntered) return;
   bEntered = true;
   Queued.Runtime->ResetSkills();
   Queued.Runtime->ResetSkills();
   DuringReset = F.Submit(Queued, TEXT("ResetProbe"));
  });
 TestTrue(TEXT("Controlled resource write enters the real delegate"), Queued.ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f));
 Queued.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Handle);
 TestTrue(TEXT("Resource delegate invoked Reset"), bEntered);
 TestFalse(TEXT("Queued Reset still blocks skill requests in the delegate"), DuringReset.bSuccess);
 TestEqual(TEXT("Queued Reset reports its active barrier"), DuringReset.Reason, FName(TEXT("Resetting")));
 TestEqual(TEXT("Queued Reset applies after the outer resource write"), Queued.ASC->GetActualEnergy(), Queued.Runtime->GetMaxEnergyValue());
 TestTrue(TEXT("Completed queued Reset releases request barrier"), F.Submit(Queued, TEXT("ResetProbe")).bSuccess);

 auto Replaced = F.MakeUnit(FVector(2000, 0, 0));
 Replaced.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f);
 bool bReplaced = false; FSkillLabSkillResult ReplacementCast;
 const auto ReplaceHandle = Replaced.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   if (bReplaced) return;
   bReplaced = true;
   Replaced.Runtime->ResetSkills();
   F.RebuildRuntime(Replaced);
   ReplacementCast = F.Submit(Replaced, TEXT("WaitingProbe"));
  });
 Replaced.ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
 Replaced.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(ReplaceHandle);
 TestTrue(TEXT("Runtime replaced inside resource delegate"), bReplaced);
 TestTrue(TEXT("Replacement starts a new Cast before old queue drains"), ReplacementCast.bSuccess);
 TestEqual(TEXT("Old Runtime queued restore cannot write after replacement"), Replaced.ASC->GetActualEnergy(), 41.0f);
 TestNotNull(TEXT("Old restore does not cancel replacement Cast"), Replaced.Runtime->FindExecution(ReplacementCast.CastToken));

 auto Failed = F.MakeUnit(FVector(2500, 0, 0));
 Failed.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f);
 int32 FailureReports = 0; bool bBypassed = false;
 const auto FailureEvent = Failed.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
  { if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("ResetResourceChangeFailed")) ++FailureReports; });
 const auto BypassHandle = Failed.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   if (!bBypassed) { bBypassed = true; Failed.ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 70.0f); }
  });
 AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
 AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
 Failed.Runtime->ResetSkills();
 Failed.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(BypassHandle);
 Failed.Runtime->OnNativeSkillEvent.Remove(FailureEvent);
 TestTrue(TEXT("Fault injection entered actual Reset resource write"), bBypassed);
 TestEqual(TEXT("Attempted write failure reported exactly once"), FailureReports, 1);
 TestEqual(TEXT("Failed Reset does not compensate observed resource side effect"), Failed.ASC->GetActualEnergy(), 70.0f);

 auto EndedASC = F.MakeUnit(FVector(3000, 0, 0));
 EndedASC.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f);
 bool bEnded = false; int32 Unavailable = 0;
 const auto EventHandle = EndedASC.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
  {
   if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("ResetResourceChangeUnavailable"))
   {
    ++Unavailable;
    if (Unavailable == 1) EndedASC.Runtime->ResetSkills();
   }
  });
 const auto EndHandle = EndedASC.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   if (bEnded) return;
   bEnded = true;
   EndedASC.Runtime->ResetSkills();
   EndedASC.ASC->EndPlay(EEndPlayReason::RemovedFromWorld);
  });
 EndedASC.ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
 EndedASC.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(EndHandle);
 EndedASC.Runtime->OnNativeSkillEvent.Remove(EventHandle);
 TestTrue(TEXT("ASC ends inside resource delegate"), bEnded);
 TestEqual(TEXT("ASC closure and reentrant new attempt each report unavailable once"), Unavailable, 2);
 TestEqual(TEXT("ASC closure cannot apply queued restore"), EndedASC.ASC->GetActualEnergy(), 41.0f);

 auto Destroyed = F.MakeUnit(FVector(4000, 0, 0));
 Destroyed.ASC->RequestResourceChange(false, EGameplayModOp::Override, 40.0f);
 bool bDestroyed = false;
 const auto DestroyHandle = Destroyed.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
  [&](const FOnAttributeChangeData&)
  {
   if (bDestroyed) return;
   bDestroyed = true;
   Destroyed.Runtime->ResetSkills();
   Destroyed.Actor->Destroy();
  });
 Destroyed.ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
 if (IsValid(Destroyed.ASC)) Destroyed.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DestroyHandle);
 TestTrue(TEXT("Actor destroyed during queued Reset"), bDestroyed);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabResetSubmissionDuringCompletionTest, "SkillLab.Skills.ResetSubmissionDuringCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabResetSubmissionDuringCompletionTest::RunTest(const FString&)
{
 FSkillLabProductionFixture F(this);
 if (!F.bReady) return false;
 auto U = F.MakeUnit();
 if (!TestTrue(TEXT("Formal Cast starts"), F.Start(U).bSuccess) || !F.Begin(U)) return false;
 const uint64 Transaction = U.Production.Key.Transaction;
 F.Apply(U, U.Actor);
 U.Session->SetComponentTickEnabled(false);
 int32 Blocked = 0;
 const auto Handle = U.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
  { if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("ResetBlockedByUnconfirmedSubmission")) ++Blocked; });
 U.Runtime->ResetSkills();
 TestEqual(TEXT("Submitting Reset waits for submission handoff"), Blocked, 0);
 TestEqual(TEXT("Pending submission has one reservation"), U.ASC->GetReservedEnergy(), 2.0f);
 F.LoseReturn(U);
 TestEqual(TEXT("New unresolved result blocks Reset once"), Blocked, 1);
 TestTrue(TEXT("Unresolved transaction retained"), U.ASC->HasIndeterminateEnergyEscrow(Transaction));
 TestEqual(TEXT("Reset does not restore or debit pending energy"), U.ASC->GetActualEnergy(), 100.0f);
 TestEqual(TEXT("Reset retains exactly one reservation"), U.ASC->GetReservedEnergy(), 2.0f);
 TestTrue(TEXT("Original skill remains isolated"), U.Session->BlocksSkill(TEXT("Receipt_Instant")));
 TestEqual(TEXT("No cooldown is invented before confirmation"), U.Runtime->GetCooldownRemaining(TEXT("Receipt_Instant")), 0.0f);
 U.Session->ReconcileProductionResults();
 U.Session->ReconcileProductionResults();
 TestFalse(TEXT("Real late producer result clears isolation"), U.Session->HasUnconfirmed());
 TestEqual(TEXT("Late result charges once"), U.ASC->GetActualEnergy(), 98.0f);
 TestEqual(TEXT("Late result clears reservation"), U.ASC->GetReservedEnergy(), 0.0f);
 TestTrue(TEXT("Late confirmation starts cooldown"), U.Runtime->GetCooldownRemaining(TEXT("Receipt_Instant")) > 0.0f);
 TestEqual(TEXT("Reconciliation does not repeat Reset terminal"), Blocked, 1);
 U.Runtime->OnNativeSkillEvent.Remove(Handle);
 return true;
}
#endif
