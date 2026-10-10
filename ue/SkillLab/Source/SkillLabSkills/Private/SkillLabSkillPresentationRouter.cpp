#include "SkillLabSkillPresentationRouter.h"

#include "Engine/Engine.h"
#include "SkillLabSkillFeedbackActor.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillTypes.h"

UWorld* USkillLabSkillPresentationRouter::GetWorld() const
{
	return Source ? Source->GetWorld() : nullptr;
}

void USkillLabSkillPresentationRouter::Bind(USkillLabSkillRuntimeComponent* InSource)
{
	if (Source == InSource) return;
	if (Source) Source->OnNativeSkillEvent.RemoveAll(this);
	Source = InSource;
	if (Source) Source->OnNativeSkillEvent.AddUObject(this, &USkillLabSkillPresentationRouter::HandleEvent);
}

void USkillLabSkillPresentationRouter::HandleEvent(const FSkillLabSkillResult& Result)
{
	UWorld* World = GetWorld();
	if (!World) return;
	ShowScreenMessage(Result);
	if (Result.Event == ESkillLabSkillEvent::TargetSelected && Result.VisualRadius > 0.0f)
	{
		SpawnWorldMarker(Result.TargetLocation + FVector(0.0f, 0.0f, 3.0f), Result.VisualRadius, FColor::Orange, 1.5f, true);
	}
	else if ((Result.Event == ESkillLabSkillEvent::Hit || Result.Event == ESkillLabSkillEvent::DamageTick)
		&& Result.bSuccess && Result.SourceActor && Result.TargetActor)
	{
		const FColor Color = Result.Effect == ESkillLabEffectKind::Healing ? FColor::Green : FColor::Red;
		// A short-lived white target frame makes the final hit location legible even with no assets.
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 76.0f, FColor::White, 0.28f, false);
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 70.0f, Color, 0.65f, false);
		const FString Label = Result.Effect == ESkillLabEffectKind::Healing
			? (Result.ActualAmount > 0.0f
				? FString::Printf(TEXT("HEAL +%.0f"), Result.ActualAmount)
				: FString(TEXT("HEAL FULL")))
			: FString::Printf(TEXT("DAMAGE -%.0f"), Result.ActualAmount);
		SpawnTextMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 190.0f), Label, Color,
			Result.Event == ESkillLabSkillEvent::DamageTick ? 0.9f : 1.5f);
	}
	else if (Result.Event == ESkillLabSkillEvent::BuffApplied && Result.TargetActor)
	{
		SpawnWorldMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 125.0f), 90.0f, FColor::Purple, 4.0f, false);
		SpawnTextMarker(Result.TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 205.0f), TEXT("BURNING"), FColor::Purple, 2.0f);
	}
}

void USkillLabSkillPresentationRouter::ShowScreenMessage(const FSkillLabSkillResult& Result)
{
	if (!GEngine) return;
	FColor Color = FColor::White;
	FString Message;
	switch (Result.Event)
	{
	case ESkillLabSkillEvent::Started:
		Color = FColor::Yellow;
		Message = FString::Printf(TEXT("CAST  %s"), *Result.SkillId.ToString());
		break;
	case ESkillLabSkillEvent::Hit:
		Color = Result.bSuccess ? FColor::Green : FColor::Red;
		Message = Result.bSuccess
			? FString::Printf(TEXT("HIT  %s  %s%.0f"), *Result.SkillId.ToString(), Result.Effect == ESkillLabEffectKind::Healing ? TEXT("+") : TEXT("-"), Result.ActualAmount)
			: FString::Printf(TEXT("MISS  %s  %s"), *Result.SkillId.ToString(), *Result.Reason.ToString());
		break;
	case ESkillLabSkillEvent::DamageTick:
		Color = Result.bSuccess ? FColor::Orange : FColor::Red;
		Message = Result.bSuccess
			? FString::Printf(TEXT("TICK  %s  -%.0f"), *Result.SkillId.ToString(), Result.ActualAmount)
			: FString::Printf(TEXT("TICK  %s  %s"), *Result.SkillId.ToString(), *Result.Reason.ToString());
		break;
	case ESkillLabSkillEvent::BuffApplied:
		Color = FColor::Purple;
		Message = FString::Printf(TEXT("BUFF  %s"), *Result.SkillId.ToString());
		break;
	case ESkillLabSkillEvent::Rejected:
		Color = FColor::Red;
		Message = FString::Printf(TEXT("REJECTED  %s"), *Result.Reason.ToString());
		break;
	default:
		break;
	}
	if (!Message.IsEmpty()) GEngine->AddOnScreenDebugMessage(-1, 2.0f, Color, Message);
}

void USkillLabSkillPresentationRouter::SpawnWorldMarker(const FVector& Location, const float Radius, const FColor& Color, const float LifeSeconds, const bool bFlat)
{
	UWorld* World = GetWorld();
	if (!World) return;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkillLabSkillFeedbackActor* Marker = World->SpawnActor<ASkillLabSkillFeedbackActor>(ASkillLabSkillFeedbackActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
	if (Marker)
	{
		const ESkillLabFeedbackShape Shape = bFlat
			? ESkillLabFeedbackShape::Ring
			: (Color == FColor::White ? ESkillLabFeedbackShape::TargetFrame : ESkillLabFeedbackShape::Target);
		Marker->InitializeShape(Shape, Radius, Color, LifeSeconds);
	}
}

void USkillLabSkillPresentationRouter::SpawnTextMarker(const FVector& Location, const FString& Text, const FColor& Color, const float LifeSeconds)
{
	UWorld* World = GetWorld();
	if (!World) return;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASkillLabSkillFeedbackActor* Marker = World->SpawnActor<ASkillLabSkillFeedbackActor>(ASkillLabSkillFeedbackActor::StaticClass(), Location, FRotator::ZeroRotator, SpawnParameters);
	if (Marker) Marker->InitializeText(Text, Color, LifeSeconds);
}
