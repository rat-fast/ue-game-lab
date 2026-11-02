#include "SkillLabAnimationContentCommandlet.h"
#include "SkillLabAnimationContentTypes.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AlphaBlend.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "GameplayTagsManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/SavePackage.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "SkillLabSkillAnimNotify.h"
#include "SkillLabAnimationNotifyPatch.h"

namespace
{
	FString GetParam(const TMap<FString, FString>& Params, const TCHAR* Key)
	{
		if (const FString* Value = Params.Find(Key))
		{
			return *Value;
		}
		return FString();
	}

	bool HasFlag(const FString& Params, const TCHAR* Flag)
	{
		return Params.Contains(Flag, ESearchCase::IgnoreCase);
	}

	bool AddIssue(FSkillLabAnimationContentReport& Report, const FString& Code, const FString& Location, const FString& Message)
	{
		FSkillLabAnimationIssue& Issue = Report.Issues.AddDefaulted_GetRef();
		Issue.Code = Code;
		Issue.Location = Location;
		Issue.Message = Message;
		return false;
	}

	bool ReadSpec(const FString& Filename, FSkillLabAnimationContentSpec& OutSpec, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Filename))
		{
			OutError = FString::Printf(TEXT("cannot read spec file: %s"), *Filename);
			return false;
		}
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutError = TEXT("spec is not valid JSON");
			return false;
		}
		if (!Root->TryGetStringField(TEXT("asset"), OutSpec.AssetPath) || OutSpec.AssetPath.IsEmpty())
		{
			OutError = TEXT("missing required field: asset");
			return false;
		}
		const TSet<FString> AllowedFields = { TEXT("asset"), TEXT("rateScale"), TEXT("blendInTime"), TEXT("blendOutTime"), TEXT("slot"), TEXT("section"), TEXT("notify"), TEXT("segment") };
		for (const auto& Pair : Root->Values)
		{
			const FString FieldName(Pair.Key); if (!AllowedFields.Contains(FieldName)) { OutError = FString::Printf(TEXT("unsupported field: %s"), *FieldName); return false; }
		}
		if (Root->HasField(TEXT("rateScale"))) { double Value = 0.0; if (!Root->TryGetNumberField(TEXT("rateScale"), Value)) { OutError = TEXT("rateScale must be a number"); return false; } OutSpec.RateScale = static_cast<float>(Value); }
		if (Root->HasField(TEXT("blendInTime"))) { double Value = 0.0; if (!Root->TryGetNumberField(TEXT("blendInTime"), Value)) { OutError = TEXT("blendInTime must be a number"); return false; } OutSpec.BlendInTime = static_cast<float>(Value); }
		if (Root->HasField(TEXT("blendOutTime"))) { double Value = 0.0; if (!Root->TryGetNumberField(TEXT("blendOutTime"), Value)) { OutError = TEXT("blendOutTime must be a number"); return false; } OutSpec.BlendOutTime = static_cast<float>(Value); }
		if (Root->HasField(TEXT("slot"))) { FString SlotValue; if (!Root->TryGetStringField(TEXT("slot"), SlotValue)) { OutError = TEXT("slot must be a string"); return false; } OutSpec.SlotName = SlotValue; }
		const TSharedPtr<FJsonObject>* SectionPtr = nullptr;
		if (Root->TryGetObjectField(TEXT("section"), SectionPtr) && SectionPtr && SectionPtr->IsValid())
		{
			const TSharedPtr<FJsonObject>& Section = *SectionPtr;
			for (const auto& Pair : Section->Values) { const FString FieldName(Pair.Key); if (!TSet<FString>{ TEXT("name"), TEXT("startTime"), TEXT("next") }.Contains(FieldName)) { OutError = FString::Printf(TEXT("unsupported section field: %s"), *FieldName); return false; } }
			FString SectionValue;
			if (!Section->TryGetStringField(TEXT("name"), SectionValue) || SectionValue.IsEmpty()) { OutError = TEXT("section.name is required and must be a non-empty string"); return false; }
			OutSpec.SectionName = SectionValue;
			if (Section->HasField(TEXT("startTime"))) { double SectionStart = 0.0; if (!Section->TryGetNumberField(TEXT("startTime"), SectionStart)) { OutError = TEXT("section.startTime must be a number"); return false; } OutSpec.SectionStartTime = static_cast<float>(SectionStart); }
			if (Section->TryGetStringField(TEXT("next"), SectionValue)) OutSpec.NextSectionName = SectionValue;
		}
		const TSharedPtr<FJsonObject>* NotifyPtr = nullptr;
		if (Root->TryGetObjectField(TEXT("notify"), NotifyPtr) && NotifyPtr && NotifyPtr->IsValid())
		{
			const TSharedPtr<FJsonObject>& Notify = *NotifyPtr;
			for (const auto& Pair : Notify->Values) { const FString FieldName(Pair.Key); if (!TSet<FString>{ TEXT("time"), TEXT("eventTag") }.Contains(FieldName)) { OutError = FString::Printf(TEXT("unsupported notify field: %s"), *FieldName); return false; } }
			FString NotifyValue;
			if (!Notify->HasField(TEXT("time")) || !Notify->HasField(TEXT("eventTag"))) { OutError = TEXT("notify requires both time and eventTag"); return false; }
			double NotifyTime = 0.0;
			if (!Notify->TryGetNumberField(TEXT("time"), NotifyTime)) { OutError = TEXT("notify.time must be a number"); return false; }
			OutSpec.NotifyTime = static_cast<float>(NotifyTime);
			if (!Notify->TryGetStringField(TEXT("eventTag"), NotifyValue) || NotifyValue.IsEmpty()) { OutError = TEXT("notify.eventTag is required and must be a non-empty string"); return false; }
			OutSpec.NotifyEventTag = NotifyValue;
		}
		if (Root->HasField(TEXT("segment"))) { FString SegmentValue; if (!Root->TryGetStringField(TEXT("segment"), SegmentValue) || SegmentValue.IsEmpty()) { OutError = TEXT("segment must be a non-empty string"); return false; } OutSpec.SegmentAssetPath = SegmentValue; }
		if (OutSpec.RateScale.IsSet() && OutSpec.RateScale.GetValue() <= 0.0f) OutError = TEXT("rateScale must be greater than zero");
		if (OutSpec.BlendInTime.IsSet() && OutSpec.BlendInTime.GetValue() < 0.0f) OutError = TEXT("blendInTime cannot be negative");
		if (OutSpec.BlendOutTime.IsSet() && OutSpec.BlendOutTime.GetValue() < 0.0f) OutError = TEXT("blendOutTime cannot be negative");
		if (OutSpec.SlotName.IsSet() && OutSpec.SlotName.GetValue().IsEmpty()) OutError = TEXT("slot cannot be empty");
		if (OutSpec.SlotName.IsSet() && OutSpec.SlotName.GetValue().Contains(TEXT("."))) OutError = TEXT("slot must be a slot name; SlotGroup.SlotName cannot be written without a resolved slot asset");
		if ((OutSpec.NotifyTime.IsSet() && !OutSpec.NotifyEventTag.IsSet()) || (!OutSpec.NotifyTime.IsSet() && OutSpec.NotifyEventTag.IsSet())) OutError = TEXT("notify requires both time and eventTag");
		if (!OutError.IsEmpty()) return false;
		return true;
	}

	void ReadMontage(UAnimMontage* Montage, FSkillLabAnimationContentReport& Report)
	{
		if (Montage) Report.AssetPath = Montage->GetPathName();
		Report.AssetClass = Montage ? Montage->GetClass()->GetPathName() : FString();
		if (!Montage) return;
		Report.RateScale = Montage->RateScale;
		Report.BlendInTime = Montage->BlendIn.GetBlendTime();
		Report.BlendOutTime = Montage->BlendOut.GetBlendTime();
		if (Montage->GetSkeleton()) Report.SkeletonPath = Montage->GetSkeleton()->GetPathName();
		for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
		{
			Report.Slots.Add(Track.SlotName.ToString());
			for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
			{
				if (Segment.GetAnimReference()) Report.SegmentAssets.Add(Segment.GetAnimReference()->GetPathName());
			}
		}
		for (const FCompositeSection& Section : Montage->CompositeSections)
		{
			Report.Sections.Add(FString::Printf(TEXT("%s@%.3f->%s"), *Section.SectionName.ToString(), Section.GetTime(), *Section.NextSectionName.ToString()));
		}
		for (const FAnimNotifyEvent& Notify : Montage->Notifies)
		{
			const FString Id = Notify.NotifyName.IsNone() ? TEXT("<missing-id>") : Notify.NotifyName.ToString();
			FString Name = Notify.Notify ? Notify.Notify->GetNotifyName() : (Notify.NotifyStateClass ? GetNameSafe(Notify.NotifyStateClass) : TEXT("<null>"));
			Report.NotifyTags.Add(FString::Printf(TEXT("%s:%s@%.3f"), *Id, *Name, Notify.GetTriggerTime()));
			const FGameplayTag Tag = Notify.Notify ? Cast<USkillLabSkillAnimNotify>(Notify.Notify) ? Cast<USkillLabSkillAnimNotify>(Notify.Notify)->EventTag : FGameplayTag()
				: (Notify.NotifyStateClass && Cast<USkillLabSkillAnimNotifyState>(Notify.NotifyStateClass) ? Cast<USkillLabSkillAnimNotifyState>(Notify.NotifyStateClass)->EventTag : FGameplayTag());
			Report.NotifyDetails.Add(Notify.NotifyStateClass
				? FString::Printf(TEXT("%s|state|%s|%.3f|%.3f"), *Id, *Tag.ToString(), Notify.GetTriggerTime(), Notify.GetEndTriggerTime())
				: FString::Printf(TEXT("%s|instant|%s|%.3f"), *Id, *Tag.ToString(), Notify.GetTriggerTime()));
		}
	}

	bool ValidateMontage(UAnimMontage* Montage, FSkillLabAnimationContentReport& Report)
	{
		if (!Montage) return AddIssue(Report, TEXT("MissingAsset"), TEXT("asset"), TEXT("asset did not resolve to UAnimMontage"));
		if (!Montage->GetSkeleton()) AddIssue(Report, TEXT("MissingSkeleton"), TEXT("skeleton"), TEXT("Montage has no target Skeleton"));
		if (Montage->RateScale <= 0.0f) AddIssue(Report, TEXT("InvalidPlayRate"), TEXT("rateScale"), TEXT("RateScale must be greater than zero"));
		if (Montage->BlendIn.GetBlendTime() < 0.0f || Montage->BlendOut.GetBlendTime() < 0.0f) AddIssue(Report, TEXT("InvalidBlend"), TEXT("blend"), TEXT("Blend times cannot be negative"));
		for (int32 Index = 0; Index < Montage->SlotAnimTracks.Num(); ++Index)
		{
			const FSlotAnimationTrack& Track = Montage->SlotAnimTracks[Index];
			if (Track.SlotName.IsNone()) AddIssue(Report, TEXT("InvalidSlot"), FString::Printf(TEXT("slot[%d]"), Index), TEXT("Slot name is empty"));
			for (int32 SegmentIndex = 0; SegmentIndex < Track.AnimTrack.AnimSegments.Num(); ++SegmentIndex)
			{
				if (!Track.AnimTrack.AnimSegments[SegmentIndex].GetAnimReference())
					AddIssue(Report, TEXT("MissingSegment"), FString::Printf(TEXT("slot[%d].segment[%d]"), Index, SegmentIndex), TEXT("Animation segment reference is null"));
				else if (Montage->GetSkeleton() && Track.AnimTrack.AnimSegments[SegmentIndex].GetAnimReference()->GetSkeleton()
					&& Montage->GetSkeleton() != Track.AnimTrack.AnimSegments[SegmentIndex].GetAnimReference()->GetSkeleton())
					AddIssue(Report, TEXT("IncompatibleSkeleton"), FString::Printf(TEXT("slot[%d].segment[%d].skeleton"), Index, SegmentIndex), TEXT("Animation segment Skeleton differs from Montage Skeleton"));
			}
		}
		for (int32 Index = 0; Index < Montage->CompositeSections.Num(); ++Index)
		{
			const FCompositeSection& Section = Montage->CompositeSections[Index];
			if (Section.SectionName.IsNone()) AddIssue(Report, TEXT("InvalidSection"), FString::Printf(TEXT("section[%d]"), Index), TEXT("Section name is empty"));
			if (Section.GetTime() < 0.0f || Section.GetTime() > Montage->GetPlayLength()) AddIssue(Report, TEXT("InvalidSectionTime"), FString::Printf(TEXT("section[%d].startTime"), Index), TEXT("Section start is outside Montage length"));
			if (!Section.NextSectionName.IsNone() && !Montage->CompositeSections.ContainsByPredicate([&](const FCompositeSection& Other){ return Other.SectionName == Section.NextSectionName; }))
				AddIssue(Report, TEXT("InvalidSectionLink"), FString::Printf(TEXT("section[%d].next"), Index), TEXT("NextSectionName does not resolve to a section"));
		}
		for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
		{
			const FAnimNotifyEvent& Notify = Montage->Notifies[Index];
			if (!Notify.Notify && !Notify.NotifyStateClass) AddIssue(Report, TEXT("InvalidNotify"), FString::Printf(TEXT("notify[%d]"), Index), TEXT("Notify object is null"));
			if (Notify.GetTriggerTime() < 0.0f || Notify.GetTriggerTime() > Montage->GetPlayLength()) AddIssue(Report, TEXT("InvalidNotifyTime"), FString::Printf(TEXT("notify[%d].time"), Index), TEXT("Notify time is outside Montage length"));
		}
		return Report.Issues.Num() == 0;
	}

	void ApplySpec(UAnimMontage* Montage, const FSkillLabAnimationContentSpec& Spec, FSkillLabAnimationContentReport& Report)
	{
		if (Spec.RateScale.IsSet()) Montage->RateScale = FMath::Max(0.001f, Spec.RateScale.GetValue());
		if (Spec.BlendInTime.IsSet()) Montage->BlendIn.SetBlendTime(FMath::Max(0.0f, Spec.BlendInTime.GetValue()));
		if (Spec.BlendOutTime.IsSet()) Montage->BlendOut.SetBlendTime(FMath::Max(0.0f, Spec.BlendOutTime.GetValue()));
		if (Spec.SlotName.IsSet())
		{
			if (Montage->SlotAnimTracks.Num() == 0) Montage->SlotAnimTracks.AddDefaulted();
			Montage->SlotAnimTracks[0].SlotName = FName(*Spec.SlotName.GetValue());
		}
		if (Spec.SectionName.IsSet())
		{
			FCompositeSection* Found = Montage->CompositeSections.FindByPredicate([&](const FCompositeSection& Section){ return Section.SectionName == FName(*Spec.SectionName.GetValue()); });
			if (!Found) Found = &Montage->CompositeSections.AddDefaulted_GetRef();
			Found->SectionName = FName(*Spec.SectionName.GetValue());
			if (Spec.SectionStartTime.IsSet()) Found->SetTime(FMath::Max(0.0f, Spec.SectionStartTime.GetValue()));
			if (Spec.NextSectionName.IsSet()) Found->NextSectionName = FName(*Spec.NextSectionName.GetValue());
		}
		if (Spec.NotifyTime.IsSet() && Spec.NotifyEventTag.IsSet())
		{
			const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(*Spec.NotifyEventTag.GetValue()), false);
			USkillLabSkillAnimNotify* NotifyObject = nullptr;
			FAnimNotifyEvent* ExistingEvent = Montage->Notifies.FindByPredicate([&](const FAnimNotifyEvent& Existing)
			{
				const USkillLabSkillAnimNotify* ExistingNotify = Cast<USkillLabSkillAnimNotify>(Existing.Notify);
				return ExistingNotify && ExistingNotify->EventTag == EventTag;
			});
			if (ExistingEvent) NotifyObject = Cast<USkillLabSkillAnimNotify>(ExistingEvent->Notify);
			if (!NotifyObject) NotifyObject = NewObject<USkillLabSkillAnimNotify>(Montage);
			NotifyObject->EventTag = EventTag;
			if (!NotifyObject->EventTag.IsValid())
			{
				AddIssue(Report, TEXT("InvalidNotifyTag"), TEXT("notify.eventTag"), Spec.NotifyEventTag.GetValue());
			}
			else
			{
				FAnimNotifyEvent& Event = ExistingEvent ? *ExistingEvent : Montage->Notifies.AddDefaulted_GetRef();
				Event.Notify = NotifyObject;
				Event.Link(Montage, FMath::Clamp(Spec.NotifyTime.GetValue(), 0.0f, Montage->GetPlayLength()));
			}
		}
		if (Spec.SegmentAssetPath.IsSet())
		{
			UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(StaticLoadObject(UAnimSequenceBase::StaticClass(), nullptr, *Spec.SegmentAssetPath.GetValue()));
			if (!Sequence)
			{
				AddIssue(Report, TEXT("MissingSegmentAsset"), TEXT("segment"), Spec.SegmentAssetPath.GetValue());
			}
			else if (!Montage->GetSkeleton())
			{
				AddIssue(Report, TEXT("MissingSkeleton"), TEXT("montage.skeleton"), TEXT("Montage has no Skeleton for segment compatibility"));
			}
			else if (!Sequence->GetSkeleton())
			{
				AddIssue(Report, TEXT("MissingSegmentSkeleton"), TEXT("segment.skeleton"), FString::Printf(TEXT("%s has no Skeleton"), *Sequence->GetPathName()));
			}
			else if (Sequence->GetSkeleton() != Montage->GetSkeleton())
			{
				AddIssue(Report, TEXT("IncompatibleSkeleton"), TEXT("segment.skeleton"), FString::Printf(TEXT("%s is not compatible with Montage Skeleton"), *Sequence->GetPathName()));
			}
			else
			{
				if (Montage->SlotAnimTracks.Num() == 0) Montage->SlotAnimTracks.AddDefaulted();
				FAnimTrack& Track = Montage->SlotAnimTracks[0].AnimTrack;
				if (Track.AnimSegments.Num() == 0) Track.AnimSegments.AddDefaulted();
				FAnimSegment& Segment = Track.AnimSegments[0];
				Segment.SetAnimReference(Sequence, true);
				Segment.AnimStartTime = 0.0f;
				Segment.AnimEndTime = Sequence->GetPlayLength();
				Segment.AnimPlayRate = 1.0f;
				Segment.LoopingCount = 1;
			}
		}
		Montage->SortNotifies();
	}

	void WriteSnapshot(const TSharedRef<FJsonObject>& Root, const TCHAR* Prefix, const FSkillLabAnimationContentReport& Report)
	{
		Root->SetStringField(FString::Printf(TEXT("%sAsset"), Prefix), Report.AssetPath);
		Root->SetStringField(FString::Printf(TEXT("%sClass"), Prefix), Report.AssetClass);
		Root->SetStringField(FString::Printf(TEXT("%sSkeleton"), Prefix), Report.SkeletonPath);
		Root->SetNumberField(FString::Printf(TEXT("%sRateScale"), Prefix), Report.RateScale);
		Root->SetNumberField(FString::Printf(TEXT("%sBlendInTime"), Prefix), Report.BlendInTime);
		Root->SetNumberField(FString::Printf(TEXT("%sBlendOutTime"), Prefix), Report.BlendOutTime);
		auto SetStrings = [&Root, Prefix](const TCHAR* Name, const TArray<FString>& Values)
		{
			TArray<TSharedPtr<FJsonValue>> Json;
			for (const FString& Value : Values) Json.Add(MakeShared<FJsonValueString>(Value));
			Root->SetArrayField(FString::Printf(TEXT("%s%s"), Prefix, Name), Json);
		};
		SetStrings(TEXT("Slots"), Report.Slots);
		SetStrings(TEXT("Sections"), Report.Sections);
		SetStrings(TEXT("Segments"), Report.SegmentAssets);
		SetStrings(TEXT("Notifies"), Report.NotifyTags);
	}

	void WriteReport(const FString& Filename, const FSkillLabAnimationContentReport& Report, const FSkillLabAnimationContentReport* BeforeReport = nullptr)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("asset"), Report.AssetPath);
		Root->SetStringField(TEXT("class"), Report.AssetClass);
		Root->SetStringField(TEXT("skeleton"), Report.SkeletonPath);
		Root->SetNumberField(TEXT("rateScale"), Report.RateScale);
		Root->SetNumberField(TEXT("blendInTime"), Report.BlendInTime);
		Root->SetNumberField(TEXT("blendOutTime"), Report.BlendOutTime);
		Root->SetBoolField(TEXT("saved"), Report.bSaved);
		Root->SetBoolField(TEXT("reloaded"), Report.bReloaded);
		Root->SetBoolField(TEXT("valid"), Report.bValid);
		Root->SetBoolField(TEXT("assetUnchanged"), Report.bAssetUnchanged);
		Root->SetBoolField(TEXT("diffValid"), Report.bDiffValid);
		Root->SetStringField(TEXT("sourceObservation"), Report.SourceObservation);
		Root->SetStringField(TEXT("sourceGenerationId"), Report.SourceGenerationId);
		Root->SetStringField(TEXT("sourceInputPreviewFingerprintSha256"), Report.SourceInputPreviewFingerprintSha256);
		Root->SetStringField(TEXT("duplicatePolicy"), Report.DuplicatePolicy);
		auto SetStrings = [&Root](const TCHAR* Name, const TArray<FString>& Values){ TArray<TSharedPtr<FJsonValue>> Json; for (const FString& Value : Values) Json.Add(MakeShared<FJsonValueString>(Value)); Root->SetArrayField(Name, Json); };
		SetStrings(TEXT("slots"), Report.Slots); SetStrings(TEXT("sections"), Report.Sections); SetStrings(TEXT("segments"), Report.SegmentAssets); SetStrings(TEXT("notifies"), Report.NotifyTags); SetStrings(TEXT("notifyDetails"), Report.NotifyDetails);
		TArray<TSharedPtr<FJsonValue>> OperationValues;
		for (const FSkillLabAnimationPatchOperationReport& Operation : Report.Operations)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("id"), Operation.Id); Json->SetStringField(TEXT("op"), Operation.Op); Json->SetStringField(TEXT("status"), Operation.Status); Json->SetStringField(TEXT("before"), Operation.Before); Json->SetStringField(TEXT("after"), Operation.After); Json->SetStringField(TEXT("issueCode"), Operation.IssueCode); OperationValues.Add(MakeShared<FJsonValueObject>(Json));
		}
		Root->SetArrayField(TEXT("operations"), OperationValues);
		if (BeforeReport)
		{
			WriteSnapshot(Root, TEXT("before."), *BeforeReport);
			WriteSnapshot(Root, TEXT("after."), Report);
		}
		TArray<TSharedPtr<FJsonValue>> Issues;
		for (const FSkillLabAnimationIssue& Issue : Report.Issues)
		{
			TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>(); Json->SetStringField(TEXT("code"), Issue.Code); Json->SetStringField(TEXT("location"), Issue.Location); Json->SetStringField(TEXT("message"), Issue.Message); Issues.Add(MakeShared<FJsonValueObject>(Json));
		}
		Root->SetArrayField(TEXT("issues"), Issues);
		FString Text; FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text)); FFileHelper::SaveStringToFile(Text, *Filename);
	}

}

USkillLabAnimationContentCommandlet::USkillLabAnimationContentCommandlet()
{
	IsEditor = true;
	LogToConsole = true;
}

int32 USkillLabAnimationContentCommandlet::Main(const FString& Params)
{
	TMap<FString, FString> Parsed;
	FParse::Value(*Params, TEXT("Spec="), Parsed.FindOrAdd(TEXT("Spec")));
	FParse::Value(*Params, TEXT("Report="), Parsed.FindOrAdd(TEXT("Report")));
	const FString SpecFilename = GetParam(Parsed, TEXT("Spec"));
	const FString ReportFilename = GetParam(Parsed, TEXT("Report"));
	if (SpecFilename.IsEmpty() || ReportFilename.IsEmpty()) { UE_LOG(LogTemp, Error, TEXT("Usage: -run=SkillLabAnimationContent -Spec=<json> -Report=<json> [-Write]")); return 2; }
	if (SkillLabAnimationNotifyPatch::IsPatchDocument(SpecFilename))
	{
		return SkillLabAnimationNotifyPatch::Run(SpecFilename, ReportFilename, HasFlag(Params, TEXT("-Write")));
	}

	FSkillLabAnimationContentSpec Spec; FString Error;
	FSkillLabAnimationContentReport Report;
	TOptional<FSkillLabAnimationContentReport> BeforeReport;
	if (!ReadSpec(SpecFilename, Spec, Error))
	{
		FSkillLabAnimationIssue& Issue = Report.Issues.AddDefaulted_GetRef(); Issue.Code = TEXT("InvalidSpec"); Issue.Location = TEXT("spec"); Issue.Message = Error;
		WriteReport(ReportFilename, Report); UE_LOG(LogTemp, Error, TEXT("[SkillLabAnimation] %s"), *Error); return 3;
	}
	Report.AssetPath = Spec.AssetPath;
	UAnimMontage* Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *Spec.AssetPath));
	ReadMontage(Montage, Report);
	ValidateMontage(Montage, Report);
	if (HasFlag(Params, TEXT("-Write")) && Montage && Report.Issues.Num() == 0)
	{
		BeforeReport = Report;
		ApplySpec(Montage, Spec, Report);
		if (Report.Issues.Num() == 0)
		{
			Montage->MarkPackageDirty();
			FSavePackageArgs SaveArgs; SaveArgs.TopLevelFlags = RF_Public | RF_Standalone; SaveArgs.SaveFlags = SAVE_NoError;
			Report.bSaved = UPackage::SavePackage(Montage->GetOutermost(), Montage, *FPackageName::LongPackageNameToFilename(Montage->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), SaveArgs);
		}
		if (Report.bSaved)
		{
			ResetLoaders(Montage->GetOutermost());
			Montage = Cast<UAnimMontage>(StaticLoadObject(UAnimMontage::StaticClass(), nullptr, *Spec.AssetPath));
			const bool bReloaded = Montage != nullptr;
			FSkillLabAnimationContentReport AfterReport;
			AfterReport.AssetPath = Spec.AssetPath;
			AfterReport.bSaved = Report.bSaved;
			AfterReport.bReloaded = bReloaded;
			ReadMontage(Montage, AfterReport);
			ValidateMontage(Montage, AfterReport);
			Report = MoveTemp(AfterReport);
		}
	}
	Report.bValid = Report.Issues.Num() == 0 && (!HasFlag(Params, TEXT("-Write")) || (Report.bSaved && Report.bReloaded));
	WriteReport(ReportFilename, Report, BeforeReport.IsSet() ? &BeforeReport.GetValue() : nullptr);
	return Report.bValid ? 0 : 4;
}
