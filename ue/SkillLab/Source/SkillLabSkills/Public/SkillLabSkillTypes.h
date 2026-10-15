#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SkillLabProjectileTypes.h"
#include "SkillLabSkillTypes.generated.h"

class UGameplayAbility;
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
enum class ESkillLabSkillEvent : uint8 { Rejected, Started, TargetSelected, PhaseChanged, Hit, DamageTick, EffectApplied, BuffApplied, BuffRemoved, Cancelled, Completed };

UENUM(BlueprintType)
enum class ESkillLabCastStage : uint8 { Idle, Windup, Active, Recovery, Completed, Cancelled };

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
struct SKILLLABSKILLS_API FSkillLabSkillResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly) FName Reason = NAME_None;
	UPROPERTY(BlueprintReadOnly) FName SkillId = NAME_None;
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
	UPROPERTY(BlueprintReadOnly) TSubclassOf<USkillLabSkillBehavior> BehaviorClass;
	UPROPERTY(BlueprintReadOnly) ESkillLabReleaseTrigger ReleaseTrigger = ESkillLabReleaseTrigger::Timer;
	UPROPERTY(BlueprintReadOnly) FGameplayTag ReleaseEventTag;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer BehaviorEventTags;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UAnimMontage> CastMontage;
	UPROPERTY(BlueprintReadOnly) TSoftObjectPtr<UAnimSequence> CastAnimation;
	UPROPERTY(BlueprintReadOnly) float ReleaseEventTimeout = 3.0f;
	UPROPERTY(BlueprintReadOnly) float PowerCoefficient = 0.0f;
	UPROPERTY(BlueprintReadOnly) FGameplayTagContainer BlockedByTags;
	UPROPERTY(BlueprintReadOnly) TSubclassOf<UGameplayAbility> AbilityClass;
	/** Skill-owned projectile tuning; intentionally plain C++ data rather than JSON/Blueprint content. */
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
