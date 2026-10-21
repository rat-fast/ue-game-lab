#pragma once

#include "Commandlets/Commandlet.h"
#include "SkillLabAnimationContentCommandlet.generated.h"

UCLASS()
class USkillLabAnimationContentCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	USkillLabAnimationContentCommandlet();
	virtual int32 Main(const FString& Params) override;
};
