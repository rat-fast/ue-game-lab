#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SkillLabHUDTypes.h"
#include "SkillLabHUDManagerComponent.generated.h"

class USkillLabSkillRuntimeComponent;
class USkillLabSkillShortcutComponent;
class USkillLabHUDWidget;
class APlayerController;
class APawn;

UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabHUDManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabHUDManagerComponent();
	void SetSkillRuntime(USkillLabSkillRuntimeComponent* InSkillRuntime);
	void SetShortcutComponent(USkillLabSkillShortcutComponent* InShortcuts);
	void SetControlledPawn(APawn* InPawn);
	void InitializeForPlayer(APlayerController* Player, APawn* Pawn, USkillLabSkillShortcutComponent* Shortcuts);
	void ShutdownForPlayer();
	FSkillLabHUDSnapshot BuildSnapshot() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditDefaultsOnly, Category="SkillLab|HUD")
	TSubclassOf<USkillLabHUDWidget> WidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<USkillLabHUDWidget> HUDWidget;
	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillRuntimeComponent> SkillRuntime;
	UPROPERTY(Transient)
	TObjectPtr<USkillLabSkillShortcutComponent> ShortcutComponent;
	TWeakObjectPtr<APlayerController> OwningPlayer;
	mutable bool bLoggedFirstSnapshot = false;
};
