#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession3VisualCaptureRunner.generated.h"

class ACameraActor;
class ADiscActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class AHUD;
class UAnimMontage;
class UDiscGolfCharacterProfile;
class UDiscGolfRHBHThrowAdapterComponent;
class UDiscGolfMetaHumanAvatarBackendComponent;
class UDiscGolfThrowComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Dedicated, command-line-only Session 3 visual evidence harness.
 *
 * The first five captures use the possessed gameplay pawn, the authored RHBH
 * montage, the project release adapter, and the single authoritative gameplay
 * disc. The final three captures are transient body-profile compatibility
 * fixtures: they exercise the same pawn/framework/montage, but deliberately
 * replace the fixture launch callback so the evidence pass cannot add strokes
 * or gameplay discs after the accepted live throw.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession3VisualCaptureRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession3VisualCaptureRunner();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Start();

private:
    enum class EStage : uint8
    {
        WaitingForGameplay,
        BuildingLiveCommand,
        WaitingForHeldPose,
        CapturingHeld,
        WaitingForPlant,
        CapturingPlant,
        WaitingForRelease,
        WaitingForExactReleasePoseSettle,
        CapturingExactRelease,
        WaitingForPostRelease,
        CapturingPostRelease,
        WaitingForFollowThrough,
        CapturingFollowThrough,
        WaitingForRecoveryProof,
        CapturingRecoveryProof,
        WaitingForProductionFlightEvidence,
        WaitingForGameplayRecovery,
        CapturingGameplayRecovery,
        StartingProfile,
        WaitingForProfilePose,
        CapturingProfile,
        WaitingForProfileRecovery,
        Finished
    };

    struct FCaptureRecord
    {
        // Keep generated-module code and this non-reflected record layout in
        // lockstep; the FString-bearing evidence fields require one ABI.
        FString Filename;
        FString Evidence;
        FString Source;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        float CameraToSubjectCm = 0.0f;
        float SubjectBoundsRadiusCm = 0.0f;
        bool bCameraOutsideSubjectBounds = false;
        bool bSubjectFramed = false;
        bool bFocusPointFramed = false;
        bool bLineOfSightClear = false;
        int32 LineOfSightLandmarkCount = 0;
        int32 ClearLineOfSightLandmarkCount = 0;
        FString FirstOccludedLandmark;
        bool bBoneLengthsInvariant = false;
        float MaxBoneLengthRatioError = 0.0f;
        FString WorstBoneLengthSegment;
        float WorstBoneExpectedLengthCm = 0.0f;
        float WorstBoneActualLengthCm = 0.0f;
        bool bRetargetPoseTelemetryValid = false;
        bool bMetaHumanVisualRootUsesAbsoluteScale = false;
        FVector MetaHumanVisualRootWorldScale = FVector::ZeroVector;
        FVector MetaHumanBodyWorldScale = FVector::ZeroVector;
        FVector MetaHumanHeadWorldScale = FVector::ZeroVector;
        FVector MetaHumanOutfitWorldScale = FVector::ZeroVector;
        int32 ThrowPhase = INDEX_NONE;
        FVector SourceShoulderWorldLocation = FVector::ZeroVector;
        FVector SourceElbowWorldLocation = FVector::ZeroVector;
        FVector SourceHandWorldLocation = FVector::ZeroVector;
        FVector SourceShoulderRelativeToPelvisCm = FVector::ZeroVector;
        FVector SourceElbowRelativeToPelvisCm = FVector::ZeroVector;
        FVector SourceHandRelativeToPelvisCm = FVector::ZeroVector;
        FVector TargetShoulderWorldLocation = FVector::ZeroVector;
        FVector TargetElbowWorldLocation = FVector::ZeroVector;
        FVector TargetHandWorldLocation = FVector::ZeroVector;
        FVector TargetShoulderRelativeToPelvisCm = FVector::ZeroVector;
        FVector TargetElbowRelativeToPelvisCm = FVector::ZeroVector;
        FVector TargetHandRelativeToPelvisCm = FVector::ZeroVector;
        float SourceElbowAngleDegrees = -1.0f;
        float TargetElbowAngleDegrees = -1.0f;
        float SourceToTargetElbowAngleErrorDegrees = -1.0f;
        float SourceShoulderToHandDistanceCm = -1.0f;
        float TargetShoulderToHandDistanceCm = -1.0f;
        float SourceArmReachFraction = -1.0f;
        float TargetArmReachFraction = -1.0f;
        float SourceHandToPelvisDistanceCm = -1.0f;
        float TargetHandToPelvisDistanceCm = -1.0f;
        float TargetToSourceHandPelvisDistanceRatio = -1.0f;
        float SourceToTargetHandDirectionErrorDegrees = -1.0f;
        float TargetHandHorizontalFromPelvisCm = -1.0f;
        float TargetHandVerticalFromPelvisCm = 0.0f;
        bool bTargetPoseSpatialGateValid = false;
        FString HandCorrectionMode;
        FVector SourceDiscGripWorldLocation = FVector::ZeroVector;
        FVector DesiredTargetHandWorldLocation = FVector::ZeroVector;
        FVector TargetHandPreCorrectionWorldLocation = FVector::ZeroVector;
        FVector BoundedEffectorTargetWorldLocation = FVector::ZeroVector;
        FVector TargetHandPostCorrectionWorldLocation = FVector::ZeroVector;
        float SourceHandToDiscGripDistanceCm = -1.0f;
        float TargetHandPreToSourceHandDistanceCm = -1.0f;
        float TargetHandPostToSourceHandDistanceCm = -1.0f;
        float HandCorrectionImprovementCm = -1.0f;
        float TargetHandPreToDesiredHandDistanceCm = -1.0f;
        float TargetHandPostToDesiredHandDistanceCm = -1.0f;
        float DesiredHandCorrectionImprovementCm = -1.0f;
        float RequestedEffectorCorrectionCm = -1.0f;
        float BoundedEffectorCorrectionCm = -1.0f;
        float EffectorCorrectionCapCm = -1.0f;
        float TargetHandPostToRenderedHandDistanceCm = -1.0f;
        float SourceSnapshotToRenderedHandDistanceCm = -1.0f;
        float DesiredHandOrientationErrorDegrees = -1.0f;
        float TargetHandPostToDesiredOrientationErrorDegrees = -1.0f;
        float PhaseCorrectionWeight = 0.0f;
        float AppliedCorrectionBlendAlpha = 0.0f;
        float MaximumCorrectedSegmentRatioError = -1.0f;
        uint32 SourceBoneRevisionAtPreUpdate = 0;
        uint32 TargetBoneRevisionBeforeEvaluate = 0;
        uint32 SourceBoneRevisionAtCapture = 0;
        uint32 TargetBoneRevisionAtCapture = 0;
        uint32 SourceBoneTransformFrameAtCapture = 0;
        uint32 TargetBoneTransformFrameAtCapture = 0;
        uint64 SourceSampleFrameCounter = 0;
        uint64 TargetCorrectionFrameCounter = 0;
        uint64 PoseTelemetryCaptureFrameCounter = 0;
        uint64 ScreenshotRequestFrameCounter = 0;
        uint32 ScreenshotRequestSourceBoneRevision = 0;
        uint32 ScreenshotRequestTargetBoneRevision = 0;
        uint32 ScreenshotRequestSourceBoneTransformFrame = 0;
        uint32 ScreenshotRequestTargetBoneTransformFrame = 0;
        double ScreenshotRequestFixedDeltaTimeSeconds = 0.0;
        bool bScreenshotRequestFixedTimeStepActive = false;
        bool bScreenshotRequestPoseBound = false;
        FVector ResolvedCameraWorldLocation = FVector::ZeroVector;
        FRotator ResolvedCameraWorldRotation = FRotator::ZeroRotator;
        float ResolvedCameraFieldOfViewDegrees = -1.0f;
        float ResolvedCameraLocationErrorCm = -1.0f;
        float ResolvedCameraRotationErrorDegrees = -1.0f;
        float ResolvedCameraFieldOfViewErrorDegrees = -1.0f;
        bool bResolvedCameraInvariant = false;
        bool bHandCorrectionSnapshotValid = false;
        bool bFullGripTransformAvailable = false;
        FString GripTransformEvidenceMode;
        bool bSourceGripRelativeFallbackAvailable = false;
        bool bGripTransformEvidenceAccepted = false;
        bool bEffectorCorrectionClamped = false;
        bool bEffectorDistanceCapExceeded = false;
        bool bDesiredTargetWithinReachAnnulus = false;
        bool bHandCorrectionReachable = false;
        bool bHandCorrectionApplied = false;
        float SubjectScreenHeightFraction = 0.0f;
        float FocusProjectedDiameterPixels = 0.0f;
        bool bSubjectReadableScale = false;
        bool bEvidenceMeshReadable = false;
        FString EvidenceComponent;
        FVector EvidenceComponentWorldLocation = FVector::ZeroVector;
        float EvidenceActorToComponentCm = 0.0f;
        float EvidenceFocusAlignmentCm = 0.0f;
        bool bEvidenceFocusAnchoredToComponent = false;
        FString CheckpointName;
        int32 TargetMontageFrame = INDEX_NONE;
        float TargetMontageSeconds = -1.0f;
        float ActualMontageSeconds = -1.0f;
        float ActualMontageFrame = -1.0f;
        float MontageFrameError = -1.0f;
        bool bCheckpointTimingValid = false;
        int32 MontageInstanceId = INDEX_NONE;
        FString MontageInstanceToken;
        FString MontageInstanceAnimInstancePath;
        FString MontageInstanceMontagePath;
        float MontageActualBlendWeight = -1.0f;
        float MontageDesiredBlendWeight = -1.0f;
        bool bMontageInstanceMatchesCommittedThrow = false;
        bool bMontageFullWeightGateValid = false;
        FVector CameraWorldLocation = FVector::ZeroVector;
        FRotator CameraWorldRotation = FRotator::ZeroRotator;
        float CameraFieldOfViewDegrees = -1.0f;
        float LockedCameraLocationErrorCm = -1.0f;
        float LockedCameraRotationErrorDegrees = -1.0f;
        float LockedCameraFieldOfViewErrorDegrees = -1.0f;
        bool bLockedCameraInvariant = false;
        float FootPlantLeftAlpha = -1.0f;
        float FootPlantRightAlpha = -1.0f;
        float ReachBackAlpha = -1.0f;
        float BraceAlpha = -1.0f;
        float ReleaseApproachAlpha = -1.0f;
        float FollowThroughAlpha = -1.0f;
        float DiscPlaneStabilityAlpha = -1.0f;
        float WeightShiftAlpha = -1.0f;
        float BraceCompressionAlpha = -1.0f;
        float HipDriveAlpha = -1.0f;
        float TorsoDriveAlpha = -1.0f;
        float ShoulderDriveAlpha = -1.0f;
        float ElbowLeadAlpha = -1.0f;
        float WristLagAlpha = -1.0f;
        float FingerReleaseAlpha = -1.0f;
        float OffArmCounterbalanceAlpha = -1.0f;
        float GazeTargetAlpha = -1.0f;
        float BraceExtensionAlpha = -1.0f;
        float RecoveryBeatAlpha = -1.0f;
        bool bMotionCurvesFiniteAndNormalized = false;
        bool bCheckpointCurveGateValid = false;
        FVector FootLeftWorldLocation = FVector::ZeroVector;
        FVector FootRightWorldLocation = FVector::ZeroVector;
        FVector BallLeftWorldLocation = FVector::ZeroVector;
        FVector BallRightWorldLocation = FVector::ZeroVector;
        float BallLeftGroundGapCm = -1.0f;
        float BallRightGroundGapCm = -1.0f;
        bool bBallLeftGroundTraceValid = false;
        bool bBallRightGroundTraceValid = false;
        FString BallLeftGroundHitActor;
        FString BallLeftGroundHitComponent;
        FString BallRightGroundHitActor;
        FString BallRightGroundHitComponent;
        FVector BallLeftGroundImpactPoint = FVector::ZeroVector;
        FVector BallRightGroundImpactPoint = FVector::ZeroVector;
        FVector BallLeftGroundImpactNormal = FVector::ZeroVector;
        FVector BallRightGroundImpactNormal = FVector::ZeroVector;
        bool bBraceFootGroundGateValid = false;
        bool bRequiredFootGroundEvidenceValid = false;
        bool bDiscPlaneEvidenceAvailable = false;
        FVector DiscWorldLocation = FVector::ZeroVector;
        FVector DiscNormalWorld = FVector::ZeroVector;
        FVector DiscTangentWorld = FVector::ZeroVector;
        FVector AuthoredGripNormalWorld = FVector::ZeroVector;
        FVector AuthoredGripTangentWorld = FVector::ZeroVector;
        float DiscPlaneTiltFromWorldUpDegrees = -1.0f;
        float DiscNormalToAuthoredGripErrorDegrees = -1.0f;
        float DiscTangentToAuthoredGripErrorDegrees = -1.0f;
        float MaximumDiscToAuthoredGripPlaneErrorDegrees = -1.0f;
        bool bDiscPlaneMatchesAuthoredGrip = false;
        bool bDiscPlaneEvidenceValid = false;
        bool bMetaHumanProofTelemetryValid = false;
    };

    struct FProfileRecord
    {
        FString Name;
        FString AssetPath;
        float HeightCm = 0.0f;
        float WingspanScale = 0.0f;
        int32 ReleaseCount = 0;
        bool bAnimationStarted = false;
        bool bGripTransformUsable = false;
        bool bFollowThroughReached = false;
        bool bRecovered = false;
        bool bPoseFiniteAndPlausible = false;
        FString RecoveryReason;
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ThrowAdapter;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> GolferMesh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HeldDiscVisual;
    UPROPERTY() TObjectPtr<UAnimMontage> ThrowMontage;
    UPROPERTY() TObjectPtr<ADiscActor> LiveGameplayDisc;
    UPROPERTY() TObjectPtr<ACameraActor> CaptureCamera;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeldDiscEvidenceMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GameplayDiscEvidenceMaterial;
    UPROPERTY() TObjectPtr<UPointLightComponent> CaptureKeyLight;
    UPROPERTY() TObjectPtr<UPointLightComponent> CaptureFillLight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> PendingEvidenceMesh;
    UPROPERTY() TObjectPtr<UDiscGolfMetaHumanAvatarBackendComponent> MetaHumanBackend;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> MetaHumanVisualBody;
    UPROPERTY() TObjectPtr<UDiscGolfThrowComponent> FrameworkThrowComponent;
    UPROPERTY() TObjectPtr<AHUD> CaptureHud;

    UPROPERTY() TObjectPtr<ADiscGolferPawn> ProfileFixture;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ProfileAdapter;
    UPROPERTY() TObjectPtr<UDiscGolfThrowComponent> ProfileThrowComponent;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> ProfileMesh;

    FThrowCommand LiveCommand;
    FTransform InitialGolferTransform;
    EStage Stage = EStage::WaitingForGameplay;
    double StageStartRealSeconds = 0.0;
    bool bStarted = false;
    bool bFinished = false;
    bool bMetaHumanProductionVisualCapture = false;
    bool bScreenshotPending = false;
    bool bScreenshotIssued = false;
    int32 PendingCaptureIndex = INDEX_NONE;
    FString PendingCapturePath;
    double PendingCaptureStartRealSeconds = 0.0;
    float SavedGlobalTimeDilation = 1.0f;
    int32 BaselineWorldDiscCount = 0;
    int32 BaselineStrokes = 0;
    int32 BaselineLiveReleaseCount = 0;
    int32 LiveReleaseCallbackCount = 0;
    int32 ExactReleasePoseSettleTicksCompleted = 0;
    uint64 ExactReleasePoseCallbackFrameCounter = 0;
    uint64 ExactReleasePoseLastSettleFrameCounter = 0;
    uint64 ExactReleasePoseValidationFrameCounter = 0;
    uint32 ExactReleasePoseBoneRevisionAtCallback = 0;
    uint32 ExactReleasePoseBoneRevisionAtValidation = 0;
    uint32 ExactReleasePoseBoneTransformFrameAtValidation = 0;
    int64 PendingReleaseAttemptSerial = 0;
    bool bMetaHumanBodyTickPrerequisiteAdded = false;
    bool bInitialHudVisible = true;
    bool bHudVisibilityCaptured = false;
    bool bSavedUseFixedTimeStep = false;
    bool bFixedTimeStepCaptured = false;
    bool bCapturePresentationStateRestored = false;
    double SavedFixedDeltaTimeSeconds = 0.0;
    FVector LockedProofCameraOrigin = FVector::ZeroVector;
    FVector LockedProofCameraForward = FVector::ForwardVector;
    FVector LockedProofCameraRight = FVector::RightVector;
    FVector LockedProofCameraUp = FVector::UpVector;
    FVector LockedProofCameraTarget = FVector::ZeroVector;
    FVector LockedProofCameraLocation = FVector::ZeroVector;
    FRotator LockedProofCameraRotation = FRotator::ZeroRotator;
    bool bExactReleasePoseParallelEvaluationComplete = false;
    bool bExactReleasePosePostEvaluateProven = false;
    bool bLiveThrowRecovered = false;
    bool bLiveFollowThroughReached = false;
    bool bLiveAuthoritativeFlightSettled = false;
    bool bLiveAuthoritativeFlightHoledOut = false;
    bool bLiveAuthoritativeFlightValidationAttempted = false;
    bool bLiveAuthoritativeFlightValidationPassed = false;
    bool bLiveTrajectoryCaptureValid = false;
    bool bLiveTrajectoryExportDeferralRequested = false;
    bool bLiveTrajectoryExportDeferralDiscarded = false;
    FString LiveRecoveryReason;
    FString BaselineTrajectoryCaptureId;
    FString LiveTrajectoryCaptureId;
    FString LiveFlightValidationFailure;
    int32 LiveTrajectorySampleCount = 0;
    float LiveFlightDurationSeconds = 0.0f;
    float LiveTrajectoryAirCarryMeters = 0.0f;
    float LiveTrajectoryFinalCarryMeters = 0.0f;
    int32 LiveFlightGroundContactCount = 0;
    int32 LiveFlightBasketContactCount = 0;
    int32 ProfileIndex = 0;
    int32 ProfileLaunchCount = 0;
    int32 ProfileReleaseCallbackCount = 0;
    bool bProfileRecoveredCallback = false;
    FString ProfileRecoveryReason;
    bool bLastCameraLineOfSightClear = false;
    FColor PendingEvidenceMarkerColor = FColor::Transparent;
    FString OutputDirectory;
    FString ManifestPath;
    FString ExpectedProductionMotionRevision = TEXT("v006");
    FString ActiveProductionMontagePath;
    int32 LiveCommittedMontageInstanceId = INDEX_NONE;
    TArray<FCaptureRecord> CaptureRecords;
    TArray<FProfileRecord> ProfileRecords;

    void SetStage(EStage NewStage);
    double SecondsInStage() const;
    bool BuildTimingCommand(FThrowCommand& OutCommand, bool bBeginOnly);
    bool ConfigureMetaHumanProductionPresentation(FString& OutError);
    USkeletalMeshComponent* GetCaptureSubjectMesh() const;
    int32 GetExpectedCaptureCount() const;
    const TCHAR* GetCaptureFilename(int32 CaptureIndex) const;
    void BeginLiveThrow();
    void PrepareHeldCapture();
    void PreparePlantCapture();
    void PrepareExactReleaseCapture();
    void PreparePostReleaseCapture();
    void PrepareFollowThroughCapture();
    void PrepareProductionRecoveryCapture();
    void PrepareRecoveredCapture();
    void BeginProfileFixture();
    void PrepareProfileCapture();
    void FinishProfileFixture();
    void DestroyProfileFixture();

    void RequestCapture(int32 CaptureIndex, const FString& Evidence, const FString& Source);
    bool PollPendingCapture();
    void HandleCaptureCompleted(int32 CaptureIndex);
    void PositionFullBodyCamera(
        const AActor* Subject,
        const USkeletalMeshComponent* Mesh,
        float FieldOfViewDegrees = 38.0f);
    void PositionGripCloseupCamera(
        const AActor* Subject,
        const USkeletalMeshComponent* Mesh,
        const FVector& FocusPoint,
        float FieldOfViewDegrees = 32.0f);
    void PositionPostReleaseCamera();
    void ConfigureLockedMetaHumanProofCamera();
    void ApplyLockedMetaHumanProofCamera();
    FVector ResolveExternalCameraLocation(
        const AActor* Subject,
        const FVector& Target,
        float DistanceCm,
        float HeightBiasCm,
        float SideBiasCm);
    bool ValidateCameraFraming(
        int32 CaptureIndex,
        const AActor* Subject,
        const USkeletalMeshComponent* Mesh,
        const FVector& FocusPoint,
        bool bRequireFullBody,
        float FocusRadiusCm = 0.0f,
        const UStaticMeshComponent* EvidenceMesh = nullptr);
    void ShowEvidenceLabel(const FString& Heading, const FString& Detail) const;

    USkeletalMeshComponent* FindSkeletalMesh(AActor* ActorOwner) const;
    UStaticMeshComponent* FindNamedStaticMesh(AActor* ActorOwner, FName ComponentName) const;
    int32 CountWorldDiscs() const;
    float GetMontagePosition(USkeletalMeshComponent* Mesh) const;
    bool ValidateCompletedLiveFlightEvidence(
        ADiscActor* SettledDisc,
        FString& OutError);
    bool DiscardLiveTrajectoryExportDeferral();
    bool IsProfilePoseFiniteAndPlausible() const;
    bool ValidateBoneLengthInvariant(
        const USkeletalMeshComponent* Mesh,
        float& OutMaxRatioError,
        FString* OutWorstSegment = nullptr,
        float* OutExpectedLengthCm = nullptr,
        float* OutActualLengthCm = nullptr) const;
    bool RecordMetaHumanPoseTelemetry(int32 CaptureIndex);
    bool RecordMetaHumanProofTelemetry(
        int32 CaptureIndex,
        const UStaticMeshComponent* EvidenceMesh);
    float GetProvisionalDiscRadiusCm(const UStaticMeshComponent* DiscMesh) const;
    FVector GetDiscEvidenceLocation(const UStaticMeshComponent* DiscMesh) const;
    void SetLiveDiscSimulationPaused(bool bPaused) const;
    void ApplyNeutralProxyMaterials(USkeletalMeshComponent* Mesh) const;
    UMaterialInstanceDynamic* ApplyDiscEvidenceMaterial(
        UStaticMeshComponent* DiscMesh,
        const FLinearColor& Color) const;
    void DrawDiscEvidenceMarker(const FVector& Location, const FColor& Color) const;
    void ClearEvidenceMarkers() const;
    void RestoreCaptureTimeDilation();
    void RestoreCapturePresentationState();
    bool WriteManifest(bool bPassed, const FString& Error);
    void Fail(const FString& Reason);
    void Pass();

    UFUNCTION()
    void HandleLiveRelease(int64 AttemptSerial, bool bAuthoritativeLaunchAccepted);

    UFUNCTION()
    void HandleLiveRecovery(int64 AttemptSerial, bool bDiscWasReleased);

    UFUNCTION()
    void HandleLiveDiscSettled(ADiscActor* Disc, FVector FinalLocation);

    UFUNCTION()
    void HandleLiveDiscHoledOut(ADiscActor* Disc);

    UFUNCTION()
    void HandleProfileRelease(int64 AttemptSerial, bool bLaunchAccepted);

    UFUNCTION()
    void HandleProfileRecovery(int64 AttemptSerial, bool bDiscWasReleased);

    bool HandleProfileFixtureLaunch(
        const FThrowCommand& AuthoritativeCommand,
        const FTransform& GripWorldTransform);
};
