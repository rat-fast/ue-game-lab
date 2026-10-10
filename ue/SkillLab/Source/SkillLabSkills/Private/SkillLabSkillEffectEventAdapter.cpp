#include "SkillLabSkillEffectEventAdapter.h"

#include "GameFramework/Actor.h"
#include "GameplayEffect.h"

void USkillLabSkillEffectEventAdapter::Track(USkillLabAbilitySystemComponent* TargetASC,
	const FActiveGameplayEffectHandle Handle, const FSkillLabSkillResult& Origin)
{
	if (!IsValid(TargetASC) || !Handle.IsValid() || !TargetASC->GetActiveGameplayEffect(Handle)) return;
	PruneListeners();
	bool bBound = false;
	for (const FListener& Listener : Listeners) if (Listener.TargetASC == TargetASC) bBound = true;
	if (!bBound)
	{
		FListener Listener;
		Listener.TargetASC = TargetASC;
		Listener.Damage = TargetASC->OnPeriodicDamageResolved.AddUObject(this, &USkillLabSkillEffectEventAdapter::HandleDamage);
		Listener.Removed = TargetASC->OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &USkillLabSkillEffectEventAdapter::HandleRemoval);
		Listeners.Add(Listener);
	}
	FTrackedEffect Tracking;
	Tracking.Origin = Origin;
	Tracking.Origin.EffectInstanceId = FName(*Handle.ToString());
	Tracking.Source = Origin.SourceActor;
	Tracking.Target = Origin.TargetActor;
	Tracking.TargetASC = TargetASC;
	// The observer must not extend the lifetime of either gameplay actor.
	Tracking.Origin.SourceActor = nullptr;
	Tracking.Origin.TargetActor = nullptr;
	Effects.Add(Handle, Tracking);
}

void USkillLabSkillEffectEventAdapter::HandleDamage(const FActiveGameplayEffectHandle Handle,
	const FSkillLabDamageResult& Damage)
{
	const FTrackedEffect* Tracking = Effects.Find(Handle);
	if (!Tracking || !Tracking->Target.IsValid()) return;
	FSkillLabSkillResult Result = Tracking->Origin;
	Result.bSuccess = Damage.bApplied;
	Result.Event = ESkillLabSkillEvent::DamageTick;
	// This is an effect tick, not a transition of whichever cast is now active.
	Result.Stage = ESkillLabCastStage::Completed;
	Result.SourceActor = Tracking->Source.Get();
	Result.TargetActor = Tracking->Target.Get();
	Result.TargetLocation = Result.TargetActor->GetActorLocation();
	Result.RequestedAmount = Damage.RawDamage;
	Result.ActualAmount = Damage.AppliedDamage;
	Result.HealthBefore = Damage.HealthBefore;
	Result.HealthAfter = Damage.HealthAfter;
	Result.Reason = Damage.AppliedDamage > 0.0f ? TEXT("PeriodicDamageApplied") : TEXT("NoDamageApplied");
	OnEffectEvent.Broadcast(Result);
}

void USkillLabSkillEffectEventAdapter::HandleRemoval(const FActiveGameplayEffect& Effect)
{
	FTrackedEffect Tracking;
	if (!Effects.RemoveAndCopyValue(Effect.Handle, Tracking)) return;
	FSkillLabSkillResult Result = Tracking.Origin;
	Result.Event = ESkillLabSkillEvent::BuffRemoved;
	Result.bSuccess = true;
	Result.SourceActor = Tracking.Source.Get();
	Result.TargetActor = Tracking.Target.Get();
	Result.Reason = TEXT("EffectRemoved");
	PruneListeners();
	OnEffectEvent.Broadcast(Result);
}

void USkillLabSkillEffectEventAdapter::PruneListeners()
{
	for (auto It = Effects.CreateIterator(); It; ++It)
		if (!It.Value().TargetASC.IsValid() || !It.Value().TargetASC->GetActiveGameplayEffect(It.Key())) It.RemoveCurrent();
	for (int32 Index = Listeners.Num() - 1; Index >= 0; --Index)
	{
		const FListener& Listener = Listeners[Index];
		bool bUsed = false;
		for (const auto& Pair : Effects) if (Pair.Value.TargetASC == Listener.TargetASC) bUsed = true;
		if (!bUsed || !Listener.TargetASC.IsValid())
		{
			if (Listener.TargetASC.IsValid())
			{
				Listener.TargetASC->OnPeriodicDamageResolved.Remove(Listener.Damage);
				Listener.TargetASC->OnAnyGameplayEffectRemovedDelegate().Remove(Listener.Removed);
			}
			Listeners.RemoveAt(Index);
		}
	}
}

void USkillLabSkillEffectEventAdapter::Clear(const bool bRemoveEffects)
{
	// Clear subscriptions first: removal can synchronously broadcast and mutate collections.
	for (const FListener& Listener : Listeners)
	{
		if (!Listener.TargetASC.IsValid()) continue;
		Listener.TargetASC->OnPeriodicDamageResolved.Remove(Listener.Damage);
		Listener.TargetASC->OnAnyGameplayEffectRemovedDelegate().Remove(Listener.Removed);
	}
	Listeners.Reset();
	TMap<FActiveGameplayEffectHandle, FTrackedEffect> Previous = MoveTemp(Effects);
	Effects.Reset();
	for (const auto& Pair : Previous)
	{
		if (bRemoveEffects && Pair.Value.TargetASC.IsValid()) Pair.Value.TargetASC->RemoveActiveGameplayEffect(Pair.Key);
		FSkillLabSkillResult Result = Pair.Value.Origin;
		Result.Event = ESkillLabSkillEvent::BuffRemoved;
		Result.bSuccess = true;
		Result.Reason = TEXT("EffectTrackingCleared");
		OnEffectEvent.Broadcast(Result);
	}
}
