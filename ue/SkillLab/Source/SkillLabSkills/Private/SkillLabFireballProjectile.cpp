#include "SkillLabFireballProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "SkillLabSkillRuntimeComponent.h"

ASkillLabFireballProjectile::ASkillLabFireballProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(18.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
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
	Visual->SetRelativeScale3D(FVector(0.28f));
	Visual->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->InitialSpeed = 1250.0f;
	Movement->MaxSpeed = 1250.0f;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	InitialLifeSpan = 5.0f;
}

void ASkillLabFireballProjectile::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(true);
}

void ASkillLabFireballProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	DrawDebugSphere(GetWorld(), GetActorLocation(), 24.0f, 10, FColor::Orange, false, 0.08f, 0, 3.0f);
}

void ASkillLabFireballProjectile::Initialize(AActor* InSource, AActor* InTarget, const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, const int32 InCastId)
{
	Source = InSource;
	Target = InTarget;
	Definition = InDefinition;
	Request = InRequest;
	CastId = InCastId;
	if (Source)
	{
		Collision->IgnoreActorWhenMoving(Source, true);
	}
	const FVector Destination = Target ? Target->GetActorLocation() : Request.TargetLocation;
	const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal();
	Movement->Velocity = Direction * Movement->InitialSpeed;
}

void ASkillLabFireballProjectile::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	if (OtherActor && OtherActor != Source)
	{
		ResolveImpact(OtherActor);
	}
}

void ASkillLabFireballProjectile::HandleHit(UPrimitiveComponent*, AActor* HitActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (HitActor && HitActor != Source)
	{
		ResolveImpact(HitActor);
	}
}

void ASkillLabFireballProjectile::ResolveImpact(AActor* HitActor)
{
	if (bResolved)
	{
		return;
	}
	bResolved = true;
	if (USkillLabSkillRuntimeComponent* Skills = Source ? Source->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr)
	{
		Skills->ResolveFireballImpact(HitActor, Definition, Request, CastId);
	}
	Destroy();
}
