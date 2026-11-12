#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillLabProjectileTypes.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSubmissionResults.h"
#include "SkillLabFireballProjectile.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabSkillEffectEventAdapter;
class USkillLabSkillEffectExecutor;
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
	void Initialize(AActor* InSource, AActor* InTarget, const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, int32 InCastId, const FSkillLabProjectileParameters& InParameters = FSkillLabProjectileParameters(), const FSkillLabProductionHandle& Delivery = {});
	// FinishSpawning 完成后记录交付，再开放碰撞；同步 overlap 自毁不改变交付事实。
	bool ConfirmDelivery();
	FSkillLabProjectileResolved OnResolved;
	bool IsDeliveryConfirmed() const { return bDeliveryConfirmed; }
	const FSkillLabSkillResult& GetImpactResult() const { return ImpactResult; }

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
	TWeakObjectPtr<AActor> Source;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Target;

	friend class USkillLabSkillEffectExecutor;
	TWeakObjectPtr<USkillLabAbilitySystemComponent> SourceASC;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> OriginRuntime;
	TWeakObjectPtr<USkillLabSkillEffectEventAdapter> EffectObserver;
	FSkillLabSkillResult Origin, ImpactResult;
	int32 SourceTeam = 0;
	bool bDeliveryConfirmed = false;
	bool bHitEffectConsumed = false;
	FSkillLabSkillDefinition Definition;
	FSkillLabSkillRequest Request;
	FSkillLabProjectileParameters Parameters;
	int32 CastId = 0;
	bool bResolved = false;
	bool bInitialized = false;
	bool bActivationStarted = false;
	bool bDeliveryPending = false;
	FSkillLabProductionHandle Delivery;
	FTimerHandle ExpireTimerHandle;
};
