#include "SkillLabSkillBehavior.h"
#include "SkillLabLua.h"
#include "SkillLabSkillRuntimeComponent.h"

namespace
{
 USkillLabLuaSkillBehavior* GetLuaBehavior(FSkillLabLuaState* State)
 { return static_cast<USkillLabLuaSkillBehavior*>(SkillLabLua::GetCallbackContext(State)); }
 int32 ReadPointId(FSkillLabLuaState* State)
 {
  if (SkillLabLua::GetTop(State) == 0) return 0;
  const int64 Value = SkillLabLua::CheckInteger(State, 1);
  return Value > 0 && Value <= MAX_int32 ? static_cast<int32>(Value) : -1;
 }
 int PushResult(FSkillLabLuaState* State, USkillLabLuaSkillBehavior* Behavior, bool bAccepted)
 {
  SkillLabLua::NewTable(State);
  SkillLabLua::PushBoolean(State, bAccepted); SkillLabLua::SetField(State, -2, "accepted");
  if (!Behavior) return 1;
  const auto& Result = Behavior->GetEffectResult();
  SkillLabLua::PushInteger(State, Result.SubmissionPointId); SkillLabLua::SetField(State, -2, "pointId");
  const auto Submission = Result.SubmissionKey.Id.ToString(EGuidFormats::DigitsWithHyphens);
  SkillLabLua::PushString(State, TCHAR_TO_UTF8(*Submission)); SkillLabLua::SetField(State, -2, "submissionId");
  SkillLabLua::PushBoolean(State, bAccepted && Result.bEffectApplied); SkillLabLua::SetField(State, -2, "applied");
  SkillLabLua::PushBoolean(State, Behavior->IsActionPending()); SkillLabLua::SetField(State, -2, "pending");
  SkillLabLua::PushBoolean(State, Behavior->IsActionCompleted()); SkillLabLua::SetField(State, -2, "completed");
  SkillLabLua::PushString(State, TCHAR_TO_UTF8(*Result.Reason.ToString())); SkillLabLua::SetField(State, -2, "reason");
  SkillLabLua::PushNumber(State, Result.RequestedAmount); SkillLabLua::SetField(State, -2, "requestedAmount");
  SkillLabLua::PushNumber(State, Result.ActualAmount); SkillLabLua::SetField(State, -2, "actualAmount");
  SkillLabLua::PushNumber(State, Result.HealthBefore); SkillLabLua::SetField(State, -2, "healthBefore");
  SkillLabLua::PushNumber(State, Result.HealthAfter); SkillLabLua::SetField(State, -2, "healthAfter");
  return 1;
 }
 int LuaApplyEffect(FSkillLabLuaState* State)
 {
  const int32 Point = ReadPointId(State);
  auto* Behavior = GetLuaBehavior(State);
  return PushResult(State, Behavior, Behavior && Behavior->RequestDeclaredEffect(Point));
 }
 int LuaRequestAction(FSkillLabLuaState* State)
 {
  const int32 Point = ReadPointId(State);
  auto* Behavior = GetLuaBehavior(State);
  return PushResult(State, Behavior, Behavior && Behavior->RequestDeclaredAction(Point));
 }
 int LuaFinish(FSkillLabLuaState* State)
 {
  auto* Behavior = GetLuaBehavior(State);
  SkillLabLua::PushBoolean(State, Behavior && Behavior->RequestFinish());
  return 1;
 }
 int LuaDefinitionAlreadyLoaded(FSkillLabLuaState* State) { SkillLabLua::PushBoolean(State, true); return 1; }
}

void USkillLabLuaSkillBehavior::RunScript()
{
	if (!IsCurrent() || Definition.LuaScript.IsEmpty())
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	if (Definition.LuaSource.IsEmpty())
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	FSkillLabLuaState* Lua = SkillLabLua::Create();
	if (!Lua)
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	SkillLabLua::SetInstructionBudget(Lua, 100000);
	// 顶层声明已经在加载期形成快照，运行宿主仅允许原声明语句重放，不修改定义。
	SkillLabLua::PushCallback(Lua, &LuaDefinitionAlreadyLoaded, this);
	SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Define");
	SkillLabLua::PushCallback(Lua, &LuaDefinitionAlreadyLoaded, this);
	SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Spatial");
	FTCHARToUTF8 Utf8(*Definition.LuaSource);
	if (!SkillLabLua::LoadBuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*Definition.LuaScript))
		|| !SkillLabLua::Call(Lua, 0, 0))
	{
		SkillLabLua::Close(Lua);
		Fail(TEXT("ScriptError"));
		return;
	}
	SkillLabLua::GetGlobal(Lua, "OnSkillCast");
	if (!SkillLabLua::IsFunction(Lua, -1))
	{
		SkillLabLua::Close(Lua);
		Fail(TEXT("ScriptEntryMissing"));
		return;
	}
	SkillLabLua::NewTable(Lua);
	SkillLabLua::PushInteger(Lua, CastId); SkillLabLua::SetField(Lua, -2, "castId");
	SkillLabLua::PushInteger(Lua, Request.RequestId); SkillLabLua::SetField(Lua, -2, "requestId");
	FTCHARToUTF8 SkillUtf8(*Definition.SkillId.ToString());
	SkillLabLua::PushString(Lua, SkillUtf8.Get()); SkillLabLua::SetField(Lua, -2, "skillId");
	SkillLabLua::PushCallback(Lua, &LuaRequestAction, this);
	SkillLabLua::SetGlobal(Lua, "SkillLabSkill_RequestAction");
	SkillLabLua::PushCallback(Lua, &LuaApplyEffect, this);
	SkillLabLua::SetGlobal(Lua, "SkillLabSkill_ApplyEffect");
	SkillLabLua::PushCallback(Lua, &LuaFinish, this);
	SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Finish");
	if (!SkillLabLua::Call(Lua, 1, 0))
	{
		SkillLabLua::Close(Lua);
		UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Lua OnSkillCast failed for %s"), *Definition.SkillId.ToString());
		Fail(TEXT("ScriptError"));
		return;
	}
	const bool bCompleted = bFinished;
	SkillLabLua::Close(Lua);
	if (!bCompleted && IsCurrent() && !HasLegalWait())
	{
		Fail(!LastResponse.bEffectApplied && !LastResponse.Reason.IsNone()
			? LastResponse.Reason : TEXT("ScriptNoFinish"));
	}
}

