#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "SkillLabSkillTypes.h"
namespace SkillLabSpatialExport
{
	inline FString JsonText(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	inline FString JsonLineText(const TSharedRef<FJsonObject>& Object)
	{
		FString Output;
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Object, Writer);
		return Output;
	}

	inline TSharedRef<FJsonObject> TransformJson(const FTransform& Transform)
	{
		TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
		const FVector Location = Transform.GetLocation();
		const FRotator Rotation = Transform.Rotator();
		const FVector Scale = Transform.GetScale3D();
		Value->SetStringField(TEXT("position_unit"), TEXT("cm"));
		Value->SetArrayField(TEXT("location_cm"), {MakeShared<FJsonValueNumber>(Location.X), MakeShared<FJsonValueNumber>(Location.Y), MakeShared<FJsonValueNumber>(Location.Z)});
		Value->SetStringField(TEXT("rotation_order"), TEXT("Pitch,Yaw,Roll"));
		Value->SetArrayField(TEXT("rotation_deg"), {MakeShared<FJsonValueNumber>(Rotation.Pitch), MakeShared<FJsonValueNumber>(Rotation.Yaw), MakeShared<FJsonValueNumber>(Rotation.Roll)});
		Value->SetArrayField(TEXT("scale"), {MakeShared<FJsonValueNumber>(Scale.X), MakeShared<FJsonValueNumber>(Scale.Y), MakeShared<FJsonValueNumber>(Scale.Z)});
		return Value;
	}

	inline FString UtcNow()
	{
		return FDateTime::UtcNow().ToIso8601();
	}

	inline FString SourceFingerprint(const FString& Source)
	{
		FTCHARToUTF8 Utf8(*Source);
		return TEXT("sha1:") + FSHA1::HashBuffer(Utf8.Get(), Utf8.Length()).ToString().ToLower();
	}

	inline FString SpatialObjectName(const ESkillLabSpatialObject Value)
	{
		return Value == ESkillLabSpatialObject::Projectile ? TEXT("Projectile") : TEXT("Marker");
	}

	inline FString SpatialSourceName(const ESkillLabSpatialSource Value)
	{
		switch (Value)
		{
		case ESkillLabSpatialSource::Source: return TEXT("Source");
		case ESkillLabSpatialSource::Target: return TEXT("Target");
		case ESkillLabSpatialSource::Ground: return TEXT("Ground");
		case ESkillLabSpatialSource::Related: return TEXT("Related");
		default: return TEXT("Unknown");
		}
	}

	inline FString SpatialDirectionName(const ESkillLabSpatialDirection Value)
	{
		switch (Value)
		{
		case ESkillLabSpatialDirection::Reference: return TEXT("Reference");
		case ESkillLabSpatialDirection::World: return TEXT("World");
		case ESkillLabSpatialDirection::ExecutionDirection: return TEXT("ExecutionDirection");
		default: return TEXT("Unknown");
		}
	}

	inline FString SpatialOffsetName(const ESkillLabSpatialOffsetSpace Value)
	{
		switch (Value)
		{
		case ESkillLabSpatialOffsetSpace::Reference: return TEXT("Reference");
		case ESkillLabSpatialOffsetSpace::World: return TEXT("World");
		case ESkillLabSpatialOffsetSpace::ExecutionDirection: return TEXT("ExecutionDirection");
		default: return TEXT("Unknown");
		}
	}
}
