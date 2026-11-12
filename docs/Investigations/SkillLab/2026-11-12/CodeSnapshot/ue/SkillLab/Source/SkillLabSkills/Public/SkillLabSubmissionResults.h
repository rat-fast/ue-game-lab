#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectKey.h"
#include "SkillLabSubmissionResults.generated.h"

class USkillLabUnconfirmedSubmissions;
class USkillLabSkillRuntimeComponent;
class USkillLabSkillEffectExecutor;
class USkillLabLuaSkillBehavior;
class ASkillLabFireballProjectile;
class UWorld;

enum class ESkillLabProductionState : uint8 { Unknown, InProgress, ConfirmedAccepted, ConfirmedRejected, Unavailable };
enum class ESkillLabProductionKind : uint8 { Effect, Area, Projectile };

// OperationId 在会话内单调分配；完整键使新 Runtime、ASC 或会话无法误认旧结果。
USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSubmissionKey
{
	GENERATED_BODY()
	// 原操作创建时生成；Blueprint 用此不透明值关联反馈，不解析对象地址或数字 Cast。
	UPROPERTY(BlueprintReadOnly) FGuid Id;
	UPROPERTY(BlueprintReadOnly) int32 RequestId = 0;
	UPROPERTY(BlueprintReadOnly) FName SkillId;
	FObjectKey Session, ResourceOwner, Runtime;
	uint64 Generation = 0, Cast = 0, Point = 0, Transaction = 0, Operation = 0;
	bool IsSet() const { return Operation != 0; }
	bool operator==(const FSkillLabSubmissionKey& Other) const;
};

// 通过动态委托/反射复制时也保留原生完整键；不解析或持有已销毁对象。
template<> struct TStructOpsTypeTraits<FSkillLabSubmissionKey> : TStructOpsTypeTraitsBase2<FSkillLabSubmissionKey>
{
	enum { WithCopy = true };
};

struct SKILLLABSKILLS_API FSkillLabProductionHandle
{
	TWeakObjectPtr<USkillLabUnconfirmedSubmissions> Owner;
	FSkillLabSubmissionKey Key;
};

struct SKILLLABSKILLS_API FSkillLabProductionResult
{
	FSkillLabSubmissionKey Key;
	ESkillLabProductionKind Kind = ESkillLabProductionKind::Effect;
	ESkillLabProductionState State = ESkillLabProductionState::Unknown;
	FName Reason;
	uint64 Version = 0, ParentOperation = 0;
	float Cost = 0.0f, Cooldown = 0.0f;
	bool bAlreadyCommitted = false, bAreaQueryComplete = false;
	bool bCooldownRecorded = false;
	TOptional<double> AcceptedAt;
	TWeakObjectPtr<UWorld> Clock;
	TArray<FSkillLabSubmissionKey> Children;
};

// 会话持有，查询无副作用。写入仅向实际生产适配器开放；脚本无确认入口。
class SKILLLABSKILLS_API FSkillLabSubmissionResults
{
public:
	FSkillLabProductionResult Query(const FSkillLabSubmissionKey& Key) const;
private:
	friend class USkillLabUnconfirmedSubmissions;
	friend class USkillLabSkillRuntimeComponent;
	friend class USkillLabSkillEffectExecutor;
	friend class USkillLabLuaSkillBehavior;
	friend class ASkillLabFireballProjectile;
	friend class FSkillLabProductionRecordsTest;
	FSkillLabSubmissionKey Begin(FSkillLabSubmissionKey Key, ESkillLabProductionKind Kind,
		float Cost, float Cooldown, bool bAlreadyCommitted, UWorld* Clock);
	TArray<FSkillLabSubmissionKey> PlanArea(const FSkillLabSubmissionKey& Key, int32 Count, bool bQueryComplete);
	bool Confirm(const FSkillLabSubmissionKey& Key, bool bAccepted, FName Reason, TOptional<double> AcceptedAt = {});
	void Aggregate(uint64 Operation);
	void EndSession();
	uint64 NextOperation = 1;
	TMap<uint64, FSkillLabProductionResult> Results;
};
