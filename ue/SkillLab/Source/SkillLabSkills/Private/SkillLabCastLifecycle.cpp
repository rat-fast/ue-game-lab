#include "SkillLabCastLifecycle.h"

#include <cmath>

bool FSkillLabCastLifecycle::IsFiniteNonNegative(const double Value)
{
	return std::isfinite(Value) && Value >= 0.0;
}

bool FSkillLabCastLifecycle::IsTerminal() const
{
	return Stage == EStage::Completed || Stage == EStage::Cancelled;
}

bool FSkillLabCastLifecycle::Begin(const std::uint64_t NewCastId, const FConfig& NewConfig, const double NowSeconds)
{
	if (NewCastId == 0 || !IsFiniteNonNegative(NowSeconds) ||
		!IsFiniteNonNegative(NewConfig.WindupSeconds) ||
		!IsFiniteNonNegative(NewConfig.ActiveSeconds) ||
		!IsFiniteNonNegative(NewConfig.RecoverySeconds))
	{
		return false;
	}

	if (Stage != EStage::Idle && !IsTerminal())
	{
		return false;
	}

	Config = NewConfig;
	CastId = NewCastId;
	++Generation;
	if (Generation == 0)
	{
		// Extremely unlikely uint64 wrap; zero is reserved for an invalid token.
		++Generation;
	}
	PhaseToken = {CastId, Generation};
	StageStartedAt = NowSeconds;
	bHasReleased = false;
	Stage = EStage::Windup;
	return true;
}

bool FSkillLabCastLifecycle::IsTokenCurrent(const FSkillLabCastPhaseToken& Token) const
{
	return Token.IsValid() && Token == PhaseToken && !IsTerminal();
}

bool FSkillLabCastLifecycle::AdvanceTo(const EStage NewStage, const double NowSeconds)
{
	if (!IsFiniteNonNegative(NowSeconds) || NowSeconds < StageStartedAt)
	{
		return false;
	}
	Stage = NewStage;
	StageStartedAt = NowSeconds;
	++Generation;
	if (Generation == 0)
	{
		++Generation;
	}
	PhaseToken = {CastId, Generation};
	return true;
}

FSkillLabCastLifecycle::EAdvanceResult FSkillLabCastLifecycle::AdvancePhase(
	const FSkillLabCastPhaseToken& Token, const double NowSeconds)
{
	if (!IsTokenCurrent(Token) || !IsFiniteNonNegative(NowSeconds) || NowSeconds < StageStartedAt)
	{
		return EAdvanceResult::InvalidToken;
	}

	switch (Stage)
	{
	case EStage::Windup:
		return AdvanceTo(EStage::Active, NowSeconds)
			? EAdvanceResult::EnteredActive
			: EAdvanceResult::InvalidToken;
	case EStage::Active:
		return AdvanceTo(EStage::Recovery, NowSeconds)
			? EAdvanceResult::EnteredRecovery
			: EAdvanceResult::InvalidToken;
	case EStage::Recovery:
		return AdvanceTo(EStage::Completed, NowSeconds)
			? EAdvanceResult::Finished
			: EAdvanceResult::InvalidToken;
	default:
		return EAdvanceResult::NoChange;
	}
}

FSkillLabCastLifecycle::EAdvanceResult FSkillLabCastLifecycle::AdvanceTimedStage(
	const FSkillLabCastPhaseToken& Token, const double NowSeconds)
{
	if (!IsTokenCurrent(Token) || !IsFiniteNonNegative(NowSeconds) || NowSeconds < StageStartedAt)
	{
		return EAdvanceResult::InvalidToken;
	}

	const double Elapsed = NowSeconds - StageStartedAt;
	switch (Stage)
	{
	case EStage::Windup:
		if (Elapsed >= Config.WindupSeconds)
		{
			return AdvanceTo(EStage::Active, NowSeconds)
				? EAdvanceResult::EnteredActive
				: EAdvanceResult::InvalidToken;
		}
		return EAdvanceResult::NoChange;
	case EStage::Active:
		if (Config.bUseActiveTimeout && Config.ActiveSeconds > 0.0 && Elapsed >= Config.ActiveSeconds)
		{
			return AdvanceTo(EStage::Recovery, NowSeconds)
				? EAdvanceResult::EnteredRecovery
				: EAdvanceResult::InvalidToken;
		}
		return EAdvanceResult::NoChange;
	case EStage::Recovery:
		if (Elapsed >= Config.RecoverySeconds)
		{
			return AdvanceTo(EStage::Completed, NowSeconds)
				? EAdvanceResult::Finished
				: EAdvanceResult::InvalidToken;
		}
		return EAdvanceResult::NoChange;
	default:
		return EAdvanceResult::NoChange;
	}
}

FSkillLabCastLifecycle::EAdvanceResult FSkillLabCastLifecycle::AdvanceIfElapsed(
	const FSkillLabCastPhaseToken& Token, const double NowSeconds)
{
	return AdvanceTimedStage(Token, NowSeconds);
}

bool FSkillLabCastLifecycle::Cancel(const FSkillLabCastPhaseToken& Token, const double NowSeconds)
{
	if (!IsTokenCurrent(Token) || !IsFiniteNonNegative(NowSeconds))
	{
		return false;
	}
	return AdvanceTo(EStage::Cancelled, NowSeconds);
}

bool FSkillLabCastLifecycle::Finish(const FSkillLabCastPhaseToken& Token, const double NowSeconds)
{
	if (!IsTokenCurrent(Token) || !IsFiniteNonNegative(NowSeconds))
	{
		return false;
	}

	// A behaviour may finish from Active only when it explicitly has no recovery
	// phase. A configured recovery cannot be skipped by a caller.
	if (Stage == EStage::Active && Config.RecoverySeconds > 0.0)
	{
		return false;
	}
	if (Stage != EStage::Active && Stage != EStage::Recovery)
	{
		return false;
	}
	return AdvanceTo(EStage::Completed, NowSeconds);
}

bool FSkillLabCastLifecycle::TryMarkRelease(const FSkillLabCastPhaseToken& Token)
{
	if (!IsTokenCurrent(Token) || Stage != EStage::Active || bHasReleased)
	{
		return false;
	}
	bHasReleased = true;
	return true;
}

FSkillLabCastLifecycle::FSnapshot FSkillLabCastLifecycle::GetSnapshot() const
{
	FSnapshot Snapshot;
	Snapshot.CastId = CastId;
	Snapshot.Stage = Stage;
	Snapshot.StageStartedAt = StageStartedAt;
	Snapshot.PhaseToken = PhaseToken;
	Snapshot.bHasReleased = bHasReleased;
	Snapshot.bFinished = Stage == EStage::Completed || Stage == EStage::Cancelled;
	return Snapshot;
}
