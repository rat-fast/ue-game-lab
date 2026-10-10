// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabPlayerController.h"

#include "SkillLabCharacter.h"
#include "SkillLabHUDManagerComponent.h"
#include "SkillLabPlayerInputComponent.h"
#include "SkillLabSkillInputComponent.h"
#include "SkillLabSkillShortcutComponent.h"
#include "SkillLabTestCommandsComponent.h"

ASkillLabPlayerController::ASkillLabPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	HUDManager = CreateDefaultSubobject<USkillLabHUDManagerComponent>(TEXT("HUDManager"));
	InputAdapter = CreateDefaultSubobject<USkillLabPlayerInputComponent>(TEXT("InputAdapter"));
	SkillInput = CreateDefaultSubobject<USkillLabSkillInputComponent>(TEXT("SkillInput"));
	SkillShortcuts = CreateDefaultSubobject<USkillLabSkillShortcutComponent>(TEXT("SkillShortcuts"));
	TestCommands = CreateDefaultSubobject<USkillLabTestCommandsComponent>(TEXT("TestCommands"));
}

void ASkillLabPlayerController::SkillTest()
{
	if (TestCommands) TestCommands->SkillTest();
}

void ASkillLabPlayerController::SkillTestRecover()
{
	if (TestCommands) TestCommands->SkillTestRecover();
}

void ASkillLabPlayerController::SkillArea() { if (TestCommands) TestCommands->SkillArea(); }
void ASkillLabPlayerController::SkillBurn() { if (TestCommands) TestCommands->SkillBurn(); }

void ASkillLabPlayerController::SkillSpawnTargets()
{
	if (TestCommands) TestCommands->SpawnTargets();
}

void ASkillLabPlayerController::SkillReset()
{
	if (TestCommands) TestCommands->ResetSkills();
}

void ASkillLabPlayerController::SkillReloadDefinitions()
{
	if (TestCommands) TestCommands->ReloadDefinitions();
}

void ASkillLabPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (SkillInput) SkillInput->InitializeForPlayer(this, GetPawn());
	if (TestCommands) TestCommands->InitializeForPawn(GetPawn(), SkillShortcuts);

	bShowMouseCursor = true;
	bEnableClickEvents = true;
	bEnableMouseOverEvents = false;

	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

	if (HUDManager) HUDManager->InitializeForPlayer(this, GetPawn(), SkillShortcuts);
	if (InputAdapter) InputAdapter->ActivateInputMapping();
}

void ASkillLabPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputAdapter)
	{
		InputAdapter->Configure(MoveOrRotateAction, CameraLookAction, CameraZoomAction,
			MoveSuccessEffect, MoveFailureEffect, DragThresholdPixels, SkillInput);
		InputAdapter->BindInput(InputComponent);
	}
	if (InputComponent)
	{
		if (SkillShortcuts) SkillShortcuts->BindInput(InputComponent, SkillInput);
	}
}

void ASkillLabPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (!HasActorBegunPlay()) return;
	if (HUDManager) HUDManager->InitializeForPlayer(this, InPawn, SkillShortcuts);
	if (SkillInput) SkillInput->InitializeForPlayer(this, InPawn);
	if (TestCommands) TestCommands->InitializeForPawn(InPawn, SkillShortcuts);
}

void ASkillLabPlayerController::OnUnPossess()
{
	if (SkillInput) SkillInput->ClearPlayerContext();
	if (InputAdapter) InputAdapter->ResetPointerState();
	if (HUDManager) HUDManager->SetControlledPawn(nullptr);
	if (TestCommands) TestCommands->InitializeForPawn(nullptr, SkillShortcuts);
	Super::OnUnPossess();
}
