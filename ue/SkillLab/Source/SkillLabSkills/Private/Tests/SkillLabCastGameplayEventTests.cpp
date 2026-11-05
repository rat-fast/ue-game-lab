#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "TimerManager.h"

class FSkillLabBufferedIntentConsumeCommand final : public IAutomationLatentCommand
{
public:
	FSkillLabBufferedIntentConsumeCommand(FAutomationTestBase* InTest, USkillLabSkillRuntimeComponent* InRuntime,
		const int32 InOldCastId, const int32 InBufferedRequestId, AActor* InAttacker, const float InInitialEnergy,
		const TMap<FName, FSkillLabSkillDefinition>& InPreviousDefinitions, const bool bInPreviousLoadAttempted)
		: Test(InTest), Runtime(InRuntime), OldCastId(InOldCastId), BufferedRequestId(InBufferedRequestId),
		Attacker(InAttacker), InitialEnergy(InInitialEnergy), PreviousDefinitions(InPreviousDefinitions),
		bPreviousLoadAttempted(bInPreviousLoadAttempted)
	{
		if (Runtime.IsValid())
		{
			RejectHandle = Runtime->OnNativeSkillEvent.AddLambda([this](const FSkillLabSkillResult& Result)
			{
				if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("InsufficientEnergy"))
				{
					bObservedGateReject = true;
					ObservedGateRejectReason = Result.Reason;
				}
			});
		}
	}

	virtual ~FSkillLabBufferedIntentConsumeCommand() override
	{
		if (Runtime.IsValid() && RejectHandle.IsValid()) Runtime->OnNativeSkillEvent.Remove(RejectHandle);
		FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(PreviousDefinitions, bPreviousLoadAttempted);
	}

	virtual bool Update() override
	{
		if (!Runtime.IsValid())
		{
			Test->TestNotNull(TEXT("Runtime remains valid while consuming buffered intent"), Runtime.Get());
			return true;
		}
		if (UWorld* World = Runtime->GetWorld())
		{
			World->GetTimerManager().Tick(1.0f / 60.0f);
		}
		if (bAwaitingInitialConsumption && (Runtime->ActiveCastId == OldCastId || Runtime->PendingSkillIntent.IsSet()))
		{
			if (++WaitFrames <= 120) return false;
			Test->TestTrue(TEXT("Buffered intent consumption completes within the latent timeout"), false);
			return true;
		}

		if (bAwaitingInitialConsumption)
		{
			Test->TestFalse(TEXT("Natural-completion buffer ID is not reused as a new RequestId"),
				Runtime->ActiveRequest.RequestId == BufferedRequestId);
			Test->TestTrue(TEXT("Natural completion starts a fresh Cast after clearing the old Cast"), Runtime->ActiveCastId > OldCastId);
			Test->TestEqual(TEXT("Natural completion activates the buffered skill definition"), Runtime->ActiveRequest.SkillId,
				FName(TEXT("Skill_BufferedSuccess")));
			Test->TestTrue(TEXT("Fresh Cast has a live GAS Ability only after deferred consumption"), Runtime->ActiveAbility.IsValid()
				&& Runtime->ActiveAbility->bSkillActive && Runtime->ActiveAbility->BoundCastId == Runtime->ActiveCastId);
			Test->TestFalse(TEXT("Old Cast Notify cannot advance the consumed Cast"), Runtime->ForwardValidatedNotify(OldCastId, SkillLabGASTags::Event_Cast));
			bAwaitingInitialConsumption = false;
			WaitFrames = 0;

			Runtime->CancelActiveCast(TEXT("BufferedConsumptionTestCleanup"));
			FSkillLabSkillRequest FailedVictimRequest;
			FailedVictimRequest.Caster = Runtime->GetOwner();
			FailedVictimRequest.SkillId = TEXT("Skill_BufferedVictim");
			const FSkillLabSkillResult FailedVictim = Runtime->SubmitSkillRequest(FailedVictimRequest);
			Test->TestTrue(TEXT("Second natural-completion gate fixture starts formally"), FailedVictim.bSuccess);
			FSkillLabSkillRequest FailedBufferedRequest = FailedVictimRequest;
			FailedBufferedRequest.SkillId = TEXT("Skill_BufferedFail");
			const FSkillLabSkillResult FailedBuffered = Runtime->SubmitSkillRequest(FailedBufferedRequest);
			Test->TestEqual(TEXT("Second intent is buffered during Cast"), FailedBuffered.Reason, FName(TEXT("InputBufferedLatestWins")));
			Test->TestTrue(TEXT("Second fixture releases through the current Ability"),
				Runtime->ForwardValidatedNotify(FailedVictim.CastId, SkillLabGASTags::Event_Cast));
			Runtime->ASC->ApplyModToAttribute(USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Override, 0.0f);
			FailedOldCastId = FailedVictim.CastId;
			Runtime->FinishRecovery(Runtime->Lifecycle.GetPhaseToken());
			Test->TestEqual(TEXT("Buffered request does not activate while Recovery is ending"), Runtime->GetActiveCastId(), 0);
			Test->TestTrue(TEXT("Intent remains until deferred formal gates run"), Runtime->PendingSkillIntent.IsSet());
		}
		return CompleteAfterGateFailure();
	}

private:
	bool CompleteAfterGateFailure()
	{
		if (Runtime->ActiveCastId == FailedOldCastId || Runtime->PendingSkillIntent.IsSet())
		{
			if (++WaitFrames <= 120) return false;
			Test->TestTrue(TEXT("Buffered resource rejection completes within the latent timeout"), false);
			return true;
		}
		Test->TestEqual(TEXT("Failed buffered activation does not revive old Cast"), Runtime->GetActiveCastId(), 0);
		Test->TestFalse(TEXT("Failed buffered activation clears the slot"), Runtime->PendingSkillIntent.IsSet());
		Test->TestTrue(TEXT("Failed buffered activation reports the formal resource gate"), bObservedGateReject);
		Test->TestEqual(TEXT("Failed buffered activation exposes the exact resource rejection reason"),
			ObservedGateRejectReason, FName(TEXT("InsufficientEnergy")));
		Runtime->ResetSkills();
		Runtime->ASC->ApplyModToAttribute(USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Override,
			InitialEnergy);
		Test->TestEqual(TEXT("Reset restores the original energy state"), Runtime->GetEnergyValue(), InitialEnergy);
		if (Attacker.IsValid()) Attacker->Destroy();
		if (Runtime->GetOwner()) Runtime->GetOwner()->Destroy();
		return true;
	}

	FAutomationTestBase* Test;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	int32 OldCastId;
	int32 BufferedRequestId;
	TWeakObjectPtr<AActor> Attacker;
	float InitialEnergy;
	int32 WaitFrames = 0;
	FDelegateHandle RejectHandle;
	bool bObservedGateReject = false;
	FName ObservedGateRejectReason = NAME_None;
	bool bAwaitingInitialConsumption = true;
	int32 FailedOldCastId = 0;
	TMap<FName, FSkillLabSkillDefinition> PreviousDefinitions;
	bool bPreviousLoadAttempted = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastGameplayEventTest, "SkillLab.Skills.CastGameplayEvent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabCastGameplayEventTest::RunTest(const FString&)
{
	TMap<FName, FSkillLabSkillDefinition> PreviousDefinitions;
	const bool bPreviousLoadAttempted = FSkillLabSkillDefinitionProvider::CopyDefinitionsForAutomation(PreviousDefinitions);
	const FString FixtureJson = TEXT(R"({"skills":[{"id":"Skill_Mend","luaScript":"Skill_Mend.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedSuccess","luaScript":"Skill_BufferedSuccess.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedVictim","luaScript":"Skill_BufferedVictim.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedFail","luaScript":"Skill_BufferedFail.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3}]})");
	const FString FixtureLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
	TMap<FName, FSkillLabSkillDefinition> FixtureDefinitions;
	FString FixtureError;
	TMap<FString, FString> LuaSources;
	LuaSources.Add(TEXT("Skill_Mend.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedSuccess.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedVictim.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedFail.lua"), FixtureLua);
	bool bDefinitionsRestored = false;
	if (!TestTrue(TEXT("Gameplay Event test definition fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		FixtureJson, LuaSources, FixtureDefinitions, FixtureError)))
	{
		AddError(FixtureError);
		return false;
	}
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(FixtureDefinitions);
	ON_SCOPE_EXIT
	{
		if (!bDefinitionsRestored)
		{
			FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(PreviousDefinitions, bPreviousLoadAttempted);
		}
	};

	UWorld* World = nullptr;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.World() && (Context.WorldType == EWorldType::Editor || Context.WorldType == EWorldType::Game
				|| Context.WorldType == EWorldType::PIE))
			{
				World = Context.World();
				break;
			}
		}
	}
	if (!TestNotNull(TEXT("Automation world exists"), World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = TEXT("SkillLabEventTestOwner");
	AActor* Owner = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	USkillLabAbilitySystemComponent* ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
	Owner->AddInstanceComponent(ASC);
	ASC->RegisterComponent();
	USkillLabSkillRuntimeComponent* Runtime = NewObject<USkillLabSkillRuntimeComponent>(Owner);
	Owner->AddInstanceComponent(Runtime);
	Runtime->RegisterComponent();
	Runtime->InitializeSkills();
	FSkillLabSkillDefinition Definition;
	if (!TestTrue(TEXT("Self-heal fixture resolves"), FSkillLabSkillDefinitionProvider::Resolve(TEXT("Skill_Mend"), Definition)))
	{
		Owner->Destroy();
		return false;
	}
	int32 ReleaseCount = 0;
	int32 EffectCount = 0;
	int32 StartedCount = 0;
	Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
	{
		if (Result.Event == ESkillLabSkillEvent::Started) ++StartedCount;
		if (Result.Event == ESkillLabSkillEvent::PhaseChanged && Result.Stage == ESkillLabCastStage::Active) ++ReleaseCount;
		if (Result.Event == ESkillLabSkillEvent::EffectApplied) ++EffectCount;
	});
	auto Request = [&]()
	{
		FSkillLabSkillRequest Value;
		Value.Caster = Owner;
		Value.SkillId = Definition.SkillId;
		return Runtime->SubmitSkillRequest(Value);
	};
	const float InitialEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult First = Request();
	TestTrue(TEXT("GAS Ability activation accepts the cast"), First.bSuccess);
	TestEqual(TEXT("No release before valid event"), ReleaseCount, 0);
	TestEqual(TEXT("No cost before valid event"), Runtime->GetEnergyValue(), InitialEnergy);
	FGameplayEventData BarePayload;
	BarePayload.EventTag = Definition.ReleaseEventTag;
	BarePayload.Instigator = Owner;
	BarePayload.EventMagnitude = static_cast<float>(First.CastId);
	ASC->HandleGameplayEvent(Definition.ReleaseEventTag, &BarePayload);
	TestEqual(TEXT("Bare ASC event cannot borrow current cast identity"), ReleaseCount, 0);
	TestFalse(TEXT("Wrong tag is rejected"), Runtime->ForwardValidatedNotify(First.CastId, FGameplayTag()));
	TestFalse(TEXT("Wrong cast is rejected"), Runtime->ForwardValidatedNotify(First.CastId + 1, Definition.ReleaseEventTag));
	TestTrue(TEXT("Validated Notify traverses ASC and current Ability"), Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag));
	TestEqual(TEXT("Current Ability advances exactly one release"), ReleaseCount, 1);
	TestEqual(TEXT("Authoritative effect is submitted"), EffectCount, 1);
	TestEqual(TEXT("Cost is committed once"), Runtime->GetEnergyValue(), InitialEnergy - Definition.Cost);
	Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag);
	TestEqual(TEXT("Duplicate event cannot release twice"), ReleaseCount, 1);
	TestEqual(TEXT("Duplicate event cannot submit effect twice"), EffectCount, 1);
	TestEqual(TEXT("Cast enters Recovery after successful behavior"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	const FSkillLabSkillResult RecoveryRequest = Request();
	TestFalse(TEXT("Recovery buffers but does not activate new skill"), RecoveryRequest.bSuccess);
	TestEqual(TEXT("Recovery request remains latest-wins buffered"), RecoveryRequest.Reason, FName(TEXT("InputBufferedLatestWins")));
	TestEqual(TEXT("Recovery buffer does not start another Cast"), Runtime->GetActiveCastId(), First.CastId);

	FActorSpawnParameters AttackerParameters;
	AttackerParameters.Name = TEXT("SkillLabCombatRulesAttacker");
	AActor* Attacker = World->SpawnActor<AActor>(AActor::StaticClass(), FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator, AttackerParameters);
	USkillLabAbilitySystemComponent* AttackerASC = NewObject<USkillLabAbilitySystemComponent>(Attacker);
	Attacker->AddInstanceComponent(AttackerASC);
	AttackerASC->RegisterComponent();
	AttackerASC->InitializeSkillLabAbilityActorInfo(Attacker, Attacker);
	AttackerASC->ApplyDamageToTarget(ASC, 1.0f, nullptr, SkillLabGASTags::Event_Attack_Normal);
	TestEqual(TEXT("Normal categorized hit does not interrupt"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	AttackerASC->ApplyDamageToTarget(ASC, 1.0f, nullptr, FGameplayTag());
	TestEqual(TEXT("Unclassified hit does not interrupt"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	AttackerASC->ApplyDamageToTarget(ASC, 1.0f, nullptr, SkillLabGASTags::Event_Attack_Heavy);
	TestEqual(TEXT("Heavy categorized hit interrupts Recovery Cast"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Post-commit hit interruption does not refund energy"), Runtime->GetEnergyValue(), InitialEnergy - Definition.Cost);

	Runtime->ResetSkills();
	TestFalse(TEXT("Reset clears cached intent"), Runtime->PendingSkillIntent.IsSet());
	const FSkillLabSkillResult Second = Request();
	TestTrue(TEXT("New cast activates after Reset"), Second.bSuccess);
	TestTrue(TEXT("Cast identity is monotonic across Reset"), Second.CastId > First.CastId);
	TestFalse(TEXT("Old Notify cannot advance new cast"), Runtime->ForwardValidatedNotify(First.CastId, Definition.ReleaseEventTag));
	TestEqual(TEXT("New cast still waits"), Runtime->GetCastStage(), ESkillLabCastStage::Windup);
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	TestFalse(TEXT("Cancelled cast rejects Notify"), Runtime->ForwardValidatedNotify(Second.CastId, Definition.ReleaseEventTag));
	Runtime->ResetSkills();

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
	ASC->ApplyModToAttribute(USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Override, 0.0f);
	const FSkillLabSkillResult FailedDodge = Runtime->SubmitSkillOperationRequest(DodgeRequest, ESkillLabOperationCategory::Dodge);
	TestFalse(TEXT("Dodge still obeys formal resource gate"), FailedDodge.bSuccess);
	TestEqual(TEXT("Dodge reports formal resource rejection"), FailedDodge.Reason, FName(TEXT("InsufficientEnergy")));
	TestEqual(TEXT("Failed dodge does not restore interrupted Cast"), Runtime->GetActiveCastId(), 0);
	ASC->ApplyModToAttribute(USkillLabCombatAttributeSet::GetEnergyAttribute(), EGameplayModOp::Override, InitialEnergy);

	const FSkillLabSkillResult CacheVictim = Request();
	Runtime->NotifyNonInterruptingOperation(ESkillLabOperationCategory::Movement);
	TestEqual(TEXT("Movement category leaves current Cast intact"), Runtime->GetActiveCastId(), CacheVictim.CastId);
	Runtime->NotifyNonInterruptingOperation(ESkillLabOperationCategory::UserCancel);
	TestEqual(TEXT("User cancellation category leaves current Cast intact"), Runtime->GetActiveCastId(), CacheVictim.CastId);
	FSkillLabSkillRequest BufferedRequest;
	BufferedRequest.Caster = Owner;
	BufferedRequest.SkillId = Definition.SkillId;
	const FSkillLabSkillResult BufferedResult = Runtime->SubmitSkillRequest(BufferedRequest);
	TestEqual(TEXT("Busy skill intent occupies latest-wins slot"), BufferedResult.Reason, FName(TEXT("InputBufferedLatestWins")));
	FSkillLabSkillRequest LatestRequest = BufferedRequest;
	const FSkillLabSkillResult LatestBufferedResult = Runtime->SubmitSkillRequest(LatestRequest);
	TestEqual(TEXT("Latest skill intent replaces prior buffered intent"), LatestBufferedResult.Reason, FName(TEXT("InputBufferedLatestWins")));
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	TestEqual(TEXT("Cancelled Cast clears buffered intent without auto activation"), Runtime->GetActiveCastId(), 0);
	TestTrue(TEXT("Buffered Cast victim was live"), CacheVictim.bSuccess);
	Runtime->ResetSkills();
	TestFalse(TEXT("Reset clears any pending input slot"), Runtime->PendingSkillIntent.IsSet());
	const FSkillLabSkillResult ResetVictim = Request();
	FSkillLabSkillRequest ResetBufferedRequest;
	ResetBufferedRequest.Caster = Owner;
	ResetBufferedRequest.SkillId = Definition.SkillId;
	Runtime->SubmitSkillRequest(ResetBufferedRequest);
	TestTrue(TEXT("Reset fixture populated the pending input slot"), Runtime->PendingSkillIntent.IsSet());
	Runtime->ResetSkills();
	TestFalse(TEXT("Reset actively clears a pending intent"), Runtime->PendingSkillIntent.IsSet());
	TestTrue(TEXT("Reset fixture cast was active"), ResetVictim.bSuccess);

	const FSkillLabSkillResult DeathVictim = Request();
	FSkillLabSkillRequest DeathBufferedRequest;
	DeathBufferedRequest.Caster = Owner;
	DeathBufferedRequest.SkillId = Definition.SkillId;
	Runtime->SubmitSkillRequest(DeathBufferedRequest);
	TestTrue(TEXT("Death fixture populated the pending input slot"), Runtime->PendingSkillIntent.IsSet());
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Dead);
	TestEqual(TEXT("Death cancels the current Cast"), Runtime->GetActiveCastId(), 0);
	TestFalse(TEXT("Death clears the cached intent"), Runtime->PendingSkillIntent.IsSet());
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Dead);
	TestTrue(TEXT("Death cleanup fixture started a Cast"), DeathVictim.bSuccess);
	Runtime->ResetSkills();

	const FSkillLabSkillResult TerminalVictim = Request();
	FSkillLabSkillRequest TerminalBufferedRequest;
	TerminalBufferedRequest.Caster = Owner;
	TerminalBufferedRequest.SkillId = TEXT("Skill_BufferedSuccess");
	const FSkillLabSkillResult TerminalBuffered = Runtime->SubmitSkillRequest(TerminalBufferedRequest);
	TestEqual(TEXT("Natural-completion input is accepted into the single slot"), TerminalBuffered.Reason, FName(TEXT("InputBufferedLatestWins")));
	Runtime->ForwardValidatedNotify(TerminalVictim.CastId, Definition.ReleaseEventTag);
	TestEqual(TEXT("Buffered intent cannot activate in Recovery"), Runtime->GetCastStage(), ESkillLabCastStage::Recovery);
	Runtime->FinishRecovery(Runtime->Lifecycle.GetPhaseToken());
	TestEqual(TEXT("Natural completion clears old Cast before scheduling buffered activation"), Runtime->GetActiveCastId(), 0);
	TestFalse(TEXT("Old Ability has ended before deferred buffered activation"), Runtime->ActiveAbility.IsValid());
	TestTrue(TEXT("Natural completion preserves buffered intent until next tick"), Runtime->PendingSkillIntent.IsSet());
	TestEqual(TEXT("No Cast starts synchronously inside old Ability EndAbility"), StartedCount, 10);
	bDefinitionsRestored = true;
	ADD_LATENT_AUTOMATION_COMMAND(FSkillLabBufferedIntentConsumeCommand(this, Runtime, TerminalVictim.CastId,
		TerminalBuffered.RequestId, Attacker, InitialEnergy, PreviousDefinitions, bPreviousLoadAttempted));
	return true;
}
#endif
