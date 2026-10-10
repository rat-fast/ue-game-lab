#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabTestCommandsComponent.generated.h"

class APawn;

/** Optional demo commands. Disabling this component disables setup and all test commands. */
UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabTestCommandsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabTestCommandsComponent();
	void InitializeForPawn(APawn* Pawn);
	void RequestSlot(int32 SlotIndex);
	void SkillTest();
	void SkillTestRecover();
	void SkillArea();
	void SkillBurn();
	void SpawnTargets();
	void ResetSkills();
	void ReloadDefinitions();

	UPROPERTY(EditAnywhere, Category="SkillLab|Testing") bool bEnableTestCommands = true;
	UPROPERTY(EditAnywhere, Category="SkillLab|Testing") bool bInitializeTrainingArena = true;

private:
	APawn* GetControlledPawn() const;
};
