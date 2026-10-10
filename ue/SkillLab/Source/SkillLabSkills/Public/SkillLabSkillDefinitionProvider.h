#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class SKILLLABSKILLS_API FSkillLabSkillDefinitionProvider
{
public:
	static void GetDefaultSkillIds(TArray<FName>& OutIds);
	static bool Resolve(FName SkillId, FSkillLabSkillDefinition& OutDefinition);
	/** Loads and validates an entire candidate catalog before replacing the current one. */
	static bool ReloadDefinitions();
	/** Pure catalog validation entry used by editor tooling and automation tests. */
	static bool ValidateJsonText(const FString& JsonText, FString& OutError);
};
