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
  FVector Start = GetSkillLabProjectileAimPoint(Runtime->GetOwner());
  FQuat SpatialRotation = FQuat::Identity;
  const auto* Spatial = Definition.SpatialDefinitions.FindByPredicate([](const auto& D) { return D.Object == ESkillLabSpatialObject::Projectile; });
  if (Spatial)
  {
   const FVector Destination = IsValid(Selected) ? GetSkillLabProjectileAimPoint(Selected) : Request.TargetLocation;
   FTransform Placement; FString Error;
   if (!FSkillLabSpatialTransform::Resolve(*Spatial, Runtime->GetOwner(), Selected, Request.RelatedActor.Get(), Request.TargetLocation,
    Destination - Start, Placement, Error))
   {
    UE_LOG(LogTemp, Error, TEXT("[SkillLabSpatial] Skill=%s Object=%s: %s"), *Definition.SkillId.ToString(), *Spatial->Name.ToString(), *Error);
    if (Production.Owner.IsValid() && Production.Owner->CanAcceptConfirmedCooldown(Production.Owner->ResourceOwner.Get()))
     Production.Owner->ProductionResults.Confirm(Production.Key, false, TEXT("ProjectileSpatialInvalid"));
    Point.Result.Reason = TEXT("ProjectileSpatialInvalid"); Point.State = EPointState::Resolved; return false;
   }
   Start = Placement.GetLocation();
   SpatialRotation = Placement.GetRotation();
   UE_LOG(LogTemp, Log, TEXT("[SkillLabSpatial] Skill=%s Object=%s Spawn=%s"), *Definition.SkillId.ToString(), *Spatial->Name.ToString(), *Start.ToString());
  }
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
  if (Spatial && Spatial->Direction != ESkillLabSpatialDirection::ExecutionDirection) Created->SetSpatialVisualDirection(SpatialRotation);
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
