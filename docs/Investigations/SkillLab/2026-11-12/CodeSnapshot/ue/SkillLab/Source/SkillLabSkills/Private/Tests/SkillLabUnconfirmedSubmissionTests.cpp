#include "SkillLabCombatTestFixture.h"
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"
#include "SkillLabGASTags.h"
#include "SkillLabSkillBehavior.h"
#include "SkillLabSkillDefinitionProvider.h"
#include "SkillLabSkillGameplayAbility.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "SkillLabUnconfirmedSubmissions.h"
#include "TimerManager.h"
#include "SkillLabSubmissionTestWorld.h"

// 本夹具只替换当前用例的定义，析构时恢复；不复制大集成测试的缓存/窗口流程。
class FSkillLabUnconfirmedWaitCommand final : public IAutomationLatentCommand
{
public:
	FSkillLabUnconfirmedWaitCommand(FAutomationTestBase* InTest, UWorld* InWorld,
		const TMap<FName, FSkillLabSkillDefinition>& InDefinitions, bool bInLoadAttempted)
		: Test(InTest), World(InWorld), PreviousDefinitions(InDefinitions), bPreviousLoadAttempted(bInLoadAttempted) {}
	~FSkillLabUnconfirmedWaitCommand() override
	{
		for (TWeakObjectPtr<AActor> Actor : Actors) if (Actor.IsValid()) Actor->Destroy();
		FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(PreviousDefinitions, bPreviousLoadAttempted);
		World->DestroyWorld(false);
		if (GEngine) GEngine->DestroyWorldContext(World);
	}
	bool Update() override
	{
		if (!bStarted)
		{
			bStarted = true;
			if (!Setup()) return true;
			StartedAt = LastTickAt = FPlatformTime::Seconds();
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		// 独立 Game World 使用真实经过时间推进世界时钟及正式 TimerManager。
		World->Tick(LEVELTICK_All, static_cast<float>(Now - LastTickAt));
		LastTickAt = Now;
		if (Now - StartedAt < 1.1) return false;
		Finish();
		return true;
	}
private:
	struct FFixture
	{
		AActor* Owner = nullptr;
		USkillLabAbilitySystemComponent* ASC = nullptr;
		USkillLabSkillRuntimeComponent* Runtime = nullptr;
		USkillLabUnconfirmedSubmissions* Records = nullptr;
		TWeakObjectPtr<USkillLabSkillGameplayAbility> Ability;
		uint64 Transaction = 0;
		TStrongObjectPtr<USkillLabCastExecution> Execution;
	};
	FFixture MakeFixture()
	{
		FFixture Fixture;
		Fixture.Owner = World->SpawnActor<AActor>();
		Actors.Add(Fixture.Owner);
		Fixture.ASC = NewObject<USkillLabAbilitySystemComponent>(Fixture.Owner);
		Fixture.Owner->AddInstanceComponent(Fixture.ASC);
		Fixture.ASC->RegisterComponent();
		Fixture.Runtime = NewRuntime(Fixture.Owner);
		return Fixture;
	}
	USkillLabSkillRuntimeComponent* NewRuntime(AActor* Owner)
	{
		auto* Runtime = NewObject<USkillLabSkillRuntimeComponent>(Owner);
		Owner->AddInstanceComponent(Runtime);
		Runtime->RegisterComponent();
		if (!Runtime->HasBegunPlay()) Runtime->BeginPlay();
		Runtime->InitializeSkills();
		return Runtime;
	}
	FSkillLabSkillRequest Request(const FFixture& Fixture, FName Skill) const
	{
		FSkillLabSkillRequest Result;
		Result.Caster = Fixture.Owner;
		Result.SkillId = Skill;
		return Result;
	}
	bool MakeUnconfirmed(FFixture& Fixture)
	{
		const FSkillLabSkillResult Cast = Fixture.Runtime->SubmitSkillRequest(Request(Fixture, TEXT("Skill_Wait")));
		if (!Test->TestTrue(TEXT("No-receipt fixture activates through GAS"), Cast.bSuccess && SkillLabObservedBehavior(Fixture.Runtime))) return false;
		Fixture.Execution.Reset(Fixture.Runtime->FindMutableExecution(Cast.CastId));
		Fixture.Ability = SkillLabObservedAbility(Fixture.Runtime);
		FName Reason;
		if (!Test->TestTrue(TEXT("Formal submission reserves first cost"), Fixture.Runtime->BeginBehaviorSubmission(
			SkillLabObservedBehavior(Fixture.Runtime), Cast.CastId, 1, Reason))) return false;
		Fixture.Transaction = Fixture.Runtime->GetSingleExecution()->Submission.TransactionId;
		Test->AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		Fixture.Runtime->AbortBehaviorSubmission(SkillLabObservedBehavior(Fixture.Runtime), Cast.CastId, 1);
		Fixture.Records = Fixture.Owner->FindComponentByClass<USkillLabUnconfirmedSubmissions>();
		return Test->TestNotNull(TEXT("Independent Skills owner retains unresolved identity"), Fixture.Records);
	}
	bool Setup()
	{
		TestSettlementDestruction();
		Timeout = MakeFixture();
		if (!MakeUnconfirmed(Timeout)) return false;
		Test->TestEqual(TEXT("Reservation does not debit actual energy"), Timeout.Runtime->GetEnergyValue(), 100.0f);
		Test->TestEqual(TEXT("First unresolved submission reserves cost"), Timeout.Runtime->GetReservedEnergyValue(), 2.0f);
		Test->TestEqual(TEXT("Formal query exposes remaining available energy"), Timeout.Runtime->GetAvailableEnergyValue(), 98.0f);
		Test->TestEqual(TEXT("Same skill rejects during waiting"), Timeout.Runtime->SubmitSkillRequest(Request(Timeout, TEXT("Skill_Wait"))).Reason,
			FName(TEXT("SubmissionUnconfirmed")));
		Test->TestEqual(TEXT("Other skill may buffer until abnormal Cast ends"), Timeout.Runtime->SubmitSkillRequest(Request(Timeout, TEXT("Skill_Other"))).Reason,
			FName(TEXT("InputBufferedLatestWins")));
		Test->TestTrue(TEXT("Waiting fixture owns buffered intent"), Timeout.Runtime->PendingSkillIntent.IsSet());
		Timeout.Runtime->OnNativeSkillEvent.AddLambda([this](const FSkillLabSkillResult& Result)
		{
			if (Result.Event == ESkillLabSkillEvent::Cancelled && Result.Reason == TEXT("SubmissionUnconfirmed")) ++AbnormalTerminals;
		});
		ResetDuringWait = MakeFixture();
		if (!MakeUnconfirmed(ResetDuringWait)) return false;
		ResetDuringWait.ASC->RequestResourceChange(false, EGameplayModOp::Override, 60.0f);
		bool bResetReportedBlocked = false;
		const FDelegateHandle ResetHandle = ResetDuringWait.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
		{
			if (Result.Event == ESkillLabSkillEvent::Rejected && Result.Reason == TEXT("ResetBlockedByUnconfirmedSubmission")) bResetReportedBlocked = true;
		});
		ResetDuringWait.Runtime->ResetSkills();
		ResetDuringWait.Runtime->OnNativeSkillEvent.Remove(ResetHandle);
		Test->TestTrue(TEXT("Incomplete Reset explicitly reports unresolved settlement"), bResetReportedBlocked);
		Test->TestEqual(TEXT("Reset cancels waiting Cast"), ResetDuringWait.Runtime->GetActiveCastId(), 0);
		Test->TestEqual(TEXT("Blocked Reset preserves independently changed actual energy"), ResetDuringWait.Runtime->GetEnergyValue(), 60.0f);
		Test->TestTrue(TEXT("Reset retains same-skill isolation"), ResetDuringWait.Records->BlocksSkill(TEXT("Skill_Wait")));
		Test->TestTrue(TEXT("Reset retains ASC reservation"), ResetDuringWait.ASC->HasIndeterminateEnergyEscrow(ResetDuringWait.Transaction));
		DestroyedRuntime = MakeFixture();
		if (!MakeUnconfirmed(DestroyedRuntime)) return false;
		DestroyedRuntime.Runtime->DestroyComponent();
		DestroyedRuntime.Runtime = NewRuntime(DestroyedRuntime.Owner);
		Test->TestEqual(TEXT("Recreated Runtime reuses actor-level unresolved owner"),
			DestroyedRuntime.Owner->FindComponentByClass<USkillLabUnconfirmedSubmissions>(), DestroyedRuntime.Records);
		Test->TestEqual(TEXT("Same ASC blocks retry after Runtime destruction"), DestroyedRuntime.Runtime->SubmitSkillRequest(Request(DestroyedRuntime, TEXT("Skill_Wait"))).Reason,
			FName(TEXT("SubmissionUnconfirmed")));
		DestroyedASC = MakeFixture();
		if (!MakeUnconfirmed(DestroyedASC)) return false;
		DestroyedASC.ASC->DestroyComponent();
		DestroyedASC.Records->TickComponent(0.0f, LEVELTICK_All, nullptr);
		Test->TestTrue(TEXT("Destroyed ASC produces session-unconfirmable diagnosis"), DestroyedASC.Records->HasLostResourceOwnerDiagnosis());
		Test->TestTrue(TEXT("Runtime remains alive when its ASC is destroyed first"), IsValid(DestroyedASC.Runtime));
		Paid = MakeFixture();
		Test->TestTrue(TEXT("Another skill commits before pending cast"), Paid.Runtime->SubmitSkillRequest(Request(Paid, TEXT("Skill_OtherCooldown"))).bSuccess);
		const FSkillLabSkillResult PaidCast = Paid.Runtime->SubmitSkillRequest(Request(Paid, TEXT("Skill_PaidWait")));
		if (!Test->TestTrue(TEXT("Already-paid fixture activates formally"), PaidCast.bSuccess && SkillLabObservedBehavior(Paid.Runtime))) return false;
		FName Reason;
		Test->TestTrue(TEXT("Already-paid fixture begins first point"), Paid.Runtime->BeginBehaviorSubmission(SkillLabObservedBehavior(Paid.Runtime), PaidCast.CastId, 1, Reason));
		Test->TestTrue(TEXT("First point commits the unique cost"), Paid.Runtime->ResolveBehaviorSubmission(SkillLabObservedBehavior(Paid.Runtime), PaidCast.CastId, 1, true));
		Test->TestTrue(TEXT("Already-paid fixture begins next point"), Paid.Runtime->BeginBehaviorSubmission(SkillLabObservedBehavior(Paid.Runtime), PaidCast.CastId, 2, Reason));
		Test->AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		Paid.Runtime->AbortBehaviorSubmission(SkillLabObservedBehavior(Paid.Runtime), PaidCast.CastId, 2);
		Paid.Records = Paid.Owner->FindComponentByClass<USkillLabUnconfirmedSubmissions>();
		Test->TestTrue(TEXT("Paid unconfirmed identity is independently retained"), Paid.Records && Paid.Records->BlocksSkill(TEXT("Skill_PaidWait")));
		Test->TestEqual(TEXT("Already-paid unresolved submission has no new transaction"), Paid.Runtime->GetSingleExecution()->Submission.TransactionId, uint64(0));
		Test->TestEqual(TEXT("Later unconfirmed point reserves no second cost"), Paid.Runtime->GetReservedEnergyValue(), 0.0f);
		Test->TestEqual(TEXT("Both confirmed skills debit their cost once"), Paid.Runtime->GetEnergyValue(), 95.0f);
		PaidDeadline = World->GetTimeSeconds() + Paid.Runtime->GetCooldownRemaining(TEXT("Skill_PaidWait"));
		OtherDeadline = World->GetTimeSeconds() + Paid.Runtime->GetCooldownRemaining(TEXT("Skill_OtherCooldown"));
		Test->TestTrue(TEXT("Paid unresolved skill has a nonzero long cooldown"), Paid.Runtime->GetCooldownRemaining(TEXT("Skill_PaidWait")) > 10.0f);
		Paid.Runtime->DestroyComponent();
		Paid.Runtime = NewRuntime(Paid.Owner);
		CheckPaidCooldowns(TEXT("Rebuild during wait"));
		Test->TestEqual(TEXT("Paid unresolved point blocks retry after rebuild"), Paid.Runtime->SubmitSkillRequest(Request(Paid, TEXT("Skill_PaidWait"))).Reason,
			FName(TEXT("SubmissionUnconfirmed")));
		PaidAfterTimeout = MakeFixture();
		const FSkillLabSkillResult PaidTimeoutCast = PaidAfterTimeout.Runtime->SubmitSkillRequest(Request(PaidAfterTimeout, TEXT("Skill_PaidWait")));
		if (!Test->TestTrue(TEXT("Paid timeout fixture activates"), PaidTimeoutCast.bSuccess && SkillLabObservedBehavior(PaidAfterTimeout.Runtime))) return false;
		Test->TestTrue(TEXT("Paid timeout begins first point"), PaidAfterTimeout.Runtime->BeginBehaviorSubmission(SkillLabObservedBehavior(PaidAfterTimeout.Runtime), PaidTimeoutCast.CastId, 1, Reason));
		Test->TestTrue(TEXT("Paid timeout confirms first point"), PaidAfterTimeout.Runtime->ResolveBehaviorSubmission(SkillLabObservedBehavior(PaidAfterTimeout.Runtime), PaidTimeoutCast.CastId, 1, true));
		TimeoutPaidDeadline = World->GetTimeSeconds() + PaidAfterTimeout.Runtime->GetCooldownRemaining(TEXT("Skill_PaidWait"));
		Test->TestTrue(TEXT("Paid timeout begins second point"), PaidAfterTimeout.Runtime->BeginBehaviorSubmission(SkillLabObservedBehavior(PaidAfterTimeout.Runtime), PaidTimeoutCast.CastId, 2, Reason));
		Test->AddExpectedError(TEXT("Submission result indeterminate"), EAutomationExpectedErrorFlags::Contains, 1);
		PaidAfterTimeout.Runtime->AbortBehaviorSubmission(SkillLabObservedBehavior(PaidAfterTimeout.Runtime), PaidTimeoutCast.CastId, 2);
		for (int32 Mode = 0; Mode < 3; ++Mode)
		{
			FFixture Late = MakeFixture();
			auto* Records = Late.Owner->FindComponentByClass<USkillLabUnconfirmedSubmissions>();
			if (!Test->TestNotNull(TEXT("Late-record fixture has independent owner"), Records)) return false;
			FSkillLabUnconfirmedSubmission First;
			First.SkillId = FName(*FString::Printf(TEXT("Skill_Diagnostic_%dA"), Mode));
			First.RuntimeIdentity = FObjectKey(Late.Runtime);
			First.RuntimeId = Late.Runtime->GetUniqueID();
			First.CastId = 1;
			FSkillLabUnconfirmedSubmission Second = First;
			Second.SkillId = FName(*FString::Printf(TEXT("Skill_Diagnostic_%dB"), Mode));
			Second.CastId = 2;
			for (const FSkillLabUnconfirmedSubmission& Expected : {First, Second})
				Test->AddExpectedMessagePlain(FString::Printf(TEXT("Session cannot confirm: resource owner or actor destroyed; Skill=%s "), *Expected.SkillId.ToString()),
					ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			Records->Record(First);
			if (Mode == 1) Late.Owner->Destroy();
			else if (Mode == 2)
			{
				Late.ASC->DestroyComponent();
				Records->TickComponent(0.0f, LEVELTICK_All, nullptr);
			}
			else
			{
				if (!Records->HasBegunPlay()) Records->BeginPlay();
				Records->EndPlay(EEndPlayReason::RemovedFromWorld);
			}
			// 所有者 API 的延迟交接边界，不伪造生产者权威回执。
			Records->Record(Second);
			Records->Record(First);
			Records->Record(Second);
			if (Records->IsRegistered() && Records->HasBegunPlay()) Records->TickComponent(0.0f, LEVELTICK_All, nullptr);
			Test->TestEqual(TEXT("Repeated handoff retains exactly two execution records"), Records->GetRecords().Num(), 2);
			Test->TestTrue(TEXT("Record arriving after owner termination is immediately diagnosed"), Records->HasLostResourceOwnerDiagnosis());
		}
		return true;
	}
	void Finish()
	{
		CheckPaidCooldowns(TEXT("Cooldown advances with elapsed world time"));
		Test->TestTrue(TEXT("Original deadline decreases while Runtime replacement lives"), Paid.Runtime->GetCooldownRemaining(TEXT("Skill_PaidWait")) < 19.5f);
		Paid.Runtime->ResetSkills();
		CheckPaidCooldowns(TEXT("Blocked Reset preserves deadlines"));
		Test->TestEqual(TEXT("Other skill still obeys original cooldown after rebuild"), Paid.Runtime->SubmitSkillRequest(Request(Paid, TEXT("Skill_OtherCooldown"))).Reason, FName(TEXT("Cooldown")));
		Test->TestEqual(TEXT("Paid unresolved Cast times out"), PaidAfterTimeout.Runtime->GetActiveCastId(), 0);
		PaidAfterTimeout.Runtime->DestroyComponent();
		PaidAfterTimeout.Runtime = NewRuntime(PaidAfterTimeout.Owner);
		Test->TestTrue(TEXT("Rebuild after abnormal terminal preserves original cooldown deadline"), FMath::IsNearlyEqual(
			PaidAfterTimeout.Runtime->GetCooldownRemaining(TEXT("Skill_PaidWait")), static_cast<float>(TimeoutPaidDeadline - World->GetTimeSeconds()), 0.01f));
		Test->TestEqual(TEXT("Rebuild after abnormal terminal does not debit again"), PaidAfterTimeout.Runtime->GetEnergyValue(), 98.0f);
		Test->TestEqual(TEXT("Rebuild after abnormal terminal adds no reservation"), PaidAfterTimeout.Runtime->GetReservedEnergyValue(), 0.0f);
		Test->TestEqual(TEXT("Rebuild after abnormal terminal still isolates skill"), PaidAfterTimeout.Runtime->SubmitSkillRequest(Request(PaidAfterTimeout, TEXT("Skill_PaidWait"))).Reason, FName(TEXT("SubmissionUnconfirmed")));
		Test->TestEqual(TEXT("Real finite wait emits one distinguishable abnormal terminal"), AbnormalTerminals, 1);
		Test->TestEqual(TEXT("Real finite wait releases active Cast"), Timeout.Runtime->GetActiveCastId(), 0);
		Test->TestEqual(TEXT("ASC-first destruction still lets finite wait release Runtime Cast"), DestroyedASC.Runtime->GetActiveCastId(), 0);
		Test->TestFalse(TEXT("ASC-first destruction leaves no live wait timer"), World->GetTimerManager().IsTimerActive(DestroyedASC.Execution->IndeterminateTimer));
		Test->TestFalse(TEXT("Real finite wait releases GAS Ability lock"), Timeout.Ability.IsValid() && Timeout.Ability->IsSkillAbilityActive());
		Test->TestFalse(TEXT("Abnormal terminal clears buffered input"), Timeout.Runtime->PendingSkillIntent.IsSet());
		Test->TestFalse(TEXT("Finite wait timer is cleared"), World->GetTimerManager().IsTimerActive(Timeout.Execution->IndeterminateTimer));
		Test->TestFalse(TEXT("Phase timer is cleared"), World->GetTimerManager().IsTimerActive(Timeout.Execution->PhaseTimer));
		Test->TestFalse(TEXT("No active coordinator or queued resource write"), Timeout.ASC->HasPendingResourceChanges());
		Test->TestTrue(TEXT("No receipt leaves reservation unresolved after Cast cleanup"), Timeout.ASC->HasIndeterminateEnergyEscrow(Timeout.Transaction));
		Test->TestEqual(TEXT("No receipt does not start cooldown"), Timeout.Runtime->GetCooldownRemaining(TEXT("Skill_Wait")), 0.0f);
		Test->TestEqual(TEXT("Same skill rejects after wait expires"), Timeout.Runtime->SubmitSkillRequest(Request(Timeout, TEXT("Skill_Wait"))).Reason,
			FName(TEXT("SubmissionUnconfirmed")));
		Test->TestTrue(TEXT("Other skill activates using unreserved available funds"), Timeout.Runtime->SubmitSkillRequest(Request(Timeout, TEXT("Skill_Other"))).bSuccess);
		Test->TestEqual(TEXT("Other skill charges its own cost exactly once"), Timeout.Runtime->GetEnergyValue(), 97.0f);
		Test->TestEqual(TEXT("Other skill leaves old reservation intact"), Timeout.Runtime->GetReservedEnergyValue(), 2.0f);
		Test->TestEqual(TEXT("Other expensive skill still obeys available gate"), Timeout.Runtime->SubmitSkillRequest(Request(Timeout, TEXT("Skill_Expensive"))).Reason,
			FName(TEXT("InsufficientEnergy")));
		Timeout.Runtime->ResetSkills();
		Test->TestEqual(TEXT("Reset after terminal does not restore reserved balance"), Timeout.Runtime->GetEnergyValue(), 97.0f);
		Test->TestTrue(TEXT("Reset after terminal retains unresolved identity"), Timeout.Records->BlocksSkill(TEXT("Skill_Wait")));
		Test->TestEqual(TEXT("Owner holds only original unresolved identity"), Timeout.Records->GetRecords().Num(), 1);
		if (!Timeout.Records->GetRecords().IsEmpty())
			Test->TestEqual(TEXT("Owner identity matches unique ASC transaction"), Timeout.Records->GetRecords()[0].TransactionId, Timeout.Transaction);
		Timeout.Runtime->DestroyComponent();
		Timeout.Runtime = NewRuntime(Timeout.Owner);
		Test->TestEqual(TEXT("Runtime rebuilt after terminal still rejects original skill"),
			Timeout.Runtime->SubmitSkillRequest(Request(Timeout, TEXT("Skill_Wait"))).Reason, FName(TEXT("SubmissionUnconfirmed")));
		Test->TestTrue(TEXT("Runtime rebuilt after terminal does not consume reservation"), Timeout.ASC->HasIndeterminateEnergyEscrow(Timeout.Transaction));
		Timeout.ASC->DestroyComponent();
		Timeout.Records->TickComponent(0.0f, LEVELTICK_All, nullptr);
		Test->TestTrue(TEXT("ASC destruction after terminal diagnoses unresolved record"), Timeout.Records->HasLostResourceOwnerDiagnosis());
		Timeout.Owner->Destroy();
		Test->TestTrue(TEXT("Actor destruction after terminal preserves unconfirmed diagnosis"), Timeout.Records->HasLostResourceOwnerDiagnosis());
		FFixture DestroyedActor = MakeFixture();
		if (MakeUnconfirmed(DestroyedActor))
		{
			DestroyedActor.Owner->Destroy();
			Test->TestTrue(TEXT("Actor destruction diagnoses unresolved session result"), DestroyedActor.Records->HasLostResourceOwnerDiagnosis());
		}
	}
	FAutomationTestBase* Test;
	void TestSettlementDestruction()
	{
		for (const bool bDestroyActor : {true, false})
		{
			FFixture Fixture = MakeFixture();
			Fixture.Runtime->SubmitSkillRequest(Request(Fixture, TEXT("Skill_PaidWait")));
			Fixture.Execution.Reset(Fixture.Runtime->FindMutableExecution(Fixture.Runtime->GetActiveCastId()));
			auto* Behavior = Cast<USkillLabLuaSkillBehavior>(SkillLabObservedBehavior(Fixture.Runtime));
			if (!Test->TestNotNull(TEXT("Settlement lifetime fixture uses real behavior"), Behavior)) continue;
			auto* Session = Fixture.Owner->FindComponentByClass<USkillLabUnconfirmedSubmissions>();
			const auto Ability = SkillLabObservedAbility(Fixture.Runtime);
			int32 DebitCallbacks = 0, Effects = 0, Commits = 0;
			float DebitedEnergy = 0.0f;
			const FDelegateHandle EventHandle = Fixture.Runtime->OnNativeSkillEvent.AddLambda([&](const FSkillLabSkillResult& Result)
			{
				if (Result.Event == ESkillLabSkillEvent::EffectApplied) ++Effects;
				if (Result.Event == ESkillLabSkillEvent::SubmissionCommitted && Result.bSuccess) ++Commits;
			});
			Test->AddExpectedMessagePlain(FString::Printf(TEXT("Submission delivery unavailable: Skill=Skill_PaidWait Cast=1 Transaction=1 ActionSucceeded=1 ResourceResolved=%d CooldownRecorded=0"), bDestroyActor ? 0 : 1),
				ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);
			FDelegateHandle DebitHandle;
			DebitHandle = Fixture.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).AddLambda(
				[&](const FOnAttributeChangeData& Change)
				{
					if (Change.NewValue >= Change.OldValue) return;
					++DebitCallbacks;
					DebitedEnergy = Change.NewValue;
					Fixture.ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute()).Remove(DebitHandle);
					if (bDestroyActor) Fixture.Owner->Destroy();
					else
					{
						if (!Session->HasBegunPlay()) Session->BeginPlay();
						Session->EndPlay(EEndPlayReason::RemovedFromWorld);
					}
				});
			Test->TestFalse(TEXT("Lost delivery owner never reports complete submission success"), Behavior->RequestDeclaredAction());
			if (IsValid(Fixture.Runtime)) Fixture.Runtime->OnNativeSkillEvent.Remove(EventHandle);
			Test->TestEqual(TEXT("Irreversible effect executes once"), Effects, 1);
			Test->TestEqual(TEXT("Cost notification executes once"), DebitCallbacks, 1);
			Test->TestEqual(TEXT("Cost is debited at most once"), DebitedEnergy, 98.0f);
			Test->TestEqual(TEXT("No complete success event without cooldown recipient"), Commits, 0);
			Test->TestEqual(TEXT("Behavior distinguishes delivery failure from unknown resource result"), Behavior->GetEffectResult().Reason, FName(TEXT("SubmissionDeliveryUnavailable")));
			Test->TestEqual(TEXT("Destroyed flow clears local Cast identity"), Fixture.Runtime->GetActiveCastId(), 0);
			Test->TestFalse(TEXT("Destroyed flow closes local submission lock"), Fixture.Runtime->AnySubmitting());
			Test->TestFalse(TEXT("Destroyed flow removes wait timer"), World->GetTimerManager().IsTimerActive(Fixture.Execution->IndeterminateTimer));
			Test->TestFalse(TEXT("Destroyed flow removes phase timer"), World->GetTimerManager().IsTimerActive(Fixture.Execution->PhaseTimer));
			Test->TestTrue(TEXT("Destroyed flow releases behavior ownership"), SkillLabObservedBehavior(Fixture.Runtime) == nullptr);
			Test->TestFalse(TEXT("No active Ability survives receiver termination"), Ability.IsValid() && Ability->IsSkillAbilityActive());
			if (!bDestroyActor)
			{
				Test->TestTrue(TEXT("Ended session remains a valid UObject in this branch"), IsValid(Session));
				Test->TestEqual(TEXT("Confirmed energy remains debited after session EndPlay"), Fixture.ASC->GetActualEnergy(), 98.0f);
				Test->TestEqual(TEXT("Failed cooldown write is visible rather than fabricated"), Session->GetCooldownRemaining(TEXT("Skill_PaidWait")), 0.0f);
				Test->TestFalse(TEXT("Confirmed reservation is not reopened"), Fixture.ASC->IsEnergyEscrowActive());
				Test->TestEqual(TEXT("Ended session cannot silently accept another cast"), Fixture.Runtime->SubmitSkillRequest(Request(Fixture, TEXT("Skill_Other"))).Reason, FName(TEXT("SessionOwnerUnavailable")));
			}
		}
	}
	void CheckPaidCooldowns(const TCHAR* Context)
	{
		Test->TestTrue(Context, FMath::IsNearlyEqual(Paid.Runtime->GetCooldownRemaining(TEXT("Skill_PaidWait")), static_cast<float>(PaidDeadline - World->GetTimeSeconds()), 0.01f));
		Test->TestTrue(TEXT("Other confirmed cooldown keeps original deadline"), FMath::IsNearlyEqual(Paid.Runtime->GetCooldownRemaining(TEXT("Skill_OtherCooldown")), static_cast<float>(OtherDeadline - World->GetTimeSeconds()), 0.01f));
		Test->TestEqual(TEXT("Rebuild and Reset never repeat confirmed cost"), Paid.Runtime->GetEnergyValue(), 95.0f);
		Test->TestEqual(TEXT("Rebuild and Reset never add a reservation"), Paid.Runtime->GetReservedEnergyValue(), 0.0f);
	}
	UWorld* World;
	TMap<FName, FSkillLabSkillDefinition> PreviousDefinitions;
	bool bPreviousLoadAttempted;
	TArray<TWeakObjectPtr<AActor>> Actors;
	FFixture Timeout, ResetDuringWait, DestroyedRuntime, DestroyedASC, Paid, PaidAfterTimeout;
	double PaidDeadline = 0.0, OtherDeadline = 0.0, TimeoutPaidDeadline = 0.0;
	bool bStarted = false;
	double StartedAt = 0.0, LastTickAt = 0.0;
	int32 AbnormalTerminals = 0;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabUnconfirmedSubmissionTest, "SkillLab.Skills.UnconfirmedSubmission",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabUnconfirmedSubmissionTest::RunTest(const FString&)
{
	UWorld* World = CreateSkillLabSubmissionTestWorld();
	if (!TestNotNull(TEXT("Automation world exists"), World)) return false;
	TMap<FName, FSkillLabSkillDefinition> Previous;
	const bool bPreviousLoad = FSkillLabSkillDefinitionProvider::CopyDefinitionsForAutomation(Previous);
	TMap<FName, FSkillLabSkillDefinition> Definitions;
	TMap<FString, FString> Sources;
	Sources.Add(TEXT("Hold.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) end"));
	Sources.Add(TEXT("Other.lua"), TEXT("SkillLabSkill_Define(\"targetShape\",\"Self\",\"relation\",\"Self\",\"effect\",\"Healing\",\"execution\",\"InstantTarget\")\nfunction OnSkillCast(context) SkillLabSkill_RequestAction(); SkillLabSkill_Finish() end"));
	FString Error;
	if (!TestTrue(TEXT("No-receipt test catalog parses"), FSkillLabSkillDefinitionProvider::ParseCatalogText(
		TEXT(R"({"skills":[{"id":"Skill_Wait","luaScript":"Hold.lua","cost":2,"cooldown":3,"magnitude":1,"range":1000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},{"id":"Skill_PaidWait","luaScript":"Hold.lua","cost":2,"cooldown":20,"magnitude":1,"range":1000,"active":10,"completionEventTag":"SkillLab.Event.Cast.Complete"},{"id":"Skill_Other","luaScript":"Other.lua","cost":3,"cooldown":0,"magnitude":1,"range":1000},{"id":"Skill_OtherCooldown","luaScript":"Other.lua","cost":3,"cooldown":25,"magnitude":1,"range":1000},{"id":"Skill_Expensive","luaScript":"Other.lua","cost":98,"cooldown":0,"magnitude":1,"range":1000}]})"), Sources, Definitions, Error)))
	{ AddError(Error); World->DestroyWorld(false); if (GEngine) GEngine->DestroyWorldContext(World); return false; }
	FSkillLabSkillDefinitionProvider::ReplaceDefinitionsForAutomation(Definitions);
	ADD_LATENT_AUTOMATION_COMMAND(FSkillLabUnconfirmedWaitCommand(this, World, Previous, bPreviousLoad));
	return true;
}

#endif
