#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillLabSpatialTransform.h"
#include "SkillLabSkillTypes.h"
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
	void SetObservationSource(const FSkillLabSkillResult& Result, FName BindingName = NAME_None, float RadiusBasisCm = 0.0f, float VisualScale = 1.0f);
	FName GetObservedSkillId() const { return ObservedSkillId; }
	FName GetObservedEventName() const { return ObservedEventName; }
	FName GetObservedEffectInstanceId() const { return ObservedEffectInstanceId; }
	FName GetObservedBindingName() const { return ObservedBindingName; }
	AActor* GetObservedSourceActor() const { return ObservedSourceActor.Get(); }
	AActor* GetObservedTargetActor() const { return ObservedTargetActor.Get(); }
	int32 GetObservedRequestId() const { return ObservedRequestId; }
	int32 GetObservedCastId() const { return ObservedCastId; }
	const FSkillLabCastExecutionToken& GetObservedCastToken() const { return ObservedCastToken; }
	const FSkillLabSubmissionKey& GetObservedSubmissionKey() const { return ObservedSubmissionKey; }
	USkillLabSkillRuntimeComponent* GetObservedRuntime() const { return ObservedCastToken.Runtime.Get(); }
	uint64 GetObservedRuntimeGeneration() const { return ObservedCastToken.RuntimeGeneration; }
	int64 GetObservedSubmissionPointId() const { return ObservedSubmissionPointId; }
	FName GetObservedShapeName() const;
	float GetObservedRadiusCm() const { return ObservedRadiusCm; }
	float GetObservedRadiusBasisCm() const { return ObservedRadiusBasisCm; }
	float GetObservedVisualScale() const { return ObservedVisualScale; }
	FName GetObservedSizeBasisKind() const { return ObservedSizeBasisKind; }
	void SetObservedRemovalCause(FName Reason) { PendingRemovalReason = Reason; }

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
	FName ObservedSkillId = NAME_None;
	FName ObservedEventName = NAME_None;
	FName ObservedEffectInstanceId = NAME_None;
	FName ObservedBindingName = NAME_None;
	TWeakObjectPtr<AActor> ObservedSourceActor;
	TWeakObjectPtr<AActor> ObservedTargetActor;
	int32 ObservedRequestId = 0;
	int32 ObservedCastId = 0;
	FSkillLabCastExecutionToken ObservedCastToken;
	FSkillLabSubmissionKey ObservedSubmissionKey;
	int64 ObservedSubmissionPointId = 0;
	float ObservedRadiusCm = 0.0f;
	float ObservedRadiusBasisCm = 0.0f;
	float ObservedVisualScale = 1.0f;
	FName ObservedSizeBasisKind = NAME_None;
	FName PendingRemovalReason = NAME_None;
	double ObservationCreatedWorldTime = 0.0;
	FSkillLabSpatialDefinition SpatialDefinition;
	TWeakObjectPtr<AActor> SpatialSource;
	TWeakObjectPtr<AActor> SpatialTarget;
	FVector SpatialGround = FVector::ZeroVector;
	FQuat FixedRotation = FQuat::Identity;
};

struct FSkillLabFeedbackObservation
{
	TWeakObjectPtr<ASkillLabSkillFeedbackActor> Actor;
	FName Event = NAME_None;
	FTransform Transform = FTransform::Identity;
	double WorldTimeSeconds = 0.0;
	double WallTimeSeconds = 0.0;
	FName Reason = NAME_None;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FSkillLabFeedbackObserved, const FSkillLabFeedbackObservation&);

class SKILLLABPRESENTATION_API FSkillLabFeedbackObservationBus
{
public:
	static FSkillLabFeedbackObserved& OnObserved();
	static void Broadcast(const FSkillLabFeedbackObservation& Observation);
};
