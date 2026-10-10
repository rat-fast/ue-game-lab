#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillLabHUDTypes.h"
#include "SkillLabHUDSkillSlotWidget.generated.h"

class UBorder;
class UTextBlock;
class UProgressBar;

/** One data-driven skill card. It owns presentation only; activation remains in the controller/runtime. */
UCLASS()
class SKILLLAB_API USkillLabHUDSkillSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	void SetSkillState(const FSkillLabHUDSkillState& InState);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildWidgetTree();
	void RefreshVisuals();

	FSkillLabHUDSkillState State;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CardBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CostText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CooldownText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> CooldownBar;
};
