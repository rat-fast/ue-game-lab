#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h"
#include "SkillLabSkillLoadout.generated.h"

class USkillLabAbilitySystemComponent;

/** Owns the relationship between a unit's skill definitions and granted GAS specs. */
UCLASS()
class SKILLLABSKILLS_API USkillLabSkillLoadout : public UObject
{
	GENERATED_BODY()

public:
	/** Grants the current default catalog once and preserves the existing handles. */
	bool Initialize(USkillLabAbilitySystemComponent* InAbilitySystemComponent);
	bool IsGranted(FName SkillId) const;
	FGameplayAbilitySpecHandle FindGrantedHandle(FName SkillId) const;
	void GetGrantedSkillIds(TArray<FName>& OutIds) const;
	int32 NumGrantedSkills() const { return GrantedSkills.Num(); }

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<USkillLabAbilitySystemComponent> AbilitySystemComponent;
	TMap<FName, FGameplayAbilitySpecHandle> GrantedSkills;
};
