#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SkillLabHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UHorizontalBox;
class USkillLabHUDManagerComponent;
class USkillLabHUDSkillSlotWidget;

UCLASS()
class SKILLLAB_API USkillLabHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetHUDManager(USkillLabHUDManagerComponent* InHUDManager);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildWidgetTree();
	void RefreshFromSnapshot();
	void RefreshSkillSlots(const struct FSkillLabHUDSnapshot& Snapshot);

	UPROPERTY(Transient)
	TObjectPtr<USkillLabHUDManagerComponent> HUDManager;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EnergyText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> EnergyBar;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> SkillLayout;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USkillLabHUDSkillSlotWidget>> SkillSlots;

	bool bWidgetTreeBuilt = false;
};
