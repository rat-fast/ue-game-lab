#pragma once

#include "SkillLabSkillBehavior.h"
#include "SkillLabBurningSkill.generated.h"

UCLASS()
class SKILLLABSKILLS_API USkillLabBurningBehavior : public USkillLabSkillBehavior
{
	GENERATED_BODY()
public:
	virtual void Execute() override;
};
