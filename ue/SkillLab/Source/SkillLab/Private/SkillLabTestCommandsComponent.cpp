#include "SkillLabTestCommandsComponent.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "SkillLabSkillShortcutComponent.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabTestArenaSubsystem.h"

USkillLabTestCommandsComponent::USkillLabTestCommandsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

APawn* USkillLabTestCommandsComponent::GetControlledPawn() const
{
	return ControlledPawn.Get();
}

void USkillLabTestCommandsComponent::InitializeForPawn(APawn* Pawn, USkillLabSkillShortcutComponent* InShortcuts)
{
	ControlledPawn = Pawn;
	ShortcutComponent = InShortcuts;
	SkillRuntime = Pawn ? Pawn->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	if (!bEnableTestCommands || !bInitializeTrainingArena || !Pawn) return;
	if (USkillLabTestArenaSubsystem* Arena = GetWorld() ? GetWorld()->GetSubsystem<USkillLabTestArenaSubsystem>() : nullptr)
	{
		Arena->PrepareForSkillTesting(Pawn);
	}
}

void USkillLabTestCommandsComponent::RequestSlot(const int32 SlotIndex)
{
	if (!bEnableTestCommands) return;
	if (ShortcutComponent.IsValid())
	{
		ShortcutComponent->ActivateSlot(SlotIndex);
	}
}

void USkillLabTestCommandsComponent::SkillTest() { RequestSlot(0); }
void USkillLabTestCommandsComponent::SkillTestRecover() { RequestSlot(2); }
void USkillLabTestCommandsComponent::SkillArea() { RequestSlot(1); }
void USkillLabTestCommandsComponent::SkillBurn() { RequestSlot(3); }

void USkillLabTestCommandsComponent::SpawnTargets()
{
	if (!bEnableTestCommands) return;
	if (USkillLabTestArenaSubsystem* Arena = GetWorld() ? GetWorld()->GetSubsystem<USkillLabTestArenaSubsystem>() : nullptr)
	{
		Arena->SpawnTrainingTargets(GetControlledPawn());
	}
}

void USkillLabTestCommandsComponent::ResetSkills()
{
	if (!bEnableTestCommands) return;
	if (SkillRuntime.IsValid()) SkillRuntime->ResetSkills();
}

void USkillLabTestCommandsComponent::ReloadDefinitions()
{
	if (!bEnableTestCommands) return;
	const bool bReloaded = FSkillLabSkillDefinitionProvider::ReloadDefinitions();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabTesting] SkillDefinitions reload %s"), bReloaded ? TEXT("succeeded") : TEXT("failed; previous definitions retained"));
}
