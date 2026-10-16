// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Text owned, validated rules used by the native locomotion AnimInstance. */
struct SKILLLAB_API FSkillLabAnimationDefinition
{
	int32 SchemaVersion = 1;
	float MovementThreshold = 0.01f;
	float DirectionClampDegrees = 45.0f;
	float DirectionMin = -180.0f;
	float DirectionMax = 180.0f;
};

class SKILLLAB_API FSkillLabAnimationDefinitionProvider
{
public:
	static const FSkillLabAnimationDefinition& Get();
	static bool Reload(FString* OutError = nullptr);
	static bool ValidateJsonText(const FString& JsonText, FString& OutError);

private:
	static bool LoadCandidate(FSkillLabAnimationDefinition& OutDefinition, FString& OutError);
};
