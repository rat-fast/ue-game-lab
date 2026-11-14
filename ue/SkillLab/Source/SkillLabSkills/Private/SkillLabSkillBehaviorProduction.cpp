#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabTargetingService.h"
#include "SkillLabFireballProjectile.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

bool USkillLabLuaSkillBehavior::ExecuteDeclaredAction(FPointExecution& Point)
{
 const auto Production = Point.Production;
 const int32 PointId = Point.Declaration.Id;
 AActor* Selected = Point.Declaration.TargetBinding == ESkillLabSubmissionTargetBinding::Source ? Runtime->GetOwner() : Request.Target.Get();
 if (Definition.Execution == ESkillLabExecutionTemplate::Projectile)
 {
  const auto Start = GetSkillLabProjectileAimPoint(Runtime->GetOwner());
  auto* Created = GetWorld() ? GetWorld()->SpawnActorDeferred<ASkillLabFireballProjectile>(
   ASkillLabFireballProjectile::StaticClass(), FTransform(FRotator::ZeroRotator, Start), Runtime->GetOwner(),
   Cast<APawn>(Runtime->GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn) : nullptr;
  if (!Created)
  {
   if (Production.Owner.IsValid() && Production.Owner->CanAcceptConfirmedCooldown(Production.Owner->ResourceOwner.Get()))
    Production.Owner->ProductionResults.Confirm(Production.Key, false, TEXT("ProjectileSpawnFailed"));
   Point.Result.Reason = TEXT("ProjectileSpawnFailed");
   Point.State = EPointState::Resolved;
   return false;
  }
  Point.Projectile = Created;
  Point.State = EPointState::AwaitingAction;
  Point.Result.Reason = TEXT("ProjectilePending");
  Created->OnResolved.BindWeakLambda(this, [this, PointId, Created](AActor* Target, FName Reason)
  { OnProjectileResolved(PointId, Created, Target, Reason); });
  Created->Initialize(Runtime->GetOwner(), Selected, Definition, Request, CastId, Definition.ProjectileParameters, Production);
  Created->FinishSpawning(FTransform(FRotator::ZeroRotator, Start));
  const bool bDelivered = Created->ConfirmDelivery();
  if (!bDelivered) Point.State = EPointState::Resolved;
  return bDelivered;
 }
 if (Definition.Execution == ESkillLabExecutionTemplate::GroundArea)
 {
  TArray<AActor*> Targets;
  const bool bQueryComplete = FSkillLabTargetingService::FindAreaTargets(*Runtime, Request.TargetLocation, Definition, Targets);
  const auto Children = Production.Owner.IsValid() ? Production.Owner->ProductionResults.PlanArea(Production.Key, Targets.Num(), bQueryComplete) : TArray<FSkillLabSubmissionKey>();
  bool bAnyApplied = false;
  for (int32 Index = 0; Index < Targets.Num(); ++Index)
  {
   if (!IsCurrent()) break;
   const auto Child = Children.IsValidIndex(Index) ? FSkillLabProductionHandle{Production.Owner, Children[Index]} : FSkillLabProductionHandle();
   const auto Applied = ApplyEffect(Targets[Index], Child);
   bAnyApplied |= Applied.bEffectApplied;
   Point.Result.ActualAmount += Applied.ActualAmount;
   if (!Applied.bEffectApplied && Point.Result.Reason.IsNone()) Point.Result.Reason = Applied.Reason;
  }
  Point.Result.bEffectApplied = bAnyApplied;
  Point.Result.Reason = Targets.Num() > 0 ? (bAnyApplied ? FName(TEXT("AreaHit")) : Point.Result.Reason) : FName(TEXT("AreaEmpty"));
  if (bQueryComplete && Targets.IsEmpty()) ReportMiss(TEXT("AreaEmpty"));
  Point.State = EPointState::Resolved;
  return (bQueryComplete && Targets.IsEmpty()) || bAnyApplied;
 }
 Point.Result = ApplyEffect(Selected, Production);
 Point.State = EPointState::Resolved;
 return Point.Result.bEffectApplied;
}
void USkillLabLuaSkillBehavior::OnProjectileResolved(const int32 PointId, ASkillLabFireballProjectile* Action, AActor* Target, const FName Reason)
{
 auto* Point = Points.Find(PointId);
 if (!Point || Point->Projectile.Get() != Action || !IsCurrent() || Runtime->IsSubmissionIndeterminate(this, CastId)
  || Point->State != EPointState::AwaitingAction) return;
 Point->Result = Action->GetImpactResult();
 Point->State = EPointState::Resolved;
 LastPointId = PointId; LastResponse = Point->Result;
 // 旧 Projectile 脚本无需主动 Finish，动作结束收束原等待；仍检查整组点。
 bFinishRequested = true;
 TryFinish();
}
