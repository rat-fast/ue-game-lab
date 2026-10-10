#pragma once

#include "SkillLabSkillBehavior.h"
#include "SkillLabMendSkill.generated.h"

UCLASS()
class SKILLLABSKILLS_API USkillLabMendBehavior : public USkillLabSkillBehavior
{
	GENERATED_BODY()
public:
	virtual void Execute() override;
};
