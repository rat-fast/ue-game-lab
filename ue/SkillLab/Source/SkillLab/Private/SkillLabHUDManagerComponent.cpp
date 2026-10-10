#include "SkillLabHUDManagerComponent.h"

#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillShortcutComponent.h"
#include "SkillLabHUDWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

USkillLabHUDManagerComponent::USkillLabHUDManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WidgetClass = USkillLabHUDWidget::StaticClass();
}

void USkillLabHUDManagerComponent::SetSkillRuntime(USkillLabSkillRuntimeComponent* InSkillRuntime)
{
	SkillRuntime = InSkillRuntime;
}

void USkillLabHUDManagerComponent::SetShortcutComponent(USkillLabSkillShortcutComponent* InShortcuts)
{
	ShortcutComponent = InShortcuts;
}

void USkillLabHUDManagerComponent::InitializeForPlayer(APlayerController* Player, APawn* Pawn, USkillLabSkillShortcutComponent* Shortcuts)
{
	SetSkillRuntime(Pawn ? Pawn->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr);
	SetShortcutComponent(Shortcuts);
	if (!Player || !Player->IsLocalController() || !WidgetClass || HUDWidget) return;
	HUDWidget = CreateWidget<USkillLabHUDWidget>(Player, WidgetClass);
	if (HUDWidget)
	{
		HUDWidget->SetHUDManager(this);
		HUDWidget->SetVisibility(ESlateVisibility::Visible);
		HUDWidget->SetRenderOpacity(1.0f);
		HUDWidget->AddToViewport(1000);
		UE_LOG(LogTemp, Log, TEXT("[SkillLabHUD] Created HUD widget and connected shared skill shortcut mapping"));
	}
}

void USkillLabHUDManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HUDWidget) { HUDWidget->RemoveFromParent(); HUDWidget = nullptr; }
	SkillRuntime = nullptr;
	ShortcutComponent = nullptr;
	Super::EndPlay(EndPlayReason);
}

FSkillLabHUDSnapshot USkillLabHUDManagerComponent::BuildSnapshot() const
{
	FSkillLabHUDSnapshot Snapshot;
	if (!SkillRuntime || !SkillRuntime->IsInitialized()) return Snapshot;
	Snapshot.bReady = true;

	Snapshot.Health = SkillRuntime->GetHealthValue();
	Snapshot.MaxHealth = SkillRuntime->GetMaxHealthValue();
	Snapshot.Energy = SkillRuntime->GetEnergyValue();
	Snapshot.MaxEnergy = SkillRuntime->GetMaxEnergyValue();

	TArray<FSkillLabSkillShortcut> Shortcuts = ShortcutComponent ? ShortcutComponent->GetShortcuts() : TArray<FSkillLabSkillShortcut>();
	for (int32 Index = 0; Index < Shortcuts.Num(); ++Index)
	{
		FSkillLabSkillDefinition Definition;
		if (!FSkillLabSkillDefinitionProvider::Resolve(Shortcuts[Index].SkillId, Definition)) continue;
		FSkillLabHUDSkillState& State = Snapshot.Skills.AddDefaulted_GetRef();
		State.SkillId = Shortcuts[Index].SkillId;
		State.SlotIndex = Index;
		State.ShortcutKey = Shortcuts[Index].Key;
		State.Cost = Definition.Cost;
		State.CooldownDuration = Definition.Cooldown;
		State.CooldownRemaining = SkillRuntime->GetCooldownRemaining(Shortcuts[Index].SkillId);
	}
	if (!bLoggedFirstSnapshot)
	{
		bLoggedFirstSnapshot = true;
		UE_LOG(LogTemp, Log, TEXT("[SkillLabHUD] Snapshot HP=%.0f/%.0f Energy=%.0f/%.0f Skills=%d"), Snapshot.Health, Snapshot.MaxHealth, Snapshot.Energy, Snapshot.MaxEnergy, Snapshot.Skills.Num());
	}
	return Snapshot;
}
