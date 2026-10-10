#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class USkillLabSkillRuntimeComponent;
class APlayerController;

/** Shared target rules used by every skill execution template. */
class SKILLLABSKILLS_API FSkillLabTargetingService
{
public:
	/** Returns whether the target satisfies the relation declared by a skill. This is the
	 * single relation policy used by request validation, cursor targeting and area queries. */
	static bool MatchesRelation(const USkillLabSkillRuntimeComponent& Source,
		const USkillLabSkillRuntimeComponent& Target, ESkillLabTargetRelation Relation);
	/** Validates an already selected unit without performing any input or cursor query. */
	static bool ValidateUnitTarget(const USkillLabSkillRuntimeComponent& Source, AActor* Candidate,
		const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FName& OutReason, bool bCheckCastRange = true);
	static bool ValidateRequest(const USkillLabSkillRuntimeComponent& Source, const FSkillLabSkillRequest& Request,
		const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FName& OutReason);
	/** Resolves a unit-targeting skill from the player's cursor. Self-target skills bypass the cursor. */
	static bool ResolveCursorUnitTarget(APlayerController& Controller, const USkillLabSkillRuntimeComponent& Source,
		const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FVector& OutLocation, FName& OutReason);
	static void FindAreaTargets(const USkillLabSkillRuntimeComponent& Source, const FVector& Center,
		const FSkillLabSkillDefinition& Definition, TArray<AActor*>& OutTargets);
};
