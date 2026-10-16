// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabAnimationDefinitionProvider.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FSkillLabAnimationDefinition CachedDefinition;
	bool bLoadAttempted = false;

	bool Fail(FString& OutError, const FString& Error)
	{
		OutError = Error;
		return false;
	}

	bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutValue, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Value = Object->TryGetField(Field);
		double Number = 0.0;
		if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(Number)
			|| !FMath::IsFinite(Number) || FMath::Abs(Number) > static_cast<double>(MAX_flt))
		{
			return Fail(OutError, FString::Printf(TEXT("locomotion.%s must be a finite number"), Field));
		}
		OutValue = static_cast<float>(Number);
		return true;
	}

	bool Parse(const TSharedPtr<FJsonObject>& Root, FSkillLabAnimationDefinition& OutDefinition, FString& OutError)
	{
		if (!Root.IsValid()) return Fail(OutError, TEXT("root must be an object"));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Root->Values)
		{
			if (Field.Key != TEXT("schemaVersion") && Field.Key != TEXT("locomotion"))
				return Fail(OutError, FString::Printf(TEXT("unknown root field '%s'"), *Field.Key));
		}

		int32 SchemaVersion = 0;
		const TSharedPtr<FJsonValue> Schema = Root->TryGetField(TEXT("schemaVersion"));
		if (!Schema.IsValid() || Schema->Type != EJson::Number || !Schema->TryGetNumber(SchemaVersion) || SchemaVersion != 1)
			return Fail(OutError, TEXT("schemaVersion must be 1"));

		const TSharedPtr<FJsonObject>* Locomotion = nullptr;
		if (!Root->TryGetObjectField(TEXT("locomotion"), Locomotion) || !Locomotion || !Locomotion->IsValid())
			return Fail(OutError, TEXT("locomotion must be an object"));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Locomotion)->Values)
		{
			if (Field.Key != TEXT("movementThreshold") && Field.Key != TEXT("directionClampDegrees")
				&& Field.Key != TEXT("directionMin") && Field.Key != TEXT("directionMax"))
				return Fail(OutError, FString::Printf(TEXT("unknown locomotion field '%s'"), *Field.Key));
		}

		OutDefinition.SchemaVersion = SchemaVersion;
		if (!ReadNumber(*Locomotion, TEXT("movementThreshold"), OutDefinition.MovementThreshold, OutError)
			|| !ReadNumber(*Locomotion, TEXT("directionClampDegrees"), OutDefinition.DirectionClampDegrees, OutError)
			|| !ReadNumber(*Locomotion, TEXT("directionMin"), OutDefinition.DirectionMin, OutError)
			|| !ReadNumber(*Locomotion, TEXT("directionMax"), OutDefinition.DirectionMax, OutError)) return false;
		if (OutDefinition.MovementThreshold < 0.0f || OutDefinition.DirectionClampDegrees <= 0.0f
			|| OutDefinition.DirectionClampDegrees > 180.0f || OutDefinition.DirectionMin >= OutDefinition.DirectionMax
			|| OutDefinition.DirectionMin < -180.0f || OutDefinition.DirectionMax > 180.0f)
			return Fail(OutError, TEXT("locomotion values are outside the supported range"));
		return true;
	}
}

const FSkillLabAnimationDefinition& FSkillLabAnimationDefinitionProvider::Get()
{
	if (!bLoadAttempted) Reload();
	return CachedDefinition;
}

bool FSkillLabAnimationDefinitionProvider::Reload(FString* OutError)
{
	bLoadAttempted = true;
	FSkillLabAnimationDefinition Candidate;
	FString Error;
	if (!LoadCandidate(Candidate, Error))
	{
		if (OutError) *OutError = Error;
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Animation definition rejected; previous definition retained: %s"), *Error);
		return false;
	}
	CachedDefinition = Candidate;
	if (OutError) OutError->Reset();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimation] Animation definition accepted: threshold=%.4f clamp=%.2f range=[%.2f,%.2f]"),
		CachedDefinition.MovementThreshold, CachedDefinition.DirectionClampDegrees,
		CachedDefinition.DirectionMin, CachedDefinition.DirectionMax);
	return true;
}

bool FSkillLabAnimationDefinitionProvider::ValidateJsonText(const FString& JsonText, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root)) return Fail(OutError, TEXT("invalid JSON"));
	FSkillLabAnimationDefinition Candidate;
	return Parse(Root, Candidate, OutError);
}

bool FSkillLabAnimationDefinitionProvider::LoadCandidate(FSkillLabAnimationDefinition& OutDefinition, FString& OutError)
{
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("AnimationDefinitions.json"));
	FString Contents;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Contents, *Path)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Root))
		return Fail(OutError, FString::Printf(TEXT("cannot read or parse %s"), *Path));
	return Parse(Root, OutDefinition, OutError);
}
