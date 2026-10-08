#include "SkillLabHUDManagerComponent.h"

#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillTypes.h"

USkillLabHUDManagerComponent::USkillLabHUDManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillLabHUDManagerComponent::SetSkillRuntime(USkillLabSkillRuntimeComponent* InSkillRuntime)
{
	SkillRuntime = InSkillRuntime;
}

FSkillLabHUDSnapshot USkillLabHUDManagerComponent::BuildSnapshot() const
{
	FSkillLabHUDSnapshot Snapshot;
	if (!SkillRuntime) return Snapshot;

	Snapshot.Health = SkillRuntime->GetHealthValue();
	Snapshot.MaxHealth = SkillRuntime->GetMaxHealthValue();
	Snapshot.Energy = SkillRuntime->GetEnergyValue();
	Snapshot.MaxEnergy = SkillRuntime->GetMaxEnergyValue();

	TArray<FName> SkillIds;
	SkillRuntime->GetGrantedSkillIds(SkillIds);
	for (int32 Index = 0; Index < SkillIds.Num(); ++Index)
	{
		FSkillLabSkillDefinition Definition;
		if (!FSkillLabSkillDefinitionProvider::Resolve(SkillIds[Index], Definition)) continue;
		FSkillLabHUDSkillState& State = Snapshot.Skills.AddDefaulted_GetRef();
		State.SkillId = SkillIds[Index];
		State.SlotIndex = Index;
		State.Cost = Definition.Cost;
		State.CooldownRemaining = SkillRuntime->GetCooldownRemaining(SkillIds[Index]);
	}
	if (!bLoggedFirstSnapshot)
	{
		bLoggedFirstSnapshot = true;
		UE_LOG(LogTemp, Log, TEXT("[SkillLabHUD] Snapshot HP=%.0f/%.0f Energy=%.0f/%.0f Skills=%d"), Snapshot.Health, Snapshot.MaxHealth, Snapshot.Energy, Snapshot.MaxEnergy, Snapshot.Skills.Num());
	}
	return Snapshot;
}
