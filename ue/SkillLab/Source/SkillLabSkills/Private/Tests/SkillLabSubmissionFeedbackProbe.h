#pragma once
#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSubmissionFeedbackProbe.generated.h"

// 自动化通过真实动态委托观察 Blueprint 反馈通道；不参与生产行为或结果确认。
UCLASS(Transient)
class USkillLabSubmissionFeedbackProbe : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION()
	void Receive(const FSkillLabSkillResult& Result)
	{
		if (OnAnyEvent) OnAnyEvent(Result);
		if (Result.Event != ESkillLabSkillEvent::SubmissionResolved) return;
		++Count;
		Last = Result;
		if (OnResolved) OnResolved();
	}
	int32 Count = 0;
	FSkillLabSkillResult Last;
	TFunction<void()> OnResolved;
	TFunction<void(const FSkillLabSkillResult&)> OnAnyEvent;
};
