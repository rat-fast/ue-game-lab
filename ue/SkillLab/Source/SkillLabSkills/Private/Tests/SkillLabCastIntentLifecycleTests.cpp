#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabGASTags.h"
#include "SkillLabCombatAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastIntentLifecycleTest, "SkillLab.Skills.CastIntentLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastIntentLifecycleTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Wait","skill":"Buffer","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[
 {"id":"Prepared","luaScript":"prepare.lua","cost":5,"cooldown":0,"magnitude":1,"range":1000,"windup":0.05,"active":2,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":0.5,"completionEventTag":"SkillLab.Event.Cast.Complete"},
 {"id":"Waiting","luaScript":"wait.lua","cost":5,"cooldown":0,"magnitude":1,"range":1000,"active":2,"completionEventTag":"SkillLab.Event.Cast.Complete"},
 {"id":"Expensive","luaScript":"open.lua","cost":200,"cooldown":0,"magnitude":1,"range":1000,"active":2,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})");
 const FString Open = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\",\"combatRules\",\"Open\",\"recoveryRules\",\"Open\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 const FString Prepare = Open.Replace(TEXT("\"recoveryRules\",\"Open\""),TEXT("\"recoveryRules\",\"Open\",\"windupEnd\",\"Timer\""));
 const FString Wait = Open.Replace(TEXT("\"Open\""),TEXT("\"Wait\""));
 if (!F.LoadCatalog(Json,{{TEXT("open.lua"),Open},{TEXT("prepare.lua"),Prepare},{TEXT("wait.lua"),Wait}})) return false;
 auto U = F.MakeUnit();
 const auto A = F.Submit(U,TEXT("Prepared"));
 TestEqual(TEXT("Preparation begins without effect cost"),U.ASC->GetActualEnergy(),100.0f);
 TestFalse(TEXT("Release arriving before independent preparation ends is ignored"),U.Runtime->FindExecution(A.CastToken)->Lifecycle.GetSnapshot().bHasReleased);
 F.Notify(U,A.CastId,SkillLabGASTags::Event_Cast); F.Tick(0.08f);
 const auto* Prepared = U.Runtime->FindExecution(A.CastToken);
 TestTrue(TEXT("Independent preparation ends while still waiting for release evidence"),Prepared && Prepared->Lifecycle.GetStage() == FSkillLabCastLifecycle::EStage::Active && !Prepared->Lifecycle.GetSnapshot().bHasReleased);
 TestEqual(TEXT("Windup Timer does not submit"),U.ASC->GetActualEnergy(),100.0f);
 F.Notify(U,A.CastId,SkillLabGASTags::Event_Cast);
 TestEqual(TEXT("Later real Ability release event submits once"),U.ASC->GetActualEnergy(),95.0f);
 F.Notify(U,A.CastId,SkillLabGASTags::Event_Cast_Complete);
 TestTrue(TEXT("Prepared Cast finishes after explicit completion"),U.Runtime->GetExecutionTokens().IsEmpty());

 FName Rejected;
 U.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& R) { if (R.SkillId == TEXT("Expensive") && R.Event == ESkillLabSkillEvent::Rejected) Rejected = R.Reason; });
 const auto W = F.Submit(U,TEXT("Waiting"));
 TestEqual(TEXT("Waiting restriction buffers before formal resource activation"),F.Submit(U,TEXT("Expensive")).Reason,FName(TEXT("InputBufferedLatestWins")));
 F.Notify(U,W.CastId,SkillLabGASTags::Event_Cast_Complete); F.Tick(0.05f);
 TestEqual(TEXT("Cache consumption reruns actual resource gate"),Rejected,FName(TEXT("InsufficientEnergy")));
 TestTrue(TEXT("Failed cache consumption leaves no new Cast"),U.Runtime->GetExecutionTokens().IsEmpty());
 const auto W2 = F.Submit(U,TEXT("Waiting")); F.Submit(U,TEXT("Expensive"));
 U.Runtime->ResetSkills(); Rejected = NAME_None; F.Tick(0.05f);
 TestTrue(TEXT("Reset clears intent and all Casts"),U.Runtime->GetExecutionTokens().IsEmpty() && Rejected.IsNone());
 F.Submit(U,TEXT("Waiting")); F.Submit(U,TEXT("Expensive"));
 auto Attacker = F.MakeUnit(FVector(200,0,0));
 Attacker.ASC->ApplyDamageToTarget(U.ASC,1000,Attacker.Actor,SkillLabGASTags::Event_Attack_Normal); F.Tick(0.05f);
 TestTrue(TEXT("Real lethal damage cancels Casts and prevents cached activation"),!U.Runtime->IsAlive() && U.Runtime->GetExecutionTokens().IsEmpty());
 U.Runtime->OnNativeSkillEvent.Clear();
 return true;
}
#endif
