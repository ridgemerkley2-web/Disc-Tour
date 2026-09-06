#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession4VisualCaptureRunner.generated.h"

class ACameraActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class AHUD;
class UAnimMontage;
class UDiscGolfCharacterCreatorWidget;
class UDiscGolfRHBHThrowAdapterComponent;
class UPointLightComponent;
class USkeletalMeshComponent;

/**
 * Command-line-only Session 4 manual visual-evidence harness.
 *
 * All comparison golfers are transient instances of the real project pawn.
 * Their release adapters are rebound to a validation-only callback that never
 * creates a gameplay disc, stroke, lie, replay, or package save. The possessed
 * pawn is used only for the live creator screenshot and is restored through
 * the creator's normal Cancel path.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession4VisualCaptureRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession4VisualCaptureRunner();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Start();

private:
    enum class EStage : uint8
    {
        WaitingForRuntime,
        WaitingForNeutralFront,
        WaitingForNeutralSide,
        WaitingForReachback,
        WaitingForPlant,
        WaitingForRelease,
        WaitingForFollowThrough,
        WaitingForRecovery,
        WaitingForCreator,
        WaitingForExtremes,
        Capturing,
        Finished
    };

    struct FBodyMetrics
    {
        float HeightLandmarkCm = 0.0f;
        float BoundsHeightCm = 0.0f;
        float WingspanChainCm = 0.0f;
        float ShoulderWidthCm = 0.0f;
        float HandLengthCm = 0.0f;
        float FootLengthCm = 0.0f;
        float GripOffsetCm = 0.0f;
        bool bFiniteAndPlausible = false;
    };

    struct FProfileEvidence
    {
        FString Name;
        FString AssetPath;
        FString FixtureKind;
        FDGBodyProfile Body;
        FDGThrowStyle ThrowStyle;
        EDGHandedness Handedness = EDGHandedness::Right;
        FBodyMetrics NeutralMetrics;
        float NeutralLeftFootZ = 0.0f;
        float NeutralRightFootZ = 0.0f;
        int32 ReleaseCommitCount = 0;
        bool bSameSkeletalMesh = false;
        bool bSameSkeleton = false;
        bool bSameAnimBlueprint = false;
        bool bFiniteNeutralPose = false;
    };

    struct FCaptureEvidence
    {
        FString Filename;
        FString Evidence;
        FString Phase;
        FString CameraView;
        FString Sha1;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        int32 FramedProfileCount = 0;
        float MinSubjectScreenHeightFraction = 0.0f;
        float MaxSubjectScreenHeightFraction = 0.0f;
        float MinMontageSeconds = -1.0f;
        float MaxMontageSeconds = -1.0f;
        float MinRequiredCurveValue = -1.0f;
        bool bAllSubjectsFramed = false;
        bool bCameraOutsideSubjects = false;
        bool bLineOfSightClear = false;
        bool bFiniteTransforms = false;
        bool bReadable = false;
        bool bContactGateRequired = false;
        bool bContactGatePassed = true;
        bool bReleaseGateRequired = false;
        bool bReleaseGatePassed = true;
        bool bExactPhaseGateRequired = false;
        bool bExactPhaseGatePassed = true;
        int32 WorldDiscDelta = 0;
        int32 StrokeDelta = 0;
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

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> PossessedGolfer;
    UPROPERTY() TObjectPtr<ACameraActor> CaptureCamera;
    UPROPERTY() TObjectPtr<UPointLightComponent> CaptureKeyLight;
    UPROPERTY() TObjectPtr<UPointLightComponent> CaptureFillLight;
    UPROPERTY() TObjectPtr<UAnimMontage> ThrowMontage;
    UPROPERTY() TObjectPtr<UDiscGolfCharacterCreatorWidget> LiveCreatorWidget;
    UPROPERTY() TArray<TObjectPtr<UObject>> RequiredAssets;
    UPROPERTY() TArray<TObjectPtr<ADiscGolferPawn>> FixturePawns;
    UPROPERTY() TArray<TObjectPtr<UDiscGolfRHBHThrowAdapterComponent>> FixtureAdapters;
    UPROPERTY() TArray<TObjectPtr<USkeletalMeshComponent>> FixtureMeshes;

    EStage Stage = EStage::WaitingForRuntime;
    EStage StageAfterCapture = EStage::Finished;
    double StageStartRealSeconds = 0.0;
    bool bStarted = false;
    bool bFinished = false;
    bool bScreenshotPending = false;
    bool bScreenshotIssued = false;
    bool bPendingUsesCaptureCamera = true;
    bool bInitialGolferHidden = false;
    bool bInitialHudVisible = true;
    bool bCreatorOpened = false;
    bool bCreatorLiveAdjustmentApplied = false;
    bool bCreatorCancelRestored = false;
    bool bProductionMotionVisualCapture = false;
    bool bHandednessBoundaryPassed = false;
    bool bHandednessRejectedBeforeMontage = false;
    bool bHandednessProfileAssetUnchanged = false;
    bool bPersistentWriteCheckPassed = false;
    int32 PendingCaptureIndex = INDEX_NONE;
    int32 BaselineWorldDiscCount = 0;
    int32 BaselineStrokes = 0;
    int32 FixtureLaunchCount = 0;
    float MinimumPhaseCurveValue = -1.0f;
    FString PendingCapturePath;
    FString OutputDirectory;
    FString ManifestPath;
    FString FailureReason;
    FString ExpectedProductionMotionRevision = TEXT("v006");
    FString ActiveProductionMontagePath;
    FThrowCommand FixtureThrowCommand;
    FTransform InitialGolferTransform;
    FTransform FixtureStageTransform;
    TWeakObjectPtr<AActor> InitialViewTarget;
    FDGBodyProfile CreatorOpeningBody;
    FDGThrowStyle CreatorOpeningThrowStyle;
    EDGHandedness CreatorOpeningHandedness = EDGHandedness::Right;
    FDGBodyProfile CreatorEvidenceBody;
    FDGThrowStyle CreatorEvidenceThrowStyle;
    EDGHandedness CreatorEvidenceHandedness = EDGHandedness::Right;
    TArray<FTransform> FixtureLaunchGripTransforms;
    TArray<FThrowCommand> FixtureThrowCommands;
    TArray<FProfileEvidence> ProfileEvidence;
    TArray<int32> ActiveProfileEvidenceIndices;
    TArray<FCaptureEvidence> CaptureEvidence;
    TMap<FString, FFileStamp> InitialPackageFiles;
    TMap<FString, FFileStamp> InitialSaveGameFiles;
    TArray<FString> ChangedPackageFiles;
    TArray<FString> ChangedSaveGameFiles;

    void SetStage(EStage NewStage);
    double SecondsInStage() const;
    bool PreflightAssets();
    bool CreateCaptureCamera();
    bool SpawnRegularFixtures();
    bool SpawnProductionMotionFixtures();
    bool SpawnExtremeFixtures();
    bool SpawnFixture(
        const FString& Name,
        const FString& AssetPath,
        const FString& FixtureKind,
        const FDGBodyProfile& Body,
        const FDGThrowStyle& ThrowStyle,
        EDGHandedness Handedness,
        const FTransform& Transform);
    void DestroyFixtures();
    void ArrangeFixturesFront();
    void ArrangeFixturesSide();
    bool RunHandednessBoundaryAssertion();
    bool BeginFixtureThrows();
    bool HandleFixtureLaunch(
        const FThrowCommand& AuthoritativeCommand,
        const FTransform& GripWorldTransform);
    bool AreFixturesAtMontageTime(float TargetSeconds) const;
    bool AreFixturesAtPhase(EDGThrowPhase TargetPhase) const;
    void PauseFixtureMontages(bool bPause) const;
    float GetMontagePosition(const USkeletalMeshComponent* Mesh) const;
    float GetMinimumCurveValue(FName CurveName) const;
    bool ValidatePhaseGate(FName CurveName, float MinimumValue);
    bool ValidateExactPhaseGate(EDGThrowPhase TargetPhase) const;
    bool ValidatePlantContactGate();
    bool ValidateReleaseGate();
    bool ValidateNoGameplayMutation() const;

    void PrepareNeutralFrontCapture();
    void PrepareNeutralSideCapture();
    void PrepareAimCapture();
    void PrepareReachbackCapture();
    void PreparePlantCapture();
    void PrepareReleaseCapture();
    void PrepareFollowThroughCapture();
    void PrepareRecoveryCapture();
    bool PrepareCreatorCapture();
    bool FinishCreatorAndPrepareExtremes();
    void PrepareExtremeCapture();

    void PositionCompositeCamera(bool bSideView, bool bExtremeView = false);
    bool ValidateCompositeFraming(int32 CaptureIndex);
    bool ValidateCreatorFraming(int32 CaptureIndex);
    bool IsPoseFiniteAndPlausible(const USkeletalMeshComponent* Mesh) const;
    bool IsDynamicPoseFiniteAndPlausible(const USkeletalMeshComponent* Mesh) const;
    FBodyMetrics MeasureBody(const USkeletalMeshComponent* Mesh) const;
    bool RecordNeutralMetrics();
    bool ValidateProfileOrdering() const;
    FVector GetBoneLocation(const USkeletalMeshComponent* Mesh, FName Bone) const;
    float BoneDistance(const USkeletalMeshComponent* Mesh, FName A, FName B) const;

    void RequestCapture(
        int32 CaptureIndex,
        const FString& Evidence,
        const FString& Phase,
        const FString& CameraView,
        EStage NextStage,
        bool bUseCaptureCamera = true);
    bool PollPendingCapture();
    void HandleCaptureCompleted(int32 CaptureIndex);
    void ShowEvidenceLabel(const FString& Heading, const FString& Detail) const;
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
    void ApplyEvidenceMaterial(USkeletalMeshComponent* Mesh, const FLinearColor& Color) const;
    void RestoreRuntimeState();
    void WriteManifest(bool bPassed, const FString& Error);
    void WriteProductionMotionManifest(bool bPassed, const FString& Error);
    int32 GetExpectedCaptureCount() const;
    const TCHAR* GetCaptureFilename(int32 CaptureIndex) const;
    void Fail(const FString& Reason);
    void Pass();
};
