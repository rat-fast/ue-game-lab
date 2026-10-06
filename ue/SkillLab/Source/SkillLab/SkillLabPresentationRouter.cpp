#include "SkillLabPresentationRouter.h"

#include "SkillLabSkillComponent.h"

void USkillLabPresentationRouter::Bind(USkillLabSkillComponent* InSource)
{
	if (Source == InSource)
	{
		return;
	}

	if (Source)
	{
		Source->OnSkillEvent.RemoveAll(this);
	}

	Source = InSource;
	if (Source)
	{
		Source->OnSkillEvent.AddUObject(this, &USkillLabPresentationRouter::HandleSkillEvent);
	}
}

void USkillLabPresentationRouter::ClearActivePresentation()
{
	ActiveCastId = 0;
	bHasActiveCast = false;
}

void USkillLabPresentationRouter::HandleSkillEvent(const FSkillLabEventData& Event)
{
	switch (Event.EventType)
	{
	case ESkillLabEventType::RequestStarted:
		ActiveCastId = Event.CastId;
		bHasActiveCast = Event.CastId != 0;
		break;
	case ESkillLabEventType::Completed:
	case ESkillLabEventType::Cancelled:
	case ESkillLabEventType::Interrupted:
	case ESkillLabEventType::ResetStarted:
	case ESkillLabEventType::ResetCompleted:
		ClearActivePresentation();
		break;
	default:
		break;
	}
}
