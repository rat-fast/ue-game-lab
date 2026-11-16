#include "SkillLabSkillDefinitionParsing.h"

#include "GameplayTagContainer.h"
#include "SkillLabGASTags.h"
#include "SkillLabLua.h"

namespace SkillLabSkillDefinitionParsing
{
	namespace
	{
		constexpr int32 MaxLuaSourceLength = 256 * 1024;

		struct FLuaDefinitionCapture
		{
			TMap<FString, FString> Fields;
			TArray<FSkillLabSpatialDefinition> Spatial;
			FString Error;
			bool bCalled = false;
		};

		int32 CaptureSpatial(FSkillLabLuaState* State)
		{
			auto* Capture = static_cast<FLuaDefinitionCapture*>(SkillLabLua::GetCallbackContext(State));
			if (!Capture) return 0;
			if (SkillLabLua::GetTop(State) != 8 || Capture->Spatial.Num() >= 32)
			{ Capture->Error = TEXT("SkillLabSkill_Spatial expects eight strings and at most 32 objects"); return 0; }
			FString V[8];
			for (int32 I = 0; I < 8; ++I)
			{
				if (!SkillLabLua::IsString(State, I + 1)) { Capture->Error = TEXT("spatial arguments must be strings"); return 0; }
				V[I] = UTF8_TO_TCHAR(SkillLabLua::ToString(State, I + 1));
				if (V[I].IsEmpty() || V[I] != V[I].TrimStartAndEnd()) { Capture->Error = TEXT("spatial arguments must be nonempty and trimmed"); return 0; }
			}
			FSkillLabSpatialDefinition D; D.Name = FName(*V[0]); D.Event = FName(*V[2]);
			if (D.Name.IsNone() || Capture->Spatial.ContainsByPredicate([&](const auto& Existing) { return Existing.Name == D.Name; }))
			{ Capture->Error = TEXT("duplicate or invalid spatial object name"); return 0; }
			if (V[1] == TEXT("Projectile")) D.Object = ESkillLabSpatialObject::Projectile;
			else if (V[1] != TEXT("Marker")) { Capture->Error = TEXT("spatial object must be Projectile or Marker"); return 0; }
			if (V[3] == TEXT("Source")) D.Source = ESkillLabSpatialSource::Source;
			else if (V[3] == TEXT("Target")) D.Source = ESkillLabSpatialSource::Target;
			else if (V[3] == TEXT("Ground")) D.Source = ESkillLabSpatialSource::Ground;
			else if (V[3] == TEXT("Related")) D.Source = ESkillLabSpatialSource::Related;
			else { Capture->Error = TEXT("invalid spatial source"); return 0; }
			if (V[4] == TEXT("Reference")) D.OffsetSpace = ESkillLabSpatialOffsetSpace::Reference;
			else if (V[4] == TEXT("World")) D.OffsetSpace = ESkillLabSpatialOffsetSpace::World;
			else if (V[4] == TEXT("ExecutionDirection")) D.OffsetSpace = ESkillLabSpatialOffsetSpace::ExecutionDirection;
			else { Capture->Error = TEXT("invalid spatial offset space"); return 0; }
			if (V[5] == TEXT("Reference")) D.Direction = ESkillLabSpatialDirection::Reference;
			else if (V[5] == TEXT("World")) D.Direction = ESkillLabSpatialDirection::World;
			else if (V[5] == TEXT("ExecutionDirection")) D.Direction = ESkillLabSpatialDirection::ExecutionDirection;
			else { Capture->Error = TEXT("invalid spatial direction"); return 0; }
			if (V[6] == TEXT("Follow")) D.bFollow = true;
			else if (V[6] != TEXT("Once")) { Capture->Error = TEXT("spatial relation must be Once or Follow"); return 0; }
			if (V[7] == TEXT("FollowRotation")) D.bFollowRotation = true;
			else if (V[7] != TEXT("KeepRotation")) { Capture->Error = TEXT("spatial rotation relation invalid"); return 0; }
			if ((D.Object == ESkillLabSpatialObject::Projectile && (V[2] != TEXT("Release") || D.bFollow))
				|| (D.Object == ESkillLabSpatialObject::Marker && V[2] != TEXT("TargetSelected") && V[2] != TEXT("Hit") && V[2] != TEXT("BuffApplied"))
				|| (D.bFollow && (D.Source == ESkillLabSpatialSource::Ground || D.Source == ESkillLabSpatialSource::Related))
				|| (D.bFollowRotation && !D.bFollow) || (D.bFollow && V[2] != TEXT("BuffApplied")))
			{ Capture->Error = TEXT("invalid spatial object, event, or follow combination"); return 0; }
			Capture->Spatial.Add(D); return 0;
		}

		bool Fail(FString& Error, const FString& Message)
		{
			Error = Message;
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
	}

	bool ParseLuaDeclaration(const FString& Source, const FString& SourceName, FSkillLabSkillDefinition& Out, FString& OutError)
	{
		if (Source.IsEmpty() || Source.Len() > MaxLuaSourceLength)
			return Fail(OutError, FString::Printf(TEXT("%s must contain at most %d characters"), *SourceName, MaxLuaSourceLength));
		FSkillLabLuaState* Lua = SkillLabLua::Create();
		if (!Lua) return Fail(OutError, FString::Printf(TEXT("%s: Lua VM creation failed"), *SourceName));
		SkillLabLua::SetInstructionBudget(Lua, 100000);
		FLuaDefinitionCapture Capture;
		SkillLabLua::PushCallback(Lua, &CaptureLuaDefinition, &Capture);
		SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Define");
		SkillLabLua::PushCallback(Lua, &CaptureSpatial, &Capture);
		SkillLabLua::SetGlobal(Lua, "SkillLabSkill_Spatial");
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
		Out.SpatialDefinitions = MoveTemp(Capture.Spatial);

		static const TSet<FString> RequiredFields = { TEXT("targetShape"), TEXT("relation"), TEXT("effect"), TEXT("execution") };
		for (const FString& Field : RequiredFields)
			if (!Capture.Fields.Contains(Field)) return Fail(OutError, FString::Printf(TEXT("%s: missing Lua declaration field '%s'"), *SourceName, *Field));
		for (const TPair<FString, FString>& Field : Capture.Fields)
			if (!RequiredFields.Contains(Field.Key) && Field.Key != TEXT("combatRules") && Field.Key != TEXT("recoveryRules") && Field.Key != TEXT("category") && Field.Key != TEXT("categoryTag") && Field.Key != TEXT("windupEnd") && Field.Key != TEXT("windowConflict"))
				return Fail(OutError, FString::Printf(TEXT("%s: unknown Lua declaration field '%s'"), *SourceName, *Field.Key));
		if (Capture.Fields.Contains(TEXT("category")) && Capture.Fields.Contains(TEXT("categoryTag")))
			return Fail(OutError, TEXT("category and categoryTag cannot both be declared"));
		if (const FString* Category = Capture.Fields.Find(TEXT("category")))
		{
			if (*Category == TEXT("Skill")) Out.Category = ESkillLabOperationCategory::Skill;
			else if (*Category == TEXT("Dodge")) Out.Category = ESkillLabOperationCategory::Dodge;
			else return Fail(OutError, TEXT("category must be Skill or Dodge"));
		}
		Out.AbilityCategory = Out.Category == ESkillLabOperationCategory::Dodge ? SkillLabGASTags::Ability_Dodge : SkillLabGASTags::Ability_Skill;
		if (const FString* CategoryTag = Capture.Fields.Find(TEXT("categoryTag")))
		{
			const FGameplayTag Root = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.SkillLab")), false);
			Out.AbilityCategory = FGameplayTag::RequestGameplayTag(FName(**CategoryTag), false);
			if (!Out.AbilityCategory.IsValid() || !Root.IsValid() || !Out.AbilityCategory.MatchesTag(Root) || Out.AbilityCategory == Root)
				return Fail(OutError, TEXT("categoryTag must be a registered Ability.SkillLab child tag"));
		}
		if (const FString* WindupEnd = Capture.Fields.Find(TEXT("windupEnd")))
		{
			if (*WindupEnd == TEXT("Timer")) Out.bIndependentWindup = true;
			else if (*WindupEnd != TEXT("Release")) return Fail(OutError, TEXT("windupEnd must be Timer or Release"));
		}
		if (const FString* Conflict = Capture.Fields.Find(TEXT("windowConflict")))
		{
			if (*Conflict != TEXT("Reject")) return Fail(OutError, TEXT("windowConflict currently supports explicit Reject only"));
			Out.bRejectWindowConflicts = true;
		}
		if (const FString* Value = Capture.Fields.Find(TEXT("combatRules"))) Out.OperationRulesId = FName(**Value);
		if (const FString* Value = Capture.Fields.Find(TEXT("recoveryRules"))) Out.RecoveryRulesId = FName(**Value);
		if (Out.OperationRulesId.IsNone() != Out.RecoveryRulesId.IsNone())
			return Fail(OutError, TEXT("combatRules and recoveryRules must be declared together"));

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
}
