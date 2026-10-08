#include "SkillLabSkillDefinitionProvider.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkillLabSkillGameplayAbility.h"

namespace
{
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	bool bLoaded = false;

	bool Parse(const TSharedPtr<FJsonObject>& Json, FSkillLabSkillDefinition& Out)
	{
		FString Text;
		if (!Json.IsValid() || !Json->TryGetStringField(TEXT("id"), Text)) return false;
		Out.SkillId = FName(*Text);
		if (!Json->TryGetNumberField(TEXT("cost"), Out.Cost) || !Json->TryGetNumberField(TEXT("cooldown"), Out.Cooldown)
			|| !Json->TryGetNumberField(TEXT("magnitude"), Out.Magnitude)) return false;
		Json->TryGetNumberField(TEXT("powerCoefficient"), Out.PowerCoefficient);

		// Targeting, delivery, hit rules and timing are code-owned skill behavior.
		if (Out.SkillId == TEXT("Skill_Fireball"))
		{
			Out.TargetShape = ESkillLabTargetShape::Unit;
			Out.Relation = ESkillLabTargetRelation::Enemy;
			Out.Execution = ESkillLabExecutionTemplate::Projectile;
			Out.Effect = ESkillLabEffectKind::Damage;
			Out.Range = 1200.0f;
			Out.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		}
		else if (Out.SkillId == TEXT("Skill_AreaBurst"))
		{
			Out.TargetShape = ESkillLabTargetShape::Ground;
			Out.Relation = ESkillLabTargetRelation::Enemy;
			Out.Execution = ESkillLabExecutionTemplate::GroundArea;
			Out.Effect = ESkillLabEffectKind::Damage;
			Out.Range = 1000.0f;
			Out.Radius = 350.0f;
			Out.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		}
		else if (Out.SkillId == TEXT("Skill_Mend"))
		{
			Out.TargetShape = ESkillLabTargetShape::Unit;
			Out.Relation = ESkillLabTargetRelation::Ally;
			Out.Execution = ESkillLabExecutionTemplate::InstantTarget;
			Out.Effect = ESkillLabEffectKind::Healing;
			Out.Range = 800.0f;
			Out.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		}
		else if (Out.SkillId == TEXT("Skill_BurningBuff"))
		{
			Out.TargetShape = ESkillLabTargetShape::Unit;
			Out.Relation = ESkillLabTargetRelation::Enemy;
			Out.Execution = ESkillLabExecutionTemplate::InstantTarget;
			Out.Effect = ESkillLabEffectKind::DamageOverTime;
			Out.Range = 900.0f;
			Out.Duration = 4.0f;
			Out.Period = 1.0f;
			Out.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		}
		else return false;

		return Out.Cost >= 0.0f && Out.Cooldown >= 0.0f && Out.Range > 0.0f && Out.Magnitude >= 0.0f
			&& (Out.TargetShape != ESkillLabTargetShape::Ground || Out.Radius > 0.0f)
			&& (Out.Effect != ESkillLabEffectKind::DamageOverTime || (Out.Duration > 0.0f && Out.Period > 0.0f));
	}

	void EnsureLoaded()
	{
		if (bLoaded) return;
		bLoaded = true;
		const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SkillDefinitions.json"));
		FString Contents;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Contents, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Contents), Root) || !Root.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("[SkillLabSkills] Failed to load %s"), *Path);
			return;
		}
		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("skills"), Skills)) return;
		for (const TSharedPtr<FJsonValue>& Value : *Skills)
		{
			FSkillLabSkillDefinition Definition;
			if (Value.IsValid() && Parse(Value->AsObject(), Definition) && !Definitions.Contains(Definition.SkillId)) Definitions.Add(Definition.SkillId, Definition);
		}
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
	if (const FSkillLabSkillDefinition* Found = Definitions.Find(SkillId)) { OutDefinition = *Found; return true; }
	OutDefinition = FSkillLabSkillDefinition();
	return false;
}

void FSkillLabSkillDefinitionProvider::ReloadDefinitions()
{
	Definitions.Reset();
	bLoaded = false;
	EnsureLoaded();
}
