#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabFireballProjectile.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Misc/ScopeExit.h"
#include "UObject/StrongObjectPtr.h"

void USkillLabLuaSkillBehavior::EnsurePoints()
{
 if (!Points.IsEmpty()) return;
 for (const auto& Declaration : Definition.SubmissionPoints)
 {
  FPointExecution Point; Point.Declaration = Declaration;
  Point.Result.SkillId = Definition.SkillId; Point.Result.CastId = CastId;
  Point.Result.CastToken = GetToken(); Point.Result.RequestId = Request.RequestId;
  Point.Result.SourceActor = Request.Caster; Point.Result.SubmissionPointId = Declaration.Id;
  Points.Add(Declaration.Id, MoveTemp(Point));
 }
}
FSkillLabCastExecutionToken USkillLabLuaSkillBehavior::GetToken() const
{
 FSkillLabCastExecutionToken Token; Token.Runtime = Runtime; Token.RuntimeGeneration = RuntimeGeneration; Token.CastId = CastId;
 return Token;
}
void USkillLabLuaSkillBehavior::Execute()
{
 TStrongObjectPtr<USkillLabLuaSkillBehavior> Guard(this);
 EnsurePoints(); bReleased = true;
 TArray<int32> Ids;
 for (const auto& Declaration : Definition.SubmissionPoints) Ids.Add(Declaration.Id);
 {
  TGuardValue<bool> Dispatch(bDispatching, true);
  for (int32 Id : Ids)
  {
   if (!IsCurrent()) return;
   auto& Point = Points.FindChecked(Id);
   if (Point.Declaration.Trigger == ESkillLabSubmissionTrigger::Release)
    RequestSubmissionPoint(GetToken(), Id, ESkillLabSubmissionTrigger::Release);
   else if (Point.Declaration.Trigger == ESkillLabSubmissionTrigger::Timer)
   {
    const auto Token = GetToken();
    if (Point.Declaration.DelaySeconds <= 0.0f) RequestSubmissionPoint(Token, Id, ESkillLabSubmissionTrigger::Timer);
    else if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(Point.Timer, FTimerDelegate::CreateWeakLambda(this, [this, Token, Id]()
    { RequestSubmissionPoint(Token, Id, ESkillLabSubmissionTrigger::Timer); }), Point.Declaration.DelaySeconds, false);
   }
  }
  if (IsCurrent()) RunScript();
 }
 TryFinish();
}
bool USkillLabLuaSkillBehavior::RequestDeclaredAction(const int32 PointId)
{
 return RequestSubmissionPoint(GetToken(), PointId, ESkillLabSubmissionTrigger::Script);
}
bool USkillLabLuaSkillBehavior::RequestDeclaredEffect(const int32 PointId)
{
 return RequestSubmissionPoint(GetToken(), PointId, ESkillLabSubmissionTrigger::Script);
}
bool USkillLabLuaSkillBehavior::RequestSubmissionPoint(const FSkillLabCastExecutionToken& Token, int32 PointId,
 const ESkillLabSubmissionTrigger Trigger, const FGameplayTag EventTag)
{
 TStrongObjectPtr<USkillLabLuaSkillBehavior> Guard(this);
 EnsurePoints();
 LastResponse = {}; LastResponse.CastToken = GetToken(); LastResponse.CastId = CastId;
 if (!(Token == GetToken()) || !IsCurrent() || !bReleased || bFinished)
 { LastResponse.Reason = TEXT("CastNotActive"); return false; }
 if (PointId == 0 && !Definition.bHasExplicitSubmissionPoints && Points.Num() == 1) PointId = 1;
 auto* Point = Points.Find(PointId);
 if (!Point) { LastPointId = 0; LastResponse.Reason = TEXT("SubmissionPointUnknown"); return false; }
 LastPointId = PointId; LastResponse = Point->Result;
 if (Point->Declaration.Trigger != Trigger || (Trigger == ESkillLabSubmissionTrigger::GameplayEvent && Point->Declaration.EventTag != EventTag))
 { LastResponse.Reason = TEXT("SubmissionTriggerMismatch"); return false; }
 if (Point->State == EPointState::Executing || Point->State == EPointState::AwaitingAction || Point->State == EPointState::Unconfirmed
  || Point->State == EPointState::Cancelled || (Point->State == EPointState::Resolved && Point->Result.bSuccess))
 { LastResponse.Reason = Point->State == EPointState::Unconfirmed ? TEXT("SubmissionUnconfirmed") : TEXT("SubmissionPointUnavailable"); return false; }
 FName Reason;
 if (!Runtime->BeginBehaviorSubmission(this, CastId, static_cast<uint64>(PointId), Reason))
 {
  Point->Result.bSuccess = false;
  Point->Result.Reason = Reason;
  Point->State = Reason == TEXT("SubmissionUnconfirmed") ? EPointState::Unconfirmed : EPointState::Resolved;
  LastResponse = Point->Result;
  if (!bDispatching) TryFinish();
  return false;
 }
 ON_SCOPE_EXIT { if (Runtime) Runtime->AbortBehaviorSubmission(this, CastId, static_cast<uint64>(PointId)); };
 Point->State = EPointState::Executing;
 Point->Production = Runtime->GetProductionHandle(this, CastId);
 Point->Result.SubmissionKey = Point->Production.Key;
 const bool bSucceeded = ExecuteDeclaredAction(*Point);
 bool bIndeterminate = false;
 FName DeliveryFailure;
 const bool bResolved = Runtime->ResolveBehaviorSubmission(this, CastId, static_cast<uint64>(PointId), bSucceeded, &bIndeterminate, &DeliveryFailure);
 if (!DeliveryFailure.IsNone())
 { Point->Result.Reason = DeliveryFailure; Point->State = EPointState::Cancelled; }
 else if (bIndeterminate)
 { Point->Result.Reason = TEXT("SubmissionUnconfirmed"); Point->State = EPointState::Unconfirmed; }
 else if (Point->State != EPointState::AwaitingAction) Point->State = EPointState::Resolved;
 Point->Result.bSuccess = bResolved && bSucceeded;
 LastResponse = Point->Result;
 if (!bDispatching) TryFinish();
 return bResolved && bSucceeded;
}
void USkillLabLuaSkillBehavior::ReceiveEvent(const FGameplayTag EventTag)
{
 if (!IsCurrent()) return;
 {
  TGuardValue<bool> Dispatch(bDispatching, true);
  for (const auto& Declaration : Definition.SubmissionPoints)
   if (Declaration.Trigger == ESkillLabSubmissionTrigger::GameplayEvent && Declaration.EventTag == EventTag)
    RequestSubmissionPoint(GetToken(), Declaration.Id, ESkillLabSubmissionTrigger::GameplayEvent, EventTag);
  if (EventTag == Definition.CompletionEventTag) bFinishRequested = true;
 }
 TryFinish();
}
const FSkillLabSkillResult* USkillLabLuaSkillBehavior::GetPointResult(const int32 Id) const
{ const auto* Point = Points.Find(Id); return Point ? &Point->Result : nullptr; }
FSkillLabProductionHandle USkillLabLuaSkillBehavior::GetPointProduction(const int32 Id) const
{ const auto* Point = Points.Find(Id); return Point ? Point->Production : FSkillLabProductionHandle(); }
ASkillLabFireballProjectile* USkillLabLuaSkillBehavior::GetPointProjectile(const int32 Id) const
{ const auto* Point = Points.Find(Id); return Point ? Point->Projectile.Get() : nullptr; }
bool USkillLabLuaSkillBehavior::IsPointPending(const int32 Id) const
{ const auto* P = Points.Find(Id); return P && (P->State == EPointState::Executing || P->State == EPointState::AwaitingAction || P->State == EPointState::Unconfirmed); }
bool USkillLabLuaSkillBehavior::IsPointCompleted(const int32 Id) const
{ const auto* P = Points.Find(Id); return P && P->State == EPointState::Resolved; }
bool USkillLabLuaSkillBehavior::AllPointsResolved() const
{
 if (Points.IsEmpty()) return false;
 for (const auto& Pair : Points) if (Pair.Value.State != EPointState::Resolved) return false;
 return true;
}
bool USkillLabLuaSkillBehavior::HasLegalWait() const
{
 if (Definition.CompletionEventTag.IsValid() || ((bFinishRequested || Definition.CompletionMode == ESkillLabCompletionMode::AllSubmissionPointsResolved) && AllPointsResolved())) return true;
 for (const auto& Pair : Points)
 {
  const auto& P = Pair.Value;
  if (P.State == EPointState::Executing || P.State == EPointState::AwaitingAction || P.State == EPointState::Unconfirmed) return true;
  if (P.State == EPointState::Waiting && (P.Declaration.Trigger == ESkillLabSubmissionTrigger::Timer || P.Declaration.Trigger == ESkillLabSubmissionTrigger::GameplayEvent)) return true;
 }
 return false;
}
bool USkillLabLuaSkillBehavior::RequestFinish()
{
 if (!IsCurrent() || bFinished) return false;
 bFinishRequested = true;
 TryFinish();
 return bFinished;
}
void USkillLabLuaSkillBehavior::TryFinish()
{
 if (bDispatching || bFinished || !IsCurrent() || !AllPointsResolved()) return;
 if (!bFinishRequested && Definition.CompletionMode != ESkillLabCompletionMode::AllSubmissionPointsResolved) return;
 bFinished = true;
 bool bAnyAccepted = false;
 for (const auto& Pair : Points) bAnyAccepted |= Pair.Value.Result.bSuccess;
 if (!bAnyAccepted)
 {
  Complete(LastResponse.Reason.IsNone() ? FName(TEXT("SubmissionPointsFailed")) : LastResponse.Reason);
  return;
 }
 Complete(LastResponse.Reason == TEXT("AreaEmpty") ? FName(TEXT("AreaEmpty"))
  : (Definition.Effect == ESkillLabEffectKind::Healing ? FName(TEXT("HealingApplied"))
  : Definition.Effect == ESkillLabEffectKind::DamageOverTime ? FName(TEXT("BurningStarted")) : FName(TEXT("EffectApplied"))));
}
void USkillLabLuaSkillBehavior::Cleanup()
{
 for (auto& Pair : Points)
 {
  auto& P = Pair.Value;
  if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(P.Timer);
  if (auto* Projectile = P.Projectile.Get())
  {
   Projectile->OnResolved.Unbind();
   if (!Projectile->IsDeliveryConfirmed() && !Projectile->IsActorBeingDestroyed()) Projectile->Destroy();
  }
  if (P.State == EPointState::Waiting) P.State = EPointState::Cancelled;
 }
}
