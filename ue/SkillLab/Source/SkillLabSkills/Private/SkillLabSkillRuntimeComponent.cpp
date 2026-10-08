#include "SkillLabSkillRuntimeComponent.h"

#include "AbilitySystemGlobals.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "SkillLabSkillPresentationRouter.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabTargetingService.h"

USkillLabSkillRuntimeComponent::USkillLabSkillRuntimeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabSkillRuntimeComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeSkills();
}

void USkillLabSkillRuntimeComponent::InitializeSkills()
{
	if (bInitialized) return;
	AActor* Owner = GetOwner();
	ASC = Owner ? Owner->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!ASC) return;
	ASC->InitializeSkillLabAbilityActorInfo(Owner, Owner);
	PresentationRouter = NewObject<USkillLabSkillPresentationRouter>(this);
	if (PresentationRouter) PresentationRouter->Bind(this);
	UnitId = Owner ? Owner->GetUniqueID() : 0;
	TArray<FName> Ids;
	FSkillLabSkillDefinitionProvider::GetDefaultSkillIds(Ids);
	for (const FName Id : Ids)
	{
		FSkillLabSkillDefinition Def;
		if (FSkillLabSkillDefinitionProvider::Resolve(Id, Def) && Def.AbilityClass)
		{
			GrantedSkills.Add(Id, ASC->GiveAbility(FGameplayAbilitySpec(Def.AbilityClass, 1)));
		}
	}
	bInitialized = GrantedSkills.Num() > 0;
	if (bInitialized)
	{
		FSkillLabSkillResult R; R.bSuccess = true; R.SourceUnitId = UnitId;
		Emit(ESkillLabSkillEvent::Completed, R, TEXT("Initialized"));
	}
}

bool USkillLabSkillRuntimeComponent::IsAlive() const
{
	const USkillLabCombatAttributeSet* Attr = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return bInitialized && Attr && Attr->GetHealth() > 0.0f;
}

bool USkillLabSkillRuntimeComponent::IsEnemy(const USkillLabSkillRuntimeComponent* Other) const
{
	return Other && Other != this && TeamId != Other->TeamId;
}

float USkillLabSkillRuntimeComponent::GetCooldownRemaining(const FName SkillId) const
{
	const double* End = CooldownEnd.Find(SkillId);
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	return End ? FMath::Max(0.0, static_cast<float>(*End - Now)) : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetEnergyValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetEnergy() : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetMaxEnergyValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetMaxEnergy() : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetHealthValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetHealth() : 0.0f;
}

float USkillLabSkillRuntimeComponent::GetMaxHealthValue() const
{
	const USkillLabCombatAttributeSet* Attributes = ASC ? ASC->GetCombatAttributeSet() : nullptr;
	return Attributes ? Attributes->GetMaxHealth() : 0.0f;
}

void USkillLabSkillRuntimeComponent::GetGrantedSkillIds(TArray<FName>& OutIds) const
{
	GrantedSkills.GetKeys(OutIds);
	OutIds.Sort(FNameLexicalLess());
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::CastSkill(const FName SkillId, AActor* Target, const FVector TargetLocation)
{
	FSkillLabSkillRequest Request;
	Request.SkillId = SkillId;
	Request.Caster = GetOwner();
	Request.Target = Target;
	Request.TargetLocation = TargetLocation;
	Request.bHasLocation = true;
	return SubmitSkillRequest(Request);
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::Reject(const FSkillLabSkillRequest& Request, const FName Reason)
{
	FSkillLabSkillResult R;
	R.SkillId = Request.SkillId; R.RequestId = Request.RequestId; R.SourceUnitId = UnitId; R.Reason = Reason;
	Emit(ESkillLabSkillEvent::Rejected, R, Reason);
	return R;
}

FSkillLabSkillResult USkillLabSkillRuntimeComponent::SubmitSkillRequest(const FSkillLabSkillRequest& InRequest)
{
	FSkillLabSkillRequest Request = InRequest;
	Request.RequestId = Request.RequestId > 0 ? Request.RequestId : NextRequestId++;
	FSkillLabSkillDefinition Def;
	if (!bInitialized || !ASC) return Reject(Request, TEXT("NotInitialized"));
	if (!FSkillLabSkillDefinitionProvider::Resolve(Request.SkillId, Def)) return Reject(Request, TEXT("SkillUnknown"));
	if (!IsAlive()) return Reject(Request, TEXT("CasterDead"));
	if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.SkillLab.Dead"), false))) return Reject(Request, TEXT("CasterDead"));
	if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.SkillLab.Stunned"), false))) return Reject(Request, TEXT("CasterStunned"));
	if (ASC->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.SkillLab.Silenced"), false))) return Reject(Request, TEXT("CasterSilenced"));
	if (!IsValid(Request.Caster)) return Reject(Request, TEXT("CasterRequired"));
	if (Request.Caster != GetOwner()) return Reject(Request, TEXT("CasterMismatch"));
	if (!GrantedSkills.Contains(Request.SkillId)) return Reject(Request, TEXT("SkillNotGranted"));
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (const double* End = CooldownEnd.Find(Request.SkillId); End && Now < *End) return Reject(Request, TEXT("Cooldown"));
	const USkillLabCombatAttributeSet* Attr = ASC->GetCombatAttributeSet();
	if (!Attr || Attr->GetEnergy() < Def.Cost) return Reject(Request, TEXT("InsufficientEnergy"));
	AActor* Target = nullptr; FName TargetReason;
	if (!ResolveAndValidateTarget(Request, Def, Target, TargetReason)) return Reject(Request, TargetReason);
	PendingRequest = Request; PendingDefinition = Def;
	PendingRequest.Target = Target;
	PendingAbility = nullptr;
	if (!ASC->TryActivateAbility(GrantedSkills[Request.SkillId]) || !PendingAbility)
	{
		PendingAbility = nullptr; return Reject(Request, TEXT("ActivationFailed"));
	}
	PendingCastId = NextCastId++;
	CooldownEnd.Add(Request.SkillId, Now + Def.Cooldown);
	ASC->ApplyModToAttribute(USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Additive, -Def.Cost);
	FSkillLabSkillResult Started; Started.bSuccess = true; Started.SkillId = Request.SkillId; Started.RequestId = Request.RequestId; Started.CastId = PendingCastId; Started.SourceUnitId = UnitId;
	Started.Effect = Def.Effect;
	Started.SourceActor = GetOwner(); Started.TargetActor = Target; Started.TargetLocation = Def.TargetShape == ESkillLabTargetShape::Ground ? Request.TargetLocation : Target->GetActorLocation();
	Emit(ESkillLabSkillEvent::Started, Started);
	ExecuteActivatedAbility();
	Started.bSuccess = true;
	Emit(ESkillLabSkillEvent::Completed, Started);
	return Started;
}

bool USkillLabSkillRuntimeComponent::ResolveAndValidateTarget(const FSkillLabSkillRequest& Request, const FSkillLabSkillDefinition& Def, AActor*& OutTarget, FName& OutReason) const
{
	return FSkillLabTargetingService::ValidateRequest(*this, Request, Def, OutTarget, OutReason);
}

void USkillLabSkillRuntimeComponent::NotifyAbilityActivated(USkillLabSkillGameplayAbility* Ability)
{
	PendingAbility = Ability;
}

void USkillLabSkillRuntimeComponent::ExecuteActivatedAbility()
{
	const FSkillLabSkillDefinition Def = PendingDefinition;
	FSkillLabSkillResult Result; Result.bSuccess = true; Result.SkillId = Def.SkillId; Result.RequestId = PendingRequest.RequestId; Result.CastId = PendingCastId; Result.SourceUnitId = UnitId;
	Result.Effect = Def.Effect;
	Result.SourceActor = GetOwner(); Result.TargetActor = PendingRequest.Target;
	Result.TargetLocation = Def.TargetShape == ESkillLabTargetShape::Ground ? PendingRequest.TargetLocation : (PendingRequest.Target ? PendingRequest.Target->GetActorLocation() : FVector::ZeroVector);
	Result.VisualRadius = Def.Radius;
	Emit(ESkillLabSkillEvent::TargetSelected, Result);
	if (Def.Execution == ESkillLabExecutionTemplate::Projectile)
	{
		const FVector Start = GetOwner()->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = GetOwner();
		SpawnParams.Instigator = Cast<APawn>(GetOwner());
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ASkillLabFireballProjectile* Projectile = GetWorld()->SpawnActor<ASkillLabFireballProjectile>(ASkillLabFireballProjectile::StaticClass(), Start, FRotator::ZeroRotator, SpawnParams);
		if (Projectile)
		{
			Projectile->Initialize(GetOwner(), PendingRequest.Target, Def, PendingRequest, PendingCastId);
		}
		else
		{
			// Keep the gameplay path usable in commandlet/headless worlds where actor spawning can be unavailable.
			ApplyToTarget(PendingRequest.Target, Def, Result, PendingCastId);
		}
	}
	else if (Def.Execution == ESkillLabExecutionTemplate::GroundArea) ApplyArea(PendingRequest.TargetLocation, Def, Result);
	else ApplyToTarget(PendingRequest.Target, Def, Result);
	PendingAbility = nullptr;
}

void USkillLabSkillRuntimeComponent::ResolveFireballImpact(AActor* HitActor, const FSkillLabSkillDefinition& Def, const FSkillLabSkillRequest& Request, const int32 CastId)
{
	FSkillLabSkillResult Result;
	Result.bSuccess = false; Result.SkillId = Def.SkillId; Result.RequestId = Request.RequestId; Result.CastId = CastId; Result.SourceUnitId = UnitId;
	Result.Effect = Def.Effect;
	Result.SourceActor = GetOwner(); Result.TargetActor = HitActor;
	if (HitActor)
	{
		const USkillLabSkillRuntimeComponent* HitSkills = HitActor->FindComponentByClass<USkillLabSkillRuntimeComponent>();
		if (!HitSkills || !IsEnemy(HitSkills))
		{
			Result.Reason = TEXT("HitTargetNotEnemy");
			Emit(ESkillLabSkillEvent::Hit, Result);
			return;
		}
		ApplyToTarget(HitActor, Def, Result, CastId);
	}
	else
	{
		Result.Reason = TEXT("ProjectileBlocked");
		Emit(ESkillLabSkillEvent::Hit, Result);
	}
}

void USkillLabSkillRuntimeComponent::ApplyToTarget(AActor* Target, const FSkillLabSkillDefinition& Def, FSkillLabSkillResult& Result, const int32 CastId)
{
	USkillLabSkillRuntimeComponent* TargetComp = Target ? Target->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
	USkillLabAbilitySystemComponent* TargetASC = Target ? Target->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!TargetComp || !TargetASC || !TargetComp->IsAlive()) { Result.bSuccess = false; Result.Reason = TEXT("HitTargetInvalid"); Emit(ESkillLabSkillEvent::Hit, Result); return; }
	const USkillLabCombatAttributeSet* Before = TargetASC->GetCombatAttributeSet();
	Result.TargetUnitId = TargetComp->GetUnitId(); Result.HealthBefore = Before ? Before->GetHealth() : 0.0f; Result.RequestedAmount = Def.Magnitude;
	Result.Effect = Def.Effect;
	Result.TargetActor = Target;
	const bool bEffectApplied = Def.Effect == ESkillLabEffectKind::Healing
		? ASC->ApplyHealingToTarget(TargetASC, Def.Magnitude, GetOwner())
		: ASC->ResolveDamage(FSkillLabDamageRequest{ASC, TargetASC, Def.Magnitude, Def.PowerCoefficient, ESkillLabDamageType::Physical, GetOwner()}).bApplied;
	const USkillLabCombatAttributeSet* After = TargetASC->GetCombatAttributeSet();
	Result.HealthAfter = After ? After->GetHealth() : Result.HealthBefore;
	Result.ActualAmount = Def.Effect == ESkillLabEffectKind::Healing ? Result.HealthAfter - Result.HealthBefore : Result.HealthBefore - Result.HealthAfter;
	Result.bSuccess = bEffectApplied;
	if (!bEffectApplied) Result.Reason = TEXT("GameplayEffectFailed");
	Emit(ESkillLabSkillEvent::Hit, Result);
	if (bEffectApplied)
	{
		Emit(ESkillLabSkillEvent::EffectApplied, Result, Def.Effect == ESkillLabEffectKind::Healing ? TEXT("HealingApplied") : TEXT("DamageApplied"));
	if (Def.Effect == ESkillLabEffectKind::DamageOverTime) AddBurning(Target, Def, CastId != 0 ? CastId : PendingCastId);
	}
}

void USkillLabSkillRuntimeComponent::ApplyArea(const FVector& Center, const FSkillLabSkillDefinition& Def, FSkillLabSkillResult& Result)
{
	TSet<int32> HitUnits;
	TArray<AActor*> Targets;
	FSkillLabTargetingService::FindAreaTargets(*this, Center, Def, Targets);
	for (AActor* Candidate : Targets)
	{
		if (USkillLabSkillRuntimeComponent* Comp = Candidate->FindComponentByClass<USkillLabSkillRuntimeComponent>()) HitUnits.Add(Comp->GetUnitId());
		ApplyToTarget(Candidate, Def, Result);
	}
	FSkillLabSkillResult Summary = Result;
	Summary.TargetActor = nullptr;
	Summary.TargetUnitId = 0;
	Summary.ActualAmount = 0.0f;
	Summary.RequestedAmount = Def.Magnitude;
	Summary.bSuccess = HitUnits.Num() > 0;
	Emit(ESkillLabSkillEvent::Hit, Summary, HitUnits.Num() ? TEXT("AreaHit") : TEXT("AreaEmpty"));
}

void USkillLabSkillRuntimeComponent::AddBurning(AActor* Target, const FSkillLabSkillDefinition& Def, const int32 CastId)
{
	USkillLabAbilitySystemComponent* TargetASC = Target ? Target->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!TargetASC || !ASC) return;
	FSkillLabDamageRequest DamageRequest;
	DamageRequest.SourceASC = ASC;
	DamageRequest.TargetASC = TargetASC;
	DamageRequest.BaseDamage = Def.Magnitude;
	DamageRequest.PowerCoefficient = Def.PowerCoefficient;
	DamageRequest.SourceObject = GetOwner();
	if (!ASC->ApplyDamageOverTimeToTarget(DamageRequest, Def.Duration, Def.Period).bApplied) return;
	FSkillLabSkillResult R; R.bSuccess = true; R.SkillId = Def.SkillId; R.CastId = CastId; R.SourceUnitId = UnitId; R.TargetUnitId = Target->FindComponentByClass<USkillLabSkillRuntimeComponent>()->GetUnitId();
	R.TargetActor = Target;
	Emit(ESkillLabSkillEvent::BuffApplied, R, TEXT("BurningStarted"));
}

void USkillLabSkillRuntimeComponent::ResetSkills()
{
	CooldownEnd.Reset();
	if (ASC && ASC->GetCombatAttributeSet())
	{
		ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetHealthAttribute(), 100.0f);
		ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 100.0f);
	}
}

void USkillLabSkillRuntimeComponent::Emit(const ESkillLabSkillEvent EventType, const FSkillLabSkillResult& Result, const FName Reason)
{
	FSkillLabSkillResult EventResult = Result;
	EventResult.Event = EventType;
	if (!Reason.IsNone()) EventResult.Reason = Reason;
	OnSkillEvent.Broadcast(EventResult);
	OnNativeSkillEvent.Broadcast(EventResult);
	UE_LOG(LogTemp, Log, TEXT("[SkillLabSkills] Event=%d Skill=%s Request=%d Cast=%d Source=%d Target=%d Success=%s Reason=%s Amount=%.2f"),
		static_cast<int32>(EventType), *EventResult.SkillId.ToString(), EventResult.RequestId, EventResult.CastId,
		EventResult.SourceUnitId, EventResult.TargetUnitId, EventResult.bSuccess ? TEXT("true") : TEXT("false"),
		*EventResult.Reason.ToString(), EventResult.ActualAmount);
}
