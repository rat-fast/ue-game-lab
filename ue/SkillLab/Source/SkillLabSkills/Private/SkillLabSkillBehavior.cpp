#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabTargetingService.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "SkillLabLua.h"

namespace
{
	USkillLabLuaSkillBehavior* GetLuaBehavior(FSkillLabLuaState* State)
	{
		return static_cast<USkillLabLuaSkillBehavior*>(SkillLabLua::GetCallbackContext(State));
	}

	int LuaApplyEffect(FSkillLabLuaState* State)
	{
		USkillLabLuaSkillBehavior* Behavior = GetLuaBehavior(State);
		const bool bApplied = Behavior && Behavior->RequestDeclaredEffect();
		const FSkillLabSkillResult* Result = Behavior ? &Behavior->GetEffectResult() : nullptr;
		SkillLabLua::NewTable(State);
		SkillLabLua::PushBoolean(State, bApplied); SkillLabLua::SetField(State, -2, "applied");
		if (Result)
		{
			FTCHARToUTF8 ReasonUtf8(*Result->Reason.ToString());
			SkillLabLua::PushString(State, ReasonUtf8.Get()); SkillLabLua::SetField(State, -2, "reason");
			SkillLabLua::PushNumber(State, Result->RequestedAmount); SkillLabLua::SetField(State, -2, "requestedAmount");
			SkillLabLua::PushNumber(State, Result->ActualAmount); SkillLabLua::SetField(State, -2, "actualAmount");
			SkillLabLua::PushNumber(State, Result->HealthBefore); SkillLabLua::SetField(State, -2, "healthBefore");
			SkillLabLua::PushNumber(State, Result->HealthAfter); SkillLabLua::SetField(State, -2, "healthAfter");
		}
		return 1;
	}

	int LuaRequestAction(FSkillLabLuaState* State)
	{
		USkillLabLuaSkillBehavior* Behavior = GetLuaBehavior(State);
		const bool bAccepted = Behavior && Behavior->RequestDeclaredAction();
		const FSkillLabSkillResult* Result = Behavior ? &Behavior->GetEffectResult() : nullptr;
		SkillLabLua::NewTable(State);
		SkillLabLua::PushBoolean(State, bAccepted); SkillLabLua::SetField(State, -2, "accepted");
		if (Result)
		{
			SkillLabLua::PushBoolean(State, Result->bEffectApplied); SkillLabLua::SetField(State, -2, "applied");
			SkillLabLua::PushBoolean(State, Behavior->IsActionPending()); SkillLabLua::SetField(State, -2, "pending");
			SkillLabLua::PushBoolean(State, Behavior->IsActionCompleted()); SkillLabLua::SetField(State, -2, "completed");
			FTCHARToUTF8 ReasonUtf8(*Result->Reason.ToString());
			SkillLabLua::PushString(State, ReasonUtf8.Get()); SkillLabLua::SetField(State, -2, "reason");
			SkillLabLua::PushNumber(State, Result->ActualAmount); SkillLabLua::SetField(State, -2, "actualAmount");
		}
		return 1;
	}

	int LuaFinish(FSkillLabLuaState* State)
	{
		USkillLabLuaSkillBehavior* Behavior = GetLuaBehavior(State);
		SkillLabLua::PushBoolean(State, Behavior && Behavior->RequestFinish());
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
	FSkillLabLuaState* Lua = SkillLabLua::Create();
	if (!Lua)
	{
		OutReason = TEXT("ScriptUnavailable");
		return false;
	}
	SkillLabLua::SetInstructionBudget(Lua, 100000);
	FTCHARToUTF8 Utf8(*Script);
	const bool bLoaded = SkillLabLua::LoadBuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*ScriptPath));
	const bool bExecuted = bLoaded && SkillLabLua::Call(Lua, 0, 0);
	const bool bHasEntry = bExecuted && (SkillLabLua::GetGlobal(Lua, "OnSkillCast"), SkillLabLua::IsFunction(Lua, -1));
	if (!bLoaded || !bExecuted) OutReason = TEXT("ScriptError");
	else if (!bHasEntry) OutReason = TEXT("ScriptEntryMissing");
	SkillLabLua::Close(Lua);
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
	FSkillLabLuaState* Lua = SkillLabLua::Create();
	if (!Lua)
	{
		Fail(TEXT("ScriptUnavailable"));
		return;
	}
	SkillLabLua::SetInstructionBudget(Lua, 100000);
	FTCHARToUTF8 Utf8(*Script);
	if (!SkillLabLua::LoadBuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*ScriptPath))
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
	SkillLabLua::PushCallback(Lua, &LuaFinish, this);
	SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Finish");
	if (!SkillLabLua::Call(Lua, 1, 0))
	{
		SkillLabLua::Close(Lua);
		Fail(TEXT("ScriptError"));
		return;
	}
	const bool bCompleted = bFinished;
	SkillLabLua::Close(Lua);
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
