#pragma once
#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillBehavior.generated.h"

class USkillLabSkillRuntimeComponent;

/** One instance per cast. Skill-specific steps live here, never in the scheduler. */
UCLASS(Abstract)
class SKILLLABSKILLS_API USkillLabSkillBehavior : public UObject
{
	GENERATED_BODY()
public:
	virtual UWorld* GetWorld() const override;
	void Initialize(USkillLabSkillRuntimeComponent* InRuntime, const FSkillLabSkillDefinition& InDefinition,
		const FSkillLabSkillRequest& InRequest, int32 InCastId);
	virtual void Execute() PURE_VIRTUAL(USkillLabSkillBehavior::Execute, );
	virtual void ReceiveEvent(FGameplayTag EventTag) {}
	virtual void Cleanup() {}
protected:
	bool IsCurrent() const;
	FSkillLabSkillResult ApplyEffect(AActor* Target);
	void ReportMiss(FName Reason, AActor* Target = nullptr);
	void Complete(FName Reason = NAME_None);
	void Fail(FName Reason);
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	UPROPERTY(Transient) FSkillLabSkillDefinition Definition;
	UPROPERTY(Transient) FSkillLabSkillRequest Request;
	int32 CastId = 0;
};
