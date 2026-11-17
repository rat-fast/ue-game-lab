#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSpatialExportJson.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillObservation.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Animation/AnimSequenceBase.h"
#include "SkillLabCastExecution.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformTime.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabSkillFeedbackActor.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
using namespace SkillLabSpatialExport;
void USkillLabSpatialExportSubsystem::RecordSkillEvent(const FSkillLabSkillResult& Event)
{
	if (!Request.IsValid() || Event.SkillId != FName(*Request->GetStringField(TEXT("skill_id")))
		|| Event.SourceActor != Caster.Get() || Event.CastToken.Runtime != Runtime
		|| Event.CastToken.RuntimeGeneration != Runtime->GetRuntimeGeneration()) return;
	if (bSubmittingRequest && RequestId == 0 && Event.Event == ESkillLabSkillEvent::Started && Event.RequestId > 0)
	{
		RequestId = Event.RequestId;
		CapturedCastId = Event.CastId;
		CapturedRuntimeGeneration = Event.CastToken.RuntimeGeneration;
	}
	if (RequestId <= 0 || Event.RequestId != RequestId || Event.CastId != CapturedCastId
		|| Event.CastToken.CastId != CapturedCastId || Event.CastToken.RuntimeGeneration != CapturedRuntimeGeneration) return;
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event"), UEnum::GetValueAsString(Event.Event));
	Json->SetStringField(TEXT("stage"), UEnum::GetValueAsString(Event.Stage));
	Json->SetStringField(TEXT("skill_id"), Event.SkillId.ToString());
	Json->SetNumberField(TEXT("request_id"), Event.RequestId);
	Json->SetNumberField(TEXT("cast_id"), Event.CastId);
	Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(Event.CastToken.RuntimeGeneration));
	Json->SetBoolField(TEXT("released"), Event.bHasReleased);
	Json->SetStringField(TEXT("reason"), Event.Reason.ToString());
	Json->SetNumberField(TEXT("world_time_seconds"), GetWorld()->GetTimeSeconds());
	Json->SetNumberField(TEXT("since_request_world_seconds"), RequestId > 0 ? GetWorld()->GetTimeSeconds() - RequestWorldTime : 0.0);
	Json->SetNumberField(TEXT("since_request_wall_seconds"), RequestId > 0 ? FPlatformTime::Seconds() - RequestWallTime : 0.0);
	if (Event.SubmissionKey.IsSet()) Json->SetStringField(TEXT("submission_key"), Event.SubmissionKey.Id.ToString());
	if (Event.SubmissionKey.IsSet())
	{
		Json->SetNumberField(TEXT("submission_generation"), static_cast<double>(Event.SubmissionKey.Generation));
		Json->SetNumberField(TEXT("submission_cast"), static_cast<double>(Event.SubmissionKey.Cast));
		Json->SetNumberField(TEXT("submission_point"), static_cast<double>(Event.SubmissionKey.Point));
		Json->SetNumberField(TEXT("submission_transaction"), static_cast<double>(Event.SubmissionKey.Transaction));
		Json->SetNumberField(TEXT("submission_operation"), static_cast<double>(Event.SubmissionKey.Operation));
		Json->SetStringField(TEXT("submission_skill_id"), Event.SubmissionKey.SkillId.ToString());
	}
	Json->SetStringField(TEXT("effect_instance_id"), Event.EffectInstanceId.ToString());
	Json->SetBoolField(TEXT("effect_applied"), Event.bEffectApplied);
	Json->SetNumberField(TEXT("actual_amount"), Event.ActualAmount);
	Json->SetNumberField(TEXT("visual_radius_cm"), Event.VisualRadius);
	Json->SetStringField(TEXT("target_path"), Event.TargetActor ? Event.TargetActor->GetPathName() : FString());
	Json->SetObjectField(TEXT("target_location"), TransformJson(FTransform(Event.TargetLocation)));
	if (Event.Event == ESkillLabSkillEvent::Started)
	{
		if (const auto* Execution = Runtime->FindExecutionByCastId(Event.CastId))
		{
			const auto& Definition = Execution->ActiveDefinition;
			auto Snapshot = MakeShared<FJsonObject>();
			Snapshot->SetStringField(TEXT("skill_id"), Definition.SkillId.ToString());
			Snapshot->SetNumberField(TEXT("cost"), Definition.Cost); Snapshot->SetNumberField(TEXT("cooldown"), Definition.Cooldown);
			Snapshot->SetNumberField(TEXT("range_cm"), Definition.Range); Snapshot->SetNumberField(TEXT("radius_cm"), Definition.Radius);
			Snapshot->SetStringField(TEXT("execution"), UEnum::GetValueAsString(Definition.Execution));
			Snapshot->SetStringField(TEXT("target_shape"), UEnum::GetValueAsString(Definition.TargetShape));
			Snapshot->SetStringField(TEXT("lua_script"), Definition.LuaScript);
			Snapshot->SetStringField(TEXT("lua_source_fingerprint"), SourceFingerprint(Definition.LuaSource));
			Snapshot->SetStringField(TEXT("accepted_catalog_fingerprint"), FSkillLabSkillDefinitionProvider::GetAcceptedCatalogSourceFingerprint());
			Snapshot->SetStringField(TEXT("cast_montage"), Definition.CastMontage.ToString());
			Snapshot->SetStringField(TEXT("cast_animation"), Definition.CastAnimation.ToString());
			Json->SetObjectField(TEXT("active_definition_snapshot"), Snapshot);
		}
	}
	AppendJsonLine(TEXT("events.jsonl"), Json);
}

void USkillLabSpatialExportSubsystem::RecordProjectile(const FSkillLabProjectileObservation& Observation)
{
	if (!Request.IsValid() || Observation.SkillId != FName(*Request->GetStringField(TEXT("skill_id")))
		|| Observation.World.Get() != GetWorld() || Observation.Runtime != Runtime
		|| Observation.Caster.Get() != Caster.Get() || Observation.CastToken.Runtime != Runtime
		|| Observation.CastToken.RuntimeGeneration != CapturedRuntimeGeneration
		|| Observation.RequestId != RequestId || Observation.CastId != CapturedCastId
		|| Observation.CastToken.CastId != CapturedCastId || RequestId <= 0) return;
	if (Observation.Projectile.IsValid())
	{
		if (!Observation.bRemoved && !ObservedProjectiles.Contains(Observation.Projectile)) ObservedProjectiles.Add(Observation.Projectile);
		ObservedProjectileIds.Add(Observation.Projectile, FIntPoint(Observation.RequestId, Observation.CastId));
		ObservedProjectileSubmissionIds.Add(Observation.Projectile, Observation.SubmissionKey.Id);
		ObservedProjectilePaths.Add(Observation.Projectile, Observation.ActorPath.IsEmpty() ? Observation.Projectile->GetPathName() : Observation.ActorPath);
	}
	if (Observation.bRemoved)
	{
		if (Observation.SubmissionKey.Id.IsValid()) RemovedProjectileIds.Add(Observation.SubmissionKey.Id);
		if (!Observation.ActorPath.IsEmpty()) RemovedProjectilePaths.Add(Observation.ActorPath);
		else if (Observation.Projectile.IsValid()) RemovedProjectilePaths.Add(Observation.Projectile->GetPathName());
	}
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event"), Observation.bRemoved ? TEXT("projectile_removed") : Observation.bDelivered ? TEXT("projectile_delivery") : TEXT("projectile_created"));
	Json->SetStringField(TEXT("skill_id"), Observation.SkillId.ToString());
	Json->SetNumberField(TEXT("request_id"), Observation.RequestId);
	Json->SetNumberField(TEXT("cast_id"), Observation.CastId);
	Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(Observation.CastToken.RuntimeGeneration));
	Json->SetStringField(TEXT("submission_key"), Observation.SubmissionKey.Id.ToString());
	Json->SetNumberField(TEXT("submission_point_id"), static_cast<double>(Observation.SubmissionPointId));
	Json->SetBoolField(TEXT("delivered"), Observation.bDelivered);
	Json->SetBoolField(TEXT("delivery_accepted"), Observation.bDeliveryAccepted);
	Json->SetBoolField(TEXT("removed"), Observation.bRemoved);
	Json->SetStringField(TEXT("reason"), Observation.Reason.ToString());
	Json->SetObjectField(TEXT("spawn_transform"), TransformJson(Observation.SpawnTransform));
	if (auto* Projectile = Observation.Projectile.Get())
	{
		Json->SetStringField(TEXT("projectile_path"), Observation.ActorPath.IsEmpty() ? Projectile->GetPathName() : Observation.ActorPath);
		if (!Observation.bRemoved) Json->SetObjectField(TEXT("projectile_transform"), TransformJson(Projectile->GetActorTransform()));
		const FVector Velocity = Projectile->GetProjectileMovement() ? Projectile->GetProjectileMovement()->Velocity : FVector::ZeroVector;
		Json->SetArrayField(TEXT("movement_velocity_cm_s"), {MakeShared<FJsonValueNumber>(Velocity.X), MakeShared<FJsonValueNumber>(Velocity.Y), MakeShared<FJsonValueNumber>(Velocity.Z)});
		const FRotator Direction = Velocity.GetSafeNormal().Rotation();
		Json->SetArrayField(TEXT("execution_direction_pitch_yaw_roll_deg"), {MakeShared<FJsonValueNumber>(Direction.Pitch), MakeShared<FJsonValueNumber>(Direction.Yaw), MakeShared<FJsonValueNumber>(Direction.Roll)});
		if (const auto* Collision = Projectile->GetCollisionShape())
		{
			Json->SetNumberField(TEXT("collision_sphere_radius_cm"), Collision->GetScaledSphereRadius());
			Json->SetObjectField(TEXT("collision_transform"), TransformJson(Collision->GetComponentTransform()));
			Json->SetStringField(TEXT("collision_profile"), Collision->GetCollisionProfileName().ToString());
		}
		if (const auto* Visual = Projectile->GetProjectileVisual())
		{
			Json->SetStringField(TEXT("visual_component"), Visual->GetPathName());
			Json->SetStringField(TEXT("visual_mesh"), Visual->GetStaticMesh() ? Visual->GetStaticMesh()->GetPathName() : FString());
			Json->SetStringField(TEXT("visual_parent_component"), Visual->GetAttachParent() ? Visual->GetAttachParent()->GetPathName() : FString());
			Json->SetBoolField(TEXT("visual_visible"), Visual->IsVisible());
			Json->SetObjectField(TEXT("visual_transform"), TransformJson(Visual->GetComponentTransform()));
			Json->SetObjectField(TEXT("visual_forward"), TransformJson(FTransform(Visual->GetForwardVector().Rotation())));
			Json->SetBoolField(TEXT("visual_visible"), Visual->IsVisible());
			if (const UStaticMesh* Mesh = Visual->GetStaticMesh())
			{
				const FBox Bounds = Mesh->GetBoundingBox();
				Json->SetArrayField(TEXT("visual_local_bounds_center_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetCenter().X), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Y), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Z)});
				Json->SetArrayField(TEXT("visual_local_bounds_extent_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetExtent().X), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Y), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Z)});
				Json->SetStringField(TEXT("visual_mesh_lighting_guid"), Mesh->GetLightingGuid().ToString());
				Json->SetStringField(TEXT("visual_package_guid"), Mesh->GetPackage()->GetPersistentGuid().ToString());
			}
			else Json->SetStringField(TEXT("visual_mesh_status"), TEXT("missing"));
		}
	}
	Json->SetNumberField(TEXT("observation_world_time_seconds"), Observation.ObservationWorldTime);
	Json->SetNumberField(TEXT("observation_wall_seconds_since_start"), Observation.ObservationWallTime - SessionStartWall);
	Json->SetNumberField(TEXT("since_request_world_seconds"), Observation.ObservationWorldTime - RequestWorldTime);
	Json->SetNumberField(TEXT("since_request_wall_seconds"), Observation.ObservationWallTime - RequestWallTime);
	AppendJsonLine(TEXT("events.jsonl"), Json);
}

void USkillLabSpatialExportSubsystem::RecordFeedbackObservation(const FSkillLabFeedbackObservation& Observation)
{
	ASkillLabSkillFeedbackActor* Actor = Observation.Actor.Get();
	if (!IsValid(Actor) || Actor->GetWorld() != GetWorld() || Actor->GetObservedSourceActor() != Caster.Get()
		|| Actor->GetObservedRuntime() != Runtime.Get() || Actor->GetObservedRuntimeGeneration() != CapturedRuntimeGeneration
		|| Actor->GetObservedRequestId() != RequestId || Actor->GetObservedCastId() != CapturedCastId
		|| Actor->GetObservedCastToken().Runtime != Runtime || RequestId <= 0) return;
	const FGuid SubmissionId = Actor->GetObservedSubmissionKey().Id;
	if (Observation.Event == TEXT("feedback_created"))
	{
		if (!ObservedFeedbackActors.Contains(Actor)) ObservedFeedbackActors.Add(Actor);
		ObservedFeedbackSubmissionIds.Add(Actor, SubmissionId);
		ObservedFeedbackPaths.Add(Actor, Actor->GetPathName());
	}
	if (Observation.Event == TEXT("feedback_removed") && SubmissionId.IsValid()) RemovedFeedbackIds.Add(SubmissionId);
	if (Observation.Event == TEXT("feedback_removed")) RemovedFeedbackPaths.Add(Actor->GetPathName());
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event"), Observation.Event.ToString());
	Json->SetStringField(TEXT("actor_path"), Actor->GetPathName());
	Json->SetStringField(TEXT("skill_id"), Actor->GetObservedSkillId().ToString());
	Json->SetStringField(TEXT("source_actor"), Actor->GetObservedSourceActor()->GetPathName());
	Json->SetStringField(TEXT("target_actor"), Actor->GetObservedTargetActor() ? Actor->GetObservedTargetActor()->GetPathName() : FString());
	Json->SetNumberField(TEXT("request_id"), Actor->GetObservedRequestId());
	Json->SetNumberField(TEXT("cast_id"), Actor->GetObservedCastId());
	Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(Actor->GetObservedRuntimeGeneration()));
	Json->SetStringField(TEXT("submission_key"), SubmissionId.ToString());
	Json->SetNumberField(TEXT("submission_point_id"), static_cast<double>(Actor->GetObservedSubmissionPointId()));
	Json->SetStringField(TEXT("event_name"), Actor->GetObservedEventName().ToString());
	Json->SetStringField(TEXT("effect_instance_id"), Actor->GetObservedEffectInstanceId().ToString());
	Json->SetStringField(TEXT("binding_name"), Actor->GetObservedBindingName().ToString());
	Json->SetStringField(TEXT("shape"), Actor->GetObservedShapeName().ToString());
	Json->SetNumberField(TEXT("configured_radius_cm"), Actor->GetObservedRadiusCm());
	Json->SetNumberField(TEXT("radius_basis_cm"), Actor->GetObservedRadiusBasisCm());
	Json->SetNumberField(TEXT("visual_scale"), Actor->GetObservedVisualScale());
	Json->SetStringField(TEXT("radius_basis_kind"), Actor->GetObservedSizeBasisKind().ToString());
	Json->SetObjectField(TEXT("transform"), TransformJson(Observation.Transform));
	Json->SetNumberField(TEXT("observation_world_time_seconds"), Observation.WorldTimeSeconds);
	Json->SetNumberField(TEXT("observation_wall_seconds_since_start"), Observation.WallTimeSeconds - SessionStartWall);
	Json->SetNumberField(TEXT("since_request_world_seconds"), Observation.WorldTimeSeconds - RequestWorldTime);
	Json->SetNumberField(TEXT("since_request_wall_seconds"), Observation.WallTimeSeconds - RequestWallTime);
	Json->SetStringField(TEXT("removal_reason"), Observation.Reason.ToString());
	TArray<TSharedPtr<FJsonValue>> ComponentsJson;
	TArray<UStaticMeshComponent*> Components;
	Actor->GetComponents(Components);
	for (const UStaticMeshComponent* Component : Components)
	{
		if (!Component) continue;
		TSharedRef<FJsonObject> ComponentJson = MakeShared<FJsonObject>();
		ComponentJson->SetStringField(TEXT("component_path"), Component->GetPathName());
		ComponentJson->SetStringField(TEXT("parent_component_path"), Component->GetAttachParent() ? Component->GetAttachParent()->GetPathName() : FString());
		ComponentJson->SetBoolField(TEXT("visible"), Component->IsVisible());
		ComponentJson->SetObjectField(TEXT("world_transform"), TransformJson(Component->GetComponentTransform()));
		if (const UStaticMesh* Mesh = Component->GetStaticMesh())
		{
			ComponentJson->SetStringField(TEXT("mesh"), Mesh->GetPathName());
			const FBox Bounds = Mesh->GetBoundingBox();
			ComponentJson->SetArrayField(TEXT("local_bounds_center_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetCenter().X), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Y), MakeShared<FJsonValueNumber>(Bounds.GetCenter().Z)});
			ComponentJson->SetArrayField(TEXT("local_bounds_extent_cm"), {MakeShared<FJsonValueNumber>(Bounds.GetExtent().X), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Y), MakeShared<FJsonValueNumber>(Bounds.GetExtent().Z)});
			ComponentJson->SetStringField(TEXT("lighting_guid"), Mesh->GetLightingGuid().ToString());
			ComponentJson->SetStringField(TEXT("package_guid"), Mesh->GetPackage()->GetPersistentGuid().ToString());
		}
		else ComponentJson->SetStringField(TEXT("mesh_status"), TEXT("missing"));
		ComponentsJson.Add(MakeShared<FJsonValueObject>(ComponentJson));
	}
	Json->SetArrayField(TEXT("static_mesh_components"), ComponentsJson);
	AppendJsonLine(TEXT("events.jsonl"), Json);
}

void USkillLabSpatialExportSubsystem::RecordAnimationNotify(const FSkillLabCastExecutionToken& Token, UAnimSequenceBase* Animation, const FGameplayTag EventTag, const bool bStateEnd, const bool bForwarded)
{
	if (!bRequestAccepted || Token.Runtime != Runtime || Token.RuntimeGeneration != CapturedRuntimeGeneration || Token.CastId != CapturedCastId) return;
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("event"), TEXT("animation_notify"));
	Json->SetNumberField(TEXT("request_id"), RequestId);
	Json->SetNumberField(TEXT("cast_id"), Token.CastId);
	Json->SetNumberField(TEXT("runtime_generation"), static_cast<double>(Token.RuntimeGeneration));
	Json->SetStringField(TEXT("animation"), Animation ? Animation->GetPathName() : FString());
	Json->SetStringField(TEXT("event_tag"), EventTag.ToString());
	Json->SetBoolField(TEXT("state_end"), bStateEnd);
	Json->SetBoolField(TEXT("forwarded"), bForwarded);
	Json->SetNumberField(TEXT("world_time_seconds"), GetWorld()->GetTimeSeconds());
	Json->SetNumberField(TEXT("since_request_world_seconds"), GetWorld()->GetTimeSeconds() - RequestWorldTime);
	Json->SetNumberField(TEXT("since_request_wall_seconds"), FPlatformTime::Seconds() - RequestWallTime);
	AppendJsonLine(TEXT("events.jsonl"), Json);
}

