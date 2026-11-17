#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/WorldSubsystem.h"
#include "SkillLabSpatialExportSubsystem.generated.h"

class AActor;
class USkillLabSkillRuntimeComponent;
class FJsonObject;
class UAnimSequenceBase;
struct FSkillLabCastExecutionToken;

UCLASS()
class SKILLLAB_API USkillLabSpatialExportSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	friend class FSkillLabSpatialExportAnimationNotifyGenerationTest;

	enum class EState : uint8 { Disabled, WaitingWorld, Warmup, PreRequest, Capturing, Finished };
	EState State = EState::Disabled;
	FString Mode;
	FString RunId;
	FString OutputDirectory;
	FString RequestPath;
	TSharedPtr<FJsonObject> Request;
	TWeakObjectPtr<AActor> Caster;
	TWeakObjectPtr<AActor> Target;
	TWeakObjectPtr<AActor> RelatedActor;
	TWeakObjectPtr<USkillLabSkillRuntimeComponent> Runtime;
	FDelegateHandle SkillEventHandle;
	FDelegateHandle ProjectileHandle;
	FDelegateHandle FeedbackHandle;
	FDelegateHandle WorldTickHandle;
	FDelegateHandle AnimationNotifyHandle;
	TWeakObjectPtr<class USkillLabSkillAnimationComponent> AnimationComponent;
	UPROPERTY(Transient) TArray<TObjectPtr<class USceneCaptureComponent2D>> CaptureComponents;
	UPROPERTY(Transient) TArray<TObjectPtr<class UTextureRenderTarget2D>> CaptureTargets;
	TMap<TWeakObjectPtr<class USkeletalMeshComponent>, FDelegateHandle> PoseHandles;
	TMap<TWeakObjectPtr<class USkeletalMeshComponent>, uint64> PoseFinalizedFrames;
	TArray<TWeakObjectPtr<class ASkillLabFireballProjectile>> ObservedProjectiles;
	TMap<TWeakObjectPtr<class ASkillLabFireballProjectile>, FIntPoint> ObservedProjectileIds;
	TMap<TWeakObjectPtr<class ASkillLabFireballProjectile>, FGuid> ObservedProjectileSubmissionIds;
	TMap<TWeakObjectPtr<class ASkillLabFireballProjectile>, FString> ObservedProjectilePaths;
	TSet<FGuid> RemovedProjectileIds;
	TSet<FString> RemovedProjectilePaths;
	TArray<TWeakObjectPtr<class ASkillLabSkillFeedbackActor>> ObservedFeedbackActors;
	TMap<TWeakObjectPtr<class ASkillLabSkillFeedbackActor>, FGuid> ObservedFeedbackSubmissionIds;
	TMap<TWeakObjectPtr<class ASkillLabSkillFeedbackActor>, FString> ObservedFeedbackPaths;
	TSet<FGuid> RemovedFeedbackIds;
	TSet<FString> RemovedFeedbackPaths;
	void ReleaseCaptureResources();
	void FinalizeObservedObjectStates();
	void CaptureViews(const TSharedRef<FJsonObject>& Sample, const TArray<TSharedPtr<FJsonValue>>& Queries);
	void AppendModelSnapshots(TArray<TSharedPtr<FJsonValue>>& Values);
	double SessionStartWall = 0.0;
	FString SessionStartedAt;
	double WarmupStartWorldTime = 0.0;
	double PreRequestStartWall = 0.0;
	double WarmupSeconds = 1.0;
	double PreRequestSeconds = 0.25;
	double DurationSeconds = 0.0;
	double SampleInterval = 1.0 / 30.0;
	double RequestWallTime = 0.0;
	double RequestWorldTime = 0.0;
	double FirstSampleWorldTime = 0.0;
	double LastSampleWorldTime = 0.0;
	double FirstSampleWallTime = 0.0;
	double LastSampleWallTime = 0.0;
	double NextSampleWall = 0.0;
	double PreviousSampleWall = 0.0;
	double TimeoutSeconds = 120.0;
	int32 SampleIndex = 0;
	int32 RequestStartSampleIndex = 0;
	int32 MissedExpectedSamples = 0;
	int32 RequestId = 0;
	int32 CapturedCastId = 0;
	uint64 CapturedRuntimeGeneration = 0;
	bool bRequestAccepted = false;
	bool bSubmittingRequest = false;
	bool bRuntimeReadyObserved = false;
	bool bProjectileExpected = false;
	bool bConfigurationWriteFailed = false;
	TArray<FString> Errors;
	FString LastError;
	FString CurrentPhase = TEXT("initializing");
	FString FailurePhase;

	bool LoadRequest();
	void AdvanceSession();
	void Fail(const FString& Error);
	void Finish(bool bComplete);
	void ResolveActorsAndRuntime();
	AActor* ResolveActor(const TSharedPtr<FJsonObject>& Locator, const FString& Label, bool bRequired);
	void WriteConfigurationAndReferences();
	void AppendReferenceQueries(TArray<TSharedPtr<FJsonValue>>& OutValues);
	void SubmitRequest();
	void CaptureSample();
	void RecordSkillEvent(const struct FSkillLabSkillResult& Event);
	void RecordProjectile(const struct FSkillLabProjectileObservation& Observation);
	void RecordFeedbackObservation(const struct FSkillLabFeedbackObservation& Observation);
	void RecordAnimationNotify(const FSkillLabCastExecutionToken& Token, class UAnimSequenceBase* Animation, FGameplayTag EventTag, bool bStateEnd, bool bForwarded);
	void SetPhase(const FString& Phase);
	void AppendJsonLine(const FString& RelativePath, const TSharedRef<FJsonObject>& Object);
	void WriteReceipt(bool bComplete);
};
