#include "SkillLabSkillDefinitionParsing.h"

#include "GameplayTagContainer.h"

namespace SkillLabSkillDefinitionParsing
{
	namespace
	{
		bool Fail(FString& Error, const FString& Message)
		{
			Error = Message;
			return false;
		}

		bool ReadRequiredString(const FJsonObject& Json, const TCHAR* FieldName, FString& OutValue, FString& Error)
		{
			const TSharedPtr<FJsonValue> Value = Json.TryGetField(FieldName);
			if (!Value.IsValid() || Value->Type != EJson::String || !Value->TryGetString(OutValue)
				|| OutValue.IsEmpty() || OutValue != OutValue.TrimStartAndEnd())
				return Fail(Error, FString::Printf(TEXT("submission point '%s' must be a nonempty trimmed string"), FieldName));
			return true;
		}

		ESkillLabSubmissionTargetBinding LegacyTargetBinding(const FSkillLabSkillDefinition& Definition)
		{
			switch (Definition.TargetShape)
			{
			case ESkillLabTargetShape::Self: return ESkillLabSubmissionTargetBinding::Source;
			case ESkillLabTargetShape::Ground: return ESkillLabSubmissionTargetBinding::RequestLocation;
			default: return ESkillLabSubmissionTargetBinding::RequestTarget;
			}
		}
	}

	bool ParseSubmissionPoints(const TSharedPtr<FJsonObject>& Json, FSkillLabSkillDefinition& Definition, FString& Error)
	{
		Definition.SubmissionPoints.Reset();
		const TSharedPtr<FJsonValue> Field = Json->TryGetField(TEXT("submissionPoints"));
		if (!Field.IsValid())
		{
			FSkillLabSubmissionPointDefinition LegacyPoint;
			LegacyPoint.Id = 1;
			LegacyPoint.Trigger = ESkillLabSubmissionTrigger::Script;
			LegacyPoint.TargetBinding = LegacyTargetBinding(Definition);
			Definition.SubmissionPoints.Add(LegacyPoint);
			Definition.bHasExplicitSubmissionPoints = false;
			return true;
		}

		Definition.bHasExplicitSubmissionPoints = true;
		if (Field->Type != EJson::Array || Field->AsArray().IsEmpty())
			return Fail(Error, TEXT("submissionPoints must be a nonempty array"));

		TSet<int32> Ids;
		const FGameplayTag EventRoot = FGameplayTag::RequestGameplayTag(FName(TEXT("SkillLab.Event")), false);
		for (int32 Index = 0; Index < Field->AsArray().Num(); ++Index)
		{
			const TSharedPtr<FJsonValue>& PointValue = Field->AsArray()[Index];
			if (!PointValue.IsValid() || PointValue->Type != EJson::Object)
				return Fail(Error, FString::Printf(TEXT("submissionPoints[%d] must be an object"), Index));
			const TSharedPtr<FJsonObject> PointJson = PointValue->AsObject();
			static const TSet<FString> AllowedFields = { TEXT("id"), TEXT("trigger"), TEXT("delay"), TEXT("eventTag"), TEXT("target") };
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : PointJson->Values)
				if (!AllowedFields.Contains(Pair.Key))
					return Fail(Error, FString::Printf(TEXT("unknown submissionPoints[%d] field '%s'"), Index, *Pair.Key));

			FSkillLabSubmissionPointDefinition Point;
			const TSharedPtr<FJsonValue> IdValue = PointJson->TryGetField(TEXT("id"));
			double IdNumber = 0.0;
			if (!IdValue.IsValid() || IdValue->Type != EJson::Number || !IdValue->TryGetNumber(IdNumber)
				|| !FMath::IsFinite(IdNumber) || IdNumber < 1.0 || IdNumber > static_cast<double>(MAX_int32)
				|| FMath::FloorToDouble(IdNumber) != IdNumber)
				return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].id must be a positive int32"), Index));
			Point.Id = static_cast<int32>(IdNumber);
			if (Ids.Contains(Point.Id))
				return Fail(Error, FString::Printf(TEXT("duplicate submission point id %d"), Point.Id));
			Ids.Add(Point.Id);

			FString Trigger;
			if (!ReadRequiredString(*PointJson, TEXT("trigger"), Trigger, Error)) return false;
			if (Trigger == TEXT("Script")) Point.Trigger = ESkillLabSubmissionTrigger::Script;
			else if (Trigger == TEXT("Release")) Point.Trigger = ESkillLabSubmissionTrigger::Release;
			else if (Trigger == TEXT("Timer")) Point.Trigger = ESkillLabSubmissionTrigger::Timer;
			else if (Trigger == TEXT("GameplayEvent")) Point.Trigger = ESkillLabSubmissionTrigger::GameplayEvent;
			else return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].trigger must be Script, Release, Timer, or GameplayEvent"), Index));

			FString Target;
			if (!ReadRequiredString(*PointJson, TEXT("target"), Target, Error)) return false;
			if (Target == TEXT("Source")) Point.TargetBinding = ESkillLabSubmissionTargetBinding::Source;
			else if (Target == TEXT("RequestTarget")) Point.TargetBinding = ESkillLabSubmissionTargetBinding::RequestTarget;
			else if (Target == TEXT("RequestLocation")) Point.TargetBinding = ESkillLabSubmissionTargetBinding::RequestLocation;
			else return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].target must be Source, RequestTarget, or RequestLocation"), Index));

			const TSharedPtr<FJsonValue> DelayValue = PointJson->TryGetField(TEXT("delay"));
			if (Point.Trigger == ESkillLabSubmissionTrigger::Timer)
			{
				double Delay = 0.0;
				if (!DelayValue.IsValid() || DelayValue->Type != EJson::Number || !DelayValue->TryGetNumber(Delay)
					|| !FMath::IsFinite(Delay) || Delay < 0.0 || Delay > static_cast<double>(MAX_flt))
					return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].delay must be a finite nonnegative Timer delay"), Index));
				Point.DelaySeconds = static_cast<float>(Delay);
			}
			else if (DelayValue.IsValid())
				return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].delay is only valid for Timer"), Index));

			const TSharedPtr<FJsonValue> EventValue = PointJson->TryGetField(TEXT("eventTag"));
			if (Point.Trigger == ESkillLabSubmissionTrigger::GameplayEvent)
			{
				if (!EventValue.IsValid())
					return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].eventTag is required for GameplayEvent trigger"), Index));
				FString EventName;
				if (!ReadRequiredString(*PointJson, TEXT("eventTag"), EventName, Error)) return false;
				Point.EventTag = FGameplayTag::RequestGameplayTag(FName(*EventName), false);
				if (!Point.EventTag.IsValid() || !EventRoot.IsValid() || !Point.EventTag.MatchesTag(EventRoot) || Point.EventTag == EventRoot)
					return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].eventTag must be a registered SkillLab.Event child tag"), Index));
				if (Point.EventTag == Definition.ReleaseEventTag || Point.EventTag == Definition.CompletionEventTag)
					return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].eventTag conflicts with releaseEventTag or completionEventTag"), Index));
				for (const FSkillLabSubmissionPointDefinition& Existing : Definition.SubmissionPoints)
					if (Existing.EventTag == Point.EventTag)
						return Fail(Error, FString::Printf(TEXT("duplicate submission event tag '%s'"), *Point.EventTag.ToString()));
				Definition.BehaviorEventTags.AddTag(Point.EventTag);
			}
			else if (EventValue.IsValid())
				return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].eventTag is only valid for GameplayEvent"), Index));

			const bool bTargetCompatible =
				(Point.TargetBinding == ESkillLabSubmissionTargetBinding::Source && Definition.TargetShape == ESkillLabTargetShape::Self)
				|| (Point.TargetBinding == ESkillLabSubmissionTargetBinding::RequestTarget && Definition.TargetShape == ESkillLabTargetShape::Unit)
				|| (Point.TargetBinding == ESkillLabSubmissionTargetBinding::RequestLocation && Definition.TargetShape == ESkillLabTargetShape::Ground);
			if (!bTargetCompatible)
				return Fail(Error, FString::Printf(TEXT("submissionPoints[%d].target is incompatible with the skill target shape"), Index));
			Definition.SubmissionPoints.Add(Point);
		}

		return true;
	}
}
