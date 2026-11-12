#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayEffect.h"
#include "SkillLabAbilitySystemComponent.h"
#include "SkillLabCombatAttributeSet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSkillLabResourceWriteIntegrationTest, "SkillLab.GAS.ResourceWriteIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSkillLabResourceWriteIntegrationTest::RunTest(const FString&)
{
	const auto Values = UWorld::InitializationValues().AllowAudioPlayback(false).RequiresHitProxies(false)
		.CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	if (!TestNotNull(TEXT("Independent resource integration world"), World)) return false;
	if (GEngine) GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	TArray<AActor*> Owners;
	ON_SCOPE_EXIT
	{
		for (auto* Owner : Owners) if (IsValid(Owner)) Owner->Destroy();
		World->DestroyWorld(false);
		if (GEngine) GEngine->DestroyWorldContext(World);
	};
	const auto MakeASC = [&]()
	{
		auto* Owner = World->SpawnActor<AActor>();
		Owners.Add(Owner);
		auto* ASC = NewObject<USkillLabAbilitySystemComponent>(Owner);
		Owner->AddInstanceComponent(ASC);
		ASC->RegisterComponent();
		ASC->InitializeSkillLabAbilityActorInfo(Owner, Owner);
		if (!ASC->HasBegunPlay()) ASC->BeginPlay();
		return ASC;
	};
	for (const int32 Mode : {0, 1, 2})
	{
		auto* ASC = MakeASC();
		TestTrue(TEXT("Formal coordinator accepts finite saturating write"), ASC->RequestResourceChange(Mode == 2,
			Mode == 0 ? EGameplayModOp::Additive : EGameplayModOp::Override, Mode == 0 ? 25.0f : (Mode == 1 ? 150.0f : -10.0f)));
		const float Expected = Mode == 2 ? 0.0f : 100.0f;
		TestEqual(TEXT("Clamped Energy current is correct"), ASC->GetActualEnergy(), Expected);
		TestEqual(TEXT("Clamped Energy base matches confirmed current"), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()), Expected);
		TestEqual(TEXT("Maximum base matches clamped current"), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()), ASC->GetCombatAttributeSet()->GetMaxEnergy());
		const uint64 Id = ASC->AllocateEnergyTransactionId();
		const bool bBegan = ASC->BeginEnergyEscrow(Id, Mode == 2 ? 0.0f : 2.0f);
		TestTrue(TEXT("Legal saturated resource write does not poison later reservation"), bBegan);
		UE_LOG(LogTemp, Display, TEXT("[SkillLabResourceRepair] Clamp Mode=%d Current=%.1f Base=%.1f Max=%.1f MaxBase=%.1f Begin=%d"),
			Mode, ASC->GetActualEnergy(), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()),
			ASC->GetCombatAttributeSet()->GetMaxEnergy(), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetMaxEnergyAttribute()), bBegan);
		if (bBegan) TestTrue(TEXT("Independent reservation explicitly releases"), ASC->ResolveEnergyEscrow(Id, false));
	}
	{
		auto* ASC = MakeASC();
		int32 Count = 0;
		FSkillLabResourceReceipt Receipt;
		auto Finished = [&](const FSkillLabResourceReceipt& Value) { ++Count; Receipt = Value; };
		TestTrue(TEXT("Tracked immediate applied"), ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 90.0f, 1, ASC->GetOwner(), [] { return NAME_None; }, Finished) == ESkillLabResourceChangeResult::Applied);
		TestTrue(TEXT("Immediate receipt identity and result"), Count == 1 && Receipt.AttemptId == 1 && Receipt.ASC == FObjectKey(ASC) && Receipt.Result == ESkillLabResourceCompletion::Applied);
		TestTrue(TEXT("Permission denial rejected"), ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 20.0f, 2, ASC->GetOwner(), [] { return FName(TEXT("Denied")); }, Finished) == ESkillLabResourceChangeResult::Rejected);
		TestTrue(TEXT("Permission receipt once"), Count == 2 && Receipt.Result == ESkillLabResourceCompletion::Rejected && Receipt.Reason == TEXT("Denied"));
		TestEqual(TEXT("Denied request does not write"), ASC->GetActualEnergy(), 90.0f);
	}
	{
		auto* ASC = MakeASC();
		int32 Finished = 0;
		bool bRequested = false;
		ESkillLabResourceCompletion Results[2] = {};
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (bRequested) return;
			bRequested = true;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				const auto Return = ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 60.0f + Index, 10 + Index, ASC->GetOwner(),
					[Index] { return Index == 0 ? FName(TEXT("Denied")) : NAME_None; },
					[&, Index](const FSkillLabResourceReceipt& R) { Results[Index] = R.Result; ++Finished; if (Index == 0) ASC->RequestResourceChange(false, EGameplayModOp::Additive, 1.0f); });
				TestTrue(TEXT("Tracked reentry queues"), Return == ESkillLabResourceChangeResult::Queued);
			}
			TestEqual(TEXT("No receipt inside resource write"), Finished, 0);
		});
		ASC->RequestResourceChange(false, EGameplayModOp::Override, 80.0f);
		Delegate.Remove(Handle);
		TestTrue(TEXT("Both queued receipts exactly once"), Finished == 2 && Results[0] == ESkillLabResourceCompletion::Rejected && Results[1] == ESkillLabResourceCompletion::Applied);
		TestEqual(TEXT("Later queue item retained"), ASC->GetActualEnergy(), 61.0f);
	}
	{
		auto* ASC = MakeASC();
		int32 Finished = 0;
		bool bRequested = false;
		ESkillLabResourceCompletion Result = ESkillLabResourceCompletion::Applied;
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (bRequested) return;
			bRequested = true;
			ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 50.0f, 20, ASC->GetOwner(), [] { return NAME_None; },
				[&](const FSkillLabResourceReceipt& R) { ++Finished; Result = R.Result; });
			ASC->EndPlay(EEndPlayReason::RemovedFromWorld);
			TestEqual(TEXT("Session closure waits for safe point"), Finished, 0);
		});
		ASC->RequestResourceChange(false, EGameplayModOp::Override, 80.0f);
		Delegate.Remove(Handle);
		TestTrue(TEXT("Session closure completes queued receipt once"), Finished == 1 && Result == ESkillLabResourceCompletion::Unavailable);
	}
	{
		auto* ASC = MakeASC();
		int32 Finished = 0;
		bool bRequested = false;
		ESkillLabResourceCompletion Result = ESkillLabResourceCompletion::Applied;
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (bRequested) return;
			bRequested = true;
			ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 50.0f, 23, ASC->GetOwner(), [] { return NAME_None; },
				[&](const FSkillLabResourceReceipt& R) { ++Finished; Result = R.Result; });
			ASC->ConditionalBeginDestroy();
		});
		ASC->RequestResourceChange(false, EGameplayModOp::Override, 80.0f);
		Delegate.Remove(Handle);
		TestTrue(TEXT("BeginDestroy closes queued receipt once"), Finished == 1 && Result == ESkillLabResourceCompletion::Unavailable);
	}
	{
		auto* ASC = MakeASC();
		int32 Finished = 0;
		bool bRequested = false;
		auto* Receiver = World->SpawnActor<AActor>();
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (bRequested) return;
			bRequested = true;
			ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 50.0f, 21, Receiver, [] { return NAME_None; },
				[&](const FSkillLabResourceReceipt&) { ++Finished; });
			Receiver->Destroy();
		});
		ASC->RequestResourceChange(false, EGameplayModOp::Override, 80.0f);
		Delegate.Remove(Handle);
		TestEqual(TEXT("Invalid receiver is not called"), Finished, 0);
		TestEqual(TEXT("Invalid receiver request does not write"), ASC->GetActualEnergy(), 80.0f);
		TestFalse(TEXT("Invalid receiver queue cleared"), ASC->HasPendingResourceChanges());
	}
	{
		auto* ASC = MakeASC();
		int32 Finished = 0;
		bool bBypassed = false;
		ESkillLabResourceCompletion Result = ESkillLabResourceCompletion::Applied;
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const auto Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (!bBypassed) { bBypassed = true; ASC->SetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute(), 70.0f); }
		});
		AddExpectedError(TEXT("Unsupported reentrant Energy/MaxEnergy write"), EAutomationExpectedErrorFlags::Contains, 1);
		AddExpectedError(TEXT("Coordinated Energy/MaxEnergy change violated"), EAutomationExpectedErrorFlags::Contains, 1);
		ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 80.0f, 22, ASC->GetOwner(), [] { return NAME_None; },
			[&](const FSkillLabResourceReceipt& R) { ++Finished; Result = R.Result; });
		Delegate.Remove(Handle);
		TestTrue(TEXT("Attempted bypass reports failed once"), Finished == 1 && Result == ESkillLabResourceCompletion::Failed);
	}
	{
		auto* ASC = MakeASC();
		const uint64 First = ASC->AllocateEnergyTransactionId();
		const uint64 Second = ASC->AllocateEnergyTransactionId();
		TestTrue(TEXT("First reservation"), ASC->BeginEnergyEscrow(First, 40.0f));
		TestTrue(TEXT("Second reservation"), ASC->BeginEnergyEscrow(Second, 30.0f));
		TestTrue(TEXT("Write below reservations rejected"), ASC->RequestResourceChangeDetailed(false, EGameplayModOp::Override, 20.0f) == ESkillLabResourceChangeResult::Rejected);
		TestEqual(TEXT("Rejected write preserves actual"), ASC->GetActualEnergy(), 100.0f);
		TestEqual(TEXT("Rejected write preserves reserved"), ASC->GetReservedEnergy(), 70.0f);
		TestTrue(TEXT("First reservation commits"), ASC->ResolveEnergyEscrow(First, true));
		TestTrue(TEXT("Second reservation releases"), ASC->ResolveEnergyEscrow(Second, false));
		TestFalse(TEXT("Duplicate commit rejected"), ASC->ResolveEnergyEscrow(First, true));
		TestEqual(TEXT("Only one debit"), ASC->GetActualEnergy(), 60.0f);
		TestEqual(TEXT("Reservations cleared"), ASC->GetReservedEnergy(), 0.0f);
		TestTrue(TEXT("Lower maximum clamps current"), ASC->RequestResourceChangeDetailed(true, EGameplayModOp::Override, 50.0f) == ESkillLabResourceChangeResult::Applied);
		TestEqual(TEXT("Lower maximum clamps energy base"), ASC->GetNumericAttributeBase(USkillLabCombatAttributeSet::GetEnergyAttribute()), 50.0f);
	}
	{
		auto* ASC = MakeASC();
		ESkillLabResourceChangeResult Nested = ESkillLabResourceChangeResult::Rejected;
		bool bRequested = false;
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const FDelegateHandle Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (!bRequested)
			{
				bRequested = true;
				Nested = ASC->RequestResourceChangeDetailed(false, EGameplayModOp::Additive, -10.0f);
			}
		});
		TestTrue(TEXT("Outer coordinated change applied"), ASC->RequestResourceChangeDetailed(false, EGameplayModOp::Override, 80.0f) == ESkillLabResourceChangeResult::Applied);
		Delegate.Remove(Handle);
		TestTrue(TEXT("Reentrant change reports queued"), Nested == ESkillLabResourceChangeResult::Queued);
		TestEqual(TEXT("Queued change drains after outer write"), ASC->GetActualEnergy(), 70.0f);
	}
	{
		auto* ASC = MakeASC();
		const uint64 Id = ASC->AllocateEnergyTransactionId();
		TestTrue(TEXT("Reserve before queued rejection"), ASC->BeginEnergyEscrow(Id, 80.0f));
		ESkillLabResourceChangeResult Nested = ESkillLabResourceChangeResult::Rejected;
		int32 ReceiptCount = 0;
		ESkillLabResourceCompletion Final = ESkillLabResourceCompletion::Applied;
		bool bRequested = false;
		auto& Delegate = ASC->GetGameplayAttributeValueChangeDelegate(USkillLabCombatAttributeSet::GetEnergyAttribute());
		const FDelegateHandle Handle = Delegate.AddLambda([&](const FOnAttributeChangeData&)
		{
			if (!bRequested) { bRequested = true; Nested = ASC->RequestResourceChangeTracked(false, EGameplayModOp::Override, 10.0f, 30, ASC->GetOwner(), [] { return NAME_None; }, [&](const FSkillLabResourceReceipt& R) { ++ReceiptCount; Final = R.Result; }); }
		});
		TestTrue(TEXT("Outer write above reservation"), ASC->RequestResourceChangeDetailed(false, EGameplayModOp::Override, 90.0f) == ESkillLabResourceChangeResult::Applied);
		Delegate.Remove(Handle);
		TestTrue(TEXT("Nested request was queued"), Nested == ESkillLabResourceChangeResult::Queued);
		TestTrue(TEXT("Queued reservation conflict completes rejected exactly once"), ReceiptCount == 1 && Final == ESkillLabResourceCompletion::Rejected);
		TestEqual(TEXT("Queued invalid write rejected on drain"), ASC->GetActualEnergy(), 90.0f);
		TestTrue(TEXT("Queued rejection preserves reservation"), ASC->ResolveEnergyEscrow(Id, false));
	}
	{
		auto* ASC = MakeASC();
		auto* Effect = NewObject<UGameplayEffect>(GetTransientPackage());
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = USkillLabCombatAttributeSet::GetEnergyAttribute();
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FScalableFloat(-10.0f);
		Effect->Modifiers.Add(Modifier);
		FGameplayEffectSpec Spec(Effect, ASC->MakeEffectContext(), 1.0f);
		const uint64 Id = ASC->AllocateEnergyTransactionId();
		TestTrue(TEXT("Reservation before resource GE"), ASC->BeginEnergyEscrow(Id, 80.0f));
		ASC->ApplyGameplayEffectSpecToSelf(Spec);
		TestEqual(TEXT("Raw resource GE rejected before write"), ASC->GetActualEnergy(), 100.0f);
		FName Reason;
		TestTrue(TEXT("Coordinated resource GE applied"), ASC->RequestResourceEffect(Spec, Reason) == ESkillLabResourceChangeResult::Applied);
		TestEqual(TEXT("Coordinated GE updates resource"), ASC->GetActualEnergy(), 90.0f);
		TestEqual(TEXT("Coordinated GE preserves reservation"), ASC->GetReservedEnergy(), 80.0f);
		Effect->DurationPolicy = EGameplayEffectDurationType::Infinite;
		FGameplayEffectSpec DurationSpec(Effect, ASC->MakeEffectContext(), 1.0f);
		TestFalse(TEXT("Resource duration GE has no active handle"), ASC->ApplyGameplayEffectSpecToSelf(DurationSpec).IsValid());
		TestTrue(TEXT("Resource duration GE rejected by coordinator"), ASC->RequestResourceEffect(DurationSpec, Reason) == ESkillLabResourceChangeResult::Rejected);
		TestEqual(TEXT("Rejected duration GE preserves actual"), ASC->GetActualEnergy(), 90.0f);
		TestTrue(TEXT("GE reservation released"), ASC->ResolveEnergyEscrow(Id, false));
	}
	return true;
}
#endif
