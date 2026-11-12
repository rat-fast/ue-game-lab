#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSubmissionFeedbackProbe.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"

namespace
{
 FString OwnershipCatalog()
 {
  return TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"Reject","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Wait","skill":"Buffer","dodge":"Reject","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[
   {"id":"Parallel","luaScript":"hold.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
   {"id":"Recovery","luaScript":"hold.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"recovery":0.3,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
   {"id":"Next","luaScript":"hold.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
   {"id":"Latest","luaScript":"hold.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
   {"id":"Expensive","luaScript":"hold.lua","cost":200,"cooldown":0,"magnitude":2,"range":5000,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
   {"id":"Notify","luaScript":"hold.lua","cost":5,"cooldown":0,"magnitude":2,"range":5000,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":0.15,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
   {"id":"Race","luaScript":"hold.lua","cost":70,"cooldown":0,"magnitude":2,"range":5000,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":2,"active":5,"completionEventTag":"SkillLab.Event.Cast.Complete"}
  ]})");
 }
 TMap<FString,FString> OwnershipSources()
 {
  return {{TEXT("hold.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\",\"combatRules\",\"Open\",\"recoveryRules\",\"Wait\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end")}};
 }
 USkillLabLuaSkillBehavior* Behavior(const FSkillLabCombatTestFixture::FUnit& Unit, const FSkillLabSkillResult& Result)
 {
  const auto* E = Unit.Runtime->FindExecution(Result.CastToken);
  return E ? Cast<USkillLabLuaSkillBehavior>(E->ActiveBehavior) : nullptr;
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastExecutionOwnershipTest, "SkillLab.Skills.CastExecutionOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastExecutionOwnershipTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 if (!F.LoadCatalog(OwnershipCatalog(), OwnershipSources())) return false;
 auto U = F.MakeUnit();
 TStrongObjectPtr<USkillLabSubmissionFeedbackProbe> Probe(NewObject<USkillLabSubmissionFeedbackProbe>());
 TArray<FSkillLabSkillResult> DynamicEvents;
 Probe->OnAnyEvent = [&](const auto& R) { DynamicEvents.Add(R); };
 U.Runtime->OnSkillEvent.AddDynamic(Probe.Get(), &USkillLabSubmissionFeedbackProbe::Receive);
 auto A = F.Submit(U, TEXT("Parallel")); auto B = F.Submit(U, TEXT("Parallel"));
 if (!TestTrue(TEXT("Allow starts two formal same-spec Casts"), A.bSuccess && B.bSuccess && U.Runtime->GetExecutionTokens().Num() == 2)) return false;
 auto AbilityA = U.Runtime->FindExecution(A.CastToken)->ActiveAbility;
 auto AbilityB = U.Runtime->FindExecution(B.CastToken)->ActiveAbility;
 TestTrue(TEXT("Same spec has distinct LocalOnly execution instances"), AbilityA.IsValid() && AbilityB.IsValid() && AbilityA != AbilityB);
 TestEqual(TEXT("Two Casts each commit one cost"), U.ASC->GetActualEnergy(), 90.0f);
 auto Failed = F.Submit(U, TEXT("Expensive"));
 TestEqual(TEXT("New candidate keeps formal available-energy gate"), Failed.Reason, FName(TEXT("InsufficientEnergy")));
 TestNotNull(TEXT("Failed candidate preserves old Cast A"), U.Runtime->FindExecution(A.CastToken));
 TestTrue(TEXT("Failed candidate preserves old Ability B"), AbilityB->IsSkillAbilityActive());
 // Actual ASC GameplayEvent with a wrong token must not borrow the other instance.
 auto* Identity = NewObject<USkillLabCastGameplayEventData>(U.Runtime);
 Identity->Runtime = U.Runtime; Identity->Ability = AbilityB; Identity->CastId = B.CastId; Identity->CastToken = A.CastToken;
 FGameplayEventData Payload; Payload.EventTag = SkillLabGASTags::Event_Cast_Complete; Payload.Instigator = U.Actor; Payload.OptionalObject = Identity;
 U.ASC->HandleGameplayEvent(Payload.EventTag, &Payload);
 TestNotNull(TEXT("Cross-Cast GameplayEvent is ignored"), U.Runtime->FindExecution(B.CastToken));
 AbilityA->FinishSkillAbility(true);
 TestNull(TEXT("External Ability End only removes A"), U.Runtime->FindExecution(A.CastToken));
 TestNotNull(TEXT("External Ability End leaves B"), U.Runtime->FindExecution(B.CastToken));
 F.Notify(U, B.CastId, SkillLabGASTags::Event_Cast_Complete);
 TestTrue(TEXT("Real GameplayEvent completes matching Ability"), U.Runtime->GetExecutionTokens().IsEmpty() && (!AbilityB.IsValid() || !AbilityB->IsSkillAbilityActive()));
 for (const auto& R : DynamicEvents)
  if (R.CastId == A.CastId || R.CastId == B.CastId)
   TestTrue(TEXT("Actual dynamic lifecycle channel retains complete Cast token"), R.CastToken == (R.CastId == A.CastId ? A.CastToken : B.CastToken));
 // Two accepted requests compete against the same actual available energy when released.
 U.Runtime->ResetSkills();
 auto R1 = F.Submit(U, TEXT("Race")); auto R2 = F.Submit(U, TEXT("Race"));
 TestTrue(TEXT("Uncommitted same-spec requests can coexist"), R1.bSuccess && R2.bSuccess);
 F.Notify(U, R1.CastId, SkillLabGASTags::Event_Cast);
 F.Notify(U, R2.CastId, SkillLabGASTags::Event_Cast);
 TestEqual(TEXT("Shared energy charges only successful release"), U.ASC->GetActualEnergy(), 30.0f);
 TestNotNull(TEXT("Resource race preserves first execution"), U.Runtime->FindExecution(R1.CastToken));
 TestNull(TEXT("Second release fails its own resource gate"), U.Runtime->FindExecution(R2.CastToken));
 U.Runtime->ResetSkills();
 auto N = F.Submit(U, TEXT("Notify"));
 TStrongObjectPtr<USkillLabCastExecution> Waiting(const_cast<USkillLabCastExecution*>(U.Runtime->FindExecution(N.CastToken)));
 auto WaitingAbility = Waiting->ActiveAbility;
 F.Tick(0.22f);
 TestNull(TEXT("Notify timeout actually expires without auto-effect"), U.Runtime->FindExecution(N.CastToken));
 TestEqual(TEXT("Notify timeout costs no energy"), U.ASC->GetActualEnergy(), 100.0f);
 TestFalse(TEXT("Timed-out Ability has ended"), WaitingAbility.IsValid() && WaitingAbility->IsSkillAbilityActive());
 TestFalse(TEXT("Old Notify Timer is cleared"), F.World->GetTimerManager().IsTimerActive(Waiting->ActiveTimeoutTimer));
 TestFalse(TEXT("Late exact token cannot release a timed-out Cast"), U.Runtime->ForwardValidatedNotify(N.CastToken, SkillLabGASTags::Event_Cast));
 auto OldToken = N.CastToken;
 F.RebuildRuntime(U);
 auto New = F.Submit(U, TEXT("Notify"));
 TestFalse(TEXT("Replacement Runtime rejects complete old identity"), U.Runtime->ForwardValidatedNotify(OldToken, SkillLabGASTags::Event_Cast));
 TestNotNull(TEXT("Replacement execution remains waiting"), U.Runtime->FindExecution(New.CastToken));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabSharedCastArbitrationTest, "SkillLab.Skills.SharedCastArbitration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabSharedCastArbitrationTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 if (!F.LoadCatalog(OwnershipCatalog(), OwnershipSources())) return false;
 auto U = F.MakeUnit(); TArray<FSkillLabSkillResult> Events;
 U.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& Result) { Events.Add(Result); });
 auto A = F.Submit(U, TEXT("Recovery")); auto B = F.Submit(U, TEXT("Parallel"));
 F.Notify(U, A.CastId, SkillLabGASTags::Event_Cast_Complete);
 const auto C = F.Submit(U, TEXT("Next"));
 TestEqual(TEXT("Older Recovery constrains third request despite newer Allow"), C.Reason, FName(TEXT("InputBufferedLatestWins")));
 const auto D = F.Submit(U, TEXT("Latest"));
 TestEqual(TEXT("Shared waiting slot accepts latest replacement"), D.Reason, FName(TEXT("InputBufferedLatestWins")));
 F.Notify(U, B.CastId, SkillLabGASTags::Event_Cast_Complete);
 F.Tick(0.1f);
 TestEqual(TEXT("Other completion cannot bypass remaining Recovery"), U.Runtime->GetExecutionTokens().Num(), 1);
 F.Tick(0.3f);
 TestEqual(TEXT("All restrictions cleared consumes latest intent"), U.Runtime->GetActiveSkillId(), FName(TEXT("Latest")));
 const auto* Latest = U.Runtime->GetSingleExecution();
 TestTrue(TEXT("Consumption allocates a fresh request identity"), Latest && Latest->ActiveRequest.RequestId != D.RequestId);
 // Dodge receives the explicit cancel-all authorization even when profiles say Reject.
 auto B2 = F.Submit(U, TEXT("Parallel"));
 FSkillLabSkillRequest Dodge; Dodge.Caster = U.Actor; Dodge.SkillId = TEXT("Expensive");
 auto FailedDodge = U.Runtime->SubmitSkillOperationRequest(Dodge, ESkillLabOperationCategory::Dodge);
 TestEqual(TEXT("Dodge still passes resource gate after cancellation"), FailedDodge.Reason, FName(TEXT("InsufficientEnergy")));
 TestTrue(TEXT("Failed dodge restores none of the old Casts"), U.Runtime->GetExecutionTokens().IsEmpty());
 F.Submit(U, TEXT("Parallel")); F.Submit(U, TEXT("Parallel")); Dodge.SkillId = TEXT("Next");
 auto GoodDodge = U.Runtime->SubmitSkillOperationRequest(Dodge, ESkillLabOperationCategory::Dodge);
 TestTrue(TEXT("Successful dodge cancels both then activates one"), GoodDodge.bSuccess && U.Runtime->GetExecutionTokens().Num() == 1);
 U.Runtime->OnNativeSkillEvent.Clear();
 return true;
}
#endif
