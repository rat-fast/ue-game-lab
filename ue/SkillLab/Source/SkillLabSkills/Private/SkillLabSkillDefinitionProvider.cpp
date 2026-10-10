#include "SkillLabSkillDefinitionProvider.h"

#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkillLabSkillBehaviorRegistry.h"
#include "SkillLabSkillBehavior.h"
#include "Abilities/GameplayAbility.h"

namespace
{
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	bool bLoadAttempted = false;

	bool Fail(FString& OutError, const FString& Error)
	{
		OutError = Error;
		return false;
	}

	bool ReadNumber(const FJsonObject& Json, const TCHAR* Field, float& OutNumber, const bool bRequired, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
		if (!Value.IsValid()) return !bRequired || Fail(OutError, FString::Printf(TEXT("required number '%s' is missing"), Field));
		double Number = 0.0;
		if (Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number) || FMath::Abs(Number) > static_cast<double>(MAX_flt))
			return Fail(OutError, FString::Printf(TEXT("'%s' must be a finite numeric value"), Field));
		OutNumber = static_cast<float>(Number);
		return true;
	}

	bool Parse(const TSharedPtr<FJsonObject>& Json, FSkillLabSkillDefinition& Out, FString& OutError)
	{
		if (!Json.IsValid()) return Fail(OutError, TEXT("skill entry must be an object"));
		static const TSet<FString> KnownFields = {
			TEXT("id"), TEXT("cost"), TEXT("cooldown"), TEXT("magnitude"), TEXT("powerCoefficient"),
			TEXT("windup"), TEXT("active"), TEXT("recovery"), TEXT("duration"), TEXT("period"), TEXT("blockedBy")
		};
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Json->Values)
			if (!KnownFields.Contains(Field.Key)) return Fail(OutError, FString::Printf(TEXT("unknown skill field '%s'"), *Field.Key));

		FString Id;
		const TSharedPtr<FJsonValue> IdValue = Json->TryGetField(TEXT("id"));
		if (!IdValue.IsValid() || IdValue->Type != EJson::String || !IdValue->TryGetString(Id) || Id.IsEmpty() || Id != Id.TrimStartAndEnd())
			return Fail(OutError, TEXT("id must be a nonempty string without surrounding whitespace"));
		Out.SkillId = FName(*Id);
		if (Out.SkillId.IsNone()) return Fail(OutError, TEXT("id cannot be None"));
		// Behavior and targeting are owned by skill code. The text source overrides only content values.
		if (!FSkillLabSkillBehaviorRegistry::Apply(Out.SkillId, Out)) return Fail(OutError, TEXT("no C++ behavior registered for this id"));
		if (!ReadNumber(*Json, TEXT("cost"), Out.Cost, true, OutError) || !ReadNumber(*Json, TEXT("cooldown"), Out.Cooldown, true, OutError)
			|| !ReadNumber(*Json, TEXT("magnitude"), Out.Magnitude, true, OutError) || !ReadNumber(*Json, TEXT("powerCoefficient"), Out.PowerCoefficient, false, OutError)
			|| !ReadNumber(*Json, TEXT("windup"), Out.Phases.WindupSeconds, false, OutError) || !ReadNumber(*Json, TEXT("active"), Out.Phases.ActiveSeconds, false, OutError)
			|| !ReadNumber(*Json, TEXT("recovery"), Out.Phases.RecoverySeconds, false, OutError) || !ReadNumber(*Json, TEXT("duration"), Out.Duration, false, OutError)
			|| !ReadNumber(*Json, TEXT("period"), Out.Period, false, OutError)) return false;

		const TSharedPtr<FJsonValue> BlockedByValue = Json->TryGetField(TEXT("blockedBy"));
		if (BlockedByValue.IsValid())
		{
			if (BlockedByValue->Type != EJson::Array) return Fail(OutError, TEXT("blockedBy must be an array of registered tag names"));
			Out.BlockedByTags.Reset();
			for (const TSharedPtr<FJsonValue>& Value : BlockedByValue->AsArray())
			{
				FString TagName;
				if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(TagName) || TagName.IsEmpty()) return Fail(OutError, TEXT("blockedBy entries must be nonempty strings"));
				const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
				if (!Tag.IsValid()) return Fail(OutError, FString::Printf(TEXT("unknown GameplayTag '%s'"), *TagName));
				Out.BlockedByTags.AddTag(Tag);
			}
		}

		if (!Out.AbilityClass || !Out.BehaviorClass) return Fail(OutError, TEXT("registered skill must declare AbilityClass and BehaviorClass"));
		if (!FMath::IsFinite(Out.Cost) || !FMath::IsFinite(Out.Cooldown) || !FMath::IsFinite(Out.Magnitude) || !FMath::IsFinite(Out.PowerCoefficient)
			|| !FMath::IsFinite(Out.Range) || !FMath::IsFinite(Out.Radius) || !FMath::IsFinite(Out.Duration) || !FMath::IsFinite(Out.Period)
			|| !FMath::IsFinite(Out.Phases.WindupSeconds) || !FMath::IsFinite(Out.Phases.ActiveSeconds) || !FMath::IsFinite(Out.Phases.RecoverySeconds)) return Fail(OutError, TEXT("skill contains nonfinite C++ defaults"));
		if (Out.Cost < 0.0f || Out.Cooldown < 0.0f || Out.Magnitude < 0.0f || Out.Range <= 0.0f || Out.Radius < 0.0f
			|| Out.Phases.WindupSeconds < 0.0f || Out.Phases.ActiveSeconds < 0.0f || Out.Phases.RecoverySeconds < 0.0f
			|| Out.Duration < 0.0f || Out.Period < 0.0f) return Fail(OutError, TEXT("skill contains a negative duration/value or nonpositive range"));
		if (Out.TargetShape == ESkillLabTargetShape::Ground && Out.Radius <= 0.0f) return Fail(OutError, TEXT("ground skills require a positive radius"));
		if (Out.Effect == ESkillLabEffectKind::DamageOverTime && (Out.Duration <= 0.0f || Out.Period <= 0.0f)) return Fail(OutError, TEXT("periodic effects require positive duration and period"));
		if (Out.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
			&& (!Out.ReleaseEventTag.IsValid() || !FMath::IsFinite(Out.ReleaseEventTimeout) || Out.ReleaseEventTimeout <= 0.0f))
			return Fail(OutError, TEXT("event release requires a registered ReleaseEventTag and positive ReleaseEventTimeout"));
		return true;
	}

	bool LoadCandidate(TMap<FName, FSkillLabSkillDefinition>& OutCandidates)
	{
		FSkillLabSkillBehaviorRegistry::RegisterBuiltInBehaviors();
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SkillDefinitions.json"));
		FString Contents;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Contents, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Definition load rejected: cannot read/parse %s; previous definitions retained"), *Path); return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("skills"), Skills) || !Skills || Skills->IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Definition load rejected: skills must be a nonempty array; previous definitions retained")); return false;
		}
		for (int32 Index = 0; Index < Skills->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Skills)[Index];
			FSkillLabSkillDefinition Definition;
			FString Error;
			if (!Value.IsValid() || Value->Type != EJson::Object || !Parse(Value->AsObject(), Definition, Error))
			{
				UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Definition load rejected at skills[%d]: %s; previous definitions retained"), Index, Error.IsEmpty() ? TEXT("invalid object") : *Error); return false;
			}
			if (OutCandidates.Contains(Definition.SkillId))
			{
				UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Definition load rejected: duplicate id '%s'; previous definitions retained"), *Definition.SkillId.ToString()); return false;
			}
			OutCandidates.Add(Definition.SkillId, Definition);
		}
		return true;
	}

	bool ParseCatalog(const TSharedPtr<FJsonObject>& Root, TMap<FName, FSkillLabSkillDefinition>& OutCandidates, FString& OutError)
	{
		if (!Root.IsValid()) return Fail(OutError, TEXT("root must be an object"));
		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("skills"), Skills) || !Skills || Skills->IsEmpty())
			return Fail(OutError, TEXT("skills must be a nonempty array"));
		for (int32 Index = 0; Index < Skills->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Skills)[Index];
			FSkillLabSkillDefinition Definition;
			if (!Value.IsValid() || Value->Type != EJson::Object || !Parse(Value->AsObject(), Definition, OutError))
			{
				if (OutError.IsEmpty()) OutError = FString::Printf(TEXT("invalid skill entry at index %d"), Index);
				return false;
			}
			if (OutCandidates.Contains(Definition.SkillId)) return Fail(OutError, FString::Printf(TEXT("duplicate id '%s'"), *Definition.SkillId.ToString()));
			OutCandidates.Add(Definition.SkillId, Definition);
		}
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
		return true;
	}
	return false;
}

bool FSkillLabSkillDefinitionProvider::ReloadDefinitions()
{
	bLoadAttempted = true;
	TMap<FName, FSkillLabSkillDefinition> Candidates;
	if (!LoadCandidate(Candidates)) return false;
	// A rejected candidate never mutates the catalog used by existing callers.
	Definitions = MoveTemp(Candidates);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabSkills] Definition load accepted: %d skills"), Definitions.Num());
	return true;
}

bool FSkillLabSkillDefinitionProvider::ValidateJsonText(const FString& JsonText, FString& OutError)
{
	OutError.Reset();
	FSkillLabSkillBehaviorRegistry::RegisterBuiltInBehaviors();
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root)) return Fail(OutError, TEXT("invalid JSON"));
	TMap<FName, FSkillLabSkillDefinition> Candidates;
	return ParseCatalog(Root, Candidates, OutError);
}
