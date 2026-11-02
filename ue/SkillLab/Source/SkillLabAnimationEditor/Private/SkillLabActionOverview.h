#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

namespace SkillLabActionOverview
{
	using FObject = TSharedRef<FJsonObject>;
	bool SaveJson(const FString& Path, const FObject& Object);
	FObject Observe(const FString& Asset, const FString& MeshOverride, const FString& Directory,
		const FString& Root, const FString& BatchId, bool bMetadataOnly);
}
