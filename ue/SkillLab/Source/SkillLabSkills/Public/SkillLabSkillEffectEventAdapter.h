#pragma once

#include "CoreMinimal.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillEffectEventAdapter.generated.h"

/** Tracks effects independently of a cast's lifecycle; never applies or recalculates damage. */
UCLASS()
class SKILLLABSKILLS_API USkillLabSkillEffectEventAdapter : public UObject
{
	GENERATED_BODY()
public:
	FSkillLabNativeSkillEvent OnEffectEvent;
	void Track(USkillLabAbilitySystemComponent* TargetASC, FActiveGameplayEffectHandle Handle, const FSkillLabSkillResult& Origin);
	void Clear(bool bRemoveEffects);
	int32 GetTrackedEffectCount() const { return Effects.Num(); }

private:
	struct FTrackedEffect
	{
		FSkillLabSkillResult Origin;
		TWeakObjectPtr<AActor> Source;
		TWeakObjectPtr<AActor> Target;
		TWeakObjectPtr<USkillLabAbilitySystemComponent> TargetASC;
	};
	struct FListener
	{
		TWeakObjectPtr<USkillLabAbilitySystemComponent> TargetASC;
		FDelegateHandle Damage;
		FDelegateHandle Removed;
	};
	TMap<FActiveGameplayEffectHandle, FTrackedEffect> Effects;
	TArray<FListener> Listeners;
	void HandleDamage(FActiveGameplayEffectHandle Handle, const FSkillLabDamageResult& Damage);
	void HandleRemoval(const FActiveGameplayEffect& Effect);
	void PruneListeners();
};
