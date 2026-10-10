#pragma once

#include <algorithm>
#include <cmath>

/** Pure settlement arithmetic shared by AttributeSet and standalone regression tests. */
namespace SkillLabDamageCalculation
{
	struct FResult
	{
		float HealthBefore = 0.0f;
		float HealthAfter = 0.0f;
		float RawDamage = 0.0f;
		float MitigatedDamage = 0.0f;
		float AppliedDamage = 0.0f;
	};

	inline FResult Resolve(float Health, float MaxHealth, float Armor, float RawDamage)
	{
		FResult Result;
		if (!std::isfinite(Health) || !std::isfinite(MaxHealth) || !std::isfinite(Armor) || !std::isfinite(RawDamage)) return Result;
		Result.HealthBefore = std::clamp(Health, 0.0f, std::max(0.0f, MaxHealth));
		Result.RawDamage = std::max(0.0f, RawDamage);
		Result.MitigatedDamage = std::max(0.0f, Result.RawDamage - std::max(0.0f, Armor));
		Result.HealthAfter = std::max(0.0f, Result.HealthBefore - Result.MitigatedDamage);
		Result.AppliedDamage = Result.HealthBefore - Result.HealthAfter;
		return Result;
	}
}
