#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabTestCommandsComponent.generated.h"

class APawn;
class USkillLabSkillShortcutComponent;
class USkillLabSkillRuntimeComponent;
struct FSkillLabSkillResult;

/** Optional demo commands. Disabling this component disables setup and all test commands. */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabTestCommandsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabTestCommandsComponent();
	void InitializeForPawn(APawn* Pawn, USkillLabSkillShortcutComponent* InShortcuts);
	void RequestSlot(int32 SlotIndex);
	void SkillTest();
	void SkillTestRecover();
	void SkillArea();
	void SkillBurn();
	void SpawnTargets();
	void ResetSkills();
	void ReloadDefinitions();
	void AnimationStateSmoke();
	void SkillPresentationSmoke();

	UPROPERTY(EditAnywhere, Category="SkillLab|Testing") bool bEnableTestCommands = true;
	UPROPERTY(EditAnywhere, Category="SkillLab|Testing") bool bInitializeTrainingArena = true;

private:
	APawn* GetControlledPawn() const;
	void ApplyAnimationSmokeMovement();
	void TriggerAnimationSmokeJump();
	void StopAnimationSmokeMovement();
	void CaptureAnimationSmokeState();
	void StopAnimationSmokeCapture();
	void RunSkillPresentationSmokeStep();
	void HandleSkillPresentationSmokeEvent(const FSkillLabSkillResult& Result);
	TWeakObjectPtr<APawn> ControlledPawn;
	TWeakObjectPtr<USkillLabSkillShortcutComponent> ShortcutComponent;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	FTimerHandle AnimationSmokeMoveTimer;
	FTimerHandle AnimationSmokeJumpTimer;
	FTimerHandle AnimationSmokeStopTimer;
	FTimerHandle AnimationSmokeCaptureTimer;
	FTimerHandle AnimationSmokeCaptureEndTimer;
	FTimerHandle SkillPresentationSmokeTimer;
	TWeakObjectPtr<AActor> SkillPresentationSmokeTarget;
	int32 SkillPresentationSmokeStep = 0;
	int32 LastCapturedAnimationState = INDEX_NONE;
	int32 AnimationSmokeCaptureIndex = 0;
};
