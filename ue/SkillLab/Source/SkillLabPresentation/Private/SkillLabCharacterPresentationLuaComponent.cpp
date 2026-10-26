#include "SkillLabCharacterPresentationLuaComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SkillLabSkillAnimationComponent.h"
#include "SkillLabSkillPresentationRouter.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabCharacterAnimInstance.h"
#include "UObject/UnrealType.h"

#include "SkillLabLua.h"

namespace
{
	const TCHAR* ToString(const ESkillLabCharacterAnimationState State)
	{
		switch (State)
		{
		case ESkillLabCharacterAnimationState::Idle: return TEXT("Idle");
		case ESkillLabCharacterAnimationState::Move: return TEXT("Move");
		case ESkillLabCharacterAnimationState::Jump: return TEXT("Jump");
		case ESkillLabCharacterAnimationState::Fall: return TEXT("Fall");
		case ESkillLabCharacterAnimationState::Land: return TEXT("Land");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* ToString(const ESkillLabSkillEvent Event)
	{
		switch (Event)
		{
		case ESkillLabSkillEvent::Rejected: return TEXT("Rejected");
		case ESkillLabSkillEvent::Started: return TEXT("Started");
		case ESkillLabSkillEvent::TargetSelected: return TEXT("TargetSelected");
		case ESkillLabSkillEvent::PhaseChanged: return TEXT("PhaseChanged");
		case ESkillLabSkillEvent::Hit: return TEXT("Hit");
		case ESkillLabSkillEvent::DamageTick: return TEXT("DamageTick");
		case ESkillLabSkillEvent::EffectApplied: return TEXT("EffectApplied");
		case ESkillLabSkillEvent::BuffApplied: return TEXT("BuffApplied");
		case ESkillLabSkillEvent::BuffRemoved: return TEXT("BuffRemoved");
		case ESkillLabSkillEvent::Cancelled: return TEXT("Cancelled");
		case ESkillLabSkillEvent::Completed: return TEXT("Completed");
		default: return TEXT("Unknown");
		}
	}

	const TCHAR* ToString(const ESkillLabCastStage Stage)
	{
		switch (Stage)
		{
		case ESkillLabCastStage::Idle: return TEXT("Idle");
		case ESkillLabCastStage::Windup: return TEXT("Windup");
		case ESkillLabCastStage::Active: return TEXT("Active");
		case ESkillLabCastStage::Recovery: return TEXT("Recovery");
		case ESkillLabCastStage::Completed: return TEXT("Completed");
		case ESkillLabCastStage::Cancelled: return TEXT("Cancelled");
		default: return TEXT("Unknown");
		}
	}
}

USkillLabCharacterPresentationLuaComponent::USkillLabCharacterPresentationLuaComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabCharacterPresentationLuaComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	SkillRuntime = Owner ? Owner->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	SkillAnimation = Owner ? Owner->FindComponentByClass<USkillLabSkillAnimationComponent>() : nullptr;
	AnimationState = Owner ? Owner->FindComponentByClass<USkillLabCharacterAnimationStateComponent>() : nullptr;
	SkillPresentationRouter = NewObject<USkillLabSkillPresentationRouter>(this);
	if (SkillPresentationRouter && SkillRuntime)
	{
		SkillPresentationRouter->Bind(SkillRuntime);
		SkillPresentationRouter->OnPresentationEvent.AddUObject(this,
			&USkillLabCharacterPresentationLuaComponent::HandleSkillEvent);
	}

	const bool bScriptReady = LoadScript();
	if (AnimationState)
	{
		AnimationState->OnStateChanged.AddUObject(this,
			&USkillLabCharacterPresentationLuaComponent::HandleBaseStateChanged);
		HandleBaseStateChanged(AnimationState->GetAnimationState(), AnimationState->GetTransitionSerial());
	}
	UE_LOG(LogTemp, Log, TEXT("[SkillLabLuaPresentation] %s runtime=%s; Lua owns presentation event policy; UE owns animation playback"),
		*GetNameSafe(Owner), bScriptReady ? TEXT("ready") : TEXT("unavailable, using native fallback"));
}

void USkillLabCharacterPresentationLuaComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SkillPresentationRouter)
	{
		SkillPresentationRouter->OnPresentationEvent.RemoveAll(this);
		SkillPresentationRouter->Bind(nullptr);
	}
	if (AnimationState)
	{
		AnimationState->OnStateChanged.RemoveAll(this);
	}
	if (SkillAnimation)
	{
		SkillAnimation->StopCastAction(SkillAnimation->GetAnimationCastId());
	}
	if (Lua)
	{
		SkillLabLua::Close(Lua);
		Lua = nullptr;
		UE_LOG(LogTemp, Log, TEXT("[SkillLabLuaPresentation] %s runtime closed (EndPlay)"), *GetNameSafe(GetOwner()));
	}
	Super::EndPlay(EndPlayReason);
}

bool USkillLabCharacterPresentationLuaComponent::LoadScript()
{
	const FString ScriptPath = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("CharacterPresentation.lua"));
	FString Script;
	if (!FFileHelper::LoadFileToString(Script, *ScriptPath))
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillLabLuaPresentation] Cannot read script: %s"), *ScriptPath);
		return false;
	}

	Lua = SkillLabLua::Create();
	if (!Lua)
	{
		UE_LOG(LogTemp, Error, TEXT("[SkillLabLuaPresentation] Lua state allocation failed"));
		return false;
	}
	SkillLabLua::OpenLibraries(Lua);
	RegisterBridgeFunctions();
	FTCHARToUTF8 Utf8(*Script);
	if (!SkillLabLua::LoadBuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*ScriptPath))
		|| !SkillLabLua::Call(Lua, 0, 0))
	{
		LogLuaError(TEXT("load"));
		SkillLabLua::Close(Lua);
		Lua = nullptr;
		return false;
	}
	return true;
}

void USkillLabCharacterPresentationLuaComponent::RegisterBridgeFunctions()
{
	SkillLabLua::NewTable(Lua);
	SkillLabLua::PushCallback(Lua, &LuaPlaySkillAction, this);
	SkillLabLua::SetField(Lua, -2, "PlaySkillAction");
	SkillLabLua::PushCallback(Lua, &LuaStopSkillAction, this);
	SkillLabLua::SetField(Lua, -2, "StopSkillAction");
	SkillLabLua::PushCallback(Lua, &LuaSetLocomotionIntent, this);
	SkillLabLua::SetField(Lua, -2, "SetLocomotionIntent");
	SkillLabLua::PushCallback(Lua, &LuaLog, this);
	SkillLabLua::SetField(Lua, -2, "Log");
	SkillLabLua::SetGlobal(Lua, "SkillLabPresentation");
}

int32 USkillLabCharacterPresentationLuaComponent::LuaPlaySkillAction(FSkillLabLuaState* State)
{
	auto* Self = static_cast<USkillLabCharacterPresentationLuaComponent*>(SkillLabLua::GetCallbackContext(State));
	const int32 CastId = static_cast<int32>(SkillLabLua::CheckInteger(State, 1));
	const FName SkillId(UTF8_TO_TCHAR(SkillLabLua::CheckString(State, 2)));
	const bool bPlayed = Self && Self->SkillAnimation && Self->SkillAnimation->PlayCastAction(CastId, SkillId);
	SkillLabLua::PushBoolean(State, bPlayed);
	return 1;
}

int32 USkillLabCharacterPresentationLuaComponent::LuaStopSkillAction(FSkillLabLuaState* State)
{
	auto* Self = static_cast<USkillLabCharacterPresentationLuaComponent*>(SkillLabLua::GetCallbackContext(State));
	const int32 CastId = static_cast<int32>(SkillLabLua::CheckInteger(State, 1));
	if (Self && Self->SkillAnimation)
	{
		Self->SkillAnimation->StopCastAction(CastId);
	}
	return 0;
}

int32 USkillLabCharacterPresentationLuaComponent::LuaSetLocomotionIntent(FSkillLabLuaState* State)
{
	auto* Self = static_cast<USkillLabCharacterPresentationLuaComponent*>(SkillLabLua::GetCallbackContext(State));
	const bool bShouldMove = SkillLabLua::ToBoolean(State, 1) != 0;
	const bool bIsFalling = SkillLabLua::ToBoolean(State, 2) != 0;
	if (Self)
	{
		TArray<USkeletalMeshComponent*> Meshes;
		if (AActor* Owner = Self->GetOwner())
		{
			Owner->GetComponents<USkeletalMeshComponent>(Meshes);
		}
		USkeletalMeshComponent* Mesh = Meshes.Num() > 0 ? Meshes[0] : nullptr;
		Self->ApplyNativeLocomotionFallback(bShouldMove, bIsFalling);
	}
	return 0;
}

int32 USkillLabCharacterPresentationLuaComponent::LuaLog(FSkillLabLuaState* State)
{
	auto* Self = static_cast<USkillLabCharacterPresentationLuaComponent*>(SkillLabLua::GetCallbackContext(State));
	const char* Message = SkillLabLua::CheckString(State, 1);
	if (Self)
	{
		UE_LOG(LogTemp, Log, TEXT("[SkillLabLuaPresentation] %s"), UTF8_TO_TCHAR(Message));
	}
	return 0;
}

void USkillLabCharacterPresentationLuaComponent::HandleSkillEvent(const FSkillLabSkillResult& Result)
{
	const bool bHandled = CallSkillCallback(Result);
	if (!bHandled || !Lua)
	{
		// Preserve reliable basic behavior when a script callback is missing or fails.
		if (Result.Event == ESkillLabSkillEvent::Started && SkillAnimation)
		{
			SkillAnimation->PlayCastAction(Result.CastId, Result.SkillId);
		}
		else if ((Result.Event == ESkillLabSkillEvent::Cancelled || Result.Event == ESkillLabSkillEvent::Completed) && SkillAnimation)
		{
			SkillAnimation->StopCastAction(Result.CastId);
		}
	}
}

void USkillLabCharacterPresentationLuaComponent::HandleBaseStateChanged(
	const ESkillLabCharacterAnimationState State, const int32 TransitionSerial)
{
	const FSkillLabCharacterAnimationSnapshot Snapshot = AnimationState
		? AnimationState->GetAnimationSnapshot() : FSkillLabCharacterAnimationSnapshot();
	if (!CallStateCallback(State, TransitionSerial, Snapshot.bShouldMove, Snapshot.bIsFalling))
	{
		ApplyNativeLocomotionFallback(Snapshot.bShouldMove, Snapshot.bIsFalling);
	}
}

bool USkillLabCharacterPresentationLuaComponent::CallSkillCallback(const FSkillLabSkillResult& Result)
{
	if (!Lua)
	{
		return false;
	}
	SkillLabLua::GetGlobal(Lua, "OnSkillEvent");
	if (!SkillLabLua::IsFunction(Lua, -1))
	{
		SkillLabLua::Pop(Lua, 1);
		return false;
	}
	SkillLabLua::NewTable(Lua);
	auto SetString = [this](const char* Key, const TCHAR* Value)
	{
		FTCHARToUTF8 Utf8(Value);
		SkillLabLua::PushString(Lua, Utf8.Get(), Utf8.Length());
		SkillLabLua::SetField(Lua, -2, Key);
	};
	SkillLabLua::PushInteger(Lua, Result.CastId); SkillLabLua::SetField(Lua, -2, "castId");
	SkillLabLua::PushInteger(Lua, Result.RequestId); SkillLabLua::SetField(Lua, -2, "requestId");
	SetString("skillId", *Result.SkillId.ToString());
	SetString("event", ToString(Result.Event));
	SetString("stage", ToString(Result.Stage));
	SkillLabLua::PushBoolean(Lua, Result.bSuccess); SkillLabLua::SetField(Lua, -2, "success");
	SkillLabLua::PushNumber(Lua, Result.ActualAmount); SkillLabLua::SetField(Lua, -2, "actualAmount");
	SetString("reason", *Result.Reason.ToString());
	if (!SkillLabLua::Call(Lua, 1, 0))
	{
		LogLuaError(TEXT("OnSkillEvent"));
		return false;
	}
	return true;
}

bool USkillLabCharacterPresentationLuaComponent::CallStateCallback(
	const ESkillLabCharacterAnimationState State, const int32 TransitionSerial,
	const bool bShouldMove, const bool bIsFalling)
{
	if (!Lua)
	{
		return false;
	}
	SkillLabLua::GetGlobal(Lua, "OnBaseStateChanged");
	if (!SkillLabLua::IsFunction(Lua, -1))
	{
		SkillLabLua::Pop(Lua, 1);
		return false;
	}
	FTCHARToUTF8 StateUtf8(ToString(State));
	SkillLabLua::PushString(Lua, StateUtf8.Get(), StateUtf8.Length());
	SkillLabLua::PushInteger(Lua, TransitionSerial);
	SkillLabLua::PushBoolean(Lua, bShouldMove);
	SkillLabLua::PushBoolean(Lua, bIsFalling);
	if (!SkillLabLua::Call(Lua, 4, 0))
	{
		LogLuaError(TEXT("OnBaseStateChanged"));
		return false;
	}
	return true;
}

void USkillLabCharacterPresentationLuaComponent::ApplyNativeLocomotionFallback(
	const bool bShouldMove, const bool bIsFalling)
{
	TArray<USkeletalMeshComponent*> Meshes;
	if (AActor* Owner = GetOwner())
	{
		Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	}
	USkeletalMeshComponent* Mesh = Meshes.Num() > 0 ? Meshes[0] : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabLuaPresentation] Cannot apply locomotion intent: AnimInstance is unavailable"));
		return;
	}
	if (USkillLabCharacterAnimInstance* SkillLabAnim = Cast<USkillLabCharacterAnimInstance>(AnimInstance))
	{
		SkillLabAnim->SetLuaLocomotionIntent(bShouldMove, bIsFalling);
		return;
	}
	int32 AppliedCount = 0;
	if (FBoolProperty* Property = CastField<FBoolProperty>(AnimInstance->GetClass()->FindPropertyByName(TEXT("ShouldMove"))))
	{
		Property->SetPropertyValue_InContainer(AnimInstance, bShouldMove);
		++AppliedCount;
	}
	if (FBoolProperty* Property = CastField<FBoolProperty>(AnimInstance->GetClass()->FindPropertyByName(TEXT("IsFalling"))))
	{
		Property->SetPropertyValue_InContainer(AnimInstance, bIsFalling);
		++AppliedCount;
	}
	if (AppliedCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabLuaPresentation] AnimInstance %s has no legacy ShouldMove/IsFalling inputs"),
			*GetNameSafe(AnimInstance));
	}
}

void USkillLabCharacterPresentationLuaComponent::LogLuaError(const TCHAR* Callback)
{
	const char* Error = SkillLabLua::ToString(Lua, -1);
	UE_LOG(LogTemp, Error, TEXT("[SkillLabLuaPresentation] %s failed: %s"), Callback,
		UTF8_TO_TCHAR(Error ? Error : "unknown Lua error"));
	SkillLabLua::Pop(Lua, 1);
}
