#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillLabHUDWidget.generated.h"

class USkillLabHUDManagerComponent;

UCLASS()
class SKILLLAB_API USkillLabHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHUDManager(USkillLabHUDManagerComponent* InHUDManager);

protected:
	virtual void NativeConstruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USkillLabHUDManagerComponent> HUDManager;
};
