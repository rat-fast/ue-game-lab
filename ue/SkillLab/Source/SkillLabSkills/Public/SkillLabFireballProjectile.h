#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabFireballProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UStaticMeshComponent;

UCLASS()
class SKILLLABSKILLS_API ASkillLabFireballProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASkillLabFireballProjectile();
	void Initialize(AActor* InSource, AActor* InTarget, const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, int32 InCastId);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* HitActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	void ResolveImpact(AActor* HitActor);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Source;

	UPROPERTY(Transient)
	TObjectPtr<AActor> Target;

	FSkillLabSkillDefinition Definition;
	FSkillLabSkillRequest Request;
	int32 CastId = 0;
	bool bResolved = false;
};
