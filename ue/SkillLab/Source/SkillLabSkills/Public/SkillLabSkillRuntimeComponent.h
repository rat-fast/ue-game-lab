#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabCastLifecycle.h"
#include "SkillLabSkillRuntimeComponent.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabSkillGameplayAbility;
class USkillLabSkillPresentationRouter;
class USkillLabSkillBehavior;
class USkillLabSkillEffectEventAdapter;

/** Shared orchestration. No concrete skill identity or delivery implementation belongs here. */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLABSKILLS_API USkillLabSkillRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USkillLabSkillRuntimeComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillLab|Unit") int32 TeamId = 1;
	UPROPERTY(BlueprintAssignable, Category="SkillLab|Skill") FSkillLabSkillEventDelegate OnSkillEvent;
	FSkillLabNativeSkillEvent OnNativeSkillEvent;

	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void InitializeSkills();
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult SubmitSkillRequest(const FSkillLabSkillRequest& Request);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult CastSkill(FName SkillId, AActor* Target, FVector TargetLocation);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void CancelActiveCast(FName Reason);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void ResetSkills();
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") int32 GetUnitId() const { return UnitId; }
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") bool IsInitialized() const { return bInitialized; }
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") bool IsAlive() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") bool IsEnemy(const USkillLabSkillRuntimeComponent* Other) const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetCooldownRemaining(FName SkillId) const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetEnergyValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetMaxEnergyValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetHealthValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetMaxHealthValue() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") ESkillLabCastStage GetCastStage() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") float GetStageRemaining() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") FName GetActiveSkillId() const;
	UFUNCTION(BlueprintPure, Category="SkillLab|Skill") int32 GetActiveCastId() const { return ActiveCastId; }
	void GetGrantedSkillIds(TArray<FName>& OutIds) const;

	void NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability);
	void NotifyAbilityEnded(USkillLabSkillGameplayAbility* Ability);
	void ReceiveCastEvent(int32 CastId, FGameplayTag EventTag);
	bool IsBehaviorCurrent(const USkillLabSkillBehavior* Behavior, int32 CastId) const;
	FSkillLabSkillResult ApplyBehaviorEffect(const USkillLabSkillBehavior* Behavior, int32 CastId,
		AActor* Target, const FSkillLabSkillDefinition& Definition);
	void ReportBehaviorMiss(const USkillLabSkillBehavior* Behavior, int32 CastId, AActor* Target, FName Reason);
	void CompleteBehavior(const USkillLabSkillBehavior* Behavior, int32 CastId, FName Reason = NAME_None);

private:
	UPROPERTY(VisibleAnywhere) int32 UnitId = 0;
	UPROPERTY(VisibleAnywhere) bool bInitialized = false;
	UPROPERTY(Transient) TObjectPtr<USkillLabAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillGameplayAbility> ActiveAbility;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillBehavior> ActiveBehavior;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillPresentationRouter> PresentationRouter;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillEffectEventAdapter> EffectEventAdapter;
	UPROPERTY(Transient) FSkillLabSkillRequest ActiveRequest;
	UPROPERTY(Transient) FSkillLabSkillDefinition ActiveDefinition;
	TMap<FName, FGameplayAbilitySpecHandle> GrantedSkills;
	TMap<FName, double> CooldownEnd;
	TMap<FGameplayTag, FDelegateHandle> InterruptHandles;
	FTimerHandle PhaseTimer;
	FTimerHandle ActiveTimeoutTimer;
	FSkillLabCastLifecycle Lifecycle;
	int32 NextRequestId = 1;
	int32 NextCastId = 1;
	int32 ActiveCastId = 0;
	double StageEndTime = 0.0;
	bool bCommitted = false;
	bool bFinishing = false;
	bool bShuttingDown = false;
	FName OutcomeReason;

	FSkillLabSkillResult Reject(const FSkillLabSkillRequest& Request, FName Reason);
	FSkillLabSkillResult MakeResult() const;
	void Emit(ESkillLabSkillEvent Event, const FSkillLabSkillResult& Result, FName Reason = NAME_None);
	void ReleaseCast(FSkillLabCastPhaseToken Token);
	void BeginRecovery();
	void FinishRecovery(FSkillLabCastPhaseToken Token);
	void FinishCast(bool bCancelled, FName Reason);
	void ListenForInterrupts();
	void ClearInterruptListeners();
	void OnBlockingTagChanged(FGameplayTag Tag, int32 Count);
	bool CheckGates(FName& OutReason) const;
	void ForwardEffectEvent(const FSkillLabSkillResult& Result);
};
