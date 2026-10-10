#include "SkillLabSkillInputComponent.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillFeedbackActor.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabTargetingService.h"

USkillLabSkillInputComponent::USkillLabSkillInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = 0.016f;
}

void USkillLabSkillInputComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelSkill();
	Super::EndPlay(EndPlayReason);
}

bool USkillLabSkillInputComponent::ResolveCasterAndRuntime(USkillLabSkillRuntimeComponent*& OutRuntime) const
{
	OutRuntime = nullptr;
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	OutRuntime = Pawn ? Pawn->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	return OutRuntime != nullptr;
}

bool USkillLabSkillInputComponent::ResolveGroundLocation(FVector& OutLocation) const
{
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (!Controller || !Controller->GetWorld()) return false;
	FVector WorldOrigin, WorldDirection;
	if (!Controller->DeprojectMousePositionToWorld(WorldOrigin, WorldDirection)) return false;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SkillLabGroundAim), true);
	if (const AActor* Caster = Controller->GetPawn()) QueryParams.AddIgnoredActor(Caster);
	FHitResult GroundHit;
	const FVector TraceEnd = WorldOrigin + WorldDirection * 100000.0f;
	if (!Controller->GetWorld()->LineTraceSingleByChannel(GroundHit, WorldOrigin, TraceEnd, ECC_WorldStatic, QueryParams)
		|| FVector::DotProduct(GroundHit.ImpactNormal, FVector::UpVector) < 0.5f) return false;
	OutLocation = GroundHit.ImpactPoint;
	return FMath::IsFinite(OutLocation.X) && FMath::IsFinite(OutLocation.Y) && FMath::IsFinite(OutLocation.Z);
}

void USkillLabSkillInputComponent::BeginSkill(const FName SkillId)
{
	USkillLabSkillRuntimeComponent* Runtime = nullptr;
	if (!ResolveCasterAndRuntime(Runtime)) { UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Skill %s rejected: missing caster runtime"), *SkillId.ToString()); return; }
	FSkillLabSkillDefinition Definition;
	if (!FSkillLabSkillDefinitionProvider::Resolve(SkillId, Definition)) { UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Skill %s rejected: definition missing"), *SkillId.ToString()); return; }
	CancelSkill();
	if (Definition.TargetShape == ESkillLabTargetShape::Ground) { StartGroundPreview(Definition); return; }
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	AActor* Target = nullptr; FVector Location = FVector::ZeroVector; FName TargetReason;
	if (!Controller || !FSkillLabTargetingService::ResolveCursorUnitTarget(*Controller, *Runtime, Definition, Target, Location, TargetReason))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Skill %s rejected: %s"), *SkillId.ToString(), *TargetReason.ToString()); return;
	}
	const FSkillLabSkillResult Result = Runtime->CastSkill(SkillId, Target, Location);
	if (!Result.bSuccess) UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Skill=%s Target=%s Rejected=%s"), *SkillId.ToString(), *GetNameSafe(Target), *Result.Reason.ToString());
}

void USkillLabSkillInputComponent::StartGroundPreview(const FSkillLabSkillDefinition& Definition)
{
	FVector Location;
	if (!ResolveGroundLocation(Location)) { UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Skill %s could not project cursor to ground"), *Definition.SkillId.ToString()); return; }
	PendingSkillId = Definition.SkillId;
	bTargetingGround = true;
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	TargetingCaster = Controller ? Controller->GetPawn() : nullptr;
	SetComponentTickEnabled(true);
	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParameters; SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = World->SpawnActor<ASkillLabSkillFeedbackActor>(ASkillLabSkillFeedbackActor::StaticClass(), Location + FVector(0.0f, 0.0f, 4.0f), FRotator::ZeroRotator, SpawnParameters);
		if (PreviewActor) PreviewActor->InitializeShape(ESkillLabFeedbackShape::Ring, Definition.Radius, FColor::Orange, 3600.0f);
	}
	UE_LOG(LogTemp, Log, TEXT("[SkillLabTargeting] Ground skill %s targeting started; left click confirms, Escape cancels"), *Definition.SkillId.ToString());
}

void USkillLabSkillInputComponent::ConfirmSkill()
{
	if (!bTargetingGround || PendingSkillId.IsNone()) return;
	USkillLabSkillRuntimeComponent* Runtime = nullptr; FVector Location;
	if (ResolveCasterAndRuntime(Runtime) && Runtime->GetOwner() == TargetingCaster.Get() && ResolveGroundLocation(Location))
	{
		const FSkillLabSkillResult Result = Runtime->CastSkill(PendingSkillId, nullptr, Location);
		if (!Result.bSuccess) UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Ground confirm for %s rejected: %s"), *PendingSkillId.ToString(), *Result.Reason.ToString());
	}
	else UE_LOG(LogTemp, Warning, TEXT("[SkillLabTargeting] Ground confirm for %s failed: cursor did not hit ground"), *PendingSkillId.ToString());
	CancelSkill();
}

void USkillLabSkillInputComponent::CancelSkill()
{
	DestroyPreview(); PendingSkillId = NAME_None; bTargetingGround = false;
	TargetingCaster.Reset();
	SetComponentTickEnabled(false);
}

void USkillLabSkillInputComponent::DestroyPreview()
{
	if (PreviewActor) { PreviewActor->Destroy(); PreviewActor = nullptr; }
}

void USkillLabSkillInputComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bTargetingGround) return;
	USkillLabSkillRuntimeComponent* Runtime = nullptr;
	if (!ResolveCasterAndRuntime(Runtime) || Runtime->GetOwner() != TargetingCaster.Get()) { CancelSkill(); return; }
	if (!PreviewActor) return;
	FVector Location;
	const bool bHasLocation = ResolveGroundLocation(Location);
	PreviewActor->SetActorHiddenInGame(!bHasLocation);
	if (bHasLocation) PreviewActor->SetActorLocation(Location + FVector(0.0f, 0.0f, 4.0f));
}
