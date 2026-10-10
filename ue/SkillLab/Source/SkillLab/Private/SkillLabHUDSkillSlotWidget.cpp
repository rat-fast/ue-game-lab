#include "SkillLabHUDSkillSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace
{
	FSlateBrush MakeFlatBrush(const FLinearColor Color)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.TintColor = FSlateColor(Color);
		Brush.Margin = FMargin(0.2f);
		return Brush;
	}

	FSlateFontInfo MakeFont(const int32 Size, const bool bBold = false)
	{
		FSlateFontInfo Font = FCoreStyle::Get().GetFontStyle(bBold ? TEXT("BoldFont") : TEXT("NormalFont"));
		Font.Size = Size;
		return Font;
	}
}

TSharedRef<SWidget> USkillLabHUDSkillSlotWidget::RebuildWidget()
{
	Initialize();
	BuildWidgetTree();
	RefreshVisuals();
	return Super::RebuildWidget();
}

void USkillLabHUDSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshVisuals();
}

void USkillLabHUDSkillSlotWidget::BuildWidgetTree()
{
	if (!WidgetTree || WidgetTree->RootWidget)
	{
		RefreshVisuals();
		return;
	}

	USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SkillSlotSize"));
	SizeBox->SetWidthOverride(168.0f);
	SizeBox->SetHeightOverride(84.0f);

	CardBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SkillCard"));
	CardBorder->SetPadding(FMargin(10.0f, 8.0f));
	CardBorder->SetBrush(MakeFlatBrush(FLinearColor(0.018f, 0.024f, 0.040f, 0.94f)));
	SizeBox->AddChild(CardBorder);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SkillContent"));
	CardBorder->AddChild(Content);

	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SkillName"));
	NameText->SetFont(MakeFont(16, true));
	NameText->SetColorAndOpacity(FLinearColor::White);
	Content->AddChildToVerticalBox(NameText);

	UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillStatus"));
	Content->AddChildToVerticalBox(StatusRow);

	CostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SkillCost"));
	CostText->SetFont(MakeFont(12));
	CostText->SetColorAndOpacity(FLinearColor(0.48f, 0.80f, 1.0f, 1.0f));
	StatusRow->AddChildToHorizontalBox(CostText);

	CooldownText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SkillCooldown"));
	CooldownText->SetFont(MakeFont(12));
	CooldownText->SetColorAndOpacity(FLinearColor(1.0f, 0.68f, 0.24f, 1.0f));
	UHorizontalBoxSlot* CooldownSlot = StatusRow->AddChildToHorizontalBox(CooldownText);
	CooldownSlot->SetPadding(FMargin(12.0f, 0.0f, 0.0f, 0.0f));

	CooldownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SkillCooldownBar"));
	FProgressBarStyle BarStyle;
	BarStyle.BackgroundImage = MakeFlatBrush(FLinearColor(0.10f, 0.12f, 0.16f, 1.0f));
	BarStyle.FillImage = MakeFlatBrush(FLinearColor(1.0f, 0.50f, 0.12f, 1.0f));
	CooldownBar->SetWidgetStyle(BarStyle);
	CooldownBar->SetPercent(0.0f);
	UVerticalBoxSlot* BarSlot = Content->AddChildToVerticalBox(CooldownBar);
	BarSlot->SetPadding(FMargin(0.0f, 7.0f, 0.0f, 0.0f));
	BarSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	WidgetTree->RootWidget = SizeBox;
	RefreshVisuals();
}

void USkillLabHUDSkillSlotWidget::SetSkillState(const FSkillLabHUDSkillState& InState)
{
	State = InState;
	RefreshVisuals();
}

void USkillLabHUDSkillSlotWidget::RefreshVisuals()
{
	if (!NameText || !CostText || !CooldownText || !CooldownBar)
	{
		return;
	}

	const FString DisplayName = State.SkillId.ToString().Replace(TEXT("Skill_"), TEXT(""));
	const FString KeyLabel = State.ShortcutKey.IsValid() ? State.ShortcutKey.GetDisplayName().ToString() : TEXT("--");
	NameText->SetText(FText::FromString(FString::Printf(TEXT("%s  %s"), *KeyLabel, *DisplayName)));
	CostText->SetText(FText::FromString(FString::Printf(TEXT("COST %.0f"), State.Cost)));

	const bool bOnCooldown = State.CooldownRemaining > KINDA_SMALL_NUMBER;
	CooldownText->SetText(bOnCooldown
		? FText::FromString(FString::Printf(TEXT("CD %.1fs"), State.CooldownRemaining))
		: FText::FromString(TEXT("READY")));
	CooldownText->SetColorAndOpacity(bOnCooldown
		? FLinearColor(1.0f, 0.60f, 0.18f, 1.0f)
		: FLinearColor(0.36f, 0.95f, 0.58f, 1.0f));
	CooldownBar->SetPercent(State.CooldownDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(State.CooldownRemaining / State.CooldownDuration, 0.0f, 1.0f)
		: 0.0f);
}
