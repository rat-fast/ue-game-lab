#pragma once

#include "CoreMinimal.h"
#include "SkillLabHUDTypes.generated.h"

USTRUCT()
struct SKILLLAB_API FSkillLabHUDSkillState
{
	GENERATED_BODY()
	FName SkillId = NAME_None;
	int32 SlotIndex = INDEX_NONE;
	float Cost = 0.0f;
	float CooldownRemaining = 0.0f;
};

USTRUCT()
struct SKILLLAB_API FSkillLabHUDSnapshot
{
	GENERATED_BODY()
	float Health = 0.0f;
	float MaxHealth = 0.0f;
	float Energy = 0.0f;
	float MaxEnergy = 0.0f;
	TArray<FSkillLabHUDSkillState> Skills;
};
