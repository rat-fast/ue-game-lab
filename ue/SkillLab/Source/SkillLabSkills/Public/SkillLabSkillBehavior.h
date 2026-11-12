#pragma once
#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSubmissionResults.h"
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
	FSkillLabSkillResult ApplyEffect(AActor* Target, const FSkillLabProductionHandle& Production = {});
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

/** 每 Cast 一张点表；Lua 只引用声明 id，不持有生产确认能力。 */
UCLASS()
class SKILLLABSKILLS_API USkillLabLuaSkillBehavior : public USkillLabSkillBehavior
{
	GENERATED_BODY()
public:
	virtual void Execute() override;
	virtual void Cleanup() override;
	virtual void ReceiveEvent(FGameplayTag EventTag) override;
	bool RequestDeclaredEffect(int32 PointId = 0);
	bool RequestDeclaredAction(int32 PointId = 0);
	bool RequestFinish();
	bool RequestSubmissionPoint(const FSkillLabCastExecutionToken& Token, int32 PointId,
		ESkillLabSubmissionTrigger Trigger, FGameplayTag EventTag = {});
	const FSkillLabSkillResult& GetEffectResult() const { return LastResponse; }
	bool IsActionPending() const { return IsPointPending(LastPointId); }
	bool IsActionCompleted() const { return IsPointCompleted(LastPointId); }
	const FSkillLabSkillResult* GetPointResult(int32 PointId) const;
	FSkillLabProductionHandle GetPointProduction(int32 PointId) const;
	ASkillLabFireballProjectile* GetPointProjectile(int32 PointId) const;
	bool IsPointPending(int32 PointId) const;
	bool IsPointCompleted(int32 PointId) const;
private:
	friend class FSkillLabCastGameplayEventTest;
	friend class FSkillLabProductionFixture;
	enum class EPointState : uint8 { Waiting, Executing, AwaitingAction, Resolved, Unconfirmed, Cancelled };
	struct FPointExecution
	{
		FSkillLabSubmissionPointDefinition Declaration;
		EPointState State = EPointState::Waiting;
		FSkillLabProductionHandle Production;
		FSkillLabSkillResult Result;
		TWeakObjectPtr<ASkillLabFireballProjectile> Projectile;
		FTimerHandle Timer;
	};
	void EnsurePoints();
	FSkillLabCastExecutionToken GetToken() const;
	void RunScript();
	bool ExecuteDeclaredAction(); // 既有失联回执 fixture 的真实生产调用适配。
	bool ExecuteDeclaredAction(FPointExecution& Point);
	void OnProjectileResolved(int32 PointId, ASkillLabFireballProjectile* Action, AActor* Target, FName Reason);
	bool AllPointsResolved() const;
	bool HasLegalWait() const;
	void TryFinish();
	TMap<int32, FPointExecution> Points;
	FSkillLabSkillResult LastResponse;
	int32 LastPointId = 0;
	bool bReleased = false;
	bool bFinished = false;
	bool bFinishRequested = false;
	bool bDispatching = false;
};
