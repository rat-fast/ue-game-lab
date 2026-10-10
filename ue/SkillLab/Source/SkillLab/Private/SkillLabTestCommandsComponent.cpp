#include "SkillLabTestCommandsComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	return Controller ? Controller->GetPawn() : nullptr;
}

void USkillLabTestCommandsComponent::InitializeForPawn(APawn* Pawn)
{
	if (!bEnableTestCommands || !bInitializeTrainingArena || !Pawn) return;
	if (USkillLabTestArenaSubsystem* Arena = GetWorld() ? GetWorld()->GetSubsystem<USkillLabTestArenaSubsystem>() : nullptr)
	{
		Arena->PrepareForSkillTesting(Pawn);
	}
}

void USkillLabTestCommandsComponent::RequestSlot(const int32 SlotIndex)
{
	if (!bEnableTestCommands) return;
	if (USkillLabSkillShortcutComponent* Shortcuts = GetOwner() ? GetOwner()->FindComponentByClass<USkillLabSkillShortcutComponent>() : nullptr)
	{
		Shortcuts->ActivateSlot(SlotIndex);
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
	const APawn* Pawn = GetControlledPawn();
	if (USkillLabSkillRuntimeComponent* Runtime = Pawn ? Pawn->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr) Runtime->ResetSkills();
}

void USkillLabTestCommandsComponent::ReloadDefinitions()
{
	if (!bEnableTestCommands) return;
	const bool bReloaded = FSkillLabSkillDefinitionProvider::ReloadDefinitions();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabTesting] SkillDefinitions reload %s"), bReloaded ? TEXT("succeeded") : TEXT("failed; previous definitions retained"));
}
