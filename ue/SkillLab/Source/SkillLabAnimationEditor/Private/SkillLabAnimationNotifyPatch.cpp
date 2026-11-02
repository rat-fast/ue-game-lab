#include "SkillLabAnimationNotifyPatch.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/PackageReload.h"
#include "UObject/UObjectGlobals.h"
#include "SkillLabSkillAnimNotify.h"

namespace
{
	using FObject = TSharedPtr<FJsonObject>;

	void Issue(FSkillLabAnimationContentReport& Report, const FString& Code, const FString& Location, const FString& Message)
	{
		FSkillLabAnimationIssue& Entry = Report.Issues.AddDefaulted_GetRef();
		Entry.Code = Code;
		Entry.Location = Location;
		Entry.Message = Message;
	}

	bool ReadJsonFile(const FString& Filename, FObject& OutObject, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutError = FString::Printf(TEXT("cannot read JSON file: %s"), *Filename);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			OutError = FString::Printf(TEXT("invalid JSON object: %s"), *Filename);
			return false;
		}
		return true;
	}

	bool NonEmptyString(const FObject& Object, const TCHAR* Field, FString& Out, FString& Error)
	{
		if (!Object->TryGetStringField(Field, Out) || Out.IsEmpty() || Out != Out.TrimStartAndEnd())
		{
			Error = FString::Printf(TEXT("%s must be a non-empty string without surrounding whitespace"), Field);
			return false;
		}
		return true;
	}

	bool Number(const FObject& Object, const TCHAR* Field, double& Out, FString& Error)
	{
		if (!Object->TryGetNumberField(Field, Out) || !FMath::IsFinite(Out))
		{
			Error = FString::Printf(TEXT("%s must be a finite number"), Field);
			return false;
		}
		return true;
	}

	bool IsSha256(const FString& Value)
	{
		if (Value.Len() != 64) return false;
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsHexDigit(Character)) return false;
		}
		return true;
	}

	bool IsAllowed(const FObject& Object, const TSet<FString>& Allowed, FString& Error)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Object->Values)
		{
			if (!Allowed.Contains(Pair.Key))
			{
				Error = FString::Printf(TEXT("unknown field '%s'"), *Pair.Key);
				return false;
			}
		}
		return true;
	}

	bool ParseEvidence(const FObject& Object, FSkillLabAnimationPatchOperation& Out, FString& Error)
	{
		const TSharedPtr<FJsonObject>* EvidenceObject = nullptr;
		if (!Object->TryGetObjectField(TEXT("evidence"), EvidenceObject) || !EvidenceObject || !EvidenceObject->IsValid())
		{
			Error = TEXT("evidence must be an object");
			return false;
		}
		const FObject& Evidence = *EvidenceObject;
		if (!IsAllowed(Evidence, { TEXT("sourceFrames"), TEXT("sourceSeconds"), TEXT("basis"), TEXT("uncertainty") }, Error)) return false;
		FSkillLabAnimationPatchEvidence Parsed;
		if (!NonEmptyString(Evidence, TEXT("basis"), Parsed.Basis, Error) || !NonEmptyString(Evidence, TEXT("uncertainty"), Parsed.Uncertainty, Error)) return false;
		const TArray<TSharedPtr<FJsonValue>>* Frames = nullptr;
		if (Evidence->HasField(TEXT("sourceFrames")) && !Evidence->TryGetArrayField(TEXT("sourceFrames"), Frames)) { Error = TEXT("evidence.sourceFrames must be an array"); return false; }
		if (Frames)
		{
			for (int32 Index = 0; Index < Frames->Num(); ++Index)
			{
				double Value = 0.0;
				if (!(*Frames)[Index].IsValid() || !(*Frames)[Index]->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0.0 || !FMath::IsNearlyEqual(Value, FMath::RoundToDouble(Value)))
				{
					Error = FString::Printf(TEXT("evidence.sourceFrames[%d] must be a nonnegative integer"), Index);
					return false;
				}
				Parsed.SourceFrames.Add(static_cast<int32>(Value));
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Seconds = nullptr;
		if (Evidence->HasField(TEXT("sourceSeconds")) && !Evidence->TryGetArrayField(TEXT("sourceSeconds"), Seconds)) { Error = TEXT("evidence.sourceSeconds must be an array"); return false; }
		if (Seconds)
		{
			for (int32 Index = 0; Index < Seconds->Num(); ++Index)
			{
				double Value = 0.0;
				if (!(*Seconds)[Index].IsValid() || !(*Seconds)[Index]->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value < 0.0)
				{
					Error = FString::Printf(TEXT("evidence.sourceSeconds[%d] must be a nonnegative finite number"), Index);
					return false;
				}
				Parsed.SourceSeconds.Add(static_cast<float>(Value));
			}
		}
		Out.Evidence = Parsed;
		return true;
	}

	bool ParseOperation(const TSharedPtr<FJsonValue>& Value, FSkillLabAnimationPatchOperation& Out, FString& Error)
	{
		const FObject Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object.IsValid()) { Error = TEXT("operation must be an object"); return false; }
		if (!Object->TryGetStringField(TEXT("op"), Out.Op) || !Object->TryGetStringField(TEXT("id"), Out.Id) || Out.Op.IsEmpty() || Out.Id.IsEmpty() || Out.Id != Out.Id.TrimStartAndEnd())
		{
			Error = TEXT("operation requires nonempty op and stable id");
			return false;
		}
		if (Out.Op == TEXT("delete"))
		{
			if (!IsAllowed(Object, { TEXT("op"), TEXT("id") }, Error)) return false;
			return true;
		}
		if (Out.Op != TEXT("upsert")) { Error = TEXT("operation op must be 'upsert' or 'delete'"); return false; }
		if (!IsAllowed(Object, { TEXT("op"), TEXT("id"), TEXT("kind"), TEXT("eventTag"), TEXT("time"), TEXT("stateClass"), TEXT("startTime"), TEXT("endTime"), TEXT("evidence") }, Error)) return false;
		if (!NonEmptyString(Object, TEXT("kind"), Out.Kind, Error) || !NonEmptyString(Object, TEXT("eventTag"), Out.EventTag, Error)) return false;
		if (Out.Kind != TEXT("instant") && Out.Kind != TEXT("state")) { Error = TEXT("upsert kind must be 'instant' or 'state'"); return false; }
		const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*Out.EventTag), false);
		if (!EventTag.IsValid()) { Error = FString::Printf(TEXT("unknown GameplayTag '%s'"), *Out.EventTag); return false; }
		if (Out.Kind == TEXT("instant"))
		{
			double Time = 0.0;
			if (!Number(Object, TEXT("time"), Time, Error) || Time < 0.0) { if (Error.IsEmpty()) Error = TEXT("instant time must be nonnegative"); return false; }
			Out.Time = static_cast<float>(Time);
			if (Object->HasField(TEXT("stateClass")) || Object->HasField(TEXT("startTime")) || Object->HasField(TEXT("endTime"))) { Error = TEXT("instant operation cannot contain state fields"); return false; }
		}
		else
		{
			if (!NonEmptyString(Object, TEXT("stateClass"), Out.StateClass, Error)) return false;
			if (Out.StateClass != TEXT("SkillLabSkillAnimNotifyState") && Out.StateClass != TEXT("/Script/SkillLabPresentation.SkillLabSkillAnimNotifyState"))
			{
				Error = FString::Printf(TEXT("unsupported Notify State class '%s'"), *Out.StateClass);
				return false;
			}
			double Start = 0.0, End = 0.0;
			if (!Number(Object, TEXT("startTime"), Start, Error) || !Number(Object, TEXT("endTime"), End, Error) || Start < 0.0 || End <= Start)
			{
				if (Error.IsEmpty()) Error = TEXT("state requires 0 <= startTime < endTime");
				return false;
			}
			Out.StartTime = static_cast<float>(Start);
			Out.EndTime = static_cast<float>(End);
			if (Object->HasField(TEXT("time"))) { Error = TEXT("state operation cannot contain time"); return false; }
		}
		if (!Object->HasField(TEXT("evidence"))) { Error = TEXT("upsert requires evidence"); return false; }
		if (!ParseEvidence(Object, Out, Error)) return false;
		return true;
	}

	bool TryGetMappingTarget(const FObject& Object, FString& OutTarget)
	{
		const TSharedPtr<FJsonObject>* MappingObject = nullptr;
		if (!Object->TryGetObjectField(TEXT("playbackMapping"), MappingObject) || !MappingObject || !MappingObject->IsValid()) return false;
		return (*MappingObject)->TryGetStringField(TEXT("targetAsset"), OutTarget)
			|| (*MappingObject)->TryGetStringField(TEXT("montage"), OutTarget)
			|| (*MappingObject)->TryGetStringField(TEXT("asset"), OutTarget);
	}

	bool ValidateObservation(const FSkillLabAnimationPatch& Patch, FSkillLabAnimationContentReport& Report)
	{
		FString Filename = Patch.Observation;
		if (FPaths::IsRelative(Filename)) Filename = FPaths::Combine(FPaths::ProjectDir(), Filename);
		FObject Observation;
		FString Error;
		if (!ReadJsonFile(Filename, Observation, Error)) { Issue(Report, TEXT("MissingObservation"), TEXT("source.observation"), Error); return false; }
		FString Value;
		if (!Observation->TryGetStringField(TEXT("generationId"), Value) || Value != Patch.GenerationId) { Issue(Report, TEXT("ObservationGenerationMismatch"), TEXT("source.generationId"), TEXT("observation generationId does not match patch")); return false; }
		if (!Observation->TryGetStringField(TEXT("inputPreviewFingerprintSha256"), Value) || !Value.Equals(Patch.InputPreviewFingerprintSha256, ESearchCase::IgnoreCase)) { Issue(Report, TEXT("ObservationFingerprintMismatch"), TEXT("source.inputPreviewFingerprintSha256"), TEXT("observation fingerprint does not match patch")); return false; }
		FString ManifestFilename;
		if (!Observation->TryGetStringField(TEXT("manifest"), ManifestFilename) || ManifestFilename.IsEmpty()) { Issue(Report, TEXT("MissingObservationManifest"), TEXT("source.observation.manifest"), TEXT("observation must reference its completed action overview manifest")); return false; }
		if (FPaths::IsRelative(ManifestFilename)) ManifestFilename = FPaths::Combine(FPaths::ProjectDir(), ManifestFilename);
		FObject Manifest;
		if (!ReadJsonFile(ManifestFilename, Manifest, Error)) { Issue(Report, TEXT("MissingObservationManifest"), TEXT("source.observation.manifest"), Error); return false; }
		if (!Manifest->TryGetStringField(TEXT("generationId"), Value) || Value != Patch.GenerationId) { Issue(Report, TEXT("ManifestGenerationMismatch"), TEXT("source.generationId"), TEXT("manifest generationId does not match patch")); return false; }
		if (!Manifest->TryGetStringField(TEXT("inputPreviewFingerprintSha256"), Value) || !Value.Equals(Patch.InputPreviewFingerprintSha256, ESearchCase::IgnoreCase)) { Issue(Report, TEXT("ManifestFingerprintMismatch"), TEXT("source.inputPreviewFingerprintSha256"), TEXT("manifest fingerprint does not match patch")); return false; }
		FString Status;
		if (!Manifest->TryGetStringField(TEXT("status"), Status) || Status != TEXT("complete")) { Issue(Report, TEXT("IncompleteObservation"), TEXT("source.observation.manifest.status"), TEXT("source manifest must have status=complete")); return false; }
		FString Mapping;
		if (Observation->TryGetStringField(TEXT("playbackMapping"), Mapping) && Mapping == TEXT("sourceOnly_notEstablished")) { Issue(Report, TEXT("SourceMappingUnavailable"), TEXT("source.observation.playbackMapping"), TEXT("source-only observation cannot write Montage time")); return false; }
		if (Manifest->TryGetStringField(TEXT("playbackMapping"), Mapping) && Mapping == TEXT("sourceOnly_notEstablished")) { Issue(Report, TEXT("SourceMappingUnavailable"), TEXT("source.observation.manifest.playbackMapping"), TEXT("source-only manifest cannot write Montage time")); return false; }
		FString ObservationTarget;
		FString ManifestTarget;
		const bool bHasObservationMapping = TryGetMappingTarget(Observation, ObservationTarget);
		const bool bHasManifestMapping = TryGetMappingTarget(Manifest, ManifestTarget);
		if (!bHasObservationMapping && !bHasManifestMapping) { Issue(Report, TEXT("SourceMappingUnavailable"), TEXT("source.observation.playbackMapping"), TEXT("observation or manifest must provide an explicit target Montage mapping")); return false; }
		if ((bHasObservationMapping && ObservationTarget != Patch.AssetPath) || (bHasManifestMapping && ManifestTarget != Patch.AssetPath)) { Issue(Report, TEXT("ObservationTargetMismatch"), TEXT("source.observation.playbackMapping"), TEXT("observation mapping target does not match patch asset")); return false; }
		if (bHasObservationMapping && bHasManifestMapping && ObservationTarget != ManifestTarget) { Issue(Report, TEXT("ObservationTargetMismatch"), TEXT("source.observation.playbackMapping"), TEXT("observation and manifest mapping targets disagree")); return false; }
		return true;
	}

	FString NotifyTag(const FAnimNotifyEvent& Event)
	{
		if (const USkillLabSkillAnimNotify* Notify = Cast<USkillLabSkillAnimNotify>(Event.Notify)) return Notify->EventTag.ToString();
		if (const USkillLabSkillAnimNotifyState* State = Cast<USkillLabSkillAnimNotifyState>(Event.NotifyStateClass)) return State->EventTag.ToString();
		return FString();
	}

	FString NotifySignature(const FAnimNotifyEvent& Event)
	{
		const FString Kind = Event.NotifyStateClass ? TEXT("state") : TEXT("instant");
		return FString::Printf(TEXT("%s|%s|%s|%.6f|%.6f"), *Kind, Event.NotifyName.IsNone() ? TEXT("<missing-id>") : *Event.NotifyName.ToString(), *NotifyTag(Event), Event.GetTriggerTime(), Event.GetEndTriggerTime());
	}

	FString ExpectedSignature(const FSkillLabAnimationPatchOperation& Operation)
	{
		return FString::Printf(TEXT("%s|%s|%s|%.6f|%.6f"), *Operation.Kind, *Operation.Id, *Operation.EventTag,
			Operation.Kind == TEXT("instant") ? Operation.Time : Operation.StartTime,
			Operation.Kind == TEXT("instant") ? Operation.Time : Operation.EndTime);
	}

	void ReadMontage(UAnimMontage* Montage, FSkillLabAnimationContentReport& Report)
	{
		if (!Montage) return;
		Report.AssetPath = Montage->GetPathName();
		Report.AssetClass = Montage->GetClass()->GetPathName();
		Report.SkeletonPath = Montage->GetSkeleton() ? Montage->GetSkeleton()->GetPathName() : FString();
		Report.RateScale = Montage->RateScale;
		Report.BlendInTime = Montage->BlendIn.GetBlendTime();
		Report.BlendOutTime = Montage->BlendOut.GetBlendTime();
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			Report.Slots.Add(Track.SlotName.ToString());
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments) if (Segment.GetAnimReference()) Report.SegmentAssets.Add(Segment.GetAnimReference()->GetPathName());
		}
		for (const FCompositeSection& Section : Montage->CompositeSections) Report.Sections.Add(FString::Printf(TEXT("%s@%.3f->%s"), *Section.SectionName.ToString(), Section.GetTime(), *Section.NextSectionName.ToString()));
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			const FString Id = Event.NotifyName.IsNone() ? TEXT("<missing-id>") : Event.NotifyName.ToString();
			Report.NotifyTags.Add(FString::Printf(TEXT("%s:%s@%.3f"), *Id, Event.Notify ? *Event.Notify->GetNotifyName() : *GetNameSafe(Event.NotifyStateClass), Event.GetTriggerTime()));
			Report.NotifyDetails.Add(NotifySignature(Event));
		}
	}

	bool ValidateMontage(UAnimMontage* Montage, FSkillLabAnimationContentReport& Report)
	{
		if (!Montage) { Issue(Report, TEXT("MissingAsset"), TEXT("asset"), TEXT("asset did not resolve to UAnimMontage")); return false; }
		if (!Montage->GetSkeleton()) Issue(Report, TEXT("MissingSkeleton"), TEXT("skeleton"), TEXT("Montage has no target Skeleton"));
		for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
		{
			const FAnimNotifyEvent& Event = Montage->Notifies[Index];
			if (!Event.Notify && !Event.NotifyStateClass) Issue(Report, TEXT("InvalidNotify"), FString::Printf(TEXT("notify[%d]"), Index), TEXT("Notify object is null"));
			if (Event.GetTriggerTime() < 0.0f || Event.GetTriggerTime() > Montage->GetPlayLength() || (Event.NotifyStateClass && (Event.GetEndTriggerTime() <= Event.GetTriggerTime() || Event.GetEndTriggerTime() > Montage->GetPlayLength()))) Issue(Report, TEXT("InvalidNotifyTime"), FString::Printf(TEXT("notify[%d].time"), Index), TEXT("Notify time is outside Montage length"));
		}
		return Report.Issues.Num() == 0;
	}

	int32 FindById(UAnimMontage* Montage, const FString& Id, int32& OutIndex)
	{
		int32 Count = 0;
		OutIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index) if (Montage->Notifies[Index].NotifyName == FName(*Id)) { ++Count; OutIndex = Index; }
		return Count;
	}

	void SetEvent(FAnimNotifyEvent& Event, UAnimMontage* Montage, const FSkillLabAnimationPatchOperation& Operation)
	{
		Event.NotifyName = FName(*Operation.Id);
		if (Operation.Kind == TEXT("instant"))
		{
			USkillLabSkillAnimNotify* Notify = Cast<USkillLabSkillAnimNotify>(Event.Notify);
			if (!Notify) Notify = NewObject<USkillLabSkillAnimNotify>(Montage);
			Notify->EventTag = FGameplayTag::RequestGameplayTag(FName(*Operation.EventTag), false);
			Event.Notify = Notify;
			Event.NotifyStateClass = nullptr;
			Event.Link(Montage, Operation.Time);
		}
		else
		{
			USkillLabSkillAnimNotifyState* State = Cast<USkillLabSkillAnimNotifyState>(Event.NotifyStateClass);
			if (!State) State = NewObject<USkillLabSkillAnimNotifyState>(Montage);
			State->EventTag = FGameplayTag::RequestGameplayTag(FName(*Operation.EventTag), false);
			Event.Notify = nullptr;
			Event.NotifyStateClass = State;
			Event.Link(Montage, Operation.StartTime);
			Event.EndLink.Link(Montage, Operation.EndTime);
			Event.SetDuration(Operation.EndTime - Operation.StartTime);
		}
	}

	struct FFileBackup { FString Path; TArray<uint8> Bytes; bool Exists = false; };

	void CaptureBackup(const FString& PackageFilename, TArray<FFileBackup>& Out)
	{
		for (const TCHAR* Extension : { TEXT("uasset"), TEXT("uexp"), TEXT("ubulk"), TEXT("uptnl") })
		{
			const FString Path = Extension == FString(TEXT("uasset")) ? PackageFilename : FPaths::ChangeExtension(PackageFilename, Extension);
			FFileBackup& Entry = Out.AddDefaulted_GetRef(); Entry.Path = Path; Entry.Exists = IFileManager::Get().FileExists(*Path);
			if (Entry.Exists) FFileHelper::LoadFileToArray(Entry.Bytes, *Path);
		}
	}

	bool RestoreBackup(const TArray<FFileBackup>& Backup)
	{
		bool bOk = true;
		for (const FFileBackup& Entry : Backup)
		{
			if (Entry.Exists) bOk &= FFileHelper::SaveArrayToFile(Entry.Bytes, *Entry.Path);
			else if (IFileManager::Get().FileExists(*Entry.Path)) bOk &= IFileManager::Get().Delete(*Entry.Path);
		}
		return bOk;
	}

	bool SameFiles(const TArray<FFileBackup>& Backup)
	{
		for (const FFileBackup& Entry : Backup)
		{
			if (Entry.Exists != IFileManager::Get().FileExists(*Entry.Path)) return false;
			if (!Entry.Exists) continue;
			TArray<uint8> Current;
			if (!FFileHelper::LoadFileToArray(Current, *Entry.Path) || Current != Entry.Bytes) return false;
		}
		return true;
	}

	FString SignatureId(const FString& Signature)
	{
		int32 FirstSeparator = INDEX_NONE;
		if (!Signature.FindChar(TEXT('|'), FirstSeparator)) return FString();
		const int32 IdStart = FirstSeparator + 1;
		const int32 SecondSeparator = Signature.Find(TEXT("|"), ESearchCase::CaseSensitive, ESearchDir::FromStart, IdStart);
		if (SecondSeparator <= IdStart) return FString();
		return Signature.Mid(IdStart, SecondSeparator - IdStart);
	}

	bool SameRetainedNotifyDetails(const TArray<FString>& Before, const TArray<FString>& After, const TSet<FString>& PatchedIds)
	{
		TArray<FString> BeforeRetained;
		TArray<FString> AfterRetained;
		for (const FString& Signature : Before) if (!PatchedIds.Contains(SignatureId(Signature))) BeforeRetained.Add(Signature);
		for (const FString& Signature : After) if (!PatchedIds.Contains(SignatureId(Signature))) AfterRetained.Add(Signature);
		BeforeRetained.Sort();
		AfterRetained.Sort();
		return BeforeRetained == AfterRetained;
	}

	void WriteSnapshot(const TSharedRef<FJsonObject>& Root, const TCHAR* Prefix, const FSkillLabAnimationContentReport& Snapshot)
	{
		Root->SetStringField(FString::Printf(TEXT("%sAsset"), Prefix), Snapshot.AssetPath);
		Root->SetStringField(FString::Printf(TEXT("%sClass"), Prefix), Snapshot.AssetClass);
		Root->SetStringField(FString::Printf(TEXT("%sSkeleton"), Prefix), Snapshot.SkeletonPath);
		TArray<TSharedPtr<FJsonValue>> Details;
		for (const FString& Value : Snapshot.NotifyDetails) Details.Add(MakeShared<FJsonValueString>(Value));
		Root->SetArrayField(FString::Printf(TEXT("%sNotifyDetails"), Prefix), Details);
	}

	bool SaveReport(const FString& Filename, const FSkillLabAnimationContentReport& Report, const FSkillLabAnimationContentReport* Before)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("asset"), Report.AssetPath); Root->SetStringField(TEXT("class"), Report.AssetClass); Root->SetStringField(TEXT("skeleton"), Report.SkeletonPath);
		Root->SetStringField(TEXT("sourceObservation"), Report.SourceObservation); Root->SetStringField(TEXT("sourceGenerationId"), Report.SourceGenerationId); Root->SetStringField(TEXT("sourceInputPreviewFingerprintSha256"), Report.SourceInputPreviewFingerprintSha256); Root->SetStringField(TEXT("duplicatePolicy"), Report.DuplicatePolicy);
		Root->SetBoolField(TEXT("saved"), Report.bSaved); Root->SetBoolField(TEXT("reloaded"), Report.bReloaded); Root->SetBoolField(TEXT("valid"), Report.bValid); Root->SetBoolField(TEXT("assetUnchanged"), Report.bAssetUnchanged); Root->SetBoolField(TEXT("diffValid"), Report.bDiffValid);
		Root->SetArrayField(TEXT("notifyDetails"), [&Report](){ TArray<TSharedPtr<FJsonValue>> A; for (const FString& S : Report.NotifyDetails) A.Add(MakeShared<FJsonValueString>(S)); return A; }());
		Root->SetArrayField(TEXT("retainedNotifyDetails"), [&Report](){ TArray<TSharedPtr<FJsonValue>> A; for (const FString& S : Report.RetainedNotifyDetails) A.Add(MakeShared<FJsonValueString>(S)); return A; }());
		if (Before)
		{
			WriteSnapshot(Root, TEXT("before."), *Before);
			WriteSnapshot(Root, TEXT("after."), Report);
		}
		TArray<TSharedPtr<FJsonValue>> Operations;
		for (const FSkillLabAnimationPatchOperationReport& Op : Report.Operations) { TSharedRef<FJsonObject> O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("id"), Op.Id); O->SetStringField(TEXT("op"), Op.Op); O->SetStringField(TEXT("status"), Op.Status); O->SetStringField(TEXT("before"), Op.Before); O->SetStringField(TEXT("after"), Op.After); O->SetStringField(TEXT("issueCode"), Op.IssueCode); Operations.Add(MakeShared<FJsonValueObject>(O)); }
		Root->SetArrayField(TEXT("operations"), Operations);
		TArray<TSharedPtr<FJsonValue>> Issues; for (const FSkillLabAnimationIssue& Entry : Report.Issues) { TSharedRef<FJsonObject> I = MakeShared<FJsonObject>(); I->SetStringField(TEXT("code"), Entry.Code); I->SetStringField(TEXT("location"), Entry.Location); I->SetStringField(TEXT("message"), Entry.Message); Issues.Add(MakeShared<FJsonValueObject>(I)); } Root->SetArrayField(TEXT("issues"), Issues);
		FString Text; if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text))) return false;
		return FFileHelper::SaveStringToFile(Text, *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

namespace SkillLabAnimationNotifyPatch
{
	bool IsPatchDocument(const FString& Filename)
	{
		FObject Root; FString Error;
		return ReadJsonFile(Filename, Root, Error) && (Root->HasField(TEXT("version")) || Root->HasField(TEXT("operations")) || Root->HasField(TEXT("source")) || Root->HasField(TEXT("duplicatePolicy")));
	}

	bool Parse(const FString& Text, FSkillLabAnimationPatch& Patch, FString& Error)
	{
		FObject Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { Error = TEXT("patch is not valid JSON"); return false; }
		if (!IsAllowed(Root, { TEXT("version"), TEXT("asset"), TEXT("source"), TEXT("duplicatePolicy"), TEXT("operations") }, Error)) return false;
		double Version = 0.0;
		if (!Number(Root, TEXT("version"), Version, Error) || Version != 1.0) { if (Error.IsEmpty()) Error = TEXT("version must be 1"); return false; }
		if (!NonEmptyString(Root, TEXT("asset"), Patch.AssetPath, Error) || !NonEmptyString(Root, TEXT("duplicatePolicy"), Patch.DuplicatePolicy, Error)) return false;
		if (Patch.DuplicatePolicy != TEXT("error") && Patch.DuplicatePolicy != TEXT("replace")) { Error = TEXT("duplicatePolicy must be 'error' or 'replace'"); return false; }
		const TSharedPtr<FJsonObject>* SourcePtr = nullptr;
		if (!Root->TryGetObjectField(TEXT("source"), SourcePtr) || !SourcePtr || !SourcePtr->IsValid()) { Error = TEXT("source must be an object"); return false; }
		if (!IsAllowed(*SourcePtr, { TEXT("observation"), TEXT("generationId"), TEXT("inputPreviewFingerprintSha256") }, Error)) return false;
		if (!NonEmptyString(*SourcePtr, TEXT("observation"), Patch.Observation, Error) || !NonEmptyString(*SourcePtr, TEXT("generationId"), Patch.GenerationId, Error) || !NonEmptyString(*SourcePtr, TEXT("inputPreviewFingerprintSha256"), Patch.InputPreviewFingerprintSha256, Error)) return false;
		if (!IsSha256(Patch.InputPreviewFingerprintSha256)) { Error = TEXT("inputPreviewFingerprintSha256 must contain 64 hex characters"); return false; }
		const TArray<TSharedPtr<FJsonValue>>* Operations = nullptr;
		if (!Root->HasField(TEXT("operations")) || !Root->TryGetArrayField(TEXT("operations"), Operations) || !Operations || Operations->Num() == 0) { Error = TEXT("operations must be a non-empty array"); return false; }
		TSet<FString> Ids;
		for (int32 Index = 0; Index < Operations->Num(); ++Index)
		{
			FSkillLabAnimationPatchOperation& Operation = Patch.Operations.AddDefaulted_GetRef();
			if (!ParseOperation((*Operations)[Index], Operation, Error)) { Error = FString::Printf(TEXT("operations[%d]: %s"), Index, *Error); return false; }
			if (Ids.Contains(Operation.Id)) { Error = FString::Printf(TEXT("duplicate operation id '%s'"), *Operation.Id); return false; }
			Ids.Add(Operation.Id);
		}
		return true;
	}

	int32 Run(const FString& SpecFilename, const FString& ReportFilename, const bool bWrite)
	{
		FSkillLabAnimationContentReport Report;
		FString Text;
		FSkillLabAnimationPatch Patch;
		FString ParseError;
		if (!FFileHelper::LoadFileToString(Text, *SpecFilename) || !Parse(Text, Patch, ParseError))
		{
			Issue(Report, TEXT("InvalidPatch"), TEXT("spec"), ParseError.IsEmpty() ? TEXT("cannot read patch file") : ParseError);
			Report.bAssetUnchanged = true;
			Report.bValid = false;
			SaveReport(ReportFilename, Report, nullptr);
			return 3;
		}
		Report.AssetPath = Patch.AssetPath; Report.SourceObservation = Patch.Observation; Report.SourceGenerationId = Patch.GenerationId; Report.SourceInputPreviewFingerprintSha256 = Patch.InputPreviewFingerprintSha256; Report.DuplicatePolicy = Patch.DuplicatePolicy;
		const FString ObjectPath = Patch.AssetPath;
		UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *ObjectPath));
		ReadMontage(Montage, Report);
		ValidateMontage(Montage, Report);
		ValidateObservation(Patch, Report);
		if (!Montage) { Report.bValid = false; SaveReport(ReportFilename, Report, nullptr); return 4; }
		const FString PackageFilename = FPackageName::LongPackageNameToFilename(Montage->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		UPackage* TargetPackage = Montage->GetOutermost();
		TArray<FFileBackup> Backup; CaptureBackup(PackageFilename, Backup);
		TMap<FString, int32> Existing;
		for (const FSkillLabAnimationPatchOperation& Operation : Patch.Operations)
		{
			int32 Index = INDEX_NONE;
			const int32 Count = FindById(Montage, Operation.Id, Index);
			if (Count > 1) Issue(Report, TEXT("DuplicateIdentity"), Operation.Id, TEXT("target Montage contains duplicate stable identities"));
			if (Operation.Op == TEXT("delete") && Count == 0) Issue(Report, TEXT("MissingIdentity"), Operation.Id, TEXT("delete target identity does not exist"));
			if (Operation.Op == TEXT("upsert") && Count > 0 && Patch.DuplicatePolicy == TEXT("error")) Issue(Report, TEXT("DuplicateIdentity"), Operation.Id, TEXT("upsert would modify an existing identity while duplicatePolicy is error"));
			if (Operation.Op == TEXT("upsert"))
			{
				const float MaxTime = Montage->GetPlayLength();
				if ((Operation.Kind == TEXT("instant") && Operation.Time > MaxTime) || (Operation.Kind == TEXT("state") && (Operation.StartTime > MaxTime || Operation.EndTime > MaxTime))) Issue(Report, TEXT("InvalidNotifyTime"), Operation.Id, TEXT("operation time is outside Montage length"));
			}
			Existing.Add(Operation.Id, Index);
		}
		FSkillLabAnimationContentReport BeforeReport = Report;
		if (Report.Issues.Num() > 0 || !bWrite)
		{
			for (const FSkillLabAnimationPatchOperation& Operation : Patch.Operations)
			{
				FSkillLabAnimationPatchOperationReport& O = Report.Operations.AddDefaulted_GetRef(); O.Id = Operation.Id; O.Op = Operation.Op; O.Status = Report.Issues.Num() == 0 ? TEXT("validated") : TEXT("rejected");
				for (const FSkillLabAnimationIssue& Entry : Report.Issues) if (Entry.Location == Operation.Id || Entry.Location.Contains(Operation.Id)) { O.IssueCode = Entry.Code; break; }
			}
			Report.bDiffValid = Report.Issues.Num() == 0;
			Report.bValid = Report.Issues.Num() == 0;
			Report.bAssetUnchanged = SameFiles(Backup);
			SaveReport(ReportFilename, Report, &BeforeReport);
			return Report.bValid ? 0 : 4;
		}
		for (const FSkillLabAnimationPatchOperation& Operation : Patch.Operations)
		{
			int32 Index = INDEX_NONE; const int32 Count = FindById(Montage, Operation.Id, Index);
			FSkillLabAnimationPatchOperationReport& O = Report.Operations.AddDefaulted_GetRef(); O.Id = Operation.Id; O.Op = Operation.Op; O.Before = Count == 1 ? NotifySignature(Montage->Notifies[Index]) : FString();
			if (Operation.Op == TEXT("delete")) { Montage->Notifies.RemoveAt(Index); O.Status = TEXT("deleted"); }
			else { if (Count == 0) { Index = Montage->Notifies.AddDefaulted(); } SetEvent(Montage->Notifies[Index], Montage, Operation); O.Status = Count == 0 ? TEXT("created") : TEXT("updated"); }
		}
		Montage->SortNotifies(); Montage->MarkPackageDirty();
		FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone; SaveArgs.SaveFlags = SAVE_NoError;
		Report.bSaved = UPackage::SavePackage(Montage->GetOutermost(), Montage, *PackageFilename, SaveArgs);
		if (Report.bSaved)
		{
			// ReloadPackage replaces the package and its subobjects from disk and
			// fixes up references. ResetLoaders alone would reuse the live Montage.
			const TWeakObjectPtr<UPackage> PreviousPackage = TargetPackage;
			UPackage* ReloadedPackage = ReloadPackage(TargetPackage, LOAD_None);
			TargetPackage = ReloadedPackage ? ReloadedPackage : PreviousPackage.Get();
			Montage = ReloadedPackage ? Cast<UAnimMontage>(StaticFindObject(UAnimMontage::StaticClass(), nullptr, *ObjectPath)) : nullptr;
			Report.bReloaded = Montage != nullptr;
		}
		if (Report.bReloaded && Montage)
		{
			Report.AssetClass.Reset(); Report.SkeletonPath.Reset(); Report.Slots.Reset(); Report.Sections.Reset(); Report.SegmentAssets.Reset(); Report.NotifyTags.Reset(); Report.NotifyDetails.Reset();
			ReadMontage(Montage, Report);
			ValidateMontage(Montage, Report);
			for (FSkillLabAnimationPatchOperationReport& O : Report.Operations)
			{
				int32 Index = INDEX_NONE; const int32 Count = FindById(Montage, O.Id, Index); O.After = Count == 1 ? NotifySignature(Montage->Notifies[Index]) : FString();
			}
			Report.bDiffValid = true;
			for (const FSkillLabAnimationPatchOperationReport& O : Report.Operations)
			{
				const FSkillLabAnimationPatchOperation* Requested = Patch.Operations.FindByPredicate([&O](const FSkillLabAnimationPatchOperation& Candidate){ return Candidate.Id == O.Id; });
				if (!Requested || (O.Op == TEXT("delete") ? !O.After.IsEmpty() : O.After != ExpectedSignature(*Requested)))
				{
					Report.bDiffValid = false;
					Issue(Report, TEXT("DiffMismatch"), O.Id, FString::Printf(TEXT("read back '%s', expected '%s'"), *O.After, Requested ? *ExpectedSignature(*Requested) : TEXT("<missing-operation>")));
				}
			}
			if (Report.Issues.Num() > 0) Report.bDiffValid = false;
			TSet<FString> PatchedIds;
			for (const FSkillLabAnimationPatchOperation& Operation : Patch.Operations) PatchedIds.Add(Operation.Id);
			for (const FString& Signature : BeforeReport.NotifyDetails)
			{
				const FString Id = SignatureId(Signature);
				if (!PatchedIds.Contains(Id)) Report.RetainedNotifyDetails.Add(Signature);
			}
			if (!SameRetainedNotifyDetails(BeforeReport.NotifyDetails, Report.NotifyDetails, PatchedIds)) Report.bDiffValid = false;
		}
		if (!Report.bSaved || !Report.bReloaded || !Report.bDiffValid)
		{
			const bool bRollbackFiles = RestoreBackup(Backup);
			UPackage* RestoredPackage = bRollbackFiles && TargetPackage ? ReloadPackage(TargetPackage, LOAD_None) : nullptr;
			UAnimMontage* RestoredMontage = RestoredPackage ? Cast<UAnimMontage>(StaticFindObject(UAnimMontage::StaticClass(), nullptr, *ObjectPath)) : nullptr;
			if (RestoredMontage)
			{
				Report.AssetClass.Reset(); Report.SkeletonPath.Reset(); Report.Slots.Reset(); Report.Sections.Reset(); Report.SegmentAssets.Reset(); Report.NotifyTags.Reset(); Report.NotifyDetails.Reset();
				ReadMontage(RestoredMontage, Report);
			}
			Report.bAssetUnchanged = SameFiles(Backup);
			Report.bValid = false;
			const FString FailureCode = !Report.bSaved ? TEXT("SaveFailed") : !Report.bReloaded ? TEXT("ReloadFailed") : TEXT("DiffFailed");
			Issue(Report, FailureCode, TEXT("asset"), TEXT("Patch transaction was rolled back"));
			if (!bRollbackFiles || !Report.bAssetUnchanged) Issue(Report, TEXT("RollbackFailed"), TEXT("asset"), TEXT("original package bytes could not be fully restored"));
			for (FSkillLabAnimationPatchOperationReport& O : Report.Operations) if (O.IssueCode.IsEmpty()) O.IssueCode = FailureCode;
		}
		else { Report.bAssetUnchanged = false; Report.bValid = true; }
		SaveReport(ReportFilename, Report, &BeforeReport);
		return Report.bValid ? 0 : 4;
	}
}
