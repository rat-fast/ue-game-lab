#include "SkillLabSkillFeedbackActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SkillLabProjectileTypes.h"
#include "HAL/PlatformTime.h"

namespace
{
	const TCHAR* MeshPathForShape(const ESkillLabFeedbackShape Shape)
	{
		return Shape == ESkillLabFeedbackShape::Ring
			? TEXT("/Engine/BasicShapes/Cylinder.Cylinder")
			: TEXT("/Engine/BasicShapes/Cube.Cube");
	}
}

ASkillLabSkillFeedbackActor::ASkillLabSkillFeedbackActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.016f;
	SetReplicates(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	ShapeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShapeMesh"));
	ShapeMesh->SetupAttachment(SceneRoot);
	ShapeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShapeMesh->SetCastShadow(false);
	ShapeMesh->SetVisibility(false);
	static const FName FrameNames[] = { TEXT("FrameTop"), TEXT("FrameBottom"), TEXT("FrameRight"), TEXT("FrameLeft") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Piece = CreateDefaultSubobject<UStaticMeshComponent>(FrameNames[Index]);
		Piece->SetupAttachment(RootComponent);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetCastShadow(false);
		Piece->SetVisibility(false);
		FramePieces.Add(Piece);
	}

	TextComponent = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetWorldSize(32.0f);
	TextComponent->SetTextRenderColor(FColor::White);
	TextComponent->SetVisibility(false);
	TextComponent->SetupAttachment(RootComponent);
}

void ASkillLabSkillFeedbackActor::InitializeShape(const ESkillLabFeedbackShape InShape, const float Radius, const FColor& Color, const float LifeSeconds)
{
	Shape = InShape;
	ObservedRadiusCm = FMath::Max(1.0f, Radius);
	InitialLife = FMath::Max(0.05f, LifeSeconds);
	Elapsed = 0.0f;
	SetLifeSpan(InitialLife);
	SetActorTickEnabled(true);

	if (!ShapeMesh)
	{
		return;
	}
	ShapeMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPathForShape(Shape)));
	ShapeMesh->SetVisibility(Shape != ESkillLabFeedbackShape::TargetFrame);
	const float SafeRadius = ObservedRadiusCm;
	if (Shape == ESkillLabFeedbackShape::TargetFrame)
	{
		const float Half = SafeRadius / 50.0f;
		const float Thickness = 0.035f;
		const FVector Scales[] = { FVector(Half, Thickness, Thickness), FVector(Half, Thickness, Thickness), FVector(Thickness, Half, Thickness), FVector(Thickness, Half, Thickness) };
		const FVector Locations[] = { FVector(0.0f, SafeRadius, 0.0f), FVector(0.0f, -SafeRadius, 0.0f), FVector(SafeRadius, 0.0f, 0.0f), FVector(-SafeRadius, 0.0f, 0.0f) };
		for (int32 Index = 0; Index < FramePieces.Num(); ++Index)
		{
			FramePieces[Index]->SetStaticMesh(ShapeMesh->GetStaticMesh());
			FramePieces[Index]->SetRelativeScale3D(Scales[Index]);
			FramePieces[Index]->SetRelativeLocation(Locations[Index]);
			FramePieces[Index]->SetVisibility(true);
		}
	}
	else if (Shape == ESkillLabFeedbackShape::Ring)
	{
		ShapeMesh->SetRelativeScale3D(FVector(SafeRadius / 50.0f, SafeRadius / 50.0f, 0.035f));
	}
	else
	{
		ShapeMesh->SetRelativeScale3D(FVector(SafeRadius / 50.0f));
	}

	DynamicMaterial = Shape == ESkillLabFeedbackShape::TargetFrame && FramePieces.Num() > 0
		? FramePieces[0]->CreateAndSetMaterialInstanceDynamic(0)
		: ShapeMesh->CreateAndSetMaterialInstanceDynamic(0);
	if (Shape == ESkillLabFeedbackShape::TargetFrame)
	{
		for (int32 Index = 1; Index < FramePieces.Num(); ++Index)
		{
			FramePieces[Index]->SetMaterial(0, DynamicMaterial);
		}
	}
	if (DynamicMaterial)
	{
		const FLinearColor LinearColor(Color);
		DynamicMaterial->SetVectorParameterValue(TEXT("Color"), LinearColor);
		DynamicMaterial->SetVectorParameterValue(TEXT("BaseColor"), LinearColor);
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.75f);
	}
}

void ASkillLabSkillFeedbackActor::InitializeText(const FString& Text, const FColor& Color, const float LifeSeconds)
{
	InitialLife = FMath::Max(0.05f, LifeSeconds);
	Elapsed = 0.0f;
	SetLifeSpan(InitialLife);
	SetActorTickEnabled(true);
	if (ShapeMesh)
	{
		ShapeMesh->SetVisibility(false);
	}
	for (UStaticMeshComponent* Piece : FramePieces)
	{
		Piece->SetVisibility(false);
	}
	if (TextComponent)
	{
		TextComponent->SetText(FText::FromString(Text));
		InitialTextColor = Color;
		TextComponent->SetTextRenderColor(Color);
		TextComponent->SetVisibility(true);
	}
}

void ASkillLabSkillFeedbackActor::SetSpatialMesh(UStaticMesh* Mesh)
{
	ShapeMesh->SetStaticMesh(Mesh);
	for (UStaticMeshComponent* Piece : FramePieces) Piece->SetStaticMesh(Mesh);
}

void ASkillLabSkillFeedbackActor::SetObservationSource(const FSkillLabSkillResult& Result, const FName BindingName, const float RadiusBasisCm, const float VisualScale)
{
	ObservedSkillId = Result.SkillId;
	ObservedEventName = FName(*UEnum::GetValueAsString(Result.Event));
	ObservedEffectInstanceId = Result.EffectInstanceId;
	ObservedBindingName = BindingName;
	ObservedSourceActor = Result.SourceActor;
	ObservedTargetActor = Result.TargetActor;
	ObservedRequestId = Result.RequestId;
	ObservedCastId = Result.CastId;
	ObservedCastToken = Result.CastToken;
	ObservedSubmissionKey = Result.SubmissionKey;
	ObservedSubmissionPointId = Result.SubmissionPointId;
	ObservedRadiusBasisCm = RadiusBasisCm;
	ObservedVisualScale = VisualScale;
	ObservedSizeBasisKind = RadiusBasisCm > 0.0f ? (Result.VisualRadius > 0.0f ? FName(TEXT("event_visual_radius_cm")) : FName(TEXT("fallback_or_explicit_radius_cm"))) : NAME_None;
	FSkillLabFeedbackObservation Observation;
	Observation.Actor = this;
	Observation.Event = TEXT("feedback_created");
	Observation.Transform = GetActorTransform();
	Observation.WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Observation.WallTimeSeconds = FPlatformTime::Seconds();
	ObservationCreatedWorldTime = Observation.WorldTimeSeconds;
	FSkillLabFeedbackObservationBus::Broadcast(Observation);
}

FName ASkillLabSkillFeedbackActor::GetObservedShapeName() const
{
	switch (Shape)
	{
	case ESkillLabFeedbackShape::Ring: return TEXT("Ring");
	case ESkillLabFeedbackShape::Target: return TEXT("Target");
	case ESkillLabFeedbackShape::TargetFrame: return TEXT("TargetFrame");
	case ESkillLabFeedbackShape::Buff: return TEXT("Buff");
	case ESkillLabFeedbackShape::ProjectileImpact: return TEXT("ProjectileImpact");
	default: return NAME_None;
	}
}

void ASkillLabSkillFeedbackActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ObservedCastToken.IsValid() || ObservedRequestId != 0)
	{
		FSkillLabFeedbackObservation Observation;
		Observation.Actor = this;
		Observation.Event = TEXT("feedback_removed");
		Observation.Transform = GetActorTransform();
		Observation.WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		Observation.WallTimeSeconds = FPlatformTime::Seconds();
		Observation.Reason = !PendingRemovalReason.IsNone() ? PendingRemovalReason
			: EndPlayReason != EEndPlayReason::Destroyed ? FName(TEXT("WorldTeardown"))
			: InitialLife > 0.0f && Elapsed >= InitialLife - 0.05f ? FName(TEXT("LifeSpanExpired"))
			: FName(TEXT("Destroyed"));
		FSkillLabFeedbackObservationBus::Broadcast(Observation);
	}
	Super::EndPlay(EndPlayReason);
}

void ASkillLabSkillFeedbackActor::StartSpatialFollow(const FSkillLabSpatialDefinition& InDefinition, AActor* InSource,
	AActor* InTarget, const FVector& InGround, const FQuat& InitialRotation)
{
	SpatialDefinition = InDefinition; SpatialSource = InSource; SpatialTarget = InTarget;
	SpatialGround = InGround; FixedRotation = InitialRotation; bSpatialFollow = true;
	SetLifeSpan(0.0f);
}

void ASkillLabSkillFeedbackActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bSpatialFollow)
	{
		AActor* Source = SpatialSource.Get(); AActor* Target = SpatialTarget.Get();
		if (!IsValid(Source) || Source->IsActorBeingDestroyed() || !IsValid(Target) || Target->IsActorBeingDestroyed())
		{ PendingRemovalReason = TEXT("FollowReferenceLost"); Destroy(); return; }
		FTransform Transform; FString Error;
		const FVector Direction = GetSkillLabProjectileAimPoint(Target) - GetSkillLabProjectileAimPoint(Source);
		if (!FSkillLabSpatialTransform::Resolve(SpatialDefinition, Source, Target, nullptr, SpatialGround, Direction, Transform, Error))
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillLabSpatial] Follow object=%s: %s"), *SpatialDefinition.Name.ToString(), *Error);
			PendingRemovalReason = TEXT("FollowReferenceResolveFailed"); Destroy(); return;
		}
		SetActorLocationAndRotation(Transform.GetLocation(), SpatialDefinition.bFollowRotation
			? Transform.GetRotation() * SpatialDefinition.VisualRotation.Quaternion() : FixedRotation);
		return;
	}
	Elapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(Elapsed / InitialLife, 0.0f, 1.0f);
	if (TextComponent && TextComponent->IsVisible())
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, 38.0f * DeltaSeconds));
		const APlayerController* PlayerController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (const APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr)
		{
			TextComponent->SetWorldRotation((CameraManager->GetCameraLocation() - TextComponent->GetComponentLocation()).Rotation());
		}
		FColor FadedColor = InitialTextColor;
		FadedColor.A = static_cast<uint8>((1.0f - Alpha) * 255.0f);
		TextComponent->SetTextRenderColor(FadedColor);
	}
	if (DynamicMaterial)
	{
		DynamicMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.75f * (1.0f - Alpha));
	}
}

FSkillLabFeedbackObserved& FSkillLabFeedbackObservationBus::OnObserved()
{
	static FSkillLabFeedbackObserved Delegate;
	return Delegate;
}

void FSkillLabFeedbackObservationBus::Broadcast(const FSkillLabFeedbackObservation& Observation)
{
	OnObserved().Broadcast(Observation);
}
