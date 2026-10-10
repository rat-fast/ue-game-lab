#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillLabProjectileTypes.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabFireballProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

DECLARE_DELEGATE_TwoParams(FSkillLabProjectileResolved, AActor*, FName);

UCLASS()
class SKILLLABSKILLS_API ASkillLabFireballProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASkillLabFireballProjectile();
	void Initialize(AActor* InSource, AActor* InTarget, const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, int32 InCastId, const FSkillLabProjectileParameters& InParameters = FSkillLabProjectileParameters());
	FSkillLabProjectileResolved OnResolved;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* HitActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	void ResolveImpact(AActor* HitActor, FName ResolveReason);
	void ActivateProjectile();
	void ExpireProjectile();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VisualMaterial;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Source;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Target;

	FSkillLabSkillDefinition Definition;
	FSkillLabSkillRequest Request;
	FSkillLabProjectileParameters Parameters;
	int32 CastId = 0;
	bool bResolved = false;
	bool bInitialized = false;
	bool bActivationStarted = false;
	FTimerHandle ExpireTimerHandle;
};
