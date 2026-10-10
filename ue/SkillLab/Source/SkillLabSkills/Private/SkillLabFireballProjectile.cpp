#include "SkillLabFireballProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

ASkillLabFireballProjectile::ASkillLabFireballProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(18.0f);
	// Keep the projectile inert until Initialize has supplied its cast context.
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	RootComponent = Collision;
	Collision->OnComponentBeginOverlap.AddDynamic(this, &ASkillLabFireballProjectile::HandleOverlap);
	Collision->OnComponentHit.AddDynamic(this, &ASkillLabFireballProjectile::HandleHit);

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	Visual->SetRelativeScale3D(FVector(0.62f));
	Visual->SetVisibility(true);
	Visual->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetLightColor(FLinearColor(1.0f, 0.12f, 0.01f));
	GlowLight->SetIntensity(900.0f);
	GlowLight->SetAttenuationRadius(260.0f);
	GlowLight->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bAutoActivate = false;
	Movement->InitialSpeed = 1250.0f;
	Movement->MaxSpeed = 1250.0f;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	InitialLifeSpan = 0.0f;
}

void ASkillLabFireballProjectile::BeginPlay()
{
	Super::BeginPlay();
	VisualMaterial = Visual->CreateAndSetMaterialInstanceDynamic(0);
	if (VisualMaterial)
	{
		VisualMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.0f, 0.24f, 0.035f, 1.0f));
		VisualMaterial->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 0.24f, 0.035f, 1.0f));
	}
	ActivateProjectile();
}

void ASkillLabFireballProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpireTimerHandle);
	}
	// Teardown of a world is not a gameplay failure; external destruction is.
	if (EndPlayReason == EEndPlayReason::Destroyed && bInitialized && !bResolved)
	{
		bResolved = true;
		OnResolved.ExecuteIfBound(nullptr, TEXT("Destroyed"));
	}
	Super::EndPlay(EndPlayReason);
}

void ASkillLabFireballProjectile::Initialize(AActor* InSource, AActor* InTarget, const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, const int32 InCastId, const FSkillLabProjectileParameters& InParameters)
{
	if (bInitialized) return;
	Source = InSource;
	Target = InTarget;
	Definition = InDefinition;
	Request = InRequest;
	CastId = InCastId;
	Parameters = InParameters;
	Parameters.Speed = FMath::IsFinite(Parameters.Speed) ? FMath::Max(1.0f, Parameters.Speed) : 1250.0f;
	Parameters.CollisionRadius = FMath::IsFinite(Parameters.CollisionRadius) ? FMath::Max(1.0f, Parameters.CollisionRadius) : 18.0f;
	Parameters.Lifetime = FMath::IsFinite(Parameters.Lifetime) ? FMath::Max(0.05f, Parameters.Lifetime) : 5.0f;
	Parameters.VisualScale = FMath::IsFinite(Parameters.VisualScale) ? FMath::Max(0.01f, Parameters.VisualScale) : 0.62f;
	Collision->SetSphereRadius(Parameters.CollisionRadius, false);
	Visual->SetRelativeScale3D(FVector(Parameters.VisualScale));
	Movement->InitialSpeed = Parameters.Speed;
	Movement->MaxSpeed = Parameters.Speed;
	bInitialized = true;
	if (Source)
	{
		Collision->IgnoreActorWhenMoving(Source, true);
	}
	const FVector Destination = Target ? GetSkillLabProjectileAimPoint(Target) : Request.TargetLocation;
	const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal();
	Movement->Velocity = Direction * Movement->InitialSpeed;
	ActivateProjectile();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabProjectile] Spawned visible fireball Cast=%d Target=%s Distance=%.0f Speed=%.0f"),
		CastId, *GetNameSafe(Target), FVector::Dist(GetActorLocation(), Destination), Movement->InitialSpeed);
}

void ASkillLabFireballProjectile::ActivateProjectile()
{
	if (!bInitialized || !HasActorBegunPlay() || bActivationStarted || IsActorBeingDestroyed())
	{
		return;
	}
	bActivationStarted = true;
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	if (bResolved || IsActorBeingDestroyed())
	{
		return;
	}
	Movement->Activate(true);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ExpireTimerHandle, this, &ASkillLabFireballProjectile::ExpireProjectile, Parameters.Lifetime, false);
	}
}

void ASkillLabFireballProjectile::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32, bool, const FHitResult&)
{
	if (bActivationStarted && OtherActor && OtherActor != Source)
	{
		// A non-intercepting projectile passes through units other than its selected target.
		const bool bUnitCollision = OtherComponent && OtherComponent->GetCollisionObjectType() == ECC_Pawn;
		if (!Parameters.bInterceptOtherUnits && bUnitCollision && OtherActor != Target) return;
		ResolveImpact(OtherActor, bUnitCollision || OtherActor == Target ? TEXT("Hit") : TEXT("Blocked"));
	}
}

void ASkillLabFireballProjectile::HandleHit(UPrimitiveComponent*, AActor* HitActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (bActivationStarted && HitActor && HitActor != Source)
	{
		ResolveImpact(HitActor, TEXT("Blocked"));
	}
}

void ASkillLabFireballProjectile::ResolveImpact(AActor* HitActor, const FName ResolveReason)
{
	if (bResolved || !bActivationStarted)
	{
		return;
	}
	bResolved = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpireTimerHandle);
	}
	OnResolved.ExecuteIfBound(HitActor, ResolveReason);
	Destroy();
}

void ASkillLabFireballProjectile::ExpireProjectile()
{
	ResolveImpact(nullptr, TEXT("Expired"));
}
