#include "SkillLabSkillDefinitionParsing.h"

#include "Abilities/GameplayAbility.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Dom/JsonObject.h"
#include "GameplayTagContainer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SkillLabSkillBehavior.h"
#include "UObject/SoftObjectPath.h"

namespace SkillLabSkillDefinitionParsing
{
	namespace
	{
		constexpr double MaxContentNumber = 100000.0;

		bool Fail(FString& Error, const FString& Message)
		{
			Error = Message;
			return false;
		}

		bool ReadNumber(const FJsonObject& Json, const TCHAR* Field, float& Out, const bool bRequired, FString& Error)
		{
			const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
			if (!Value.IsValid()) return !bRequired || Fail(Error, FString::Printf(TEXT("required number '%s' is missing"), Field));
			double Number = 0.0;
			if (Value->Type != EJson::Number || !Value->TryGetNumber(Number) || !FMath::IsFinite(Number) || FMath::Abs(Number) > MaxContentNumber)
				return Fail(Error, FString::Printf(TEXT("'%s' must be a finite number in [-%.0f, %.0f]"), Field, MaxContentNumber, MaxContentNumber));
			Out = static_cast<float>(Number);
			return true;
		}

		bool ReadBoolean(const FJsonObject& Json, const TCHAR* Field, bool& Out, const bool bRequired, FString& Error)
		{
			const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
			if (!Value.IsValid()) return !bRequired || Fail(Error, FString::Printf(TEXT("required boolean '%s' is missing"), Field));
			if (Value->Type != EJson::Boolean || !Value->TryGetBool(Out)) return Fail(Error, FString::Printf(TEXT("'%s' must be a boolean"), Field));
			return true;
		}

		bool ReadString(const FJsonObject& Json, const TCHAR* Field, FString& Out, const bool bRequired, FString& Error)
		{
			const TSharedPtr<FJsonValue> Value = Json.TryGetField(Field);
			if (!Value.IsValid()) return !bRequired || Fail(Error, FString::Printf(TEXT("required string '%s' is missing"), Field));
			if (Value->Type != EJson::String || !Value->TryGetString(Out) || Out.IsEmpty() || Out != Out.TrimStartAndEnd())
				return Fail(Error, FString::Printf(TEXT("'%s' must be a nonempty trimmed string"), Field));
			return true;
		}
	}

	bool ParseSkillEntry(const TSharedPtr<FJsonObject>& Json, const TMap<FString, FString>& LuaSources,
		FSkillLabSkillDefinition& Out, FString& Error)
	{
		if (!Json.IsValid()) return Fail(Error, TEXT("skill entry must be an object"));
		static const TSet<FString> KnownFields = {
			TEXT("id"), TEXT("cost"), TEXT("cooldown"), TEXT("magnitude"), TEXT("powerCoefficient"), TEXT("range"), TEXT("radius"),
			TEXT("projectileSpeed"), TEXT("projectileCollisionRadius"), TEXT("projectileLifetime"), TEXT("projectileVisualScale"),
			TEXT("projectileInterceptOtherUnits"), TEXT("windup"), TEXT("active"), TEXT("recovery"), TEXT("duration"), TEXT("period"),
			TEXT("blockedBy"), TEXT("interruptBy"), TEXT("luaScript"), TEXT("castMontage"), TEXT("castAnimation"), TEXT("releaseTrigger"),
			TEXT("releaseEventTag"), TEXT("releaseEventTimeout"), TEXT("ruleWindows"), TEXT("completionEventTag"),
			TEXT("submissionPoints"), TEXT("completionMode"), TEXT("spatial")
		};
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Json->Values)
			if (!KnownFields.Contains(Pair.Key)) return Fail(Error, FString::Printf(TEXT("unknown JSON field '%s'"), *Pair.Key));
		FString Id;
		if (!ReadString(*Json, TEXT("id"), Id, true, Error)) return false;
		Out.SkillId = FName(*Id);
		if (Out.SkillId.IsNone()) return Fail(Error, TEXT("id cannot be None"));
		if (!ReadString(*Json, TEXT("luaScript"), Out.LuaScript, true, Error)) return false;
		FString NormalizedLuaPath = Out.LuaScript;
		FPaths::NormalizeFilename(NormalizedLuaPath);
		if (!FPaths::IsRelative(NormalizedLuaPath) || NormalizedLuaPath.Contains(TEXT("..")) || NormalizedLuaPath.Contains(TEXT(":")))
			return Fail(Error, TEXT("luaScript must be a relative path below Config"));
		const FString* LuaSource = LuaSources.Find(Out.LuaScript);
		FString LoadedLuaSource;
		if (!LuaSource)
		{
			const FString LuaPath = FPaths::Combine(FPaths::ProjectConfigDir(), NormalizedLuaPath);
			if (!FFileHelper::LoadFileToString(LoadedLuaSource, *LuaPath)) return Fail(Error, FString::Printf(TEXT("cannot read Lua declaration source '%s'"), *LuaPath));
			LuaSource = &LoadedLuaSource;
		}
		if (!ParseLuaDeclaration(*LuaSource, Out.LuaScript, Out, Error)) return false;
		Out.RecoveryRules.Dodge = ESkillLabRuleDecision::Buffer;
		Out.LuaSource = *LuaSource;

		const TSharedPtr<FJsonObject>* Spatial = nullptr;
		if (Json->HasField(TEXT("spatial")) && (!Json->TryGetObjectField(TEXT("spatial"), Spatial) || !Spatial || !Spatial->IsValid()))
			return Fail(Error, TEXT("spatial must be an object keyed by Lua object name"));
		if (Spatial)
		{
			for (const auto& Pair : (*Spatial)->Values)
			{
				auto* D = Out.SpatialDefinitions.FindByPredicate([&](const auto& Entry) { return Entry.Name == FName(*Pair.Key); });
				if (!D || !Pair.Value.IsValid() || Pair.Value->Type != EJson::Object)
					return Fail(Error, FString::Printf(TEXT("spatial.%s has no Lua declaration or is not an object"), *Pair.Key));
				const auto Obj = Pair.Value->AsObject();
				const TSet<FString> Allowed = { TEXT("socket"), TEXT("mesh"), TEXT("offset"), TEXT("worldDirection"), TEXT("visualRotation"), TEXT("visualScale") };
				for (const auto& Field : Obj->Values) if (!Allowed.Contains(FString(Field.Key)))
					return Fail(Error, FString::Printf(TEXT("spatial.%s: unknown field '%s'"), *Pair.Key, *Field.Key));
				FString Socket;
				if (!ReadString(*Obj, TEXT("socket"), Socket, false, Error)) return false;
				if (!ReadString(*Obj, TEXT("mesh"), D->VisualMesh, false, Error)) return false;
				if (!D->VisualMesh.IsEmpty() && !FSoftObjectPath(D->VisualMesh).IsValid())
					return Fail(Error, FString::Printf(TEXT("spatial.%s.mesh must be a valid soft object path"), *Pair.Key));
				D->Socket = FName(*Socket);
				if (D->Source == ESkillLabSpatialSource::Ground && !D->Socket.IsNone()) return Fail(Error, TEXT("ground spatial object cannot bind a socket"));
				auto ReadVector = [&](const TCHAR* Name, FVector& Value)
				{
					const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
					if (!Obj->HasField(Name)) return true;
					if (!Obj->TryGetArrayField(Name, Values) || !Values || Values->Num() != 3)
						return Fail(Error, FString::Printf(TEXT("spatial.%s.%s must be a three-number array"), *Pair.Key, Name));
					double Components[3];
					for (int32 I = 0; I < 3; ++I)
						if (!(*Values)[I].IsValid() || (*Values)[I]->Type != EJson::Number || !(*Values)[I]->TryGetNumber(Components[I])
							|| !FMath::IsFinite(Components[I]) || FMath::Abs(Components[I]) > MaxContentNumber)
							return Fail(Error, FString::Printf(TEXT("spatial.%s.%s contains an invalid number"), *Pair.Key, Name));
					Value = FVector(Components[0], Components[1], Components[2]); return true;
				};
				FVector WorldAngles = FVector::ZeroVector, VisualAngles = FVector::ZeroVector;
				if (!ReadVector(TEXT("offset"), D->Offset) || !ReadVector(TEXT("worldDirection"), WorldAngles)
					|| !ReadVector(TEXT("visualRotation"), VisualAngles) || !ReadNumber(*Obj, TEXT("visualScale"), D->VisualScale, false, Error)) return false;
				D->WorldDirection = FRotator(WorldAngles.X, WorldAngles.Y, WorldAngles.Z);
				D->VisualRotation = FRotator(VisualAngles.X, VisualAngles.Y, VisualAngles.Z);
				if (D->VisualScale <= 0) return Fail(Error, FString::Printf(TEXT("spatial.%s.visualScale must be positive"), *Pair.Key));
			}
		}
		for (const auto& D : Out.SpatialDefinitions)
		{
			if (!Spatial || !(*Spatial)->HasField(D.Name.ToString())) return Fail(Error, FString::Printf(TEXT("spatial.%s is missing JSON parameters"), *D.Name.ToString()));
			if (D.Object == ESkillLabSpatialObject::Projectile && Out.Execution != ESkillLabExecutionTemplate::Projectile)
				return Fail(Error, TEXT("Projectile spatial object requires Projectile execution"));
		}
		if (Out.SpatialDefinitions.FilterByPredicate([](const auto& D) { return D.Object == ESkillLabSpatialObject::Projectile; }).Num() > 1)
			return Fail(Error, TEXT("only one Projectile spatial object is allowed"));

		const TSharedPtr<FJsonValue> MontageValue = Json->TryGetField(TEXT("castMontage"));
		if (MontageValue.IsValid())
		{
			FString Path;
			if (MontageValue->Type != EJson::String || !MontageValue->TryGetString(Path) || Path.IsEmpty()) return Fail(Error, TEXT("castMontage must be a nonempty soft asset path"));
			Out.CastMontage = TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(Path));
		}
		const TSharedPtr<FJsonValue> AnimationValue = Json->TryGetField(TEXT("castAnimation"));
		if (AnimationValue.IsValid())
		{
			FString Path;
			if (AnimationValue->Type != EJson::String || !AnimationValue->TryGetString(Path) || Path.IsEmpty()) return Fail(Error, TEXT("castAnimation must be a nonempty soft asset path"));
			Out.CastAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(Path));
		}
		if (!Out.CastMontage.IsNull() && !Out.CastAnimation.IsNull()) return Fail(Error, TEXT("castMontage and castAnimation are mutually exclusive"));

		if (!ReadNumber(*Json, TEXT("cost"), Out.Cost, true, Error) || !ReadNumber(*Json, TEXT("cooldown"), Out.Cooldown, true, Error)
			|| !ReadNumber(*Json, TEXT("magnitude"), Out.Magnitude, true, Error) || !ReadNumber(*Json, TEXT("powerCoefficient"), Out.PowerCoefficient, false, Error)
			|| !ReadNumber(*Json, TEXT("range"), Out.Range, true, Error) || !ReadNumber(*Json, TEXT("radius"), Out.Radius, Out.TargetShape == ESkillLabTargetShape::Ground, Error)
			|| !ReadNumber(*Json, TEXT("windup"), Out.Phases.WindupSeconds, false, Error) || !ReadNumber(*Json, TEXT("active"), Out.Phases.ActiveSeconds, false, Error)
			|| !ReadNumber(*Json, TEXT("recovery"), Out.Phases.RecoverySeconds, false, Error)
			|| !ReadNumber(*Json, TEXT("duration"), Out.Duration, Out.Effect == ESkillLabEffectKind::DamageOverTime, Error)
			|| !ReadNumber(*Json, TEXT("period"), Out.Period, Out.Effect == ESkillLabEffectKind::DamageOverTime, Error)) return false;
		if (Out.Execution == ESkillLabExecutionTemplate::Projectile)
		{
			if (!ReadNumber(*Json, TEXT("projectileSpeed"), Out.ProjectileParameters.Speed, true, Error)
				|| !ReadNumber(*Json, TEXT("projectileCollisionRadius"), Out.ProjectileParameters.CollisionRadius, true, Error)
				|| !ReadNumber(*Json, TEXT("projectileLifetime"), Out.ProjectileParameters.Lifetime, true, Error)
				|| !ReadNumber(*Json, TEXT("projectileVisualScale"), Out.ProjectileParameters.VisualScale, true, Error)
				|| !ReadBoolean(*Json, TEXT("projectileInterceptOtherUnits"), Out.ProjectileParameters.bInterceptOtherUnits, true, Error)) return false;
		}
		else if (Json->HasField(TEXT("projectileSpeed")) || Json->HasField(TEXT("projectileCollisionRadius")) || Json->HasField(TEXT("projectileLifetime"))
			|| Json->HasField(TEXT("projectileVisualScale")) || Json->HasField(TEXT("projectileInterceptOtherUnits")))
			return Fail(Error, TEXT("projectile tuning is only valid for Projectile execution"));

		FString ReleaseTrigger;
		if (!ReadString(*Json, TEXT("releaseTrigger"), ReleaseTrigger, false, Error)) return false;
		if (!ReleaseTrigger.IsEmpty())
		{
			if (ReleaseTrigger == TEXT("Timer")) Out.ReleaseTrigger = ESkillLabReleaseTrigger::Timer;
			else if (ReleaseTrigger == TEXT("GameplayEvent")) Out.ReleaseTrigger = ESkillLabReleaseTrigger::GameplayEvent;
			else return Fail(Error, TEXT("releaseTrigger must be 'Timer' or 'GameplayEvent'"));
		}
		FString ReleaseTag;
		if (!ReadString(*Json, TEXT("releaseEventTag"), ReleaseTag, false, Error)) return false;
		if (!ReleaseTag.IsEmpty())
		{
			Out.ReleaseEventTag = FGameplayTag::RequestGameplayTag(FName(*ReleaseTag), false);
			if (!Out.ReleaseEventTag.IsValid()) return Fail(Error, FString::Printf(TEXT("unknown GameplayTag '%s' in releaseEventTag"), *ReleaseTag));
		}
		if (!ReadNumber(*Json, TEXT("releaseEventTimeout"), Out.ReleaseEventTimeout, false, Error)) return false;
		FString CompletionEvent;
		if (!ReadString(*Json, TEXT("completionEventTag"), CompletionEvent, false, Error)) return false;
		if (!CompletionEvent.IsEmpty())
		{
			Out.CompletionEventTag = FGameplayTag::RequestGameplayTag(FName(*CompletionEvent), false);
			if (!Out.CompletionEventTag.IsValid() || Out.CompletionEventTag == Out.ReleaseEventTag || Out.Phases.ActiveSeconds <= 0.0f)
				return Fail(Error, TEXT("completionEventTag requires a distinct registered tag and a positive active timeout"));
		}
		FString CompletionMode;
		if (!ReadString(*Json, TEXT("completionMode"), CompletionMode, false, Error)) return false;
		if (!CompletionMode.IsEmpty())
		{
			if (CompletionMode == TEXT("ScriptOrEvent")) Out.CompletionMode = ESkillLabCompletionMode::ScriptOrEvent;
			else if (CompletionMode == TEXT("AllSubmissionPointsResolved")) Out.CompletionMode = ESkillLabCompletionMode::AllSubmissionPointsResolved;
			else return Fail(Error, TEXT("completionMode must be ScriptOrEvent or AllSubmissionPointsResolved"));
		}

		auto ReadTags = [&](const TCHAR* Field, FGameplayTagContainer& Tags)
		{
			const TSharedPtr<FJsonValue> Value = Json->TryGetField(Field);
			if (!Value.IsValid()) return true;
			if (Value->Type != EJson::Array) return Fail(Error, FString::Printf(TEXT("%s must be an array of registered tag names"), Field));
			for (const TSharedPtr<FJsonValue>& Entry : Value->AsArray())
			{
				FString Name;
				if (!Entry.IsValid() || Entry->Type != EJson::String || !Entry->TryGetString(Name) || Name.IsEmpty()) return Fail(Error, FString::Printf(TEXT("%s entries must be nonempty strings"), Field));
				const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name), false);
				if (!Tag.IsValid()) return Fail(Error, FString::Printf(TEXT("unknown GameplayTag '%s'"), *Name));
				Tags.AddTag(Tag);
			}
			return true;
		};
		if (!ReadTags(TEXT("blockedBy"), Out.BlockedByTags)) return false;
		if (!Json->HasField(TEXT("interruptBy"))) Out.InterruptedByTags = Out.BlockedByTags;
		else if (!ReadTags(TEXT("interruptBy"), Out.InterruptedByTags)) return false;

		if (!Out.AbilityClass || !Out.BehaviorClass) return Fail(Error, TEXT("shared runtime classes were not assigned"));
		if (Out.Cost < 0.0f || Out.Cooldown < 0.0f || Out.Magnitude < 0.0f || Out.PowerCoefficient < 0.0f || Out.Range <= 0.0f
			|| Out.Radius < 0.0f || Out.Phases.WindupSeconds < 0.0f || Out.Phases.ActiveSeconds < 0.0f || Out.Phases.RecoverySeconds < 0.0f
			|| Out.Duration < 0.0f || Out.Period < 0.0f) return Fail(Error, TEXT("JSON numbers must be nonnegative and range must be positive"));
		if ((Out.TargetShape == ESkillLabTargetShape::Self) != (Out.Relation == ESkillLabTargetRelation::Self)) return Fail(Error, TEXT("Self target shape and Self relation must be declared together"));
		if ((Out.Execution == ESkillLabExecutionTemplate::GroundArea) != (Out.TargetShape == ESkillLabTargetShape::Ground)) return Fail(Error, TEXT("GroundArea execution requires Ground target shape, and Ground requires GroundArea"));
		if (Out.TargetShape == ESkillLabTargetShape::Ground && Out.Radius <= 0.0f) return Fail(Error, TEXT("Ground definitions require a positive JSON radius"));
		if (Out.Execution == ESkillLabExecutionTemplate::Projectile && Out.TargetShape != ESkillLabTargetShape::Unit) return Fail(Error, TEXT("Projectile execution requires Unit target shape"));
		if (Out.Execution == ESkillLabExecutionTemplate::InstantTarget && Out.TargetShape == ESkillLabTargetShape::Ground) return Fail(Error, TEXT("InstantTarget execution cannot use Ground target shape"));
		if (Out.Effect == ESkillLabEffectKind::DamageOverTime && (Out.Duration <= 0.0f || Out.Period <= 0.0f)) return Fail(Error, TEXT("DamageOverTime requires positive JSON duration and period"));
		if (Out.Execution == ESkillLabExecutionTemplate::Projectile && (Out.ProjectileParameters.Speed <= 0.0f || Out.ProjectileParameters.CollisionRadius <= 0.0f
			|| Out.ProjectileParameters.Lifetime <= 0.0f || Out.ProjectileParameters.VisualScale <= 0.0f)) return Fail(Error, TEXT("Projectile JSON tuning values must be positive"));
		if (!FMath::IsFinite(Out.ReleaseEventTimeout) || (Out.ReleaseTrigger == ESkillLabReleaseTrigger::GameplayEvent && (!Out.ReleaseEventTag.IsValid() || Out.ReleaseEventTimeout <= 0.0f))
			|| (Out.ReleaseTrigger == ESkillLabReleaseTrigger::Timer && Out.ReleaseEventTag.IsValid())) return Fail(Error, TEXT("release trigger, tag, and timeout combination is invalid"));
		if (Out.bIndependentWindup && (Out.ReleaseTrigger != ESkillLabReleaseTrigger::GameplayEvent || Out.Phases.WindupSeconds >= Out.ReleaseEventTimeout))
			return Fail(Error, TEXT("independent windup requires an event release with a later timeout"));
		if (!ParseSubmissionPoints(Json, Out, Error)) return false;
		if (Out.CompletionMode == ESkillLabCompletionMode::AllSubmissionPointsResolved && (!Out.bHasExplicitSubmissionPoints || Out.CompletionEventTag.IsValid()))
			return Fail(Error, TEXT("AllSubmissionPointsResolved requires explicit submissionPoints and cannot use completionEventTag"));
		// 释放超时在释放时已清理；后续事件必须由 Active 超时保障终止。
		if (Out.Phases.ActiveSeconds <= 0.0f && Out.SubmissionPoints.ContainsByPredicate([](const FSkillLabSubmissionPointDefinition& Point)
			{ return Point.Trigger == ESkillLabSubmissionTrigger::GameplayEvent; }))
			return Fail(Error, TEXT("GameplayEvent submission points require a positive active timeout"));
		return true;
	}
}
