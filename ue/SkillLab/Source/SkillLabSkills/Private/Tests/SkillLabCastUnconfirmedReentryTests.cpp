#include "SkillLabCastScenarioFixture.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabCastUnconfirmedReentryTest, "SkillLab.Skills.CastUnconfirmedReentry",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSkillLabCastUnconfirmedReentryTest::RunTest(const FString&)
{
	FSkillLabCastScenarioFixture Fixture(this);
	if (!Fixture.bReady) return false;
	auto Unit = Fixture.MakeUnit();
	auto* World = Unit.Actor->GetWorld();
	auto* Owner = Unit.Actor;
	auto* ASC = Unit.ASC;
	auto* Runtime = Unit.Runtime;
	FSkillLabSkillRequest HoldRequest; HoldRequest.Caster = Owner; HoldRequest.SkillId = TEXT("Skill_Hold");
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
	const bool bHadBufferedIntent = FSkillLabCastScenarioFixture::HasIntent(Runtime);
	TestTrue(TEXT("Unconfirmed cast retains a buffered intent for the late-callback test"), bHadBufferedIntent);
	Runtime->CompleteBehavior(BypassBehavior, BypassCast.CastId, TEXT("LateComplete"));
	TestEqual(TEXT("Late completion cannot end an unconfirmed cast"), Runtime->GetActiveCastId(), BypassCast.CastId);
	TestEqual(TEXT("Late completion cannot consume buffered input"), FSkillLabCastScenarioFixture::HasIntent(Runtime), bHadBufferedIntent);
	if (BypassBehavior)
	{
		FSkillLabCastScenarioFixture::LateProjectileResult(BypassBehavior, Owner, TEXT("Hit"));
		FSkillLabCastScenarioFixture::LateProjectileResult(BypassBehavior, nullptr, TEXT("Expired"));
		FSkillLabCastScenarioFixture::LateProjectileResult(BypassBehavior, nullptr, TEXT("Destroyed"));
		TestTrue(TEXT("Late projectile results preserve pending state"), BypassBehavior->IsActionPending());
		TestFalse(TEXT("Late projectile results cannot complete the action"), BypassBehavior->IsActionCompleted());
	}
	TestEqual(TEXT("Late projectile results cannot end the cast"), Runtime->GetActiveCastId(), BypassCast.CastId);
	TestEqual(TEXT("Late projectile results cannot consume buffered input"), FSkillLabCastScenarioFixture::HasIntent(Runtime), bHadBufferedIntent);
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
	return true;
}
#endif
