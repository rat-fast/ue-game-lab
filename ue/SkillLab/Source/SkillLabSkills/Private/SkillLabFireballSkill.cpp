#include "SkillLabFireballSkill.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillBehaviorRegistry.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

namespace
{
	void Configure(FSkillLabSkillDefinition& Definition)
	{
		Definition.TargetShape = ESkillLabTargetShape::Unit;
		Definition.Relation = ESkillLabTargetRelation::Enemy;
		Definition.Execution = ESkillLabExecutionTemplate::Projectile;
		Definition.Effect = ESkillLabEffectKind::Damage;
		Definition.Range = 1200.0f;
		Definition.AbilityClass = USkillLabSkillGameplayAbility::StaticClass();
		Definition.BehaviorClass = USkillLabFireballBehavior::StaticClass();
		Definition.ProjectileParameters.Speed = 1250.0f;
		Definition.ProjectileParameters.CollisionRadius = 18.0f;
		Definition.ProjectileParameters.Lifetime = 5.0f;
		Definition.ProjectileParameters.bInterceptOtherUnits = true;
		Definition.ProjectileParameters.VisualScale = 0.62f;
	}
}

void USkillLabFireballBehavior::Execute()
{
	if (!IsCurrent() || !GetWorld() || !Runtime) return;
	const FVector Start = GetSkillLabProjectileAimPoint(Runtime->GetOwner());
	Projectile = GetWorld()->SpawnActorDeferred<ASkillLabFireballProjectile>(ASkillLabFireballProjectile::StaticClass(), FTransform(FRotator::ZeroRotator, Start), Runtime->GetOwner(), Cast<APawn>(Runtime->GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		ReportMiss(TEXT("ProjectileSpawnFailed"));
		Complete(TEXT("ProjectileSpawnFailed"));
		return;
	}
	Projectile->OnResolved.BindUObject(this, &USkillLabFireballBehavior::OnProjectileResolved);
	Projectile->Initialize(Runtime->GetOwner(), Request.Target, Definition, Request, CastId, Definition.ProjectileParameters);
	Projectile->FinishSpawning(FTransform(FRotator::ZeroRotator, Start));
}

void USkillLabFireballBehavior::OnProjectileResolved(AActor* HitTarget, const FName Reason)
{
	if (!IsCurrent()) return;
	if (Reason == TEXT("Hit") || Reason == TEXT("Blocked"))
	{
		if (HitTarget) ApplyEffect(HitTarget); else ReportMiss(TEXT("ProjectileBlocked"));
	}
	else ReportMiss(Reason);
	Complete(Reason);
}

void USkillLabFireballBehavior::Cleanup()
{
	if (Projectile)
	{
		Projectile->OnResolved.Unbind();
		if (!Projectile->IsActorBeingDestroyed()) Projectile->Destroy();
	}
	Projectile = nullptr;
}

FSkillLabSkillBehaviorRegistration RegisterSkillLabFireballBehavior()
{
	return {TEXT("Skill_Fireball"), &Configure};
}
