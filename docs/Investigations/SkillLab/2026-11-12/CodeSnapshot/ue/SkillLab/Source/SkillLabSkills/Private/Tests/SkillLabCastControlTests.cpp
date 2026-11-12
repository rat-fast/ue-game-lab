#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabGASTags.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
 FActiveGameplayEffectHandle GrantControl(USkillLabAbilitySystemComponent* ASC, FGameplayTag Tag)
 {
  auto* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
  Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
  FInheritedTagContainer Tags; Tags.AddTag(Tag);
  Effect->FindOrAddComponent<UTargetTagsGameplayEffectComponent>().SetAndApplyTargetTagChanges(Tags);
  const FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.0f);
  return ASC->ApplyGameplayEffectSpecToSelf(Spec);
 }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastControlAndRulesTest, "SkillLab.Skills.CastControlAndRules", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastControlAndRulesTest::RunTest(const FString&)
{
 FSkillLabCombatTestFixture F(this);
 const FString Json = TEXT(R"({"combatRules":[{"id":"Open","skill":"Allow","dodge":"Reject","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Stop","skill":"Reject","dodge":"Reject","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Wait","skill":"Buffer","dodge":"Reject","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[{"id":"Windows","luaScript":"open.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete","ruleWindows":[{"id":"Reject","beginEvent":"SkillLab.Event.Window.Open","endEvent":"SkillLab.Event.Window.Close","rules":"Stop"},{"id":"Wait","beginEvent":"SkillLab.Event.Window.AlternateOpen","endEvent":"SkillLab.Event.Window.AlternateClose","rules":"Wait"}]},{"id":"Shared","luaScript":"open.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"},{"id":"Added","luaScript":"open.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"},{"id":"Different","luaScript":"stop.lua","cost":1,"cooldown":0,"magnitude":1,"range":500,"active":3,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})");
 const FString Open = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\",\"combatRules\",\"Open\",\"recoveryRules\",\"Open\",\"windowConflict\",\"Reject\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction() end");
 const auto Sources = TMap<FString,FString>{{TEXT("open.lua"),Open},{TEXT("stop.lua"),Open.Replace(TEXT("\"Open\""),TEXT("\"Stop\""))}};
 if (!F.LoadCatalog(Json, Sources)) return false;
 auto U = F.MakeUnit();
 const auto A = F.Submit(U, TEXT("Windows")); const auto B = F.Submit(U, TEXT("Shared"));
 F.Notify(U, A.CastId, FGameplayTag::RequestGameplayTag(TEXT("SkillLab.Event.Window.Open")));
 TestEqual(TEXT("Older explicit Reject beats newer Allow"), F.Submit(U,TEXT("Added")).Reason, FName(TEXT("Stop")));
 F.Notify(U, A.CastId, FGameplayTag::RequestGameplayTag(TEXT("SkillLab.Event.Window.AlternateOpen")));
 TestEqual(TEXT("Ordinary overlapping window remains observable conflict"), F.Submit(U,TEXT("Added")).Reason, FName(TEXT("RuleWindowConflict")));
 U.Runtime->ResetSkills();
 F.Submit(U,TEXT("Windows")); F.Submit(U,TEXT("Shared"));
 FName Reason;
 TestTrue(TEXT("Configured movement continues both"), U.Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, Reason));
 auto Attacker = F.MakeUnit(FVector(250,0,0));
 Attacker.ASC->ApplyDamageToTarget(U.ASC, 1, Attacker.Actor, SkillLabGASTags::Event_Attack_Normal);
 TestEqual(TEXT("Authoritative normal-hit Tag preserves both"), U.Runtime->GetExecutionTokens().Num(), 2);
 Attacker.ASC->ApplyDamageToTarget(U.ASC, 1, Attacker.Actor, SkillLabGASTags::Event_Attack_Heavy);
 TestTrue(TEXT("Authoritative heavy-hit Tag interrupts both"), U.Runtime->GetExecutionTokens().IsEmpty());
 U.Runtime->ResetSkills(); F.Submit(U,TEXT("Windows")); F.Submit(U,TEXT("Shared"));
 for (const auto Tag : {FGameplayTag(SkillLabGASTags::State_Silenced),FGameplayTag(SkillLabGASTags::State_Stunned)})
 {
  const auto H1 = GrantControl(U.ASC, Tag), H2 = GrantControl(U.ASC, Tag);
  TestTrue(TEXT("Two real GE sources are active"), H1.IsValid() && H2.IsValid());
  TestTrue(TEXT("Control interrupts all Casts"), U.Runtime->GetExecutionTokens().IsEmpty());
  U.ASC->RemoveActiveGameplayEffect(H1);
  TestTrue(TEXT("One GE removal preserves remaining source Tag"), U.ASC->HasMatchingGameplayTag(Tag));
  TestEqual(TEXT("Remaining source still blocks activation"), F.Submit(U,TEXT("Shared")).Reason, FName(TEXT("BlockedByControlState")));
  U.ASC->RemoveActiveGameplayEffect(H2);
  TestFalse(TEXT("Last GE removal clears Tag"), U.ASC->HasMatchingGameplayTag(Tag));
  TestTrue(TEXT("After actual final GE removal formal activation recovers"), F.Submit(U,TEXT("Shared")).bSuccess);
 }
 U.Runtime->ResetSkills();
 // Reparse one central profile change, without per-skill branch or repeated edits.
 const auto Changed = Json.Replace(TEXT("\"id\":\"Open\",\"skill\":\"Allow\""), TEXT("\"id\":\"Open\",\"skill\":\"Buffer\""));
 if (!F.LoadCatalog(Changed,Sources)) return false;
 F.Submit(U,TEXT("Shared"));
 TestEqual(TEXT("Shared profile edit affects newly added same-type skill"), F.Submit(U,TEXT("Added")).Reason,FName(TEXT("InputBufferedLatestWins")));
 U.Runtime->ResetSkills();
 F.Submit(U,TEXT("Different"));
 TestEqual(TEXT("Skill choosing a different profile keeps distinct behavior"), F.Submit(U,TEXT("Added")).Reason,FName(TEXT("Stop")));
 return true;
}
#endif
