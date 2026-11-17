#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSpatialExportJson.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillObservation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformTime.h"
using namespace SkillLabSpatialExport;
void USkillLabSpatialExportSubsystem::AppendReferenceQueries(TArray<TSharedPtr<FJsonValue>>& OutValues)
{
	const TArray<TSharedPtr<FJsonValue>>* Queries = nullptr;
	Request->TryGetArrayField(TEXT("reference_queries"), Queries);
	if (Queries) for (const TSharedPtr<FJsonValue>& QueryValue : *Queries)
	{
		if (!QueryValue.IsValid() || QueryValue->Type != EJson::Object) continue;
		const TSharedPtr<FJsonObject> Query = QueryValue->AsObject();
		FString SourceName, ComponentPath, Kind, Name;
		Query->TryGetStringField(TEXT("source"), SourceName);
		Query->TryGetStringField(TEXT("component_path"), ComponentPath);
		Query->TryGetStringField(TEXT("kind"), Kind);
		Query->TryGetStringField(TEXT("name"), Name);
		AActor* Subject = SourceName == TEXT("caster") ? Caster.Get() : SourceName == TEXT("target") ? Target.Get() : SourceName == TEXT("related_actor") ? RelatedActor.Get() : nullptr;
		TArray<USkeletalMeshComponent*> Matches;
		if (Subject)
		{
			TArray<USkeletalMeshComponent*> Components;
			Subject->GetComponents<USkeletalMeshComponent>(Components);
			for (USkeletalMeshComponent* Component : Components)
				if (Component && Component->GetPathName() == ComponentPath) Matches.Add(Component);
		}
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("source"), SourceName);
		Result->SetStringField(TEXT("component_path"), ComponentPath);
		Result->SetStringField(TEXT("kind"), Kind);
		Result->SetStringField(TEXT("name"), Name);
		if (Matches.Num() != 1)
		{
			Result->SetStringField(TEXT("status"), Subject ? TEXT("invalid_component") : TEXT("invalid_source"));
			Errors.AddUnique(FString::Printf(TEXT("reference query component path matched %d skeletal mesh components: %s"), Matches.Num(), *ComponentPath));
			OutValues.Add(MakeShared<FJsonValueObject>(Result));
			continue;
		}
		USkeletalMeshComponent* Mesh = Matches[0];
		const FName ReferenceName(*Name);
		FTransform WorldTransform = FTransform::Identity;
		FTransform LocalTransform = FTransform::Identity;
		FString Parent;
		bool bValid = false;
		if (Kind == TEXT("bone"))
		{
			const int32 BoneIndex = Mesh->GetBoneIndex(ReferenceName);
			bValid = BoneIndex != INDEX_NONE;
			if (bValid)
			{
				WorldTransform = Mesh->GetBoneTransform(BoneIndex);
				const int32 ParentIndex = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetParentIndex(BoneIndex);
				Parent = ParentIndex == INDEX_NONE ? Mesh->GetPathName() : Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(ParentIndex).ToString();
				const FTransform ParentWorld = ParentIndex == INDEX_NONE ? Mesh->GetComponentTransform() : Mesh->GetBoneTransform(ParentIndex);
				LocalTransform = WorldTransform.GetRelativeTransform(ParentWorld);
			}
		}
		else if (Kind == TEXT("socket"))
		{
			bValid = Mesh->GetSocketByName(ReferenceName) != nullptr;
			if (bValid)
			{
				WorldTransform = Mesh->GetSocketTransform(ReferenceName, RTS_World);
				LocalTransform = Mesh->GetSocketTransform(ReferenceName, RTS_ParentBoneSpace);
				Parent = Mesh->GetSocketBoneName(ReferenceName).ToString();
			}
		}
		if (!bValid) Errors.AddUnique(FString::Printf(TEXT("reference query %s not found as %s on %s"), *Name, *Kind, *ComponentPath));
		Result->SetStringField(TEXT("status"), bValid ? TEXT("resolved") : TEXT("not_found"));
		if (bValid)
		{
			Result->SetStringField(TEXT("parent_reference"), Parent);
			Result->SetObjectField(TEXT("local_transform"), TransformJson(LocalTransform));
			Result->SetObjectField(TEXT("world_transform"), TransformJson(WorldTransform));
		}
		OutValues.Add(MakeShared<FJsonValueObject>(Result));
	}
	if (Mode != TEXT("capture")) return;
	FString SkillIdText;
	if (!Request->TryGetStringField(TEXT("skill_id"), SkillIdText)) return;
	FSkillLabSkillDefinition Definition;
	if (!FSkillLabSkillDefinitionProvider::Resolve(FName(*SkillIdText), Definition)) return;
	FVector Ground = FVector::ZeroVector;
	const TSharedPtr<FJsonObject>* TargetJson = nullptr;
	if (Request->TryGetObjectField(TEXT("target"), TargetJson) && TargetJson && TargetJson->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if ((*TargetJson)->TryGetArrayField(TEXT("ground_location"), Values) && Values && Values->Num() == 3)
			Ground = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
	}
	for (const FSkillLabSpatialDefinition& Spatial : Definition.SpatialDefinitions)
	{

		AActor* SourceActor = Spatial.Source == ESkillLabSpatialSource::Source ? Caster.Get()
			: Spatial.Source == ESkillLabSpatialSource::Target ? Target.Get()
			: Spatial.Source == ESkillLabSpatialSource::Related ? RelatedActor.Get() : nullptr;
		FSkillLabSpatialResolvedReference Resolved;
		FString Error;
		const bool bResolved = FSkillLabSpatialTransform::ResolveReference(Spatial, Caster.Get(), Target.Get(), RelatedActor.Get(), Ground, Resolved, Error);
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("auto_binding"), true);
		Result->SetStringField(TEXT("binding_name"), Spatial.Name.ToString());
		Result->SetStringField(TEXT("source"), SpatialSourceName(Spatial.Source));
		Result->SetStringField(TEXT("configured_object"), SpatialObjectName(Spatial.Object));
		Result->SetStringField(TEXT("configured_event"), Spatial.Event.ToString());
		Result->SetStringField(TEXT("kind"), Spatial.Socket.IsNone() ? TEXT("origin") : Resolved.bIsBone ? TEXT("bone") : TEXT("socket"));
		Result->SetStringField(TEXT("name"), Spatial.Socket.ToString());
		USkinnedMeshComponent* Mesh = Resolved.Component.Get();
		Result->SetStringField(TEXT("actor_path"), SourceActor ? SourceActor->GetPathName() : FString());
		Result->SetStringField(TEXT("component_path"), Mesh ? Mesh->GetPathName() : FString());
		Result->SetStringField(TEXT("status"), bResolved ? TEXT("resolved") : TEXT("unavailable"));
		if (bResolved)
		{
			FTransform LocalTransform = Resolved.WorldTransform;
			if (Mesh && Resolved.bIsBone)
			{
				const int32 BoneIndex = Mesh->GetBoneIndex(Spatial.Socket);
				if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh->GetSkinnedAsset()))
				{
					const int32 ParentIndex = SkeletalMesh->GetRefSkeleton().GetParentIndex(BoneIndex);
					const FTransform ParentWorld = ParentIndex == INDEX_NONE ? Mesh->GetComponentTransform() : Mesh->GetBoneTransform(ParentIndex);
					LocalTransform = Resolved.WorldTransform.GetRelativeTransform(ParentWorld);
				}
			}
			else if (Mesh && !Spatial.Socket.IsNone()) LocalTransform = Mesh->GetSocketTransform(Spatial.Socket, RTS_ParentBoneSpace);
			else if (SourceActor && SourceActor->GetRootComponent())
			{
				LocalTransform = Resolved.WorldTransform.GetRelativeTransform(SourceActor->GetRootComponent()->GetComponentTransform());
				Result->SetStringField(TEXT("parent_reference"), SourceActor->GetRootComponent()->GetPathName());
			}
			if (!Result->HasField(TEXT("parent_reference")))
				Result->SetStringField(TEXT("parent_reference"), Resolved.ParentReference.IsNone() ? (Mesh ? Mesh->GetPathName() : FString()) : Resolved.ParentReference.ToString());
			Result->SetObjectField(TEXT("local_transform"), TransformJson(LocalTransform));
			Result->SetObjectField(TEXT("world_transform"), TransformJson(Resolved.WorldTransform));
		}
		else
		{
			Result->SetStringField(TEXT("error"), Error);
			Errors.AddUnique(FString::Printf(TEXT("spatial binding %s reference failed: %s"), *Spatial.Name.ToString(), *Error));
		}
		OutValues.Add(MakeShared<FJsonValueObject>(Result));
	}
}

