#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabHUDTypes.h"
#include "SkillLabHUDManagerComponent.generated.h"

class USkillLabSkillRuntimeComponent;

UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabHUDManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabHUDManagerComponent();
	void SetSkillRuntime(USkillLabSkillRuntimeComponent* InSkillRuntime);
	FSkillLabHUDSnapshot BuildSnapshot() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	mutable bool bLoggedFirstSnapshot = false;
};
