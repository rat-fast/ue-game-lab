#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabGASTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastRequestReentryTest, "SkillLab.Skills.CastRequestReentry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastRequestReentryTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Wait","skill":"Buffer","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Stop","skill":"Reject","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[
 {"id":"A","luaScript":"wait.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"},
 {"id":"C","luaScript":"open.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"},
 {"id":"D","luaScript":"open.lua","cost":7,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"},
 {"id":"EReject","luaScript":"stop.lua","cost":2,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"},
 {"id":"EBuffer","luaScript":"wait.lua","cost":2,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})");
 const FString Open = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\",\"combatRules\",\"Open\",\"recoveryRules\",\"Open\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 if (!F.LoadCatalog(Json,{{TEXT("open.lua"),Open},{TEXT("wait.lua"),Open.Replace(TEXT("\"Open\""),TEXT("\"Wait\""))},{TEXT("stop.lua"),Open.Replace(TEXT("\"Open\""),TEXT("\"Stop\""))}})) return false;
 for (int32 Mode = 0; Mode < 5; ++Mode)
 {
  auto U = F.MakeUnit(FVector(Mode*1000,0,0));
  TStrongObjectPtr<USkillLabSkillRuntimeComponent> Runtime(U.Runtime);
  FSkillLabSkillResult D, E, NewC; bool bRequestedD = false, bReplacementCallback = false;
  int32 CReplaced = 0, CStarted = 0, CStartedRequest = 0, DStarted = 0, DCommitted = 0, DEffects = 0, DRejected = 0;
  const auto Listener = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& R)
  {
   CStarted += R.SkillId == TEXT("C") && R.Event == ESkillLabSkillEvent::Started;
   if (R.SkillId == TEXT("C") && R.Event == ESkillLabSkillEvent::Started) CStartedRequest = R.RequestId;
   DStarted += R.SkillId == TEXT("D") && R.Event == ESkillLabSkillEvent::Started;
   DCommitted += R.SkillId == TEXT("D") && R.Event == ESkillLabSkillEvent::SubmissionCommitted;
   DEffects += R.SkillId == TEXT("D") && R.Event == ESkillLabSkillEvent::EffectApplied;
   DRejected += R.SkillId == TEXT("D") && R.Event == ESkillLabSkillEvent::Rejected;
   if (R.SkillId == TEXT("A") && R.Event == ESkillLabSkillEvent::Completed && !bRequestedD)
   { bRequestedD = true; D = F.Submit(U,TEXT("D")); }
   if (R.SkillId == TEXT("C") && R.Event == ESkillLabSkillEvent::Rejected && R.Reason == TEXT("BufferedIntentReplaced"))
   {
    ++CReplaced;
    if (bReplacementCallback) return;
    bReplacementCallback = true;
    if (Mode < 2 || Mode == 4)
    {
     E = F.Submit(U,Mode == 0 ? FName(TEXT("EReject")) : FName(TEXT("EBuffer")));
     if (Mode == 4) NewC = F.Submit(U,TEXT("C"));
    }
    else if (Mode == 2) U.Runtime->ResetSkills();
    else U.Runtime->DestroyComponent();
   }
  });
  const auto A = F.Submit(U,TEXT("A"));
  TestTrue(TEXT("A starts through formal request"),A.bSuccess);
  const auto OldC = F.Submit(U,TEXT("C"));
  TestEqual(TEXT("A buffers C through shared arbitration"),OldC.Reason,FName(TEXT("InputBufferedLatestWins")));
  F.Notify(U,A.CastId,SkillLabGASTags::Event_Cast_Complete);
  TestTrue(TEXT("Completed -> replaced-cache callback is exercised"),bRequestedD && bReplacementCallback);
  TestEqual(TEXT("Old C gets one replacement notice"),CReplaced,1);
  TestEqual(TEXT("Cleared C never starts"),CStarted,0);
  TestEqual(TEXT("Outer D cannot start from a stale activation plan"),DStarted,0);
  TestEqual(TEXT("Outer D cannot produce cost/effect commitment"),DCommitted,0);
  TestEqual(TEXT("Outer D cannot apply an effect before arbitration"),DEffects,0);
  TestFalse(TEXT("Outer D is rejected or buffered"),D.bSuccess);
  TestEqual(TEXT("Live receiver gets one immediate D result; EndPlay receiver gets none"),DRejected,Mode == 3 ? 0 : 1);
  if (Mode < 2 || Mode == 4)
  {
   TestTrue(TEXT("Nested E activates formally"),E.bSuccess);
   TestNotNull(TEXT("Outer stale plan does not cancel E"),Runtime->FindExecution(E.CastToken));
   TestEqual(TEXT("Only A and E have committed energy"),U.ASC->GetActualEnergy(),97.0f);
   TestEqual(TEXT("New E decision or newer input is observable"),D.Reason,Mode == 0 ? FName(TEXT("Stop")) : Mode == 1 ? FName(TEXT("InputBufferedLatestWins")) : FName(TEXT("OperationReentered")));
   F.Tick(0.05f);
   TestEqual(TEXT("Next tick cannot bypass E restriction"),DStarted,0);
   F.Notify(U,E.CastId,SkillLabGASTags::Event_Cast_Complete); F.Tick(0.05f);
   TestEqual(TEXT("Only buffered D consumes when E naturally ends"),DStarted,Mode == 1 ? 1 : 0);
   TestEqual(TEXT("Only explicitly requeued newer C consumes"),CStarted,Mode == 4 ? 1 : 0);
   if (Mode == 4) TestTrue(TEXT("Preserved latest intent consumes once under a fresh request identity"),NewC.RequestId != OldC.RequestId && CStartedRequest != OldC.RequestId && CStartedRequest != NewC.RequestId && CReplaced == 1);
  }
  else
  {
   F.Tick(0.05f);
   TestTrue(TEXT("Reset/EndPlay callback leaves no Cast from invalidated request"),Runtime->GetExecutionTokens().IsEmpty());
   TestEqual(TEXT("Reset/EndPlay callback cannot charge D"),U.ASC->GetActualEnergy(),Mode == 2 ? 100.0f : 99.0f);
   TestEqual(TEXT("Reset/EndPlay invalidation is explicit"),D.Reason,FName(TEXT("OperationReentered")));
  }
  Runtime->OnNativeSkillEvent.Remove(Listener);
 }
 return true;
}
#endif
