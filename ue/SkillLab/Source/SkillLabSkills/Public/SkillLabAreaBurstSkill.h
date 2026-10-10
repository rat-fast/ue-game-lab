#pragma once

#include "SkillLabSkillBehavior.h"
#include "SkillLabAreaBurstSkill.generated.h"

UCLASS()
class SKILLLABSKILLS_API USkillLabAreaBurstBehavior : public USkillLabSkillBehavior
{
	GENERATED_BODY()
public:
	virtual void Execute() override;
};
