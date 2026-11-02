#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FSkillLabAnimationPatchEvidence
{
	TArray<int32> SourceFrames;
	TArray<float> SourceSeconds;
	FString Basis;
	FString Uncertainty;
};

struct FSkillLabAnimationPatchOperation
{
	FString Op;
	FString Id;
	FString Kind;
	FString EventTag;
	FString StateClass;
	float Time = 0.0f;
	float StartTime = 0.0f;
	float EndTime = 0.0f;
	TOptional<FSkillLabAnimationPatchEvidence> Evidence;
};

struct FSkillLabAnimationPatch
{
	int32 Version = 0;
	FString AssetPath;
	FString Observation;
	FString GenerationId;
	FString InputPreviewFingerprintSha256;
	FString DuplicatePolicy;
	TArray<FSkillLabAnimationPatchOperation> Operations;
};

struct FSkillLabAnimationPatchOperationReport
{
	FString Id;
	FString Op;
	FString Status;
	FString Before;
	FString After;
	FString IssueCode;
};

/** A machine-readable montage description. JSON is an input/patch format; the
 * saved UAnimMontage remains the runtime authority. */
struct FSkillLabAnimationContentSpec
{
	FString AssetPath;
	TOptional<float> RateScale;
	TOptional<float> BlendInTime;
	TOptional<float> BlendOutTime;
	TOptional<FString> SlotName;
	TOptional<FString> SectionName;
	TOptional<float> SectionStartTime;
	TOptional<FString> NextSectionName;
	TOptional<float> NotifyTime;
	TOptional<FString> NotifyEventTag;
	TOptional<FString> SegmentAssetPath;
};

struct FSkillLabAnimationIssue
{
	FString Code;
	FString Location;
	FString Message;
	bool bError = true;
};

struct FSkillLabAnimationContentReport
{
	FString AssetPath;
	FString AssetClass;
	FString SkeletonPath;
	float RateScale = 1.0f;
	float BlendInTime = 0.0f;
	float BlendOutTime = 0.0f;
	TArray<FString> Slots;
	TArray<FString> Sections;
	TArray<FString> SegmentAssets;
	TArray<FString> NotifyTags;
	TArray<FString> NotifyDetails;
	/** Notify signatures from the pre-patch snapshot that are outside this patch. */
	TArray<FString> RetainedNotifyDetails;
	TArray<FSkillLabAnimationPatchOperationReport> Operations;
	FString SourceObservation;
	FString SourceGenerationId;
	FString SourceInputPreviewFingerprintSha256;
	FString DuplicatePolicy;
	bool bAssetUnchanged = true;
	bool bDiffValid = false;
	TArray<FSkillLabAnimationIssue> Issues;
	bool bSaved = false;
	bool bReloaded = false;
	bool bValid = false;
};
