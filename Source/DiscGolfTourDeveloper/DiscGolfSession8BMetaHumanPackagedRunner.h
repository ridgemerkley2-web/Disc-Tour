#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfMetaHumanRetargetAnimInstance.h"
#include "DiscGolfPerformanceBudget.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession8BMetaHumanPackagedRunner.generated.h"

class ADiscActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class UDiscGolfAvatarBackendProfile;
class UDiscGolfMetaHumanAvatarBackendComponent;
class UDiscGolfRHBHThrowAdapterComponent;
class UDiscGolfThrowComponent;
class UDiscGolfTourGameInstance;
class UDiscFlightComponent;
class USkeletalMeshComponent;
enum class EDGMetaHumanPresentationPolicy : uint8;

/**
 * Packaged-only Session 8B acceptance driver. It exercises only public runtime
 * seams and observes the existing DG throw/flight authorities; it never owns a
 * gameplay disc, launch command, flight step, inventory item, or save slot.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession8BMetaHumanPackagedRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession8BMetaHumanPackagedRunner();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void Start();

private:
    enum class EPhase : uint8
    {
        Invalid,
        ApplyMetaHuman,
        ReloadCancelFailureSwitchDG,
        ReloadDGRestoreMetaHumanVisualThrow,
        MetaHumanPerformance
    };

    enum class EStep : uint8
    {
        Idle,
        Phase1CaptureDG,
        Phase1SelectMetaHuman,
        Phase1CaptureMetaHumanFullBody,
        Phase1CaptureMetaHumanCloseup,
        Phase1Apply,
        Phase2PreviewDG,
        Phase2CaptureDGPreview,
        Phase2Cancel,
        Phase2CaptureMetaHumanAfterCancel,
        Phase2FailureProbes,
        Phase2ApplyDG,
        Phase3CaptureFreshDG,
        Phase3ApplyMetaHuman,
        Phase3BeginTiming,
        Phase3CommitTiming,
        Phase3CaptureGrip,
        Phase3AwaitRelease,
        Phase3AwaitFollowThrough,
        Phase3AwaitRecovery,
        Phase4WarmupCreator,
        Phase4SampleCreator,
        Phase4BeginGameplay,
        Phase4CommitTiming,
        Phase4SampleGameplay,
        Phase4AwaitRecovery,
        Finished
    };

    struct FPresentationPolicyCounters
    {
        int32 TransitionSuccess = 0;
        int32 TransitionFailure = 0;
        int32 RollbackSuccess = 0;
        int32 RollbackFailure = 0;
    };

    struct FLODSyncComponentEntryEvidence
    {
        FString Name;
        FString SyncOption;
        bool bComponentPresent = false;
    };

    struct FLODSyncMappingEvidence
    {
        FString Name;
        TArray<int32> Mapping;
    };

    struct FLODSyncPolicyEvidence
    {
        FString ComponentPath;
        FString ContractStatus;
        int32 NumLODs = 0;
        int32 MinLOD = INDEX_NONE;
        int32 ForcedLOD = INDEX_NONE;
        bool bRegistered = false;
        bool bTickEnabled = false;
        bool bTickEvenWhenPaused = false;
        bool bContractValid = false;
        TArray<FLODSyncComponentEntryEvidence> ComponentsToSync;
        TArray<FLODSyncMappingEvidence> CustomLODMapping;
        TArray<FString> MissingDeclaredComponents;
    };

    struct FSkeletalPolicyEvidence
    {
        FString Name;
        FString ComponentPath;
        FString SkeletalMeshPath;
        FString VisibilityTickOption;
        int32 LODCount = 0;
        int32 ExpectedMappedLOD = INDEX_NONE;
        int32 ForcedLODLegacyOneBased = 0;
        int32 ForceRenderedLOD = INDEX_NONE;
        int32 ForceStreamedLOD = INDEX_NONE;
        int32 ActualRenderedLOD = INDEX_NONE;
        int32 PredictedLOD = INDEX_NONE;
        int32 DesiredSyncLOD = INDEX_NONE;
        int32 BestAvailableLOD = INDEX_NONE;
        bool bRegistered = false;
        bool bVisiblePresentation = false;
        bool bTickEnabled = false;
        bool bTickEvenWhenPaused = false;
        bool bUpdateRateOptimizations = false;
        bool bSourceTickPrerequisite = false;
    };

    struct FGroomPolicyEvidence
    {
        FString Name;
        FString ComponentPath;
        FString GroomAssetPath;
        int32 ExpectedMappedLOD = INDEX_NONE;
        int32 ForcedLOD = INDEX_NONE;
        int32 ForceRenderedLOD = INDEX_NONE;
        int32 ForceStreamedLOD = INDEX_NONE;
        int32 DesiredSyncLOD = INDEX_NONE;
        int32 BestAvailableLOD = INDEX_NONE;
        bool bMappedByLODSync = false;
        bool bRegistered = false;
        bool bTickEnabled = false;
        bool bTickEvenWhenPaused = false;
    };

    struct FOutfitRequiredBonesHelperEvidence
    {
        FString ComponentPath;
        FString LeaderPoseComponentPath;
        int32 ConfiguredActualOutfitLOD = INDEX_NONE;
        int32 ConfiguredPredictedOutfitLOD = INDEX_NONE;
        int32 ConfiguredOutfitBoneCount = 0;
        int32 ConfiguredReadyOutfitLODCount = 0;
        int32 RequiredLeaderBoneCount = 0;
        int32 MappedOutfitUsedLeaderBoneCount = 0;
        bool bRegistered = false;
        bool bConfiguredForBodyAndOutfit = false;
        bool bAssetless = false;
        bool bVisible = false;
        bool bHiddenInGame = true;
        bool bShouldRender = false;
        bool bRenderInMainPass = false;
        bool bVisibleInSceneCaptureOnly = false;
        bool bCanEverTick = false;
        bool bTickEnabled = false;
        bool bCollisionDisabled = false;
        bool bGenerateOverlapEvents = false;
        bool bRegisteredAsBodyFollower = false;
    };

    struct FPresentationPolicyEvidence
    {
        FString QualityProfileId;
        FString RequestedPolicy;
        FString VerifiedPolicy;
        FString PolicyStatus;
        FString ActiveVisualActor;
        FString AnimationSourceComponent;
        int32 VerifiedForcedLOD = INDEX_NONE;
        bool bPolicyVerified = false;
        bool bCollected = false;
        FPresentationPolicyCounters Counters;
        FLODSyncPolicyEvidence LODSync;
        TArray<FSkeletalPolicyEvidence> SkeletalComponents;
        TArray<FGroomPolicyEvidence> GroomComponents;
        FOutfitRequiredBonesHelperEvidence OutfitRequiredBonesHelper;
        /** Stable policy state excludes renderer-selected actual/predicted LODs. */
        FString StableStateFingerprint;
        /** Exact state additionally includes all four public policy counters. */
        FString ExactStateFingerprint;
    };

    struct FPresentationPolicyProbeRecord
    {
        FString Name;
        FString Classification;
        FString Invocation;
        bool bCallResult = false;
        bool bExpectedCallResult = false;
        bool bActorIdentityPreserved = false;
        bool bHelperIdentityPreserved = false;
        bool bStablePolicyStatePreserved = false;
        FPresentationPolicyCounters CounterDelta;
        FPresentationPolicyEvidence Before;
        FPresentationPolicyEvidence After;
    };

    struct FCaptureRecord
    {
        struct FMeshRenderEvidence
        {
            FString ComponentPath;
            FString SkeletalMeshPath;
            FVector BoundsOrigin = FVector::ZeroVector;
            FVector BoundsExtent = FVector::ZeroVector;
            FVector2D ProjectedMin = FVector2D::ZeroVector;
            FVector2D ProjectedMax = FVector2D::ZeroVector;
            float BoundsRadiusCm = 0.0f;
            float DistanceToSourceCm = 0.0f;
            float LastRenderTime = 0.0f;
            float LastRenderTimeOnScreen = 0.0f;
            int32 LODCount = 0;
            int32 MaterialCount = 0;
            int32 ReferenceBoneCount = 0;
            int32 ComponentSpaceTransformCount = 0;
            bool bRegistered = false;
            bool bVisible = false;
            bool bHiddenInGame = true;
            bool bShouldRender = false;
            bool bRenderStateCreated = false;
            bool bRecentlyRendered = false;
            bool bOwnerNoSee = false;
            bool bOnlyOwnerSee = false;
            bool bRenderInMainPass = false;
            bool bVisibleInSceneCaptureOnly = false;
            bool bBoundsFinite = false;
            bool bPoseFinite = false;
            bool bProjectedViewportOverlap = false;
            bool bProjectedViewportContained = false;
            FString LeaderPoseComponentPath;
            FString AnimationInstanceClassPath;
            FString PostProcessClassPath;
            FString PoseSourceComponentPath;
            FString PoseSyncMode;
            bool bSupportedPoseSource = false;
            int32 SharedAnchorCount = 0;
            float MaximumSharedAnchorDistanceCm = -1.0f;
            bool bSharedAnchorsConverged = false;
        };

        struct FPrimitiveRenderEvidence
        {
            FString ComponentPath;
            FString ResourcePath;
            FVector BoundsOrigin = FVector::ZeroVector;
            FVector BoundsExtent = FVector::ZeroVector;
            FVector2D ProjectedMin = FVector2D::ZeroVector;
            FVector2D ProjectedMax = FVector2D::ZeroVector;
            float BoundsRadiusCm = 0.0f;
            float DistanceToSourceCm = 0.0f;
            float LastRenderTime = 0.0f;
            float LastRenderTimeOnScreen = 0.0f;
            int32 MaterialCount = 0;
            int32 AssetGroupCount = 0;
            int32 ComponentGroupCount = 0;
            int32 LODCount = 0;
            int32 DesiredSyncLOD = INDEX_NONE;
            int32 BestAvailableLOD = INDEX_NONE;
            int32 ForcedLOD = INDEX_NONE;
            bool bRegistered = false;
            bool bVisible = false;
            bool bHiddenInGame = true;
            bool bShouldRender = false;
            bool bRenderStateCreated = false;
            bool bRecentlyRendered = false;
            bool bOwnerNoSee = false;
            bool bOnlyOwnerSee = false;
            bool bRenderInMainPass = false;
            bool bVisibleInSceneCaptureOnly = false;
            bool bBoundsFinite = false;
            bool bProjectedViewportOverlap = false;
            bool bAssetValid = false;
            bool bAssetGroupsValid = false;
            bool bCompiling = true;
            bool bTickEvenWhenPaused = false;
            bool bProjectedViewportContained = false;
            bool bLODSyncRegistered = false;
            bool bLODSyncTickEnabled = false;
            bool bLODSyncTickEvenWhenPaused = false;
        };

        struct FOutfitSkinSectionEvidence
        {
            FString MaterialSlot;
            FString MaterialPath;
            FString SemanticReason;
            FString WorstVertexInfluences;
            FVector BoundsMinimumBodyCm = FVector::ZeroVector;
            FVector BoundsMaximumBodyCm = FVector::ZeroVector;
            FVector WorstVertexBodyCm = FVector::ZeroVector;
            float HeightCm = 0.0f;
            float KneeClearanceCm = 0.0f;
            float FootClearanceCm = 0.0f;
            int32 SectionIndex = INDEX_NONE;
            int32 AuthoredMaterialIndex = INDEX_NONE;
            int32 ResolvedMaterialIndex = INDEX_NONE;
            int32 BaseVertexIndex = INDEX_NONE;
            int32 VertexCount = 0;
            int32 TriangleCount = 0;
            int32 CheckedVertexCount = 0;
            int32 BelowKneeVertexCount = 0;
            int32 BelowFootVertexCount = 0;
            int32 WorstVertexIndex = INDEX_NONE;
            bool bEnabled = false;
            bool bHasClothingData = false;
            bool bMaterialUsesWorldPositionOffset = false;
            bool bSemanticAccepted = false;
        };

        struct FOutfitLiveSkinEvidence
        {
            FString PoseSyncMode;
            FString StructuralReason;
            FString SemanticReason;
            FString FixedFunctionSupportReason;
            FString ActiveMorphTargetSummary;
            FString ActiveMeshDeformerName;
            FString ActiveMeshDeformerPath;
            FString BodyPostProcessClassPath;
            FString RendererFallbackBoneList;
            FString RendererFallbackSummary;
            FString RendererFallbackReason;
            float KneePlaneBodyZCm = 0.0f;
            float FootPlaneBodyZCm = 0.0f;
            float KneeToFootSpanCm = 0.0f;
            int32 ActualRenderedLOD = INDEX_NONE;
            int32 PredictedLOD = INDEX_NONE;
            int32 DesiredSyncLOD = INDEX_NONE;
            int32 BestAvailableLOD = INDEX_NONE;
            int32 ForceRenderedLOD = INDEX_NONE;
            int32 ForceStreamedLOD = INDEX_NONE;
            int32 ForcedLODLegacyOneBased = 0;
            int32 ComputedMinLOD = INDEX_NONE;
            int32 AssetMinLOD = INDEX_NONE;
            int32 CurrentFirstLOD = INDEX_NONE;
            int32 PendingFirstLOD = INDEX_NONE;
            int32 VertexCount = 0;
            int32 SectionCount = 0;
            int32 OutfitReferenceBoneCount = 0;
            int32 BodyReferenceBoneCount = 0;
            int32 BodyComponentSpaceTransformCount = 0;
            int32 LeaderBoneMapCount = 0;
            int32 UsedOutfitBoneCount = 0;
            int32 MissingLeaderMappingCount = 0;
            int32 LeaderNameMismatchCount = 0;
            int32 CheckedVertexCount = 0;
            int32 CheckedInfluenceCount = 0;
            int32 NonFiniteVertexCount = 0;
            int32 InvalidInfluenceCount = 0;
            int32 NonNormalizedWeightVertexCount = 0;
            int32 InvalidSectionBoneMapCount = 0;
            int32 InactiveUsedOutfitBoneCount = 0;
            int32 HiddenLeaderBoneCount = 0;
            int32 InvalidLeaderPoseBoneCount = 0;
            int32 RendererFallbackBoneCount = 0;
            int32 MorphTargetMapEntryCount = 0;
            int32 ActiveMorphTargetCount = 0;
            int32 NonZeroMorphTargetWeightCount = 0;
            int32 NonZeroMorphCurveCount = 0;
            int32 InvalidActiveMorphTargetCount = 0;
            int32 NonFiniteMorphWeightCount = 0;
            int32 ActiveExternalMorphTargetCount = 0;
            int32 NonFiniteExternalMorphWeightCount = 0;
            int32 ClothingSimulationCount = 0;
            uint32 BodyBoneRevisionBefore = 0;
            uint32 BodyBoneRevisionAfter = 0;
            bool bMeshObjectPresent = false;
            bool bMeshObjectDynamicDataValid = false;
            bool bPositionCPUAccessRequested = false;
            bool bPositionDataPresent = false;
            bool bSkinWeightCPUAccessRequested = false;
            bool bSkinWeightDataPresent = false;
            bool bSkinWeightLookupPresent = false;
            bool bSkinWeightVertexCountMatches = false;
            bool bSkinWeightProfilePending = false;
            bool bUsingSkinWeightProfile = false;
            bool bEveryVertexCoveredExactlyOnce = false;
            bool bRefPoseOverridePresent = false;
            bool bLeaderSafePoseValidationEnabled = false;
            bool bLeaderValidMeshPoseArrayPresent = false;
            bool bBodyPostProcessDisabled = false;
            bool bBodyPostProcessShouldEvaluate = false;
            bool bHasActiveMorphTargets = false;
            bool bHasActiveExternalMorphTargets = false;
            bool bHasMeshDeformer = false;
            bool bHasClothingSimulation = false;
            bool bHasSectionClothingData = false;
            bool bHasWorldPositionOffsetMaterial = false;
            bool bFixedFunctionPathComplete = false;
            bool bStructurallyValid = false;
            bool bSemanticAccepted = false;
            TArray<FOutfitSkinSectionEvidence> Sections;
        };

        FString Filename;
        FString Scene;
        FString BackendId;
        FString ActiveVisualActor;
        bool bMetaHumanVisualRootUsesAbsoluteScale = false;
        FVector MetaHumanVisualRootWorldScale = FVector::ZeroVector;
        FVector MetaHumanBodyWorldScale = FVector::ZeroVector;
        FVector MetaHumanHeadWorldScale = FVector::ZeroVector;
        FVector MetaHumanOutfitWorldScale = FVector::ZeroVector;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        int32 StrokeCount = 0;
        int32 WorldDiscCount = 0;
        int32 ReleaseCommitCount = 0;
        int32 ExpectedStrokeCount = 0;
        int32 ExpectedWorldDiscCount = 0;
        int32 ExpectedReleaseCommitCount = 0;
        FString ThrowPhase;
        FVector GripWorldLocation = FVector::ZeroVector;
        FVector GameplayDiscWorldLocation = FVector::ZeroVector;
        float DiscToGripDistanceCm = -1.0f;
        FVector MetaHumanHandWorldLocation = FVector::ZeroVector;
        FVector VisibleDiscWorldLocation = FVector::ZeroVector;
        float VisibleDiscToMetaHumanHandDistanceCm = -1.0f;
        FString PresentationHandCorrectionMode = TEXT("INACTIVE");
        FVector SourceHandWorldLocation = FVector::ZeroVector;
        FVector SourceDiscGripWorldLocation = FVector::ZeroVector;
        FVector MetaHumanHandPreCorrectionWorldLocation = FVector::ZeroVector;
        FVector MetaHumanHandPostCorrectionWorldLocation = FVector::ZeroVector;
        float SourceHandToDiscGripDistanceCm = -1.0f;
        float MetaHumanHandPreToSourceHandDistanceCm = -1.0f;
        float MetaHumanHandPostToSourceHandDistanceCm = -1.0f;
        uint32 SourceBoneRevisionAtPreUpdate = 0;
        uint32 TargetBoneRevisionBeforeEvaluate = 0;
        uint32 TargetBoneRevisionAtCapture = 0;
        uint64 SourceSampleFrameCounter = 0;
        uint64 TargetCorrectionFrameCounter = 0;
        uint64 ReleaseCallbackFrameCounter = 0;
        uint64 CaptureFreezeFrameCounter = 0;
        uint64 ReleaseCallbackEventOrder = 0;
        uint64 CaptureFreezeEventOrder = 0;
        FVector2D CreatorSemanticMin = FVector2D::ZeroVector;
        FVector2D CreatorSemanticMax = FVector2D::ZeroVector;
        int32 CreatorSemanticPointCount = 0;
        bool bCreatorOpen = false;
        bool bDGProxyVisible = true;
        bool bMetaHumanReady = false;
        bool bHeldDiscVisible = false;
        bool bGameplayDiscActive = false;
        bool bReleaseEvidenceCollected = false;
        bool bImmutableReleaseGripValid = false;
        bool bGameplayDiscLocationFinite = false;
        bool bDiscToGripDistanceFinite = false;
        bool bReleasedDiscIdentityVerified = false;
        bool bFlightComponentIdentityVerified = false;
        bool bFlightIsFlying = false;
        bool bFlightTickSnapshotPresent = false;
        bool bFlightTickWasEnabledBeforePause = false;
        bool bFlightTickSuspended = false;
        bool bWorldPausedForCapture = false;
        bool bMetaHumanRenderEvidenceCollected = false;
        bool bMetaHumanHairRenderEvidenceCollected = false;
        bool bMetaHumanOutfitRenderEvidenceCollected = false;
        bool bMetaHumanOutfitLiveSkinEvidenceCollected = false;
        bool bVisibleHandAlignmentEvidenceCollected = false;
        bool bVisibleHandLocationFinite = false;
        bool bVisibleDiscLocationFinite = false;
        bool bVisibleHandDistanceFinite = false;
        bool bPresentationHandCorrectionSnapshotValid = false;
        bool bPresentationHandCorrectionReachable = false;
        bool bPresentationHandCorrectionApplied = false;
        bool bCreatorFramingEvidenceCollected = false;
        bool bCreatorFramingAccepted = false;
        FMeshRenderEvidence BodyRender;
        FMeshRenderEvidence HeadRender;
        FMeshRenderEvidence OutfitRender;
        FPrimitiveRenderEvidence HairRender;
        FOutfitLiveSkinEvidence OutfitLiveSkin;
        FPresentationPolicyEvidence PresentationPolicy;
    };

    struct FPerformanceRecord
    {
        FString Segment;
        /** Conservative <=600 contiguous-bucket maxima spanning the full segment. */
        FDiscGolfPerformanceSummary Summary;
        /** Every rendered frame interval observed across the full 15-second segment. */
        FDiscGolfPerformanceSummary RawSummary;
        EDiscGolfPerformanceBudgetState State =
            EDiscGolfPerformanceBudgetState::WarmingUp;
        double DurationSeconds = 0.0;
        FPresentationPolicyEvidence PresentationPolicyBegin;
        FPresentationPolicyEvidence PresentationPolicyEnd;
    };

    struct FPausedSkeletalMeshState
    {
        TWeakObjectPtr<USkeletalMeshComponent> Mesh;
        bool bPreviousPauseAnims = false;
    };

    UPROPERTY(Transient) TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY(Transient) TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY(Transient) TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfTourGameInstance> GameInstance;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfMetaHumanAvatarBackendComponent> AvatarBackend;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ThrowAdapter;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfThrowComponent> FrameworkThrow;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfAvatarBackendProfile> CanonicalMetaHumanProfile;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfAvatarBackendProfile> TransientFailureProfile;
    UPROPERTY(Transient) TObjectPtr<ADiscActor> ReleasedGameplayDisc;
    UPROPERTY(Transient) TObjectPtr<UDiscFlightComponent> ThrowCaptureFlightComponent;
    UPROPERTY(Transient) TObjectPtr<AActor> StableThrowVisualActor;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> StableThrowVisualBody;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> StableThrowVisualHead;

    EPhase Phase = EPhase::Invalid;
    EStep Step = EStep::Idle;
    EStep NextStepAfterCapture = EStep::Idle;
    FString PhaseName;
    FString RunId;
    FString UserDirectory;
    FString SavePath;
    FString OutputDirectory;
    FString ReportPath;
    FString PendingCapturePath;
    FString PendingCaptureFilename;
    FString PendingCaptureScene;
    FString BackendFailureAdapterStatus;
    TArray<FCaptureRecord> Captures;
    TArray<FPerformanceRecord> PerformanceRecords;
    TArray<FPresentationPolicyProbeRecord>
        PresentationPolicyCandidateIsolationProbes;
    TArray<FPresentationPolicyProbeRecord> PresentationPolicyTransitionProbes;
    TArray<uint8> InitialSaveBytes;
    TArray<uint8> CheckpointSaveBytes;
    FDGFullCharacterCustomization InitialCustomization;
    FDGFullCharacterCustomization ProxySentinelCustomization;
    FThrowCommand LiveThrowCommand;
    FDiscGolfPerformanceTracker SegmentTracker;
    TArray<float> PerformanceRawFrameTimesMs;
    TArray<FPausedSkeletalMeshState> PausedSkeletalMeshStates;
    FVector ImmutableReleaseGripWorldLocation = FVector::ZeroVector;
    FDiscGolfMetaHumanHandCorrectionEvidence FrozenHandCorrectionEvidence;
    FPresentationPolicyEvidence PendingPerformancePolicyBegin;

    double StepStartedSeconds = 0.0;
    double PendingCaptureStartedSeconds = 0.0;
    double PerformanceWarmupStartedSeconds = 0.0;
    double PerformanceSegmentStartedSeconds = 0.0;
    double LastPerformanceFrameSeconds = 0.0;
    uint64 PerformanceMaxUsedPhysicalBytes = 0;
    uint64 ReleaseCallbackFrameCounter = 0;
    uint64 CaptureFreezeFrameCounter = 0;
    uint64 PresentationEventSerial = 0;
    uint64 ReleaseCallbackEventOrder = 0;
    uint64 CaptureFreezeEventOrder = 0;
    uint32 FrozenHandCorrectionTargetBoneRevision = 0;
    int32 BaselineStrokes = 0;
    int32 BaselineWorldDiscs = 0;
    int32 BaselineReleaseCommits = 0;
    int32 ReleaseCallbackCount = 0;
    int32 RecoveryCallbackCount = 0;
    int64 LiveAttemptSerial = 0;
    bool bStarted = false;
    bool bFinished = false;
    bool bCapturePending = false;
    bool bCaptureIssued = false;
    bool bPausedForThrowCapture = false;
    bool bCosmeticFailureProbePassed = false;
    bool bBackendFailureProbePassed = false;
    bool bDuplicateReleaseNoOpPassed = false;
    bool bPerformanceSawAuthoritativeFlight = false;
    bool bTrajectoryExportDeferredAtSettlement = false;
    bool bTrajectorySummaryReadyBeforeFlush = false;
    bool bTrajectoryExportFlushedAfterSegment = false;
    bool bTrajectoryExportFlushPassed = false;
    FString DeferredTrajectoryCaptureId;
    FString FlushedTrajectoryCaptureId;
    FString DeferredPerformanceFailure;
    bool bProxyValuesPreserved = false;
    bool bSaveWasMutatedByPhase = false;
    bool bHasImmutableReleaseGripWorldLocation = false;
    bool bHasThrowCaptureFlightTickSnapshot = false;
    bool bThrowCaptureFlightTickWasEnabled = false;
    bool bReleasePresentationCapturePending = false;
    bool bHasFrozenHandCorrectionEvidence = false;
    bool bGameplayCandidateIsolationProbePassed = false;
    bool bPresentationPolicyTransitionProbesPassed = false;
    bool bHasPendingPerformancePolicyBegin = false;

    bool ValidateInvocation(FString& OutError);
    bool AcquireRuntime(FString& OutError);
    bool BeginPhase(FString& OutError);
    void RunStep();
    void SetStep(EStep NewStep);

    bool VerifyDGPresentation(FString& OutError) const;
    bool VerifyMetaHumanPresentation(FString& OutError) const;
    bool FreezeStableThrowPresentation(FString& OutError);
    bool VerifyStableThrowPresentation(FString& OutError) const;
    bool VerifyCurrentCustomization(
        FName BackendId,
        const FDGFullCharacterCustomization* Expected,
        FString& OutError) const;
    bool VerifyDiskProfile(
        FName BackendId,
        const FDGFullCharacterCustomization* Expected,
        FString& OutError) const;
    bool VerifyOnlyIsolatedProfileSave(FString& OutError) const;
    bool LoadSaveBytes(TArray<uint8>& OutBytes) const;

    bool OpenCreator(FString& OutError);
    bool SelectBackend(FName BackendId, FString& OutError);
    bool ApplyCreatorDraft(FString& OutError);
    bool RunGameplayCandidateIsolationProbe(FString& OutError);
    bool RunFailureAtomicityProbes(FString& OutError);
    bool BeginRealRHBHTiming(FString& OutError);
    bool CommitRealRHBHThrow(FString& OutError);
    bool PauseForThrowCapture(FString& OutError);
    void ResumeAfterThrowCapture();

    bool RequestCapture(
        const TCHAR* Filename,
        const TCHAR* Scene,
        EStep NextStep,
        FString& OutError);
    void PollCapture();
    bool PopulateCaptureScene(FCaptureRecord& OutCapture, FString& OutError) const;
    bool PopulateMetaHumanRenderEvidence(
        FCaptureRecord& OutCapture,
        FString& OutError) const;
    bool PopulatePresentationPolicyEvidence(
        EDGMetaHumanPresentationPolicy ExpectedPolicy,
        FPresentationPolicyEvidence& OutEvidence,
        FString& OutError) const;
    bool AppendPresentationPolicyProbe(
        const TCHAR* Name,
        const TCHAR* Classification,
        const TCHAR* Invocation,
        bool bCallResult,
        bool bExpectedCallResult,
        const FPresentationPolicyCounters& ExpectedDelta,
        bool bExpectStableStatePreserved,
        const FPresentationPolicyEvidence& Before,
        const FPresentationPolicyEvidence& After,
        TArray<FPresentationPolicyProbeRecord>& OutRecords,
        FString& OutError) const;
    bool RunRedundantPresentationPolicyProbe(
        const TCHAR* Name,
        EDGMetaHumanPresentationPolicy Policy,
        FString& OutError);
    bool PopulateOutfitLiveSkinEvidence(
        USkeletalMeshComponent* Body,
        USkeletalMeshComponent* Outfit,
        FCaptureRecord::FOutfitLiveSkinEvidence& OutEvidence,
        FString& OutError) const;

    bool BeginPerformanceSegment(const TCHAR* Segment, FString& OutError);
    void TickPerformanceSegment();
    bool FinishPerformanceSegment(const TCHAR* Segment, FString& OutError);

    UFUNCTION()
    void HandleReleaseCommitted(int64 AttemptSerial, bool bAccepted);

    UFUNCTION()
    void HandleThrowRecovered(int64 AttemptSerial, bool bDiscWasReleased);

    int32 CountWorldDiscs() const;
    int32 CountActiveVisualActors(UClass* ActorClass) const;
    FString SuccessStatus() const;
    bool WritePhaseReport(
        const FString& Status,
        const FString& FailureCode,
        const FString& Reason) const;
    void Fail(const FString& Code, const FString& Reason);
    void Pass();
};
