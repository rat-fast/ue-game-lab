#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSpatialExportJson.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillObservation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "SkillLabSkillFeedbackActor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformTime.h"
#include "SkillLabFireballProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
using namespace SkillLabSpatialExport;
void USkillLabSpatialExportSubsystem::CaptureSample()
{
	TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
	const double SampleWall = FPlatformTime::Seconds();
	SampleIndex++;
	Sample->SetStringField(TEXT("sample_id"), FString::Printf(TEXT("%06d"), SampleIndex));
	Sample->SetNumberField(TEXT("sample_index"), SampleIndex);
	Sample->SetNumberField(TEXT("world_time_seconds"), GetWorld()->GetTimeSeconds());
	Sample->SetNumberField(TEXT("wall_seconds_since_start"), SampleWall - SessionStartWall);
	if (bRequestAccepted)
	{
		Sample->SetNumberField(TEXT("since_request_world_seconds"), GetWorld()->GetTimeSeconds() - RequestWorldTime);
		Sample->SetNumberField(TEXT("since_request_wall_seconds"), SampleWall - RequestWallTime);
		Sample->SetStringField(TEXT("request_time_relation"), TEXT("after_request"));
	}
	else
	{
	Sample->SetField(TEXT("since_request_world_seconds"), MakeShared<FJsonValueNull>());
	Sample->SetField(TEXT("since_request_wall_seconds"), MakeShared<FJsonValueNull>());
		Sample->SetStringField(TEXT("request_time_relation"), TEXT("before_request"));
	}
	Sample->SetNumberField(TEXT("target_interval_seconds"), SampleInterval);
	if (PreviousSampleWall > 0.0)
	{
		const double Delta = SampleWall - PreviousSampleWall;
		Sample->SetNumberField(TEXT("actual_sample_interval_seconds"), Delta);
		const int32 ExpectedSteps = FMath::Max(1, FMath::FloorToInt(Delta / SampleInterval));
		const int32 Missed = FMath::Max(0, ExpectedSteps - 1);
		MissedExpectedSamples += Missed;
		Sample->SetNumberField(TEXT("missed_expected_samples_before"), Missed);
	}
	PreviousSampleWall = SampleWall;
	if (FirstSampleWorldTime == 0.0)
	{
		FirstSampleWorldTime = GetWorld()->GetTimeSeconds();
		FirstSampleWallTime = SampleWall;
	}
	LastSampleWorldTime = GetWorld()->GetTimeSeconds();
	LastSampleWallTime = SampleWall;
	TArray<TSharedPtr<FJsonValue>> Objects;
	for (const TWeakObjectPtr<AActor>& Weak : {Caster, Target, RelatedActor})
	{
		if (AActor* Actor = Weak.Get())
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("path"), Actor->GetPathName());
			Object->SetObjectField(TEXT("transform"), TransformJson(Actor->GetActorTransform()));
			Objects.Add(MakeShared<FJsonValueObject>(Object));
		}
	}
	Sample->SetArrayField(TEXT("actors"), Objects);
	TArray<TSharedPtr<FJsonValue>> Projectiles;
	for (auto It = ObservedProjectiles.CreateIterator(); It; ++It)
	{
		auto* Projectile = It->Get();
		if (!IsValid(Projectile)) { It.RemoveCurrent(); continue; }
		auto Object = MakeShared<FJsonObject>(); Object->SetStringField(TEXT("path"), Projectile->GetPathName());
		if (const FIntPoint* Identity = ObservedProjectileIds.Find(Projectile))
		{
			Object->SetNumberField(TEXT("request_id"), Identity->X); Object->SetNumberField(TEXT("cast_id"), Identity->Y);
		}
		Object->SetObjectField(TEXT("transform"), TransformJson(Projectile->GetActorTransform()));
		Object->SetStringField(TEXT("caster_path"), Projectile->GetSourceActor() ? Projectile->GetSourceActor()->GetPathName() : FString());
		Object->SetStringField(TEXT("target_path"), Projectile->GetTargetActor() ? Projectile->GetTargetActor()->GetPathName() : FString());
		const FVector Velocity = Projectile->GetProjectileMovement()->Velocity;
		Object->SetArrayField(TEXT("velocity_cm_s"), {MakeShared<FJsonValueNumber>(Velocity.X), MakeShared<FJsonValueNumber>(Velocity.Y), MakeShared<FJsonValueNumber>(Velocity.Z)});
		const FRotator Direction = Velocity.GetSafeNormal().Rotation();
		Object->SetArrayField(TEXT("execution_direction_pitch_yaw_roll_deg"), {MakeShared<FJsonValueNumber>(Direction.Pitch), MakeShared<FJsonValueNumber>(Direction.Yaw), MakeShared<FJsonValueNumber>(Direction.Roll)});
		if (const auto* Collision = Projectile->GetCollisionShape()) Object->SetNumberField(TEXT("collision_sphere_radius_cm"), Collision->GetScaledSphereRadius());
		if (const auto* Visual = Projectile->GetProjectileVisual())
		{
			Object->SetObjectField(TEXT("visual_transform"), TransformJson(Visual->GetComponentTransform()));
			Object->SetStringField(TEXT("visual_mesh"), Visual->GetStaticMesh() ? Visual->GetStaticMesh()->GetPathName() : FString());
		}
		Projectiles.Add(MakeShared<FJsonValueObject>(Object));
	}
	Sample->SetArrayField(TEXT("projectiles"), Projectiles);
	TArray<TSharedPtr<FJsonValue>> Feedback;
	for (const TWeakObjectPtr<ASkillLabSkillFeedbackActor>& WeakFeedback : ObservedFeedbackActors)
	{
		ASkillLabSkillFeedbackActor* Actor = WeakFeedback.Get();
		if (!IsValid(Actor)) continue;
		auto Object = MakeShared<FJsonObject>(); Object->SetStringField(TEXT("path"), Actor->GetPathName());
		Object->SetObjectField(TEXT("transform"), TransformJson(Actor->GetActorTransform()));
		Object->SetStringField(TEXT("skill_id"), Actor->GetObservedSkillId().ToString());
		Object->SetStringField(TEXT("event"), Actor->GetObservedEventName().ToString());
		Object->SetStringField(TEXT("effect_instance_id"), Actor->GetObservedEffectInstanceId().ToString());
		Object->SetStringField(TEXT("binding_name"), Actor->GetObservedBindingName().ToString());
		Object->SetNumberField(TEXT("request_id"), Actor->GetObservedRequestId());
		Object->SetNumberField(TEXT("cast_id"), Actor->GetObservedCastId());
		Object->SetNumberField(TEXT("runtime_generation"), static_cast<double>(Actor->GetObservedRuntimeGeneration()));
		Object->SetStringField(TEXT("submission_key"), Actor->GetObservedSubmissionKey().Id.ToString());
		Object->SetNumberField(TEXT("submission_point_id"), static_cast<double>(Actor->GetObservedSubmissionPointId()));
		Object->SetStringField(TEXT("shape"), Actor->GetObservedShapeName().ToString());
		Object->SetNumberField(TEXT("configured_radius_cm"), Actor->GetObservedRadiusCm());
		Object->SetStringField(TEXT("source_actor"), Actor->GetObservedSourceActor() ? Actor->GetObservedSourceActor()->GetPathName() : FString());
		Object->SetStringField(TEXT("target_actor"), Actor->GetObservedTargetActor() ? Actor->GetObservedTargetActor()->GetPathName() : FString());
		TArray<TSharedPtr<FJsonValue>> Meshes;
		TArray<UStaticMeshComponent*> Components; Actor->GetComponents(Components);
		for (const auto* Component : Components) if (Component)
		{
			auto Mesh = MakeShared<FJsonObject>(); Mesh->SetStringField(TEXT("component_path"), Component->GetPathName());
			Mesh->SetStringField(TEXT("mesh"), Component->GetStaticMesh() ? Component->GetStaticMesh()->GetPathName() : FString());
			Mesh->SetObjectField(TEXT("transform"), TransformJson(Component->GetComponentTransform()));
			Mesh->SetStringField(TEXT("parent_component_path"), Component->GetAttachParent() ? Component->GetAttachParent()->GetPathName() : FString());
			Mesh->SetBoolField(TEXT("visible"), Component->IsVisible());
			if (const UStaticMesh* StaticMesh = Component->GetStaticMesh())
			{
				const FBox Bounds = StaticMesh->GetBoundingBox();
				Mesh->SetArrayField(TEXT("local_bounds_center_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetCenter().X), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Y), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Z)});
				Mesh->SetArrayField(TEXT("local_bounds_extent_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetExtent().X), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Y), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Z)});
				Mesh->SetStringField(TEXT("lighting_guid"), StaticMesh->GetLightingGuid().ToString());
				Mesh->SetStringField(TEXT("package_guid"), StaticMesh->GetPackage()->GetPersistentGuid().ToString());
			}
			else Mesh->SetStringField(TEXT("mesh_status"), TEXT("missing"));
			Meshes.Add(MakeShared<FJsonValueObject>(Mesh));
		}
		Object->SetArrayField(TEXT("static_meshes"), Meshes); Feedback.Add(MakeShared<FJsonValueObject>(Object));
	}
	Sample->SetArrayField(TEXT("feedback_actors"), Feedback);
	const TSharedPtr<FJsonObject>* SampleTarget = nullptr;
	if (Request->TryGetObjectField(TEXT("target"), SampleTarget) && SampleTarget && SampleTarget->IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Ground = nullptr;
		if ((*SampleTarget)->TryGetArrayField(TEXT("ground_location"), Ground) && Ground && Ground->Num() == 3)
		{
			TArray<TSharedPtr<FJsonValue>> Location{(*Ground)[0], (*Ground)[1], (*Ground)[2]};
			Sample->SetArrayField(TEXT("ground_target_location_cm"), Location);
		}
	}
	TArray<TSharedPtr<FJsonValue>> Queries;
	AppendReferenceQueries(Queries);
	Sample->SetArrayField(TEXT("reference_queries"), Queries);
	TArray<TSharedPtr<FJsonValue>> Models;
	AppendModelSnapshots(Models);
	Sample->SetArrayField(TEXT("models"), Models);
	Sample->SetNumberField(TEXT("game_frame"), static_cast<double>(GFrameCounter));
	CaptureViews(Sample, Queries);
	AppendJsonLine(TEXT("samples.jsonl"), Sample);
}

