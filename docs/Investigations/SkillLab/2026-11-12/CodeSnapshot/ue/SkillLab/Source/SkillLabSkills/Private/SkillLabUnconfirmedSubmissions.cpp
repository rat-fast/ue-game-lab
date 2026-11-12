#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

USkillLabUnconfirmedSubmissions::USkillLabUnconfirmedSubmissions()
{
	PrimaryComponentTick.bCanEverTick = true;
}

USkillLabUnconfirmedSubmissions* USkillLabUnconfirmedSubmissions::FindOrCreate(AActor* Owner, USkillLabAbilitySystemComponent* ASC)
{
	if (!IsValid(Owner) || !IsValid(ASC)) return nullptr;
	TInlineComponentArray<USkillLabUnconfirmedSubmissions*> Components(Owner);
	for (USkillLabUnconfirmedSubmissions* Component : Components)
		if (Component->ResourceOwner.Get() == ASC) return Component;
	USkillLabUnconfirmedSubmissions* Component = NewObject<USkillLabUnconfirmedSubmissions>(Owner);
	if (!Component) return nullptr;
	Component->ResourceOwner = ASC;
	Owner->AddInstanceComponent(Component);
	Component->RegisterComponent();
	return Component;
}

void USkillLabUnconfirmedSubmissions::Record(const FSkillLabUnconfirmedSubmission& Record)
{
	for (const FSkillLabUnconfirmedSubmission& Existing : Records)
		if (Existing.RuntimeIdentity == Record.RuntimeIdentity && Existing.RuntimeGeneration == Record.RuntimeGeneration && Existing.CastId == Record.CastId
			&& Existing.PointId == Record.PointId && Existing.TransactionId == Record.TransactionId
			&& Existing.SkillId == Record.SkillId && Existing.ProductionKey == Record.ProductionKey) return;
	Records.Add(Record);
	UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] Retained Skill=%s Generation=%llu Cast=%llu Point=%llu Transaction=%llu AlreadyCommitted=%d"),
		*Record.SkillId.ToString(), Record.RuntimeGeneration, Record.CastId, Record.PointId, Record.TransactionId, Record.bAlreadyCommitted);
	if (bOwnerEnded || !ResourceOwner.IsValid() || ResourceOwner->IsBeingDestroyed()
		|| !IsValid(GetOwner()) || GetOwner()->IsActorBeingDestroyed()) ReportLostOwner();
}

bool USkillLabUnconfirmedSubmissions::BlocksSkill(const FName SkillId) const
{
	return Records.ContainsByPredicate([SkillId](const FSkillLabUnconfirmedSubmission& Record)
		{ return Record.SkillId == SkillId && !Record.bResolved; });
}

bool USkillLabUnconfirmedSubmissions::HasUnconfirmed() const
{
	return Records.ContainsByPredicate([](const FSkillLabUnconfirmedSubmission& Record) { return !Record.bResolved; });
}

void USkillLabUnconfirmedSubmissions::ReportLostOwner()
{
	for (int32 Index = 0; Index < Records.Num(); ++Index)
	{
		if (ReportedLostOwnerRecords.Contains(Index)) continue;
		if (Records[Index].bResolved) continue;
		ReportedLostOwnerRecords.Add(Index);
		const FSkillLabUnconfirmedSubmission& Record = Records[Index];
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabUnconfirmed] Session cannot confirm: resource owner or actor destroyed; Skill=%s Generation=%llu Cast=%llu Point=%llu Transaction=%llu"),
			*Record.SkillId.ToString(), Record.RuntimeGeneration, Record.CastId, Record.PointId, Record.TransactionId);
	}
}

float USkillLabUnconfirmedSubmissions::GetCooldownRemaining(const FName SkillId) const
{
	const double* End = CooldownEnd.Find(SkillId);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return End ? FMath::Max(0.0f, static_cast<float>(*End - Now)) : 0.0f;
}

bool USkillLabUnconfirmedSubmissions::CanAcceptConfirmedCooldown(const USkillLabAbilitySystemComponent* ASC) const
{
	return IsValid(this) && !bOwnerEnded && IsValid(GetOwner()) && !GetOwner()->IsActorBeingDestroyed()
		&& ResourceOwner.IsValid() && ResourceOwner.Get() == ASC && !ResourceOwner->IsBeingDestroyed() && !ResourceOwner->HasEndedEnergySession();
}

bool USkillLabUnconfirmedSubmissions::StartConfirmedCooldown(const FName SkillId, const double EndTime)
{
	if (!CanAcceptConfirmedCooldown(ResourceOwner.Get())) return false;
	CooldownEnd.Add(SkillId, EndTime);
	return true;
}

void USkillLabUnconfirmedSubmissions::ResetConfirmedCooldowns()
{
	if (!HasUnconfirmed()) CooldownEnd.Reset();
}

void USkillLabUnconfirmedSubmissions::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!ResourceOwner.IsValid() || ResourceOwner->IsBeingDestroyed() || ResourceOwner->HasEndedEnergySession()) { ProductionResults.EndSession(); ReportLostOwner(); }
	else ReconcileProductionResults();
}

void USkillLabUnconfirmedSubmissions::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bOwnerEnded = true;
	ProductionResults.EndSession();
	ReportLostOwner();
	Super::EndPlay(EndPlayReason);
}

void USkillLabUnconfirmedSubmissions::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	bOwnerEnded = true;
	ProductionResults.EndSession();
	ReportLostOwner();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
