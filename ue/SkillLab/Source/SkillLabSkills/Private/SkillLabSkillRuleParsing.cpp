#include "SkillLabSkillDefinitionParsing.h"

#include "GameplayTagContainer.h"
#include "SkillLabGASTags.h"

namespace SkillLabSkillRuleParsing
{
	namespace
	{
		bool Fail(FString& Error, const FString& Message)
		{
			Error = Message;
			return false;
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

	bool ParseRuleProfiles(const TSharedPtr<FJsonObject>& Root, TMap<FName, FSkillLabOperationRules>& Out, FString& Error)
	{
		const TSharedPtr<FJsonValue> Field = Root->TryGetField(TEXT("combatRules"));
		if (!Field.IsValid()) return true;
		if (Field->Type != EJson::Array) return Fail(Error, TEXT("combatRules must be an array"));
		for (const TSharedPtr<FJsonValue>& Value : Field->AsArray())
		{
			if (!Value.IsValid() || Value->Type != EJson::Object) return Fail(Error, TEXT("combatRules entries must be objects"));
			const TSharedPtr<FJsonObject> Obj = Value->AsObject();
			static const TSet<FString> Fields = {TEXT("id"), TEXT("skill"), TEXT("dodge"), TEXT("movement"), TEXT("userCancel"), TEXT("normalHit"), TEXT("heavyHit"), TEXT("unknownHit"), TEXT("interruptBy"), TEXT("abilityCategories")};
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
				if (!Fields.Contains(Pair.Key)) return Fail(Error, FString::Printf(TEXT("unknown combatRules field '%s'"), *Pair.Key));
			FString Id;
			if (!ReadString(*Obj, TEXT("id"), Id, true, Error)) return false;
			FSkillLabOperationRules Rules;
			Rules.Id = FName(*Id);
			if (Rules.Id.IsNone() || Out.Contains(Rules.Id)) return Fail(Error, TEXT("combatRules id is empty or duplicated"));
			auto ReadDecision = [&](const TCHAR* Name, ESkillLabRuleDecision& Decision, const bool bSkillRequest, const bool bAllowReject = true)
			{
				FString Text;
				if (!ReadString(*Obj, Name, Text, true, Error)) return false;
				if (bSkillRequest && Text == TEXT("Allow")) Decision = ESkillLabRuleDecision::Allow;
				else if (bSkillRequest && Text == TEXT("Buffer")) Decision = ESkillLabRuleDecision::Buffer;
				else if (bSkillRequest && Text == TEXT("CancelAndActivate")) Decision = ESkillLabRuleDecision::CancelAndActivate;
				else if (bAllowReject && Text == TEXT("Reject")) Decision = ESkillLabRuleDecision::Reject;
				else if (!bSkillRequest && Text == TEXT("Continue")) Decision = ESkillLabRuleDecision::Continue;
				else if (!bSkillRequest && Text == TEXT("Interrupt")) Decision = ESkillLabRuleDecision::Interrupt;
				else return Fail(Error, FString::Printf(TEXT("invalid decision '%s' for %s"), *Text, Name));
				return true;
			};
			if (!ReadDecision(TEXT("skill"), Rules.Skill, true) || !ReadDecision(TEXT("dodge"), Rules.Dodge, true)
				|| !ReadDecision(TEXT("movement"), Rules.Movement, false) || !ReadDecision(TEXT("userCancel"), Rules.UserCancel, false)
				|| !ReadDecision(TEXT("normalHit"), Rules.NormalHit, false, false) || !ReadDecision(TEXT("heavyHit"), Rules.HeavyHit, false, false)
				|| !ReadDecision(TEXT("unknownHit"), Rules.UnknownHit, false, false)) return false;
			Rules.AbilityCategories.Add(SkillLabGASTags::Ability_Skill, Rules.Skill);
			Rules.AbilityCategories.Add(SkillLabGASTags::Ability_Dodge, Rules.Dodge);
			const TSharedPtr<FJsonValue> CategoriesField = Obj->TryGetField(TEXT("abilityCategories"));
			if (CategoriesField.IsValid())
			{
				if (CategoriesField->Type != EJson::Object) return Fail(Error, TEXT("abilityCategories must be an object"));
				const FGameplayTag RootTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Ability.SkillLab")), false);
				for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : CategoriesField->AsObject()->Values)
				{
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Pair.Key), false);
					FString Choice;
					if (!Tag.IsValid() || !RootTag.IsValid() || !Tag.MatchesTag(RootTag) || Tag == RootTag
						|| Tag == SkillLabGASTags::Ability_Skill || Tag == SkillLabGASTags::Ability_Dodge
						|| !Pair.Value.IsValid() || Pair.Value->Type != EJson::String || !Pair.Value->TryGetString(Choice))
						return Fail(Error, TEXT("invalid or duplicate built-in abilityCategories entry"));
					ESkillLabRuleDecision Decision;
					if (Choice == TEXT("Allow")) Decision = ESkillLabRuleDecision::Allow;
					else if (Choice == TEXT("Buffer")) Decision = ESkillLabRuleDecision::Buffer;
					else if (Choice == TEXT("Reject")) Decision = ESkillLabRuleDecision::Reject;
					else if (Choice == TEXT("CancelAndActivate")) Decision = ESkillLabRuleDecision::CancelAndActivate;
					else return Fail(Error, TEXT("invalid abilityCategories decision"));
					Rules.AbilityCategories.Add(Tag, Decision);
				}
			}
			const TSharedPtr<FJsonValue> InterruptField = Obj->TryGetField(TEXT("interruptBy"));
			if (InterruptField.IsValid())
			{
				if (InterruptField->Type != EJson::Array) return Fail(Error, TEXT("combatRules interruptBy must be an array"));
				Rules.bOverrideInterruptBy = true;
				for (const TSharedPtr<FJsonValue>& Entry : InterruptField->AsArray())
				{
					FString Name;
					if (!Entry.IsValid() || Entry->Type != EJson::String || !Entry->TryGetString(Name) || Name.IsEmpty())
						return Fail(Error, TEXT("combatRules interruptBy entries must be tag names"));
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Name), false);
					const FGameplayTag StateRoot = FGameplayTag::RequestGameplayTag(FName(TEXT("State.SkillLab")), false);
					if (!Tag.IsValid() || !StateRoot.IsValid() || !Tag.MatchesTag(StateRoot))
						return Fail(Error, FString::Printf(TEXT("invalid state tag '%s' in combatRules interruptBy"), *Name));
					if (Rules.InterruptBy.HasTagExact(Tag)) return Fail(Error, TEXT("duplicate combatRules interruptBy tag"));
					Rules.InterruptBy.AddTag(Tag);
				}
			}
			Out.Add(Rules.Id, Rules);
		}
		return true;
	}

	bool ParseRuleWindows(const TSharedPtr<FJsonObject>& Obj, const TMap<FName, FSkillLabOperationRules>& Profiles,
		FSkillLabSkillDefinition& Definition, FString& Error)
	{
		const TSharedPtr<FJsonValue> Field = Obj->TryGetField(TEXT("ruleWindows"));
		if (!Field.IsValid()) return true;
		if (Field->Type != EJson::Array) return Fail(Error, TEXT("ruleWindows must be an array"));
		TSet<FName> Ids;
		TSet<FGameplayTag> Events;
		for (const TSharedPtr<FJsonValue>& Value : Field->AsArray())
		{
			if (!Value.IsValid() || Value->Type != EJson::Object) return Fail(Error, TEXT("ruleWindows entries must be objects"));
			const TSharedPtr<FJsonObject> WindowObj = Value->AsObject();
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : WindowObj->Values)
				if (Pair.Key != TEXT("id") && Pair.Key != TEXT("beginEvent") && Pair.Key != TEXT("endEvent") && Pair.Key != TEXT("rules"))
					return Fail(Error, FString::Printf(TEXT("unknown ruleWindows field '%s'"), *Pair.Key));
			FString Id, Begin, End, RuleId;
			if (!ReadString(*WindowObj, TEXT("id"), Id, true, Error) || !ReadString(*WindowObj, TEXT("beginEvent"), Begin, true, Error)
				|| !ReadString(*WindowObj, TEXT("endEvent"), End, true, Error) || !ReadString(*WindowObj, TEXT("rules"), RuleId, true, Error)) return false;
			FSkillLabRuleWindow Window;
			Window.Id = FName(*Id);
			Window.BeginEvent = FGameplayTag::RequestGameplayTag(FName(*Begin), false);
			Window.EndEvent = FGameplayTag::RequestGameplayTag(FName(*End), false);
			const FSkillLabOperationRules* Rules = Profiles.Find(FName(*RuleId));
			if (Window.Id.IsNone() || Ids.Contains(Window.Id) || !Window.BeginEvent.IsValid() || !Window.EndEvent.IsValid()
				|| Window.BeginEvent == Window.EndEvent || Events.Contains(Window.BeginEvent) || Events.Contains(Window.EndEvent)
				|| Definition.BehaviorEventTags.HasTagExact(Window.BeginEvent) || Definition.BehaviorEventTags.HasTagExact(Window.EndEvent)
				|| Window.BeginEvent == Definition.ReleaseEventTag || Window.EndEvent == Definition.ReleaseEventTag
				|| Window.BeginEvent == Definition.CompletionEventTag || Window.EndEvent == Definition.CompletionEventTag || !Rules)
				return Fail(Error, TEXT("ruleWindows require unique ids and registered, distinct event tags and a known rules profile"));
			Ids.Add(Window.Id); Events.Add(Window.BeginEvent); Events.Add(Window.EndEvent);
			Window.Rules = *Rules;
			Definition.RuleWindows.Add(Window);
		}
		if (Definition.RuleWindows.Num() > 1 && !Definition.bRejectWindowConflicts)
			return Fail(Error, TEXT("multiple ruleWindows require an explicit windowConflict declaration"));
		return true;
	}
}
