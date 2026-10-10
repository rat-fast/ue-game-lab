#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameplayTagContainer.h"
#include "SkillLabSkillAnimNotify.generated.h"

/** Reusable montage notify. It carries only an event tag, never a SkillId. */
UCLASS(const, hidecategories=Object, collapsecategories)
class SKILLLABSKILLS_API USkillLabSkillAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SkillLab|Animation")
	FGameplayTag EventTag;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
