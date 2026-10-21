#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "SkillLabSkillAnimationComponent.generated.h"

class UAnimInstance;
class UAnimMontage;
class UAnimSequenceBase;
class USkillLabSkillRuntimeComponent;
struct FSkillLabSkillResult;
struct FAnimNotifyEventReference;

/**
 * Adapts skill lifecycle events to the actor's animation layer.
 *
 * This component owns no skill rules. It resolves the montage already declared
 * by the skill definition, plays it on Started, and routes animation events
 * back to the runtime with the cast identity that started that montage.
 */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLABSKILLS_API USkillLabSkillAnimationComponent : public UActorComponent
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
		const FAnimNotifyEventReference& EventReference);

	UFUNCTION(BlueprintPure, Category="SkillLab|Animation")
	int32 GetAnimationCastId() const { return ActiveCastId; }

private:
	void HandleSkillEvent(const FSkillLabSkillResult& Result);
	bool PlayCastMontage(const FSkillLabSkillResult& Result);
	void StopCastMontage(int32 CastId);
	USkeletalMeshComponent* FindMesh() const;

	UPROPERTY(Transient) TObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	UPROPERTY(Transient) TObjectPtr<UAnimMontage> ActiveMontage;
	/** Animation asset reported by notify callbacks. Dynamic montages report their source sequence. */
	UPROPERTY(Transient) TObjectPtr<UAnimSequenceBase> ActiveNotifyAnimation;
	UPROPERTY(Transient) TObjectPtr<UAnimInstance> ActiveAnimInstance;
	int32 ActiveCastId = 0;
	int32 ActiveMontageInstanceId = INDEX_NONE;
};
