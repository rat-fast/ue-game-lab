#include "SkillLabSkillLoadout.h"

#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillTypes.h"

bool USkillLabSkillLoadout::Initialize(USkillLabAbilitySystemComponent* InAbilitySystemComponent)
{
	if (!InAbilitySystemComponent)
	{
		return false;
	}

	if (AbilitySystemComponent.IsValid() && AbilitySystemComponent.Get() != InAbilitySystemComponent)
	{
		return false;
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	if (GrantedSkills.Num() > 0)
	{
		return true;
	}

	TArray<FName> SkillIds;
	FSkillLabSkillDefinitionProvider::GetDefaultSkillIds(SkillIds);
	for (const FName SkillId : SkillIds)
	{
		FSkillLabSkillDefinition Definition;
		if (FSkillLabSkillDefinitionProvider::Resolve(SkillId, Definition) && Definition.AbilityClass)
		{
			GrantedSkills.Add(SkillId, AbilitySystemComponent->GiveAbility(FGameplayAbilitySpec(Definition.AbilityClass, 1)));
		}
	}
	return GrantedSkills.Num() > 0;
}

bool USkillLabSkillLoadout::IsGranted(const FName SkillId) const
{
	return GrantedSkills.Contains(SkillId);
}

FGameplayAbilitySpecHandle USkillLabSkillLoadout::FindGrantedHandle(const FName SkillId) const
{
	if (const FGameplayAbilitySpecHandle* Handle = GrantedSkills.Find(SkillId))
	{
		return *Handle;
	}
	return FGameplayAbilitySpecHandle();
}

void USkillLabSkillLoadout::GetGrantedSkillIds(TArray<FName>& OutIds) const
{
	GrantedSkills.GetKeys(OutIds);
	OutIds.Sort(FNameLexicalLess());
}
