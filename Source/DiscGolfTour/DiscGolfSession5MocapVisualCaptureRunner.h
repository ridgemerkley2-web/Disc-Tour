#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession5MocapVisualCaptureRunner.generated.h"

class ACameraActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class ASkeletalMeshActor;
class UAnimMontage;
class UAnimSequence;
class UDiscGolfRHBHThrowAdapterComponent;
class UPointLightComponent;
class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * Rendered, command-line-only evidence for the synthetic Session 5 pipeline.
 * It writes PNG/JSON evidence under Saved, but snapshots and rejects any
 * package or SaveGame mutation.  Fixture release delegates never create a
 * gameplay disc; the separate Session 5 smoke owns authoritative flight proof.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfSession5MocapVisualCaptureRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession5MocapVisualCaptureRunner();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Start();

private:
    enum class EStage : uint8
    {
        WaitingForRuntime,
        WaitingForSource,
        WaitingForRetargeted,
        WaitingForCleaned,
        WaitingForReachback,
        WaitingForPlant,
        WaitingForRelease,
        WaitingForFollowThrough,
        WaitingForShortProfile,
        WaitingForBaselineProfile,
        WaitingForTallProfile,
        WaitingForRecovery,
        Capturing,
        Finished
    };

    struct FFileStamp
    {
        int64 Size = INDEX_NONE;
        int64 TimestampTicks = 0;

        bool operator==(const FFileStamp& Other) const
        {
            return Size == Other.Size && TimestampTicks == Other.TimestampTicks;
        }
    };

    struct FCaptureRecord
    {
        FString Filename;
        FString Evidence;
        FString StageLabel;
        FString AnimationAssetPath;
        FString SkeletalMeshPath;
        FString MontagePath;
        FString Sha1;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        int32 VisibleSubjects = 0;
        int32 FramedSubjects = 0;
        int32 ProfileAwareSubjects = 0;
        float MontagePositionSeconds = -1.0f;
        float ExpectedPhaseSeconds = -1.0f;
        float MinSubjectScreenHeightFraction = 0.0f;
        float MaxSubjectScreenHeightFraction = 0.0f;
        float MinSubjectScreenWidthFraction = 0.0f;
        float MaxSubjectScreenWidthFraction = 0.0f;
        float MaxBoneLengthRatioError = 0.0f;
        bool bReadable = false;
        bool bFinitePose = false;
        bool bAllSubjectsFramed = false;
        bool bReadableProjectedScale = false;
        bool bPlausibleBoneLengths = false;
        bool bCameraOutsideSubjects = false;
        bool bStageIdentityPassed = false;
        bool bHeldDiscGateRequired = false;
        bool bHeldDiscGatePassed = true;
        int32 WorldDiscDelta = 0;
        int32 StrokeDelta = 0;
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> PossessedGolfer;
    UPROPERTY() TObjectPtr<ACameraActor> CaptureCamera;
    UPROPERTY() TObjectPtr<UPointLightComponent> KeyLight;
    UPROPERTY() TObjectPtr<UPointLightComponent> FillLight;
    UPROPERTY() TObjectPtr<ASkeletalMeshActor> StageActor;
    UPROPERTY() TObjectPtr<UAnimSequence> SourceSequence;
    UPROPERTY() TObjectPtr<UAnimSequence> RetargetedSequence;
    UPROPERTY() TObjectPtr<UAnimSequence> CleanedSequence;
    UPROPERTY() TObjectPtr<UAnimSequence> ProductionSequence;
    UPROPERTY() TObjectPtr<UAnimMontage> PipelineMontage;
    UPROPERTY() TObjectPtr<USkeletalMesh> SourceMesh;
    UPROPERTY() TObjectPtr<USkeletalMesh> TargetMesh;
    UPROPERTY() TArray<TObjectPtr<ADiscGolferPawn>> ProfilePawns;
    UPROPERTY() TArray<TObjectPtr<UDiscGolfRHBHThrowAdapterComponent>> ProfileAdapters;
    UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> ProfileMeshes;
    UPROPERTY() TArray<FDGBodyProfile> ProfileBodies;
    UPROPERTY() TArray<TObjectPtr<UObject>> RequiredAssets;

    EStage Stage = EStage::WaitingForRuntime;
    double StageStartSeconds = 0.0;
    bool bStarted = false;
    bool bFinished = false;
    bool bScreenshotPending = false;
    bool bScreenshotIssued = false;
    bool bCapturePausedWorld = false;
    bool bInitialGolferHidden = false;
    bool bInitialHudVisible = true;
    int32 PendingCaptureIndex = INDEX_NONE;
    int32 BaselineWorldDiscCount = 0;
    int32 BaselineStrokes = 0;
    int32 FixtureReleaseCallbackCount = 0;
    int32 ReleaseNotifyCount = 0;
    int32 FinishNotifyCount = 0;
    float ReachbackSeconds = -1.0f;
    float PlantSeconds = -1.0f;
    float ReleaseSeconds = -1.0f;
    float FollowThroughSeconds = -1.0f;
    float RecoverySeconds = -1.0f;
    FString OutputDirectory;
    FString ManifestPath;
    FString PendingCapturePath;
    FTransform StageTransform;
    FThrowCommand FixtureCommand;
    TWeakObjectPtr<AActor> InitialViewTarget;
    TArray<FCaptureRecord> CaptureRecords;
    TMap<FString, FFileStamp> InitialPackageFiles;
    TMap<FString, FFileStamp> InitialSaveGameFiles;
    TArray<FString> ChangedPackageFiles;
    TArray<FString> ChangedSaveGameFiles;
    TArray<FString> ProfileRecoveryReasonLabels;

    void SetStage(EStage NewStage);
    double SecondsInStage() const;
    bool PreflightAssets();
    bool ResolveMontageContract();
    bool CreateCaptureStage();
    bool PrepareSequenceStage(
        UAnimSequence* Sequence,
        USkeletalMesh* Mesh,
        float NormalizedTime);
    void DestroySequenceStage();
    bool SpawnProfileFixtures();
    bool SpawnProfileFixture(int32 ProfileIndex, const FTransform& Transform);
    bool BeginProfileThrows();
    bool HandleFixtureRelease(
        const FThrowCommand& AuthoritativeCommand,
        const FTransform& GripWorldTransform);
    float MinimumMontagePosition() const;
    void PauseProfileMontages(bool bPause) const;
    bool ProfilePosesFinite() const;
    bool ValidateHeldDiscState(bool bExpectedVisible) const;
    void SetOnlyProfileVisible(int32 VisibleIndex) const;
    void ShowAllProfiles() const;
    bool ValidateNoGameplayMutation() const;
    bool ValidateProfileReleaseCounts() const;

    void RefreshMeshForCapture(USkeletalMeshComponent* Mesh) const;
    bool CollectSubjectKeyPoints(
        const USkeletalMeshComponent* Mesh,
        TArray<FVector>& OutPoints) const;
    bool ValidateBoneLengthInvariant(
        const USkeletalMeshComponent* Mesh,
        const FDGBodyProfile* ExpectedProfile,
        float& OutMaxRatioError) const;
    void PositionCameraForSubjects(
        const TArray<USkeletalMeshComponent*>& Meshes,
        float FieldOfViewDegrees);
    void PositionCameraForSequence();
    void PositionCameraForProfiles(bool bSingleProfile = false, int32 ProfileIndex = INDEX_NONE);
    bool ValidateVisibleFraming(int32 CaptureIndex, int32 ExpectedSubjects);
    void ShowEvidenceLabel(const FString& Heading, const FString& Detail) const;
    void RequestCapture(
        int32 CaptureIndex,
        const FString& Evidence,
        const FString& StageLabel,
        EStage NextStage,
        int32 ExpectedSubjects);
    bool PollPendingCapture();
    void HandleCaptureCompleted(int32 CaptureIndex);
    FString ComputeSha1(const TArray<uint8>& Bytes) const;

    void SnapshotPersistentFiles(
        const FString& RootDirectory,
        bool bPackagesOnly,
        TMap<FString, FFileStamp>& OutFiles) const;
    void DiffPersistentFiles(
        const TMap<FString, FFileStamp>& Before,
        const TMap<FString, FFileStamp>& After,
        TArray<FString>& OutChanged) const;
    bool VerifyNoPersistentWrites();
    int32 CountWorldDiscs() const;
    void ApplyEvidenceMaterial(USkeletalMeshComponent* Mesh) const;
    void RestoreRuntimeState();
    void WriteManifest(bool bPassed, const FString& Error);
    void Fail(const FString& Reason);
    void Pass();
};
