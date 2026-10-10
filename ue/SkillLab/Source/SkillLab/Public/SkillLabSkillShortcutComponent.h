#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "SkillLabSkillShortcutComponent.generated.h"

class UInputComponent;

/** The single source for skill bar ordering, activation keys and HUD labels. */
USTRUCT(BlueprintType)
struct SKILLLAB_API FSkillLabSkillShortcut
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SkillId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FKey Key;
};

UCLASS(ClassGroup=(SkillLab), meta=(BlueprintSpawnableComponent))
class SKILLLAB_API USkillLabSkillShortcutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillLabSkillShortcutComponent();
	void BindInput(UInputComponent* Input);
	void ActivateSlot(int32 SlotIndex);
	/** Invalid/duplicate entries are omitted by both input and HUD. */
	TArray<FSkillLabSkillShortcut> GetShortcuts() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UnbindInput();
	void HandleShortcutPressed(FKey Key);
	void ConfirmTarget();
	void CancelTarget();

	/** This demo loadout can be replaced in a controller Blueprint without code changes. */
	UPROPERTY(EditDefaultsOnly, Category="SkillLab|Skill Bar")
	TArray<FSkillLabSkillShortcut> Shortcuts;

	UPROPERTY(EditDefaultsOnly, Category="SkillLab|Skill Bar") FKey ConfirmKey;
	UPROPERTY(EditDefaultsOnly, Category="SkillLab|Skill Bar") FKey CancelKey;
	TWeakObjectPtr<UInputComponent> BoundInput;
};
