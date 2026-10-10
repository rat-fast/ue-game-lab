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
	TestEqual(TEXT("Begin starts in Windup"), Lifecycle.GetStage(), FSkillLabCastLifecycle::EStage::Windup);
	TestEqual(TEXT("Early timer does not skip Windup"), Lifecycle.AdvanceIfElapsed(Token, 10.1), FSkillLabCastLifecycle::EAdvanceResult::NoChange);
	TestEqual(TEXT("Timer enters Active"), Lifecycle.AdvanceIfElapsed(Token, 10.25), FSkillLabCastLifecycle::EAdvanceResult::EnteredActive);
	const FSkillLabCastPhaseToken ActiveToken = Lifecycle.GetPhaseToken();
	TestEqual(TEXT("Old phase callback is rejected"), Lifecycle.AdvancePhase(Token, 10.3), FSkillLabCastLifecycle::EAdvanceResult::InvalidToken);
	TestEqual(TEXT("Time cannot move backwards"), Lifecycle.AdvancePhase(ActiveToken, 10.2), FSkillLabCastLifecycle::EAdvanceResult::InvalidToken);
	TestFalse(TEXT("Active cannot skip configured Recovery"), Lifecycle.Finish(ActiveToken, 10.3));
	TestTrue(TEXT("Release is one-shot"), Lifecycle.TryMarkRelease(ActiveToken));
	TestFalse(TEXT("Second release is rejected"), Lifecycle.TryMarkRelease(ActiveToken));
	TestEqual(TEXT("Explicit phase event enters Recovery"), Lifecycle.AdvancePhase(ActiveToken, 10.3), FSkillLabCastLifecycle::EAdvanceResult::EnteredRecovery);
	const FSkillLabCastPhaseToken RecoveryToken = Lifecycle.GetPhaseToken();
	TestFalse(TEXT("Active cast cannot Begin again"), Lifecycle.Begin(1001, Config, 20.0));
	TestEqual(TEXT("Recovery timer finishes"), Lifecycle.AdvanceIfElapsed(RecoveryToken, 10.8), FSkillLabCastLifecycle::EAdvanceResult::Finished);
	TestTrue(TEXT("Old token cannot mutate a new cast"), Lifecycle.Begin(1001, Config, 20.0));
	TestFalse(TEXT("Stale callback is rejected"), Lifecycle.Cancel(Token, 20.0));

	const FSkillLabCastPhaseToken NewToken = Lifecycle.GetPhaseToken();
	TestTrue(TEXT("New token can be cancelled"), Lifecycle.Cancel(NewToken, 20.1));
	TestFalse(TEXT("Cancelled cast cannot release"), Lifecycle.TryMarkRelease(NewToken));
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
	TestEqual(TEXT("Active waits for behaviour event"), Lifecycle.AdvanceIfElapsed(EventToken, 40.0), FSkillLabCastLifecycle::EAdvanceResult::NoChange);
	TestTrue(TEXT("Behaviour can release after waiting"), Lifecycle.TryMarkRelease(EventToken));
	TestTrue(TEXT("Behaviour can finish Active without Recovery"), Lifecycle.Finish(EventToken, 40.0));
	return true;
}

#endif
