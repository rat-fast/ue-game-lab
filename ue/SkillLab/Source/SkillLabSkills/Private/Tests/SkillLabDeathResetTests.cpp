#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabDeathResetTest, "SkillLab.Skills.DeathReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabDeathResetTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const auto Json = TEXT(R"({"skills":[{"id":"ReviveProbe","luaScript":"probe.lua","cost":7,"cooldown":4,"magnitude":1,"range":500,"active":2,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":1}]})");
 const auto Lua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
 if (!F.bReady || !F.LoadCatalog(Json, {{TEXT("probe.lua"), Lua}})) return false;
 auto U = F.MakeUnit();
 auto Attacker = F.MakeUnit(FVector(250, 0, 0));
 for (int32 Cycle = 0; Cycle < 2; ++Cycle)
 {
  const auto Waiting = F.Submit(U, TEXT("ReviveProbe"));
  if (!TestTrue(TEXT("Skill waits for release before death"), Waiting.bSuccess)) return false;
  const auto OldAbility = SkillLabObservedAbility(U.Runtime);
  TestEqual(TEXT("Next intent is buffered before death"), F.Submit(U, TEXT("ReviveProbe")).Reason, FName(TEXT("InputBufferedLatestWins")));
  TestTrue(TEXT("Authoritative damage kills the caster"), Attacker.ASC->ApplyDamageToTarget(U.ASC, 1000.0f, Attacker.Actor, SkillLabGASTags::Event_Attack_Normal));
  TestEqual(TEXT("Lethal damage reaches zero Health"), U.ASC->GetCombatAttributeSet()->GetHealth(), 0.0f);
  TestTrue(TEXT("Lethal damage adds Dead state"), U.ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead));
  TestTrue(TEXT("Death ends the active Cast"), U.Runtime->GetExecutionTokens().IsEmpty());
  TestTrue(TEXT("Death ends its Ability"), !OldAbility.IsValid() || !OldAbility->IsActive());
  TestEqual(TEXT("Dead caster is gated"), F.Submit(U, TEXT("ReviveProbe")).Reason, FName(TEXT("CasterDead")));
  U.Runtime->ResetSkills();
  TestEqual(TEXT("Reset restores Energy"), U.ASC->GetActualEnergy(), U.ASC->GetCombatAttributeSet()->GetMaxEnergy());
  TestEqual(TEXT("Reset restores Health"), U.ASC->GetCombatAttributeSet()->GetHealth(), U.ASC->GetCombatAttributeSet()->GetMaxHealth());
  TestFalse(TEXT("Reset removes the Health-owned Dead state"), U.ASC->HasMatchingGameplayTag(SkillLabGASTags::State_Dead));
  F.Tick(0.05f);
  TestTrue(TEXT("Death and Reset discard buffered intent"), U.Runtime->GetExecutionTokens().IsEmpty());
  TestFalse(TEXT("Old release cannot revive an old Cast"), F.Notify(U, Waiting.CastId, SkillLabGASTags::Event_Cast));
  const auto Fresh = F.Submit(U, TEXT("ReviveProbe"));
  if (!TestTrue(TEXT("New skill activates after death Reset"), Fresh.bSuccess)) return false;
  TestTrue(TEXT("New cast has a fresh identity"), Fresh.CastId != Waiting.CastId);
  TestTrue(TEXT("New GameplayEvent releases through Ability"), F.Notify(U, Fresh.CastId, SkillLabGASTags::Event_Cast));
  TestEqual(TEXT("New cast pays once"), U.ASC->GetActualEnergy(), 93.0f);
  TestTrue(TEXT("New cast completes"), U.Runtime->GetExecutionTokens().IsEmpty());
  U.Runtime->ResetSkills();
 }
 return true;
}
#endif
