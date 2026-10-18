#pragma once

#include "CoreMinimal.h"

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
	TArray<FSkillLabAnimationIssue> Issues;
	bool bSaved = false;
	bool bReloaded = false;
	bool bValid = false;
};
