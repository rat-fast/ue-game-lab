#include "SkillLabSpatialExportSubsystem.h"
#include "SkillLabSpatialExportJson.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"
#include "ImageUtils.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
using namespace SkillLabSpatialExport;

void USkillLabSpatialExportSubsystem::ReleaseCaptureResources()
{
	for (const auto& Pair : PoseHandles) if (Pair.Key.IsValid()) Pair.Key->UnregisterOnBoneTransformsFinalizedDelegate(Pair.Value);
	PoseHandles.Reset();
	for (const TObjectPtr<USceneCaptureComponent2D>& Capture : CaptureComponents) if (Capture) Capture->DestroyComponent();
	CaptureComponents.Reset(); CaptureTargets.Reset();
}

void USkillLabSpatialExportSubsystem::CaptureViews(const TSharedRef<FJsonObject>& Sample, const TArray<TSharedPtr<FJsonValue>>& Queries)
{
	if (!GDynamicRHI || FString(GDynamicRHI->GetName()).Contains(TEXT("Null")))
	{ Errors.AddUnique(TEXT("capture requires real RHI")); return; }
	const auto& Views = Request->GetArrayField(TEXT("views"));
	const auto& Resolution = Request->GetArrayField(TEXT("resolution"));
	const int32 Width = Resolution[0]->AsNumber(), Height = Resolution[1]->AsNumber();
	if (CaptureComponents.IsEmpty()) for (int32 Index = 0; Index < Views.Num(); ++Index)
	{
		auto* TargetTexture = NewObject<UTextureRenderTarget2D>(this);
		TargetTexture->InitCustomFormat(Width, Height, PF_B8G8R8A8, false);
		TargetTexture->UpdateResourceImmediate(true); CaptureTargets.Add(TargetTexture);
		auto* Capture = NewObject<USceneCaptureComponent2D>(this);
		Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->TextureTarget = TargetTexture;
		Capture->bAlwaysPersistRenderingState = true;
		Capture->ShowFlags.SetMotionBlur(false);
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->RegisterComponentWithWorld(GetWorld()); CaptureComponents.Add(Capture);
	}
	// 同一游戏线程阶段先冻结所有相机，然后按顺序提交/同步回读；不运行额外 World tick。
	TArray<FMinimalViewInfo> Cameras;
	for (const auto& Value : Views)
	{
		const auto View = Value->AsObject(); FMinimalViewInfo Camera;
		Camera.AspectRatio = static_cast<float>(Width) / Height;
		if (View->GetStringField(TEXT("type")) == TEXT("game"))
		{
			auto* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
			if (!PC || !PC->PlayerCameraManager) { Errors.AddUnique(TEXT("game camera unavailable")); return; }
			Camera = PC->PlayerCameraManager->GetCameraCacheView();
			if (!Camera.bConstrainAspectRatio) Camera.AspectRatio = static_cast<float>(Width) / Height;
		}
		else
		{
			const auto& L = View->GetArrayField(TEXT("location_cm")); const auto& R = View->GetArrayField(TEXT("rotation_deg"));
			Camera.Location = FVector(L[0]->AsNumber(), L[1]->AsNumber(), L[2]->AsNumber());
			Camera.Rotation = FRotator(R[0]->AsNumber(), R[1]->AsNumber(), R[2]->AsNumber()); Camera.FOV = View->GetNumberField(TEXT("fov_deg"));
		}
		Cameras.Add(Camera);
	}
	GetWorld()->SendAllEndOfFrameUpdates(); FlushRenderingCommands();
	TArray<TSharedPtr<FJsonValue>> Images;
	for (int32 Index = 0; Index < Cameras.Num(); ++Index)
	{
		const auto View = Views[Index]->AsObject(); const auto& Camera = Cameras[Index];
		USceneCaptureComponent2D* Capture = CaptureComponents[Index].Get();
		Capture->SetWorldLocationAndRotation(Camera.Location, Camera.Rotation);
		Capture->ProjectionType = ECameraProjectionMode::Perspective; Capture->FOVAngle = Camera.FOV;
		Capture->PostProcessSettings = Camera.PostProcessSettings; Capture->PostProcessBlendWeight = Camera.PostProcessBlendWeight;
		Capture->bUseCustomProjectionMatrix = true; Capture->CustomProjectionMatrix = Camera.CalculateProjectionMatrix();
		Capture->CaptureScene(); FlushRenderingCommands();
		auto* Resource = CaptureTargets[Index]->GameThread_GetRenderTargetResource();
		const FString ViewId = View->GetStringField(TEXT("id"));
		auto SavePixels = [&](const FString& Folder) -> bool
		{
			TArray<FColor> Pixels;
			if (!Resource->ReadPixels(Pixels) || Pixels.Num() != Width * Height) return false;
			for (auto& Pixel : Pixels) Pixel.A = 255;
			TArray64<uint8> Png; FImageUtils::PNGCompressImageArray(Width, Height, Pixels, Png);
			const FString Relative = FString::Printf(TEXT("%s/%s/%06d.png"), *Folder, *ViewId, SampleIndex);
			const FString Path = FPaths::Combine(OutputDirectory, Relative);
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			return !Png.IsEmpty() && FFileHelper::SaveArrayToFile(Png, *Path);
		};
		if (!SavePixels(TEXT("frames"))) { Errors.AddUnique(TEXT("RHI readback/PNG write failed: ") + ViewId); continue; }
		TSharedRef<FJsonObject> Image = MakeShared<FJsonObject>();
		Image->SetStringField(TEXT("view_id"), ViewId); Image->SetStringField(TEXT("type"), View->GetStringField(TEXT("type")));
		Image->SetStringField(TEXT("image"), FString::Printf(TEXT("frames/%s/%06d.png"), *ViewId, SampleIndex));
		Image->SetObjectField(TEXT("camera_transform"), TransformJson(FTransform(Camera.Rotation, Camera.Location)));
		Image->SetNumberField(TEXT("fov_horizontal_deg"), Camera.FOV); Image->SetNumberField(TEXT("aspect_ratio"), Camera.AspectRatio);
		Image->SetBoolField(TEXT("constrain_aspect_ratio"), Camera.bConstrainAspectRatio);
		Image->SetStringField(TEXT("rhi"), GDynamicRHI->GetName());
		Image->SetNumberField(TEXT("game_frame"), static_cast<double>(GFrameCounter));
		Image->SetNumberField(TEXT("capture_request_game_frame"), static_cast<double>(GFrameCounter));
		Image->SetNumberField(TEXT("readback_completed_game_thread_frame"), static_cast<double>(GFrameCounter));
		Image->SetStringField(TEXT("render_capture_id"), FString::Printf(TEXT("%s:%06d:%s"), *RunId, SampleIndex, *ViewId));
		Image->SetStringField(TEXT("engine_gpu_frame_identity"), TEXT("unavailable; render frame is associated by synchronous scene capture and readback request id"));
		Image->SetNumberField(TEXT("world_simulation_time_seconds"), GetWorld()->GetTimeSeconds());
		Image->SetNumberField(TEXT("world_time_seconds"), GetWorld()->GetTimeSeconds());
		Image->SetStringField(TEXT("capture_method"), TEXT("real_world_scene_capture_synchronous_readback"));
		Image->SetStringField(TEXT("render_settings"), TEXT("game camera scene; no HUD; motion blur and temporal AA disabled; output aspect when unconstrained"));
		Image->SetStringField(TEXT("sync"), TEXT("post_world_tick_end_of_frame_updates_flush_capture_flush_readpixels_no_intervening_tick"));
		TArray<TSharedPtr<FJsonValue>> Matrix;
		for (int32 Row = 0; Row < 4; ++Row) for (int32 Col = 0; Col < 4; ++Col) Matrix.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Capture->CustomProjectionMatrix.M[Row][Col])));
		Image->SetArrayField(TEXT("projection_matrix_row_major"), Matrix);
		if (Request->GetBoolField(TEXT("markers")))
		{
			FCanvas Canvas(Resource, nullptr, GetWorld(), GetWorld()->GetFeatureLevel());
			const FMatrix ViewRotation = FInverseRotationMatrix(Camera.Rotation) * FMatrix(FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));
			const FMatrix ViewProjection = FTranslationMatrix(-Camera.Location) * ViewRotation * Capture->CustomProjectionMatrix;
			auto Project = [&](const FVector& P, FVector2D& Out)
			{
				const FVector4 Clip = ViewProjection.TransformFVector4(FVector4(P, 1)); if (Clip.W <= 0) return false;
				Out = FVector2D((0.5 + Clip.X / Clip.W * 0.5) * Width, (0.5 - Clip.Y / Clip.W * 0.5) * Height); return true;
			};
			for (const auto& Value : Queries)
			{
				const auto Q = Value->AsObject(); const TSharedPtr<FJsonObject>* T = nullptr;
				if (!Q->TryGetObjectField(TEXT("world_transform"), T)) continue;
				const auto& L = (*T)->GetArrayField(TEXT("location_cm")); const auto& R = (*T)->GetArrayField(TEXT("rotation_deg"));
				const FVector Origin(L[0]->AsNumber(), L[1]->AsNumber(), L[2]->AsNumber());
				const FRotator Rot(R[0]->AsNumber(), R[1]->AsNumber(), R[2]->AsNumber()); FVector2D Start;
				if (!Project(Origin, Start)) continue;
				const FColor Colors[] = {FColor::Red, FColor::Green, FColor::Blue};
				for (int32 Axis = 0; Axis < 3; ++Axis)
				{
					FVector Offset = FVector::ZeroVector; Offset[Axis] = 25; FVector2D End;
					if (Project(Origin + Rot.RotateVector(Offset), End)) { FCanvasLineItem Line(Start, End); Line.SetColor(Colors[Axis]); Line.LineThickness = 2; Canvas.DrawItem(Line); }
				}
				FString Name; Q->TryGetStringField(TEXT("name"), Name);
				FCanvasTextItem Label(Start + FVector2D(4,-14), FText::FromString(Name), GEngine->GetSmallFont(), FLinearColor::White); Label.EnableShadow(FLinearColor::Black); Canvas.DrawItem(Label);
			}
			Canvas.Flush_GameThread(); FlushRenderingCommands();
			if (SavePixels(TEXT("marked"))) Image->SetStringField(TEXT("marked_image"), FString::Printf(TEXT("marked/%s/%06d.png"), *ViewId, SampleIndex));
			else Errors.AddUnique(TEXT("marked PNG write failed: ") + ViewId);
			Image->SetStringField(TEXT("marker_policy"), TEXT("query name and local XYZ axes 25cm; overlay without depth occlusion; clean PNG preserved"));
		}
		Images.Add(MakeShared<FJsonValueObject>(Image));
	}
	Sample->SetArrayField(TEXT("views"), Images);
}
