#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.generated.h"

class UGameplayAbility;

UENUM(BlueprintType)
enum class ESkillLabTargetShape : uint8 { Self, Unit, Ground };

UENUM(BlueprintType)
enum class ESkillLabTargetRelation : uint8 { Self, Ally, Enemy, Any };

UENUM(BlueprintType)
enum class ESkillLabEffectKind : uint8 { Damage, Healing, DamageOverTime };

UENUM(BlueprintType)
enum class ESkillLabExecutionTemplate : uint8 { InstantTarget, Projectile, GroundArea };

UENUM(BlueprintType)
enum class ESkillLabSkillEvent : uint8 { Rejected, Started, TargetSelected, Hit, EffectApplied, BuffApplied, Completed };

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
	UPROPERTY(BlueprintReadOnly) ESkillLabEffectKind Effect = ESkillLabEffectKind::Damage;
	UPROPERTY(BlueprintReadOnly) ESkillLabExecutionTemplate Execution = ESkillLabExecutionTemplate::InstantTarget;
	UPROPERTY(BlueprintReadOnly) float PowerCoefficient = 0.0f;
	UPROPERTY(BlueprintReadOnly) TSubclassOf<UGameplayAbility> AbilityClass;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSkillLabSkillEventDelegate, const FSkillLabSkillResult&, Event);
DECLARE_MULTICAST_DELEGATE_OneParam(FSkillLabNativeSkillEvent, const FSkillLabSkillResult&);
