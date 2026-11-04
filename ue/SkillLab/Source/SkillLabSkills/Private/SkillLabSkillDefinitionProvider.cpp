#include "SkillLabSkillDefinitionProvider.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkillLabLua.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillBehaviorRegistry.h"

namespace
{
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	bool bLoadAttempted = false;
	constexpr int32 MaxLuaSourceLength = 256 * 1024;
	constexpr double MaxContentNumber = 100000.0;

	struct FLuaDefinitionCapture
	{
		TMap<FString, FString> Fields;
		FString Error;
		bool bCalled = false;
	};

	bool Fail(FString& OutError, const FString& Error)
	{
		OutError = Error;
		return false;
	}

	int32 CaptureLuaDefinition(FSkillLabLuaState* State)
	{
		FLuaDefinitionCapture* Capture = static_cast<FLuaDefinitionCapture*>(SkillLabLua::GetCallbackContext(State));
		const int32 ArgumentCount = SkillLabLua::GetTop(State);
		if (!Capture || Capture->bCalled)
		{
			if (Capture) Capture->Error = TEXT("SkillLabSkill_Define may be called exactly once");
			SkillLabLua::PushBoolean(State, false);
			return 1;
		}
		Capture->bCalled = true;
		if (ArgumentCount <= 0 || ArgumentCount % 2 != 0)
		{
			Capture->Error = TEXT("SkillLabSkill_Define expects key/value string pairs");
			SkillLabLua::PushBoolean(State, false);
			return 1;
		}
		for (int32 Index = 1; Index <= ArgumentCount; Index += 2)
		{
			if (!SkillLabLua::IsString(State, Index) || !SkillLabLua::IsString(State, Index + 1))
			{
				Capture->Error = FString::Printf(TEXT("SkillLabSkill_Define argument pair %d must contain strings"), (Index + 1) / 2);
				break;
			}
			const char* KeyUtf8 = SkillLabLua::ToString(State, Index);
			const char* ValueUtf8 = SkillLabLua::ToString(State, Index + 1);
			const FString Key = KeyUtf8 ? UTF8_TO_TCHAR(KeyUtf8) : TEXT("");
			const FString Value = ValueUtf8 ? UTF8_TO_TCHAR(ValueUtf8) : TEXT("");
			if (Key.IsEmpty() || Value.IsEmpty() || Key != Key.TrimStartAndEnd() || Value != Value.TrimStartAndEnd())
			{
				Capture->Error = FString::Printf(TEXT("SkillLabSkill_Define argument pair %d must be nonempty and trimmed"), (Index + 1) / 2);
				break;
			}
			if (Capture->Fields.Contains(Key))
			{
				Capture->Error = FString::Printf(TEXT("duplicate Lua declaration field '%s'"), *Key);
				break;
			}
			Capture->Fields.Add(Key, Value);
		}
		SkillLabLua::PushBoolean(State, Capture->Error.IsEmpty());
		return 1;
	}

	bool ParseLuaDefinition(const FString& Source, const FString& SourceName, FSkillLabSkillDefinition& Out, FString& OutError)
	{
		if (Source.IsEmpty() || Source.Len() > MaxLuaSourceLength)
			return Fail(OutError, FString::Printf(TEXT("%s must contain at most %d characters"), *SourceName, MaxLuaSourceLength));
		FSkillLabLuaState* Lua = SkillLabLua::Create();
		if (!Lua) return Fail(OutError, FString::Printf(TEXT("%s: Lua VM creation failed"), *SourceName));
		SkillLabLua::SetInstructionBudget(Lua, 100000);
		FLuaDefinitionCapture Capture;
		SkillLabLua::PushCallback(Lua, &CaptureLuaDefinition, &Capture);
		SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Define");
		FTCHARToUTF8 Utf8(*Source);
		const bool bLoaded = SkillLabLua::LoadBuffer(Lua, Utf8.Get(), Utf8.Length(), TCHAR_TO_UTF8(*SourceName));
		const bool bExecuted = bLoaded && SkillLabLua::Call(Lua, 0, 0);
		if (!bLoaded || !bExecuted)
		{
			const char* LuaMessage = SkillLabLua::ToString(Lua, -1);
			OutError = FString::Printf(TEXT("%s: Lua declaration evaluation failed: %s"), *SourceName,
				LuaMessage ? UTF8_TO_TCHAR(LuaMessage) : TEXT("unknown Lua error"));
			SkillLabLua::Close(Lua);
			return false;
		}
		SkillLabLua::GetGlobal(Lua, "OnSkillCast");
		const bool bHasRuntimeEntry = SkillLabLua::IsFunction(Lua, -1);
		SkillLabLua::Close(Lua);
		if (!Capture.Error.IsEmpty()) return Fail(OutError, FString::Printf(TEXT("%s: %s"), *SourceName, *Capture.Error));
		if (!Capture.bCalled) return Fail(OutError, FString::Printf(TEXT("%s: missing SkillLabSkill_Define declaration"), *SourceName));
		if (!bHasRuntimeEntry) return Fail(OutError, FString::Printf(TEXT("%s: missing OnSkillCast function"), *SourceName));

		static const TSet<FString> RequiredFields = { TEXT("targetShape"), TEXT("relation"), TEXT("effect"), TEXT("execution") };
		for (const FString& Field : RequiredFields)
			if (!Capture.Fields.Contains(Field)) return Fail(OutError, FString::Printf(TEXT("%s: missing Lua declaration field '%s'"), *SourceName, *Field));
		for (const TPair<FString, FString>& Field : Capture.Fields)
			if (!RequiredFields.Contains(Field.Key)) return Fail(OutError, FString::Printf(TEXT("%s: unknown Lua declaration field '%s'"), *SourceName, *Field.Key));

		const FString& Shape = Capture.Fields.FindChecked(TEXT("targetShape"));
		if (Shape == TEXT("Self")) Out.TargetShape = ESkillLabTargetShape::Self;
		else if (Shape == TEXT("Unit")) Out.TargetShape = ESkillLabTargetShape::Unit;
		else if (Shape == TEXT("Ground")) Out.TargetShape = ESkillLabTargetShape::Ground;
		else return Fail(OutError, FString::Printf(TEXT("%s: unknown targetShape '%s'"), *SourceName, *Shape));

		const FString& Relation = Capture.Fields.FindChecked(TEXT("relation"));
		if (Relation == TEXT("Self")) Out.Relation = ESkillLabTargetRelation::Self;
		else if (Relation == TEXT("Ally")) Out.Relation = ESkillLabTargetRelation::Ally;
		else if (Relation == TEXT("Enemy")) Out.Relation = ESkillLabTargetRelation::Enemy;
		else if (Relation == TEXT("Any")) Out.Relation = ESkillLabTargetRelation::Any;
		else return Fail(OutError, FString::Printf(TEXT("%s: unknown relation '%s'"), *SourceName, *Relation));

		const FString& Effect = Capture.Fields.FindChecked(TEXT("effect"));
		if (Effect == TEXT("Damage")) Out.Effect = ESkillLabEffectKind::Damage;
		else if (Effect == TEXT("Healing")) Out.Effect = ESkillLabEffectKind::Healing;
		else if (Effect == TEXT("DamageOverTime")) Out.Effect = ESkillLabEffectKind::DamageOverTime;
		else return Fail(OutError, FString::Printf(TEXT("%s: unknown effect '%s'"), *SourceName, *Effect));

		const FString& Execution = Capture.Fields.FindChecked(TEXT("execution"));
		if (Execution == TEXT("InstantTarget")) Out.Execution = ESkillLabExecutionTemplate::InstantTarget;
		else if (Execution == TEXT("Projectile")) Out.Execution = ESkillLabExecutionTemplate::Projectile;
		else if (Execution == TEXT("GroundArea")) Out.Execution = ESkillLabExecutionTemplate::GroundArea;
		else return Fail(OutError, FString::Printf(TEXT("%s: unknown execution '%s'"), *SourceName, *Execution));
		return true;
	}

	bool ReadNumber(const FJsonObject& Json, const TCHAR* Field, float& OutNumber, const bool bRequired, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
		if (!Value.IsValid()) return !bRequired || Fail(OutError, FString::Printf(TEXT("required number '%s' is missing"), Field));
		double Number = 0.0;
		if (Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number)
			|| FMath::Abs(Number) > MaxContentNumber)
			return Fail(OutError, FString::Printf(TEXT("'%s' must be a finite number in [-%.0f, %.0f]"), Field, MaxContentNumber, MaxContentNumber));
		OutNumber = static_cast<float>(Number);
		return true;
	}

	bool ReadBoolean(const FJsonObject& Json, const TCHAR* Field, bool& OutBoolean, const bool bRequired, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
		if (!Value.IsValid()) return !bRequired || Fail(OutError, FString::Printf(TEXT("required boolean '%s' is missing"), Field));
		if (Value->Type != EJson::Boolean || !Value->TryGetBool(OutBoolean))
			return Fail(OutError, FString::Printf(TEXT("'%s' must be a boolean"), Field));
		return true;
	}

	bool ReadString(const FJsonObject& Json, const TCHAR* Field, FString& OutString, const bool bRequired, FString& OutError)
	{
		const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
		if (!Value.IsValid()) return !bRequired || Fail(OutError, FString::Printf(TEXT("required string '%s' is missing"), Field));
		if (Value->Type != EJson::String || !Value->TryGetString(OutString) || OutString.IsEmpty() || OutString != OutString.TrimStartAndEnd())
			return Fail(OutError, FString::Printf(TEXT("'%s' must be a nonempty trimmed string"), Field));
		return true;
	}

	bool Parse(const TSharedPtr<FJsonObject>& Json, const TMap<FString, FString>& LuaSources,
		FSkillLabSkillDefinition& Out, FString& OutError)
	{
		if (!Json.IsValid()) return Fail(OutError, TEXT("skill entry must be an object"));
		static const TSet<FString> KnownFields = {
			TEXT("id"), TEXT("cost"), TEXT("cooldown"), TEXT("magnitude"), TEXT("powerCoefficient"), TEXT("range"), TEXT("radius"),
			TEXT("projectileSpeed"), TEXT("projectileCollisionRadius"), TEXT("projectileLifetime"), TEXT("projectileVisualScale"),
			TEXT("projectileInterceptOtherUnits"), TEXT("windup"), TEXT("active"), TEXT("recovery"), TEXT("duration"), TEXT("period"),
			TEXT("blockedBy"), TEXT("luaScript"), TEXT("castMontage"), TEXT("castAnimation"), TEXT("releaseTrigger"),
			TEXT("releaseEventTag"), TEXT("releaseEventTimeout")
		};
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Json->Values)
			if (!KnownFields.Contains(Field.Key)) return Fail(OutError, FString::Printf(TEXT("unknown JSON field '%s'"), *Field.Key));

		FString Id;
		if (!ReadString(*Json, TEXT("id"), Id, true, OutError)) return false;
		Out.SkillId = FName(*Id);
		if (Out.SkillId.IsNone()) return Fail(OutError, TEXT("id cannot be None"));
		if (!ReadString(*Json, TEXT("luaScript"), Out.LuaScript, true, OutError)) return false;
		FString NormalizedLuaPath = Out.LuaScript;
		FPaths::NormalizeFilename(NormalizedLuaPath);
		if (!FPaths::IsRelative(NormalizedLuaPath) || NormalizedLuaPath.Contains(TEXT("..")) || NormalizedLuaPath.Contains(TEXT(":")))
			return Fail(OutError, TEXT("luaScript must be a relative path below Config"));
		const FString* LuaSource = LuaSources.Find(Out.LuaScript);
		FString LoadedLuaSource;
		if (!LuaSource)
		{
			const FString LuaPath = FPaths::Combine(FPaths::ProjectConfigDir(), NormalizedLuaPath);
			if (!FFileHelper::LoadFileToString(LoadedLuaSource, *LuaPath))
				return Fail(OutError, FString::Printf(TEXT("cannot read Lua declaration source '%s'"), *LuaPath));
			LuaSource = &LoadedLuaSource;
		}
		if (!ParseLuaDefinition(*LuaSource, Out.LuaScript, Out, OutError)) return false;
		Out.LuaSource = *LuaSource;

		const TSharedPtr<FJsonValue> MontageValue = Json->TryGetField(TEXT("castMontage"));
		if (MontageValue.IsValid())
		{
			FString AssetPath;
			if (MontageValue->Type != EJson::String || !MontageValue->TryGetString(AssetPath) || AssetPath.IsEmpty())
				return Fail(OutError, TEXT("castMontage must be a nonempty soft asset path"));
			Out.CastMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(AssetPath));
		}
		const TSharedPtr<FJsonValue> AnimationValue = Json->TryGetField(TEXT("castAnimation"));
		if (AnimationValue.IsValid())
		{
			FString AssetPath;
			if (AnimationValue->Type != EJson::String || !AnimationValue->TryGetString(AssetPath) || AssetPath.IsEmpty())
				return Fail(OutError, TEXT("castAnimation must be a nonempty soft asset path"));
			Out.CastAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(AssetPath));
		}
		if (!Out.CastMontage.IsNull() && !Out.CastAnimation.IsNull())
			return Fail(OutError, TEXT("castMontage and castAnimation are mutually exclusive"));

		if (!ReadNumber(*Json, TEXT("cost"), Out.Cost, true, OutError)
			|| !ReadNumber(*Json, TEXT("cooldown"), Out.Cooldown, true, OutError)
			|| !ReadNumber(*Json, TEXT("magnitude"), Out.Magnitude, true, OutError)
			|| !ReadNumber(*Json, TEXT("powerCoefficient"), Out.PowerCoefficient, false, OutError)
			|| !ReadNumber(*Json, TEXT("range"), Out.Range, true, OutError)
			|| !ReadNumber(*Json, TEXT("radius"), Out.Radius, Out.TargetShape == ESkillLabTargetShape::Ground, OutError)
			|| !ReadNumber(*Json, TEXT("windup"), Out.Phases.WindupSeconds, true, OutError)
			|| !ReadNumber(*Json, TEXT("active"), Out.Phases.ActiveSeconds, false, OutError)
			|| !ReadNumber(*Json, TEXT("recovery"), Out.Phases.RecoverySeconds, true, OutError)
			|| !ReadNumber(*Json, TEXT("duration"), Out.Duration, Out.Effect == ESkillLabEffectKind::DamageOverTime, OutError)
			|| !ReadNumber(*Json, TEXT("period"), Out.Period, Out.Effect == ESkillLabEffectKind::DamageOverTime, OutError)) return false;

		if (Out.Execution == ESkillLabExecutionTemplate::Projectile)
		{
			if (!ReadNumber(*Json, TEXT("projectileSpeed"), Out.ProjectileParameters.Speed, true, OutError)
				|| !ReadNumber(*Json, TEXT("projectileCollisionRadius"), Out.ProjectileParameters.CollisionRadius, true, OutError)
				|| !ReadNumber(*Json, TEXT("projectileLifetime"), Out.ProjectileParameters.Lifetime, true, OutError)
				|| !ReadNumber(*Json, TEXT("projectileVisualScale"), Out.ProjectileParameters.VisualScale, true, OutError)
				|| !ReadBoolean(*Json, TEXT("projectileInterceptOtherUnits"), Out.ProjectileParameters.bInterceptOtherUnits, true, OutError)) return false;
		}
		else if (Json->HasField(TEXT("projectileSpeed")) || Json->HasField(TEXT("projectileCollisionRadius"))
			|| Json->HasField(TEXT("projectileLifetime")) || Json->HasField(TEXT("projectileVisualScale"))
			|| Json->HasField(TEXT("projectileInterceptOtherUnits")))
			return Fail(OutError, TEXT("projectile tuning is only valid for Projectile execution"));

		FString ReleaseTrigger;
		if (!ReadString(*Json, TEXT("releaseTrigger"), ReleaseTrigger, false, OutError)) return false;
		if (!ReleaseTrigger.IsEmpty())
		{
			if (ReleaseTrigger == TEXT("Timer")) Out.ReleaseTrigger = ESkillLabReleaseTrigger::Timer;
			else if (ReleaseTrigger == TEXT("GameplayEvent")) Out.ReleaseTrigger = ESkillLabReleaseTrigger::GameplayEvent;
			else return Fail(OutError, TEXT("releaseTrigger must be 'Timer' or 'GameplayEvent'"));
		}
		FString ReleaseEventTag;
		if (!ReadString(*Json, TEXT("releaseEventTag"), ReleaseEventTag, false, OutError)) return false;
		if (!ReleaseEventTag.IsEmpty())
		{
			Out.ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(*ReleaseEventTag), false);
			if (!Out.ReleaseEventTag.IsValid()) return Fail(OutError, FString::Printf(TEXT("unknown GameplayTag '%s' in releaseEventTag"), *ReleaseEventTag));
		}
		if (!ReadNumber(*Json, TEXT("releaseEventTimeout"), Out.ReleaseEventTimeout, false, OutError)) return false;

		const TSharedPtr<FJsonValue> BlockedByValue = Json->TryGetField(TEXT("blockedBy"));
		if (BlockedByValue.IsValid())
		{
			if (BlockedByValue->Type != EJson::Array) return Fail(OutError, TEXT("blockedBy must be an array of registered tag names"));
			for (const TSharedPtr<FJsonValue>& Value : BlockedByValue->AsArray())
			{
				FString TagName;
				if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(TagName) || TagName.IsEmpty())
					return Fail(OutError, TEXT("blockedBy entries must be nonempty strings"));
				const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
				if (!Tag.IsValid()) return Fail(OutError, FString::Printf(TEXT("unknown GameplayTag '%s'"), *TagName));
				Out.BlockedByTags.AddTag(Tag);
			}
		}

		if (!Out.AbilityClass || !Out.BehaviorClass)
			return Fail(OutError, TEXT("shared runtime classes were not assigned"));
		if (Out.Cost < 0.0f || Out.Cooldown < 0.0f || Out.Magnitude < 0.0f || Out.PowerCoefficient < 0.0f
			|| Out.Range <= 0.0f || Out.Radius < 0.0f || Out.Phases.WindupSeconds < 0.0f
			|| Out.Phases.ActiveSeconds < 0.0f || Out.Phases.RecoverySeconds < 0.0f
			|| Out.Duration < 0.0f || Out.Period < 0.0f)
			return Fail(OutError, TEXT("JSON numbers must be nonnegative and range must be positive"));
		if ((Out.TargetShape == ESkillLabTargetShape::Self) != (Out.Relation == ESkillLabTargetRelation::Self))
			return Fail(OutError, TEXT("Self target shape and Self relation must be declared together"));
		if ((Out.Execution == ESkillLabExecutionTemplate::GroundArea) != (Out.TargetShape == ESkillLabTargetShape::Ground))
			return Fail(OutError, TEXT("GroundArea execution requires Ground target shape, and Ground requires GroundArea"));
		if (Out.TargetShape == ESkillLabTargetShape::Ground && Out.Radius <= 0.0f)
			return Fail(OutError, TEXT("Ground definitions require a positive JSON radius"));
		if (Out.Execution == ESkillLabExecutionTemplate::Projectile && Out.TargetShape != ESkillLabTargetShape::Unit)
			return Fail(OutError, TEXT("Projectile execution requires Unit target shape"));
		if (Out.Execution == ESkillLabExecutionTemplate::InstantTarget && Out.TargetShape == ESkillLabTargetShape::Ground)
			return Fail(OutError, TEXT("InstantTarget execution cannot use Ground target shape"));
		if (Out.Effect == ESkillLabEffectKind::DamageOverTime && (Out.Duration <= 0.0f || Out.Period <= 0.0f))
			return Fail(OutError, TEXT("DamageOverTime requires positive JSON duration and period"));
		if (Out.Execution == ESkillLabExecutionTemplate::Projectile
			&& (Out.ProjectileParameters.Speed <= 0.0f || Out.ProjectileParameters.CollisionRadius <= 0.0f
				|| Out.ProjectileParameters.Lifetime <= 0.0f || Out.ProjectileParameters.VisualScale <= 0.0f))
			return Fail(OutError, TEXT("Projectile JSON tuning values must be positive"));
		if (!FMath::IsFinite(Out.ReleaseEventTimeout)
			|| (Out.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent
				&& (!Out.ReleaseEventTag.IsValid() || Out.ReleaseEventTimeout <= 0.0f))
			|| (Out.ReleaseTrigger == ESkillLabReleaseTrigger::Timer && Out.ReleaseEventTag.IsValid()))
			return Fail(OutError, TEXT("release trigger, tag, and timeout combination is invalid"));
		return true;
	}

	bool ParseCatalog(const TSharedPtr<FJsonObject>& Root, const TMap<FString, FString>& LuaSources,
		TMap<FName, FSkillLabSkillDefinition>& OutCandidates, FString& OutError)
	{
		if (!Root.IsValid()) return Fail(OutError, TEXT("root must be an object"));
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Root->Values)
			if (Field.Key != TEXT("skills")) return Fail(OutError, FString::Printf(TEXT("unknown catalog field '%s'"), *Field.Key));
		const TArray<TSharedPtr<FJsonValue>>* Skills = nullptr;
		if (!Root->TryGetArrayField(TEXT("skills"), Skills) || !Skills || Skills->IsEmpty())
			return Fail(OutError, TEXT("skills must be a nonempty array"));
		TMap<FName, FSkillLabSkillDefinition> Candidate;
		for (int32 Index = 0; Index < Skills->Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& Value = (*Skills)[Index];
			FSkillLabSkillDefinition Definition;
			FSkillLabSkillBehaviorRegistry::ApplySharedRuntimeClasses(Definition);
			if (!Value.IsValid() || Value->Type != EJson::Object
				|| !Parse(Value->AsObject(), LuaSources, Definition, OutError))
			{
				if (OutError.IsEmpty()) OutError = FString::Printf(TEXT("invalid skill entry at skills[%d]"), Index);
				OutError = FString::Printf(TEXT("skills[%d]: %s"), Index, *OutError);
				return false;
			}
			if (Candidate.Contains(Definition.SkillId))
				return Fail(OutError, FString::Printf(TEXT("duplicate skill id '%s'"), *Definition.SkillId.ToString()));
			Candidate.Add(Definition.SkillId, Definition);
		}
		OutCandidates = MoveTemp(Candidate);
		return true;
	}

	bool ParseText(const FString& JsonText, const TMap<FString, FString>& LuaSources,
		TMap<FName, FSkillLabSkillDefinition>& OutCandidates, FString& OutError)
	{
		// DOM 会覆盖同名字段；先用引擎 token reader 检查每个对象的字段唯一性。
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
			if (Notation == EJsonNotation::Error) return Fail(OutError, TEXT("invalid JSON"));
			if (Notation == EJsonNotation::ObjectEnd || Notation == EJsonNotation::ArrayEnd)
			{
				if (!Scopes.IsEmpty()) Scopes.Pop();
				continue;
			}
			if (!Scopes.IsEmpty() && Scopes.Last().bObject)
			{
				const FString& Field = Reader->GetIdentifier();
				if (Scopes.Last().Fields.Contains(Field))
					return Fail(OutError, FString::Printf(TEXT("duplicate JSON field '%s' at line %d"), *Field, Reader->GetLineNumber()));
				Scopes.Last().Fields.Add(Field);
			}
			if (Notation == EJsonNotation::ObjectStart || Notation == EJsonNotation::ArrayStart)
				Scopes.Add({ Notation == EJsonNotation::ObjectStart, {} });
		}
		if (!Reader->GetErrorMessage().IsEmpty()) return Fail(OutError, TEXT("invalid JSON"));
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(JsonText), Root))
			return Fail(OutError, TEXT("invalid JSON"));
		return ParseCatalog(Root, LuaSources, OutCandidates, OutError);
	}

	bool LoadCandidate(TMap<FName, FSkillLabSkillDefinition>& OutCandidates)
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
	if (!LoadCandidate(Candidates)) return false;
	Definitions = MoveTemp(Candidates);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabSkills] Definition load accepted: %d skills"), Definitions.Num());
	return true;
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
