#include "SkillLabCombatTestFixture.h"
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
#include "SkillLabSkillBehavior.h"
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
		if (bAwaitingInitialConsumption && (Runtime->GetActiveCastId() == OldCastId || Runtime->PendingSkillIntent.IsSet()))
		{
			if (++WaitFrames <= 120) return false;
			Test->TestTrue(TEXT("Buffered intent consumption completes within the latent timeout"), false);
			return true;
		}

		if (bAwaitingInitialConsumption)
		{
			Test->TestFalse(TEXT("Natural-completion buffer ID is not reused as a new RequestId"),
				Runtime->GetSingleExecution()->ActiveRequest.RequestId == BufferedRequestId);
			Test->TestTrue(TEXT("Natural completion starts a fresh Cast after clearing the old Cast"), Runtime->GetActiveCastId() > OldCastId);
			Test->TestEqual(TEXT("Natural completion activates the buffered skill definition"), Runtime->GetSingleExecution()->ActiveRequest.SkillId,
				FName(TEXT("Skill_BufferedSuccess")));
			Test->TestTrue(TEXT("Fresh Cast has a live GAS Ability only after deferred consumption"), SkillLabObservedAbility(Runtime).IsValid()
				&& SkillLabObservedAbility(Runtime)->bSkillActive && SkillLabObservedAbility(Runtime)->BoundCastToken.CastId == Runtime->GetActiveCastId());
			Test->TestFalse(TEXT("Old Cast Notify cannot advance the consumed Cast"), Runtime->ForwardValidatedNotify(OldCastId, SkillLabGASTags::Event_Cast));
			FName RuleReason;
			Test->TestFalse(TEXT("Another skill selects a distinct movement policy"), Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, RuleReason));
			Test->TestEqual(TEXT("Distinct policy reports its content identity"), RuleReason, FName(TEXT("Restricted")));
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
			Runtime->ASC->RequestResourceChange(false, EGameplayModOp::Override, 0.0f);
			FailedOldCastId = FailedVictim.CastId;
			Runtime->FinishRecovery(*Runtime->FindMutableExecution(Runtime->GetActiveCastId()), Runtime->GetSingleExecution()->Lifecycle.GetPhaseToken());
			Test->TestEqual(TEXT("Buffered request does not activate while Recovery is ending"), Runtime->GetActiveCastId(), 0);
			Test->TestTrue(TEXT("Intent remains until deferred formal gates run"), Runtime->PendingSkillIntent.IsSet());
		}
		return CompleteAfterGateFailure();
	}

private:
	bool CompleteAfterGateFailure()
	{
		if (Runtime->GetActiveCastId() == FailedOldCastId || Runtime->PendingSkillIntent.IsSet())
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
		Runtime->ASC->RequestResourceChange(false, EGameplayModOp::Override,
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
	const FString FixtureJson = TEXT(R"({"combatRules":[{"id":"Shared","skill":"Buffer","dodge":"CancelAndActivate","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Recovery","skill":"Buffer","dodge":"Buffer","movement":"Continue","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"},{"id":"Restricted","skill":"Reject","dodge":"Reject","movement":"Reject","userCancel":"Continue","normalHit":"Continue","heavyHit":"Interrupt","unknownHit":"Continue"}],"skills":[{"id":"Skill_Mend","luaScript":"Skill_Mend.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3,"ruleWindows":[{"id":"RestrictedWindow","beginEvent":"SkillLab.Event.Window.Open","endEvent":"SkillLab.Event.Window.Close","rules":"Restricted"},{"id":"OtherWindow","beginEvent":"SkillLab.Event.Window.AlternateOpen","endEvent":"SkillLab.Event.Window.AlternateClose","rules":"Shared"}]},{"id":"Skill_BufferedSuccess","luaScript":"Skill_BufferedSuccess.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedVictim","luaScript":"Skill_BufferedVictim.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3},{"id":"Skill_BufferedFail","luaScript":"Skill_BufferedFail.lua","cost":15,"cooldown":2,"magnitude":40,"range":1000,"windup":0.5,"recovery":0.5,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3}]})");
	const FString FixtureLua = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\",\"combatRules\",\"Shared\",\"recoveryRules\",\"Recovery\",\"windowConflict\",\"Reject\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end");
	const FString StateFixtureJson = FixtureJson.Replace(TEXT("\"id\":\"Restricted\",\"skill\""),
		TEXT("\"id\":\"Restricted\",\"abilityCategories\":{\"Ability.SkillLab.Utility\":\"Reject\"},\"interruptBy\":[\"State.SkillLab.Marked\"],\"skill\""));
	TMap<FName, FSkillLabSkillDefinition> FixtureDefinitions;
	FString FixtureError;
	TMap<FString, FString> LuaSources;
	LuaSources.Add(TEXT("Skill_Mend.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedSuccess.lua"), FixtureLua.Replace(TEXT("\"Shared\""), TEXT("\"Restricted\"")));
	LuaSources.Add(TEXT("Skill_BufferedVictim.lua"), FixtureLua);
	LuaSources.Add(TEXT("Skill_BufferedFail.lua"), FixtureLua);
	bool bDefinitionsRestored = false;
	if (!TestTrue(TEXT("Gameplay Event test definition fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		StateFixtureJson, LuaSources, FixtureDefinitions, FixtureError)))
	{
		AddError(FixtureError);
		return false;
	}
	{
		TMap<FString, FString> MissingConflictSources = LuaSources;
		MissingConflictSources[TEXT("Skill_Mend.lua")] = FixtureLua.Replace(TEXT(",\"windowConflict\",\"Reject\""), TEXT(""));
		TMap<FName, FSkillLabSkillDefinition> Invalid;
		FString Error;
		TestFalse(TEXT("Overlapping-capable windows cannot use an implicit conflict policy"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
			StateFixtureJson, MissingConflictSources, Invalid, Error));
		TestTrue(TEXT("Missing conflict policy identifies the declaration"), Error.Contains(TEXT("windowConflict")));
		Error.Reset();
		TestFalse(TEXT("Duplicate window identities are rejected at load"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
			StateFixtureJson.Replace(TEXT("\"id\":\"OtherWindow\""), TEXT("\"id\":\"RestrictedWindow\"")), LuaSources, Invalid, Error));
	}
	TMap<FName, FSkillLabSkillDefinition> ImmediateDefinitions;
	TMap<FString, FString> ImmediateSources;
	ImmediateSources.Add(TEXT("Immediate.lua"), FixtureLua.Replace(TEXT(",\"combatRules\",\"Shared\",\"recoveryRules\",\"Recovery\""), TEXT("")));
	if (!TestTrue(TEXT("Timing without windup or recovery declarations parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Immediate","luaScript":"Immediate.lua","cost":2,"cooldown":0,"magnitude":1,"range":1000}]})"),
		ImmediateSources, ImmediateDefinitions, FixtureError)))
	{
		AddError(FixtureError);
		return false;
	}
	FixtureDefinitions.Append(ImmediateDefinitions);
	TMap<FName, FSkillLabSkillDefinition> IndependentDefinitions;
	TMap<FString, FString> IndependentSources;
	IndependentSources.Add(TEXT("Independent.lua"), ImmediateSources.FindChecked(TEXT("Immediate.lua")).Replace(
		TEXT("\"execution\",\"InstantTarget\""), TEXT("\"execution\",\"InstantTarget\",\"windupEnd\",\"Timer\"")));
	if (!TestTrue(TEXT("Independent windup fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Independent","luaScript":"Independent.lua","cost":2,"cooldown":0,"magnitude":1,"range":1000,"releaseTrigger":"GameplayEvent","releaseEventTag":"SkillLab.Event.Cast","releaseEventTimeout":3}]})"),
		IndependentSources, IndependentDefinitions, FixtureError))) { AddError(FixtureError); return false; }
	FixtureDefinitions.Append(IndependentDefinitions);
	TMap<FName, FSkillLabSkillDefinition> HoldDefinitions;
	TMap<FString, FString> HoldSources;
	HoldSources.Add(TEXT("Hold.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) end"));
	if (!TestTrue(TEXT("Independent completion event fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Hold","luaScript":"Hold.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"}]})"),
		HoldSources, HoldDefinitions, FixtureError))) { AddError(FixtureError); return false; }
	FixtureDefinitions.Append(HoldDefinitions);
	TMap<FName, FSkillLabSkillDefinition> LuaCheckDefinitions;
	TMap<FString, FString> LuaCheckSources;
	const FString LuaDeclaration = TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\n");
	LuaCheckSources.Add(TEXT("ActionCheck.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local r=SkillLabSkill_RequestAction(); if r.accepted or r.applied or not r.pending or r.completed or r.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	LuaCheckSources.Add(TEXT("EffectCheck.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local r=SkillLabSkill_ApplyEffect(); if r.applied or not r.pending or r.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	LuaCheckSources.Add(TEXT("ActionThenEffect.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local a=SkillLabSkill_RequestAction(); local b=SkillLabSkill_ApplyEffect(); if a.accepted or a.applied or not a.pending or a.completed or a.reason ~= 'SubmissionUnconfirmed' or b.applied or not b.pending or b.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	LuaCheckSources.Add(TEXT("EffectThenAction.lua"), LuaDeclaration + TEXT("function OnSkillCast(context) local a=SkillLabSkill_ApplyEffect(); local b=SkillLabSkill_RequestAction(); if a.applied or not a.pending or a.reason ~= 'SubmissionUnconfirmed' or b.accepted or b.applied or not b.pending or b.completed or b.reason ~= 'SubmissionUnconfirmed' or SkillLabSkill_Finish() then invalid_result() end end"));
	if (!TestTrue(TEXT("Lua submission fixture parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_LuaAction","luaScript":"ActionCheck.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000},{"id":"Skill_LuaEffect","luaScript":"EffectCheck.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000},{"id":"Skill_ActionThenEffect","luaScript":"ActionThenEffect.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000},{"id":"Skill_EffectThenAction","luaScript":"EffectThenAction.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000}]})"),
		LuaCheckSources, LuaCheckDefinitions, FixtureError))) { AddError(FixtureError); return false; }
	FixtureDefinitions.Append(LuaCheckDefinitions);
	TMap<FName, FSkillLabSkillDefinition> UtilityDefinitions;
	TMap<FString, FString> UtilitySources;
	UtilitySources.Add(TEXT("Utility.lua"), ImmediateSources.FindChecked(TEXT("Immediate.lua")).Replace(
		TEXT("\"execution\",\"InstantTarget\""), TEXT("\"execution\",\"InstantTarget\",\"categoryTag\",\"Ability.SkillLab.Utility\"")));
	if (!TestTrue(TEXT("Registered additional ability category parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Utility","luaScript":"Utility.lua","cost":2,"cooldown":0,"magnitude":1,"range":1000}]})"),
		UtilitySources, UtilityDefinitions, FixtureError))) { AddError(FixtureError); return false; }
	FixtureDefinitions.Append(UtilityDefinitions);
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
	FSkillLabSkillRequest ImmediateRequest;
	ImmediateRequest.Caster = Owner;
	ImmediateRequest.SkillId = TEXT("Skill_Immediate");
	const uint64 BlockingReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reserve energy through the ASC ledger"), ASC->BeginEnergyEscrow(BlockingReservation, Runtime->GetEnergyValue() - 1.0f));
	TestEqual(TEXT("Runtime exposes actual energy"), Runtime->GetEnergyValue(), ASC->GetActualEnergy());
	TestEqual(TEXT("Runtime exposes reserved energy"), Runtime->GetReservedEnergyValue(), ASC->GetReservedEnergy());
	TestEqual(TEXT("Runtime exposes available energy"), Runtime->GetAvailableEnergyValue(), 1.0f);
	TestFalse(TEXT("Formal request rejects insufficient available energy"), Runtime->SubmitSkillRequest(ImmediateRequest).bSuccess);
	TestTrue(TEXT("Release blocking reservation"), ASC->ResolveEnergyEscrow(BlockingReservation, false));
	const float BeforeImmediateEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult Immediate = Runtime->SubmitSkillRequest(ImmediateRequest);
	TestTrue(TEXT("Immediate timing uses formal activation"), Immediate.bSuccess);
	TestEqual(TEXT("Immediate timing completes without a configured recovery"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Immediate timing commits cost once"), Runtime->GetEnergyValue(), BeforeImmediateEnergy - 2.0f);
	Runtime->ResetSkills();
	FSkillLabSkillRequest HoldRequest;
	FSkillLabSkillRequest IndependentRequest;
	IndependentRequest.Caster = Owner;
	IndependentRequest.SkillId = TEXT("Skill_Independent");
	const FSkillLabSkillResult BlockedRelease = Runtime->SubmitSkillRequest(IndependentRequest);
	TestTrue(TEXT("Release gate fixture activates before competing reservation"), BlockedRelease.bSuccess);
	const uint64 ReleaseReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reserve funds before release"), ASC->BeginEnergyEscrow(ReleaseReservation, Runtime->GetAvailableEnergyValue() - 1.0f));
	Runtime->ForwardValidatedNotify(BlockedRelease.CastId, SkillLabGASTags::Event_Cast);
	TestEqual(TEXT("Release rejects insufficient available funds"), Runtime->GetActiveCastId(), 0);
	TestTrue(TEXT("Release competing reservation"), ASC->ResolveEnergyEscrow(ReleaseReservation, false));
	Runtime->ResetSkills();
	const float BeforeIndependentEnergy = Runtime->GetEnergyValue();
	const FSkillLabSkillResult Independent = Runtime->SubmitSkillRequest(IndependentRequest);
	TestTrue(TEXT("Independent preparation activates through GAS"), Independent.bSuccess);
	TestEqual(TEXT("Preparation can end while release is still pending"), Runtime->GetCastStage(), ESkillLabCastStage::Active);
	TestFalse(TEXT("Preparation ending does not mark release"), Runtime->GetSingleExecution()->Lifecycle.GetSnapshot().bHasReleased);
	TestEqual(TEXT("Preparation ending does not charge cost"), Runtime->GetEnergyValue(), BeforeIndependentEnergy);
	TestTrue(TEXT("Release event after preparation submits normally"), Runtime->ForwardValidatedNotify(Independent.CastId, SkillLabGASTags::Event_Cast));
	TestEqual(TEXT("Independent release completes without recovery delay"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Independent release charges once"), Runtime->GetEnergyValue(), BeforeIndependentEnergy - 2.0f);
	Runtime->ResetSkills();
	HoldRequest.Caster = Owner;
	HoldRequest.SkillId = TEXT("Skill_Hold");
	const FSkillLabSkillResult Hold = Runtime->SubmitSkillRequest(HoldRequest);
	TestTrue(TEXT("Hold fixture enters a formal Cast"), Hold.bSuccess);
	TestEqual(TEXT("Effect and completion are independently timed"), Runtime->GetCastStage(), ESkillLabCastStage::Active);
	USkillLabLuaSkillBehavior* HoldBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	if (!TestNotNull(TEXT("Hold fixture has a public behavior endpoint"), HoldBehavior)) return false;
	const uint64 SubmissionReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Reserve funds before first submission"), ASC->BeginEnergyEscrow(SubmissionReservation, Runtime->GetAvailableEnergyValue() - 1.0f));
	const float BeforeRejectedSubmission = Runtime->GetEnergyValue();
	int32 RejectedSubmissionEffects = 0;
	const FDelegateHandle RejectedSubmissionHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.CastId == Hold.CastId && Event.Event == ESkillLabSkillEvent::EffectApplied) ++RejectedSubmissionEffects;
	});
	TestFalse(TEXT("First submission rejects insufficient available funds"), HoldBehavior->RequestDeclaredAction());
	Runtime->OnNativeSkillEvent.Remove(RejectedSubmissionHandle);
	TestEqual(TEXT("Rejected submission does not apply an effect"), RejectedSubmissionEffects, 0);
	TestEqual(TEXT("Rejected submission does not debit actual energy"), Runtime->GetEnergyValue(), BeforeRejectedSubmission);
	TestEqual(TEXT("Rejected submission does not start cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), 0.0f);
	TestTrue(TEXT("Release competing submission reservation"), ASC->ResolveEnergyEscrow(SubmissionReservation, false));
	const float BeforeHoldEnergy = Runtime->GetEnergyValue();
	auto* SavedExecutor = Runtime->EffectExecutor.Get();
	Runtime->EffectExecutor = nullptr;
	TestFalse(TEXT("Failed effect is not a successful submission"), HoldBehavior->RequestDeclaredAction());
	TestEqual(TEXT("Failed submission does not charge energy"), Runtime->GetEnergyValue(), BeforeHoldEnergy);
	TestEqual(TEXT("Failed submission does not start cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), 0.0f);
	Runtime->EffectExecutor = SavedExecutor;
	bool bNestedSubmissionRejected = false;
	const FDelegateHandle SubmissionHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.Event != ESkillLabSkillEvent::EffectApplied || Event.CastId != Hold.CastId) return;
		FName Reason;
		bNestedSubmissionRejected = !Runtime->BeginBehaviorSubmission(HoldBehavior, Hold.CastId, 2, Reason);
	});
	bool bReentrantResolveAttempted = false;
	bool bReentrantResolveAccepted = false;
	const FDelegateHandle ReentrantResolveHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bReentrantResolveAttempted && Change.NewValue < Change.OldValue)
			{
				bReentrantResolveAttempted = true;
				bReentrantResolveAccepted = Runtime->ResolveBehaviorSubmission(HoldBehavior, Hold.CastId, 1, true);
			}
		});
	TestTrue(TEXT("Retry after failure applies through the real executor"), HoldBehavior->RequestDeclaredAction());
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(ReentrantResolveHandle);
	Runtime->OnNativeSkillEvent.Remove(SubmissionHandle);
	TestTrue(TEXT("Synchronous reentry cannot submit while an attempt is pending"), bNestedSubmissionRejected);
	TestTrue(TEXT("Settlement callback attempted reentrant resolution"), bReentrantResolveAttempted);
	TestFalse(TEXT("Reentrant resolution is rejected"), bReentrantResolveAccepted);
	TestEqual(TEXT("First successful submission charges exactly once"), Runtime->GetEnergyValue(), BeforeHoldEnergy - 2.0f);
	const float FirstCooldown = Runtime->GetCooldownRemaining(HoldRequest.SkillId);
	TestTrue(TEXT("Outer resolution starts cooldown"), FirstCooldown > 0.0f);
	TestEqual(TEXT("Outer resolution clears reservation"), Runtime->GetReservedEnergyValue(), 0.0f);
	FName SubmissionReason;
	TestTrue(TEXT("Shared API accepts another independent point"), Runtime->BeginBehaviorSubmission(HoldBehavior, Hold.CastId, 2, SubmissionReason));
	TestTrue(TEXT("Shared API resolves the second accepted point"), Runtime->ResolveBehaviorSubmission(HoldBehavior, Hold.CastId, 2, true));
	TestEqual(TEXT("Second point does not charge again"), Runtime->GetEnergyValue(), BeforeHoldEnergy - 2.0f);
	TestEqual(TEXT("Second point does not restart cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), FirstCooldown);
	TestTrue(TEXT("Completion event terminates submitted behavior independently"), Runtime->ForwardValidatedNotify(Hold.CastId, SkillLabGASTags::Event_Cast_Complete));
	TestEqual(TEXT("Completion event leaves no active Cast"), Runtime->GetActiveCastId(), 0);
	Runtime->ResetSkills();
	const FSkillLabSkillResult EscrowCast = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* EscrowBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	bool bSawFullEscrow = false;
	const float BeforeEscrow = Runtime->GetEnergyValue();
	const FDelegateHandle EscrowHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.CastId != EscrowCast.CastId || Event.Event != ESkillLabSkillEvent::EffectApplied) return;
		bSawFullEscrow = FMath::IsNearlyEqual(Runtime->GetEnergyValue(), BeforeEscrow)
			&& FMath::IsNearlyEqual(Runtime->GetReservedEnergyValue(), 2.0f)
			&& FMath::IsNearlyEqual(Runtime->GetAvailableEnergyValue(), BeforeEscrow - 2.0f);
		TestFalse(TEXT("Write cannot invalidate a reservation"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 1.0f));
		TestTrue(TEXT("Valid energy write applies while reserved"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 90.0f));
		ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
	});
	TestTrue(TEXT("Escrow action submits successfully"), EscrowBehavior && EscrowBehavior->RequestDeclaredAction());
	Runtime->OnNativeSkillEvent.Remove(EscrowHandle);
	TestTrue(TEXT("Effect callback observes full cost already escrowed"), bSawFullEscrow);
	TestEqual(TEXT("Valid writes and subsequent settlement preserve the balance"), Runtime->GetEnergyValue(), 89.0f);
	TestEqual(TEXT("Successful settlement clears reservation"), Runtime->GetReservedEnergyValue(), 0.0f);
	Runtime->ResetSkills();
	const uint64 FailedReservation = ASC->AllocateEnergyTransactionId();
	const uint64 OtherReservation = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Resource coordinator reserves for failed attempt"), ASC->BeginEnergyEscrow(FailedReservation, 2.0f));
	TestTrue(TEXT("Another transaction can reserve available energy"), ASC->BeginEnergyEscrow(OtherReservation, 2.0f));
	TestTrue(TEXT("Energy override respecting reservations applies"), ASC->RequestResourceChange(false, EGameplayModOp::Override, 10.0f));
	TestTrue(TEXT("Failed attempt releases its reservation"), ASC->ResolveEnergyEscrow(FailedReservation, false));
	TestEqual(TEXT("Failed attempt does not debit actual energy"), Runtime->GetEnergyValue(), 10.0f);
	TestEqual(TEXT("Other reservation remains"), Runtime->GetReservedEnergyValue(), 2.0f);
	TestTrue(TEXT("Other reservation can be released"), ASC->ResolveEnergyEscrow(OtherReservation, false));
	Runtime->ResetSkills();
	const auto FreshFixture = [&]()
	{
		Owner->Destroy();
		Owner = World->SpawnActor<AActor>();
		ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		Runtime = NewObject<USkillLabSkillRuntimeComponent>(Owner);
		Owner->AddInstanceComponent(Runtime);
		Runtime->RegisterComponent();
		Runtime->InitializeSkills();
		HoldRequest.Caster = Owner;
	};
	FreshFixture();
	AddExpectedError(TEXT("Energy/MaxEnergy changed outside the resource coordinator"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
	const FSkillLabSkillResult BypassCast = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* BypassBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	const FDelegateHandle BypassHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.CastId == BypassCast.CastId && Event.Event == ESkillLabSkillEvent::EffectApplied)
			ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 1.0f);
	});
	TestFalse(TEXT("Unsupported bypass is not reported as accepted"), BypassBehavior && BypassBehavior->RequestDeclaredAction());
	TestTrue(TEXT("Unsupported bypass waits for confirmation"), BypassBehavior && BypassBehavior->IsActionPending());
	TestFalse(TEXT("Unsupported bypass does not report completion"), BypassBehavior && BypassBehavior->IsActionCompleted());
	TestFalse(TEXT("Unsupported bypass cannot be repeated"), BypassBehavior && BypassBehavior->RequestDeclaredAction());
	FSkillLabSkillRequest LateBufferRequest = HoldRequest;
	LateBufferRequest.SkillId = TEXT("Skill_Independent");
	Runtime->SubmitSkillRequest(LateBufferRequest);
	const bool bHadBufferedIntent = Runtime->PendingSkillIntent.IsSet();
	TestTrue(TEXT("Unconfirmed cast retains a buffered intent for the late-callback test"), bHadBufferedIntent);
	Runtime->CompleteBehavior(BypassBehavior, BypassCast.CastId, TEXT("LateComplete"));
	TestEqual(TEXT("Late completion cannot end an unconfirmed cast"), Runtime->GetActiveCastId(), BypassCast.CastId);
	TestEqual(TEXT("Late completion cannot consume buffered input"), Runtime->PendingSkillIntent.IsSet(), bHadBufferedIntent);
	if (BypassBehavior)
	{
		BypassBehavior->OnProjectileResolved(1, nullptr, Owner, TEXT("Hit"));
		BypassBehavior->OnProjectileResolved(1, nullptr, nullptr, TEXT("Expired"));
		BypassBehavior->OnProjectileResolved(1, nullptr, nullptr, TEXT("Destroyed"));
		TestTrue(TEXT("Late projectile results preserve pending state"), BypassBehavior->IsActionPending());
		TestFalse(TEXT("Late projectile results cannot complete the action"), BypassBehavior->IsActionCompleted());
	}
	TestEqual(TEXT("Late projectile results cannot end the cast"), Runtime->GetActiveCastId(), BypassCast.CastId);
	TestEqual(TEXT("Late projectile results cannot consume buffered input"), Runtime->PendingSkillIntent.IsSet(), bHadBufferedIntent);
	Runtime->OnNativeSkillEvent.Remove(BypassHandle);
	TestTrue(TEXT("Uncontrolled direct write is reported"), ASC->HasUnsupportedResourceWrite());
	TestTrue(TEXT("Uncontrolled effect callback leaves an indeterminate ledger"), ASC->HasIndeterminateEnergyEscrow(Runtime->GetSingleExecution()->Submission.TransactionId));
	TestEqual(TEXT("Uncontrolled effect callback does not claim a committed cooldown"), Runtime->GetCooldownRemaining(HoldRequest.SkillId), 0.0f);
	Runtime->ResetSkills();
	for (const FName LuaSkill : {FName(TEXT("Skill_ActionThenEffect")), FName(TEXT("Skill_EffectThenAction"))})
	{
		FreshFixture();
		AddExpectedError(TEXT("Energy/MaxEnergy changed outside the resource coordinator"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		int32 EffectCount = 0;
		const FDelegateHandle Handle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
		{
			if (Event.SkillId == LuaSkill && Event.Event == ESkillLabSkillEvent::EffectApplied)
			{
				++EffectCount;
				ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 1.0f);
			}
		});
		FSkillLabSkillRequest Request;
		Request.Caster = Owner;
		Request.SkillId = LuaSkill;
		const FSkillLabSkillResult CastResult = Runtime->SubmitSkillRequest(Request);
		Runtime->OnNativeSkillEvent.Remove(Handle);
		TestEqual(TEXT("Cross-entry Lua fixture applies exactly one effect"), EffectCount, 1);
		TestEqual(TEXT("Cross-entry Lua script does not terminate the unconfirmed cast"), Runtime->GetActiveCastId(), CastResult.CastId);
		const USkillLabLuaSkillBehavior* LuaBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
		TestTrue(TEXT("Cross-entry Lua script retains pending state"), LuaBehavior && LuaBehavior->IsActionPending());
		Runtime->ResetSkills();
	}
	// A reservation does not debit or refund. Disturb settlement instead, after the effect.
	for (const FName LuaSkill : {FName(TEXT("Skill_LuaAction")), FName(TEXT("Skill_LuaEffect"))})
	{
		FreshFixture();
		bool bCancelledOnDebit = false;
		uint64 DisturbedTransaction = 0;
		int32 EffectCount = 0;
		const FDelegateHandle EffectHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
		{
			if (Event.SkillId == LuaSkill && Event.Event == ESkillLabSkillEvent::EffectApplied) ++EffectCount;
		});
		AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Submission escrow did not close"), EAutomationExpectedErrorFlags::Contains, 1);
		const FDelegateHandle Handle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
			[&](const FOnAttributeChangeData& Data)
			{
				if (!bCancelledOnDebit && Data.NewValue < Data.OldValue && ASC->IsEnergyEscrowActive())
				{
					bCancelledOnDebit = true;
					DisturbedTransaction = Runtime->GetSingleExecution()->Submission.TransactionId;
					Runtime->CancelActiveCast(TEXT("DuringSettlement"));
					ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 50.0f);
				}
			});
		FSkillLabSkillRequest Request;
		Request.Caster = Owner;
		Request.SkillId = LuaSkill;
		Runtime->SubmitSkillRequest(Request);
		ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Handle);
		Runtime->OnNativeSkillEvent.Remove(EffectHandle);
		TestTrue(TEXT("Cancellation occurred during settlement"), bCancelledOnDebit);
		TestEqual(TEXT("Effect occurred before settlement"), EffectCount, 1);
		TestEqual(TEXT("Interrupted cast is cleaned up"), Runtime->GetActiveCastId(), 0);
		TestTrue(TEXT("Disturbed settlement leaves an indeterminate ledger"), ASC->HasIndeterminateEnergyEscrow(DisturbedTransaction));
	}
	for (int32 LuaMode = 0; LuaMode < 4; ++LuaMode)
	{
		FreshFixture();
		const FName LuaSkill = LuaMode < 2 ? FName(TEXT("Skill_LuaAction")) : FName(TEXT("Skill_LuaEffect"));
		AddExpectedError(TEXT("Energy/MaxEnergy changed outside the resource coordinator"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		bool bEffectObserved = false;
		const FDelegateHandle LuaHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
		{
			if (Event.SkillId != LuaSkill || Event.Event != ESkillLabSkillEvent::EffectApplied) return;
			bEffectObserved = true;
			ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 1.0f);
			if (LuaMode % 2 == 0) Runtime->CancelActiveCast(TEXT("LuaSyncCancel"));
			else Runtime->ResetSkills();
		});
		FSkillLabSkillRequest LuaRequest;
		LuaRequest.Caster = Owner;
		LuaRequest.SkillId = LuaSkill;
		Runtime->SubmitSkillRequest(LuaRequest);
		Runtime->OnNativeSkillEvent.Remove(LuaHandle);
		TestTrue(TEXT("Lua fixture reaches the real effect"), bEffectObserved);
		TestEqual(TEXT("Synchronous termination clears the Lua cast"), Runtime->GetActiveCastId(), 0);
		Runtime->ResetSkills();
	}
	FreshFixture();
	AddExpectedError(TEXT("Energy/MaxEnergy changed outside the resource coordinator"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
	const FSkillLabSkillResult DirectEffectCast = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* DirectEffectBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	const FDelegateHandle DirectEffectHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.CastId == DirectEffectCast.CastId && Event.Event == ESkillLabSkillEvent::EffectApplied)
			ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 1.0f);
	});
	TestFalse(TEXT("Unconfirmed declared effect is not reported as applied"), DirectEffectBehavior && DirectEffectBehavior->RequestDeclaredEffect());
	TestTrue(TEXT("Unconfirmed declared effect is pending"), DirectEffectBehavior && DirectEffectBehavior->IsActionPending());
	TestFalse(TEXT("Unconfirmed declared effect cannot be repeated"), DirectEffectBehavior && DirectEffectBehavior->RequestDeclaredEffect());
	Runtime->OnNativeSkillEvent.Remove(DirectEffectHandle);
	Runtime->ResetSkills();
	FreshFixture();
	bool bDebitCallbackRan = false;
	const FDelegateHandle DebitHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bDebitCallbackRan && Change.NewValue < Change.OldValue && ASC->IsEnergyEscrowActive())
			{
				bDebitCallbackRan = true;
				ASC->RequestResourceChange(false, EGameplayModOp::Additive, 5.0f);
				ASC->RequestResourceChange(true, EGameplayModOp::Override, 99.0f);
			}
		});
	const FSkillLabSkillResult DebitCast = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* DebitBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	TestTrue(TEXT("Debit callback fixture is active"), DebitCast.bSuccess && DebitBehavior);
	TestTrue(TEXT("Debit callback action succeeds"), DebitBehavior && DebitBehavior->RequestDeclaredAction());
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DebitHandle);
	TestTrue(TEXT("Escrow debit callback can submit controlled changes"), bDebitCallbackRan);
	TestEqual(TEXT("Queued debit callback resource change is applied"), Runtime->GetEnergyValue(), 99.0f);
	ASC->RequestResourceChange(true, EGameplayModOp::Override, 100.0f);
	Runtime->ResetSkills();
	FreshFixture();
	bool bDirectDebitCallbackRan = false;
	const FDelegateHandle DirectDebitHandle = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
		[&](const FOnAttributeChangeData& Change)
		{
			if (!bDirectDebitCallbackRan && Change.NewValue < Change.OldValue && ASC->IsEnergyEscrowActive())
			{
				bDirectDebitCallbackRan = true;
				ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 50.0f);
			}
		});
	const uint64 DirectDebitId = ASC->AllocateEnergyTransactionId();
	TestTrue(TEXT("Direct debit fixture reserves without changing actual"), ASC->BeginEnergyEscrow(DirectDebitId, 2.0f));
	TestFalse(TEXT("Reservation does not trigger debit callback"), bDirectDebitCallbackRan);
	AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Direct debit callback makes settlement unconfirmed"), ASC->ResolveEnergyEscrow(DirectDebitId, true));
	ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DirectDebitHandle);
	TestTrue(TEXT("Direct debit callback was exercised"), bDirectDebitCallbackRan);
	TestTrue(TEXT("Direct debit callback is reported"), ASC->HasUnsupportedResourceWrite());
	TestTrue(TEXT("Unconfirmed debit retains reservation"), ASC->HasIndeterminateEnergyEscrow(DirectDebitId));
	FreshFixture();
	const FSkillLabSkillResult CancelDuringSubmit = Runtime->SubmitSkillRequest(HoldRequest);
	USkillLabLuaSkillBehavior* CancelBehavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Runtime));
	const float BeforeCancelEnergy = Runtime->GetEnergyValue();
	const FDelegateHandle CancelSubmitHandle = Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
	{
		if (Event.Event == ESkillLabSkillEvent::EffectApplied && Event.CastId == CancelDuringSubmit.CastId)
			Runtime->CancelActiveCast(TEXT("SynchronousCancel"));
	});
	TestTrue(TEXT("Accepted effect survives synchronous cancellation"), CancelBehavior && CancelBehavior->RequestDeclaredAction());
	Runtime->OnNativeSkillEvent.Remove(CancelSubmitHandle);
	TestEqual(TEXT("Deferred cancellation terminates the same Cast"), Runtime->GetActiveCastId(), 0);
	TestEqual(TEXT("Accepted submission charges once before cancellation"), Runtime->GetEnergyValue(), BeforeCancelEnergy - 2.0f);
	TestTrue(TEXT("Accepted submission retains cooldown after cancellation"), Runtime->GetCooldownRemaining(HoldRequest.SkillId) > 0.0f);
	Runtime->ResetSkills();
	const auto TestSubmissionTermination = [&](const int32 Mode)
	{
		AActor* TestOwner = World->SpawnActor<AActor>();
		USkillLabAbilitySystemComponent* TestASC = NewObject<USkillLabAbilitySystemComponent>(TestOwner);
		TestOwner->AddInstanceComponent(TestASC);
		TestASC->RegisterComponent();
		USkillLabSkillRuntimeComponent* TestRuntime = NewObject<USkillLabSkillRuntimeComponent>(TestOwner);
		TestOwner->AddInstanceComponent(TestRuntime);
		TestRuntime->RegisterComponent();
		if (!TestRuntime->HasBegunPlay()) TestRuntime->BeginPlay();
		TestRuntime->InitializeSkills();
		FSkillLabSkillRequest TestRequest;
		TestRequest.Caster = TestOwner;
		TestRequest.SkillId = TEXT("Skill_Hold");
		const FSkillLabSkillResult TestCast = TestRuntime->SubmitSkillRequest(TestRequest);
		USkillLabLuaSkillBehavior* Behavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(TestRuntime));
		if (!TestTrue(TEXT("Termination fixture activates"), TestCast.bSuccess && Behavior)) { TestOwner->Destroy(); return; }
		if (Mode == 2)
		{
			FName Reason;
			TestTrue(TEXT("Indeterminate fixture begins submission"), TestRuntime->BeginBehaviorSubmission(Behavior, TestCast.CastId, 1, Reason));
			const uint64 TransactionId = TestRuntime->GetSingleExecution()->Submission.TransactionId;
			TestASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
			AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
			TestRuntime->AbortBehaviorSubmission(Behavior, TestCast.CastId, 1);
			TestTrue(TEXT("Uncertain escrow retains an explicit ledger"), TestASC->HasIndeterminateEnergyEscrow(TransactionId));
			TestEqual(TEXT("Uncertain submission does not start cooldown"), TestRuntime->GetCooldownRemaining(TestRequest.SkillId), 0.0f);
		}
		else
		{
			bool bEnded = false;
			FDelegateHandle Handle;
			if (Mode == 0 || Mode == 3)
			{
				Handle = TestASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
					[&](const FOnAttributeChangeData& Change)
					{
						if (!bEnded && Change.NewValue < Change.OldValue && TestASC->IsEnergyEscrowActive())
						{
							bEnded = true;
							TestASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f);
							if (Mode == 0) TestRuntime->EndPlay(EEndPlayReason::RemovedFromWorld);
							else TestRuntime->ResetSkills();
						}
					});
			}
			else
			{
				Handle = TestRuntime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Event)
				{
					if (!bEnded && Event.Event == ESkillLabSkillEvent::EffectApplied)
					{
						bEnded = true;
						TestASC->RequestResourceChange(false, EGameplayModOp::Additive, -1.0f);
						if (Mode == 1) TestRuntime->EndPlay(EEndPlayReason::RemovedFromWorld);
						else TestRuntime->ResetSkills();
					}
				});
			}
			const bool bSucceeded = Behavior->RequestDeclaredAction();
			if (Mode == 0 || Mode == 3) TestASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(Handle);
			else TestRuntime->OnNativeSkillEvent.Remove(Handle);
			TestTrue(TEXT("Termination occurs inside submission"), bEnded);
			TestEqual(TEXT("Only a still-live Runtime reports complete submission delivery"), bSucceeded, Mode == 3 || Mode == 4);
			TestEqual(TEXT("Termination clears the Cast"), TestRuntime->GetActiveCastId(), 0);
			const float ExpectedEnergy = Mode == 0 ? 99.0f : (Mode == 1 ? 97.0f : 100.0f);
			TestEqual(TEXT("Termination retains coordinated resource changes"), TestRuntime->GetEnergyValue(), ExpectedEnergy);
		}
		TestEqual(TEXT("Only the explicitly unconfirmed reservation remains active"), TestASC->IsEnergyEscrowActive(), Mode == 2);
		TestFalse(TEXT("Termination drains resource queue"), TestASC->HasPendingResourceChanges());
		TestOwner->Destroy();
	};
	TestSubmissionTermination(0);
	TestSubmissionTermination(1);
	TestSubmissionTermination(2);
	TestSubmissionTermination(3);
	TestSubmissionTermination(4);
	FSkillLabSkillRequest StateRequest;
	StateRequest.Caster = Owner;
	StateRequest.SkillId = TEXT("Skill_Mend");
	const FSkillLabSkillResult StateCast = Runtime->SubmitSkillRequest(StateRequest);
	FSkillLabSkillRequest UtilityRequest = StateRequest;
	UtilityRequest.SkillId = TEXT("Skill_Utility");
	TestEqual(TEXT("Unspecified registered category is rejected explicitly"), Runtime->SubmitSkillRequest(UtilityRequest).Reason, FName(TEXT("UnknownAbilityCategory")));
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Marked, 2);
	TestEqual(TEXT("A state outside the current profile does not interrupt"), Runtime->GetActiveCastId(), StateCast.CastId);
	TestTrue(TEXT("Window event is accepted for state reevaluation"), Runtime->ForwardValidatedNotify(StateCast.CastId, SkillLabGASTags::Event_Window_Open));
	TestEqual(TEXT("Opening an interrupting window reevaluates an existing state"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Marked, 1);
	const FSkillLabSkillResult StateCastTwo = Runtime->SubmitSkillRequest(StateRequest);
	TestTrue(TEXT("State remains noninterrupting outside the window"), StateCastTwo.bSuccess);
	Runtime->ForwardValidatedNotify(StateCastTwo.CastId, SkillLabGASTags::Event_Window_Open);
	TestEqual(TEXT("Remaining state count still interrupts in the window"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Marked, 1);
	const FSkillLabSkillResult StateCastThree = Runtime->SubmitSkillRequest(StateRequest);
	Runtime->ForwardValidatedNotify(StateCastThree.CastId, SkillLabGASTags::Event_Window_Open);
	TestEqual(TEXT("Removing the last state count restores the window"), Runtime->GetActiveCastId(), StateCastThree.CastId);
	ASC->AddLooseGameplayTag(SkillLabGASTags::State_Marked);
	TestEqual(TEXT("A state gained inside the window interrupts"), Runtime->GetActiveCastId(), 0);
	ASC->RemoveLooseGameplayTag(SkillLabGASTags::State_Marked);
	Runtime->ResetSkills();
	int32 ReleaseCount = 0;
	TMap<FName, FSkillLabSkillDefinition> ModifiedProfiles;
	TestTrue(TEXT("Shared profile modification parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		StateFixtureJson.Replace(TEXT("\"movement\":\"Continue\""), TEXT("\"movement\":\"Reject\"")), LuaSources, ModifiedProfiles, FixtureError));
	// Keep unrelated fixture declarations in the complete candidate catalog.
	ModifiedProfiles.Append(ImmediateDefinitions);
	ModifiedProfiles.Append(IndependentDefinitions);
	ModifiedProfiles.Append(HoldDefinitions);
	ModifiedProfiles.Append(UtilityDefinitions);
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(ModifiedProfiles);
	for (const FName Skill : {FName(TEXT("Skill_Mend")), FName(TEXT("Skill_BufferedVictim")), FName(TEXT("Skill_BufferedSuccess"))})
	{
		FSkillLabSkillRequest ProfileRequest;
		ProfileRequest.Caster = Owner;
		ProfileRequest.SkillId = Skill;
		TestTrue(TEXT("Profile fixture activates formally"), Runtime->SubmitSkillRequest(ProfileRequest).bSuccess);
		FName RuleReason;
		TestFalse(TEXT("Selected profile controls movement"), Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, RuleReason));
		TestEqual(TEXT("Shared modification preserves the unrelated rule identity"), RuleReason,
			Skill == TEXT("Skill_BufferedSuccess") ? FName(TEXT("Restricted")) : FName(TEXT("Shared")));
		Runtime->CancelActiveCast(TEXT("ProfileFixtureCleanup"));
	}
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(FixtureDefinitions);
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
	TestEqual(TEXT("Two skill definitions share a rules profile"), FixtureDefinitions.FindChecked(TEXT("Skill_Mend")).OperationRules.Id,
		FixtureDefinitions.FindChecked(TEXT("Skill_BufferedVictim")).OperationRules.Id);
	TestTrue(TEXT("Window begin uses the current Ability"), Runtime->ForwardValidatedNotify(First.CastId, SkillLabGASTags::Event_Window_Open));
	TestTrue(TEXT("Window is observable"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	TestEqual(TEXT("Window profile matches the declared ability category"), Runtime->SubmitSkillRequest(UtilityRequest).Reason, FName(TEXT("Restricted")));
	FName MovementReason;
	TestFalse(TEXT("Window profile rejects movement"), Runtime->RequestNonSkillOperation(ESkillLabOperationCategory::Movement, MovementReason));
	TestEqual(TEXT("Rejected movement identifies the selected rule"), MovementReason, FName(TEXT("Restricted")));
	TestEqual(TEXT("Window profile rejects a skill without buffering"), Request().Reason, FName(TEXT("Restricted")));
	TestTrue(TEXT("Second window can open"), Runtime->ForwardValidatedNotify(First.CastId, SkillLabGASTags::Event_Window_AlternateOpen));
	TestEqual(TEXT("Overlapping rules report conflict"), Request().Reason, FName(TEXT("RuleWindowConflict")));
	TestTrue(TEXT("Second window closes"), Runtime->ForwardValidatedNotifyStateEnd(First.CastId, SkillLabGASTags::Event_Window_AlternateOpen));
	TestTrue(TEXT("Notify state end closes first window"), Runtime->ForwardValidatedNotifyStateEnd(First.CastId, SkillLabGASTags::Event_Window_Open));
	TestFalse(TEXT("Closed window is observable"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	Runtime->ForwardValidatedNotify(First.CastId, SkillLabGASTags::Event_Window_Open);
	TestFalse(TEXT("Duplicate begin does not reopen window"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
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
	TestTrue(TEXT("New cast can open its own window"), Runtime->ForwardValidatedNotify(Second.CastId, SkillLabGASTags::Event_Window_Open));
	TestFalse(TEXT("Old cast cannot close the new cast window"), Runtime->ForwardValidatedNotifyStateEnd(First.CastId, SkillLabGASTags::Event_Window_Open));
	TestTrue(TEXT("New cast window remains open after stale end"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
	Runtime->CancelActiveCast(TEXT("AutomationCancel"));
	TestFalse(TEXT("Cancellation clears the active window"), Runtime->IsRuleWindowOpen(TEXT("RestrictedWindow")));
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
	ASC->RequestResourceChange(false, EGameplayModOp::Override, 0.0f);
	const FSkillLabSkillResult FailedDodge = Runtime->SubmitSkillOperationRequest(DodgeRequest, ESkillLabOperationCategory::Dodge);
	TestFalse(TEXT("Dodge still obeys formal resource gate"), FailedDodge.bSuccess);
	TestEqual(TEXT("Dodge reports formal resource rejection"), FailedDodge.Reason, FName(TEXT("InsufficientEnergy")));
	TestEqual(TEXT("Failed dodge does not restore interrupted Cast"), Runtime->GetActiveCastId(), 0);
	ASC->RequestResourceChange(false, EGameplayModOp::Override, InitialEnergy);

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
	Runtime->FinishRecovery(*Runtime->FindMutableExecution(Runtime->GetActiveCastId()), Runtime->GetSingleExecution()->Lifecycle.GetPhaseToken());
	TestEqual(TEXT("Natural completion clears old Cast before scheduling buffered activation"), Runtime->GetActiveCastId(), 0);
	TestFalse(TEXT("Old Ability has ended before deferred buffered activation"), SkillLabObservedAbility(Runtime).IsValid());
	TestTrue(TEXT("Natural completion preserves buffered intent until next tick"), Runtime->PendingSkillIntent.IsSet());
	TestEqual(TEXT("No Cast starts synchronously inside old Ability EndAbility"), StartedCount, 10);
	bDefinitionsRestored = true;
	ADD_LATENT_AUTOMATION_COMMAND(FSkillLabBufferedIntentConsumeCommand(this, Runtime, TerminalVictim.CastId,
		TerminalBuffered.RequestId, Attacker, InitialEnergy, PreviousDefinitions, bPreviousLoadAttempted));
	return true;
}
#endif
