#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillEffectExecutor.generated.h"

class USkillLabAbilitySystemComponent;
class USkillLabSkillBehavior;
class USkillLabSkillEffectEventAdapter;
class USkillLabSkillRuntimeComponent;
class ISkillLabSkillCastContext;

/** Executes one already-selected skill effect and returns facts to the lifecycle owner. */
UCLASS()
class SKILLLABSKILLS_API USkillLabSkillEffectExecutor : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UObject* InContextOwner, ISkillLabSkillCastContext* InContext,
		USkillLabSkillEffectEventAdapter* InEffectEventAdapter);
	/** Clears borrowed context and callbacks before the owning runtime is torn down. */
	void Shutdown();
	FSkillLabSkillResult Apply(const USkillLabSkillBehavior* Behavior, int32 CastId,
		AActor* Target, const FSkillLabSkillDefinition& Definition, FSkillLabSkillResult Result) const;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> ContextOwner;
	ISkillLabSkillCastContext* Context = nullptr;

	UPROPERTY(Transient)
	TWeakObjectPtr<USkillLabSkillEffectEventAdapter> EffectEventAdapter;
};
