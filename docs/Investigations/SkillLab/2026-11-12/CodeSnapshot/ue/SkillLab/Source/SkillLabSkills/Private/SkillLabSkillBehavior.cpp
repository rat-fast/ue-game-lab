#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillRuntimeComponent.h"

UWorld* USkillLabSkillBehavior::GetWorld() const { return Runtime ? Runtime->GetWorld() : nullptr; }
void USkillLabSkillBehavior::Initialize(USkillLabSkillRuntimeComponent* InRuntime,
	const FSkillLabSkillDefinition& InDefinition, const FSkillLabSkillRequest& InRequest, const int32 InCastId,
	const uint64 InRuntimeGeneration)
{
	Runtime = InRuntime; Definition = InDefinition; Request = InRequest; CastId = InCastId; RuntimeGeneration = InRuntimeGeneration;
}
bool USkillLabSkillBehavior::IsCurrent() const { return Runtime && Runtime->IsBehaviorCurrent(this, CastId); }
FSkillLabSkillResult USkillLabSkillBehavior::ApplyEffect(AActor* Target, const FSkillLabProductionHandle& Production)
{
	return IsCurrent() ? Runtime->ApplyBehaviorEffect(this, CastId, Target, Definition, Production) : FSkillLabSkillResult();
}
void USkillLabSkillBehavior::ReportMiss(const FName Reason, AActor* Target)
{
	if (IsCurrent()) Runtime->ReportBehaviorMiss(this, CastId, Target, Reason);
}
void USkillLabSkillBehavior::Complete(const FName Reason)
{
	if (IsCurrent()) Runtime->CompleteBehavior(this, CastId, Reason);
}
void USkillLabSkillBehavior::Fail(const FName Reason)
{
	if (IsCurrent()) Runtime->CancelBehavior(this, CastId, Reason);
}

