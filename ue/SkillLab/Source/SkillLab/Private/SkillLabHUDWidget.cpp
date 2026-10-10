#include "SkillLabHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "SkillLabHUDManagerComponent.h"
#include "SkillLabHUDSkillSlotWidget.h"
#include "SkillLabHUDTypes.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SWidget.h"
#endif

namespace SkillLabHUDStyle
{
	static const FLinearColor Panel(0.015f, 0.02f, 0.035f, 0.94f);
	static const FLinearColor Card(0.025f, 0.035f, 0.06f, 0.96f);
	static const FLinearColor CardCooldown(0.12f, 0.06f, 0.025f, 0.98f);
	static const FLinearColor Health(0.82f, 0.08f, 0.08f, 1.0f);
	static const FLinearColor Energy(0.08f, 0.32f, 0.88f, 1.0f);
}

void USkillLabHUDWidget::SetHUDManager(USkillLabHUDManagerComponent* InHUDManager)
{
	HUDManager = InHUDManager;
	RefreshFromSnapshot();
}

TSharedRef<SWidget> USkillLabHUDWidget::RebuildWidget()
{
	// UUserWidget::RebuildWidget reads RootWidget before NativeConstruct runs.
	// Build first even without player context (NativeOnInitialized is conditional).
	Initialize();
	BuildWidgetTree();
	RefreshFromSnapshot();
	return Super::RebuildWidget();
}

void USkillLabHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshFromSnapshot();
}

void USkillLabHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshFromSnapshot();
}

void USkillLabHUDWidget::BuildWidgetTree()
{
	if (bWidgetTreeBuilt || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDRoot"));
	Root->SetVisibility(ESlateVisibility::HitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBorder* StatusPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusPanel"));
	StatusPanel->SetVisibility(ESlateVisibility::Visible);
	StatusPanel->SetRenderOpacity(1.0f);
	StatusPanel->SetBrushColor(SkillLabHUDStyle::Panel);
	StatusPanel->SetPadding(FMargin(14.0f));
	UCanvasPanelSlot* StatusSlot = Root->AddChildToCanvas(StatusPanel);
	StatusSlot->SetAnchors(FAnchors(0.0f, 0.0f));
	StatusSlot->SetOffsets(FMargin(24.0f, 20.0f, 360.0f, 148.0f));

	UVerticalBox* StatusLayout = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusLayout"));
	StatusPanel->SetContent(StatusLayout);
	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
	Title->SetText(FText::FromString(TEXT("SKILL LAB")));
	Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FSlateFontInfo TitleFont = FCoreStyle::Get().GetFontStyle(TEXT("NormalFont"));
	TitleFont.Size = 18;
	Title->SetFont(TitleFont);
	StatusLayout->AddChildToVerticalBox(Title);

	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	HealthText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.72f, 0.72f, 1.0f)));
	StatusLayout->AddChildToVerticalBox(HealthText);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetFillColorAndOpacity(SkillLabHUDStyle::Health);
	USizeBox* HealthSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("HealthBarSize"));
	HealthSize->SetHeightOverride(14.0f);
	HealthSize->SetContent(HealthBar);
	StatusLayout->AddChildToVerticalBox(HealthSize)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 6.0f));

	EnergyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EnergyText"));
	EnergyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.64f, 0.82f, 1.0f, 1.0f)));
	StatusLayout->AddChildToVerticalBox(EnergyText);
	EnergyBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("EnergyBar"));
	EnergyBar->SetFillColorAndOpacity(SkillLabHUDStyle::Energy);
	USizeBox* EnergySize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("EnergyBarSize"));
	EnergySize->SetHeightOverride(14.0f);
	EnergySize->SetContent(EnergyBar);
	StatusLayout->AddChildToVerticalBox(EnergySize)->SetPadding(FMargin(0.0f, 3.0f, 0.0f, 0.0f));

	UBorder* SkillPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SkillPanel"));
	SkillPanel->SetVisibility(ESlateVisibility::Visible);
	SkillPanel->SetRenderOpacity(1.0f);
	SkillPanel->SetBrushColor(FLinearColor(0.01f, 0.015f, 0.03f, 0.92f));
	SkillPanel->SetPadding(FMargin(8.0f));
	UCanvasPanelSlot* SkillSlot = Root->AddChildToCanvas(SkillPanel);
	SkillSlot->SetAnchors(FAnchors(0.0f, 1.0f, 1.0f, 1.0f));
	SkillSlot->SetAlignment(FVector2D(0.0f, 1.0f));
	// Horizontal stretch with a bottom anchor: top is the negative panel height
	// plus the bottom margin, and bottom is the desired panel height.
	SkillSlot->SetOffsets(FMargin(24.0f, -136.0f, 24.0f, 112.0f));

	SkillLayout = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SkillLayout"));
	SkillPanel->SetContent(SkillLayout);

	bWidgetTreeBuilt = true;
}

void USkillLabHUDWidget::RefreshFromSnapshot()
{
	if (!bWidgetTreeBuilt || !HUDManager)
	{
		return;
	}

	const FSkillLabHUDSnapshot Snapshot = HUDManager->BuildSnapshot();
	if (!Snapshot.bReady)
	{
		HealthText->SetText(FText::FromString(TEXT("HP  -- / --")));
		HealthBar->SetPercent(0.0f);
		EnergyText->SetText(FText::FromString(TEXT("ENERGY  -- / --")));
		EnergyBar->SetPercent(0.0f);
		RefreshSkillSlots(Snapshot);
		return;
	}
	const float HealthMax = FMath::Max(1.0f, Snapshot.MaxHealth);
	const float EnergyMax = FMath::Max(1.0f, Snapshot.MaxEnergy);
	HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP  %.0f / %.0f"), Snapshot.Health, Snapshot.MaxHealth)));
	HealthBar->SetPercent(FMath::Clamp(Snapshot.Health / HealthMax, 0.0f, 1.0f));
	EnergyText->SetText(FText::FromString(FString::Printf(TEXT("ENERGY  %.0f / %.0f"), Snapshot.Energy, Snapshot.MaxEnergy)));
	EnergyBar->SetPercent(FMath::Clamp(Snapshot.Energy / EnergyMax, 0.0f, 1.0f));
	RefreshSkillSlots(Snapshot);
}

void USkillLabHUDWidget::RefreshSkillSlots(const FSkillLabHUDSnapshot& Snapshot)
{
	if (!SkillLayout || !WidgetTree)
	{
		return;
	}

	while (SkillSlots.Num() > Snapshot.Skills.Num())
	{
		USkillLabHUDSkillSlotWidget* RemovedSlot = SkillSlots.Pop();
		SkillLayout->RemoveChild(RemovedSlot);
	}
	while (SkillSlots.Num() < Snapshot.Skills.Num())
	{
		const int32 Index = SkillSlots.Num();
		USkillLabHUDSkillSlotWidget* SlotWidget = WidgetTree->ConstructWidget<USkillLabHUDSkillSlotWidget>(
			USkillLabHUDSkillSlotWidget::StaticClass(), *FString::Printf(TEXT("SkillSlot_%d"), Index));
		if (!SlotWidget)
		{
			break;
		}
		UHorizontalBoxSlot* CardSlot = SkillLayout->AddChildToHorizontalBox(SlotWidget);
		CardSlot->SetPadding(FMargin(4.0f));
		SkillSlots.Add(SlotWidget);
	}

	for (int32 Index = 0; Index < SkillSlots.Num(); ++Index)
	{
		SkillSlots[Index]->SetSkillState(Snapshot.Skills[Index]);
	}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabHUDLifecycleTest,
	"SkillLab.UI.WidgetTreeBeforeSlate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabHUDLifecycleTest::RunTest(const FString& Parameters)
{
	// No game, player, viewport or NativeOnInitialized callback is required:
	// TakeWidget must build the UMG tree before the superclass captures its root.
	USkillLabHUDWidget* HUD = NewObject<USkillLabHUDWidget>();
	HUD->Initialize();
	const TSharedRef<SWidget> HUDSlate = HUD->TakeWidget();
	TestEqual(TEXT("HUD returns a constructed canvas to Slate"), HUDSlate->GetType(), FName(TEXT("SConstraintCanvas")));
	UWidget* Root = HUD->WidgetTree ? HUD->WidgetTree->RootWidget : nullptr;
	TestNotNull(TEXT("HUD builds a root during RebuildWidget"), Root);
	if (!Root) return false;
	TestTrue(TEXT("HUD Slate root is already constructed"), Root->GetCachedWidget().IsValid());
	if (Root->GetCachedWidget().IsValid())
	{
		TestEqual(TEXT("HUD Slate root is a canvas, not an empty spacer"), Root->GetCachedWidget()->GetType(), FName(TEXT("SConstraintCanvas")));
	}
	UCanvasPanel* Canvas = Cast<UCanvasPanel>(Root);
	TestTrue(TEXT("Canvas contains status and skills panels"), Canvas && Canvas->GetChildrenCount() == 2);
	UBorder* SkillPanel = Cast<UBorder>(HUD->WidgetTree->FindWidget(TEXT("SkillPanel")));
	UCanvasPanelSlot* SkillPanelSlot = SkillPanel ? Cast<UCanvasPanelSlot>(SkillPanel->Slot) : nullptr;
	TestTrue(TEXT("Skill panel has enough height for an 84px card plus padding"),
		SkillPanelSlot && SkillPanelSlot->GetOffsets().Top <= -108.0f && SkillPanelSlot->GetOffsets().Bottom >= 108.0f);

	USkillLabHUDSkillSlotWidget* Slot = NewObject<USkillLabHUDSkillSlotWidget>();
	Slot->Initialize();
	FSkillLabHUDSkillState State;
	State.SkillId = TEXT("Skill_Fireball");
	State.SlotIndex = 0;
	State.ShortcutKey = EKeys::One;
	State.Cost = 10.0f;
	State.CooldownDuration = 2.0f;
	State.CooldownRemaining = 1.0f;
	Slot->SetSkillState(State); // State arrives before Slate is constructed.
	const TSharedRef<SWidget> SlotSlate = Slot->TakeWidget();
	TestEqual(TEXT("Skill card returns a constructed box to Slate"), SlotSlate->GetType(), FName(TEXT("SBox")));
	UWidget* SlotRoot = Slot->WidgetTree ? Slot->WidgetTree->RootWidget : nullptr;
	TestNotNull(TEXT("Skill card builds a root during RebuildWidget"), SlotRoot);
	if (!SlotRoot) return false;
	TestTrue(TEXT("Skill card Slate root is constructed"), SlotRoot->GetCachedWidget().IsValid());
	if (SlotRoot->GetCachedWidget().IsValid())
	{
		TestEqual(TEXT("Skill card Slate root is a box"), SlotRoot->GetCachedWidget()->GetType(), FName(TEXT("SBox")));
	}
	UTextBlock* SlotName = Cast<UTextBlock>(Slot->WidgetTree->FindWidget(TEXT("SkillName")));
	TestTrue(TEXT("Preconstruction state is rendered in card text"), SlotName && SlotName->GetText().ToString().Contains(TEXT("1  Fireball")));
	UProgressBar* Cooldown = Cast<UProgressBar>(Slot->WidgetTree->FindWidget(TEXT("SkillCooldownBar")));
	TestTrue(TEXT("Cooldown fraction reaches the actual UMG progress bar"), Cooldown && FMath::IsNearlyEqual(Cooldown->GetPercent(), 0.5f));
	return true;
}
#endif
