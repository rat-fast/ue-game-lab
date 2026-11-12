#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastSubmissionIsolationTest, "SkillLab.Skills.CastSubmissionIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastSubmissionIsolationTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[
 {"id":"Multi","luaScript":"self.lua","cost":7,"cooldown":0,"magnitude":3,"range":1000,"active":2,"completionMode":"AllSubmissionPointsResolved","submissionPoints":[{"id":1,"trigger":"Release","target":"Source"},{"id":2,"trigger":"Timer","delay":0.1,"target":"Source"}]},
 {"id":"Retry","luaScript":"unit.lua","cost":5,"cooldown":0,"magnitude":3,"range":1000,"active":2,"completionEventTag":"SkillLab.Event.Cast.Complete","submissionPoints":[{"id":1,"trigger":"Script","target":"RequestTarget"},{"id":2,"trigger":"Timer","delay":0.4,"target":"RequestTarget"}]}
 ]})");
 const FString Rules = TEXT(",\"combatRules\",\"Open\",\"recoveryRules\",\"Open\"");
 const FString Self = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\"") + Rules + TEXT(")\nfunction OnSkillCast(context) end");
 const FString Unit = TEXT("SkillLabSkill_Define(\"targetShape\",\"Unit\",\"relation\",\"Enemy\",\"effect\",\"Damage\",\"execution\",\"InstantTarget\"") + Rules + TEXT(")\nfunction OnSkillCast(context) end");
 if (!F.LoadCatalog(Json,{{TEXT("self.lua"),Self},{TEXT("unit.lua"),Unit}})) return false;
 auto U = F.MakeUnit();
 bool bReentered = false; FSkillLabSkillResult Nested;
 const auto Handle = U.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda([&](const FOnAttributeChangeData& D)
 { if (!bReentered && D.NewValue < D.OldValue) { bReentered = true; Nested = F.Submit(U,TEXT("Multi")); } });
 const auto Outer = F.Submit(U,TEXT("Multi"));
 U.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Handle);
 TestTrue(TEXT("Real energy callback activates another same-spec multi-point Cast"),bReentered && Outer.bSuccess && Nested.bSuccess);
 TestEqual(TEXT("Both Casts retain their own context after reentry"),U.Runtime->GetExecutionTokens().Num(),2);
 TStrongObjectPtr<USkillLabLuaSkillBehavior> A(Cast<USkillLabLuaSkillBehavior>(U.Runtime->FindExecution(Outer.CastToken)->ActiveBehavior));
 TStrongObjectPtr<USkillLabLuaSkillBehavior> B(Cast<USkillLabLuaSkillBehavior>(U.Runtime->FindExecution(Nested.CastToken)->ActiveBehavior));
 TestTrue(TEXT("Original submission retains identity while nested reservation is rejected"),A->GetPointProduction(1).Key.Cast == static_cast<uint64>(Outer.CastId) && !B->GetPointProduction(1).Key.IsSet());
 TestEqual(TEXT("Nested active resource write has an explicit failed point result"), B->GetPointResult(1)->Reason, FName(TEXT("ResourceEscrowFailed")));
 F.Tick(0.15f);
 TestTrue(TEXT("Both independent point Timers complete"),U.Runtime->GetExecutionTokens().IsEmpty());
 TestEqual(TEXT("Each Cast charges on its first successful point only"),U.ASC->GetActualEnergy(),86.0f);
 TestTrue(TEXT("Later points retain Cast identity and nested first success owns its unique transaction"),A->GetPointProduction(2).Key.Cast == static_cast<uint64>(Outer.CastId) && B->GetPointProduction(2).Key.Cast == static_cast<uint64>(Nested.CastId) && !A->GetPointProduction(2).Key.Transaction && B->GetPointProduction(2).Key.Transaction != 0);

 U.Runtime->ResetSkills(); auto Target = F.MakeUnit(FVector(300,0,0)); Target.Runtime->TeamId = 2;
 const auto Retry = F.Submit(U,TEXT("Retry"),Target.Actor);
 const auto* Execution = U.Runtime->FindExecution(Retry.CastToken);
 if (!TestNotNull(TEXT("Formal retry Cast is accepted"),Execution)) return false;
 TStrongObjectPtr<USkillLabLuaSkillBehavior> R(Cast<USkillLabLuaSkillBehavior>(Execution->ActiveBehavior));
 Target.Runtime->TeamId = U.Runtime->TeamId;
 TestFalse(TEXT("Changed relation produces a confirmed failure"),R->RequestDeclaredAction(1));
 const auto Failed = R->GetPointProduction(1);
 TestEqual(TEXT("Confirmed failure costs nothing"),U.ASC->GetActualEnergy(),100.0f);
 Target.Runtime->TeamId = 2;
 F.Tick(0.1f);
 TestEqual(TEXT("There is no automatic failed-point retry"),R->GetPointProduction(1).Key.Operation,Failed.Key.Operation);
 TestTrue(TEXT("Explicit retry can succeed after target validity recovers"),R->RequestDeclaredAction(1));
 TestTrue(TEXT("Retry allocates new production operation"),R->GetPointProduction(1).Key.Operation != Failed.Key.Operation);
 TestEqual(TEXT("Only successful retry commits cost"),U.ASC->GetActualEnergy(),95.0f);
 TestFalse(TEXT("Success cannot be retried"),R->RequestDeclaredAction(1));
 U.Runtime->CancelCast(Retry.CastToken,TEXT("CancelPointTimer"));
 const float HealthAfterCancel = Target.ASC->GetCombatAttributeSet()->GetHealth(); F.Tick(0.5f);
 TestEqual(TEXT("Cancelled point Timer produces no effect"),Target.ASC->GetCombatAttributeSet()->GetHealth(),HealthAfterCancel);
 TestFalse(TEXT("Terminal exact point request is refused"),R->RequestDeclaredAction(2));
 U.Runtime->ResetSkills();
 const auto Lost = F.Submit(U,TEXT("Retry"),Target.Actor);
 const auto* LostExecution = U.Runtime->FindExecution(Lost.CastToken);
 if (!TestNotNull(TEXT("Target destruction fixture starts formally"),LostExecution)) return false;
 TStrongObjectPtr<USkillLabLuaSkillBehavior> LostBehavior(Cast<USkillLabLuaSkillBehavior>(LostExecution->ActiveBehavior));
 Target.Actor->Destroy();
 TestFalse(TEXT("Original destroyed target cannot be replaced by current selection"),LostBehavior->RequestDeclaredAction(1));
 F.Tick(0.5f);
 TestEqual(TEXT("Destroyed target leaves explicit point failure"),LostBehavior->GetPointResult(2)->Reason,FName(TEXT("HitTargetInvalid")));
 TestEqual(TEXT("All failed points cost nothing"),U.ASC->GetActualEnergy(),100.0f);
 F.Notify(U,Lost.CastId,SkillLabGASTags::Event_Cast_Complete);
 TestTrue(TEXT("All explicit failures can finish without stuck wait"),U.Runtime->GetExecutionTokens().IsEmpty());
 return true;
}
#endif
