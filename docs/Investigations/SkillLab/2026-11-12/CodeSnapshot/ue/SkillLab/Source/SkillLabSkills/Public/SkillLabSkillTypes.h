#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillLabProjectileTypes.h"
#include "SkillLabSubmissionResults.h"
#include "SkillLabSkillTypes.generated.h"

class UGameplayAbility;
class USkillLabSkillRuntimeComponent;
class USkillLabSkillBehavior;
class UAnimMontage;
class UAnimSequence;

UENUM(BlueprintType)
enum class ESkillLabTargetShape : uint8 { Self, Unit, Ground };

UENUM(BlueprintType)
enum class ESkillLabTargetRelation : uint8 { Self, Ally, Enemy, Any };

UENUM(BlueprintType)
enum class ESkillLabEffectKind : uint8 { Damage, Healing, DamageOverTime };

UENUM(BlueprintType)
enum class ESkillLabExecutionTemplate : uint8 { InstantTarget, Projectile, GroundArea };

UENUM(BlueprintType)
enum class ESkillLabReleaseTrigger : uint8 { Timer, GameplayEvent };

UENUM(BlueprintType)
enum class ESkillLabSkillEvent : uint8 { Rejected, Started, TargetSelected, PhaseChanged, Hit, DamageTick, EffectApplied, BuffApplied, BuffRemoved, Cancelled, Completed, SubmissionCommitted, SubmissionResolved };

UENUM(BlueprintType)
enum class ESkillLabCastStage : uint8 { Idle, Windup, Active, Recovery, Completed, Cancelled };

UENUM(BlueprintType)
enum class ESkillLabOperationCategory : uint8 { Skill, Dodge, Movement, UserCancel, NormalHit, HeavyHit, UnknownHit };

UENUM(BlueprintType)
enum class ESkillLabRuleDecision : uint8 { Continue, Allow, Reject, Buffer, CancelAndActivate, Interrupt };

UENUM(BlueprintType)
enum class ESkillLabSubmissionTrigger : uint8 { Script, Release, Timer, GameplayEvent };

UENUM(BlueprintType)
enum class ESkillLabSubmissionTargetBinding : uint8 { Source, RequestTarget, RequestLocation };

UENUM(BlueprintType)
enum class ESkillLabCompletionMode : uint8 { ScriptOrEvent, AllSubmissionPointsResolved };

/** Identifies a Cast in the Runtime instance and generation that created it. */
USTRUCT()
struct SKILLLABSKILLS_API FSkillLabCastExecutionToken
{
	GENERATED_BODY()
	UPROPERTY() TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	UPROPERTY() uint64 RuntimeGeneration = 0;
	UPROPERTY() int32 CastId = 0;

	bool IsValid() const { return Runtime.IsValid() && RuntimeGeneration != 0 && CastId != 0; }
	friend bool operator==(const FSkillLabCastExecutionToken& A, const FSkillLabCastExecutionToken& B)
	{
		return A.Runtime == B.Runtime && A.RuntimeGeneration == B.RuntimeGeneration && A.CastId == B.CastId;
	}
};

FORCEINLINE uint32 GetTypeHash(const FSkillLabCastExecutionToken& Token)
{
	return HashCombine(HashCombine(GetTypeHash(Token.Runtime), GetTypeHash(Token.RuntimeGeneration)), GetTypeHash(Token.CastId));
}

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSubmissionPointDefinition
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Id = 1;
	UPROPERTY(BlueprintReadOnly) ESkillLabSubmissionTrigger Trigger = ESkillLabSubmissionTrigger::Script;
	UPROPERTY(BlueprintReadOnly) float DelaySeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly) FGameplayTag EventTag;
	UPROPERTY(BlueprintReadOnly) ESkillLabSubmissionTargetBinding TargetBinding = ESkillLabSubmissionTargetBinding::RequestTarget;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabOperationRules
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id = NAME_None;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision Skill = ESkillLabRuleDecision::Buffer;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision Dodge = ESkillLabRuleDecision::CancelAndActivate;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision Movement = ESkillLabRuleDecision::Continue;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision UserCancel = ESkillLabRuleDecision::Continue;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision NormalHit = ESkillLabRuleDecision::Continue;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision HeavyHit = ESkillLabRuleDecision::Interrupt;
	UPROPERTY(BlueprintReadOnly) ESkillLabRuleDecision UnknownHit = ESkillLabRuleDecision::Continue;
	/** When set, replaces the skill's interruptBy for this rule profile. */
	UPROPERTY(BlueprintReadOnly) bool bOverrideInterruptBy = false;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer InterruptBy;
	/** Exact ability-category matches; absent categories are rejected. */
	UPROPERTY(BlueprintReadOnly) TMap<FGameplayTag, ESkillLabRuleDecision> AbilityCategories;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabRuleWindow
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName Id = NAME_None;
	UPROPERTY(BlueprintReadOnly) FGameplayTag BeginEvent;
	UPROPERTY(BlueprintReadOnly) FGameplayTag EndEvent;
	UPROPERTY(BlueprintReadOnly) FSkillLabOperationRules Rules;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabCastPhaseDurations
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) float WindupSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly) float ActiveSeconds = 0.0f;
	UPROPERTY(BlueprintReadOnly) float RecoverySeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSkillRequest
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SkillId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> Caster = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<AActor> Target = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasLocation = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequestId = 0;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSkillInputIntent
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FSkillLabSkillRequest Request;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESkillLabOperationCategory Category = ESkillLabOperationCategory::Skill;
	UPROPERTY(BlueprintReadOnly) FGameplayTag AbilityCategory;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSkillResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
	/** True only when the authoritative effect request was accepted by GAS. */
	UPROPERTY(BlueprintReadOnly) bool bEffectApplied = false;
	UPROPERTY(BlueprintReadOnly) bool bHasReleased = false;
	UPROPERTY(BlueprintReadOnly) bool bCostCommitted = false;
	UPROPERTY(BlueprintReadOnly) int64 SubmissionPointId = 0;
	// 原提交的完整关联；没有提交时为空。反射接收者使用 SubmissionKey.Id 区分原操作。
	UPROPERTY(BlueprintReadOnly) FSkillLabSubmissionKey SubmissionKey;
	UPROPERTY(BlueprintReadOnly) FName Reason = NAME_None;
	UPROPERTY(BlueprintReadOnly) FName SkillId = NAME_None;
	/** Original Runtime/generation/Cast identity, preserved by reflected delegate copies. */
	UPROPERTY() FSkillLabCastExecutionToken CastToken;
	UPROPERTY(BlueprintReadOnly) ESkillLabSkillEvent Event = ESkillLabSkillEvent::Completed;
	UPROPERTY(BlueprintReadOnly) ESkillLabCastStage Stage = ESkillLabCastStage::Idle;
	UPROPERTY(BlueprintReadOnly) ESkillLabEffectKind Effect = ESkillLabEffectKind::Damage;
	UPROPERTY(BlueprintReadOnly) int32 RequestId = 0;
	UPROPERTY(BlueprintReadOnly) int32 CastId = 0;
	UPROPERTY(BlueprintReadOnly) int32 SourceUnitId = 0;
	UPROPERTY(BlueprintReadOnly) int32 TargetUnitId = 0;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> SourceActor = nullptr;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> TargetActor = nullptr;
	UPROPERTY(BlueprintReadOnly) FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly) float VisualRadius = 0.0f;
	UPROPERTY(BlueprintReadOnly) float RequestedAmount = 0.0f;
	UPROPERTY(BlueprintReadOnly) float ActualAmount = 0.0f;
	UPROPERTY(BlueprintReadOnly) float HealthBefore = 0.0f;
	UPROPERTY(BlueprintReadOnly) float HealthAfter = 0.0f;
	UPROPERTY(BlueprintReadOnly) float StageDuration = 0.0f;
	UPROPERTY(BlueprintReadOnly) FName EffectInstanceId = NAME_None;
	UPROPERTY(BlueprintReadOnly) float EffectDuration = 0.0f;
	UPROPERTY(BlueprintReadOnly) FName EffectOperationId = NAME_None;
};

USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSkillDefinition
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName SkillId = NAME_None;
	UPROPERTY(BlueprintReadOnly) ESkillLabTargetShape TargetShape = ESkillLabTargetShape::Unit;
	UPROPERTY(BlueprintReadOnly) ESkillLabTargetRelation Relation = ESkillLabTargetRelation::Enemy;
	UPROPERTY(BlueprintReadOnly) float Cost = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Cooldown = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Range = 1000.0f;
	UPROPERTY(BlueprintReadOnly) float Radius = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Magnitude = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Duration = 0.0f;
	UPROPERTY(BlueprintReadOnly) float Period = 0.0f;
	UPROPERTY(BlueprintReadOnly) FSkillLabCastPhaseDurations Phases;
	UPROPERTY(BlueprintReadOnly) ESkillLabEffectKind Effect = ESkillLabEffectKind::Damage;
	UPROPERTY(BlueprintReadOnly) ESkillLabExecutionTemplate Execution = ESkillLabExecutionTemplate::InstantTarget;
	UPROPERTY(BlueprintReadOnly) ESkillLabOperationCategory Category = ESkillLabOperationCategory::Skill;
	UPROPERTY(BlueprintReadOnly) FGameplayTag AbilityCategory;
	UPROPERTY(BlueprintReadOnly) TSubclassOf<USkillLabSkillBehavior> BehaviorClass;
	UPROPERTY(BlueprintReadOnly) ESkillLabReleaseTrigger ReleaseTrigger = ESkillLabReleaseTrigger::Timer;
	UPROPERTY(BlueprintReadOnly) TArray<FSkillLabSubmissionPointDefinition> SubmissionPoints;
	UPROPERTY(BlueprintReadOnly) bool bHasExplicitSubmissionPoints = false;
	UPROPERTY(BlueprintReadOnly) ESkillLabCompletionMode CompletionMode = ESkillLabCompletionMode::ScriptOrEvent;
	/** Optional independent preparation boundary; release still needs its own event. */
	UPROPERTY(BlueprintReadOnly) bool bIndependentWindup = false;
	UPROPERTY(BlueprintReadOnly) FGameplayTag ReleaseEventTag;
	UPROPERTY(BlueprintReadOnly) FGameplayTag CompletionEventTag;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer BehaviorEventTags;
	UPROPERTY(BlueprintReadOnly) FSkillLabOperationRules OperationRules;
	UPROPERTY(BlueprintReadOnly) FSkillLabOperationRules RecoveryRules;
	UPROPERTY(BlueprintReadOnly) TArray<FSkillLabRuleWindow> RuleWindows;
	UPROPERTY(BlueprintReadOnly) bool bRejectWindowConflicts = false;
	FName OperationRulesId = NAME_None;
	FName RecoveryRulesId = NAME_None;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> CastMontage;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UAnimSequence> CastAnimation;
	UPROPERTY(BlueprintReadOnly) float ReleaseEventTimeout = 3.0f;
	UPROPERTY(BlueprintReadOnly) float PowerCoefficient = 0.0f;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer BlockedByTags;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer InterruptedByTags;
	UPROPERTY(BlueprintReadOnly) TSubclassOf<UGameplayAbility> AbilityClass;
	/** 受限组合脚本路径，相对于 Config/。 */
	UPROPERTY(BlueprintReadOnly) FString LuaScript;
	/** 与规范化定义一起提交的已校验源文本；运行期不重读磁盘。 */
	FString LuaSource;
	/** 从 JSON 规范化得到的投射物调参。 */
	FSkillLabProjectileParameters ProjectileParameters;
};

/** Presentation-only binding resolved from a committed skill definition snapshot. */
USTRUCT(BlueprintType)
struct SKILLLABSKILLS_API FSkillLabSkillPresentationBinding
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName SkillId = NAME_None;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> CastMontage;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UAnimSequence> CastAnimation;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSkillLabSkillEventDelegate, const FSkillLabSkillResult&, Event);
DECLARE_MULTICAST_DELEGATE_OneParam(FSkillLabNativeSkillEvent, const FSkillLabSkillResult&);
