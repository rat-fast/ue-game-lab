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
