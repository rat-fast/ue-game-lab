#include "SkillLabSpatialTransform.h"
#include "Components/SkinnedMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"

bool FSkillLabSpatialTransform::Resolve(const FSkillLabSpatialDefinition& Definition, AActor* Source, AActor* Target,
	AActor* Related, const FVector& Ground, const FVector& ExecutionDirection, FTransform& Out, FString& Error)
{
	FSkillLabSpatialResolvedReference Resolved;
	if (!ResolveReference(Definition, Source, Target, Related, Ground, Resolved, Error)) return false;
	return Evaluate(Resolved.WorldTransform, ExecutionDirection, Definition.Offset, Definition.OffsetSpace,
		Definition.Direction, Definition.WorldDirection, Out, Error);
}

bool FSkillLabSpatialTransform::ResolveReference(const FSkillLabSpatialDefinition& Definition, AActor* Source, AActor* Target,
	AActor* Related, const FVector& Ground, FSkillLabSpatialResolvedReference& Out, FString& Error)
{
	Out = FSkillLabSpatialResolvedReference();
	Error.Reset();
	AActor* Actor = nullptr;
	switch (Definition.Source)
	{
	case ESkillLabSpatialSource::Source: Actor = Source; break;
	case ESkillLabSpatialSource::Target: Actor = Target; break;
	case ESkillLabSpatialSource::Related: Actor = Related; break;
	case ESkillLabSpatialSource::Ground: break;
	default: Error = TEXT("invalid reference source"); return false;
	}
	FTransform Reference;
	if (Definition.Source == ESkillLabSpatialSource::Ground)
	{
		if (!Definition.Socket.IsNone()) { Error = TEXT("ground cannot have a socket"); return false; }
		Reference = FTransform(FQuat::Identity, Ground);
	}
	else
	{
		if (!IsValid(Actor) || Actor->IsActorBeingDestroyed()) { Error = TEXT("reference actor unavailable"); return false; }
		if (Definition.Source == ESkillLabSpatialSource::Related && (!IsValid(Source) || Actor->GetWorld() != Source->GetWorld()))
		{ Error = TEXT("related actor is not in the source world"); return false; }
		Reference = Actor->GetActorTransform();
		if (!Definition.Socket.IsNone())
		{
			TArray<USkinnedMeshComponent*> Meshes;
			Actor->GetComponents(Meshes);
			USkinnedMeshComponent* Found = nullptr;
			for (USkinnedMeshComponent* Mesh : Meshes)
				if (IsValid(Mesh) && Mesh->DoesSocketExist(Definition.Socket))
				{
					if (Found) { Error = TEXT("socket is ambiguous across meshes"); return false; }
					Found = Mesh;
				}
			if (!Found) { Error = FString::Printf(TEXT("socket or bone '%s' not found"), *Definition.Socket.ToString()); return false; }
			Reference = Found->GetSocketTransform(Definition.Socket, RTS_World);
			Out.Component = Found;
			Out.Name = Definition.Socket;
			const int32 BoneIndex = Found->GetBoneIndex(Definition.Socket);
			Out.bIsBone = BoneIndex != INDEX_NONE;
			if (Out.bIsBone)
			{
				if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Found->GetSkinnedAsset()))
				{
					const FReferenceSkeleton& Skeleton = SkeletalMesh->GetRefSkeleton();
					const int32 ParentIndex = Skeleton.GetParentIndex(BoneIndex);
					Out.ParentReference = ParentIndex == INDEX_NONE ? NAME_None : Skeleton.GetBoneName(ParentIndex);
				}
			}
			else Out.ParentReference = Found->GetSocketBoneName(Definition.Socket);
		}
		Out.Actor = Actor;
	}
	Out.WorldTransform = Reference;
	return true;
}

bool FSkillLabSpatialTransform::Evaluate(const FTransform& Reference, const FVector& ExecutionDirection,
	const FVector& Offset, const ESkillLabSpatialOffsetSpace OffsetSpace,
	const ESkillLabSpatialDirection DirectionSource, const FRotator& WorldDirection,
	FTransform& Out, FString& Error)
{
	Error.Reset();
	if (!Reference.IsValid() || Offset.ContainsNaN() || ExecutionDirection.ContainsNaN() || WorldDirection.ContainsNaN())
	{
		Error = TEXT("non-finite spatial input");
		return false;
	}
	const bool bNeedsExecution = OffsetSpace == ESkillLabSpatialOffsetSpace::ExecutionDirection
		|| DirectionSource == ESkillLabSpatialDirection::ExecutionDirection;
	if (bNeedsExecution && ExecutionDirection.IsNearlyZero())
	{
		Error = TEXT("execution direction is zero");
		return false;
	}
	const FQuat ExecutionRotation = bNeedsExecution ? ExecutionDirection.Rotation().Quaternion() : FQuat::Identity;
	FVector WorldOffset;
	switch (OffsetSpace)
	{
	case ESkillLabSpatialOffsetSpace::Reference: WorldOffset = Reference.GetRotation().RotateVector(Offset); break;
	case ESkillLabSpatialOffsetSpace::World: WorldOffset = Offset; break;
	case ESkillLabSpatialOffsetSpace::ExecutionDirection: WorldOffset = ExecutionRotation.RotateVector(Offset); break;
	default: Error = TEXT("invalid offset space"); return false;
	}
	FQuat Rotation;
	switch (DirectionSource)
	{
	case ESkillLabSpatialDirection::Reference: Rotation = Reference.GetRotation(); break;
	case ESkillLabSpatialDirection::World: Rotation = WorldDirection.Quaternion(); break;
	case ESkillLabSpatialDirection::ExecutionDirection: Rotation = ExecutionRotation; break;
	default: Error = TEXT("invalid direction source"); return false;
	}
	Out = FTransform(Rotation, Reference.GetLocation() + WorldOffset, FVector::OneVector);
	return true;
}
