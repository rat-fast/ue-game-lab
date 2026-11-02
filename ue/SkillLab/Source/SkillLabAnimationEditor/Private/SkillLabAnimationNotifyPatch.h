#pragma once

#include "CoreMinimal.h"
#include "SkillLabAnimationContentTypes.h"

class UAnimMontage;

namespace SkillLabAnimationNotifyPatch
{
	bool IsPatchDocument(const FString& Filename);
	bool Parse(const FString& Text, FSkillLabAnimationPatch& Patch, FString& Error);
	/** 完整预检后才触碰目标；失败恢复磁盘和内存，返回完整 JSON 报告。 */
	int32 Run(const FString& SpecFilename, const FString& ReportFilename, bool bWrite);
}
