#include "SkillLabTargetingService.h"

#include "EngineUtils.h"
#include "SkillLabSkillRuntimeComponent.h"

bool FSkillLabTargetingService::ValidateRequest(const USkillLabSkillRuntimeComponent& Source, const FSkillLabSkillRequest& Request,
	const FSkillLabSkillDefinition& Definition, AActor*& OutTarget, FName& OutReason)
{
	OutTarget = Request.Target;
	if (Definition.TargetShape == ESkillLabTargetShape::Ground)
	{
		if (!Request.bHasLocation) { OutReason = TEXT("GroundLocationRequired"); return false; }
		if (FVector::Dist2D(Source.GetOwner()->GetActorLocation(), Request.TargetLocation) > Definition.Range) { OutReason = TEXT("OutOfRange"); return false; }
		return true;
	}
	if (!OutTarget && Definition.Relation == ESkillLabTargetRelation::Self) OutTarget = Source.GetOwner();
	if (!OutTarget || OutTarget->IsActorBeingDestroyed()) { OutReason = TEXT("TargetInvalid"); return false; }
	const USkillLabSkillRuntimeComponent* TargetComponent = OutTarget->FindComponentByClass<USkillLabSkillRuntimeComponent>();
	if (!TargetComponent || !TargetComponent->IsInitialized() || !TargetComponent->IsAlive()) { OutReason = TEXT("TargetUnavailable"); return false; }
	const bool bEnemy = Source.IsEnemy(TargetComponent);
	if (Definition.Relation == ESkillLabTargetRelation::Self && TargetComponent != &Source) { OutReason = TEXT("TargetNotSelf"); return false; }
	if (Definition.Relation == ESkillLabTargetRelation::Enemy && !bEnemy) { OutReason = TEXT("TargetNotEnemy"); return false; }
	if (Definition.Relation == ESkillLabTargetRelation::Ally && bEnemy) { OutReason = TEXT("TargetNotAlly"); return false; }
	if (FVector::Dist2D(Source.GetOwner()->GetActorLocation(), OutTarget->GetActorLocation()) > Definition.Range) { OutReason = TEXT("OutOfRange"); return false; }
	return true;
}

void FSkillLabTargetingService::FindAreaTargets(const USkillLabSkillRuntimeComponent& Source, const FVector& Center,
	const FSkillLabSkillDefinition& Definition, TArray<AActor*>& OutTargets)
{
	OutTargets.Reset();
	if (!Source.GetWorld()) return;
	TSet<int32> Seen;
	for (TActorIterator<AActor> It(Source.GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || FVector::Dist2D(Center, Candidate->GetActorLocation()) > Definition.Radius) continue;
		USkillLabSkillRuntimeComponent* Component = Candidate->FindComponentByClass<USkillLabSkillRuntimeComponent>();
		if (!Component || !Component->IsAlive() || !Source.IsEnemy(Component) || Seen.Contains(Component->GetUnitId())) continue;
		Seen.Add(Component->GetUnitId());
		OutTargets.Add(Candidate);
	}
}
