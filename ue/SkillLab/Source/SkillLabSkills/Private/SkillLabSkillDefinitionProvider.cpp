#include "SkillLabSkillDefinitionProvider.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkillLabSkillBehaviorRegistry.h"
#include "SkillLabSkillDefinitionParsing.h"

namespace
{
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	bool bLoadAttempted = false;
	FString AcceptedCatalogSourceFingerprint;

	bool Fail(FString& Error, const FString& Message)
	{
		Error = Message;
		return false;
	}

	bool ParseCatalog(const TSharedPtr<FJsonObject>& Root, const TMap<FString, FString>& LuaSources,
		TMap<FName, FSkillLabSkillDefinition>& OutCandidates, FString& Error)
	{
		if (!Root.IsValid()) return Fail(Error, TEXT("root must be an object"));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Root->Values)
			if (Field.Key != TEXT("skills") && Field.Key != TEXT("combatRules")) return Fail(Error, FString::Printf(TEXT("unknown catalog field '%s'"), *Field.Key));
		TMap<FName, FSkillLabOperationRules> Profiles;
		if (!SkillLabSkillRuleParsing::ParseRuleProfiles(Root, Profiles, Error)) return false;
		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("skills"), Skills) || !Skills || Skills->IsEmpty())
			return Fail(Error, TEXT("skills must be a nonempty array"));

		TMap<FName, FSkillLabSkillDefinition> Candidate;
		for (int32 Index = 0; Index < Skills->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Skills)[Index];
			FSkillLabSkillDefinition Definition;
			FSkillLabSkillBehaviorRegistry::ApplySharedRuntimeClasses(Definition);
			if (!Value.IsValid() || Value->Type != EJson::Object
				|| !SkillLabSkillDefinitionParsing::ParseSkillEntry(Value->AsObject(), LuaSources, Definition, Error))
			{
				if (Error.IsEmpty()) Error = FString::Printf(TEXT("invalid skill entry at skills[%d]"), Index);
				Error = FString::Printf(TEXT("skills[%d] skill=%s: %s"), Index, *Definition.SkillId.ToString(), *Error);
				return false;
			}
			if (Candidate.Contains(Definition.SkillId))
				return Fail(Error, FString::Printf(TEXT("duplicate skill id '%s'"), *Definition.SkillId.ToString()));
			if (!Definition.OperationRulesId.IsNone())
			{
				const FSkillLabOperationRules* Base = Profiles.Find(Definition.OperationRulesId);
				const FSkillLabOperationRules* Recovery = Profiles.Find(Definition.RecoveryRulesId);
				if (!Base || !Recovery) return Fail(Error, TEXT("unknown combatRules or recoveryRules profile"));
				Definition.OperationRules = *Base;
				Definition.RecoveryRules = *Recovery;
			}
			if (!SkillLabSkillRuleParsing::ParseRuleWindows(Value->AsObject(), Profiles, Definition, Error)) return false;
			Candidate.Add(Definition.SkillId, Definition);
		}
		OutCandidates = MoveTemp(Candidate);
		return true;
	}

	bool ParseText(const FString& JsonText, const TMap<FString, FString>& LuaSources,
		TMap<FName, FSkillLabSkillDefinition>& OutCandidates, FString& Error)
	{
		struct FJsonScope
		{
			bool bObject;
			TSet<FString> Fields;
		};
		TArray<FJsonScope> Scopes;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		EJsonNotation Notation;
		while (Reader->ReadNext(Notation))
		{
			if (Notation == EJsonNotation::Error) return Fail(Error, TEXT("invalid JSON"));
			if (Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd)
			{
				if (!Scopes.IsEmpty()) Scopes.Pop();
				continue;
			}
			if (!Scopes.IsEmpty() && Scopes.Last().bObject)
			{
				const FString& Field = Reader->GetIdentifier();
				if (Scopes.Last().Fields.Contains(Field))
					return Fail(Error, FString::Printf(TEXT("duplicate JSON field '%s' at line %d"), *Field, Reader->GetLineNumber()));
				Scopes.Last().Fields.Add(Field);
			}
			if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart)
				Scopes.Add({Notation == EJsonNotation::ObjectStart, {}});
		}
		if (!Reader->GetErrorMessage().IsEmpty()) return Fail(Error, TEXT("invalid JSON"));
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root)) return Fail(Error, TEXT("invalid JSON"));
		return ParseCatalog(Root, LuaSources, OutCandidates, Error);
	}

	bool LoadCandidate(TMap<FName, FSkillLabSkillDefinition>& OutCandidates, FString& OutFingerprint)
	{
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SkillDefinitions.json"));
		FString Contents;
		if (!FFileHelper::LoadFileToString(Contents, *Path))
		{
			UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Definition load rejected: cannot read %s; previous definitions retained"), *Path);
			return false;
		}
		FString Error;
		TMap<FName, FSkillLabSkillDefinition> Candidate;
		if (!ParseText(Contents, {}, Candidate, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Definition load rejected: %s; previous definitions retained"), *Error);
			return false;
		}
		FString FingerprintSource = Contents;
		TMap<FString, FString> LuaSources;
		for (const TPair<FName, FSkillLabSkillDefinition>& Pair : Candidate)
			LuaSources.Add(Pair.Value.LuaScript, Pair.Value.LuaSource);
		TArray<FString> LuaPaths;
		LuaSources.GetKeys(LuaPaths);
		LuaPaths.Sort();
		for (const FString& LuaPath : LuaPaths)
		{
			FingerprintSource += TEXT("\n-- ") + LuaPath + TEXT("\n") + LuaSources.FindChecked(LuaPath);
		}
		FTCHARToUTF8 Utf8(*FingerprintSource);
		const FSHAHash Signature = FSHA1::HashBuffer(Utf8.Get(), Utf8.Length());
		OutFingerprint = TEXT("sha1:") + Signature.ToString().ToLower();
		OutCandidates = MoveTemp(Candidate);
		return true;
	}

	void EnsureLoaded()
	{
		if (!bLoadAttempted) FSkillLabSkillDefinitionProvider::ReloadDefinitions();
	}
}

void FSkillLabSkillDefinitionProvider::GetDefaultSkillIds(TArray<FName>& OutIds)
{
	EnsureLoaded();
	Definitions.GetKeys(OutIds);
	OutIds.Sort(FNameLexicalLess());
}

bool FSkillLabSkillDefinitionProvider::Resolve(const FName SkillId, FSkillLabSkillDefinition& OutDefinition)
{
	EnsureLoaded();
	if (const FSkillLabSkillDefinition* Found = Definitions.Find(SkillId))
	{
		OutDefinition = *Found;
		return true;
	}
	OutDefinition = FSkillLabSkillDefinition();
	return false;
}

bool FSkillLabSkillDefinitionProvider::ResolvePresentationBinding(const FName SkillId, FSkillLabSkillPresentationBinding& OutBinding)
{
	EnsureLoaded();
	OutBinding = FSkillLabSkillPresentationBinding();
	if (const FSkillLabSkillDefinition* Found = Definitions.Find(SkillId))
	{
		OutBinding.SkillId = Found->SkillId;
		OutBinding.CastMontage = Found->CastMontage;
		OutBinding.CastAnimation = Found->CastAnimation;
		return true;
	}
	return false;
}

bool FSkillLabSkillDefinitionProvider::ReloadDefinitions()
{
	bLoadAttempted = true;
	TMap<FName, FSkillLabSkillDefinition> Candidates;
	FString CandidateFingerprint;
	if (!LoadCandidate(Candidates, CandidateFingerprint)) return false;
	Definitions = MoveTemp(Candidates);
	AcceptedCatalogSourceFingerprint = MoveTemp(CandidateFingerprint);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabSkills] Definition load accepted: %d skills"), Definitions.Num());
	return true;
}

FString FSkillLabSkillDefinitionProvider::GetAcceptedCatalogSourceFingerprint()
{
	EnsureLoaded();
	return AcceptedCatalogSourceFingerprint;
}

bool FSkillLabSkillDefinitionProvider::ParseCatalogText(const FString& JsonText, const TMap<FString, FString>& LuaSources,
	TMap<FName, FSkillLabSkillDefinition>& OutDefinitions, FString& OutError)
{
	OutError.Reset();
	TMap<FName, FSkillLabSkillDefinition> Candidate;
	if (!ParseText(JsonText, LuaSources, Candidate, OutError)) return false;
	OutDefinitions = MoveTemp(Candidate);
	return true;
}

bool FSkillLabSkillDefinitionProvider::ValidateJsonText(const FString& JsonText, FString& OutError)
{
	TMap<FName, FSkillLabSkillDefinition> Candidate;
	return ParseCatalogText(JsonText, {}, Candidate, OutError);
}

#if WITH_DEV_AUTOMATION_TESTS
bool FSkillLabSkillDefinitionProvider::CopyDefinitionsForAutomation(TMap<FName, FSkillLabSkillDefinition>& OutDefinitions)
{
	OutDefinitions = Definitions;
	return bLoadAttempted;
}

void FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(
	const TMap<FName, FSkillLabSkillDefinition>& InDefinitions, const bool bInLoadAttempted)
{
	Definitions = InDefinitions;
	bLoadAttempted = bInLoadAttempted;
}
#endif
