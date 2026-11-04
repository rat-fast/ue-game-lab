#include "SkillLabSkillAnimationComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/ActiveMontageInstanceScope.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"

USkillLabSkillAnimationComponent::USkillLabSkillAnimationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabSkillAnimationComponent::BeginPlay()
{
	Super::BeginPlay();
	SkillRuntime = GetOwner() ? GetOwner()->FindComponentByClass<USkillLabSkillRuntimeComponent>() : nullptr;
}

void USkillLabSkillAnimationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCastMontage(ActiveCastId);
	SkillRuntime = nullptr;
	Super::EndPlay(EndPlayReason);
}

bool USkillLabSkillAnimationComponent::PlayCastAction(const int32 CastId, const FName SkillId)
{
	FSkillLabSkillResult Result;
	Result.CastId = CastId;
	Result.SkillId = SkillId;
	return PlayCastMontage(Result);
}

void USkillLabSkillAnimationComponent::StopCastAction(const int32 CastId)
{
	StopCastMontage(CastId);
}

bool USkillLabSkillAnimationComponent::PlayCastMontage(const FSkillLabSkillResult& Result)
{
	if (!SkillRuntime || Result.CastId <= 0)
	{
		return false;
	}
	// Started may be repeated or arrive late. It must not interrupt a newer cast.
	if (ActiveCastId == Result.CastId)
	{
		return true;
	}
	if (ActiveCastId > Result.CastId)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Ignoring stale Started Cast=%d; active Cast=%d"),
			Result.CastId, ActiveCastId);
		return false;
	}
	// Tear down the previous cast before resolving the replacement binding. A
	// missing asset must still leave the old cast's action stopped; otherwise a
	// new runtime cast can inherit a visually stale montage.
	if (ActiveCastId != 0 && ActiveCastId != Result.CastId)
	{
		StopCastMontage(ActiveCastId);
	}
	FSkillLabSkillPresentationBinding Binding;
	if (!FSkillLabSkillDefinitionProvider::ResolvePresentationBinding(Result.SkillId, Binding))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Cast=%d Skill=%s has no definition; montage not played"),
			Result.CastId, *Result.SkillId.ToString());
		return false;
	}
	ActiveMontage = Binding.CastMontage.LoadSynchronous();
	ActiveNotifyAnimation = ActiveMontage;
	if (!ActiveMontage)
	{
		if (UAnimSequence* CastAnimation = Binding.CastAnimation.LoadSynchronous())
		{
			ActiveNotifyAnimation = CastAnimation;
			ActiveMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(CastAnimation, TEXT("DefaultSlot"), 0.08f, 0.12f, 1.0f);
		}
	}
	if (!ActiveMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Cast=%d Skill=%s has no usable CastMontage or CastAnimation; animation timing is not connected"),
			Result.CastId, *Result.SkillId.ToString());
		ActiveCastId = 0;
		ActiveMontageInstanceId = INDEX_NONE;
		return false;
	}
	USkeletalMeshComponent* Mesh = FindMesh();
	ActiveAnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!ActiveAnimInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Cast=%d Skill=%s has no skeletal AnimInstance; montage not played"),
			Result.CastId, *Result.SkillId.ToString());
		ActiveMontage = nullptr;
		ActiveNotifyAnimation = nullptr;
		ActiveCastId = 0;
		ActiveMontageInstanceId = INDEX_NONE;
		return false;
	}
	const float PlayedLength = ActiveAnimInstance->Montage_Play(ActiveMontage, 1.0f);
	if (PlayedLength <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Cast=%d Skill=%s failed to play CastMontage=%s"),
			Result.CastId, *Result.SkillId.ToString(), *ActiveMontage->GetName());
		ActiveAnimInstance = nullptr;
		ActiveMontage = nullptr;
		ActiveNotifyAnimation = nullptr;
		ActiveCastId = 0;
		ActiveMontageInstanceId = INDEX_NONE;
		return false;
	}
	const FAnimMontageInstance* MontageInstance = ActiveAnimInstance->GetActiveInstanceForMontage(ActiveMontage);
	if (!MontageInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabAnimation] Cast=%d Skill=%s played montage but has no active montage instance"),
			Result.CastId, *Result.SkillId.ToString());
		ActiveAnimInstance->Montage_Stop(0.0f, ActiveMontage);
		ActiveAnimInstance = nullptr;
		ActiveMontage = nullptr;
		ActiveNotifyAnimation = nullptr;
		ActiveCastId = 0;
		ActiveMontageInstanceId = INDEX_NONE;
		return false;
	}
	// Publish the cast identity only after the new montage is actually playing.
	// A delayed notify from the montage we just stopped therefore cannot borrow
	// the new cast identity during the handoff.
	ActiveCastId = Result.CastId;
	ActiveMontageInstanceId = MontageInstance->GetInstanceID();
	UE_LOG(LogTemp, Log, TEXT("[SkillLabAnimation] Cast=%d Skill=%s playing Montage=%s Slot=DefaultSlot"),
		Result.CastId, *Result.SkillId.ToString(), *ActiveMontage->GetName());
	return true;
}

bool USkillLabSkillAnimationComponent::ForwardCastEvent(const int32 CastId, const FGameplayTag EventTag)
{
	if (!SkillRuntime || CastId <= 0 || CastId != ActiveCastId || !EventTag.IsValid())
	{
		return false;
	}
	return SkillRuntime->ForwardValidatedNotify(CastId, EventTag);
}

bool USkillLabSkillAnimationComponent::ForwardAnimNotify(UAnimSequenceBase* Animation, const FGameplayTag EventTag,
	const FAnimNotifyEventReference& EventReference, const bool bStateEnd)
{
	if (!Animation || !ActiveMontage || (Animation != ActiveMontage.Get() && Animation != ActiveNotifyAnimation.Get()) || !ActiveAnimInstance
		|| !ActiveAnimInstance->Montage_IsPlaying(ActiveMontage) || ActiveCastId <= 0
		|| ActiveMontageInstanceId == INDEX_NONE)
	{
		return false;
	}
	const UE::Anim::FAnimNotifyMontageInstanceContext* MontageContext =
		EventReference.GetContextData<UE::Anim::FAnimNotifyMontageInstanceContext>();
	if (!MontageContext || MontageContext->MontageInstanceID != ActiveMontageInstanceId)
	{
		return false;
	}
	// A Notify State has two callbacks.  The runtime event contract is a point
	// event, so the end callback is validated against the same cast/instance but
	// is deliberately not replayed as a second gameplay event.
	return bStateEnd ? true : ForwardCastEvent(ActiveCastId, EventTag);
}

void USkillLabSkillAnimationComponent::StopCastMontage(const int32 CastId)
{
	if (CastId <= 0 || CastId != ActiveCastId)
	{
		return;
	}
	if (ActiveAnimInstance && ActiveMontage)
	{
		ActiveAnimInstance->Montage_Stop(0.12f, ActiveMontage);
	}
	ActiveAnimInstance = nullptr;
	ActiveMontage = nullptr;
	ActiveNotifyAnimation = nullptr;
	ActiveCastId = 0;
	ActiveMontageInstanceId = INDEX_NONE;
}

USkeletalMeshComponent* USkillLabSkillAnimationComponent::FindMesh() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	TArray<USkeletalMeshComponent*> Meshes;
	Owner->GetComponents<USkeletalMeshComponent>(Meshes);
	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}
