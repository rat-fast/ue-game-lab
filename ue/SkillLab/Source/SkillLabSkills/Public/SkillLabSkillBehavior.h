#pragma once
#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillBehavior.generated.h"

class USkillLabSkillRuntimeComponent;
class ASkillLabFireballProjectile;

/** One instance per cast. Skill-specific steps live here, never in the scheduler. */
UCLASS(Abstract)
class SKILLLABSKILLS_API USkillLabSkillBehavior : public UObject
{
	GENERATED_BODY()
public:
	virtual UWorld* GetWorld() const override;
	void Initialize(USkillLabSkillRuntimeComponent* InRuntime, const FSkillLabSkillDefinition& InDefinition,
		const FSkillLabSkillRequest& InRequest, int32 InCastId, uint64 InRuntimeGeneration);
	virtual void Execute() PURE_VIRTUAL(USkillLabSkillBehavior::Execute, );
	virtual void ReceiveEvent(FGameplayTag EventTag) {}
	virtual void Cleanup() {}
protected:
	bool IsCurrent() const;
	FSkillLabSkillResult ApplyEffect(AActor* Target);
	void ReportMiss(FName Reason, AActor* Target = nullptr);
	void Complete(FName Reason = NAME_None);
	void Fail(FName Reason);
	UPROPERTY(Transient) TObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	UPROPERTY(Transient) FSkillLabSkillDefinition Definition;
	UPROPERTY(Transient) FSkillLabSkillRequest Request;
	int32 CastId = 0;
	uint64 RuntimeGeneration = 0;
	friend class USkillLabSkillRuntimeComponent;
};

/** Generic Lua composition behavior. Lua may request only the declared effect and finish. */
UCLASS()
class SKILLLABSKILLS_API USkillLabLuaSkillBehavior : public USkillLabSkillBehavior
{
	GENERATED_BODY()
public:
	virtual void Execute() override;
	virtual void Cleanup() override;
	/** Validates a script before a cast is accepted; no skill state is mutated. */
	static bool ValidateScript(const FSkillLabSkillDefinition& InDefinition, FName& OutReason);
	bool RequestDeclaredEffect();
	bool RequestDeclaredAction();
	bool RequestFinish();
	const FSkillLabSkillResult& GetEffectResult() const { return EffectResult; }
	bool IsActionPending() const { return bActionPending; }
	bool IsActionCompleted() const { return bActionCompleted; }

private:
	void RunScript();
	void OnProjectileResolved(AActor* Target, FName Reason);
	void SetActionResult(const FSkillLabSkillResult& Result, bool bResolved, bool bPending = false);
	bool bEffectRequested = false;
	bool bFinished = false;
	bool bActionRequested = false;
	bool bActionResolved = false;
	bool bActionPending = false;
	bool bActionCompleted = false;
	bool bActionSucceeded = false;
	FSkillLabSkillResult EffectResult;
	FSkillLabSkillResult ActionResult;
	UPROPERTY(Transient) TObjectPtr<ASkillLabFireballProjectile> Projectile;
};
