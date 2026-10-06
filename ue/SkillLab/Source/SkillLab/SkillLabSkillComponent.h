#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "Components/ActorComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillComponent.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabAttributeSet;
class USkillLabPresentationRouter;
class USkillLabSkillAbility;

DECLARE_MULTICAST_DELEGATE_OneParam(FSkillLabEventDelegate, const FSkillLabEventData&);

struct FSkillLabGrantedSkillRecord
{
	FName SkillId = NAME_None;
	int32 GrantedAbilityId = 0;
	FGameplayAbilitySpecHandle SpecHandle;
};

UCLASS(ClassGroup = (SkillLab), meta = (BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabSkillComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "SkillLab|Skill")
	void InitializeUnit();

	bool GrantSkill(FName SkillId);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|Skill")
	FSkillLabRequestResult SubmitSkillRequest(const FSkillLabSkillRequest& Request);

	UFUNCTION(BlueprintCallable, Category = "SkillLab|Skill")
	void ResetUnit();

	UFUNCTION(BlueprintPure, Category = "SkillLab|Skill")
	float GetTestValue() const;

	UFUNCTION(BlueprintPure, Category = "SkillLab|Skill")
	float GetSkillEnergy() const;

	UFUNCTION(BlueprintPure, Category = "SkillLab|Skill")
	int32 GetUnitId() const { return UnitId; }

	int32 AllocateEffectApplicationId() { return NextEffectApplicationId++; }

	void NotifySkillAbilityActivationPending(USkillLabSkillAbility* Ability);

	FSkillLabEventDelegate OnSkillEvent;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 UnitId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 NextRequestId = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 NextCastId = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 NextEffectApplicationId = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 NextEventId = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 ActiveCastId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 CurrentRequestId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillLab|Skill")
	int32 CurrentGrantedAbilityId = 0;

	UPROPERTY(Transient)
	FSkillLabSkillRequest CurrentRequest;

	UPROPERTY(Transient)
	FSkillLabSkillDefinition CurrentDefinitionSnapshot;

	UPROPERTY(Transient)
	ESkillLabDefinitionStatus CurrentDefinitionStatus = ESkillLabDefinitionStatus::Missing;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillAbility> PendingSkillAbility;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabAttributeSet> AttributeSet;

	UPROPERTY(Transient)
	TObjectPtr<USkillLabPresentationRouter> PresentationRouter;

	TMap<FName, FSkillLabGrantedSkillRecord> GrantedSkillHandles;

	int32 NextGrantedAbilityId = 1;

	TMap<FName, float> CooldownEndTimes;

	void PublishEvent(ESkillLabEventType EventType, const FString& Reason = FString(), const FSkillLabEffectApplicationResult* EffectResult = nullptr);
	void LogEvent(const TCHAR* EventName, const FString& Detail = FString()) const;
	FString BuildTraceContext() const;
	void ResetRequestContext();
};
