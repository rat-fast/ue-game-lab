#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"

class SKILLLABSKILLS_API FSkillLabSkillDefinitionProvider
{
public:
	static void GetDefaultSkillIds(TArray<FName>& OutIds);
	static bool Resolve(FName SkillId, FSkillLabSkillDefinition& OutDefinition);
	/** Resolves only the presentation binding so adapters do not depend on content rules. */
	static bool ResolvePresentationBinding(FName SkillId, FSkillLabSkillPresentationBinding& OutBinding);
	/** Loads and validates an entire candidate catalog before replacing the current one. */
	static bool ReloadDefinitions();
	/** Pure catalog validation entry used by editor tooling and automation tests. */
	static bool ValidateJsonText(const FString& JsonText, FString& OutError);
	/** Parses a complete candidate catalog; Lua sources are keyed by their JSON luaScript path. */
	static bool ParseCatalogText(const FString& JsonText, const TMap<FString, FString>& LuaSources,
		TMap<FName, FSkillLabSkillDefinition>& OutDefinitions, FString& OutError);
#if WITH_DEV_AUTOMATION_TESTS
	static bool CopyDefinitionsForAutomation(TMap<FName, FSkillLabSkillDefinition>& OutDefinitions);
	static void ReplaceDefinitionsForAutomation(const TMap<FName, FSkillLabSkillDefinition>& InDefinitions,
		bool bInLoadAttempted = true);
#endif
};
