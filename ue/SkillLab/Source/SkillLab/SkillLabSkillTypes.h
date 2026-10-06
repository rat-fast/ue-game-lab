#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SkillLabSkillTypes.generated.h"

class UGameplayAbility;
class UGameplayEffect;

UENUM(BlueprintType)
enum class ESkillLabRequestSource : uint8
{
	Test,
	Player,
	AI
};

UENUM(BlueprintType)
enum class ESkillLabTargetType : uint8
{
	Self,
	Unit
};

UENUM(BlueprintType)
enum class ESkillLabRequestStatus : uint8
{
	Rejected,
	Buffered,
	Started,
	Completed,
	Cancelled,
	Interrupted,
	Expired
};

UENUM(BlueprintType)
enum class ESkillLabDefinitionStatus : uint8
{
	Missing,
	Disabled,
	Invalid,
	Available
};

UENUM(BlueprintType)
enum class ESkillLabEventStage : uint8
{
	Unknown,
	Initialization,
	Validation,
	Activation,
	Targeting,
	Effect,
	Completion,
	Reset
};

UENUM(BlueprintType)
enum class ESkillLabEffectResultStatus : uint8
{
	Applied,
	Immune,
	InvalidTarget,
	Blocked,
	Failed
};

UENUM(BlueprintType)
enum class ESkillLabEventType : uint8
{
	Initialized,
	Granted,
	Rejected,
	RequestStarted,
	ActivationChecked,
	TargetResolved,
	EffectApplied,
	EffectRejected,
	Completed,
	Cancelled,
	Interrupted,
	ResetStarted,
	ResetCompleted
};

USTRUCT(BlueprintType)
struct SKILLLAB_API FSkillLabSkillRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	int32 RequestId = 0;

	UPROPERTY(BlueprintReadWrite)
	ESkillLabRequestSource Source = ESkillLabRequestSource::Test;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> CasterObject = nullptr;

	UPROPERTY(BlueprintReadWrite)
	int32 CasterUnitId = 0;

	UPROPERTY(BlueprintReadWrite)
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadWrite)
	ESkillLabTargetType TargetType = ESkillLabTargetType::Self;

	UPROPERTY(BlueprintReadWrite)
	TObjectPtr<AActor> TargetObject = nullptr;

	UPROPERTY(BlueprintReadWrite)
	int32 TargetUnitId = 0;

	UPROPERTY(BlueprintReadWrite)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite)
	FVector TargetDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadWrite)
	int32 LoadoutVersion = 0;

	UPROPERTY(BlueprintReadWrite)
	float RequestTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	int32 DefinitionVersion = 0;

	UPROPERTY(BlueprintReadOnly)
	FName DefinitionSource = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName ExecutionTemplate = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName PresentationBinding = NAME_None;
};

USTRUCT(BlueprintType)
struct SKILLLAB_API FSkillLabEffectApplicationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 EffectApplicationId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CastId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 GrantedAbilityId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> SourceObject = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> TargetObject = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName EffectType = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	float Magnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float AppliedMagnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float WorldTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabEffectResultStatus Status = ESkillLabEffectResultStatus::Failed;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;
};

USTRUCT(BlueprintType)
struct SKILLLAB_API FSkillLabRequestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ESkillLabRequestStatus Status = ESkillLabRequestStatus::Rejected;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CastId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 GrantedAbilityId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 UnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CasterUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly)
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabRequestSource Source = ESkillLabRequestSource::Test;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabTargetType TargetType = ESkillLabTargetType::Self;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabDefinitionStatus DefinitionStatus = ESkillLabDefinitionStatus::Missing;

	UPROPERTY(BlueprintReadOnly)
	int32 DefinitionVersion = 0;

	UPROPERTY(BlueprintReadOnly)
	FName DefinitionSource = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabEventStage Stage = ESkillLabEventStage::Unknown;

	UPROPERTY(BlueprintReadOnly)
	bool bEffectSucceeded = false;

	UPROPERTY(BlueprintReadOnly)
	FSkillLabEffectApplicationResult EffectResult;
};

USTRUCT(BlueprintType)
struct SKILLLAB_API FSkillLabEventData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 EventId = 0;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabEventType EventType = ESkillLabEventType::Initialized;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabEventStage Stage = ESkillLabEventStage::Unknown;

	UPROPERTY(BlueprintReadOnly)
	int32 UnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CasterUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 SourceUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 TargetUnitId = 0;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> SourceObject = nullptr;

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> TargetObject = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 RequestId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 CastId = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 GrantedAbilityId = 0;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabRequestSource Source = ESkillLabRequestSource::Test;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabTargetType TargetType = ESkillLabTargetType::Self;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabRequestStatus ResultStatus = ESkillLabRequestStatus::Rejected;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabDefinitionStatus DefinitionStatus = ESkillLabDefinitionStatus::Missing;

	UPROPERTY(BlueprintReadOnly)
	int32 DefinitionVersion = 0;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector TargetDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly)
	bool bEffectSucceeded = false;

	UPROPERTY(BlueprintReadOnly)
	FName DefinitionSource = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FString Reason;

	UPROPERTY(BlueprintReadOnly)
	float WorldTime = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FSkillLabEffectApplicationResult EffectResult;
};

USTRUCT(BlueprintType)
struct SKILLLAB_API FSkillLabSkillDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName SkillId = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	int32 Version = 0;

	UPROPERTY(BlueprintReadOnly)
	FName DefinitionSource = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName ExecutionTemplate = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName PresentationBinding = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	ESkillLabTargetType TargetType = ESkillLabTargetType::Self;

	UPROPERTY(BlueprintReadOnly)
	float ResourceCost = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float CooldownSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	FName EffectType = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	float EffectMagnitude = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> EffectClass;

	UPROPERTY(BlueprintReadOnly)
	bool bEnabled = false;
};
