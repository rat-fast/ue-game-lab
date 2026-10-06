#include "SkillLabAttributeSet.h"

USkillLabAttributeSet::USkillLabAttributeSet()
{
	TestValue = GetDefaultTestValue();
	SkillEnergy = GetDefaultSkillEnergy();
}

float USkillLabAttributeSet::GetDefaultTestValue()
{
	return 0.0f;
}

float USkillLabAttributeSet::GetDefaultSkillEnergy()
{
	return 100.0f;
}
