#include "SkillLabActionOverview.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/Skeleton.h"
#include "AssetCompilingManager.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/TextureCube.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Misc/EngineVersion.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/Material.h"
#include "PreviewScene.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "Serialization/JsonSerializer.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"

namespace SkillLabActionOverview
{
	bool SaveJson(const FString& Path, const FObject& Object)
	{
		FString Text;
		if (!FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text))) return false;
		const FString Temp = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
		return FFileHelper::SaveStringToFile(Text, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
			IFileManager::Get().Move(*Path, *Temp, true, true);
	}

	static TArray<TSharedPtr<FJsonValue>> Vector(const FVector& V)
	{
		return { MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z) };
	}

	static FObject Transform(const FTransform& T)
	{
		FObject J = MakeShared<FJsonObject>();
		J->SetArrayField(TEXT("translationCm"), Vector(T.GetTranslation()));
		const FQuat Q = T.GetRotation();
		J->SetArrayField(TEXT("rotationXYZW"), { MakeShared<FJsonValueNumber>(Q.X), MakeShared<FJsonValueNumber>(Q.Y), MakeShared<FJsonValueNumber>(Q.Z), MakeShared<FJsonValueNumber>(Q.W) });
		J->SetArrayField(TEXT("scale"), Vector(T.GetScale3D()));
		return J;
	}

	static FObject Snapshot(UObject* Asset)
	{
		FObject J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("object"), Asset->GetPathName());
		J->SetStringField(TEXT("class"), Asset->GetClass()->GetPathName());
		J->SetBoolField(TEXT("packageDirty"), Asset->GetOutermost()->IsDirty());
		FString Filename;
		if (FPackageName::DoesPackageExist(Asset->GetOutermost()->GetName(), &Filename))
		{
			Filename = FPaths::ConvertRelativePathToFull(Filename);
			J->SetStringField(TEXT("file"), Filename);
			J->SetStringField(TEXT("md5"), LexToString(FMD5Hash::HashFile(*Filename)));
			TArray<TSharedPtr<FJsonValue>> Sidecars;
			for (const TCHAR* Extension : { TEXT("uexp"), TEXT("ubulk"), TEXT("uptnl") })
			{
				const FString SideFile = FPaths::ChangeExtension(Filename, Extension);
				if (!IFileManager::Get().FileExists(*SideFile)) continue;
				FObject Side = MakeShared<FJsonObject>();
				Side->SetStringField(TEXT("file"), SideFile);
				Side->SetStringField(TEXT("md5"), LexToString(FMD5Hash::HashFile(*SideFile)));
				Sidecars.Add(MakeShared<FJsonValueObject>(Side));
			}
			J->SetArrayField(TEXT("sidecars"), Sidecars);
		}
		return J;
	}

	static bool Evaluate(UAnimSequence* Sequence, UPoseableMeshComponent* Component, double Time)
	{
		// 直接读取源 DataModel 的骨骼姿势；不创建 AnimInstance、不播放、不派发 Notify。
		FMemMark Mark(FMemStack::Get());
		FCompactPose Pose;
		Pose.SetBoneContainer(&Component->RequiredBones);
		Pose.ResetToRefPose();
		FBlendedCurve Curves;
		Curves.InitFrom(Component->RequiredBones);
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData Data(Pose, Curves, Attributes);
		Sequence->GetBonePose(Data, FAnimExtractContext(Time, false), true);
		Component->BoneSpaceTransforms = Component->GetSkinnedAsset()->GetRefSkeleton().GetRefBonePose();
		for (FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			const int32 Index = Component->RequiredBones.MakeMeshPoseIndex(Bone).GetInt();
			if (!Component->BoneSpaceTransforms.IsValidIndex(Index) || Pose[Bone].ContainsNaN()) return false;
			Component->BoneSpaceTransforms[Index] = Pose[Bone];
		}
		Component->MarkRefreshTransformDirty();
		Component->RefreshBoneTransforms();
		return true;
	}

	static FBox SkinBounds(UPoseableMeshComponent* Component, USkeletalMesh* Mesh)
	{
		FBox Bounds(ForceInit);
		FSkeletalMeshLODRenderData& LOD = Mesh->GetResourceForRendering()->LODRenderData[0];
		FSkinWeightVertexBuffer* Weights = Component->GetSkinWeightBuffer(0);
		if (!Weights) return Bounds;
		TArray<FMatrix44f> Matrices;
		Component->GetCurrentRefToLocalMatrices(Matrices, 0);
		for (uint32 Index = 0; Index < LOD.GetNumVertices(); ++Index)
		{
			const FVector V(USkinnedMeshComponent::GetSkinnedVertexPosition(Component, Index, LOD, *Weights, Matrices));
			if (V.ContainsNaN()) return FBox(ForceInit);
			Bounds += V;
		}
		return Bounds;
	}

	FObject Observe(const FString& AssetPath, const FString& MeshOverride, const FString& Directory,
		const FString& Root, const FString& BatchId, bool bMetadataOnly)
	{
		FObject M = MakeShared<FJsonObject>();
		M->SetNumberField(TEXT("schemaVersion"), 1);
		M->SetStringField(TEXT("generator"), TEXT("SkillLabActionOverview/1"));
		M->SetStringField(TEXT("engine"), FEngineVersion::Current().ToString());
		M->SetStringField(TEXT("generatedUtc"), FDateTime::UtcNow().ToIso8601());
		M->SetStringField(TEXT("batchId"), BatchId);
		M->SetStringField(TEXT("generationId"), FPaths::GetCleanFilename(Directory));
		M->SetStringField(TEXT("asset"), AssetPath);
		M->SetStringField(TEXT("outputRoot"), Root);
		M->SetStringField(TEXT("directory"), Directory);
		M->SetStringField(TEXT("status"), TEXT("running"));
		M->SetStringField(TEXT("visualVerification"), TEXT("pendingIndependentInspection"));
		M->SetStringField(TEXT("playbackMapping"), TEXT("sourceOnly_notEstablished"));
		M->SetStringField(TEXT("optionalOverlays"), TEXT("disabled_bonesSocketsWeaponsTrails"));
		TArray<TSharedPtr<FJsonValue>> Frames;
		auto Fail = [&](const TCHAR* Stage, const FString& Reason, int32 Frame = INDEX_NONE)
		{
			M->SetStringField(TEXT("status"), TEXT("failed"));
			FObject Error = MakeShared<FJsonObject>();
			Error->SetStringField(TEXT("stage"), Stage);
			Error->SetStringField(TEXT("reason"), Reason);
			Error->SetNumberField(TEXT("frame"), Frame);
			M->SetObjectField(TEXT("error"), Error);
			M->SetArrayField(TEXT("frames"), Frames);
			UE_LOG(LogTemp, Warning, TEXT("ActionOverview %s [%s] frame=%d: %s"), *AssetPath, Stage, Frame, *Reason);
			return M;
		};
		if (!IFileManager::Get().MakeDirectory(*Directory, true)) return Fail(TEXT("output"), TEXT("无法建立资产输出目录"));
		if (!SaveJson(Directory / TEXT("manifest.json"), M)) return Fail(TEXT("output"), TEXT("无法写入初始 manifest"));
		UE_LOG(LogTemp, Display, TEXT("ActionOverview Asset=%s Phase=load"), *AssetPath);
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *AssetPath);
		if (!Sequence) return Fail(TEXT("input"), TEXT("输入未解析为 UAnimSequence"));
		USkeleton* Skeleton = Sequence->GetSkeleton();
		if (!Skeleton) return Fail(TEXT("input"), TEXT("缺少 Skeleton"));
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (Sequence->GetAdditiveAnimType() != AAT_None) return Fail(TEXT("compatibility"), TEXT("暂不支持 Additive：需要单独定义基准姿势"));
		const IAnimationDataModel* Model = Sequence->GetDataModel();
		if (!Model || Model->GetNumberOfKeys() < 1) return Fail(TEXT("sourceData"), TEXT("源 DataModel 不可读或没有采样点"));
		const FFrameRate Rate = Model->GetFrameRate();
		const int32 Count = Model->GetNumberOfKeys();
		const double Length = Model->GetPlayLength();
		if (!Rate.IsValid() || Rate.Numerator <= 0 || Count > 100000 || Count != Model->GetNumberOfFrames() + 1 ||
			!FMath::IsNearlyEqual(Rate.AsSeconds(FFrameTime(Count - 1)), Length, 0.00001) ||
			!FMath::IsNearlyEqual(Length, Sequence->GetPlayLength(), 0.00001))
			return Fail(TEXT("sourceData"), TEXT("源帧计数/有理帧率/末端长度不一致，或超出单资产 100000 采样点限制"));
		FObject Timing = MakeShared<FJsonObject>();
		Timing->SetNumberField(TEXT("numerator"), Rate.Numerator);
		Timing->SetNumberField(TEXT("denominator"), Rate.Denominator);
		Timing->SetNumberField(TEXT("intervalCount"), Model->GetNumberOfFrames());
		Timing->SetNumberField(TEXT("sampleCount"), Count);
		Timing->SetNumberField(TEXT("durationSeconds"), Length);
		Timing->SetStringField(TEXT("frameRule"), TEXT("zeroBased_inclusiveSourceEndpoint_seconds=frame*denominator/numerator_noLoopWrap"));
		Timing->SetStringField(TEXT("poseEvaluation"), TEXT("GetBonePose_forceRawData_PoseableMesh_noAnimInstance_noNotify_noPhysics_noPostProcess_noMorphCurves"));
		Timing->SetNumberField(TEXT("targetRateNumerator"), Sequence->GetSamplingFrameRate().Numerator);
		Timing->SetNumberField(TEXT("targetRateDenominator"), Sequence->GetSamplingFrameRate().Denominator);
		M->SetObjectField(TEXT("timing"), Timing);
		M->SetStringField(TEXT("skeleton"), Skeleton->GetPathName());
		USkeletalMesh* Mesh = nullptr;
		FString MeshSource;
		if (!MeshOverride.IsEmpty()) { Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshOverride); MeshSource = TEXT("explicitOverride"); }
		else
		{
			// const 版本不清空不兼容引用，也不搜索并更新 Skeleton 预览引用。
			Mesh = static_cast<const UAnimSequence*>(Sequence)->GetPreviewMesh();
			MeshSource = TEXT("animationSavedPreview");
			if (!Mesh) { Mesh = static_cast<const USkeleton*>(Skeleton)->GetPreviewMesh(); MeshSource = TEXT("skeletonSavedPreview"); }
		}
		M->SetStringField(TEXT("meshSource"), MeshSource);
		M->SetStringField(TEXT("mesh"), Mesh ? Mesh->GetPathName() : MeshOverride);
		if (!Mesh) return Fail(TEXT("previewMesh"), TEXT("缺少可加载的预览 Mesh；请在请求中明确指定 mesh"));
		if (Mesh->GetSkeleton() != Skeleton) return Fail(TEXT("compatibility"), TEXT("本版本仅支持同一 Skeleton，未实现跨 Skeleton 重定向"));
		FAssetCompilingManager::Get().FinishAllCompilation();
		TArray<TSharedPtr<FJsonValue>> Inputs;
		TArray<UObject*> Dependencies = { Sequence, Skeleton, Mesh };
		for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
		{
			UMaterialInterface* Material = Slot.MaterialInterface;
			if (!Material) continue;
			Dependencies.AddUnique(Material);
			Dependencies.AddUnique(Material->GetMaterial());
			TArray<UTexture*> Textures;
			Material->GetUsedTextures(Textures);
			for (UTexture* Texture : Textures) Dependencies.AddUnique(Texture);
			while (UMaterialInstance* Instance = Cast<UMaterialInstance>(Material))
			{
				Material = Instance->Parent;
				if (!Material) break;
				Dependencies.AddUnique(Material);
			}
		}
		TSet<UPackage*> Seen;
		for (UObject* A : Dependencies)
		{
			if (!A || Seen.Contains(A->GetOutermost())) continue;
			Seen.Add(A->GetOutermost());
			FObject S = Snapshot(A);
			if (!S->HasField(TEXT("md5")) || A->GetOutermost()->IsDirty()) return Fail(TEXT("inputSnapshot"), TEXT("输入必须是已保存且无脏状态的包"));
			Inputs.Add(MakeShared<FJsonValueObject>(S));
		}
		M->SetArrayField(TEXT("inputs"), Inputs);
		FObject Motion = MakeShared<FJsonObject>();
		Motion->SetBoolField(TEXT("enabled"), Sequence->bEnableRootMotion);
		Motion->SetBoolField(TEXT("forceRootLock"), Sequence->bForceRootLock);
		Motion->SetNumberField(TEXT("rootLockMode"), Sequence->RootMotionRootLock.GetValue());
		Motion->SetStringField(TEXT("scope"), TEXT("rawSourceRootLocal_perFrame; poseExtractionWithoutRootMotion; forceRootLockMayAffectVisiblePose; componentIdentity_noWorldMovement"));
		Motion->SetObjectField(TEXT("extractedRangeDelta"), Transform(Sequence->ExtractRootMotionFromRange(0, Length, FAnimExtractContext(Length, false))));
		M->SetObjectField(TEXT("rootMotion"), Motion);
		for (int32 Frame = 0; Frame < Count; ++Frame)
		{
			FObject F = MakeShared<FJsonObject>();
			const double Time = Rate.AsSeconds(FFrameTime(Frame));
			F->SetNumberField(TEXT("frame"), Frame);
			F->SetNumberField(TEXT("seconds"), Time);
			FTransform RootTransform;
			Sequence->GetBoneTransform(RootTransform, FSkeletonPoseBoneIndex(0), FAnimExtractContext(Time, false), true);
			if (RootTransform.ContainsNaN()) return Fail(TEXT("rootMotion"), TEXT("源 root 变换不是有限数"), Frame);
			F->SetObjectField(TEXT("sourceRootLocal"), Transform(RootTransform));
			F->SetStringField(TEXT("status"), TEXT("notRendered"));
			Frames.Add(MakeShared<FJsonValueObject>(F));
		}
		M->SetArrayField(TEXT("frames"), Frames);
		if (bMetadataOnly)
		{
			M->SetStringField(TEXT("status"), TEXT("metadataOnly"));
			return M;
		}
		if (!FApp::CanEverRender() || GUsingNullRHI || !GDynamicRHI)
			return Fail(TEXT("renderEnvironment"), TEXT("需要真实 RHI；请使用 -AllowCommandletRendering -d3d11，NullRHI 只能读取元数据"));
		UE_LOG(LogTemp, Display, TEXT("ActionOverview Asset=%s Phase=resources Samples=%d"), *AssetPath, Count);
		Mesh->SetForceMipLevelsToBeResident(30.0f);
		Mesh->WaitForPendingInitOrStreaming();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		FPreviewScene Scene(FPreviewScene::ConstructionValues().SetCreatePhysicsScene(false).ShouldSimulatePhysics(false)
			.AllowAudioPlayback(false).SetTransactional(false).SetLightRotation(FRotator(-40, 135, 0)).SetLightBrightness(3.0f).SetSkyBrightness(0.7f));
		if (!Scene.GetScene() || !Scene.GetScene()->GetSkeletalMeshUpdater()) return Fail(TEXT("renderEnvironment"), TEXT("预览场景缺少 SkeletalMeshUpdater；Commandlet 必须启用 IsClient"));
		UDirectionalLightComponent* Fill = NewObject<UDirectionalLightComponent>(GetTransientPackage());
		Fill->SetIntensity(1.5f);
		Scene.AddComponent(Fill, FTransform(FRotator(-25, -45, 0)));
		UTextureCube* Ambient = LoadObject<UTextureCube>(nullptr, TEXT("/Engine/MapTemplates/Sky/DaylightAmbientCubemap.DaylightAmbientCubemap"));
		if (Ambient) { Scene.SetSkyCubemap(Ambient); Scene.UpdateCaptureContents(); }
		UPoseableMeshComponent* Component = NewObject<UPoseableMeshComponent>(GetTransientPackage());
		Component->SetSkinnedAssetAndUpdate(Mesh);
		Component->SetForcedLOD(1);
		Component->SetComponentTickEnabled(false);
		Component->SetBoundsScale(100.0f);
		Scene.AddComponent(Component, FTransform::Identity);
		if (Component->BoneSpaceTransforms.IsEmpty() || !Component->RequiredBones.IsValid()) return Fail(TEXT("pose"), TEXT("无法初始化 Mesh 骨骼"));
		const FSkeletalMeshRenderData* RenderData = Mesh->GetResourceForRendering();
		if (!RenderData || RenderData->LODRenderData.IsEmpty()) return Fail(TEXT("meshResources"), TEXT("缺少 LOD0 渲染数据"));
		FBox AllBounds(ForceInit);
		TArray<FBox> Bounds;
		UE_LOG(LogTemp, Display, TEXT("ActionOverview Asset=%s Phase=bounds"), *AssetPath);
		for (int32 Frame = 0; Frame < Count; ++Frame)
		{
			if (!Evaluate(Sequence, Component, Rate.AsSeconds(FFrameTime(Frame)))) return Fail(TEXT("pose"), TEXT("源姿势求值失败"), Frame);
			const FBox B = SkinBounds(Component, Mesh);
			if (!B.IsValid) return Fail(TEXT("bounds"), TEXT("无法取得蒙皮顶点范围"), Frame);
			Bounds.Add(B);
			AllBounds += B;
		}
		// 整段蒙皮顶点扫描后一次锁定相机；逐帧不跟随、不缩放。
		const FVector Center = AllBounds.GetCenter();
		const FVector Direction = FVector(1, -1, 0.35).GetSafeNormal();
		const FRotator Rotation = (-Direction).Rotation();
		const FVector Right = Rotation.Quaternion().GetRightVector(), Up = Rotation.Quaternion().GetUpVector();
		double ExtentX = 0, ExtentY = 0;
		for (int32 Corner = 0; Corner < 8; ++Corner)
		{
			FVector V((Corner & 1) ? AllBounds.Max.X : AllBounds.Min.X, (Corner & 2) ? AllBounds.Max.Y : AllBounds.Min.Y, (Corner & 4) ? AllBounds.Max.Z : AllBounds.Min.Z);
			ExtentX = FMath::Max(ExtentX, FMath::Abs((V - Center).Dot(Right)));
			ExtentY = FMath::Max(ExtentY, FMath::Abs((V - Center).Dot(Up)));
		}
		const double Width = FMath::Max(200.0, 2.36 * FMath::Max(ExtentX, ExtentY));
		constexpr int32 Size = 384;
		for (int32 Frame = 0; Frame < Count; ++Frame)
			if (Bounds[Frame].GetSize().Z / Width * Size < 64) return Fail(TEXT("framing"), TEXT("全段固定取景会使主体高度低于 64px；需另选预览条件"), Frame);
		UStaticMeshComponent* Floor = NewObject<UStaticMeshComponent>(GetTransientPackage());
		Floor->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
		Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Scene.AddComponent(Floor, FTransform(FQuat::Identity, FVector(Center.X, Center.Y, 0), FVector(Width / 50)));
		UTextureRenderTarget2D* Target = NewObject<UTextureRenderTarget2D>(GetTransientPackage());
		Target->ClearColor = FLinearColor(0.035f, 0.045f, 0.065f);
		Target->InitCustomFormat(Size, Size, PF_B8G8R8A8, false);
		Target->UpdateResourceImmediate(true);
		USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(GetTransientPackage());
		Capture->TextureTarget = Target;
		Capture->ProjectionType = ECameraProjectionMode::Orthographic;
		Capture->OrthoWidth = Width;
		Capture->bCaptureEveryFrame = false;
		Capture->bCaptureOnMovement = false;
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->ShowFlags.SetMotionBlur(false);
		Capture->ShowFlags.SetTemporalAA(false);
		Capture->ShowFlags.SetAntiAliasing(false);
		Capture->ShowFlags.SetAtmosphere(false);
		Capture->PostProcessSettings.bOverride_AutoExposureMethod = true;
		Capture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		Capture->PostProcessSettings.bOverride_AutoExposureBias = true;
		Capture->PostProcessSettings.AutoExposureBias = 0;
		const FVector Location = Center + Direction * (AllBounds.GetSize().Length() + 1000);
		Scene.AddComponent(Capture, FTransform(Rotation, Location));
		FObject Preview = MakeShared<FJsonObject>();
		Preview->SetStringField(TEXT("view"), TEXT("fixedThreeQuarter"));
		Preview->SetStringField(TEXT("rhi"), GDynamicRHI->GetName());
		Preview->SetNumberField(TEXT("width"), Size);
		Preview->SetNumberField(TEXT("height"), Size);
		Preview->SetStringField(TEXT("projection"), TEXT("orthographic"));
		Preview->SetNumberField(TEXT("orthoWidthCm"), Width);
		Preview->SetArrayField(TEXT("locationCm"), Vector(Location));
		Preview->SetArrayField(TEXT("rotationPitchYawRoll"), Vector(FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll)));
		Preview->SetStringField(TEXT("reference"), TEXT("plane_worldZ0_componentIdentity_LOD0_originalMaterials_fixedKey3_rot(-40,135,0)_fill1.5_rot(-25,-45,0)_sky0.7_manualExposure0_noAA_noMotionBlur"));
		Preview->SetStringField(TEXT("ambientCubemap"), Ambient ? Ambient->GetPathName() : TEXT("unavailable"));
		Preview->SetStringField(TEXT("boundsCheck"), TEXT("allSourceSamples_allLOD0SkinnedVertices_globalFixedFraming_margin18percent"));
		M->SetObjectField(TEXT("preview"), Preview);
		FAssetCompilingManager::Get().FinishAllCompilation();
		if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
		if (!IFileManager::Get().MakeDirectory(*(Directory / TEXT("frames")), true)) return Fail(TEXT("output"), TEXT("无法建立逐帧目录"));
		if (!SaveJson(Directory / TEXT("manifest.json"), M)) return Fail(TEXT("output"), TEXT("无法写入进行中 manifest"));
		UE_LOG(LogTemp, Display, TEXT("ActionOverview Asset=%s Phase=capture"), *AssetPath);
		uint32 PreviousRevision = 0;
		for (int32 Frame = 0; Frame < Count; ++Frame)
		{
			// Commandlet 没有引擎主循环。每源采样推进渲染帧身份，使骨骼 buffer revision
			// 更新；否则 CPU 姿势变化但 GPU 会把整段视为同一帧，重复显示第一帧。
			++GFrameCounter;
			if (!Evaluate(Sequence, Component, Rate.AsSeconds(FFrameTime(Frame)))) return Fail(TEXT("pose"), TEXT("源姿势求值失败"), Frame);
			const uint32 Revision = Component->GetBoneTransformRevisionNumber();
			if (Revision <= PreviousRevision) return Fail(TEXT("poseRevision"), TEXT("GPU 骨骼 buffer revision 未推进，禁止重复旧帧"), Frame);
			PreviousRevision = Revision;
			Scene.GetWorld()->SendAllEndOfFrameUpdates();
			FlushRenderingCommands();
			Capture->CaptureScene();
			FlushRenderingCommands();
			TArray<FColor> Pixels;
			if (!Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels) || Pixels.Num() != Size * Size)
				return Fail(TEXT("readback"), TEXT("RHI 像素回读失败"), Frame);
			for (FColor& Pixel : Pixels) Pixel.A = 255;
			TArray64<uint8> PNG;
			FImageUtils::PNGCompressImageArray(Size, Size, Pixels, PNG);
			const FString Relative = FString::Printf(TEXT("frames/frame-%06d.png"), Frame);
			if (PNG.IsEmpty() || !FFileHelper::SaveArrayToFile(PNG, *(Directory / Relative))) return Fail(TEXT("encode"), TEXT("PNG 编码或写入失败"), Frame);
			const FObject F = Frames[Frame]->AsObject().ToSharedRef();
			F->SetStringField(TEXT("image"), Relative);
			F->SetStringField(TEXT("status"), TEXT("rendered"));
			F->SetNumberField(TEXT("boneRevision"), Revision);
			F->SetNumberField(TEXT("renderFrameIdentity"), static_cast<double>(GFrameCounter));
			F->SetArrayField(TEXT("skinnedBoundsMinCm"), Vector(Bounds[Frame].Min));
			F->SetArrayField(TEXT("skinnedBoundsMaxCm"), Vector(Bounds[Frame].Max));
			// 可对照 raw 姿势与图像定位首/中/末帧及滞后一帧问题。
			F->SetStringField(TEXT("poseLocalMd5"), FMD5::HashBytes(reinterpret_cast<const uint8*>(Component->BoneSpaceTransforms.GetData()), Component->BoneSpaceTransforms.Num() * sizeof(FTransform)));
		}
		for (const auto& Input : Inputs)
		{
			const FObject S = Input->AsObject().ToSharedRef();
			if (LexToString(FMD5Hash::HashFile(*S->GetStringField(TEXT("file")))) != S->GetStringField(TEXT("md5")))
				return Fail(TEXT("inputChanged"), TEXT("生成期间输入包发生变化"));
			for (const auto& SideValue : S->GetArrayField(TEXT("sidecars")))
			{
				const TSharedPtr<FJsonObject> Side = SideValue->AsObject();
				if (LexToString(FMD5Hash::HashFile(*Side->GetStringField(TEXT("file")))) != Side->GetStringField(TEXT("md5")))
					return Fail(TEXT("inputChanged"), TEXT("生成期间输入包 sidecar 发生变化"));
			}
		}
		FlushRenderingCommands();
		M->SetArrayField(TEXT("frames"), Frames);
		M->SetStringField(TEXT("status"), TEXT("framesReady"));
		return M;
	}
}
