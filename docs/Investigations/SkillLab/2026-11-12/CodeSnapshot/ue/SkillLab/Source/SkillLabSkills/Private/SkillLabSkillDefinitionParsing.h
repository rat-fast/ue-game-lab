#pragma once

#include "Dom/JsonObject.h"
#include "SkillLabSkillTypes.h"

namespace SkillLabSkillDefinitionParsing
{
	bool ParseLuaDeclaration(const FString& Source, const FString& SourceName, FSkillLabSkillDefinition& Definition, FString& Error);
	bool ParseSkillEntry(const TSharedPtr<FJsonObject>& Json, const TMap<FString, FString>& LuaSources,
		FSkillLabSkillDefinition& Definition, FString& Error);
	bool ParseSubmissionPoints(const TSharedPtr<FJsonObject>& Json, FSkillLabSkillDefinition& Definition, FString& Error);
}

namespace SkillLabSkillRuleParsing
{
	bool ParseRuleProfiles(const TSharedPtr<FJsonObject>& Root, TMap<FName, FSkillLabOperationRules>& Out, FString& Error);
	bool ParseRuleWindows(const TSharedPtr<FJsonObject>& Json, const TMap<FName, FSkillLabOperationRules>& Profiles,
		FSkillLabSkillDefinition& Definition, FString& Error);
}
