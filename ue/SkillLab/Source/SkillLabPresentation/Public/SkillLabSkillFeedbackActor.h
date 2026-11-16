#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillLabSpatialTransform.h"
#include "SkillLabSkillFeedbackActor.generated.h"

class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM()
enum class ESkillLabFeedbackShape : uint8
{
	Ring,
	Target,
	TargetFrame,
	Buff,
	ProjectileImpact
};

/** Self-contained temporary world feedback actor. Component/material lifetime belongs to the actor. */
UCLASS(NotBlueprintable)
class SKILLLABPRESENTATION_API ASkillLabSkillFeedbackActor : public AActor
{
	GENERATED_BODY()

public:
	ASkillLabSkillFeedbackActor();
	void InitializeShape(ESkillLabFeedbackShape InShape, float Radius, const FColor& Color, float LifeSeconds);
	void InitializeText(const FString& Text, const FColor& Color, float LifeSeconds);
	void SetSpatialMesh(UStaticMesh* Mesh);
	void StartSpatialFollow(const FSkillLabSpatialDefinition& InDefinition, AActor* InSource, AActor* InTarget,
		const FVector& InGround, const FQuat& InitialRotation);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShapeMesh;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> FramePieces;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> TextComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial;

	ESkillLabFeedbackShape Shape = ESkillLabFeedbackShape::Target;
	float InitialLife = 1.0f;
	float Elapsed = 0.0f;
	FColor InitialTextColor = FColor::White;
	bool bSpatialFollow = false;
	FSkillLabSpatialDefinition SpatialDefinition;
	TWeakObjectPtr<AActor> SpatialSource;
	TWeakObjectPtr<AActor> SpatialTarget;
	FVector SpatialGround = FVector::ZeroVector;
	FQuat FixedRotation = FQuat::Identity;
};
