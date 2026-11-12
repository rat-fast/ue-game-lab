#if WITH_DEV_AUTOMATION_TESTS

#include "SkillLabCastLifecycle.h"
#include "Misc/AutomationTest.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSkillLabCastLifecycleTest,
	"SkillLab.Skills.CastLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabCastLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FSkillLabCastLifecycle Lifecycle;
	FSkillLabCastLifecycle::FConfig Config;
	Config.WindupSeconds = 0.25;
	Config.ActiveSeconds = 1.0;
	Config.RecoverySeconds = 0.5;

	TestTrue(TEXT("Begin accepts a new cast"), Lifecycle.Begin(1001, Config, 10.0));
	const FSkillLabCastPhaseToken Token = Lifecycle.GetPhaseToken();
	const FSkillLabCastToken CastToken = Lifecycle.GetCastToken();
	TestFalse(TEXT("Zero window identity is invalid"), Lifecycle.OpenWindow(CastToken, 0));
	TestTrue(TEXT("A cast can open a window"), Lifecycle.OpenWindow(CastToken, 7));
	TestFalse(TEXT("Duplicate open has no effect"), Lifecycle.OpenWindow(CastToken, 7));
	TestEqual(TEXT("Begin starts in Windup"), Lifecycle.GetStage(), FSkillLabCastLifecycle::EStage::Windup);
	TestEqual(TEXT("Early timer does not skip Windup"), Lifecycle.AdvanceIfElapsed(Token, 10.1), FSkillLabCastLifecycle::EAdvanceResult::NoChange);
	TestEqual(TEXT("Timer enters Active"), Lifecycle.AdvanceIfElapsed(Token, 10.25), FSkillLabCastLifecycle::EAdvanceResult::EnteredActive);
	const FSkillLabCastPhaseToken ActiveToken = Lifecycle.GetPhaseToken();
	TestTrue(TEXT("Window remains open across phase transition"), Lifecycle.IsWindowOpen(CastToken, 7));
	TestTrue(TEXT("Current cast can close window"), Lifecycle.CloseWindow(CastToken, 7));
	TestFalse(TEXT("Duplicate close has no effect"), Lifecycle.CloseWindow(CastToken, 7));
	TestTrue(TEXT("Window can be opened again"), Lifecycle.OpenWindow(CastToken, 7));
	TestEqual(TEXT("Old phase callback is rejected"), Lifecycle.AdvancePhase(Token, 10.3), FSkillLabCastLifecycle::EAdvanceResult::InvalidToken);
	TestEqual(TEXT("Time cannot move backwards"), Lifecycle.AdvancePhase(ActiveToken, 10.2), FSkillLabCastLifecycle::EAdvanceResult::InvalidToken);
	TestFalse(TEXT("Active cannot skip configured Recovery"), Lifecycle.Finish(ActiveToken, 10.3));
	TestTrue(TEXT("Release is one-shot"), Lifecycle.TryMarkRelease(ActiveToken));
	TestFalse(TEXT("Second release is rejected"), Lifecycle.TryMarkRelease(ActiveToken));
	TestTrue(TEXT("First submission point commits"), Lifecycle.TryMarkSubmissionPoint(ActiveToken, 11));
	TestFalse(TEXT("Repeated submission point is idempotently rejected"), Lifecycle.TryMarkSubmissionPoint(ActiveToken, 11));
	TestTrue(TEXT("Different submission point has independent identity"), Lifecycle.TryMarkSubmissionPoint(ActiveToken, 12));
	TestTrue(TEXT("Attempt reserves a point"), Lifecycle.BeginSubmission(CastToken, 20));
	TestFalse(TEXT("Pending attempt rejects synchronous reentry"), Lifecycle.BeginSubmission(CastToken, 20));
	TestFalse(TEXT("Legacy marking cannot bypass a pending attempt"), Lifecycle.TryMarkSubmissionPoint(ActiveToken, 20));
	TestTrue(TEXT("Failed attempt releases its reservation"), Lifecycle.ResolveSubmission(CastToken, 20, false));
	TestTrue(TEXT("Failed point can be retried"), Lifecycle.BeginSubmission(CastToken, 20));
	TestTrue(TEXT("Successful attempt commits its identity"), Lifecycle.ResolveSubmission(CastToken, 20, true));
	TestFalse(TEXT("Successful point cannot be retried"), Lifecycle.BeginSubmission(CastToken, 20));
	TestEqual(TEXT("Explicit phase event enters Recovery"), Lifecycle.AdvancePhase(ActiveToken, 10.3), FSkillLabCastLifecycle::EAdvanceResult::EnteredRecovery);
	const FSkillLabCastPhaseToken RecoveryToken = Lifecycle.GetPhaseToken();
	TestFalse(TEXT("Active cast cannot Begin again"), Lifecycle.Begin(1001, Config, 20.0));
	TestEqual(TEXT("Recovery timer finishes"), Lifecycle.AdvanceIfElapsed(RecoveryToken, 10.8), FSkillLabCastLifecycle::EAdvanceResult::Finished);
	TestFalse(TEXT("Terminal cast no longer exposes windows"), Lifecycle.IsWindowOpen(CastToken, 7));
	TestTrue(TEXT("Old token cannot mutate a new cast"), Lifecycle.Begin(1001, Config, 20.0));
	const FSkillLabCastToken NewCastToken = Lifecycle.GetCastToken();
	TestFalse(TEXT("Previous cast cannot open a window on reused CastId"), Lifecycle.OpenWindow(CastToken, 7));
	TestFalse(TEXT("Windows are cleared before the next cast"), Lifecycle.IsWindowOpen(NewCastToken, 7));
	TestTrue(TEXT("New cast can open a window"), Lifecycle.OpenWindow(NewCastToken, 7));
	TestFalse(TEXT("Previous cast cannot close the new window"), Lifecycle.CloseWindow(CastToken, 7));
	TestFalse(TEXT("Stale callback is rejected"), Lifecycle.Cancel(Token, 20.0));

	const FSkillLabCastPhaseToken NewToken = Lifecycle.GetPhaseToken();
	TestTrue(TEXT("New token can be cancelled"), Lifecycle.Cancel(NewToken, 20.1));
	TestFalse(TEXT("Cancellation clears window access"), Lifecycle.IsWindowOpen(NewCastToken, 7));
	TestFalse(TEXT("Cancelled cast cannot release"), Lifecycle.TryMarkRelease(NewToken));
	TestFalse(TEXT("Cancelled cast cannot commit a submission point"), Lifecycle.TryMarkSubmissionPoint(NewToken, 11));
	TestFalse(TEXT("Cancelled cast cannot be completed"), Lifecycle.Finish(NewToken, 20.2));
	TestFalse(TEXT("Cancellation is terminal"), Lifecycle.Cancel(NewToken, 20.2));

	FSkillLabCastLifecycle::FConfig InvalidConfig;
	InvalidConfig.WindupSeconds = -1.0;
	TestFalse(TEXT("Negative durations are rejected"), Lifecycle.Begin(1002, InvalidConfig, 30.0));
	InvalidConfig.WindupSeconds = std::numeric_limits<double>::infinity();
	TestFalse(TEXT("Infinite durations are rejected"), Lifecycle.Begin(1002, InvalidConfig, 30.0));

	FSkillLabCastLifecycle::FConfig EventConfig;
	EventConfig.bUseActiveTimeout = false;
	TestTrue(TEXT("Event-driven zero-windup cast begins"), Lifecycle.Begin(1002, EventConfig, 30.0));
	TestEqual(TEXT("Zero windup still emits Active entry"), Lifecycle.AdvanceIfElapsed(Lifecycle.GetPhaseToken(), 30.0), FSkillLabCastLifecycle::EAdvanceResult::EnteredActive);
	const FSkillLabCastPhaseToken EventToken = Lifecycle.GetPhaseToken();
	TestTrue(TEXT("A new Cast can reuse a submission identity"), Lifecycle.TryMarkSubmissionPoint(EventToken, 12));
	TestEqual(TEXT("Active waits for behaviour event"), Lifecycle.AdvanceIfElapsed(EventToken, 40.0), FSkillLabCastLifecycle::EAdvanceResult::NoChange);
	TestTrue(TEXT("Behaviour can release after waiting"), Lifecycle.TryMarkRelease(EventToken));
	TestTrue(TEXT("Behaviour can finish Active without Recovery"), Lifecycle.Finish(EventToken, 40.0));
	return true;
}

#endif
