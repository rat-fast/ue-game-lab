#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillLabSkillTypes.h"
#include "Components/ActorComponent.h"
#include "SkillLabSkillAnimationComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class USkillLabSkillRuntimeComponent;
struct FSkillLabSkillResult;
struct FAnimNotifyEventReference;

DECLARE_MULTICAST_DELEGATE_FiveParams(FSkillLabAnimationNotifyObserved, const FSkillLabCastExecutionToken&, UAnimSequenceBase*, FGameplayTag, bool, bool);

USTRUCT()
struct FSkillLabCastAnimationPlayback
{
 GENERATED_BODY()
 UPROPERTY() FSkillLabCastExecutionToken Token;
 UPROPERTY() TObjectPtr<UAnimMontage> Montage;
 UPROPERTY() TObjectPtr<UAnimSequenceBase> NotifyAnimation;
 UPROPERTY() TWeakObjectPtr<UAnimInstance> AnimInstance;
 int32 InstanceId = INDEX_NONE;
};

/**
 * Adapts skill lifecycle events to the actor's animation layer.
 *
 * This component owns no skill rules. It resolves the montage already declared
 * by the skill definition, plays it on Started, and routes animation events
 * back to the runtime with the cast identity that started that montage.
 */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLABPRESENTATION_API USkillLabSkillAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabSkillAnimationComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Explicit bridge for animation systems that already carry CastId. */
	UFUNCTION(BlueprintCallable, Category="SkillLab|Animation")
	bool ForwardCastEvent(int32 CastId, FGameplayTag EventTag);

	/** Bridge used by SkillLabAnimNotify. Animation must be the active cast montage. */
	bool ForwardAnimNotify(UAnimSequenceBase* Animation, FGameplayTag EventTag,
		const FAnimNotifyEventReference& EventReference, bool bStateEnd = false, USkeletalMeshComponent* NotifyMesh = nullptr);

	UFUNCTION(BlueprintPure, Category="SkillLab|Animation")
	int32 GetAnimationCastId() const { return Playbacks.Num() == 1 ? Playbacks[0].Token.CastId : 0; }

	/** Narrow presentation command used by the Lua policy adapter. */
	bool PlayCastAction(int32 CastId, FName SkillId);
	void StopCastAction(int32 CastId);
	bool PlayCastAction(const FSkillLabCastExecutionToken& Token, FName SkillId);
	void StopCastAction(const FSkillLabCastExecutionToken& Token);
	void StopAllCastActions();
	/** 当前实际播放绑定的只读副本，不推进或更改动画。 */
	TArray<FSkillLabCastAnimationPlayback> GetPlaybackSnapshot() const { return Playbacks; }
	FSkillLabAnimationNotifyObserved OnNotifyObserved;

private:
	friend class FSkillLabPresentationSubmissionIdentityTest;
	bool PlayCastMontage(const FSkillLabSkillResult& Result);
	void StopCastMontage(const FSkillLabCastExecutionToken& Token);
	USkeletalMeshComponent* FindMesh() const;

	UPROPERTY(Transient) TObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	UPROPERTY(Transient) TArray<FSkillLabCastAnimationPlayback> Playbacks;
};
