#include "SkillLabSkillComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabAttributeSet.h"
#include "SkillLabPresentationRouter.h"
#include "SkillLabSkillAbility.h"
#include "SkillLabSkillDefinitionProvider.h"

namespace
{
	const TCHAR* RequestSourceToString(const ESkillLabRequestSource Source)
	{
		switch (Source)
		{
		case ESkillLabRequestSource::Player: return TEXT("Player");
		case ESkillLabRequestSource::AI: return TEXT("AI");
		default: return TEXT("Test");
		}
	}

	const TCHAR* TargetTypeToString(const ESkillLabTargetType TargetType)
	{
		return TargetType == ESkillLabTargetType::Unit ? TEXT("Unit") : TEXT("Self");
	}

	const TCHAR* EventTypeToString(const ESkillLabEventType EventType)
	{
		switch (EventType)
		{
		case ESkillLabEventType::Initialized: return TEXT("Initialized");
		case ESkillLabEventType::Granted: return TEXT("Granted");
		case ESkillLabEventType::Rejected: return TEXT("Rejected");
		case ESkillLabEventType::RequestStarted: return TEXT("RequestStarted");
		case ESkillLabEventType::ActivationChecked: return TEXT("ActivationChecked");
		case ESkillLabEventType::TargetResolved: return TEXT("TargetResolved");
		case ESkillLabEventType::EffectApplied: return TEXT("EffectApplied");
		case ESkillLabEventType::EffectRejected: return TEXT("EffectRejected");
		case ESkillLabEventType::Completed: return TEXT("Completed");
		case ESkillLabEventType::Cancelled: return TEXT("Cancelled");
		case ESkillLabEventType::Interrupted: return TEXT("Interrupted");
		case ESkillLabEventType::ResetStarted: return TEXT("ResetStarted");
		case ESkillLabEventType::ResetCompleted: return TEXT("ResetCompleted");
		default: return TEXT("Unknown");
		}
	}

	ESkillLabEventStage EventTypeToStage(const ESkillLabEventType EventType)
	{
		switch (EventType)
		{
		case ESkillLabEventType::Initialized:
		case ESkillLabEventType::Granted:
			return ESkillLabEventStage::Initialization;
		case ESkillLabEventType::Rejected:
		case ESkillLabEventType::ActivationChecked:
			return ESkillLabEventStage::Validation;
		case ESkillLabEventType::RequestStarted:
			return ESkillLabEventStage::Activation;
		case ESkillLabEventType::TargetResolved:
			return ESkillLabEventStage::Targeting;
		case ESkillLabEventType::EffectApplied:
		case ESkillLabEventType::EffectRejected:
			return ESkillLabEventStage::Effect;
		case ESkillLabEventType::Completed:
		case ESkillLabEventType::Cancelled:
		case ESkillLabEventType::Interrupted:
			return ESkillLabEventStage::Completion;
		case ESkillLabEventType::ResetStarted:
		case ESkillLabEventType::ResetCompleted:
			return ESkillLabEventStage::Reset;
		default:
			return ESkillLabEventStage::Unknown;
		}
	}
}

USkillLabSkillComponent::USkillLabSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	PresentationRouter = NewObject<USkillLabPresentationRouter>(this);
	if (PresentationRouter)
	{
		PresentationRouter->Bind(this);
	}
	InitializeUnit();
}

void USkillLabSkillComponent::InitializeUnit()
{
	if (bInitialized)
	{
		LogEvent(TEXT("InitializeSkipped"), TEXT("AlreadyInitialized"));
		return;
	}

	AActor* OwnerActor = GetOwner();
	AbilitySystemComponent = OwnerActor ? OwnerActor->FindComponentByClass<USkillLabAbilitySystemComponent>() : nullptr;
	if (!AbilitySystemComponent)
	{
		LogEvent(TEXT("InitializeFailed"), TEXT("MissingASC"));
		return;
	}

	AbilitySystemComponent->InitAbilityActorInfo(OwnerActor, OwnerActor);
	AttributeSet = const_cast<USkillLabAttributeSet*>(AbilitySystemComponent->GetSet<USkillLabAttributeSet>());
	if (!AttributeSet)
	{
		LogEvent(TEXT("InitializeFailed"), TEXT("MissingAttributeSet"));
		return;
	}

	if (UnitId == 0)
	{
		UnitId = GetUniqueID();
	}
	AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetTestValueAttribute(), USkillLabAttributeSet::GetDefaultTestValue());
	AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetSkillEnergyAttribute(), USkillLabAttributeSet::GetDefaultSkillEnergy());

	TArray<FName> DefaultSkillIds;
	FSkillLabSkillDefinitionProvider::GetDefaultSkillIds(DefaultSkillIds);
	if (DefaultSkillIds.IsEmpty())
	{
		LogEvent(TEXT("InitializeFailed"), TEXT("NoDefaultSkills"));
		return;
	}

	bool bAllSkillsGranted = true;
	for (const FName SkillId : DefaultSkillIds)
	{
		bAllSkillsGranted &= GrantSkill(SkillId);
	}
	if (!bAllSkillsGranted)
	{
		LogEvent(TEXT("InitializeFailed"), TEXT("DefaultSkillConfigurationError"));
		return;
	}

	bInitialized = true;
	PublishEvent(ESkillLabEventType::Initialized);
}

bool USkillLabSkillComponent::GrantSkill(const FName SkillId)
{
	if (SkillId.IsNone() || !AbilitySystemComponent)
	{
		LogEvent(TEXT("GrantFailed"), TEXT("MissingSkillIdOrASC"));
		return false;
	}

	if (const FSkillLabGrantedSkillRecord* ExistingRecord = GrantedSkillHandles.Find(SkillId))
	{
		if (ExistingRecord->SpecHandle.IsValid())
		{
			LogEvent(TEXT("GrantSkipped"), TEXT("AlreadyGranted"));
			return true;
		}
		GrantedSkillHandles.Remove(SkillId);
	}

	FSkillLabSkillDefinition Definition;
	const ESkillLabDefinitionStatus DefinitionStatus = FSkillLabSkillDefinitionProvider::ResolveDefinition(SkillId, Definition);
	if (DefinitionStatus != ESkillLabDefinitionStatus::Available || !Definition.AbilityClass)
	{
		LogEvent(TEXT("GrantFailed"), TEXT("SkillDefinitionUnavailable"));
		return false;
	}

	const FGameplayAbilitySpecHandle GrantedHandle = AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Definition.AbilityClass, 1));
	if (!GrantedHandle.IsValid())
	{
		LogEvent(TEXT("GrantFailed"), TEXT("InvalidAbilitySpecHandle"));
		return false;
	}

	FSkillLabGrantedSkillRecord Record;
	Record.SkillId = SkillId;
	Record.GrantedAbilityId = NextGrantedAbilityId++;
	Record.SpecHandle = GrantedHandle;
	GrantedSkillHandles.Add(SkillId, Record);
	CurrentGrantedAbilityId = Record.GrantedAbilityId;
	CurrentDefinitionStatus = DefinitionStatus;
	CurrentDefinitionSnapshot = Definition;
	CurrentRequest = FSkillLabSkillRequest();
	CurrentRequest.CasterObject = GetOwner();
	CurrentRequest.CasterUnitId = UnitId;
	CurrentRequest.Source = ESkillLabRequestSource::Test;
	CurrentRequest.TargetType = Definition.TargetType;
	CurrentRequest.TargetObject = GetOwner();
	CurrentRequest.TargetUnitId = UnitId;
	CurrentRequest.TargetLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	CurrentRequest.TargetDirection = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
	CurrentRequest.SkillId = Definition.SkillId;
	CurrentRequest.DefinitionVersion = Definition.Version;
	CurrentRequest.DefinitionSource = Definition.DefinitionSource;
	CurrentRequest.ExecutionTemplate = Definition.ExecutionTemplate;
	CurrentRequest.PresentationBinding = Definition.PresentationBinding;
	PublishEvent(ESkillLabEventType::Granted);
	return true;
}

FSkillLabRequestResult USkillLabSkillComponent::SubmitSkillRequest(const FSkillLabSkillRequest& Request)
{
	FSkillLabRequestResult Result;
	Result.RequestId = NextRequestId++;
	Result.UnitId = UnitId;
	Result.SourceUnitId = UnitId;
	Result.CasterUnitId = Request.CasterUnitId;
	Result.SkillId = Request.SkillId;
	Result.Source = Request.Source;
	Result.TargetType = Request.TargetType;
	CurrentRequestId = Result.RequestId;
	CurrentGrantedAbilityId = 0;
	CurrentRequest = Request;
	CurrentRequest.RequestId = Result.RequestId;
	CurrentRequest.RequestTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	PendingSkillAbility = nullptr;
	CurrentDefinitionSnapshot = FSkillLabSkillDefinition();
	CurrentDefinitionStatus = ESkillLabDefinitionStatus::Missing;

	FSkillLabSkillDefinition Definition;
	CurrentDefinitionStatus = FSkillLabSkillDefinitionProvider::ResolveDefinition(Request.SkillId, Definition);
	Result.DefinitionStatus = CurrentDefinitionStatus;
	if (CurrentDefinitionStatus != ESkillLabDefinitionStatus::Available)
	{
		switch (CurrentDefinitionStatus)
		{
		case ESkillLabDefinitionStatus::Missing:
			Result.Reason = TEXT("SkillDefinitionMissing");
			break;
		case ESkillLabDefinitionStatus::Disabled:
			Result.Reason = TEXT("SkillDisabled");
			break;
		case ESkillLabDefinitionStatus::Invalid:
			Result.Reason = TEXT("SkillDefinitionInvalid");
			break;
		default:
			Result.Reason = TEXT("SkillDefinitionUnavailable");
			break;
		}
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}

	CurrentDefinitionSnapshot = Definition;
	Result.DefinitionVersion = Definition.Version;
	Result.DefinitionSource = Definition.DefinitionSource;
	CurrentRequest.DefinitionVersion = Definition.Version;
	CurrentRequest.DefinitionSource = Definition.DefinitionSource;
	CurrentRequest.ExecutionTemplate = Definition.ExecutionTemplate;
	CurrentRequest.PresentationBinding = Definition.PresentationBinding;

	if (!bInitialized || !AbilitySystemComponent || !AttributeSet)
	{
		Result.Reason = TEXT("NotInitialized");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	if (!IsValid(Request.CasterObject) || Request.CasterObject->IsActorBeingDestroyed())
	{
		Result.Reason = TEXT("InvalidCasterObject");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	if (Request.CasterObject != GetOwner())
	{
		Result.Reason = TEXT("CasterObjectMismatch");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	if (Request.CasterUnitId == 0)
	{
		Result.Reason = TEXT("MissingCasterUnitId");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	if (Request.CasterUnitId != UnitId)
	{
		Result.Reason = TEXT("CasterUnitIdMismatch");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	const FSkillLabGrantedSkillRecord* GrantedRecord = GrantedSkillHandles.Find(Request.SkillId);
	if (!GrantedRecord || !GrantedRecord->SpecHandle.IsValid())
	{
		Result.Reason = TEXT("SkillNotGranted");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	CurrentGrantedAbilityId = GrantedRecord->GrantedAbilityId;
	Result.GrantedAbilityId = CurrentGrantedAbilityId;
	if (Request.TargetType != Definition.TargetType)
	{
		Result.Reason = TEXT("InvalidTarget");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	AActor* ResolvedTarget = Request.TargetObject ? Request.TargetObject.Get() : (Definition.TargetType == ESkillLabTargetType::Self ? GetOwner() : nullptr);
	if (Definition.TargetType == ESkillLabTargetType::Self && ResolvedTarget != GetOwner())
	{
		Result.Reason = TEXT("InvalidTarget");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	if (Definition.TargetType == ESkillLabTargetType::Unit && !IsValid(ResolvedTarget))
	{
		Result.Reason = TEXT("InvalidTarget");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	CurrentRequest.TargetObject = ResolvedTarget;
	if (Definition.TargetType == ESkillLabTargetType::Self)
	{
		if (Request.TargetUnitId != 0 && Request.TargetUnitId != UnitId)
		{
			Result.Reason = TEXT("TargetUnitIdMismatch");
			PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
			return Result;
		}
		CurrentRequest.TargetUnitId = UnitId;
	}
	else if (USkillLabSkillComponent* TargetSkillComponent = ResolvedTarget->FindComponentByClass<USkillLabSkillComponent>())
	{
		if (!TargetSkillComponent->bInitialized || TargetSkillComponent->GetUnitId() == 0)
		{
			Result.Reason = TEXT("TargetUnitIdUnavailable");
			PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
			return Result;
		}
		CurrentRequest.TargetUnitId = TargetSkillComponent->GetUnitId();
		if (Request.TargetUnitId != 0 && Request.TargetUnitId != CurrentRequest.TargetUnitId)
		{
			Result.Reason = TEXT("TargetUnitIdMismatch");
			PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
			return Result;
		}
	}
	else
	{
		Result.Reason = TEXT("TargetUnitIdUnavailable");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	CurrentRequest.TargetLocation = Request.TargetLocation.IsNearlyZero()
		? (ResolvedTarget ? ResolvedTarget->GetActorLocation() : FVector::ZeroVector)
		: Request.TargetLocation;
	CurrentRequest.TargetDirection = Request.TargetDirection.IsNearlyZero()
		? (ResolvedTarget ? ResolvedTarget->GetActorForwardVector() : FVector::ForwardVector)
		: Request.TargetDirection;
	Result.TargetUnitId = CurrentRequest.TargetUnitId;
	Result.TargetLocation = CurrentRequest.TargetLocation;
	Result.TargetDirection = CurrentRequest.TargetDirection;
	const float* CooldownEndTime = CooldownEndTimes.Find(Request.SkillId);
	if (CooldownEndTime && GetWorld()->GetTimeSeconds() < *CooldownEndTime)
	{
		Result.Reason = TEXT("Cooldown");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}
	if (GetSkillEnergy() < Definition.ResourceCost)
	{
		Result.Reason = TEXT("InsufficientResource");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}

	PublishEvent(ESkillLabEventType::ActivationChecked);
	if (!AbilitySystemComponent->TryActivateAbility(GrantedRecord->SpecHandle) || !PendingSkillAbility)
	{
		PendingSkillAbility = nullptr;
		Result.Reason = TEXT("ActivationFailed");
		PublishEvent(ESkillLabEventType::Rejected, Result.Reason);
		return Result;
	}

	ActiveCastId = NextCastId++;
	Result.Status = ESkillLabRequestStatus::Started;
	Result.Stage = ESkillLabEventStage::Activation;
	Result.CastId = ActiveCastId;
	AbilitySystemComponent->ApplyModToAttribute(USkillLabAttributeSet::GetSkillEnergyAttribute(), EGameplayModOp::Additive, -Definition.ResourceCost);
	CooldownEndTimes.Add(Request.SkillId, GetWorld()->GetTimeSeconds() + Definition.CooldownSeconds);
	PublishEvent(ESkillLabEventType::RequestStarted);
	PublishEvent(ESkillLabEventType::TargetResolved);

	USkillLabSkillAbility* ActivatedAbility = PendingSkillAbility;
	PendingSkillAbility = nullptr;
	if (!ActivatedAbility->ApplyEffect(Definition, CurrentRequest, ActiveCastId, CurrentGrantedAbilityId, Result.EffectResult))
	{
		Result.Reason = TEXT("EffectApplyFailed");
		Result.bEffectSucceeded = false;
		PublishEvent(ESkillLabEventType::EffectRejected, Result.Reason, &Result.EffectResult);
	}
	else
	{
		Result.bEffectSucceeded = true;
		PublishEvent(ESkillLabEventType::EffectApplied, FString(), &Result.EffectResult);
	}

	Result.Status = ESkillLabRequestStatus::Completed;
	Result.Stage = ESkillLabEventStage::Completion;
	PublishEvent(ESkillLabEventType::Completed, Result.Reason, &Result.EffectResult);
	ActiveCastId = 0;
	return Result;
}

void USkillLabSkillComponent::NotifySkillAbilityActivationPending(USkillLabSkillAbility* Ability)
{
	PendingSkillAbility = Ability;
}

void USkillLabSkillComponent::ResetUnit()
{
	PublishEvent(ESkillLabEventType::ResetStarted);
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->CancelAllAbilities();
		AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetTestValueAttribute(), USkillLabAttributeSet::GetDefaultTestValue());
		AbilitySystemComponent->SetNumericAttributeBase(USkillLabAttributeSet::GetSkillEnergyAttribute(), USkillLabAttributeSet::GetDefaultSkillEnergy());
	}

	CooldownEndTimes.Reset();
	if (ActiveCastId != 0)
	{
		PublishEvent(ESkillLabEventType::Interrupted, TEXT("Reset"));
	}
	ActiveCastId = 0;
	PendingSkillAbility = nullptr;
	PublishEvent(ESkillLabEventType::ResetCompleted);
	ResetRequestContext();
}

float USkillLabSkillComponent::GetTestValue() const
{
	return AbilitySystemComponent ? AbilitySystemComponent->GetNumericAttribute(USkillLabAttributeSet::GetTestValueAttribute()) : 0.0f;
}

float USkillLabSkillComponent::GetSkillEnergy() const
{
	return AbilitySystemComponent ? AbilitySystemComponent->GetNumericAttribute(USkillLabAttributeSet::GetSkillEnergyAttribute()) : 0.0f;
}

void USkillLabSkillComponent::PublishEvent(const ESkillLabEventType EventType, const FString& Reason, const FSkillLabEffectApplicationResult* EffectResult)
{
	FSkillLabEventData Event;
	Event.EventId = NextEventId++;
	Event.EventType = EventType;
	Event.Stage = EventTypeToStage(EventType);
	Event.UnitId = UnitId;
	Event.CasterUnitId = CurrentRequest.CasterUnitId;
	Event.SourceUnitId = UnitId;
	Event.TargetUnitId = CurrentRequest.TargetUnitId;
	Event.SourceObject = GetOwner();
	Event.SkillId = CurrentRequest.SkillId;
	Event.RequestId = CurrentRequestId;
	Event.CastId = ActiveCastId;
	Event.GrantedAbilityId = CurrentGrantedAbilityId;
	Event.Source = CurrentRequest.Source;
	Event.TargetType = CurrentRequest.TargetType;
	Event.DefinitionStatus = CurrentDefinitionStatus;
	Event.DefinitionVersion = CurrentDefinitionSnapshot.Version;
	Event.DefinitionSource = CurrentDefinitionSnapshot.DefinitionSource;
	Event.TargetLocation = CurrentRequest.TargetLocation;
	Event.TargetDirection = CurrentRequest.TargetDirection;
	switch (EventType)
	{
	case ESkillLabEventType::Rejected:
		Event.ResultStatus = ESkillLabRequestStatus::Rejected;
		break;
	case ESkillLabEventType::Completed:
		Event.ResultStatus = ESkillLabRequestStatus::Completed;
		break;
	case ESkillLabEventType::Cancelled:
		Event.ResultStatus = ESkillLabRequestStatus::Cancelled;
		break;
	case ESkillLabEventType::Interrupted:
		Event.ResultStatus = ESkillLabRequestStatus::Interrupted;
		break;
	case ESkillLabEventType::EffectRejected:
		Event.ResultStatus = ESkillLabRequestStatus::Rejected;
		break;
	case ESkillLabEventType::Initialized:
	case ESkillLabEventType::Granted:
	case ESkillLabEventType::ResetStarted:
	case ESkillLabEventType::ResetCompleted:
		Event.ResultStatus = ESkillLabRequestStatus::Rejected;
		break;
	default:
		Event.ResultStatus = ESkillLabRequestStatus::Started;
		break;
	}
	Event.TargetObject = CurrentRequest.TargetObject ? CurrentRequest.TargetObject.Get() : (CurrentRequest.TargetType == ESkillLabTargetType::Self ? GetOwner() : nullptr);
	Event.Reason = Reason;
	Event.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (EffectResult)
	{
		Event.EffectResult = *EffectResult;
		Event.bEffectSucceeded = EffectResult->Status == ESkillLabEffectResultStatus::Applied;
	}
	OnSkillEvent.Broadcast(Event);
	FString EventDetail = Reason;
	if (EffectResult)
	{
		EventDetail = FString::Printf(
			TEXT("%s EffectType=%s AppliedMagnitude=%.2f EffectStatus=%d bEffectSucceeded=%s"),
			*Reason,
			*EffectResult->EffectType.ToString(),
			EffectResult->AppliedMagnitude,
			static_cast<int32>(EffectResult->Status),
			Event.bEffectSucceeded ? TEXT("true") : TEXT("false"));
	}
	LogEvent(EventTypeToString(EventType), EventDetail);
}

void USkillLabSkillComponent::LogEvent(const TCHAR* EventName, const FString& Detail) const
{
	const FString Context = BuildTraceContext();
	UE_LOG(LogTemp, Log, TEXT("[SkillSystem] %s Event=%s%s"), *Context, EventName, Detail.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" Detail=%s"), *Detail));
}

FString USkillLabSkillComponent::BuildTraceContext() const
{
	return FString::Printf(
		TEXT("UnitId=%d CasterUnitId=%d SourceUnitId=%d TargetUnitId=%d SkillId=%s GrantedAbilityId=%d RequestId=%d CastId=%d Source=%s Target=%s"),
		UnitId,
		CurrentRequest.CasterUnitId,
		UnitId,
		CurrentRequest.TargetUnitId,
		*CurrentRequest.SkillId.ToString(),
		CurrentGrantedAbilityId,
		CurrentRequestId,
		ActiveCastId,
		RequestSourceToString(CurrentRequest.Source),
		TargetTypeToString(CurrentRequest.TargetType));
}

void USkillLabSkillComponent::ResetRequestContext()
{
	CurrentRequest = FSkillLabSkillRequest();
	CurrentDefinitionSnapshot = FSkillLabSkillDefinition();
	CurrentDefinitionStatus = ESkillLabDefinitionStatus::Missing;
	CurrentRequestId = 0;
	CurrentGrantedAbilityId = 0;
}
