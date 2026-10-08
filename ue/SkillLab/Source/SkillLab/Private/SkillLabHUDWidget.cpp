#include "SkillLabHUDWidget.h"

#include "Fonts/SlateFontInfo.h"
#include "Rendering/DrawElements.h"
#include "Slate/SlateBrushAsset.h"
#include "Styling/CoreStyle.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabHUDManagerComponent.h"
#include "SkillLabHUDTypes.h"
#include "Components/CanvasPanel.h"
#include "Blueprint/WidgetTree.h"

void USkillLabHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	}
}

void USkillLabHUDWidget::SetHUDManager(USkillLabHUDManagerComponent* InHUDManager)
{
	HUDManager = InHUDManager;
}

int32 USkillLabHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (!HUDManager) return BaseLayer;
	const FSkillLabHUDSnapshot Snapshot = HUDManager->BuildSnapshot();

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FSlateFontInfo Font = FCoreStyle::Get().GetFontStyle(TEXT("NormalFont"));
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
	auto DrawBox = [&](const FVector2D& Position, const FVector2D& BoxSize, const FLinearColor& Color, int32 Layer)
	{
		FSlateColorBrush Brush(Color);
		FSlateDrawElement::MakeBox(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(Position)), &Brush);
	};
	auto DrawText = [&](const FVector2D& Position, const FString& Text, const FLinearColor& Color, int32 Layer)
	{
		FSlateDrawElement::MakeText(OutDrawElements, Layer, AllottedGeometry.ToPaintGeometry(FVector2D(300.0f, 28.0f), FSlateLayoutTransform(Position)), FText::FromString(Text), Font, ESlateDrawEffect::None, Color);
	};

	DrawBox(FVector2D(24.0f, 20.0f), FVector2D(310.0f, 122.0f), FLinearColor(0.015f, 0.02f, 0.03f, 0.86f), BaseLayer);
	DrawText(FVector2D(40.0f, 30.0f), TEXT("SKILL LAB"), FLinearColor::White, BaseLayer + 1);

	const float Health = Snapshot.Health;
	const float MaxHealth = FMath::Max(1.0f, Snapshot.MaxHealth);
	const float Energy = Snapshot.Energy;
	const float MaxEnergy = FMath::Max(1.0f, Snapshot.MaxEnergy);
	DrawText(FVector2D(40.0f, 58.0f), FString::Printf(TEXT("HP  %.0f / %.0f"), Health, MaxHealth), FLinearColor::White, BaseLayer + 2);
	DrawBox(FVector2D(40.0f, 82.0f), FVector2D(260.0f, 12.0f), FLinearColor(0.12f, 0.04f, 0.04f, 1.0f), BaseLayer + 1);
	DrawBox(FVector2D(40.0f, 82.0f), FVector2D(260.0f * FMath::Clamp(Health / MaxHealth, 0.0f, 1.0f), 12.0f), FLinearColor(0.85f, 0.08f, 0.08f, 1.0f), BaseLayer + 2);
	DrawText(FVector2D(40.0f, 98.0f), FString::Printf(TEXT("ENERGY  %.0f / %.0f"), Energy, MaxEnergy), FLinearColor::White, BaseLayer + 2);
	DrawBox(FVector2D(40.0f, 122.0f), FVector2D(260.0f, 12.0f), FLinearColor(0.03f, 0.06f, 0.14f, 1.0f), BaseLayer + 1);
	DrawBox(FVector2D(40.0f, 122.0f), FVector2D(260.0f * FMath::Clamp(Energy / MaxEnergy, 0.0f, 1.0f), 12.0f), FLinearColor(0.08f, 0.35f, 0.95f, 1.0f), BaseLayer + 2);

	const float CardWidth = 180.0f;
	const float CardGap = 12.0f;
	const float StartX = 24.0f;
	const float CardY = FMath::Max(160.0f, Size.Y - 92.0f);
	for (int32 Index = 0; Index < Snapshot.Skills.Num(); ++Index)
	{
		const FSkillLabHUDSkillState& Skill = Snapshot.Skills[Index];
		const float Cooldown = Skill.CooldownRemaining;
		const FVector2D CardPosition(StartX + Index * (CardWidth + CardGap), CardY);
		DrawBox(CardPosition, FVector2D(CardWidth, 68.0f), FLinearColor(0.015f, 0.02f, 0.03f, 0.9f), BaseLayer);
		const FString SkillName = FString::Printf(TEXT("%d %s"), Skill.SlotIndex + 1, *Skill.SkillId.ToString().Replace(TEXT("Skill_"), TEXT("")));
		DrawText(CardPosition + FVector2D(12.0f, 8.0f), SkillName, Cooldown > 0.0f ? FLinearColor(1.0f, 0.55f, 0.2f) : FLinearColor::White, BaseLayer + 1);
		const FString Status = Cooldown > 0.0f ? FString::Printf(TEXT("CD %.1fs"), Cooldown) : FString::Printf(TEXT("COST %.0f"), Skill.Cost);
		DrawText(CardPosition + FVector2D(12.0f, 36.0f), Status, Cooldown > 0.0f ? FLinearColor(1.0f, 0.35f, 0.1f) : FLinearColor(0.4f, 0.8f, 1.0f), BaseLayer + 1);
	}

	return BaseLayer + 3;
}
