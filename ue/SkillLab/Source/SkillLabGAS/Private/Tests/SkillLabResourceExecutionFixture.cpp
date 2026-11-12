#include "SkillLabResourceExecutionFixture.h"
#include "SkillLabCombatAttributeSet.h"
#include "GameplayEffect.h"

int32 USkillLabResourceExecutionFixture::ExecutionCount = 0;

void USkillLabResourceExecutionFixture::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& Parameters, FGameplayEffectCustomExecutionOutput& Output) const
{
	++ExecutionCount;
	const int32 Mode = FMath::RoundToInt(Parameters.GetOwningSpec().GetLevel());
	const auto Health = FGameplayModifierEvaluatedData(USkillLabCombatAttributeSet::GetHealthAttribute(), EGameplayModOp::Additive, -5.0f);
	const auto Energy = FGameplayModifierEvaluatedData(Mode == 5 ? USkillLabCombatAttributeSet::GetMaxEnergyAttribute() : USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Additive, -10.0f);
	if (Mode == 1 || Mode == 4 || Mode == 6) Output.AddOutputModifier(Health);
	if (Mode != 1) Output.AddOutputModifier(Energy);
	if (Mode == 3) Output.AddOutputModifier(Health);
}
