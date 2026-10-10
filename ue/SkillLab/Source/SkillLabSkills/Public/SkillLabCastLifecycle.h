#pragma once

#include <cstdint>

// Standalone lifecycle tests do not receive UnrealBuildTool's module export
// definition. In a UE build the generated SKILLLABSKILLS_API macro wins.
#ifndef SKILLLABSKILLS_API
#define SKILLLABSKILLS_API
#endif

/**
 * Token identifying one cast and one generation of its phase callbacks.
 * Runtime and presentation callbacks should retain the token they received
 * and pass it back before changing lifecycle state.  A new Begin always
 * creates a new generation, so delayed callbacks from an older cast are
 * rejected even when the caller reuses the same CastId.
 */
struct SKILLLABSKILLS_API FSkillLabCastPhaseToken
{
	std::uint64_t CastId = 0;
	std::uint64_t Generation = 0;

	bool IsValid() const { return CastId != 0 && Generation != 0; }
	friend bool operator==(const FSkillLabCastPhaseToken& A, const FSkillLabCastPhaseToken& B)
	{
		return A.CastId == B.CastId && A.Generation == B.Generation;
	}
	friend bool operator!=(const FSkillLabCastPhaseToken& A, const FSkillLabCastPhaseToken& B)
	{
		return !(A == B);
	}
};

/**
 * Small, UObject-free cast state machine shared by skill behaviours.
 *
 * This class owns lifecycle bookkeeping only.  It does not apply damage,
 * spawn a projectile, select targets, or decide when a hit happens.  A
 * behaviour may remain in Active while it performs multiple hit checks; the
 * one-shot release gate is separate from per-hit resolution.
 */
class SKILLLABSKILLS_API FSkillLabCastLifecycle
{
public:
	enum class EStage : std::uint8_t
	{
		Idle,
		Windup,
		Active,
		Recovery,
		Completed,
		Cancelled
	};

	enum class EAdvanceResult : std::uint8_t
	{
		InvalidToken,
		NoChange,
		EnteredActive,
		EnteredRecovery,
		Finished
	};

	struct FConfig
	{
		/** Time before Active. A zero value is valid and can be advanced explicitly. */
		double WindupSeconds = 0.0;
		/** Optional Active timeout; zero disables it. Never an effect/release point. */
		double ActiveSeconds = 0.0;
		/** Time in Recovery before automatic completion when timer-driven. */
		double RecoverySeconds = 0.0;
		/** If false, Active waits for the behaviour to call AdvancePhase. */
		bool bUseActiveTimeout = true;
	};

	struct FSnapshot
	{
		std::uint64_t CastId = 0;
		EStage Stage = EStage::Idle;
		double StageStartedAt = 0.0;
		FSkillLabCastPhaseToken PhaseToken;
		bool bHasReleased = false;
		bool bFinished = false;
	};

	/** Starts a new cast. Terminal casts may be reused; active casts cannot. */
	bool Begin(std::uint64_t CastId, const FConfig& Config, double NowSeconds);

	/**
	 * Advances exactly one lifecycle phase in response to an animation or
	 * behaviour event. It never applies an effect and never skips phases.
	 */
	EAdvanceResult AdvancePhase(const FSkillLabCastPhaseToken& Token, double NowSeconds);

	/**
	 * Advances at most one phase when the configured timer has elapsed. This
	 * helper is optional; behaviours with event-driven timing can ignore it.
	 */
	EAdvanceResult AdvanceIfElapsed(const FSkillLabCastPhaseToken& Token, double NowSeconds);

	/** Cancels an active cast. A cancelled cast cannot release or finish later. */
	bool Cancel(const FSkillLabCastPhaseToken& Token, double NowSeconds);

	/** Finishes Recovery (or an Active cast that has no recovery) explicitly. */
	bool Finish(const FSkillLabCastPhaseToken& Token, double NowSeconds);

	/**
	 * Marks the behaviour's one-shot release event. A release is not a hit:
	 * projectile and area behaviours may resolve any number of hits afterwards.
	 */
	bool TryMarkRelease(const FSkillLabCastPhaseToken& Token);

	bool IsTokenCurrent(const FSkillLabCastPhaseToken& Token) const;
	EStage GetStage() const { return Stage; }
	const FSkillLabCastPhaseToken& GetPhaseToken() const { return PhaseToken; }
	FSnapshot GetSnapshot() const;

private:
	static bool IsFiniteNonNegative(double Value);
	bool IsTerminal() const;
	bool AdvanceTo(EStage NewStage, double NowSeconds);
	EAdvanceResult AdvanceTimedStage(const FSkillLabCastPhaseToken& Token, double NowSeconds);

	FConfig Config;
	EStage Stage = EStage::Idle;
	std::uint64_t CastId = 0;
	std::uint64_t Generation = 0;
	double StageStartedAt = 0.0;
	bool bHasReleased = false;
	FSkillLabCastPhaseToken PhaseToken;
};
