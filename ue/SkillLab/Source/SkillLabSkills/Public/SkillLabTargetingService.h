#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class USkillLabSkillRuntimeComponent;

/** Shared target rules used by every skill execution template. */
class SKILLLABSKILLS_API FSkillLabTargetingService
{
public:
	static bool ValidateRequest(const USkillLabSkillRuntimeComponent& Source, const FSkillLabSkillRequest& Request,
		const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FName& OutReason);
	static void FindAreaTargets(const USkillLabSkillRuntimeComponent& Source, const FVector& Center,
		const FSkillLabSkillDefinition& Definition, TArray<AActor*>& OutTargets);
};
