#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSpatialExportJson.h"
#include "SkillLabSkillAnimationComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
using namespace SkillLabSpatialExport;

void USkillLabSpatialExportSubsystem::AppendModelSnapshots(TArray<TSharedPtr<FJsonValue>>& Values)
{
	for (const auto& Weak : {Caster, Target, RelatedActor}) if (auto* Actor = Weak.Get())
	{
		TArray<USkeletalMeshComponent*> Meshes; Actor->GetComponents(Meshes);
		for (auto* Mesh : Meshes)
		{
			auto Model = MakeShared<FJsonObject>();
			Model->SetStringField(TEXT("component_path"), Mesh->GetPathName());
			Model->SetStringField(TEXT("skeletal_mesh"), Mesh->GetSkeletalMeshAsset() ? Mesh->GetSkeletalMeshAsset()->GetPathName() : FString());
			Model->SetObjectField(TEXT("world_transform"), TransformJson(Mesh->GetComponentTransform()));
			Model->SetObjectField(TEXT("local_transform"), TransformJson(Mesh->GetRelativeTransform()));
			Model->SetStringField(TEXT("parent_component"), Mesh->GetAttachParent() ? Mesh->GetAttachParent()->GetPathName() : FString());
			Model->SetNumberField(TEXT("bone_revision"), Mesh->GetBoneTransformRevisionNumber());
			const uint64* Frame = PoseFinalizedFrames.Find(Mesh);
			Model->SetNumberField(TEXT("pose_finalized_game_frame"), Frame ? static_cast<double>(*Frame) : -1);
			Model->SetNumberField(TEXT("sample_game_frame"), static_cast<double>(GFrameCounter));
			Model->SetBoolField(TEXT("parallel_evaluation_pending"), Mesh->IsRunningParallelEvaluation());
			Model->SetStringField(TEXT("pose_status"), Frame && *Frame == GFrameCounter && !Mesh->IsRunningParallelEvaluation() ? TEXT("finalized_this_frame") : TEXT("cached_pose"));
			const auto& Bones = Mesh->GetComponentSpaceTransforms();
			Model->SetStringField(TEXT("component_pose_md5"), FMD5::HashBytes(reinterpret_cast<const uint8*>(Bones.GetData()), Bones.Num() * sizeof(FTransform)));
			auto* Anim = Mesh->GetAnimInstance();
			Model->SetStringField(TEXT("anim_instance"), Anim ? Anim->GetPathName() : FString());
			Model->SetStringField(TEXT("anim_class"), Anim ? Anim->GetClass()->GetPathName() : FString());
			TArray<TSharedPtr<FJsonValue>> Playbacks;
			if (const auto* Actions = Actor->FindComponentByClass<USkillLabSkillAnimationComponent>())
			for (const auto& P : Actions->GetPlaybackSnapshot())
			{
				if (P.AnimInstance != Anim || !P.Montage) continue;
				const auto* Instance = Anim->GetMontageInstanceForID(P.InstanceId);
				auto Playback = MakeShared<FJsonObject>();
				Playback->SetNumberField(TEXT("cast_id"), P.Token.CastId);
				Playback->SetNumberField(TEXT("runtime_generation"), static_cast<double>(P.Token.RuntimeGeneration));
				Playback->SetStringField(TEXT("montage"), P.Montage->GetPathName());
				Playback->SetBoolField(TEXT("transient"), P.Montage->HasAnyFlags(RF_Transient));
				Playback->SetStringField(TEXT("notify_animation"), P.NotifyAnimation ? P.NotifyAnimation->GetPathName() : FString());
				Playback->SetNumberField(TEXT("instance_id"), P.InstanceId);
				Playback->SetBoolField(TEXT("instance_present"), Instance != nullptr);
				if (Instance)
				{
					const float Position = Instance->GetPosition();
					Playback->SetNumberField(TEXT("position_seconds"), Position);
					Playback->SetNumberField(TEXT("play_rate"), Instance->GetPlayRate());
					Playback->SetNumberField(TEXT("weight"), Instance->GetWeight());
					Playback->SetBoolField(TEXT("playing"), Instance->IsPlaying());
					Playback->SetStringField(TEXT("section"), Anim->Montage_GetCurrentSection(P.Montage).ToString());
					TArray<TSharedPtr<FJsonValue>> Sources;
					for (const auto& Slot : P.Montage->SlotAnimTracks)
					for (const auto& Segment : Slot.AnimTrack.AnimSegments)
					{
						auto Source = MakeShared<FJsonObject>(); float SourcePosition = 0;
						const auto* ActiveSource = Segment.GetAnimationData(Position, SourcePosition);
						Source->SetStringField(TEXT("slot"), Slot.SlotName.ToString());
						Source->SetStringField(TEXT("animation"), Segment.GetAnimReference() ? Segment.GetAnimReference()->GetPathName() : FString());
						Source->SetBoolField(TEXT("active"), ActiveSource != nullptr);
						Source->SetNumberField(TEXT("source_position_seconds"), SourcePosition);
						Source->SetNumberField(TEXT("segment_play_rate"), Segment.AnimPlayRate);
						Sources.Add(MakeShared<FJsonValueObject>(Source));
					}
					Playback->SetArrayField(TEXT("sources"), Sources);
				}
				Playbacks.Add(MakeShared<FJsonValueObject>(Playback));
			}
			Model->SetArrayField(TEXT("playbacks"), Playbacks);
			Values.Add(MakeShared<FJsonValueObject>(Model));
		}
	}
}
