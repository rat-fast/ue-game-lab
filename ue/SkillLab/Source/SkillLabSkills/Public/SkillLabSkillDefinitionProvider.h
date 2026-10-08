#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class SKILLLABSKILLS_API FSkillLabSkillDefinitionProvider
{
public:
	static void GetDefaultSkillIds(TArray<FName>& OutIds);
	static bool Resolve(FName SkillId, FSkillLabSkillDefinition& OutDefinition);
	static void ReloadDefinitions();
};
