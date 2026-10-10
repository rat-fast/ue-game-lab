#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabSkillInputComponent.generated.h"

class ASkillLabSkillFeedbackActor;
class USkillLabSkillRuntimeComponent;
struct FSkillLabSkillDefinition;

UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabSkillInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabSkillInputComponent();
	UFUNCTION(BlueprintCallable, Category="SkillLab|Input") void BeginSkill(FName SkillId);
	UFUNCTION(BlueprintCallable, Category="SkillLab|Input") void ConfirmSkill();
	UFUNCTION(BlueprintCallable, Category="SkillLab|Input") void CancelSkill();
	UFUNCTION(BlueprintPure, Category="SkillLab|Input") bool IsTargetingGround() const { return bTargetingGround; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool ResolveGroundLocation(FVector& OutLocation) const;
	bool ResolveCasterAndRuntime(USkillLabSkillRuntimeComponent*& OutRuntime) const;
	void StartGroundPreview(const FSkillLabSkillDefinition& Definition);
	void DestroyPreview();

	UPROPERTY(Transient) TObjectPtr<ASkillLabSkillFeedbackActor> PreviewActor;
	FName PendingSkillId = NAME_None;
	TWeakObjectPtr<AActor> TargetingCaster;
	bool bTargetingGround = false;
};
