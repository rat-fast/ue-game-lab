// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SkillLabTestArenaSubsystem.generated.h"

class APawn;

/**
 * Owns the optional training-scene setup used by the SkillLab prototype.
 *
 * This is deliberately separate from player input and skill targeting. The
 * subsystem can be removed when SkillLab is connected to a real level without
 * changing the skill request path.
 */
UCLASS()
class SKILLLAB_API USkillLabTestArenaSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** Applies the small test-level adjustments and creates the demo targets. */
	void PrepareForSkillTesting(APawn* Caster);

	/** Creates the three demo targets once for the current world. */
	void SpawnTrainingTargets(APawn* Caster);

	bool HasSpawnedTrainingTargets() const { return bTargetsSpawned; }

private:
	void ConfigureSkillTestArena();

	bool bTargetsSpawned = false;
};
