#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabTargetingService.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

extern "C"
{
#include "lua.h"
#include "lauxlib.h"
}

namespace
{
	void LuaInstructionHook(lua_State* State, lua_Debug*)
	{
		luaL_error(State, "instruction budget exceeded");
	}

	USkillLabLuaSkillBehavior* GetLuaBehavior(lua_State* State)
	{
		return static_cast<USkillLabLuaSkillBehavior*>(lua_touserdata(State, lua_upvalueindex(1)));
	}

	int LuaApplyEffect(lua_State* State)
	{
		USkillLabLuaSkillBehavior* Behavior = GetLuaBehavior(State);
		const bool bApplied = Behavior && Behavior->RequestDeclaredEffect();
		const FSkillLabSkillResult* Result = Behavior ? &Behavior->GetEffectResult() : nullptr;
		lua_newtable(State);
		lua_pushboolean(State, bApplied); lua_setfield(State, -2, "applied");
		if (Result)
		{
			FTCHARToUTF8 ReasonUtf8(*Result->Reason.ToString());
			lua_pushstring(State, ReasonUtf8.Get()); lua_setfield(State, -2, "reason");
			lua_pushnumber(State, Result->RequestedAmount); lua_setfield(State, -2, "requestedAmount");
			lua_pushnumber(State, Result->ActualAmount); lua_setfield(State, -2, "actualAmount");
			lua_pushnumber(State, Result->HealthBefore); lua_setfield(State, -2, "healthBefore");
			lua_pushnumber(State, Result->HealthAfter); lua_setfield(State, -2, "healthAfter");
		}
		return 1;
	}

	int LuaRequestAction(lua_State* State)
	{
		USkillLabLuaSkillBehavior* Behavior = GetLuaBehavior(State);
		const bool bAccepted = Behavior && Behavior->RequestDeclaredAction();
		const FSkillLabSkillResult* Result = Behavior ? &Behavior->GetEffectResult() : nullptr;
		lua_newtable(State);
		lua_pushboolean(State, bAccepted); lua_setfield(State, -2, "accepted");
		if (Result)
		{
			lua_pushboolean(State, Result->bEffectApplied); lua_setfield(State, -2, "applied");
			lua_pushboolean(State, Behavior->IsActionPending()); lua_setfield(State, -2, "pending");
			lua_pushboolean(State, Behavior->IsActionCompleted()); lua_setfield(State, -2, "completed");
			FTCHARToUTF8 ReasonUtf8(*Result->Reason.ToString());
			lua_pushstring(State, ReasonUtf8.Get()); lua_setfield(State, -2, "reason");
			lua_pushnumber(State, Result->ActualAmount); lua_setfield(State, -2, "actualAmount");
		}
		return 1;
	}

	int LuaFinish(lua_State* State)
	{
		USkillLabLuaSkillBehavior* Behavior = GetLuaBehavior(State);
		lua_pushboolean(State, Behavior && Behavior->RequestFinish());
		return 1;
	}

	bool LoadScriptText(const FSkillLabSkillDefinition& Definition, FString& OutPath, FString& OutScript)
	{
		if (Definition.LuaScript.IsEmpty()) return false;
		OutPath = FPaths::Combine(FPaths::ProjectConfigDir(), Definition.LuaScript);
		return FFileHelper::LoadFileToString(OutScript, *OutPath);
	}
}

UWorld* USkillLabSkillBehavior::GetWorld() const { return Runtime ? Runtime->GetWorld() : nullptr; }
void USkillLabSkillBehavior::Initialize(USkillLabSkillRuntimeComponent* InRuntime,
	const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, const int32 InCastId,
	const uint64 InRuntimeGeneration)
{
	Runtime = InRuntime; Definition = InDefinition; Request = InRequest; CastId = InCastId; RuntimeGeneration = InRuntimeGeneration;
}
bool USkillLabSkillBehavior::IsCurrent() const { return Runtime && Runtime->IsBehaviorCurrent(this, CastId); }
FSkillLabSkillResult USkillLabSkillBehavior::ApplyEffect(AActor* Target)
{
	return IsCurrent() ? Runtime->ApplyBehaviorEffect(this, CastId, Target, Definition) : FSkillLabSkillResult();
}
void USkillLabSkillBehavior::ReportMiss(const FName Reason, AActor* Target)
{
	if (IsCurrent()) Runtime->ReportBehaviorMiss(this, CastId, Target, Reason);
}
void USkillLabSkillBehavior::Complete(const FName Reason)
{
	if (IsCurrent()) Runtime->CompleteBehavior(this, CastId, Reason);
}
void USkillLabSkillBehavior::Fail(const FName Reason)
{
	if (IsCurrent()) Runtime->CancelActiveCast(Reason);
}

void USkillLabLuaSkillBehavior::Execute()
{
	RunScript();
}

bool USkillLabLuaSkillBehavior::ValidateScript(const FSkillLabSkillDefinition& InDefinition, FName& OutReason)
{
	OutReason = NAME_None;
	FString ScriptPath;
	FString Script;
	if (!LoadScriptText(InDefinition, ScriptPath, Script))
	{
		OutReason = TEXT("ScriptUnavailable");
		return false;
	}
	lua_State* Lua = luaL_newstate();
	if (!Lua)
	{
		OutReason = TEXT("ScriptUnavailable");
		return false;
	}
	lua_sethook(Lua, &LuaInstructionHook, LUA_MASKCOUNT, 100000);
	FTCHARToUTF8 Utf8(*Script);
	const bool bLoaded = luaL_loadbuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*ScriptPath)) == LUA_OK;
	const bool bExecuted = bLoaded && lua_pcall(Lua, 0, 0, 0) == LUA_OK;
	const bool bHasEntry = bExecuted && (lua_getglobal(Lua, "OnSkillCast"), lua_isfunction(Lua, -1));
	if (!bLoaded || !bExecuted) OutReason = TEXT("ScriptError");
	else if (!bHasEntry) OutReason = TEXT("ScriptEntryMissing");
	lua_close(Lua);
	return OutReason.IsNone();
}

bool USkillLabLuaSkillBehavior::RequestDeclaredEffect()
{
	if (bEffectRequested || !IsCurrent() || Definition.Effect != ESkillLabEffectKind::Healing)
	{
		return false;
	}
	bEffectRequested = true;
	EffectResult = ApplyEffect(Runtime->GetOwner());
	return EffectResult.bEffectApplied;
}

void USkillLabLuaSkillBehavior::SetActionResult(const FSkillLabSkillResult& Result, const bool bResolved, const bool bPending)
{
	ActionResult = Result;
	EffectResult = Result;
	bActionResolved = bResolved;
	bActionPending = bPending;
	bActionCompleted = bResolved;
	bActionSucceeded = Result.bEffectApplied;
}

bool USkillLabLuaSkillBehavior::RequestDeclaredAction()
{
	if (bActionRequested || !IsCurrent() || !Runtime) return false;
	bActionRequested = true;
	if (Definition.Execution == ESkillLabExecutionTemplate::Projectile)
	{
		const FVector Start = GetSkillLabProjectileAimPoint(Runtime->GetOwner());
		Projectile = GetWorld() ? GetWorld()->SpawnActorDeferred<ASkillLabFireballProjectile>(
			ASkillLabFireballProjectile::StaticClass(), FTransform(FRotator::ZeroRotator, Start), Runtime->GetOwner(),
			Cast<APawn>(Runtime->GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
		if (!Projectile)
		{
			FSkillLabSkillResult Result = EffectResult;
			Result.Reason = TEXT("ProjectileSpawnFailed");
			SetActionResult(Result, true);
			return true;
		}
		Projectile->OnResolved.BindUObject(this, &USkillLabLuaSkillBehavior::OnProjectileResolved);
		Projectile->Initialize(Runtime->GetOwner(), Request.Target, Definition, Request, CastId, Definition.ProjectileParameters);
		FSkillLabSkillResult Result = EffectResult;
		Result.Reason = TEXT("ProjectilePending");
		SetActionResult(Result, false, true);
		Projectile->FinishSpawning(FTransform(FRotator::ZeroRotator, Start));
		return true;
	}
	if (Definition.Execution == ESkillLabExecutionTemplate::GroundArea)
	{
		TArray<AActor*> Targets;
		FSkillLabTargetingService::FindAreaTargets(*Runtime, Request.TargetLocation, Definition, Targets);
		FSkillLabSkillResult Result = EffectResult;
		bool bAnyApplied = false;
		for (AActor* Target : Targets)
		{
			const FSkillLabSkillResult Applied = ApplyEffect(Target);
			bAnyApplied |= Applied.bEffectApplied;
			if (!Applied.bEffectApplied && Result.Reason.IsNone()) Result.Reason = Applied.Reason;
		}
		Result.bEffectApplied = bAnyApplied;
		Result.Reason = Targets.Num() > 0 ? (bAnyApplied ? TEXT("AreaHit") : Result.Reason) : TEXT("AreaEmpty");
		if (Targets.Num() == 0) ReportMiss(TEXT("AreaEmpty"));
		SetActionResult(Result, true);
		return true;
	}
	const FSkillLabSkillResult Applied = ApplyEffect(Request.Target);
	SetActionResult(Applied, true);
	return true;
}

void USkillLabLuaSkillBehavior::OnProjectileResolved(AActor* Target, const FName Reason)
{
	if (!IsCurrent() || bActionResolved) return;
	if (Reason == TEXT("Hit") || Reason == TEXT("Blocked"))
	{
		if (Target)
		{
			const FSkillLabSkillResult Applied = ApplyEffect(Target);
			SetActionResult(Applied, true);
			Complete(Reason);
		}
		else
		{
			FSkillLabSkillResult Result = EffectResult;
			Result.Reason = Reason == TEXT("Blocked") ? TEXT("ProjectileBlocked") : Reason;
			SetActionResult(Result, true);
			ReportMiss(Result.Reason);
			Complete(Result.Reason);
		}
	}
	else
	{
		FSkillLabSkillResult Result = EffectResult;
		Result.Reason = Reason;
		SetActionResult(Result, true);
		ReportMiss(Reason);
		Complete(Reason);
	}
}

bool USkillLabLuaSkillBehavior::RequestFinish()
{
	if (bFinished || !bActionRequested || !bActionResolved || bActionPending || !IsCurrent()) return false;
	bFinished = true;
	const FName FinishReason = ActionResult.Reason == TEXT("AreaEmpty") ? TEXT("AreaEmpty")
		: (Definition.Effect == ESkillLabEffectKind::Healing ? TEXT("HealingApplied")
		: (Definition.Effect == ESkillLabEffectKind::DamageOverTime ? TEXT("BurningStarted") : TEXT("EffectApplied")));
	Complete(FinishReason);
	return true;
}

void USkillLabLuaSkillBehavior::RunScript()
{
	if (!IsCurrent() || Definition.LuaScript.IsEmpty())
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	FString ScriptPath;
	FString Script;
	if (!LoadScriptText(Definition, ScriptPath, Script))
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	lua_State* Lua = luaL_newstate();
	if (!Lua)
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	lua_sethook(Lua, &LuaInstructionHook, LUA_MASKCOUNT, 100000);
	FTCHARToUTF8 Utf8(*Script);
	if (luaL_loadbuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*ScriptPath)) != LUA_OK
		|| lua_pcall(Lua, 0, 0, 0) != LUA_OK)
	{
		lua_close(Lua);
		Fail(TEXT("ScriptError"));
		return;
	}
	lua_getglobal(Lua, "OnSkillCast");
	if (!lua_isfunction(Lua, -1))
	{
		lua_close(Lua);
		Fail(TEXT("ScriptEntryMissing"));
		return;
	}
	lua_newtable(Lua);
	lua_pushinteger(Lua, CastId); lua_setfield(Lua, -2, "castId");
	lua_pushinteger(Lua, Request.RequestId); lua_setfield(Lua, -2, "requestId");
	FTCHARToUTF8 SkillUtf8(*Definition.SkillId.ToString());
	lua_pushstring(Lua, SkillUtf8.Get()); lua_setfield(Lua, -2, "skillId");
	lua_pushlightuserdata(Lua, this);
	lua_pushcclosure(Lua, &LuaRequestAction, 1);
	lua_setglobal(Lua, "SkillLabSkill_RequestAction");
	lua_pushlightuserdata(Lua, this);
	lua_pushcclosure(Lua, &LuaFinish, 1);
	lua_setglobal(Lua, "SkillLabSkill_Finish");
	if (lua_pcall(Lua, 1, 0, 0) != LUA_OK)
	{
		lua_close(Lua);
		Fail(TEXT("ScriptError"));
		return;
	}
	const bool bCompleted = bFinished;
	lua_close(Lua);
	if (!bCompleted && IsCurrent() && !bActionPending)
	{
		Fail(bEffectRequested && !EffectResult.bEffectApplied && !EffectResult.Reason.IsNone()
			? EffectResult.Reason : TEXT("ScriptNoFinish"));
	}
}

void USkillLabLuaSkillBehavior::Cleanup()
{
	if (Projectile)
	{
		Projectile->OnResolved.Unbind();
		if (!Projectile->IsActorBeingDestroyed()) Projectile->Destroy();
	}
	Projectile = nullptr;
}
