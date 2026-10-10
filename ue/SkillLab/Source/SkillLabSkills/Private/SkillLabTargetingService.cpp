#include "SkillLabTargetingService.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillRuntimeComponent.h"

namespace
{
	FName RelationFailure(const ESkillLabTargetRelation Relation)
	{
		switch (Relation)
		{
		case ESkillLabTargetRelation::Self: return TEXT("TargetNotSelf");
		case ESkillLabTargetRelation::Enemy: return TEXT("TargetNotEnemy");
		case ESkillLabTargetRelation::Ally: return TEXT("TargetNotAlly");
		default: return TEXT("TargetRelationRejected");
		}
	}
}

bool FSkillLabTargetingService::MatchesRelation(const USkillLabSkillRuntimeComponent& Source,
	const USkillLabSkillRuntimeComponent& Target, const ESkillLabTargetRelation Relation)
{
	switch (Relation)
	{
	case ESkillLabTargetRelation::Self: return &Source == &Target;
	case ESkillLabTargetRelation::Enemy: return Source.IsEnemy(&Target);
	case ESkillLabTargetRelation::Ally: return !Source.IsEnemy(&Target);
	case ESkillLabTargetRelation::Any: return true;
	default: return false;
	}
}

bool FSkillLabTargetingService::ValidateUnitTarget(const USkillLabSkillRuntimeComponent& Source, AActor* Candidate,
	const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FName& OutReason, const bool bCheckCastRange)
{
	OutTarget = Candidate;
	OutReason = NAME_None;
	if (!IsValid(OutTarget) || OutTarget->IsActorBeingDestroyed())
	{
		OutReason = TEXT("TargetInvalid");
		return false;
	}
	const USkillLabSkillRuntimeComponent* TargetComponent = OutTarget->FindComponentByClass<USkillLabSkillRuntimeComponent>();
	if (!TargetComponent || !TargetComponent->IsInitialized() || !TargetComponent->IsAlive())
	{
		OutReason = TEXT("TargetUnavailable");
		return false;
	}
	if (!MatchesRelation(Source, *TargetComponent, Definition.Relation))
	{
		OutReason = RelationFailure(Definition.Relation);
		return false;
	}
	if (bCheckCastRange && (!Source.GetOwner() || FVector::Dist2D(Source.GetOwner()->GetActorLocation(), OutTarget->GetActorLocation()) > Definition.Range))
	{
		OutReason = TEXT("OutOfRange");
		return false;
	}
	return true;
}

bool FSkillLabTargetingService::ValidateRequest(const USkillLabSkillRuntimeComponent& Source, const FSkillLabSkillRequest& Request,
	const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FName& OutReason)
{
	OutTarget = Request.Target;
	OutReason = NAME_None;
	if (Definition.TargetShape == ESkillLabTargetShape::Ground)
	{
		OutTarget = nullptr;
		if (!Request.bHasLocation) { OutReason = TEXT("GroundLocationRequired"); return false; }
		if (Request.TargetLocation.ContainsNaN()) { OutReason = TEXT("GroundLocationInvalid"); return false; }
		if (!Source.GetOwner() || FVector::Dist2D(Source.GetOwner()->GetActorLocation(), Request.TargetLocation) > Definition.Range) { OutReason = TEXT("OutOfRange"); return false; }
		return true;
	}
	if (Definition.TargetShape == ESkillLabTargetShape::Self || (!OutTarget && Definition.Relation == ESkillLabTargetRelation::Self)) OutTarget = Source.GetOwner();
	return ValidateUnitTarget(Source, OutTarget, Definition, OutTarget, OutReason);
}

bool FSkillLabTargetingService::ResolveCursorUnitTarget(APlayerController& Controller,
	const USkillLabSkillRuntimeComponent& Source, const FSkillLabSkillDefinition& Definition,
	AActor*& OutTarget, FVector& OutLocation, FName& OutReason)
{
	OutTarget = nullptr;
	OutLocation = FVector::ZeroVector;
	OutReason = NAME_None;
	if (Definition.TargetShape == ESkillLabTargetShape::Self || Definition.Relation == ESkillLabTargetRelation::Self)
	{
		if (!ValidateUnitTarget(Source, Source.GetOwner(), Definition, OutTarget, OutReason)) return false;
		OutLocation = OutTarget ? OutTarget->GetActorLocation() : FVector::ZeroVector;
		return true;
	}

	FVector RayOrigin;
	FVector RayDirection;
	if (!Controller.DeprojectMousePositionToWorld(RayOrigin, RayDirection))
	{
		OutReason = TEXT("CursorProjectionFailed");
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SkillLabUnitTarget), true);
	FHitResult TargetHit;
	const FVector TraceEnd = RayOrigin + RayDirection * 50000.0f;
	if (!Controller.GetWorld() || !Controller.GetWorld()->LineTraceSingleByChannel(TargetHit, RayOrigin, TraceEnd, ECC_Pawn, QueryParams))
	{
		OutReason = TEXT("CursorUnitNotFound");
		return false;
	}

	AActor* HitActor = TargetHit.GetActor();
	if (HitActor == Source.GetOwner() && Definition.Relation == ESkillLabTargetRelation::Enemy)
	{
		QueryParams.AddIgnoredActor(Source.GetOwner());
		TargetHit = FHitResult();
		Controller.GetWorld()->LineTraceSingleByChannel(TargetHit, RayOrigin, TraceEnd, ECC_Pawn, QueryParams);
		HitActor = TargetHit.GetActor();
	}
	if (!ValidateUnitTarget(Source, HitActor, Definition, OutTarget, OutReason)) return false;
	OutLocation = TargetHit.ImpactPoint;
	return true;
}

void FSkillLabTargetingService::FindAreaTargets(const USkillLabSkillRuntimeComponent& Source, const FVector& Center,
	const FSkillLabSkillDefinition& Definition, TArray<AActor*>& OutTargets)
{
	OutTargets.Reset();
	if (!Source.GetWorld() || Center.ContainsNaN() || !FMath::IsFinite(Definition.Radius) || Definition.Radius <= 0.0f) return;
	TSet<int32> Seen;
	for (TActorIterator<AActor> It(Source.GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || FVector::Dist2D(Center, Candidate->GetActorLocation()) > Definition.Radius) continue;
		AActor* ValidatedTarget = nullptr;
		FName Reason;
		if (!ValidateUnitTarget(Source, Candidate, Definition, ValidatedTarget, Reason, false)) continue;
		USkillLabSkillRuntimeComponent* Component = ValidatedTarget->FindComponentByClass<USkillLabSkillRuntimeComponent>();
		if (Seen.Contains(Component->GetUnitId())) continue;
		Seen.Add(Component->GetUnitId());
		OutTargets.Add(Candidate);
	}
}
