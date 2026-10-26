#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabCharacterAnimationStateComponent.h"
#include "SkillLabCharacterPresentationLuaComponent.generated.h"

struct FSkillLabSkillResult;
struct FSkillLabLuaState;
class USkillLabSkillRuntimeComponent;
class USkillLabSkillAnimationComponent;
class USkillLabSkillPresentationRouter;

/** Runs project-authored Lua presentation policy and forwards narrow commands to UE. */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLABPRESENTATION_API USkillLabCharacterPresentationLuaComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabCharacterPresentationLuaComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	static int32 LuaPlaySkillAction(FSkillLabLuaState* State);
	static int32 LuaStopSkillAction(FSkillLabLuaState* State);
	static int32 LuaSetLocomotionIntent(FSkillLabLuaState* State);
	static int32 LuaLog(FSkillLabLuaState* State);
	void HandleSkillEvent(const FSkillLabSkillResult& Result);
	void HandleBaseStateChanged(ESkillLabCharacterAnimationState State, int32 TransitionSerial);
	bool LoadScript();
	bool CallSkillCallback(const FSkillLabSkillResult& Result);
	bool CallStateCallback(ESkillLabCharacterAnimationState State, int32 TransitionSerial,
		bool bShouldMove, bool bIsFalling);
	void ApplyNativeLocomotionFallback(bool bShouldMove, bool bIsFalling);
	void RegisterBridgeFunctions();
	void LogLuaError(const TCHAR* Callback);

	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillAnimationComponent> SkillAnimation;
	UPROPERTY(Transient)
	TObjectPtr<USkillLabCharacterAnimationStateComponent> AnimationState;
	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillPresentationRouter> SkillPresentationRouter;
	FSkillLabLuaState* Lua = nullptr;
};
