#include "SkillLabSkillAnimNotify.h"

#include "SkillLabSkillAnimationComponent.h"

void USkillLabSkillAnimNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !EventTag.IsValid())
	{
		return;
	}
	AActor* Owner = MeshComp->GetOwner();
	USkillLabSkillAnimationComponent* AnimationComponent = Owner
		? Owner->FindComponentByClass<USkillLabSkillAnimationComponent>() : nullptr;
	if (!AnimationComponent || !AnimationComponent->ForwardAnimNotify(Animation, EventTag, EventReference))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SkillLabAnimation] Notify ignored: no matching active cast montage"));
	}
}

FString USkillLabSkillAnimNotify::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("SkillLabAnimNotify");
}

namespace
{
	void ForwardStateEvent(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FGameplayTag EventTag, const FAnimNotifyEventReference& EventReference)
	{
		if (!MeshComp || !EventTag.IsValid()) return;
		AActor* Owner = MeshComp->GetOwner();
		USkillLabSkillAnimationComponent* AnimationComponent = Owner
			? Owner->FindComponentByClass<USkillLabSkillAnimationComponent>() : nullptr;
		if (!AnimationComponent || !AnimationComponent->ForwardAnimNotify(Animation, EventTag, EventReference))
		{
			UE_LOG(LogTemp, Verbose, TEXT("[SkillLabAnimation] Notify state ignored: no matching active cast montage"));
		}
	}
}

void USkillLabSkillAnimNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	ForwardStateEvent(MeshComp, Animation, EventTag, EventReference);
}

void USkillLabSkillAnimNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	if (!MeshComp || !EventTag.IsValid()) return;
	AActor* Owner = MeshComp->GetOwner();
	USkillLabSkillAnimationComponent* AnimationComponent = Owner
		? Owner->FindComponentByClass<USkillLabSkillAnimationComponent>() : nullptr;
	if (!AnimationComponent || !AnimationComponent->ForwardAnimNotify(Animation, EventTag, EventReference, true))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SkillLabAnimation] Notify state end ignored: no matching active cast montage"));
	}
}

FString USkillLabSkillAnimNotifyState::GetNotifyName_Implementation() const
{
	return EventTag.IsValid() ? EventTag.ToString() : TEXT("SkillLabAnimNotifyState");
}
