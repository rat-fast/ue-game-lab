#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "SkillLabCombatTestFixture.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"
#include "SkillLabSkillBehavior.h"
#include "TimerManager.h"

// 各场景独占 World 和目录快照；只组装对象、声明内容和提供必要的私有观察。
class FSkillLabCastScenarioFixture final : public FSkillLabCombatTestFixture
{
public:
 explicit FSkillLabCastScenarioFixture(FAutomationTestBase* InTest) : FSkillLabCombatTestFixture(InTest)
 { bReady = bReady && Load(); }
 static bool HasIntent(const USkillLabSkillRuntimeComponent* Runtime) { return Runtime->PendingSkillIntent.IsSet(); }
 static USkillLabSkillEffectExecutor* Executor(USkillLabSkillRuntimeComponent* Runtime) { return Runtime->EffectExecutor.Get(); }
 static void SetExecutor(USkillLabSkillRuntimeComponent* Runtime, USkillLabSkillEffectExecutor* Value) { Runtime->EffectExecutor = Value; }
 static void FinishRecovery(USkillLabSkillRuntimeComponent* Runtime)
 {
  if (auto* Execution = Runtime->FindMutableExecution(Runtime->GetActiveCastId()))
   Runtime->FinishRecovery(*Execution, Execution->Lifecycle.GetPhaseToken());
 }
 static void LateProjectileResult(USkillLabLuaSkillBehavior* Behavior, AActor* Target, FName Reason)
 { Behavior->OnProjectileResolved(1, nullptr, Target, Reason); }
 FString StateFixtureJson, FixtureError;
 TMap<FString, FString> LuaSources;
 TMap<FName, FSkillLabSkillDefinition> FixtureDefinitions, ImmediateDefinitions, IndependentDefinitions, HoldDefinitions, UtilityDefinitions;
private:
 bool Load()
 {
	const FString FixtureJson = TEXT(R"({"combatRules":[{"id":"Shared","skill":"Buffer","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Recovery","skill":"Buffer","dodge":"Buffer","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Restricted","skill":"Reject","dodge":"Reject","movement":"Reject","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[{"id":"Skill_Mend","luaScript":"Skill_Mend.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3,"ruleWindows":[{"id":"RestrictedWindow","beginEvent":"SkillLab.Event.Window.Open","endEvent":"SkillLab.Event.Window.Close","rules":"Restricted"},{"id":"OtherWindow","beginEvent":"SkillLab.Event.Window.AlternateOpen","endEvent":"SkillLab.Event.Window.AlternateClose","rules":"Shared"}]},{"id":"Skill_BufferedSuccess","luaScript":"Skill_BufferedSuccess.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedVictim","luaScript":"Skill_BufferedVictim.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedFail","luaScript":"Skill_BufferedFail.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3}]})");
	const FString FixtureLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\",\"combatRules\",\"Shared\",\"recoveryRules\",\"Recovery\",\"windowConflict\",\"Reject\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
	StateFixtureJson = FixtureJson.Replace(TEXT("\"id\":\"Restricted\",\"skill\""),
		TEXT("\"id\":\"Restricted\",\"abilityCategories\":{\"Ability.SkillLab.Utility\":\"Reject\"},\"interruptBy\":[\"State.SkillLab.Marked\"],\"skill\""));
	LuaSources.Add(TEXT("Skill_Mend.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedSuccess.lua"), FixtureLua.Replace(TEXT("\"Shared\""), TEXT("\"Restricted\"")));
	LuaSources.Add(TEXT("Skill_BufferedVictim.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedFail.lua"), FixtureLua);
	if (!Test->TestTrue(TEXT("Gameplay Event test definition fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		StateFixtureJson, LuaSources, FixtureDefinitions, FixtureError)))
	{
		Test->AddError(FixtureError);
		return false;
	}
	TMap<FString, FString> ImmediateSources;
	ImmediateSources.Add(TEXT("Immediate.lua"), FixtureLua.Replace(TEXT(",\"combatRules\",\"Shared\",\"recoveryRules\",\"Recovery\""), TEXT("")));
	if (!Test->TestTrue(TEXT("Timing without windup or recovery declarations parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Immediate","luaScript":"Immediate.lua","cost":2,"cooldown":0,"magnitude":1,"range":1000}]})"),
		ImmediateSources, ImmediateDefinitions, FixtureError)))
	{
		Test->AddError(FixtureError);
		return false;
	}
	FixtureDefinitions.Append(ImmediateDefinitions);
	TMap<FString, FString> IndependentSources;
	IndependentSources.Add(TEXT("Independent.lua"), ImmediateSources.FindChecked(TEXT("Immediate.lua")).Replace(
		TEXT("\"execution\",\"InstantTarget\""), TEXT("\"execution\",\"InstantTarget\",\"windupEnd\",\"Timer\"")));
	if (!Test->TestTrue(TEXT("Independent windup fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Independent","luaScript":"Independent.lua","cost":2,"cooldown":0,"magnitude":1,"range":1000,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3}]})"),
		IndependentSources, IndependentDefinitions, FixtureError))) { Test->AddError(FixtureError); return false; }
	FixtureDefinitions.Append(IndependentDefinitions);
	TMap<FString, FString> HoldSources;
	HoldSources.Add(TEXT("Hold.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) end"));
	if (!Test->TestTrue(TEXT("Independent completion event fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Hold","luaScript":"Hold.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})"),
		HoldSources, HoldDefinitions, FixtureError))) { Test->AddError(FixtureError); return false; }
	FixtureDefinitions.Append(HoldDefinitions);
	TMap<FName, FSkillLabSkillDefinition> LuaCheckDefinitions;
	TMap<FString, FString> LuaCheckSources;
	const FString LuaDeclaration = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\n");
	LuaCheckSources.Add(TEXT("ActionCheck.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local r=SkillLabSkill_RequestAction(); if r.accepted or r.applied or not r.pending or r.completed or r.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	LuaCheckSources.Add(TEXT("EffectCheck.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local r=SkillLabSkill_ApplyEffect(); if r.applied or not r.pending or r.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	LuaCheckSources.Add(TEXT("ActionThenEffect.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local a=SkillLabSkill_RequestAction(); local b=SkillLabSkill_ApplyEffect(); if a.accepted or a.applied or not a.pending or a.completed or a.reason ~= 'SubmissionUnconfirmed' or b.applied or not b.pending or b.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	LuaCheckSources.Add(TEXT("EffectThenAction.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local a=SkillLabSkill_ApplyEffect(); local b=SkillLabSkill_RequestAction(); if a.applied or not a.pending or a.reason ~= 'SubmissionUnconfirmed' or b.accepted or b.applied or not b.pending or b.completed or b.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	if (!Test->TestTrue(TEXT("Lua submission fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_LuaAction","luaScript":"ActionCheck.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000},{"id":"Skill_LuaEffect","luaScript":"EffectCheck.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000},{"id":"Skill_ActionThenEffect","luaScript":"ActionThenEffect.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000},{"id":"Skill_EffectThenAction","luaScript":"EffectThenAction.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000}]})"),
		LuaCheckSources, LuaCheckDefinitions, FixtureError))) { Test->AddError(FixtureError); return false; }
	FixtureDefinitions.Append(LuaCheckDefinitions);
	TMap<FString, FString> UtilitySources;
	UtilitySources.Add(TEXT("Utility.lua"), ImmediateSources.FindChecked(TEXT("Immediate.lua")).Replace(
		TEXT("\"execution\",\"InstantTarget\""), TEXT("\"execution\",\"InstantTarget\",\"categoryTag\",\"Ability.SkillLab.Utility\"")));
	if (!Test->TestTrue(TEXT("Registered additional ability category parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Utility","luaScript":"Utility.lua","cost":2,"cooldown":0,"magnitude":1,"range":1000}]})"),
		UtilitySources, UtilityDefinitions, FixtureError))) { Test->AddError(FixtureError); return false; }
	FixtureDefinitions.Append(UtilityDefinitions);
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(FixtureDefinitions);
	return true;
 }
};
#endif
