#include "SkillLabSkillPresentationRouter.h"

#include "Engine/Engine.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"

namespace
{
	void SetMarkerColor(UStaticMeshComponent* Mesh, const FColor& Color)
	{
		if (!Mesh) return;
		if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			const FLinearColor LinearColor(Color);
			Material->SetVectorParameterValue(TEXT("Color"), LinearColor);
			Material->SetVectorParameterValue(TEXT("BaseColor"), LinearColor);
		}
	}
}

void USkillLabSkillPresentationRouter::Bind(USkillLabSkillRuntimeComponent* InSource)
{
	if (Source == InSource) return;
	if (Source) Source->OnNativeSkillEvent.RemoveAll(this);
	Source = InSource;
	if (Source) Source->OnNativeSkillEvent.AddUObject(this, &USkillLabSkillPresentationRouter::HandleEvent);
}

void USkillLabSkillPresentationRouter::HandleEvent(const FSkillLabSkillResult& Result)
{
	UWorld* World = GetWorld();
	if (!World) return;
	ShowScreenMessage(Result);
	if (Result.Event == ESkillLabSkillEvent::TargetSelected && Result.VisualRadius > 0.0f)
	{
		SpawnWorldMarker(Result.TargetLocation + FVector(0.0f, 0.0f, 3.0f), Result.VisualRadius, FColor::Orange, 1.5f, true);
	}
	else if (Result.Event == ESkillLabSkillEvent::Hit && Result.SourceActor && Result.TargetActor)
	{
		const FColor Color = Result.Effect == ESkillLabEffectKind::Healing ? FColor::Green : FColor::Red;
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f), 70.0f, Color, 0.65f, false);
		const FString Label = Result.Effect == ESkillLabEffectKind::Healing
			? FString::Printf(TEXT("HEAL +%.0f"), Result.ActualAmount)
			: FString::Printf(TEXT("DAMAGE -%.0f"), Result.ActualAmount);
		SpawnTextMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 250.0f), Label, Color, 1.5f);
	}
	else if (Result.Event == ESkillLabSkillEvent::BuffApplied && Result.TargetActor)
	{
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 90.0f, FColor::Purple, 4.0f, false);
	}
}

void USkillLabSkillPresentationRouter::ShowScreenMessage(const FSkillLabSkillResult& Result)
{
	if (!GEngine) return;
	FColor Color = FColor::White;
	FString Message;
	switch (Result.Event)
	{
	case ESkillLabSkillEvent::Started:
		Color = FColor::Yellow;
		Message = FString::Printf(TEXT("CAST  %s"), *Result.SkillId.ToString());
		break;
	case ESkillLabSkillEvent::Hit:
		Color = Result.bSuccess ? FColor::Green : FColor::Red;
		Message = FString::Printf(TEXT("HIT  %s  %s%.0f"), *Result.SkillId.ToString(), Result.Effect == ESkillLabEffectKind::Healing ? TEXT("+") : TEXT("-"), Result.ActualAmount);
		break;
	case ESkillLabSkillEvent::BuffApplied:
		Color = FColor::Purple;
		Message = FString::Printf(TEXT("BUFF  %s"), *Result.SkillId.ToString());
		break;
	case ESkillLabSkillEvent::Rejected:
		Color = FColor::Red;
		Message = FString::Printf(TEXT("REJECTED  %s"), *Result.Reason.ToString());
		break;
	default:
		break;
	}
	if (!Message.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, Color, Message);
	}
}

void USkillLabSkillPresentationRouter::SpawnWorldMarker(const FVector& Location, const float Radius, const FColor& Color, const float LifeSeconds, const bool bFlat)
{
	UWorld* World = GetWorld();
	if (!World) return;
	AActor* Marker = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator);
	if (!Marker) return;

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Marker, TEXT("SkillFeedbackMesh"));
	if (!Mesh) { Marker->Destroy(); return; }
	const TCHAR* MeshPath = bFlat ? TEXT("/Engine/BasicShapes/Cylinder.Cylinder") : TEXT("/Engine/BasicShapes/Sphere.Sphere");
	Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, MeshPath));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(bFlat ? FVector(Radius / 50.0f, Radius / 50.0f, 0.025f) : FVector(Radius / 50.0f));
	Marker->SetRootComponent(Mesh);
	Marker->AddInstanceComponent(Mesh);
	Mesh->RegisterComponent();
	SetMarkerColor(Mesh, Color);
	Marker->SetLifeSpan(LifeSeconds);
}

void USkillLabSkillPresentationRouter::SpawnTextMarker(const FVector& Location, const FString& Text, const FColor& Color, const float LifeSeconds)
{
	UWorld* World = GetWorld();
	if (!World) return;
	AActor* Marker = World->SpawnActor<AActor>(AActor::StaticClass(), Location, FRotator::ZeroRotator);
	if (!Marker) return;
	UTextRenderComponent* TextComponent = NewObject<UTextRenderComponent>(Marker, TEXT("SkillFeedbackText"));
	if (!TextComponent) { Marker->Destroy(); return; }
	TextComponent->SetText(FText::FromString(Text));
	TextComponent->SetTextRenderColor(Color);
	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetWorldSize(42.0f);
	TextComponent->SetHorizontalAlignment(EHTA_Center);
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetRootComponent(TextComponent);
	Marker->AddInstanceComponent(TextComponent);
	TextComponent->RegisterComponent();
	Marker->SetLifeSpan(LifeSeconds);
}
