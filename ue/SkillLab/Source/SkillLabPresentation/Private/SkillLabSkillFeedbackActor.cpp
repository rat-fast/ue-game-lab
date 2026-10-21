#include "SkillLabSkillFeedbackActor.h"

#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/SceneComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"

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
	const float SafeRadius = FMath::Max(1.0f, Radius);
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

void ASkillLabSkillFeedbackActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
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
