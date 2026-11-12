#include "SkillLabSkillEffectExecutor.h"
#include "SkillLabFireballProjectile.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "SkillLabSkillEffectEventAdapter.h"
#include "SkillLabAbilitySystemComponent.h"

FSkillLabSkillResult USkillLabSkillEffectExecutor::ApplySubmitted(ASkillLabFireballProjectile& Action, AActor* Target)
{
 auto Result = Action.Origin;
 Result.TargetActor = Target;
 Result.EffectOperationId = FName(*FString::Printf(TEXT("SubmittedHit_%s"), *Action.Delivery.Key.Id.ToString(EGuidFormats::Digits)));
 if (!Action.bDeliveryConfirmed || Action.bHitEffectConsumed || !Action.Delivery.Owner.IsValid())
 { Result.Reason = TEXT("DeliveryNotAvailable"); return Result; }
 const auto Receipt = Action.Delivery.Owner->GetProductionResults().Query(Action.Delivery.Key);
 if (Receipt.Kind != ESkillLabProductionKind::Projectile || Receipt.State != ESkillLabProductionState::ConfirmedAccepted)
 { Result.Reason = TEXT("DeliveryNotConfirmed"); return Result; }
 Action.bHitEffectConsumed = true;
 Result.bCostCommitted = Receipt.bAlreadyCommitted || Receipt.bCooldownRecorded;
 return ApplySelected(Action.SourceASC.Get(), Action.Source.Get(), Action.SourceTeam, Target, Action.Definition, Result,
  Action.Delivery, Action.EffectObserver.Get());
}
