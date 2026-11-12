#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"
#include "SkillLabUnconfirmedSubmissions.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabMultipleSubmissionsTest, "SkillLab.Skills.MultipleSubmissions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabMultipleSubmissionsTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const auto Json = TEXT(R"({"skills":[{"id":"Multi","luaScript":"multi.lua","cost":7,"cooldown":4,"magnitude":3,"range":500,"active":2,"completionMode":"AllSubmissionPointsResolved","submissionPoints":[{"id":1,"trigger":"Release","target":"Source"},{"id":2,"trigger":"Timer","delay":0.05,"target":"Source"},{"id":3,"trigger":"GameplayEvent","eventTag":"SkillLab.Event.Cast.Complete","target":"Source"},{"id":4,"trigger":"Script","target":"Source"}]}]})");
 const auto Lua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) local bad = SkillLabSkill_RequestAction(4294967300); if bad.accepted then invalid_overflow_submission() end; SkillLabSkill_RequestAction(4) end");
 if (!F.LoadCatalog(Json, {{TEXT("multi.lua"), Lua}})) return false;
 auto U = F.MakeUnit();
 int32 Effects = 0, Commits = 0;
 U.Runtime->OnNativeSkillEvent.AddLambda([&](const auto& R) { Effects += R.Event == ESkillLabSkillEvent::EffectApplied; Commits += R.Event == ESkillLabSkillEvent::SubmissionCommitted; });
 const auto Start = F.Submit(U, TEXT("Multi"));
 const auto* E = U.Runtime->FindExecution(Start.CastToken);
 if (!TestTrue(TEXT("Formal multi-point activation is alive"), Start.bSuccess && E)) return false;
 TStrongObjectPtr<USkillLabLuaSkillBehavior> Behavior(Cast<USkillLabLuaSkillBehavior>(E->ActiveBehavior));
 TestEqual(TEXT("Release and explicit Script hit the same target independently"), Effects, 2);
 TestEqual(TEXT("Unique cost committed once"), U.ASC->GetActualEnergy(), 93.0f);
 TestFalse(TEXT("Explicit definition cannot use unqualified legacy action"), Behavior->RequestDeclaredAction());
 TestFalse(TEXT("Unknown point is rejected without cancel"), Behavior->RequestDeclaredAction(99));
 TestFalse(TEXT("Script cannot forge event trigger"), Behavior->RequestDeclaredAction(3));
 TestFalse(TEXT("Duplicate success is rejected without cancel"), Behavior->RequestDeclaredAction(4));
 TestNotNull(TEXT("Invalid point requests preserve Cast"), U.Runtime->FindExecution(Start.CastToken));
 F.Tick(0.08f);
 TestEqual(TEXT("Real Timer submits the third same-target effect"), Effects, 3);
 const float CooldownBefore = U.Runtime->GetCooldownRemaining(TEXT("Multi"));
 F.Notify(U, Start.CastId, SkillLabGASTags::Event_Cast_Complete);
 TestEqual(TEXT("Ability GameplayEvent submits final point"), Effects, 4);
 TestEqual(TEXT("Each point has one committed fact"), Commits, 4);
 TestEqual(TEXT("Four points still cost once"), U.ASC->GetActualEnergy(), 93.0f);
 TestTrue(TEXT("Later points do not restart cooldown"), U.Runtime->GetCooldownRemaining(TEXT("Multi")) <= CooldownBefore);
 TestTrue(TEXT("All resolved mode completes after the whole set"), U.Runtime->GetExecutionTokens().IsEmpty());
 const auto P1 = Behavior->GetPointProduction(1), P2 = Behavior->GetPointProduction(2), P3 = Behavior->GetPointProduction(3), P4 = Behavior->GetPointProduction(4);
 TestTrue(TEXT("Each point has a distinct production operation"), P1.Key.Operation != P2.Key.Operation && P2.Key.Operation != P3.Key.Operation && P3.Key.Operation != P4.Key.Operation);
 TestTrue(TEXT("Only first successful point owns a transaction"), P1.Key.Transaction != 0 && P2.Key.Transaction == 0 && P3.Key.Transaction == 0 && P4.Key.Transaction == 0);
 TestFalse(TEXT("Post-terminal point cannot submit"), Behavior->RequestDeclaredAction(4));
 U.Runtime->OnNativeSkillEvent.Clear();
 return true;
}
#endif
