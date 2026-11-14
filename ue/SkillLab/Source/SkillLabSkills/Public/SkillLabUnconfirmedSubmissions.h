#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/ObjectKey.h"
#include "SkillLabSubmissionResults.h"
#include "SkillLabUnconfirmedSubmissions.generated.h"

class USkillLabAbilitySystemComponent;

// 未决身份只关联查询来源，不把保留记录本身当作生产或资源回执。
struct FSkillLabUnconfirmedSubmission
{
	FName SkillId;
	FObjectKey RuntimeIdentity;
	uint64 RuntimeGeneration = 0;
	uint64 CastId = 0;
	uint64 PointId = 0;
	uint64 TransactionId = 0;
	// None means that no authoritative producer record/query is connected yet.
	FName ResultSourceId;
	bool bAlreadyCommitted = false;
	FSkillLabSubmissionKey ProductionKey;
	bool bResolving = false, bResolved = false, bNotified = false;
	bool bReportedResourceUnknown = false;
	bool bReportedLostOwner = false;
};

struct SKILLLABSKILLS_API FSkillLabSubmissionConfirmation
{
	FSkillLabSubmissionKey Key;
	bool bActionSucceeded = false, bResourceResolved = false, bCooldownRecorded = false;
	bool bCostCommitted = false;
	float CooldownRemaining = 0.0f;
	FName Reason;
	uint64 ProductionVersion = 0, ResourceVersion = 0;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FSkillLabSubmissionConfirmed, const FSkillLabSubmissionConfirmation&);

UCLASS(Transient)
class SKILLLABSKILLS_API USkillLabUnconfirmedSubmissions : public UActorComponent
{
	GENERATED_BODY()
public:
	USkillLabUnconfirmedSubmissions();
	static USkillLabUnconfirmedSubmissions* FindOrCreate(AActor* Owner, USkillLabAbilitySystemComponent* ASC);
	void Record(const FSkillLabUnconfirmedSubmission& Record);
	bool BlocksSkill(FName SkillId) const;
	bool HasUnconfirmed() const;
	bool HasLostResourceOwnerDiagnosis() const { return bHasLostResourceOwnerDiagnosis; }
	// 同一 ASC 的已确认冷却只有此处保存截止时间，不随 Runtime 重建迁移或重启。
	float GetCooldownRemaining(FName SkillId) const;
	bool CanAcceptConfirmedCooldown(const USkillLabAbilitySystemComponent* ASC) const;
	bool StartConfirmedCooldown(FName SkillId, double EndTime);
	void ResetConfirmedCooldowns();
	const TArray<FSkillLabUnconfirmedSubmission>& GetRecords() const { return Records; }
	const FSkillLabSubmissionResults& GetProductionResults() const { return ProductionResults; }
	// 通知只提示查询；不能携带一个 bool 授权结算。定时重试同一查询不重执行动作。
	void ReconcileProductionResults();
	FSkillLabSubmissionConfirmed OnSubmissionConfirmed;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
private:
	friend class USkillLabSkillRuntimeComponent;
	friend class USkillLabSkillEffectExecutor;
	friend class USkillLabLuaSkillBehavior;
	friend class ASkillLabFireballProjectile;
	friend class FSkillLabProductionRecordsTest;
	friend class FSkillLabSubmissionReconciliationTest;
	FSkillLabProductionHandle BeginProduction(UObject* Runtime, uint64 Generation, uint64 Cast, uint64 Point,
		uint64 Transaction, int32 RequestId, const struct FSkillLabSkillDefinition& Definition, bool bAlreadyCommitted);
	void ReconcileRecord(int32 Index);
	void DeliverConfirmation(int32 Index);
	void ReportLostOwner();
	// 当前会话的反馈入口可以替换；生产身份和原 Cast 身份保持不变。
	TWeakObjectPtr<class USkillLabSkillRuntimeComponent> FeedbackRuntime;
	TMap<int32, FSkillLabSubmissionConfirmation> PendingFeedback;
	FSkillLabSubmissionResults ProductionResults;
	TWeakObjectPtr<USkillLabAbilitySystemComponent> ResourceOwner;
	TArray<FSkillLabUnconfirmedSubmission> Records;
	TSet<int32> ActiveRecordIndices;
	TMap<FName, double> CooldownEnd;
	bool bOwnerEnded = false, bHasLostResourceOwnerDiagnosis = false;
};
