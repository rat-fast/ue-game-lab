#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSpatialExportJson.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillObservation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformTime.h"
using namespace SkillLabSpatialExport;
void USkillLabSpatialExportSubsystem::WriteConfigurationAndReferences()
{
	TSharedRef<FJsonObject> Configuration = MakeShared<FJsonObject>();
	Configuration->SetStringField(TEXT("project_dir"), FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Configuration->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	Configuration->SetStringField(TEXT("catalog_source_fingerprint"), FSkillLabSkillDefinitionProvider::GetAcceptedCatalogSourceFingerprint());
	Configuration->SetStringField(TEXT("catalog_source_path"), FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("SkillDefinitions.json")));
	TSharedRef<FJsonObject> RuntimeIdentity = MakeShared<FJsonObject>();
	RuntimeIdentity->SetStringField(TEXT("actor_path"), Caster.IsValid() ? Caster->GetPathName() : FString());
	RuntimeIdentity->SetStringField(TEXT("runtime_component_path"), Runtime.IsValid() ? Runtime->GetPathName() : FString());
	RuntimeIdentity->SetStringField(TEXT("world_path"), GetWorld() ? GetWorld()->GetPathName() : FString());
	RuntimeIdentity->SetNumberField(TEXT("generation"), Runtime.IsValid() ? static_cast<double>(Runtime->GetRuntimeGeneration()) : 0.0);
	Configuration->SetObjectField(TEXT("caster_runtime"), RuntimeIdentity);
	TSharedRef<FJsonObject> ScenePreservation = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> ProtectedSceneActors;
	int32 TrainingDummyTagCount = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor) continue;
		if (Actor->ActorHasTag(TEXT("SkillLabDummy"))) ++TrainingDummyTagCount;
		const FString ActorName = Actor->GetActorNameOrLabel();
		if (!ActorName.Contains(TEXT("NavTest_Block_A")) && !ActorName.Contains(TEXT("NavTest_Block_B"))
			&& !Actor->ActorHasTag(TEXT("SkillLabDummy"))) continue;
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("actor_path"), Actor->GetPathName());
		Entry->SetStringField(TEXT("class_path"), Actor->GetClass()->GetPathName());
		Entry->SetBoolField(TEXT("hidden_in_game"), Actor->IsHidden());
		Entry->SetBoolField(TEXT("collision_enabled"), Actor->GetActorEnableCollision());
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		TArray<TSharedPtr<FJsonValue>> ComponentStates;
		for (const UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (!Component) continue;
			TSharedRef<FJsonObject> ComponentState = MakeShared<FJsonObject>();
			ComponentState->SetStringField(TEXT("component_path"), Component->GetPathName());
			ComponentState->SetStringField(TEXT("collision_enabled"), UEnum::GetValueAsString(Component->GetCollisionEnabled()));
			ComponentState->SetBoolField(TEXT("visible"), Component->IsVisible());
			ComponentStates.Add(MakeShared<FJsonValueObject>(ComponentState));
		}
		Entry->SetArrayField(TEXT("primitive_components"), ComponentStates);
		ProtectedSceneActors.Add(MakeShared<FJsonValueObject>(Entry));
	}
	ScenePreservation->SetArrayField(TEXT("protected_actors"), ProtectedSceneActors);
	ScenePreservation->SetNumberField(TEXT("skill_lab_dummy_tag_count"), TrainingDummyTagCount);
	ScenePreservation->SetStringField(TEXT("meaning"), TEXT("Captured after original map and pawn initialization; spatial export does not call TestArena preparation"));
	Configuration->SetObjectField(TEXT("scene_preservation"), ScenePreservation);
	TArray<FName> Granted;
	Runtime->GetGrantedSkillIds(Granted);
	TArray<TSharedPtr<FJsonValue>> Skills;
	FString RequestedSkill;
	Request->TryGetStringField(TEXT("skill_id"), RequestedSkill);
	for (const FName Id : Granted)
	{
		FSkillLabSkillDefinition Definition;
		if (!FSkillLabSkillDefinitionProvider::Resolve(Id, Definition)) continue;
		TSharedRef<FJsonObject> Skill = MakeShared<FJsonObject>();
		Skill->SetStringField(TEXT("skill_id"), Id.ToString());
		Skill->SetNumberField(TEXT("cost"), Definition.Cost);
		Skill->SetNumberField(TEXT("cooldown"), Definition.Cooldown);
		Skill->SetNumberField(TEXT("range_cm"), Definition.Range);
		Skill->SetNumberField(TEXT("radius_cm"), Definition.Radius);
		Skill->SetStringField(TEXT("target_shape"), UEnum::GetValueAsString(Definition.TargetShape));
		Skill->SetStringField(TEXT("execution"), UEnum::GetValueAsString(Definition.Execution));
		Skill->SetStringField(TEXT("cast_montage"), Definition.CastMontage.ToString());
		Skill->SetStringField(TEXT("cast_animation"), Definition.CastAnimation.ToString());
		Skill->SetStringField(TEXT("lua_script"), Definition.LuaScript);
		Skill->SetStringField(TEXT("lua_source_fingerprint"), SourceFingerprint(Definition.LuaSource));
		Skill->SetNumberField(TEXT("projectile_speed_cm_s"), Definition.ProjectileParameters.Speed);
		Skill->SetNumberField(TEXT("projectile_collision_radius_cm"), Definition.ProjectileParameters.CollisionRadius);
		Skill->SetNumberField(TEXT("projectile_lifetime_seconds"), Definition.ProjectileParameters.Lifetime);
		Skill->SetNumberField(TEXT("projectile_visual_scale"), Definition.ProjectileParameters.VisualScale);
		Skill->SetBoolField(TEXT("projectile_intercepts_other_units"), Definition.ProjectileParameters.bInterceptOtherUnits);
		TArray<TSharedPtr<FJsonValue>> Spatial;
		for (const FSkillLabSpatialDefinition& SpatialDef : Definition.SpatialDefinitions)
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("name"), SpatialDef.Name.ToString());
			Entry->SetStringField(TEXT("object"), SpatialObjectName(SpatialDef.Object));
			Entry->SetStringField(TEXT("event"), SpatialDef.Event.ToString());
			Entry->SetStringField(TEXT("source"), SpatialSourceName(SpatialDef.Source));
			Entry->SetStringField(TEXT("direction"), SpatialDirectionName(SpatialDef.Direction));
			Entry->SetStringField(TEXT("offset_space"), SpatialOffsetName(SpatialDef.OffsetSpace));
			Entry->SetArrayField(TEXT("offset_cm"), {MakeShared<FJsonValueNumber>(SpatialDef.Offset.X), MakeShared<FJsonValueNumber>(SpatialDef.Offset.Y), MakeShared<FJsonValueNumber>(SpatialDef.Offset.Z)});
			Entry->SetArrayField(TEXT("world_direction_deg"), {MakeShared<FJsonValueNumber>(SpatialDef.WorldDirection.Pitch), MakeShared<FJsonValueNumber>(SpatialDef.WorldDirection.Yaw), MakeShared<FJsonValueNumber>(SpatialDef.WorldDirection.Roll)});
			Entry->SetArrayField(TEXT("visual_rotation_deg"), {MakeShared<FJsonValueNumber>(SpatialDef.VisualRotation.Pitch), MakeShared<FJsonValueNumber>(SpatialDef.VisualRotation.Yaw), MakeShared<FJsonValueNumber>(SpatialDef.VisualRotation.Roll)});
			Entry->SetNumberField(TEXT("visual_scale"), SpatialDef.VisualScale);
			Entry->SetBoolField(TEXT("follow"), SpatialDef.bFollow);
			Entry->SetBoolField(TEXT("follow_rotation"), SpatialDef.bFollowRotation);
			Entry->SetStringField(TEXT("socket"), SpatialDef.Socket.ToString());
			Entry->SetStringField(TEXT("mesh"), SpatialDef.VisualMesh);
			if (!SpatialDef.VisualMesh.IsEmpty())
			{
				if (const UStaticMesh* VisualMesh = LoadObject<UStaticMesh>(nullptr, *SpatialDef.VisualMesh))
				{
					Entry->SetStringField(TEXT("mesh_lighting_guid"), VisualMesh->GetLightingGuid().ToString());
					Entry->SetStringField(TEXT("mesh_package_guid"), VisualMesh->GetPackage()->GetPersistentGuid().ToString());
					Entry->SetStringField(TEXT("mesh_version_status"), TEXT("runtime_package_and_lighting_guid_available"));
				}
				else Entry->SetStringField(TEXT("mesh_version_status"), TEXT("configured_mesh_unavailable_at_capture"));
			}
			Spatial.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Skill->SetArrayField(TEXT("spatial"), Spatial);
		Skills.Add(MakeShared<FJsonValueObject>(Skill));
	}
	Configuration->SetArrayField(TEXT("granted_skills"), Skills);
	Configuration->SetStringField(TEXT("selected_skill_id"), RequestedSkill);
	if (!FFileHelper::SaveStringToFile(JsonText(Configuration), *FPaths::Combine(OutputDirectory, TEXT("configuration.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{ bConfigurationWriteFailed = true; Errors.AddUnique(TEXT("failed to write configuration.json")); }

	TSharedRef<FJsonObject> References = MakeShared<FJsonObject>();
	References->SetStringField(TEXT("caster_path"), Caster->GetPathName());
	References->SetStringField(TEXT("unit"), TEXT("cm"));
	const TSharedPtr<FJsonObject>* ReferenceTarget = nullptr;
	if (Request->TryGetObjectField(TEXT("target"), ReferenceTarget) && ReferenceTarget && ReferenceTarget->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Ground = nullptr;
		if ((*ReferenceTarget)->TryGetArrayField(TEXT("ground_location"), Ground) && Ground && Ground->Num() == 3)
		{
			TArray<TSharedPtr<FJsonValue>> Location{(*Ground)[0], (*Ground)[1], (*Ground)[2]};
			References->SetArrayField(TEXT("ground_target_location_cm"), Location);
		}
	}
	TArray<TSharedPtr<FJsonValue>> Actors;
	TArray<TWeakObjectPtr<AActor>> SubjectActors{Caster, Target, RelatedActor};
	for (int32 SubjectIndex = 0; SubjectIndex < SubjectActors.Num(); ++SubjectIndex)
	{
		AActor* Actor = SubjectActors[SubjectIndex].Get();
		if (!Actor) continue;
		TSharedRef<FJsonObject> ActorJson = MakeShared<FJsonObject>();
		ActorJson->SetStringField(TEXT("role"), SubjectIndex == 0 ? TEXT("caster") : SubjectIndex == 1 ? TEXT("target") : TEXT("related_actor"));
		ActorJson->SetStringField(TEXT("path"), Actor->GetPathName());
		ActorJson->SetObjectField(TEXT("transform"), TransformJson(Actor->GetActorTransform()));
		TArray<TSharedPtr<FJsonValue>> Meshes;
		TArray<USkeletalMeshComponent*> Components;
		Actor->GetComponents<USkeletalMeshComponent>(Components);
		for (USkeletalMeshComponent* Mesh : Components)
		{
			if (!Mesh || !Mesh->GetSkeletalMeshAsset()) continue;
			TSharedRef<FJsonObject> MeshJson = MakeShared<FJsonObject>();
			MeshJson->SetStringField(TEXT("component_path"), Mesh->GetPathName());
			MeshJson->SetStringField(TEXT("skeletal_mesh"), Mesh->GetSkeletalMeshAsset()->GetPathName());
			MeshJson->SetStringField(TEXT("skeleton"), Mesh->GetSkeletalMeshAsset()->GetSkeleton() ? Mesh->GetSkeletalMeshAsset()->GetSkeleton()->GetPathName() : FString());
			TArray<TSharedPtr<FJsonValue>> Bones;
			for (int32 BoneIndex = 0; BoneIndex < Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetNum(); ++BoneIndex)
				Bones.Add(MakeShared<FJsonValueString>(Mesh->GetSkeletalMeshAsset()->GetRefSkeleton().GetBoneName(BoneIndex).ToString()));
			MeshJson->SetArrayField(TEXT("bones"), Bones);
			TArray<TSharedPtr<FJsonValue>> Sockets;
			for (int32 SocketIndex = 0; SocketIndex < Mesh->GetSkeletalMeshAsset()->NumSockets(); ++SocketIndex)
				if (const USkeletalMeshSocket* Socket = Mesh->GetSkeletalMeshAsset()->GetSocketByIndex(SocketIndex))
					Sockets.Add(MakeShared<FJsonValueString>(Socket->SocketName.ToString()));
			MeshJson->SetArrayField(TEXT("sockets"), Sockets);
			MeshJson->SetObjectField(TEXT("transform"), TransformJson(Mesh->GetComponentTransform()));
			Meshes.Add(MakeShared<FJsonValueObject>(MeshJson));
		}
		ActorJson->SetArrayField(TEXT("skeletal_meshes"), Meshes);
		TArray<TSharedPtr<FJsonValue>> StaticMeshes;
		TArray<UStaticMeshComponent*> StaticComponents;
		Actor->GetComponents<UStaticMeshComponent>(StaticComponents);
		for (UStaticMeshComponent* Component : StaticComponents)
		{
			if (!Component || !Component->GetStaticMesh()) continue;
			TSharedRef<FJsonObject> MeshJson = MakeShared<FJsonObject>();
			const FBox Bounds = Component->GetStaticMesh()->GetBoundingBox();
			MeshJson->SetStringField(TEXT("component_path"), Component->GetPathName());
			MeshJson->SetStringField(TEXT("static_mesh"), Component->GetStaticMesh()->GetPathName());
			MeshJson->SetStringField(TEXT("lighting_guid"), Component->GetStaticMesh()->GetLightingGuid().ToString());
			MeshJson->SetStringField(TEXT("package_guid"), Component->GetStaticMesh()->GetPackage()->GetPersistentGuid().ToString());
			MeshJson->SetObjectField(TEXT("transform"), TransformJson(Component->GetComponentTransform()));
			MeshJson->SetArrayField(TEXT("local_bounds_origin_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetCenter().X), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Y), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Z)});
			MeshJson->SetArrayField(TEXT("local_bounds_extent_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetExtent().X), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Y), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Z)});
			StaticMeshes.Add(MakeShared<FJsonValueObject>(MeshJson));
		}
		ActorJson->SetArrayField(TEXT("static_meshes"), StaticMeshes);
		Actors.Add(MakeShared<FJsonValueObject>(ActorJson));
	}
	References->SetArrayField(TEXT("actors"), Actors);
	TArray<TSharedPtr<FJsonValue>> Queries;
	AppendReferenceQueries(Queries);
	References->SetArrayField(TEXT("requested_reference_queries"), Queries);
	if (!FFileHelper::SaveStringToFile(JsonText(References), *FPaths::Combine(OutputDirectory, TEXT("references.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{ bConfigurationWriteFailed = true; Errors.AddUnique(TEXT("failed to write references.json")); }
	if (Mode == TEXT("capture") && !FFileHelper::SaveStringToFile(TEXT(""), *FPaths::Combine(OutputDirectory, TEXT("events.jsonl"))))
	{ bConfigurationWriteFailed = true; Errors.AddUnique(TEXT("failed to initialize events.jsonl")); }
}

