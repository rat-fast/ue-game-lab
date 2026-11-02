#pragma once

#include "Commandlets/Commandlet.h"
#include "SkillLabActionOverviewCommandlet.generated.h"

/** 只读源动画采样；分页与最终发布由 tools/skilllab_action_overview.py 完成。 */
UCLASS()
class USkillLabActionOverviewCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	USkillLabActionOverviewCommandlet();
	virtual int32 Main(const FString& Params) override;
};
