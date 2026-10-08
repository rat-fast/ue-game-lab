#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillRuntimeComponent.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabSkillGameplayAbility;
class USkillLabSkillPresentationRouter;

UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLABSKILLS_API USkillLabSkillRuntimeComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	USkillLabSkillRuntimeComponent();
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SkillLab|Unit") int32 TeamId = 1;
	UPROPERTY(BlueprintAssignable, Category="SkillLab|Skill") FSkillLabSkillEventDelegate OnSkillEvent;
	FSkillLabNativeSkillEvent OnNativeSkillEvent;

	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") void InitializeSkills();
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult SubmitSkillRequest(const FSkillLabSkillRequest& Request);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Skill") FSkillLabSkillResult CastSkill(FName SkillId, AActor* Target, FVector TargetLocation);
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
	void GetGrantedSkillIds(TArray<FName>& OutIds) const;

	void NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability);
	void ExecuteActivatedAbility();
	void ResolveFireballImpact(AActor* HitActor, const FSkillLabSkillDefinition& Definition, const FSkillLabSkillRequest& Request, int32 CastId);

private:
	UPROPERTY(VisibleAnywhere) int32 UnitId = 0;
	UPROPERTY(VisibleAnywhere) bool bInitialized = false;
	UPROPERTY(Transient) TObjectPtr<USkillLabAbilitySystemComponent> ASC;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillGameplayAbility> PendingAbility;
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillPresentationRouter> PresentationRouter;
	TMap<FName, FGameplayAbilitySpecHandle> GrantedSkills;
	TMap<FName, double> CooldownEnd;
	int32 NextRequestId = 1;
	int32 NextCastId = 1;

	FSkillLabSkillRequest PendingRequest;
	FSkillLabSkillDefinition PendingDefinition;
	int32 PendingCastId = 0;

	FSkillLabSkillResult Reject(const FSkillLabSkillRequest& Request, FName Reason);
	void Emit(ESkillLabSkillEvent EventType, const FSkillLabSkillResult& Result, FName Reason = NAME_None);
	bool ResolveAndValidateTarget(const FSkillLabSkillRequest& Request, const FSkillLabSkillDefinition& Def, AActor*& OutTarget, FName& OutReason) const;
	void ApplyToTarget(AActor* Target, const FSkillLabSkillDefinition& Def, FSkillLabSkillResult& Result, int32 CastId = 0);
	void ApplyArea(const FVector& Center, const FSkillLabSkillDefinition& Def, FSkillLabSkillResult& Result);
	void AddBurning(AActor* Target, const FSkillLabSkillDefinition& Def, int32 CastId);
};
