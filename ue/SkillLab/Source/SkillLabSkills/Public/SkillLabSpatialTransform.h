#pragma once

#include "CoreMinimal.h"

enum class ESkillLabSpatialOffsetSpace : uint8
{
	Reference,
	World,
	ExecutionDirection
};

enum class ESkillLabSpatialDirection : uint8
{
	Reference,
	World,
	ExecutionDirection
};

enum class ESkillLabSpatialSource : uint8 { Source, Target, Ground, Related };
enum class ESkillLabSpatialObject : uint8 { Projectile, Marker };

struct SKILLLABSKILLS_API FSkillLabSpatialDefinition
{
	FName Name;
	ESkillLabSpatialObject Object = ESkillLabSpatialObject::Marker;
	FName Event;
	ESkillLabSpatialSource Source = ESkillLabSpatialSource::Source;
	ESkillLabSpatialOffsetSpace OffsetSpace = ESkillLabSpatialOffsetSpace::Reference;
	ESkillLabSpatialDirection Direction = ESkillLabSpatialDirection::Reference;
	bool bFollow = false;
	bool bFollowRotation = false;
	FName Socket;
	FString VisualMesh;
	FVector Offset = FVector::ZeroVector;
	FRotator WorldDirection = FRotator::ZeroRotator;
	FRotator VisualRotation = FRotator::ZeroRotator;
	float VisualScale = 1.0f;
};

/** Pure spatial calculation. Reference selection and object lifetime belong to the caller. */
struct SKILLLABSKILLS_API FSkillLabSpatialTransform
{
	static bool Evaluate(const FTransform& Reference, const FVector& ExecutionDirection,
		const FVector& Offset, ESkillLabSpatialOffsetSpace OffsetSpace,
		ESkillLabSpatialDirection DirectionSource, const FRotator& WorldDirection,
		FTransform& Out, FString& Error);
	static bool Resolve(const FSkillLabSpatialDefinition& Definition, AActor* Source, AActor* Target,
		AActor* Related, const FVector& Ground, const FVector& ExecutionDirection,
		FTransform& Out, FString& Error);
};
