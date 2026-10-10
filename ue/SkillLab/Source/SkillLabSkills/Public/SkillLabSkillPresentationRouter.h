#pragma once

#include "CoreMinimal.h"
#include "SkillLabSkillTypes.h"
#include "SkillLabSkillPresentationRouter.generated.h"

class USkillLabSkillRuntimeComponent;

/** Native presentation subscription surface. Subscribers only receive runtime facts. */
using FSkillLabSkillPresentationEvent = FSkillLabNativeSkillEvent;

UCLASS()
class SKILLLABSKILLS_API USkillLabSkillPresentationRouter : public UObject
{
	GENERATED_BODY()
public:
	void Bind(USkillLabSkillRuntimeComponent* InSource);
	void HandleEvent(const FSkillLabSkillResult& Result);
	FSkillLabSkillPresentationEvent OnPresentationEvent;
	virtual UWorld* GetWorld() const override;
private:
	void ShowScreenMessage(const FSkillLabSkillResult& Result);
	void SpawnWorldMarker(const FVector& Location, float Radius, const FColor& Color, float LifeSeconds, bool bFlat);
	void SpawnTextMarker(const FVector& Location, const FString& Text, const FColor& Color, float LifeSeconds);

	UPROPERTY(Transient) TObjectPtr<USkillLabSkillRuntimeComponent> Source;
};
