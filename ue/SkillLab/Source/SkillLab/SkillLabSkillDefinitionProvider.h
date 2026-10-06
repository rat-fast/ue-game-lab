#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class SKILLLAB_API FSkillLabSkillDefinitionProvider
{
public:
	static FName GetDefaultSkillId();
	static FName GetRecoverEnergySkillId();
	static void GetDefaultSkillIds(TArray<FName>& OutSkillIds);
	static ESkillLabDefinitionStatus ResolveDefinition(FName SkillId, FSkillLabSkillDefinition& OutDefinition);
};
