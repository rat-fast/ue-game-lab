#include "SkillLabFireballProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillEffectExecutor.h"
#include "SkillLabSkillEffectEventAdapter.h"

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
		ImpactResult = Origin;
		ImpactResult.SourceActor = Source.Get();
		ImpactResult.Reason = TEXT("Destroyed");
		if (bDeliveryConfirmed && OriginRuntime.IsValid() && Source.IsValid() && !Source->IsActorBeingDestroyed())
		{
			auto Result = ImpactResult; Result.Event = ESkillLabSkillEvent::Hit;
			OriginRuntime->ForwardEffectEvent(Result);
		}
		OnResolved.ExecuteIfBound(nullptr, TEXT("Destroyed"));
	}
	Super::EndPlay(EndPlayReason);
}

void ASkillLabFireballProjectile::Initialize(AActor* InSource, AActor* InTarget, const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, const int32 InCastId, const FSkillLabProjectileParameters& InParameters, const FSkillLabProductionHandle& InDelivery)
{
	if (bInitialized) return;
	Delivery = InDelivery;
	bDeliveryPending = Delivery.Key.IsSet();
	Source = InSource;
	SourceASC = InSource ? InSource->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	OriginRuntime = InSource ? InSource->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	if (OriginRuntime.IsValid())
	{
		SourceTeam = OriginRuntime->TeamId;
		EffectObserver = OriginRuntime->EffectEventAdapter.Get();
		Origin = OriginRuntime->MakeResult(OriginRuntime->FindExecutionByCastId(InCastId));
	}
	Origin.SubmissionKey = Delivery.Key;
	Origin.SubmissionPointId = static_cast<int64>(Delivery.Key.Point);
	Origin.SourceActor = nullptr;
	Origin.TargetActor = nullptr;
	Target = InTarget;
	Definition = InDefinition;
	Request = InRequest;
	Request.Caster = nullptr; Request.Target = nullptr;
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
	if (Source.IsValid())
	{
		Collision->IgnoreActorWhenMoving(Source.Get(), true);
	}
	const FVector Destination = Target.IsValid() ? GetSkillLabProjectileAimPoint(Target.Get()) : Request.TargetLocation;
	const FVector Direction = (Destination - GetActorLocation()).GetSafeNormal();
	Movement->Velocity = Direction * Movement->InitialSpeed;
	ActivateProjectile();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabProjectile] Spawned visible fireball Cast=%d Target=%s Distance=%.0f Speed=%.0f"),
		CastId, *GetNameSafe(Target.Get()), FVector::Dist(GetActorLocation(), Destination), Movement->InitialSpeed);
}

void ASkillLabFireballProjectile::ActivateProjectile()
{
	if (!bInitialized || !HasActorBegunPlay() || bDeliveryPending || bActivationStarted || IsActorBeingDestroyed())
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

bool ASkillLabFireballProjectile::ConfirmDelivery()
{
	if (!bDeliveryPending || !Delivery.Owner.IsValid()
		|| !Delivery.Owner->CanAcceptConfirmedCooldown(Delivery.Owner->ResourceOwner.Get())) return false;
	if (!bInitialized || !HasActorBegunPlay() || IsActorBeingDestroyed())
	{
		Delivery.Owner->ProductionResults.Confirm(Delivery.Key, false, TEXT("ProjectileNotDelivered"));
		return false;
	}
	const bool bRecorded = Delivery.Owner->ProductionResults.Confirm(Delivery.Key, true, TEXT("ProjectileDelivered"),
		GetWorld() ? TOptional<double>(GetWorld()->GetTimeSeconds()) : TOptional<double>());
	if (!bRecorded) return false;
	bDeliveryPending = false;
	bDeliveryConfirmed = true;
	ActivateProjectile();
	return true;
}

void ASkillLabFireballProjectile::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32, bool, const FHitResult&)
{
	if (bActivationStarted && OtherActor && OtherActor != Source.Get())
	{
		// 同来源的多个已交付动作可同时从同一发射位置出发。
		if (const auto* OtherProjectile = Cast<ASkillLabFireballProjectile>(OtherActor); OtherProjectile && OtherProjectile->Source == Source) return;
		// A non-intercepting projectile passes through units other than its selected target.
		const bool bUnitCollision = OtherComponent && OtherComponent->GetCollisionObjectType() == ECC_Pawn;
		if (!Parameters.bInterceptOtherUnits && bUnitCollision && OtherActor != Target.Get()) return;
		ResolveImpact(OtherActor, bUnitCollision || OtherActor == Target.Get() ? TEXT("Hit") : TEXT("Blocked"));
	}
}

void ASkillLabFireballProjectile::HandleHit(UPrimitiveComponent*, AActor* HitActor, UPrimitiveComponent*, FVector, const FHitResult&)
{
	if (bActivationStarted && HitActor && HitActor != Source.Get())
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
	ImpactResult = Origin;
	ImpactResult.SourceActor = Source.Get();
	ImpactResult.Reason = ResolveReason;
	ImpactResult.TargetActor = HitActor;
	if (ResolveReason == TEXT("Hit") && HitActor) ImpactResult = USkillLabSkillEffectExecutor::ApplySubmitted(*this, HitActor);
	if (OriginRuntime.IsValid() && IsValid(Source.Get()) && !Source->IsActorBeingDestroyed())
	{
		OriginRuntime->ForwardEffectEvent([&]() { auto R = ImpactResult; R.Event = ESkillLabSkillEvent::Hit; return R; }());
		if (ImpactResult.bEffectApplied)
			OriginRuntime->ForwardEffectEvent([&]() { auto R = ImpactResult; R.Event = ESkillLabSkillEvent::EffectApplied; return R; }());
	}
	OnResolved.ExecuteIfBound(HitActor, ResolveReason);
	Destroy();
}

void ASkillLabFireballProjectile::ExpireProjectile()
{
	ResolveImpact(nullptr, TEXT("Expired"));
}
