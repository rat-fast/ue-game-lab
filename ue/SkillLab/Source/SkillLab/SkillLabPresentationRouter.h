#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SkillLabPresentationRouter.generated.h"

struct FSkillLabEventData;
class USkillLabSkillComponent;

UCLASS(BlueprintType)
class SKILLLAB_API USkillLabPresentationRouter : public UObject
{
	GENERATED_BODY()

public:
	void Bind(USkillLabSkillComponent* InSource);

	UFUNCTION(BlueprintPure, Category = "SkillLab|Presentation")
	bool HasActiveCast() const { return bHasActiveCast; }

private:
	void ClearActivePresentation();
	void HandleSkillEvent(const FSkillLabEventData& Event);

	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillComponent> Source;

	UPROPERTY(Transient)
	int32 ActiveCastId = 0;

	UPROPERTY(Transient)
	bool bHasActiveCast = false;
};
