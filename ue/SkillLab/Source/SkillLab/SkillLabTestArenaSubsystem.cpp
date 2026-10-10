// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkillLabTestArenaSubsystem.h"

#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "SkillLabCharacter.h"
#include "SkillLabSkillRuntimeComponent.h"
#include "Components/CapsuleComponent.h"
#include "CollisionShape.h"

void USkillLabTestArenaSubsystem::PrepareForSkillTesting(APawn* Caster)
{
	ConfigureSkillTestArena();
	SpawnTrainingTargets(Caster);
}

void USkillLabTestArenaSubsystem::SpawnTrainingTargets(APawn* Caster)
{
	if (!GetWorld() || bTargetsSpawned)
	{
		return;
	}

	if (!Caster)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SkillLabTestArena] Cannot spawn training targets without a possessed pawn"));
		return;
	}

	bTargetsSpawned = true;
	FVector Forward = Caster->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
	const float CapsuleHalfHeight = ASkillLabCharacter::StaticClass()->GetDefaultObject<ASkillLabCharacter>()->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();

	for (int32 Index = 0; Index < 3; ++Index)
	{
		// Search a forward-facing open patch. The trace validates the floor, then a
		// capsule overlap validates that the complete dummy footprint is unobstructed.
		ASkillLabCharacter* Target = nullptr;
		FVector DummyLocation = FVector::ZeroVector;
		FVector GroundLocation = FVector::ZeroVector;
		bool bFoundOpenLocation = false;
		for (int32 Row = 0; Row < 5 && !bFoundOpenLocation; ++Row)
		{
			const FVector GroundCandidate = Caster->GetActorLocation()
				+ Forward * (1200.0f + Row * 250.0f)
				+ Right * ((Index - 1) * 650.0f);
			FCollisionQueryParams GroundQuery(SCENE_QUERY_STAT(SkillLabDummyGround), false);
			GroundQuery.AddIgnoredActor(Caster);
			FHitResult GroundHit;
			const FVector TraceStart = GroundCandidate + FVector(0.0f, 0.0f, 5000.0f);
			const FVector TraceEnd = GroundCandidate - FVector(0.0f, 0.0f, 5000.0f);
			if (!GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, GroundQuery))
			{
				continue;
			}
			if (FVector::DotProduct(GroundHit.ImpactNormal, FVector::UpVector) < 0.85f)
			{
				continue;
			}

			const FVector CandidateLocation = GroundHit.Location + FVector(0.0f, 0.0f, CapsuleHalfHeight + 2.0f);
			FCollisionQueryParams ClearanceQuery(SCENE_QUERY_STAT(SkillLabDummyClearance), false);
			ClearanceQuery.AddIgnoredActor(Caster);
			const FCollisionShape DummyShape = FCollisionShape::MakeCapsule(48.0f, CapsuleHalfHeight);
			if (GetWorld()->OverlapBlockingTestByChannel(CandidateLocation, FQuat::Identity, ECC_WorldStatic, DummyShape, ClearanceQuery))
			{
				continue;
			}

			DummyLocation = CandidateLocation;
			GroundLocation = GroundHit.Location;
			bFoundOpenLocation = true;
		}

		if (!bFoundOpenLocation)
		{
			UE_LOG(LogTemp, Warning, TEXT("[SkillLabTestArena] Skipping training target %d: no flat open candidate in search patch"), Index);
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		Target = GetWorld()->SpawnActor<ASkillLabCharacter>(
			ASkillLabCharacter::StaticClass(), DummyLocation, FRotator::ZeroRotator, Params);
		if (Target)
		{
			Target->ConfigureAsTrainingDummy();
			if (USkillLabSkillRuntimeComponent* Skills = Target->FindComponentByClass<USkillLabSkillRuntimeComponent>())
			{
				Skills->TeamId = 2;
			}
			Target->Tags.AddUnique(TEXT("SkillLabDummy"));
			UE_LOG(LogTemp, Log, TEXT("[SkillLabTestArena] Spawned training target %s at %s on open ground %s"), *GetNameSafe(Target), *Target->GetActorLocation().ToCompactString(), *GroundLocation.ToCompactString());
		}
	}
}

void USkillLabTestArenaSubsystem::ConfigureSkillTestArena()
{
	if (!GetWorld())
	{
		return;
	}

	// The central navigation blocks are useful for movement lessons but obstruct
	// the skill lane. Remove only the explicitly named test blocks; perimeter
	// walls and the floor remain unchanged.
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		AStaticMeshActor* MeshActor = *It;
		const FString ActorName = MeshActor ? MeshActor->GetActorNameOrLabel() : FString();
		if (ActorName.Contains(TEXT("NavTest_Block_A")) || ActorName.Contains(TEXT("NavTest_Block_B")))
		{
			MeshActor->SetActorEnableCollision(false);
			MeshActor->SetActorHiddenInGame(true);
			UE_LOG(LogTemp, Log, TEXT("[SkillLabTestArena] Disabled central skill-test obstacle %s"), *ActorName);
		}
	}
}
