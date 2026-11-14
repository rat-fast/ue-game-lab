#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastControlTransitionsTest, "SkillLab.Skills.CastControlTransitions",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastControlTransitionsTest::RunTest(const FString&)
{
	FSkillLabCastScenarioFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	const auto Definition = Fixture.FixtureDefinitions.FindChecked(TEXT("Skill_Mend"));
	auto Request = [&]() { return Fixture.Submit(Unit, Definition.SkillId); };
	const float InitialEnergy = Runtime->GetEnergyValue();
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Stunned);
	const FSkillLabSkillResult StunnedRequest = Request();
	TestFalse(TEXT("Stunned state blocks formal activation"), StunnedRequest.bSuccess);
	TestEqual(TEXT("Stunned activation refusal reports state gate"), StunnedRequest.Reason, FName(TEXT("BlockedByControlState")));
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Stunned);
	const FSkillLabSkillResult StunnedCast = Request();
	TestTrue(TEXT("Cast activates after state removal"), StunnedCast.bSuccess);
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Silenced);
	TestEqual(TEXT("Silenced state interrupts current Cast"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Silenced);

	const FSkillLabSkillResult DodgeVictim = Request();
	TestTrue(TEXT("Cast starts before generic dodge operation"), DodgeVictim.bSuccess);
	FSkillLabSkillRequest DodgeRequest;
	DodgeRequest.Caster = Owner;
	DodgeRequest.SkillId = Definition.SkillId;
	const FSkillLabSkillResult DodgeActivation = Runtime->SubmitSkillOperationRequest(DodgeRequest, ESkillLabOperationCategory::Dodge);
	TestTrue(TEXT("Generic dodge operation immediately uses formal ability activation"), DodgeActivation.bSuccess);
	TestTrue(TEXT("Successful dodge operation gets a fresh Cast identity"), DodgeActivation.CastId > DodgeVictim.CastId);
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	const FSkillLabSkillResult FailedDodgeVictim = Request();
	TestTrue(TEXT("Another Cast starts before failing dodge operation"), FailedDodgeVictim.bSuccess);
	ASC->RequestResourceChange(false, EGameplayModOp::Override, 0.0f);
	const FSkillLabSkillResult FailedDodge = Runtime->SubmitSkillOperationRequest(DodgeRequest, ESkillLabOperationCategory::Dodge);
	TestFalse(TEXT("Dodge still obeys formal resource gate"), FailedDodge.bSuccess);
	TestEqual(TEXT("Dodge reports formal resource rejection"), FailedDodge.Reason, FName(TEXT("InsufficientEnergy")));
	TestEqual(TEXT("Failed dodge does not restore interrupted Cast"), Runtime->GetActiveCastId(), 0);
	ASC->RequestResourceChange(false, EGameplayModOp::Override, InitialEnergy);

	return true;
}
#endif
