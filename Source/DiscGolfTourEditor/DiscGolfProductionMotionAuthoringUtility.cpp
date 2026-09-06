#include "DiscGolfProductionMotionAuthoringUtility.h"

#include "Animation/AnimData/CurveIdentifier.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DiscGolfAnimationLibrary.h"
#include "DiscGolfProductionMotion.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimMontageFactory.h"
#include "Factories/AnimSequenceFactory.h"
#include "Factories/DataAssetFactory.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

namespace DiscGolfProductionMotionAuthoring
{
constexpr int32 FrameRate = 60;
constexpr int32 ExpectedFamilyCount = 3;
constexpr int32 ExpectedAssetCount = 7;
constexpr float TimingTolerance = 0.001f;
// v006 keeps its authored 0.25 s blend shape, but starts that blend only
// 0.10 s before the montage end. The named Recovery checkpoint therefore
// remains a full-weight production pose instead of landing in auto blend-out.
constexpr float ProductionMontageBlendOutTriggerTimeSeconds = 0.10f;
// Animation data-model keys are stored as FQuat4f. Near-identical quaternion
// angular distances therefore have a single-precision floor of roughly 0.04
// degrees after the authored FQuat is round-tripped through SetBoneTrackKeys.
// Keep this aligned with the stricter semantic intent gates below: it permits
// representation noise, not a visible or biomechanical pose change.
constexpr float AuthoredTrackRoundTripRotationToleranceDegrees = 0.05f;
// The v007 arm targets land exactly on the semantic pocket/release elbow
// boundaries. Reconstructing the authored FQuat4f tracks can move those
// derived angles by a few ten-thousandths of a degree. This tolerance is only
// for those two equality-shaped comparisons; it does not alter the reported
// thresholds or any v006/legacy gate.
constexpr float V7DriveArmBoundaryToleranceDegrees = 0.001f;

constexpr bool IsAtMostWithTolerance(float Value, float Maximum,
    float Tolerance)
{
    return Value <= Maximum + Tolerance;
}

constexpr bool IsAtLeastWithTolerance(float Value, float Minimum,
    float Tolerance)
{
    return Value >= Minimum - Tolerance;
}

static_assert(IsAtMostWithTolerance(105.0005f, 105.0f,
    V7DriveArmBoundaryToleranceDegrees));
static_assert(!IsAtMostWithTolerance(105.01f, 105.0f,
    V7DriveArmBoundaryToleranceDegrees));
static_assert(IsAtLeastWithTolerance(144.9995f, 145.0f,
    V7DriveArmBoundaryToleranceDegrees));
static_assert(!IsAtLeastWithTolerance(144.99f, 145.0f,
    V7DriveArmBoundaryToleranceDegrees));
const TCHAR* SkeletonPath =
    TEXT("/Game/DiscGolf/Characters/Meshes/SKEL_DG_Master.SKEL_DG_Master");
const TCHAR* MeshPath =
    TEXT("/Game/DiscGolf/Characters/Meshes/SK_DG_Master.SK_DG_Master");
const TCHAR* PhaseNotifyClassPath =
    TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_ThrowPhase");
const TCHAR* ReleaseNotifyClassPath =
    TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_DiscRelease");
const TCHAR* FinishNotifyClassPath =
    TEXT("/Script/DiscGolfRuntimeFoundation.AnimNotify_ThrowFinished");
const TCHAR* RequiredUserDirRoot =
    TEXT("C:/DGTour_TestRuns/Session19ProductionMotion");
const FName DefaultSlot(TEXT("DefaultSlot"));

struct FRecipeVersionSpec
{
    FString Version;
    int32 SchemaVersion = 0;
    FString AssetRevision;
    FString RecipeRelativePath;
    FString RecipeSchema;
    FString RecipeId;
    FString LibraryPath;
    FString LibraryMotionId;
    TArray<FString> SequencePaths;
    TArray<FString> MontagePaths;
    TArray<int32> MinimumPoseKeyCounts;
    TArray<float> MaximumRootTranslationCm;
    TArray<float> MinimumForwardRootExcursionCm;
    TArray<float> PresentationRootScales;
    TArray<int32> MaximumPoseKeyGaps;
    TArray<int32> MinimumRecoveryPoseKeyCounts;
    TArray<int32> MinimumAuthoredRotationChannelsPerPose;
};

bool ResolveRecipeVersion(const FString& RequestedVersion,
    FRecipeVersionSpec& OutSpec, FString& Error)
{
    const FString Version = RequestedVersion.ToLower();
    if (Version == TEXT("v1"))
    {
        OutSpec.Version = TEXT("v1");
        OutSpec.SchemaVersion = 1;
        OutSpec.AssetRevision = TEXT("v001");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v1.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v1");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V1");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V1::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V1");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V1::DriveSequence,
            DiscGolfProductionMotion::V1::ApproachSequence,
            DiscGolfProductionMotion::V1::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V1::DriveMontage,
            DiscGolfProductionMotion::V1::ApproachMontage,
            DiscGolfProductionMotion::V1::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {7, 7, 7};
        OutSpec.MaximumRootTranslationCm = {5.0f, 5.0f, 5.0f};
        OutSpec.MinimumForwardRootExcursionCm = {0.0f, 0.0f, 0.0f};
        OutSpec.PresentationRootScales = {1.0f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {144, 108, 72};
        OutSpec.MinimumRecoveryPoseKeyCounts = {0, 0, 0};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {0, 0, 0};
        return true;
    }
    if (Version == TEXT("v2"))
    {
        OutSpec.Version = TEXT("v2");
        OutSpec.SchemaVersion = 2;
        OutSpec.AssetRevision = TEXT("v002");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v2.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v2");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V2");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V2::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V2");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V2::DriveSequence,
            DiscGolfProductionMotion::V2::ApproachSequence,
            DiscGolfProductionMotion::V2::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V2::DriveMontage,
            DiscGolfProductionMotion::V2::ApproachMontage,
            DiscGolfProductionMotion::V2::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {18, 16, 17};
        OutSpec.MaximumRootTranslationCm = {110.0f, 35.0f, 12.0f};
        OutSpec.MinimumForwardRootExcursionCm = {90.0f, 24.0f, 7.0f};
        OutSpec.PresentationRootScales = {2.25f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {144, 108, 72};
        OutSpec.MinimumRecoveryPoseKeyCounts = {0, 0, 0};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {0, 0, 0};
        return true;
    }
    if (Version == TEXT("v3"))
    {
        OutSpec.Version = TEXT("v3");
        OutSpec.SchemaVersion = 3;
        OutSpec.AssetRevision = TEXT("v003");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v3.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v3");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V3");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V3::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V3");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V3::DriveSequence,
            DiscGolfProductionMotion::V3::ApproachSequence,
            DiscGolfProductionMotion::V3::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V3::DriveMontage,
            DiscGolfProductionMotion::V3::ApproachMontage,
            DiscGolfProductionMotion::V3::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {35, 36, 30};
        OutSpec.MaximumRootTranslationCm = {110.0f, 35.0f, 12.0f};
        OutSpec.MinimumForwardRootExcursionCm = {90.0f, 24.0f, 7.0f};
        OutSpec.PresentationRootScales = {2.25f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {6, 5, 4};
        OutSpec.MinimumRecoveryPoseKeyCounts = {11, 10, 10};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {23, 23, 23};
        return true;
    }
    if (Version == TEXT("v4"))
    {
        OutSpec.Version = TEXT("v4");
        OutSpec.SchemaVersion = 4;
        OutSpec.AssetRevision = TEXT("v004");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v4.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v4");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V4");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V4::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V4");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V4::DriveSequence,
            DiscGolfProductionMotion::V4::ApproachSequence,
            DiscGolfProductionMotion::V4::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V4::DriveMontage,
            DiscGolfProductionMotion::V4::ApproachMontage,
            DiscGolfProductionMotion::V4::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {35, 36, 30};
        OutSpec.MaximumRootTranslationCm = {110.0f, 35.0f, 12.0f};
        OutSpec.MinimumForwardRootExcursionCm = {90.0f, 24.0f, 7.0f};
        OutSpec.PresentationRootScales = {2.25f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {6, 5, 4};
        OutSpec.MinimumRecoveryPoseKeyCounts = {11, 10, 10};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {23, 23, 23};
        return true;
    }
    if (Version == TEXT("v5"))
    {
        OutSpec.Version = TEXT("v5");
        OutSpec.SchemaVersion = 5;
        OutSpec.AssetRevision = TEXT("v005");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v5.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v5");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V5");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V5::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V5");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V5::DriveSequence,
            DiscGolfProductionMotion::V5::ApproachSequence,
            DiscGolfProductionMotion::V5::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V5::DriveMontage,
            DiscGolfProductionMotion::V5::ApproachMontage,
            DiscGolfProductionMotion::V5::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {35, 36, 30};
        OutSpec.MaximumRootTranslationCm = {110.0f, 35.0f, 12.0f};
        OutSpec.MinimumForwardRootExcursionCm = {90.0f, 24.0f, 7.0f};
        OutSpec.PresentationRootScales = {2.25f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {6, 5, 4};
        OutSpec.MinimumRecoveryPoseKeyCounts = {11, 10, 10};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {23, 23, 23};
        return true;
    }
    if (Version == TEXT("v6"))
    {
        OutSpec.Version = TEXT("v6");
        OutSpec.SchemaVersion = 6;
        OutSpec.AssetRevision = TEXT("v006");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v6.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v6");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V6");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V6::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V6");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V6::DriveSequence,
            DiscGolfProductionMotion::V6::ApproachSequence,
            DiscGolfProductionMotion::V6::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V6::DriveMontage,
            DiscGolfProductionMotion::V6::ApproachMontage,
            DiscGolfProductionMotion::V6::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {35, 36, 30};
        OutSpec.MaximumRootTranslationCm = {110.0f, 35.0f, 12.0f};
        OutSpec.MinimumForwardRootExcursionCm = {90.0f, 24.0f, 7.0f};
        OutSpec.PresentationRootScales = {2.25f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {6, 5, 4};
        OutSpec.MinimumRecoveryPoseKeyCounts = {11, 10, 10};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {23, 23, 23};
        return true;
    }
    if (Version == TEXT("v7"))
    {
        OutSpec.Version = TEXT("v7");
        OutSpec.SchemaVersion = 7;
        OutSpec.AssetRevision = TEXT("v007");
        OutSpec.RecipeRelativePath =
            TEXT("SourceArt/DiscGolf/Motion/ProductionMotionRecipe_v7.json");
        OutSpec.RecipeSchema =
            TEXT("DiscGolfTour.ProjectAuthoredProceduralMotionRecipe.v7");
        OutSpec.RecipeId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_V7");
        OutSpec.LibraryPath = DiscGolfProductionMotion::V7::Library;
        OutSpec.LibraryMotionId = TEXT("DG_PRODUCTION_PROCEDURAL_MOTION_LIBRARY_V7");
        OutSpec.SequencePaths = {
            DiscGolfProductionMotion::V7::DriveSequence,
            DiscGolfProductionMotion::V7::ApproachSequence,
            DiscGolfProductionMotion::V7::PuttSequence
        };
        OutSpec.MontagePaths = {
            DiscGolfProductionMotion::V7::DriveMontage,
            DiscGolfProductionMotion::V7::ApproachMontage,
            DiscGolfProductionMotion::V7::PuttMontage
        };
        OutSpec.MinimumPoseKeyCounts = {35, 36, 30};
        OutSpec.MaximumRootTranslationCm = {110.0f, 35.0f, 12.0f};
        OutSpec.MinimumForwardRootExcursionCm = {90.0f, 24.0f, 7.0f};
        OutSpec.PresentationRootScales = {2.25f, 1.0f, 1.0f};
        OutSpec.MaximumPoseKeyGaps = {6, 5, 4};
        OutSpec.MinimumRecoveryPoseKeyCounts = {11, 10, 10};
        OutSpec.MinimumAuthoredRotationChannelsPerPose = {23, 23, 23};
        return true;
    }
    Error = FString::Printf(
        TEXT("Unsupported production-motion recipe version: %s"),
        *RequestedVersion);
    return false;
}

struct FPoseKey
{
    int32 Frame = 0;
    float ThrowingHandGripAlpha = 0.0f;
    int32 AuthoredRotationChannelCount = 0;
    TMap<FName, FRotator> Rotations;
    TMap<FName, FVector> Translations;
};

struct FPhaseSpec
{
    FString Name;
    int32 Frame = 0;
};

struct FCurveSpec
{
    FName Name;
    TArray<TPair<int32, float>> Keys;
};

struct FBiomechanicalEvents
{
    int32 DiscReachbackPlaneFrame = 0;
    int32 WeightShiftFrame = 0;
    int32 BraceCompressionFrame = 0;
    int32 HipFireFrame = 0;
    int32 TorsoFireFrame = 0;
    int32 OffArmCounterbalanceFrame = 0;
    int32 ShoulderFireFrame = 0;
    int32 ElbowLeadFrame = 0;
    int32 WristLagFrame = 0;
    int32 ReleaseFrame = 0;
    int32 BraceExtensionFrame = 0;
    int32 GazeReacquireFrame = 0;
    int32 RecoveryDecelerationFrame = 0;
    int32 RecoveryRecenterFrame = 0;
    int32 RecoverySettleFrame = 0;
};

struct FFamilyRecipe
{
    FString MotionId;
    FString Family;
    FString SequencePath;
    FString MontagePath;
    FName StyleId;
    int32 FrameCount = 0;
    float DurationSeconds = 0.0f;
    int32 ReleaseFrame = 0;
    int32 FinishFrame = 0;
    float PowerMin = 0.0f;
    float PowerMax = 0.0f;
    int32 MinimumPoseKeyCount = 0;
    float MaximumRootTranslationCm = 0.0f;
    float MinimumForwardRootExcursionCm = 0.0f;
    float RootReturnToleranceCm = 0.0f;
    float PresentationRootScale = 1.0f;
    int32 MaximumPoseKeyGapFrames = 0;
    int32 MinimumRecoveryPoseKeyCount = 0;
    int32 MinimumAuthoredRotationChannelsPerPose = 0;
    FBiomechanicalEvents Biomechanics;
    TArray<FString> Coverage;
    TArray<FPhaseSpec> Phases;
    TArray<FPoseKey> Poses;
    TArray<FCurveSpec> Curves;
};

struct FRecipe
{
    FRecipeVersionSpec Version;
    FString SourceTextMd5;
    FString InterpolationMode;
    FString BiomechanicalModel;
    FString DiscContactModel;
    FString RecoveryModel;
    FString RotationSpace;
    FString RootTrackPolicy;
    bool bPresentationRootTrajectoryEnabled = false;
    TArray<FName> AnimatedBones;
    TArray<FName> ComponentRotationBones;
    TArray<FName> LocalRotationBones;
    TArray<FFamilyRecipe> Families;
};

struct FV4ComponentSpaceValidationMetrics
{
    int32 SampledFrameCount = 0;
    float MinimumJointSegmentRatio = 1.0f;
    float MaximumJointSegmentRatio = 1.0f;
    float MaximumComponentIntentErrorDegrees = 0.0f;
    float MaximumLocalIntentErrorDegrees = 0.0f;
    float MaximumComponentDeltaDegrees = 0.0f;
    float MaximumLocalFrameDeltaDegrees = 0.0f;
    float MaximumFingerFrameDeltaDegrees = 0.0f;
    float MaximumRootRotationErrorDegrees = 0.0f;
    int32 NamedPhaseKneeSampleCount = 0;
    float MinimumNamedPhaseKneeAngleDegrees = 180.0f;
    float MaximumNamedPhaseKneeAngleDegrees = 0.0f;
    int32 ArmSpatialSampleCount = 0;
    float ReachBackThrowingElbowAngleDegrees = 0.0f;
    float ReleaseThrowingElbowAngleDegrees = 0.0f;
    float FollowThroughThrowingElbowAngleDegrees = 0.0f;
    float ReachBackThrowingArmReachRatio = 0.0f;
    float ReleaseThrowingArmReachRatio = 0.0f;
    float FollowThroughThrowingArmReachRatio = 0.0f;
    float ReachBackThrowingHandTorsoClearanceCm = 0.0f;
    float ReleaseThrowingHandTorsoClearanceCm = 0.0f;
    float FollowThroughThrowingHandTorsoClearanceCm = 0.0f;
    FVector ReachBackThrowingArmDirectionTorsoLocal = FVector::ZeroVector;
    FVector ReleaseThrowingArmDirectionTorsoLocal = FVector::ZeroVector;
    FVector FollowThroughThrowingArmDirectionTorsoLocal = FVector::ZeroVector;
    FVector ReachBackThrowingHandTorsoLocalCm = FVector::ZeroVector;
    FVector ReleaseThrowingHandTorsoLocalCm = FVector::ZeroVector;
    FVector FollowThroughThrowingHandTorsoLocalCm = FVector::ZeroVector;
    float ReachBackToReleaseArmDirectionDeltaDegrees = 0.0f;
    float ReleaseToFollowThroughArmDirectionDeltaDegrees = 0.0f;
    float ReachBackToFollowThroughArmDirectionDeltaDegrees = 0.0f;
    float ReachBackToFollowThroughHandTravelCm = 0.0f;
    float ReachBackThrowingHandRadialReachCm = 0.0f;
    float MinimumThrowingHandVerticalCm = 0.0f;
    float MinimumReleaseFollowThrowingHandLateralClearanceCm = 0.0f;
    float MaximumThrowingHandDirectionFrameDeltaDegrees = 0.0f;
    bool bArmSpatialGatePassed = false;
    bool bV7DriveArmGateApplied = false;
    int32 V7SupportArmSampleCount = 0;
    float V7SupportArmMaximumReachRatio = 0.0f;
    float V7SupportArmMinimumElbowAngleDegrees = 0.0f;
    float V7SupportArmMaximumElbowAngleDegrees = 0.0f;
    int32 V7PowerPocketSampleCount = 0;
    float V7PowerPocketMinimumReachRatio = 0.0f;
    float V7PowerPocketMaximumReachRatio = 0.0f;
    float V7PowerPocketMinimumElbowAngleDegrees = 0.0f;
    float V7PowerPocketMaximumElbowAngleDegrees = 0.0f;
    float V7PowerPocketMaximumAbsoluteTorsoLateralCm = 0.0f;
    float V7PowerPocketMinimumTorsoForwardCm = 0.0f;
    float V7PowerPocketMaximumTorsoForwardCm = 0.0f;
    float V7PowerPocketMinimumTorsoVerticalCm = 0.0f;
    float V7PowerPocketMaximumTorsoVerticalCm = 0.0f;
    float V7ReleaseReachRatio = 0.0f;
    float V7ReleaseElbowAngleDegrees = 0.0f;
    float V7FollowThroughReachRatio = 0.0f;
    float V7FollowThroughElbowAngleDegrees = 0.0f;
    float V7RecoveryReachRatio = 0.0f;
    float V7RecoveryElbowAngleDegrees = 0.0f;
    float V7RecoveryAbsoluteTorsoLateralCm = 0.0f;
    float V7RecoveryTorsoVerticalCm = 0.0f;
};

struct FArmSpatialPoseMetrics
{
    float ElbowAngleDegrees = 0.0f;
    float ReachRatio = 0.0f;
    float HandTorsoClearanceCm = 0.0f;
    FVector DirectionTorsoLocal = FVector::ZeroVector;
    FVector HandTorsoLocalCm = FVector::ZeroVector;
    FVector HandComponentLocation = FVector::ZeroVector;
};

struct FArmSpatialGateThresholds
{
    float MinimumElbowAngleDegrees = 110.0f;
    float MinimumArmExtensionRatio = 0.82f;
    float MinimumReachBackRadialReachCm = 64.0f;
    float MinimumHandVerticalCm = 0.0f;
    float MinimumReleaseFollowLateralClearanceCm = 60.0f;
    float MaximumDirectionFrameDeltaDegrees = 8.0f;
};

const FArmSpatialGateThresholds& V6ArmSpatialThresholds()
{
    static const FArmSpatialGateThresholds Values;
    return Values;
}

struct FV7DriveArmGateThresholds
{
    float SupportMaximumReachRatio = 0.75f;
    float SupportMinimumElbowAngleDegrees = 65.0f;
    float SupportMaximumElbowAngleDegrees = 105.0f;
    float PowerPocketMinimumReachRatio = 0.70f;
    float PowerPocketMaximumReachRatio = 0.82f;
    float PowerPocketMinimumElbowAngleDegrees = 85.0f;
    float PowerPocketMaximumElbowAngleDegrees = 105.0f;
    float PowerPocketMaximumAbsoluteTorsoLateralCm = 46.0f;
    float PowerPocketMinimumTorsoForwardCm = -45.0f;
    float PowerPocketMaximumTorsoForwardCm = -20.0f;
    float PowerPocketMinimumTorsoVerticalCm = 5.0f;
    float PowerPocketMaximumTorsoVerticalCm = 25.0f;
    float ReleaseMinimumReachRatio = 0.95f;
    float ReleaseMaximumReachRatio = 0.99f;
    float ReleaseMinimumElbowAngleDegrees = 145.0f;
    float ReleaseMaximumElbowAngleDegrees = 165.0f;
    float FollowThroughMinimumReachRatio = 0.95f;
    float FollowThroughMaximumReachRatio = 1.0f;
    float FollowThroughMinimumElbowAngleDegrees = 155.0f;
    float FollowThroughMaximumElbowAngleDegrees = 175.0f;
    float RecoveryMaximumReachRatio = 0.75f;
    float RecoveryMinimumElbowAngleDegrees = 75.0f;
    float RecoveryMaximumElbowAngleDegrees = 110.0f;
    float RecoveryMaximumAbsoluteTorsoLateralCm = 35.0f;
    float RecoveryMaximumTorsoVerticalCm = -15.0f;
};

const FV7DriveArmGateThresholds& V7DriveArmThresholds()
{
    static const FV7DriveArmGateThresholds Values;
    return Values;
}

FQuat RotationAtFrame(const FRecipe& Recipe, const TArray<FPoseKey>& Poses,
    FName Bone, int32 Frame);

const TArray<FName>& MixedSpaceComponentRotationBones()
{
    static const TArray<FName> Values = {
        TEXT("pelvis"), TEXT("spine_01"), TEXT("spine_02"),
        TEXT("spine_03"), TEXT("spine_04"), TEXT("neck_01"), TEXT("head")
    };
    return Values;
}

const TArray<FName>& MixedSpaceLocalRotationBones()
{
    static const TArray<FName> Values = {
        TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"),
        TEXT("hand_l"), TEXT("clavicle_r"), TEXT("upperarm_r"),
        TEXT("lowerarm_r"), TEXT("hand_r"), TEXT("thigh_l"),
        TEXT("calf_l"), TEXT("foot_l"), TEXT("ball_l"), TEXT("thigh_r"),
        TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r"), TEXT("thumb_01_r"),
        TEXT("thumb_02_r"), TEXT("thumb_03_r"), TEXT("index_01_r"),
        TEXT("index_02_r"), TEXT("index_03_r"), TEXT("middle_01_r"),
        TEXT("middle_02_r"), TEXT("middle_03_r"), TEXT("ring_01_r"),
        TEXT("ring_02_r"), TEXT("ring_03_r"), TEXT("pinky_01_r"),
        TEXT("pinky_02_r"), TEXT("pinky_03_r")
    };
    return Values;
}

const TArray<FString>& RequiredPhases()
{
    static const TArray<FString> Values = {
        TEXT("Aim"), TEXT("RunUp"), TEXT("ReachBack"), TEXT("Plant"),
        TEXT("Acceleration"), TEXT("FollowThrough"), TEXT("Recovery")
    };
    return Values;
}

const TArray<FName>& RequiredCurves(int32 SchemaVersion)
{
    static const TArray<FName> BaseValues = {
        TEXT("DG_FootPlant_L"), TEXT("DG_FootPlant_R"),
        TEXT("DG_ReachbackAlpha"), TEXT("DG_BraceAlpha"),
        TEXT("DG_ReleaseApproachAlpha"), TEXT("DG_FollowThroughAlpha")
    };
    static const TArray<FName> GranularValues = {
        TEXT("DG_FootPlant_L"), TEXT("DG_FootPlant_R"),
        TEXT("DG_ReachbackAlpha"), TEXT("DG_BraceAlpha"),
        TEXT("DG_ReleaseApproachAlpha"), TEXT("DG_FollowThroughAlpha"),
        TEXT("DG_WeightShiftAlpha"), TEXT("DG_BraceCompressionAlpha"),
        TEXT("DG_HipDriveAlpha"), TEXT("DG_TorsoDriveAlpha"),
        TEXT("DG_ShoulderDriveAlpha"), TEXT("DG_ElbowLeadAlpha"),
        TEXT("DG_WristLagAlpha"), TEXT("DG_FingerReleaseAlpha"),
        TEXT("DG_OffArmCounterbalanceAlpha"), TEXT("DG_GazeTargetAlpha"),
        TEXT("DG_DiscPlaneAlpha"), TEXT("DG_BraceExtensionAlpha"),
        TEXT("DG_RecoveryBeatAlpha")
    };
    return SchemaVersion >= 3 ? GranularValues : BaseValues;
}

FString JsonString(const TSharedRef<FJsonObject>& Object)
{
    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    FJsonSerializer::Serialize(Object, Writer);
    return Output;
}

FString FailureJson(const FString& Error, bool bRollbackAttempted = false,
    bool bRollbackSucceeded = false)
{
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),
        TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v1"));
    Root->SetStringField(TEXT("status"), TEXT("FAIL"));
    Root->SetStringField(TEXT("error"), Error);
    Root->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    Root->SetBoolField(TEXT("rollback_succeeded"), bRollbackSucceeded);
    return JsonString(Root);
}

bool ReadNumber(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
    double& OutValue, FString& Error)
{
    if (!Object.IsValid() || !Object->TryGetNumberField(Field, OutValue)
        || !FMath::IsFinite(OutValue))
    {
        Error = FString::Printf(TEXT("Recipe field %s is missing or non-finite"), Field);
        return false;
    }
    return true;
}

bool ReadString(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
    FString& OutValue, FString& Error)
{
    if (!Object.IsValid() || !Object->TryGetStringField(Field, OutValue)
        || OutValue.IsEmpty())
    {
        Error = FString::Printf(TEXT("Recipe field %s is missing or empty"), Field);
        return false;
    }
    return true;
}

bool ReadNameArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field,
    TArray<FName>& OutValues, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values)
    {
        Error = FString::Printf(TEXT("Recipe field %s is missing or invalid"), Field);
        return false;
    }
    OutValues.Reset();
    TSet<FName> Unique;
    for (const TSharedPtr<FJsonValue>& Value : *Values)
    {
        FString Name;
        if (!Value.IsValid() || !Value->TryGetString(Name) || Name.IsEmpty())
        {
            Error = FString::Printf(TEXT("Recipe field %s contains an invalid bone"),
                Field);
            return false;
        }
        const FName Bone(*Name);
        if (Unique.Contains(Bone))
        {
            Error = FString::Printf(TEXT("Recipe field %s duplicates %s"),
                Field, *Name);
            return false;
        }
        Unique.Add(Bone);
        OutValues.Add(Bone);
    }
    return true;
}

bool ReadVector(const TSharedPtr<FJsonValue>& Value, FVector& OutVector, FString& Error)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Value.IsValid() || !Value->TryGetArray(Values) || !Values || Values->Num() != 3)
    {
        Error = TEXT("Recipe transform value must be a three-number array");
        return false;
    }
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!(*Values)[0]->TryGetNumber(X) || !(*Values)[1]->TryGetNumber(Y)
        || !(*Values)[2]->TryGetNumber(Z) || !FMath::IsFinite(X)
        || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        Error = TEXT("Recipe transform array contains a non-finite value");
        return false;
    }
    OutVector = FVector(X, Y, Z);
    return true;
}

void ApplyThrowingHandFingerCurl(FPoseKey& Pose, int32 SchemaVersion)
{
    // V5 preserves grip/contact alpha while reducing each generated distal
    // phalanx coefficient to 24 degrees. The largest authored 0.80 release
    // delta therefore remains 19.2 degrees, below the 20-degree native gate.
    const float Alpha = Pose.ThrowingHandGripAlpha;
    const float ProximalCurlDegrees = SchemaVersion >= 5 ? 18.0f : 20.0f;
    const float MiddleCurlDegrees = SchemaVersion >= 5 ? 24.0f : 35.0f;
    const float DistalCurlDegrees = SchemaVersion >= 5 ? 24.0f : 25.0f;
    Pose.Rotations.Add(TEXT("thumb_01_r"), FRotator(-8.0f, 4.0f, 12.0f) * Alpha);
    Pose.Rotations.Add(TEXT("thumb_02_r"), FRotator(0.0f, 0.0f, -18.0f) * Alpha);
    Pose.Rotations.Add(TEXT("thumb_03_r"), FRotator(0.0f, 0.0f, -12.0f) * Alpha);
    for (const TCHAR* Digit : {
        TEXT("index"), TEXT("middle"), TEXT("ring"), TEXT("pinky")})
    {
        Pose.Rotations.Add(
            FName(*FString::Printf(TEXT("%s_01_r"), Digit)),
            FRotator(0.0f, 0.0f, -ProximalCurlDegrees) * Alpha);
        Pose.Rotations.Add(
            FName(*FString::Printf(TEXT("%s_02_r"), Digit)),
            FRotator(0.0f, 0.0f, -MiddleCurlDegrees) * Alpha);
        Pose.Rotations.Add(
            FName(*FString::Printf(TEXT("%s_03_r"), Digit)),
            FRotator(0.0f, 0.0f, -DistalCurlDegrees) * Alpha);
    }
}

bool LoadRecipe(const FRecipeVersionSpec& Version, FRecipe& OutRecipe,
    FString& Error)
{
    OutRecipe.Version = Version;
    const FString RecipeFile = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / Version.RecipeRelativePath);
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *RecipeFile))
    {
        Error = FString::Printf(TEXT("Could not read original procedural recipe: %s"),
            *RecipeFile);
        return false;
    }
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Error = TEXT("Original procedural recipe is invalid JSON");
        return false;
    }
    FString Schema;
    FString Id;
    FString SourceKind;
    FString Performer;
    FString TargetSkeleton;
    FString RecipeVersion;
    FString AssetRevision;
    FString InterpolationMode = TEXT("SMOOTH_STEP");
    FString WorldMotionAuthority;
    FString BiomechanicalModel;
    FString DiscContactModel;
    FString RecoveryModel;
    FString RotationSpace;
    FString RootTrackPolicy;
    bool bExternal = true;
    bool bSyntheticDerivative = true;
    bool bCaptureClaim = true;
    bool bRootMotion = true;
    bool bHumanAnimation = true;
    bool bHumanContact = true;
    bool bPresentationRootTrajectory = false;
    double RecipeFrameRate = 0.0;
    double SchemaVersion = 0.0;
    if (!ReadString(Root, TEXT("schema"), Schema, Error)
        || !ReadNumber(Root, TEXT("schema_version"), SchemaVersion, Error)
        || !ReadString(Root, TEXT("recipe_id"), Id, Error)
        || !ReadString(Root, TEXT("source_kind"), SourceKind, Error)
        || !ReadString(Root, TEXT("performer"), Performer, Error)
        || !ReadString(Root, TEXT("target_skeleton"), TargetSkeleton, Error)
        || !Root->TryGetBoolField(TEXT("external_source_used"), bExternal)
        || !Root->TryGetBoolField(TEXT("derived_from_synthetic_fixture"), bSyntheticDerivative)
        || !Root->TryGetBoolField(TEXT("motion_capture_claim"), bCaptureClaim)
        || !Root->TryGetBoolField(TEXT("root_motion_enabled"), bRootMotion)
        || !Root->TryGetBoolField(TEXT("human_animation_approval"), bHumanAnimation)
        || !Root->TryGetBoolField(TEXT("human_disc_contact_approval"), bHumanContact)
        || !ReadNumber(Root, TEXT("frame_rate"), RecipeFrameRate, Error))
    {
        if (Error.IsEmpty()) Error = TEXT("Recipe policy fields are incomplete");
        return false;
    }
    if (Version.SchemaVersion >= 2
        && (!ReadString(Root, TEXT("recipe_version"), RecipeVersion, Error)
            || !ReadString(Root, TEXT("asset_revision"), AssetRevision, Error)
            || !ReadString(Root, TEXT("interpolation_mode"), InterpolationMode, Error)
            || !ReadString(Root, TEXT("world_motion_authority"),
                WorldMotionAuthority, Error)
            || !Root->TryGetBoolField(TEXT("presentation_root_trajectory_enabled"),
                bPresentationRootTrajectory)))
    {
        if (Error.IsEmpty()) Error = TEXT("Versioned recipe policy fields are incomplete");
        return false;
    }
    if (Version.SchemaVersion >= 3
        && (!ReadString(Root, TEXT("biomechanical_model"),
                BiomechanicalModel, Error)
            || !ReadString(Root, TEXT("disc_contact_model"),
                DiscContactModel, Error)
            || !ReadString(Root, TEXT("recovery_model"), RecoveryModel, Error)))
    {
        if (Error.IsEmpty()) Error = TEXT("Granular biomechanical policy fields are incomplete");
        return false;
    }
    if (Version.SchemaVersion >= 4
        && !ReadString(Root, TEXT("rotation_space"), RotationSpace, Error))
    {
        if (Error.IsEmpty()) Error = TEXT("Component-space rotation policy is incomplete");
        return false;
    }
    if (Version.SchemaVersion >= 5
        && (!ReadString(Root, TEXT("root_track_policy"), RootTrackPolicy, Error)
            || !ReadNameArray(Root, TEXT("component_rotation_bones"),
                OutRecipe.ComponentRotationBones, Error)
            || !ReadNameArray(Root, TEXT("local_rotation_bones"),
                OutRecipe.LocalRotationBones, Error)))
    {
        if (Error.IsEmpty()) Error = TEXT("Mixed-space rotation policy is incomplete");
        return false;
    }
    if (Schema != Version.RecipeSchema || Id != Version.RecipeId
        || FMath::RoundToInt(SchemaVersion) != Version.SchemaVersion
        || SourceKind != TEXT("PROJECT_AUTHORED_PROCEDURAL")
        || Performer != TEXT("NOT_APPLICABLE_PROCEDURAL_NO_CAPTURE")
        || TargetSkeleton != SkeletonPath || bExternal || bSyntheticDerivative
        || bCaptureClaim || bRootMotion || bHumanAnimation || bHumanContact
        || FMath::RoundToInt(RecipeFrameRate) != FrameRate)
    {
        Error = TEXT("Recipe provenance, authority, skeleton, or approval boundary differs");
        return false;
    }
    const FString ExpectedInterpolation = Version.SchemaVersion >= 3
        ? TEXT("CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED")
        : TEXT("CATMULL_ROM_PHASE_SHAPED");
    if (Version.SchemaVersion >= 2
        && (RecipeVersion != Version.Version
            || AssetRevision != Version.AssetRevision
            || InterpolationMode != ExpectedInterpolation
            || WorldMotionAuthority
                != TEXT("GAMEPLAY_PAWN_CAPSULE_PRESENTATION_ROOT_ONLY")
            || !bPresentationRootTrajectory))
    {
        Error = TEXT("Versioned interpolation or presentation-root authority differs");
        return false;
    }
    if (Version.SchemaVersion >= 3
        && (BiomechanicalModel != TEXT("RHBH_KINETIC_CHAIN_GRANULAR_V1")
            || DiscContactModel
                != TEXT("REACHBACK_PLANE_WRIST_LAG_FINGER_RELEASE")
            || RecoveryModel
                != TEXT("THREE_BEAT_DECELERATION_RECENTER_SETTLE")))
    {
        Error = TEXT("Granular biomechanical, disc-contact, or recovery model differs");
        return false;
    }
    if (Version.SchemaVersion == 4
        && RotationSpace != TEXT("COMPONENT_SPACE_ADDITIVE_TO_REFERENCE"))
    {
        Error = TEXT("v4 component-space rotation basis differs");
        return false;
    }
    if (Version.SchemaVersion >= 5
        && (RotationSpace
                != TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE")
            || RootTrackPolicy
                != TEXT("TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE")
            || OutRecipe.ComponentRotationBones
                != MixedSpaceComponentRotationBones()
            || OutRecipe.LocalRotationBones != MixedSpaceLocalRotationBones()))
    {
        Error = FString::Printf(
            TEXT("%s mixed-space/root-track partition differs"),
            *Version.Version);
        return false;
    }
    OutRecipe.InterpolationMode = InterpolationMode;
    OutRecipe.BiomechanicalModel = BiomechanicalModel;
    OutRecipe.DiscContactModel = DiscContactModel;
    OutRecipe.RecoveryModel = RecoveryModel;
    OutRecipe.RotationSpace = RotationSpace;
    OutRecipe.RootTrackPolicy = RootTrackPolicy;
    OutRecipe.bPresentationRootTrajectoryEnabled = bPresentationRootTrajectory;

    const TArray<TSharedPtr<FJsonValue>>* BoneValues = nullptr;
    if (!Root->TryGetArrayField(TEXT("animated_bones"), BoneValues) || !BoneValues
        || BoneValues->Num() != (Version.SchemaVersion >= 2 ? 39 : 20))
    {
        Error = TEXT("Recipe animated-bone set is missing");
        return false;
    }
    TSet<FName> BoneSet;
    for (const TSharedPtr<FJsonValue>& Value : *BoneValues)
    {
        FString Bone;
        if (!Value.IsValid() || !Value->TryGetString(Bone) || Bone.IsEmpty())
        {
            Error = TEXT("Recipe contains an invalid animated bone");
            return false;
        }
        const FName BoneName(*Bone);
        if (BoneSet.Contains(BoneName))
        {
            Error = FString::Printf(TEXT("Recipe duplicates animated bone %s"), *Bone);
            return false;
        }
        BoneSet.Add(BoneName);
        OutRecipe.AnimatedBones.Add(BoneName);
    }
    TArray<FName> RequiredBones = {
        FName(TEXT("root")), FName(TEXT("pelvis")), FName(TEXT("hand_r")),
        FName(TEXT("foot_l")), FName(TEXT("foot_r"))
    };
    if (Version.SchemaVersion >= 2)
    {
        RequiredBones.Append({
            FName(TEXT("neck_01")), FName(TEXT("head")),
            FName(TEXT("clavicle_l")), FName(TEXT("clavicle_r")),
            FName(TEXT("ball_l")), FName(TEXT("ball_r")),
            FName(TEXT("thumb_01_r")), FName(TEXT("thumb_02_r")),
            FName(TEXT("thumb_03_r")), FName(TEXT("index_01_r")),
            FName(TEXT("index_02_r")), FName(TEXT("index_03_r")),
            FName(TEXT("middle_01_r")), FName(TEXT("middle_02_r")),
            FName(TEXT("middle_03_r")), FName(TEXT("ring_01_r")),
            FName(TEXT("ring_02_r")), FName(TEXT("ring_03_r")),
            FName(TEXT("pinky_01_r")), FName(TEXT("pinky_02_r")),
            FName(TEXT("pinky_03_r"))
        });
    }
    for (const FName Required : RequiredBones)
    {
        if (!BoneSet.Contains(Required))
        {
            Error = FString::Printf(TEXT("Recipe omits required bone %s"),
                *Required.ToString());
            return false;
        }
    }
    if (Version.SchemaVersion >= 5)
    {
        TSet<FName> Partition;
        for (const FName Bone : OutRecipe.ComponentRotationBones)
        {
            Partition.Add(Bone);
        }
        for (const FName Bone : OutRecipe.LocalRotationBones)
        {
            if (Partition.Contains(Bone))
            {
                Error = FString::Printf(
                    TEXT("%s mixed-space partition overlaps at %s"),
                    *Version.Version, *Bone.ToString());
                return false;
            }
            Partition.Add(Bone);
        }
        if (Partition.Num() != 38 || BoneSet.Num() != 39
            || !BoneSet.Contains(TEXT("root")))
        {
            Error = FString::Printf(
                TEXT("%s mixed-space partition cardinality differs"),
                *Version.Version);
            return false;
        }
        for (const FName Bone : Partition)
        {
            if (!BoneSet.Contains(Bone))
            {
                Error = FString::Printf(
                    TEXT("%s mixed-space partition contains unauthored bone %s"),
                    *Version.Version, *Bone.ToString());
                return false;
            }
        }
    }

    const TArray<TSharedPtr<FJsonValue>>* Families = nullptr;
    if (!Root->TryGetArrayField(TEXT("families"), Families) || !Families
        || Families->Num() != ExpectedFamilyCount)
    {
        Error = TEXT("Recipe must contain exactly Drive, Approach, and Putt families");
        return false;
    }
    const TArray<FString> ExpectedNames = {TEXT("Drive"), TEXT("Approach"), TEXT("Putt")};
    for (int32 FamilyIndex = 0; FamilyIndex < Families->Num(); ++FamilyIndex)
    {
        const TSharedPtr<FJsonObject> FamilyObject = (*Families)[FamilyIndex]->AsObject();
        FFamilyRecipe& Family = OutRecipe.Families.AddDefaulted_GetRef();
        FString Style;
        double FrameCountValue = 0.0;
        double Duration = 0.0;
        double Release = 0.0;
        double Finish = 0.0;
        double PowerMin = 0.0;
        double PowerMax = 0.0;
        double MinimumPoseKeyCount = 7.0;
        double MaximumRootTranslationCm = 5.0;
        double MinimumForwardRootExcursionCm = 0.0;
        double RootReturnToleranceCm = 5.0;
        double PresentationRootScale = 1.0;
        double MaximumPoseKeyGapFrames =
            static_cast<double>(Version.MaximumPoseKeyGaps[FamilyIndex]);
        double MinimumRecoveryPoseKeyCount =
            static_cast<double>(Version.MinimumRecoveryPoseKeyCounts[FamilyIndex]);
        double MinimumAuthoredRotationChannelsPerPose = static_cast<double>(
            Version.MinimumAuthoredRotationChannelsPerPose[FamilyIndex]);
        if (!ReadString(FamilyObject, TEXT("motion_id"), Family.MotionId, Error)
            || !ReadString(FamilyObject, TEXT("family"), Family.Family, Error)
            || !ReadString(FamilyObject, TEXT("sequence_object_path"), Family.SequencePath, Error)
            || !ReadString(FamilyObject, TEXT("montage_object_path"), Family.MontagePath, Error)
            || !ReadString(FamilyObject, TEXT("style_id"), Style, Error)
            || !ReadNumber(FamilyObject, TEXT("frame_count"), FrameCountValue, Error)
            || !ReadNumber(FamilyObject, TEXT("duration_seconds"), Duration, Error)
            || !ReadNumber(FamilyObject, TEXT("release_frame"), Release, Error)
            || !ReadNumber(FamilyObject, TEXT("finish_frame"), Finish, Error)
            || !ReadNumber(FamilyObject, TEXT("recommended_power_min"), PowerMin, Error)
            || !ReadNumber(FamilyObject, TEXT("recommended_power_max"), PowerMax, Error))
        {
            return false;
        }
        if (Version.SchemaVersion >= 2
            && (!ReadNumber(FamilyObject, TEXT("minimum_pose_key_count"),
                    MinimumPoseKeyCount, Error)
                || !ReadNumber(FamilyObject,
                    TEXT("max_presentation_root_translation_cm"),
                    MaximumRootTranslationCm, Error)
                || !ReadNumber(FamilyObject,
                    TEXT("minimum_forward_root_excursion_cm"),
                    MinimumForwardRootExcursionCm, Error)
                || !ReadNumber(FamilyObject, TEXT("root_return_tolerance_cm"),
                    RootReturnToleranceCm, Error)
                || !ReadNumber(FamilyObject, TEXT("presentation_root_scale"),
                    PresentationRootScale, Error)))
        {
            return false;
        }
        if (Version.SchemaVersion >= 3
            && (!ReadNumber(FamilyObject, TEXT("maximum_pose_key_gap_frames"),
                    MaximumPoseKeyGapFrames, Error)
                || !ReadNumber(FamilyObject,
                    TEXT("minimum_recovery_pose_key_count"),
                    MinimumRecoveryPoseKeyCount, Error)
                || !ReadNumber(FamilyObject,
                    TEXT("minimum_authored_rotation_channels_per_pose"),
                    MinimumAuthoredRotationChannelsPerPose, Error)))
        {
            return false;
        }
        Family.StyleId = FName(*Style);
        Family.FrameCount = FMath::RoundToInt(FrameCountValue);
        Family.DurationSeconds = static_cast<float>(Duration);
        Family.ReleaseFrame = FMath::RoundToInt(Release);
        Family.FinishFrame = FMath::RoundToInt(Finish);
        Family.PowerMin = static_cast<float>(PowerMin);
        Family.PowerMax = static_cast<float>(PowerMax);
        Family.MinimumPoseKeyCount = FMath::RoundToInt(MinimumPoseKeyCount);
        Family.MaximumRootTranslationCm =
            static_cast<float>(MaximumRootTranslationCm);
        Family.MinimumForwardRootExcursionCm =
            static_cast<float>(MinimumForwardRootExcursionCm);
        Family.RootReturnToleranceCm = static_cast<float>(RootReturnToleranceCm);
        Family.PresentationRootScale = static_cast<float>(PresentationRootScale);
        Family.MaximumPoseKeyGapFrames =
            FMath::RoundToInt(MaximumPoseKeyGapFrames);
        Family.MinimumRecoveryPoseKeyCount =
            FMath::RoundToInt(MinimumRecoveryPoseKeyCount);
        Family.MinimumAuthoredRotationChannelsPerPose =
            FMath::RoundToInt(MinimumAuthoredRotationChannelsPerPose);
        if (Family.Family != ExpectedNames[FamilyIndex]
            || Family.SequencePath != Version.SequencePaths[FamilyIndex]
            || Family.MontagePath != Version.MontagePaths[FamilyIndex]
            || Family.FrameCount < 48
            || !FMath::IsNearlyEqual(Family.DurationSeconds,
                static_cast<float>(Family.FrameCount) / FrameRate, TimingTolerance)
            || Family.ReleaseFrame <= 0 || Family.ReleaseFrame >= Family.FinishFrame
            || Family.FinishFrame >= Family.FrameCount
            || Family.PowerMin < 0.0f || Family.PowerMax > 1.0f
            || Family.PowerMin >= Family.PowerMax)
        {
            Error = FString::Printf(TEXT("Recipe timing/target/power differs for %s"),
                *Family.Family);
            return false;
        }
        if (Family.MinimumPoseKeyCount
                != Version.MinimumPoseKeyCounts[FamilyIndex]
            || !FMath::IsNearlyEqual(Family.MaximumRootTranslationCm,
                Version.MaximumRootTranslationCm[FamilyIndex], TimingTolerance)
            || !FMath::IsNearlyEqual(Family.MinimumForwardRootExcursionCm,
                Version.MinimumForwardRootExcursionCm[FamilyIndex],
                TimingTolerance)
            || !FMath::IsNearlyEqual(Family.PresentationRootScale,
                Version.PresentationRootScales[FamilyIndex], TimingTolerance)
            || Family.MaximumPoseKeyGapFrames
                != Version.MaximumPoseKeyGaps[FamilyIndex]
            || Family.MinimumRecoveryPoseKeyCount
                != Version.MinimumRecoveryPoseKeyCounts[FamilyIndex]
            || Family.MinimumAuthoredRotationChannelsPerPose
                != Version.MinimumAuthoredRotationChannelsPerPose[FamilyIndex]
            || Family.RootReturnToleranceCm <= 0.0f
            || Family.RootReturnToleranceCm > 5.0f)
        {
            Error = FString::Printf(
                TEXT("Recipe root-trajectory limits differ for %s"),
                *Family.Family);
            return false;
        }

        const TArray<TSharedPtr<FJsonValue>>* Coverage = nullptr;
        if (!FamilyObject->TryGetArrayField(TEXT("authored_coverage"), Coverage)
            || !Coverage || Coverage->Num() != (FamilyIndex == 2 ? 7 : 8))
        {
            Error = FString::Printf(TEXT("Recipe coverage differs for %s"), *Family.Family);
            return false;
        }
        for (const TSharedPtr<FJsonValue>& Value : *Coverage)
        {
            FString Entry;
            if (!Value->TryGetString(Entry) || Entry.IsEmpty())
            {
                Error = TEXT("Recipe contains invalid semantic coverage");
                return false;
            }
            Family.Coverage.Add(Entry);
        }

        const TArray<TSharedPtr<FJsonValue>>* PhaseValues = nullptr;
        if (!FamilyObject->TryGetArrayField(TEXT("phases"), PhaseValues)
            || !PhaseValues || PhaseValues->Num() != RequiredPhases().Num())
        {
            Error = FString::Printf(TEXT("Recipe phase set differs for %s"), *Family.Family);
            return false;
        }
        int32 PreviousPhase = -1;
        for (int32 PhaseIndex = 0; PhaseIndex < PhaseValues->Num(); ++PhaseIndex)
        {
            const TSharedPtr<FJsonObject> PhaseObject = (*PhaseValues)[PhaseIndex]->AsObject();
            FPhaseSpec& Phase = Family.Phases.AddDefaulted_GetRef();
            double PhaseFrame = 0.0;
            if (!ReadString(PhaseObject, TEXT("name"), Phase.Name, Error)
                || !ReadNumber(PhaseObject, TEXT("frame"), PhaseFrame, Error))
            {
                return false;
            }
            Phase.Frame = FMath::RoundToInt(PhaseFrame);
            if (Phase.Name != RequiredPhases()[PhaseIndex]
                || Phase.Frame <= PreviousPhase || Phase.Frame >= Family.FrameCount)
            {
                Error = FString::Printf(TEXT("Recipe phase order differs for %s"),
                    *Family.Family);
                return false;
            }
            PreviousPhase = Phase.Frame;
        }
        if (Family.ReleaseFrame <= Family.Phases[4].Frame
            || Family.ReleaseFrame >= Family.Phases[5].Frame
            || Family.FinishFrame <= Family.Phases[6].Frame)
        {
            Error = FString::Printf(
                TEXT("Release/FollowThrough/Recovery/Finish ordering differs for %s"),
                *Family.Family);
            return false;
        }
        if (Version.SchemaVersion >= 3)
        {
            const TSharedPtr<FJsonObject> EventsObject =
                FamilyObject->GetObjectField(TEXT("biomechanical_events"));
            if (!EventsObject.IsValid() || EventsObject->Values.Num() != 15)
            {
                Error = FString::Printf(
                    TEXT("Granular biomechanical event set differs for %s"),
                    *Family.Family);
                return false;
            }
            auto ReadEventFrame = [&EventsObject, &Error](
                const TCHAR* Field, int32& OutFrame) -> bool
            {
                double Value = 0.0;
                if (!ReadNumber(EventsObject, Field, Value, Error)
                    || !FMath::IsNearlyEqual(Value,
                        static_cast<double>(FMath::RoundToInt(Value)),
                        static_cast<double>(TimingTolerance)))
                {
                    if (Error.IsEmpty())
                    {
                        Error = FString::Printf(
                            TEXT("Biomechanical event %s is not an integer frame"),
                            Field);
                    }
                    return false;
                }
                OutFrame = FMath::RoundToInt(Value);
                return true;
            };
            FBiomechanicalEvents& Events = Family.Biomechanics;
            if (!ReadEventFrame(TEXT("disc_reachback_plane_frame"),
                    Events.DiscReachbackPlaneFrame)
                || !ReadEventFrame(TEXT("weight_shift_frame"),
                    Events.WeightShiftFrame)
                || !ReadEventFrame(TEXT("brace_compression_frame"),
                    Events.BraceCompressionFrame)
                || !ReadEventFrame(TEXT("hip_fire_frame"), Events.HipFireFrame)
                || !ReadEventFrame(TEXT("torso_fire_frame"), Events.TorsoFireFrame)
                || !ReadEventFrame(TEXT("off_arm_counterbalance_frame"),
                    Events.OffArmCounterbalanceFrame)
                || !ReadEventFrame(TEXT("shoulder_fire_frame"),
                    Events.ShoulderFireFrame)
                || !ReadEventFrame(TEXT("elbow_lead_frame"),
                    Events.ElbowLeadFrame)
                || !ReadEventFrame(TEXT("wrist_lag_frame"), Events.WristLagFrame)
                || !ReadEventFrame(TEXT("release_frame"), Events.ReleaseFrame)
                || !ReadEventFrame(TEXT("brace_extension_frame"),
                    Events.BraceExtensionFrame)
                || !ReadEventFrame(TEXT("gaze_reacquire_frame"),
                    Events.GazeReacquireFrame)
                || !ReadEventFrame(TEXT("recovery_deceleration_frame"),
                    Events.RecoveryDecelerationFrame)
                || !ReadEventFrame(TEXT("recovery_recenter_frame"),
                    Events.RecoveryRecenterFrame)
                || !ReadEventFrame(TEXT("recovery_settle_frame"),
                    Events.RecoverySettleFrame))
            {
                return false;
            }
            if (Events.DiscReachbackPlaneFrame != Family.Phases[2].Frame
                || Events.DiscReachbackPlaneFrame >= Events.WeightShiftFrame
                || Events.WeightShiftFrame >= Events.BraceCompressionFrame
                || Events.BraceCompressionFrame != Family.Phases[3].Frame
                || Events.BraceCompressionFrame >= Events.HipFireFrame
                || Events.HipFireFrame >= Events.TorsoFireFrame
                || Events.TorsoFireFrame != Family.Phases[4].Frame
                || Events.TorsoFireFrame >= Events.OffArmCounterbalanceFrame
                || Events.OffArmCounterbalanceFrame >= Events.ShoulderFireFrame
                || Events.ShoulderFireFrame >= Events.ElbowLeadFrame
                || Events.ElbowLeadFrame >= Events.WristLagFrame
                || Events.WristLagFrame >= Events.ReleaseFrame
                || Events.ReleaseFrame != Family.ReleaseFrame
                || Events.ReleaseFrame >= Events.BraceExtensionFrame
                || Events.BraceExtensionFrame >= Events.GazeReacquireFrame
                || Events.GazeReacquireFrame != Family.Phases[5].Frame
                || Events.GazeReacquireFrame
                    >= Events.RecoveryDecelerationFrame
                || Events.RecoveryDecelerationFrame
                    >= Events.RecoveryRecenterFrame
                || Events.RecoveryRecenterFrame != Family.Phases[6].Frame
                || Events.RecoveryRecenterFrame >= Events.RecoverySettleFrame
                || Events.RecoverySettleFrame >= Family.FinishFrame)
            {
                Error = FString::Printf(
                    TEXT("Granular kinetic-chain/recovery event order differs for %s"),
                    *Family.Family);
                return false;
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* PoseValues = nullptr;
        if (!FamilyObject->TryGetArrayField(TEXT("pose_keys"), PoseValues)
            || !PoseValues
            || PoseValues->Num() < Family.MinimumPoseKeyCount)
        {
            Error = FString::Printf(TEXT("Recipe pose keys are incomplete for %s"),
                *Family.Family);
            return false;
        }
        int32 PreviousPose = -1;
        for (const TSharedPtr<FJsonValue>& PoseValue : *PoseValues)
        {
            const TSharedPtr<FJsonObject> PoseObject = PoseValue->AsObject();
            double PoseFrame = 0.0;
            if (!ReadNumber(PoseObject, TEXT("frame"), PoseFrame, Error))
            {
                return false;
            }
            FPoseKey& Pose = Family.Poses.AddDefaulted_GetRef();
            Pose.Frame = FMath::RoundToInt(PoseFrame);
            if (Pose.Frame <= PreviousPose || Pose.Frame > Family.FrameCount)
            {
                Error = FString::Printf(TEXT("Recipe pose order differs for %s"),
                    *Family.Family);
                return false;
            }
            PreviousPose = Pose.Frame;
            if (Version.SchemaVersion >= 2)
            {
                double GripAlpha = 0.0;
                if (!ReadNumber(PoseObject,
                        TEXT("throwing_hand_grip_alpha"), GripAlpha, Error)
                    || GripAlpha < 0.0 || GripAlpha > 1.0)
                {
                    if (Error.IsEmpty())
                    {
                        Error = TEXT("v2 throwing-hand grip alpha is out of bounds");
                    }
                    return false;
                }
                Pose.ThrowingHandGripAlpha = static_cast<float>(GripAlpha);
            }
            const TSharedPtr<FJsonObject> TranslationObject =
                PoseObject->GetObjectField(TEXT("translation_cm"));
            const TSharedPtr<FJsonObject> RotationObject =
                PoseObject->GetObjectField(TEXT("rotation_degrees"));
            if (!TranslationObject.IsValid() || !RotationObject.IsValid())
            {
                Error = TEXT("Recipe pose is missing translation or rotation data");
                return false;
            }
            Pose.AuthoredRotationChannelCount = RotationObject->Values.Num();
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : TranslationObject->Values)
            {
                const FName Bone(*Pair.Key);
                FVector Value;
                const double TranslationLimit = Bone == FName(TEXT("root"))
                    ? Family.MaximumRootTranslationCm : 8.0;
                if (!BoneSet.Contains(Bone) || !ReadVector(Pair.Value, Value, Error))
                {
                    if (Error.IsEmpty()) Error = TEXT("Recipe translation is unsafe");
                    return false;
                }
                if (Bone == FName(TEXT("root")))
                {
                    Value *= Family.PresentationRootScale;
                }
                if (Value.Size() > TranslationLimit)
                {
                    Error = TEXT("Recipe translation exceeds its root-specific limit");
                    return false;
                }
                Pose.Translations.Add(Bone, Value);
            }
            for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : RotationObject->Values)
            {
                const FName Bone(*Pair.Key);
                FVector Degrees;
                float RotationLimit = 90.0f;
                constexpr float MaximumV7DriveSupportLowerArmRollDegrees = 92.0f;
                if (Version.SchemaVersion >= 2)
                {
                    const FString BoneName = Bone.ToString();
                    if (BoneName.StartsWith(TEXT("thumb_"))
                        || BoneName.StartsWith(TEXT("index_"))
                        || BoneName.StartsWith(TEXT("middle_"))
                        || BoneName.StartsWith(TEXT("ring_"))
                        || BoneName.StartsWith(TEXT("pinky_")))
                    {
                        RotationLimit = 55.0f;
                    }
                    else if (Bone == FName(TEXT("neck_01"))
                        || Bone == FName(TEXT("head")))
                    {
                        RotationLimit = 25.0f;
                    }
                    else if (Bone == FName(TEXT("ball_l"))
                        || Bone == FName(TEXT("ball_r")))
                    {
                        RotationLimit = 35.0f;
                    }
                }
                if (!BoneSet.Contains(Bone) || !ReadVector(Pair.Value, Degrees, Error))
                {
                    if (Error.IsEmpty()) Error = TEXT("Recipe rotation is unsafe");
                    return false;
                }
                const bool bV7DriveSupportLowerArm =
                    Version.SchemaVersion == 7
                    && Family.Family == TEXT("Drive")
                    && Bone == FName(TEXT("lowerarm_l"));
                const float RollRotationLimit = bV7DriveSupportLowerArm
                    ? MaximumV7DriveSupportLowerArmRollDegrees
                    : RotationLimit;
                if (FMath::Abs(Degrees.X) > RotationLimit
                    || FMath::Abs(Degrees.Y) > RotationLimit
                    || FMath::Abs(Degrees.Z) > RollRotationLimit)
                {
                    Error = FString::Printf(
                        TEXT("Recipe rotation is unsafe for %s %s frame %d bone %s: Pitch/Yaw/Roll [%.6f, %.6f, %.6f] exceeds [%.3f, %.3f, %.3f]"),
                        *Version.Version, *Family.Family, Pose.Frame, *Pair.Key,
                        Degrees.X, Degrees.Y, Degrees.Z,
                        RotationLimit, RotationLimit, RollRotationLimit);
                    return false;
                }
                Pose.Rotations.Add(Bone,
                    FRotator(Degrees.X, Degrees.Y, Degrees.Z));
            }
            if (Version.SchemaVersion >= 2)
            {
                ApplyThrowingHandFingerCurl(Pose, Version.SchemaVersion);
            }
            if (Version.SchemaVersion >= 5)
            {
                if (TranslationObject->Values.Num() != 1
                    || !TranslationObject->Values.Contains(TEXT("root"))
                    || RotationObject->Values.Contains(TEXT("root")))
                {
                    Error = FString::Printf(
                        TEXT("%s %s frame %d must author root translation only and no root rotation"),
                        *Version.Version, *Family.Family, Pose.Frame);
                    return false;
                }
                for (const FName Bone : OutRecipe.ComponentRotationBones)
                {
                    if (!Pose.Rotations.Contains(Bone))
                    {
                        Error = FString::Printf(
                            TEXT("%s %s frame %d omits explicit component-space bone %s"),
                            *Version.Version, *Family.Family, Pose.Frame,
                            *Bone.ToString());
                        return false;
                    }
                }
                for (const TPair<FName, FRotator>& Pair : Pose.Rotations)
                {
                    if (!OutRecipe.ComponentRotationBones.Contains(Pair.Key)
                        && !OutRecipe.LocalRotationBones.Contains(Pair.Key))
                    {
                        Error = FString::Printf(
                            TEXT("%s %s frame %d rotation is outside the exact partition: %s"),
                            *Version.Version, *Family.Family, Pose.Frame,
                            *Pair.Key.ToString());
                        return false;
                    }
                }
                for (const FName Calf : {
                    FName(TEXT("calf_l")), FName(TEXT("calf_r"))})
                {
                    const FRotator* KneeFlexion = Pose.Rotations.Find(Calf);
                    if (!KneeFlexion
                        || !FMath::IsNearlyZero(KneeFlexion->Pitch, 0.001f)
                        || !FMath::IsNearlyZero(KneeFlexion->Yaw, 0.001f)
                        || KneeFlexion->Roll <= 0.0f)
                    {
                        Error = FString::Printf(
                            TEXT("%s %s frame %d %s must use positive local-X Roll with zero Pitch/Yaw"),
                            *Version.Version, *Family.Family, Pose.Frame,
                            *Calf.ToString());
                        return false;
                    }
                }
            }
        }
        if (Family.Poses[0].Frame != 0
            || Family.Poses.Last().Frame != Family.FrameCount)
        {
            Error = FString::Printf(TEXT("Recipe poses must cover first/last frame for %s"),
                *Family.Family);
            return false;
        }
        if (Version.SchemaVersion >= 2)
        {
            TSet<int32> PoseFrames;
            for (const FPoseKey& Pose : Family.Poses)
            {
                PoseFrames.Add(Pose.Frame);
            }
            for (const FPhaseSpec& Phase : Family.Phases)
            {
                if (!PoseFrames.Contains(Phase.Frame))
                {
                    Error = FString::Printf(
                        TEXT("v2 pose keys omit phase %s for %s"),
                        *Phase.Name, *Family.Family);
                    return false;
                }
            }
            if (!PoseFrames.Contains(Family.ReleaseFrame)
                || !PoseFrames.Contains(Family.FinishFrame))
            {
                Error = FString::Printf(
                    TEXT("v2 pose keys omit release or finish for %s"),
                    *Family.Family);
                return false;
            }
            const FPoseKey* AccelerationPose = Family.Poses.FindByPredicate(
                [&Family](const FPoseKey& Pose)
                {
                    return Pose.Frame == Family.Phases[4].Frame;
                });
            const FPoseKey* ReleasePose = Family.Poses.FindByPredicate(
                [&Family](const FPoseKey& Pose)
                {
                    return Pose.Frame == Family.ReleaseFrame;
                });
            const FPoseKey* FollowThroughPose = Family.Poses.FindByPredicate(
                [&Family](const FPoseKey& Pose)
                {
                    return Pose.Frame == Family.Phases[5].Frame;
                });
            if (!AccelerationPose || !ReleasePose || !FollowThroughPose
                || AccelerationPose->ThrowingHandGripAlpha < 0.65f
                || ReleasePose->ThrowingHandGripAlpha > 0.25f
                || FollowThroughPose->ThrowingHandGripAlpha > 0.10f)
            {
                Error = FString::Printf(
                    TEXT("Throwing-hand curl/release shape differs for %s"),
                    *Family.Family);
                return false;
            }
            float MinimumRootX = TNumericLimits<float>::Max();
            float MaximumRootX = TNumericLimits<float>::Lowest();
            for (const FPoseKey& Pose : Family.Poses)
            {
                const FVector* RootTranslation =
                    Pose.Translations.Find(TEXT("root"));
                if (!RootTranslation)
                {
                    Error = FString::Printf(
                        TEXT("Every v2 pose must author the presentation root for %s"),
                        *Family.Family);
                    return false;
                }
                MinimumRootX = FMath::Min(MinimumRootX,
                    static_cast<float>(RootTranslation->X));
                MaximumRootX = FMath::Max(MaximumRootX,
                    static_cast<float>(RootTranslation->X));
            }
            const FVector StartRoot =
                Family.Poses[0].Translations.FindRef(TEXT("root"));
            const FVector EndRoot =
                Family.Poses.Last().Translations.FindRef(TEXT("root"));
            if (FVector::Dist(StartRoot, EndRoot)
                    > Family.RootReturnToleranceCm
                || MaximumRootX - MinimumRootX
                    < Family.MinimumForwardRootExcursionCm)
            {
                Error = FString::Printf(
                    TEXT("Presentation root trajectory is incomplete for %s"),
                    *Family.Family);
                return false;
            }
            if (Version.SchemaVersion >= 3)
            {
                int32 MaximumGap = 0;
                int32 RecoveryPoseCount = 0;
                int32 MinimumRotationChannels = TNumericLimits<int32>::Max();
                for (int32 PoseIndex = 0; PoseIndex < Family.Poses.Num(); ++PoseIndex)
                {
                    const FPoseKey& Pose = Family.Poses[PoseIndex];
                    if (PoseIndex > 0)
                    {
                        MaximumGap = FMath::Max(MaximumGap,
                            Pose.Frame - Family.Poses[PoseIndex - 1].Frame);
                    }
                    if (Pose.Frame >= Family.Phases[5].Frame
                        && Pose.Frame <= Family.FinishFrame)
                    {
                        ++RecoveryPoseCount;
                    }
                    MinimumRotationChannels = FMath::Min(
                        MinimumRotationChannels,
                        Pose.AuthoredRotationChannelCount);
                }
                const FBiomechanicalEvents& Events = Family.Biomechanics;
                const TArray<int32> RequiredEventFrames = {
                    Events.DiscReachbackPlaneFrame, Events.WeightShiftFrame,
                    Events.BraceCompressionFrame, Events.HipFireFrame,
                    Events.TorsoFireFrame, Events.OffArmCounterbalanceFrame,
                    Events.ShoulderFireFrame, Events.ElbowLeadFrame,
                    Events.WristLagFrame, Events.ReleaseFrame,
                    Events.BraceExtensionFrame, Events.GazeReacquireFrame,
                    Events.RecoveryDecelerationFrame,
                    Events.RecoveryRecenterFrame, Events.RecoverySettleFrame
                };
                for (const int32 EventFrame : RequiredEventFrames)
                {
                    if (!PoseFrames.Contains(EventFrame))
                    {
                        Error = FString::Printf(
                            TEXT("Granular pose keys omit event frame %d for %s"),
                            EventFrame, *Family.Family);
                        return false;
                    }
                }
                const FPoseKey* WristLagPose = Family.Poses.FindByPredicate(
                    [&Events](const FPoseKey& Pose)
                    {
                        return Pose.Frame == Events.WristLagFrame;
                    });
                if (MaximumGap > Family.MaximumPoseKeyGapFrames
                    || RecoveryPoseCount < Family.MinimumRecoveryPoseKeyCount
                    || MinimumRotationChannels
                        < Family.MinimumAuthoredRotationChannelsPerPose
                    || !WristLagPose
                    || WristLagPose->ThrowingHandGripAlpha < 0.85f
                    || ReleasePose->ThrowingHandGripAlpha > 0.15f
                    || FollowThroughPose->ThrowingHandGripAlpha > 0.05f)
                {
                    Error = FString::Printf(
                        TEXT("Granular density/grip/recovery contract differs for %s"),
                        *Family.Family);
                    return false;
                }
                if (Version.SchemaVersion >= 5)
                {
                    constexpr float MaximumFingerFrameDeltaDegrees = 20.0f;
                    const TArray<FName> GeneratedFingerBones = {
                        TEXT("thumb_01_r"), TEXT("thumb_02_r"),
                        TEXT("thumb_03_r"), TEXT("index_01_r"),
                        TEXT("index_02_r"), TEXT("index_03_r"),
                        TEXT("middle_01_r"), TEXT("middle_02_r"),
                        TEXT("middle_03_r"), TEXT("ring_01_r"),
                        TEXT("ring_02_r"), TEXT("ring_03_r"),
                        TEXT("pinky_01_r"), TEXT("pinky_02_r"),
                        TEXT("pinky_03_r")
                    };
                    for (const FName Bone : GeneratedFingerBones)
                    {
                        FQuat Previous = RotationAtFrame(
                            OutRecipe, Family.Poses, Bone, 0);
                        for (int32 Frame = 1; Frame <= Family.FrameCount; ++Frame)
                        {
                            const FQuat Current = RotationAtFrame(
                                OutRecipe, Family.Poses, Bone, Frame);
                            const float DeltaDegrees = FMath::RadiansToDegrees(
                                static_cast<float>(Previous.AngularDistance(Current)));
                            if (DeltaDegrees
                                > MaximumFingerFrameDeltaDegrees + 0.001f)
                            {
                                Error = FString::Printf(
                                    TEXT("%s %s generated finger %s pops %.3f degrees at frame %d"),
                                    *Version.Version, *Family.Family,
                                    *Bone.ToString(), DeltaDegrees, Frame);
                                return false;
                            }
                            Previous = Current;
                        }
                    }
                }
            }
        }

        const TSharedPtr<FJsonObject> CurvesObject =
            FamilyObject->GetObjectField(TEXT("curves"));
        const TArray<FName>& VersionCurves = RequiredCurves(Version.SchemaVersion);
        if (!CurvesObject.IsValid() || CurvesObject->Values.Num() != VersionCurves.Num())
        {
            Error = FString::Printf(TEXT("Recipe curve set differs for %s"), *Family.Family);
            return false;
        }
        for (const FName RequiredCurve : VersionCurves)
        {
            const TArray<TSharedPtr<FJsonValue>>* CurveValues = nullptr;
            if (!CurvesObject->TryGetArrayField(RequiredCurve.ToString(), CurveValues)
                || !CurveValues || CurveValues->Num() < 2)
            {
                Error = FString::Printf(TEXT("Recipe curve %s is incomplete"),
                    *RequiredCurve.ToString());
                return false;
            }
            FCurveSpec& Curve = Family.Curves.AddDefaulted_GetRef();
            Curve.Name = RequiredCurve;
            int32 PreviousCurveFrame = -1;
            for (const TSharedPtr<FJsonValue>& CurveValue : *CurveValues)
            {
                const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
                double FrameValue = 0.0;
                double Scalar = 0.0;
                if (!CurveValue->TryGetArray(Pair) || !Pair || Pair->Num() != 2
                    || !(*Pair)[0]->TryGetNumber(FrameValue)
                    || !(*Pair)[1]->TryGetNumber(Scalar)
                    || !FMath::IsFinite(FrameValue) || !FMath::IsFinite(Scalar))
                {
                    Error = FString::Printf(TEXT("Recipe curve %s has an invalid key"),
                        *RequiredCurve.ToString());
                    return false;
                }
                const int32 CurveFrame = FMath::RoundToInt(FrameValue);
                if (CurveFrame <= PreviousCurveFrame || CurveFrame > Family.FrameCount
                    || Scalar < 0.0 || Scalar > 1.0)
                {
                    Error = FString::Printf(TEXT("Recipe curve %s key is out of bounds"),
                        *RequiredCurve.ToString());
                    return false;
                }
                PreviousCurveFrame = CurveFrame;
                Curve.Keys.Emplace(CurveFrame, static_cast<float>(Scalar));
            }
        }
        if (Version.SchemaVersion >= 3)
        {
            auto HasCurveKey = [&Family](const FName CurveName,
                int32 Frame, float ExpectedValue) -> bool
            {
                const FCurveSpec* Curve = Family.Curves.FindByPredicate(
                    [CurveName](const FCurveSpec& Candidate)
                    {
                        return Candidate.Name == CurveName;
                    });
                if (!Curve) return false;
                const TPair<int32, float>* Key = Curve->Keys.FindByPredicate(
                    [Frame](const TPair<int32, float>& Candidate)
                    {
                        return Candidate.Key == Frame;
                    });
                return Key && FMath::IsNearlyEqual(
                    Key->Value, ExpectedValue, TimingTolerance);
            };
            const FBiomechanicalEvents& Events = Family.Biomechanics;
            const TArray<TTuple<FName, int32, float>> EventCurveKeys = {
                {TEXT("DG_WeightShiftAlpha"), Events.WeightShiftFrame, 1.0f},
                {TEXT("DG_BraceCompressionAlpha"),
                    Events.BraceCompressionFrame, 1.0f},
                {TEXT("DG_HipDriveAlpha"), Events.HipFireFrame, 1.0f},
                {TEXT("DG_TorsoDriveAlpha"), Events.TorsoFireFrame, 1.0f},
                {TEXT("DG_ShoulderDriveAlpha"), Events.ShoulderFireFrame, 1.0f},
                {TEXT("DG_ElbowLeadAlpha"), Events.ElbowLeadFrame, 1.0f},
                {TEXT("DG_WristLagAlpha"), Events.WristLagFrame, 1.0f},
                {TEXT("DG_FingerReleaseAlpha"), Events.ReleaseFrame, 1.0f},
                {TEXT("DG_OffArmCounterbalanceAlpha"),
                    Events.OffArmCounterbalanceFrame, 1.0f},
                {TEXT("DG_GazeTargetAlpha"), Events.GazeReacquireFrame, 1.0f},
                {TEXT("DG_DiscPlaneAlpha"),
                    Events.DiscReachbackPlaneFrame, 1.0f},
                {TEXT("DG_BraceExtensionAlpha"),
                    Events.BraceExtensionFrame, 1.0f},
                {TEXT("DG_RecoveryBeatAlpha"),
                    Events.RecoveryDecelerationFrame, 0.35f},
                {TEXT("DG_RecoveryBeatAlpha"),
                    Events.RecoveryRecenterFrame, 0.70f},
                {TEXT("DG_RecoveryBeatAlpha"),
                    Events.RecoverySettleFrame, 1.0f}
            };
            for (const TTuple<FName, int32, float>& Required : EventCurveKeys)
            {
                if (!HasCurveKey(Required.Get<0>(), Required.Get<1>(),
                        Required.Get<2>()))
                {
                    Error = FString::Printf(
                        TEXT("Granular curve %s omits event frame %d for %s"),
                        *Required.Get<0>().ToString(), Required.Get<1>(),
                        *Family.Family);
                    return false;
                }
            }
        }
    }
    OutRecipe.SourceTextMd5 = FMD5::HashAnsiString(*Text).ToUpper();
    return true;
}

float FrameToTime(int32 Frame)
{
    return static_cast<float>(Frame) / FrameRate;
}

int32 UpperPoseIndex(const TArray<FPoseKey>& Poses, int32 Frame)
{
    int32 Upper = 1;
    while (Upper < Poses.Num() && Poses[Upper].Frame < Frame)
    {
        ++Upper;
    }
    return FMath::Clamp(Upper, 1, Poses.Num() - 1);
}

FVector CatmullRom(const FVector& P0, const FVector& P1,
    const FVector& P2, const FVector& P3, float Alpha)
{
    const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
    const float T2 = T * T;
    const float T3 = T2 * T;
    return 0.5f * ((2.0f * P1)
        + (-P0 + P2) * T
        + (2.0f * P0 - 5.0f * P1 + 4.0f * P2 - P3) * T2
        + (-P0 + 3.0f * P1 - 3.0f * P2 + P3) * T3);
}

FVector RotationVector(const FPoseKey& Pose, FName Bone)
{
    const FRotator Rotation = Pose.Rotations.FindRef(Bone);
    return FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
}

FQuat RotationAtFrame(const FRecipe& Recipe, const TArray<FPoseKey>& Poses,
    FName Bone, int32 Frame)
{
    const int32 Upper = UpperPoseIndex(Poses, Frame);
    const FPoseKey& A = Poses[Upper - 1];
    const FPoseKey& B = Poses[Upper];
    const float Linear = static_cast<float>(Frame - A.Frame)
        / FMath::Max(1, B.Frame - A.Frame);
    if (Recipe.InterpolationMode == TEXT("CATMULL_ROM_PHASE_SHAPED")
        || Recipe.InterpolationMode
            == TEXT("CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED"))
    {
        const FPoseKey& P0 = Poses[FMath::Max(0, Upper - 2)];
        const FPoseKey& P3 = Poses[FMath::Min(Poses.Num() - 1, Upper + 1)];
        const FVector Degrees = CatmullRom(RotationVector(P0, Bone),
            RotationVector(A, Bone), RotationVector(B, Bone),
            RotationVector(P3, Bone), Linear);
        return FRotator(Degrees.X, Degrees.Y, Degrees.Z)
            .Quaternion().GetNormalized();
    }
    const float Alpha = FMath::SmoothStep(0.0f, 1.0f,
        FMath::Clamp(Linear, 0.0f, 1.0f));
    return FQuat::Slerp(A.Rotations.FindRef(Bone).Quaternion(),
        B.Rotations.FindRef(Bone).Quaternion(), Alpha).GetNormalized();
}

FVector TranslationAtFrame(const FRecipe& Recipe,
    const TArray<FPoseKey>& Poses, FName Bone, int32 Frame)
{
    const int32 Upper = UpperPoseIndex(Poses, Frame);
    const FPoseKey& A = Poses[Upper - 1];
    const FPoseKey& B = Poses[Upper];
    const float Linear = static_cast<float>(Frame - A.Frame)
        / FMath::Max(1, B.Frame - A.Frame);
    if (Recipe.InterpolationMode == TEXT("CATMULL_ROM_PHASE_SHAPED")
        || Recipe.InterpolationMode
            == TEXT("CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED"))
    {
        const FPoseKey& P0 = Poses[FMath::Max(0, Upper - 2)];
        const FPoseKey& P3 = Poses[FMath::Min(Poses.Num() - 1, Upper + 1)];
        return CatmullRom(P0.Translations.FindRef(Bone),
            A.Translations.FindRef(Bone), B.Translations.FindRef(Bone),
            P3.Translations.FindRef(Bone), Linear);
    }
    const float Alpha = FMath::SmoothStep(0.0f, 1.0f,
        FMath::Clamp(Linear, 0.0f, 1.0f));
    return FMath::Lerp(A.Translations.FindRef(Bone),
        B.Translations.FindRef(Bone), Alpha);
}

FRotator ComponentRotationIntentAtKey(const FPoseKey& Pose,
    const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
{
    // V4 recipe rotations are component-space additive intent. An omitted
    // descendant inherits its nearest keyed ancestor so a torso/limb turn
    // carries the chain instead of being cancelled by an implicit identity.
    for (int32 CurrentIndex = BoneIndex; CurrentIndex != INDEX_NONE;
        CurrentIndex = RefSkeleton.GetParentIndex(CurrentIndex))
    {
        if (const FRotator* Rotation = Pose.Rotations.Find(
                RefSkeleton.GetBoneName(CurrentIndex)))
        {
            return *Rotation;
        }
    }
    return FRotator::ZeroRotator;
}

FVector ComponentRotationIntentVectorAtKey(const FPoseKey& Pose,
    const FReferenceSkeleton& RefSkeleton, int32 BoneIndex)
{
    const FRotator Rotation = ComponentRotationIntentAtKey(
        Pose, RefSkeleton, BoneIndex);
    return FVector(Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
}

FQuat ComponentRotationIntentAtFrame(const FRecipe& Recipe,
    const FFamilyRecipe& Family, const FReferenceSkeleton& RefSkeleton,
    int32 BoneIndex, int32 Frame)
{
    const int32 Upper = UpperPoseIndex(Family.Poses, Frame);
    const FPoseKey& A = Family.Poses[Upper - 1];
    const FPoseKey& B = Family.Poses[Upper];
    const float Linear = static_cast<float>(Frame - A.Frame)
        / FMath::Max(1, B.Frame - A.Frame);
    if (Recipe.InterpolationMode
        == TEXT("CATMULL_ROM_BIOMECHANICAL_PHASE_SHAPED"))
    {
        const FPoseKey& P0 = Family.Poses[FMath::Max(0, Upper - 2)];
        const FPoseKey& P3 = Family.Poses[
            FMath::Min(Family.Poses.Num() - 1, Upper + 1)];
        const FVector Degrees = CatmullRom(
            ComponentRotationIntentVectorAtKey(P0, RefSkeleton, BoneIndex),
            ComponentRotationIntentVectorAtKey(A, RefSkeleton, BoneIndex),
            ComponentRotationIntentVectorAtKey(B, RefSkeleton, BoneIndex),
            ComponentRotationIntentVectorAtKey(P3, RefSkeleton, BoneIndex),
            Linear);
        return FRotator(Degrees.X, Degrees.Y, Degrees.Z)
            .Quaternion().GetNormalized();
    }
    const float Alpha = FMath::SmoothStep(0.0f, 1.0f,
        FMath::Clamp(Linear, 0.0f, 1.0f));
    return FQuat::Slerp(
        ComponentRotationIntentAtKey(A, RefSkeleton, BoneIndex).Quaternion(),
        ComponentRotationIntentAtKey(B, RefSkeleton, BoneIndex).Quaternion(),
        Alpha).GetNormalized();
}

void BuildComponentPose(const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& LocalPose, TArray<FTransform>& OutComponentPose)
{
    OutComponentPose.SetNum(LocalPose.Num());
    for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
    {
        const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        OutComponentPose[BoneIndex] = ParentIndex == INDEX_NONE
            ? LocalPose[BoneIndex]
            : LocalPose[BoneIndex] * OutComponentPose[ParentIndex];
    }
}

float DirectionDeltaDegrees(const FVector& A, const FVector& B)
{
    if (A.IsNearlyZero(0.001f) || B.IsNearlyZero(0.001f))
    {
        return 0.0f;
    }
    return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
        FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()),
        -1.0f, 1.0f)));
}

bool MeasureArmSpatialPose(const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& ComponentPose, const TCHAR* Side,
    FArmSpatialPoseMetrics& OutMetrics, FString& Error)
{
    const FString SideName(Side);
    if (SideName != TEXT("l") && SideName != TEXT("r"))
    {
        Error = TEXT("Arm spatial gate received an invalid side");
        return false;
    }
    const int32 ShoulderIndex = RefSkeleton.FindBoneIndex(
        FName(*FString::Printf(TEXT("upperarm_%s"), *SideName)));
    const int32 ElbowIndex = RefSkeleton.FindBoneIndex(
        FName(*FString::Printf(TEXT("lowerarm_%s"), *SideName)));
    const int32 HandIndex = RefSkeleton.FindBoneIndex(
        FName(*FString::Printf(TEXT("hand_%s"), *SideName)));
    const int32 PelvisIndex = RefSkeleton.FindBoneIndex(TEXT("pelvis"));
    const int32 ChestIndex = RefSkeleton.FindBoneIndex(TEXT("spine_04"));
    if (!ComponentPose.IsValidIndex(ShoulderIndex)
        || !ComponentPose.IsValidIndex(ElbowIndex)
        || !ComponentPose.IsValidIndex(HandIndex)
        || !ComponentPose.IsValidIndex(PelvisIndex)
        || !ComponentPose.IsValidIndex(ChestIndex))
    {
        Error = FString::Printf(
            TEXT("Arm spatial gate lacks the %s shoulder/elbow/hand/torso chain"),
            *SideName);
        return false;
    }

    const FVector Shoulder = ComponentPose[ShoulderIndex].GetTranslation();
    const FVector Elbow = ComponentPose[ElbowIndex].GetTranslation();
    const FVector Hand = ComponentPose[HandIndex].GetTranslation();
    const FVector Pelvis = ComponentPose[PelvisIndex].GetTranslation();
    const FVector Chest = ComponentPose[ChestIndex].GetTranslation();
    const FVector Proximal = Shoulder - Elbow;
    const FVector Distal = Hand - Elbow;
    const float UpperLength = Proximal.Size();
    const float LowerLength = Distal.Size();
    const FVector ShoulderToHand = Hand - Shoulder;
    const FVector TorsoAxis = Chest - Pelvis;
    if (UpperLength <= 0.01f || LowerLength <= 0.01f
        || ShoulderToHand.IsNearlyZero(0.001f)
        || TorsoAxis.SizeSquared() <= 0.0001f)
    {
        Error = FString::Printf(
            TEXT("Arm spatial gate encountered a degenerate %s arm or torso"),
            *SideName);
        return false;
    }

    OutMetrics.ElbowAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(
        FMath::Clamp(FVector::DotProduct(Proximal / UpperLength,
            Distal / LowerLength), -1.0f, 1.0f)));
    OutMetrics.ReachRatio = ShoulderToHand.Size() / (UpperLength + LowerLength);
    const float TorsoAlpha = FMath::Clamp(FVector::DotProduct(Hand - Pelvis,
        TorsoAxis) / TorsoAxis.SizeSquared(), 0.0f, 1.0f);
    const FVector ClosestTorsoPoint = Pelvis + TorsoAxis * TorsoAlpha;
    OutMetrics.HandTorsoClearanceCm = FVector::Distance(
        Hand, ClosestTorsoPoint);
    const FVector RawHandTorsoLocal = ComponentPose[ChestIndex].GetRotation()
        .UnrotateVector(Hand - Chest);
    // DGMaster's torso frame is reported as lateral X, forward Z, vertical -Y.
    // Converting here makes the spatial contract explicit and independent of
    // the character's component/world placement.
    OutMetrics.HandTorsoLocalCm = FVector(
        RawHandTorsoLocal.X, -RawHandTorsoLocal.Y, RawHandTorsoLocal.Z);
    OutMetrics.DirectionTorsoLocal =
        OutMetrics.HandTorsoLocalCm.GetSafeNormal();
    OutMetrics.HandComponentLocation = Hand;
    return FMath::IsFinite(OutMetrics.ElbowAngleDegrees)
        && FMath::IsFinite(OutMetrics.ReachRatio)
        && FMath::IsFinite(OutMetrics.HandTorsoClearanceCm)
        && !OutMetrics.HandTorsoLocalCm.ContainsNaN()
        && !OutMetrics.HandTorsoLocalCm.IsNearlyZero(0.001f)
        && !OutMetrics.DirectionTorsoLocal.ContainsNaN()
        && !OutMetrics.HandComponentLocation.ContainsNaN();
}

void BuildV4AuthoredLocalRotations(const FRecipe& Recipe,
    const FFamilyRecipe& Family, const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& RefPose,
    TArray<TArray<FQuat>>& OutLocalRotations)
{
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefPose, RefComponentPose);
    OutLocalRotations.SetNum(Family.FrameCount + 1);
    for (int32 Frame = 0; Frame <= Family.FrameCount; ++Frame)
    {
        TArray<FTransform> TargetComponentPose = RefComponentPose;
        for (int32 BoneIndex = 0; BoneIndex < RefPose.Num(); ++BoneIndex)
        {
            const FQuat ComponentIntent = ComponentRotationIntentAtFrame(
                Recipe, Family, RefSkeleton, BoneIndex, Frame);
            TargetComponentPose[BoneIndex].SetRotation(
                (ComponentIntent
                    * RefComponentPose[BoneIndex].GetRotation()).GetNormalized());
        }
        TArray<FQuat>& LocalRotations = OutLocalRotations[Frame];
        LocalRotations.SetNum(RefPose.Num());
        for (int32 BoneIndex = 0; BoneIndex < RefPose.Num(); ++BoneIndex)
        {
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            LocalRotations[BoneIndex] = ParentIndex == INDEX_NONE
                ? TargetComponentPose[BoneIndex].GetRotation()
                : TargetComponentPose[BoneIndex]
                    .GetRelativeTransform(TargetComponentPose[ParentIndex])
                    .GetRotation().GetNormalized();
        }
    }
}

void BuildMixedSpaceAuthoredLocalRotations(const FRecipe& Recipe,
    const FFamilyRecipe& Family, const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& RefPose,
    TArray<TArray<FQuat>>& OutLocalRotations)
{
    // V5+ deliberately separates the stable axial component-space intent from
    // appendicular local-space deltas. This prevents torso turns from being
    // reinterpreted as arm/leg component targets while retaining the proven
    // component-to-local conversion for the seven-bone axial chain.
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefPose, RefComponentPose);
    OutLocalRotations.SetNum(Family.FrameCount + 1);
    for (int32 Frame = 0; Frame <= Family.FrameCount; ++Frame)
    {
        TArray<FTransform> LocalPose = RefPose;
        for (const FName Bone : Recipe.LocalRotationBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
            if (LocalPose.IsValidIndex(BoneIndex))
            {
                LocalPose[BoneIndex].SetRotation((
                    RefPose[BoneIndex].GetRotation()
                    * RotationAtFrame(Recipe, Family.Poses, Bone, Frame))
                    .GetNormalized());
            }
        }

        TArray<FTransform> ComponentPose;
        ComponentPose.SetNum(LocalPose.Num());
        for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
        {
            const FName Bone = RefSkeleton.GetBoneName(BoneIndex);
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            if (Recipe.ComponentRotationBones.Contains(Bone))
            {
                FTransform DesiredComponent = RefComponentPose[BoneIndex];
                DesiredComponent.SetRotation((
                    RotationAtFrame(Recipe, Family.Poses, Bone, Frame)
                    * RefComponentPose[BoneIndex].GetRotation()).GetNormalized());
                LocalPose[BoneIndex].SetRotation(
                    ParentIndex == INDEX_NONE
                        ? DesiredComponent.GetRotation()
                        : DesiredComponent
                            .GetRelativeTransform(ComponentPose[ParentIndex])
                            .GetRotation().GetNormalized());
            }
            ComponentPose[BoneIndex] = ParentIndex == INDEX_NONE
                ? LocalPose[BoneIndex]
                : LocalPose[BoneIndex] * ComponentPose[ParentIndex];
        }

        TArray<FQuat>& LocalRotations = OutLocalRotations[Frame];
        LocalRotations.SetNum(LocalPose.Num());
        for (int32 BoneIndex = 0; BoneIndex < LocalPose.Num(); ++BoneIndex)
        {
            LocalRotations[BoneIndex] = LocalPose[BoneIndex]
                .GetRotation().GetNormalized();
        }
    }
}

bool RefuseTargetCollision(const FString& ObjectPath, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    const FString Filename = FPackageName::LongPackageNameToFilename(
        PackageName, FPackageName::GetAssetPackageExtension());
    if (StaticFindObject(UObject::StaticClass(), nullptr, *ObjectPath)
        || IFileManager::Get().FileExists(*Filename))
    {
        Error = FString::Printf(TEXT("Refusing existing production-motion target: %s"),
            *ObjectPath);
        return false;
    }
    return true;
}

bool SaveNewAsset(UObject* Asset, TArray<FString>& CreatedFiles, FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Attempted to save a null production-motion asset");
        return false;
    }
    UPackage* Package = Asset->GetOutermost();
    const FString Filename = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    Args.Error = GError;
    if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
    {
        Error = FString::Printf(TEXT("Failed to save %s"), *Asset->GetPathName());
        return false;
    }
    CreatedFiles.Add(Filename);
    return true;
}

void SetCandidateMetadata(UObject* Asset, const FRecipe& Recipe,
    const FString& MotionId, const TCHAR* AssetRole)
{
    FMetaData& MetaData = Asset->GetOutermost()->GetMetaData();
    MetaData.SetValue(Asset, TEXT("DG_SourceKind"),
        TEXT("PROJECT_AUTHORED_PROCEDURAL"));
    MetaData.SetValue(Asset, TEXT("DG_RecipeId"), *Recipe.Version.RecipeId);
    MetaData.SetValue(Asset, TEXT("DG_RecipePath"),
        *Recipe.Version.RecipeRelativePath);
    MetaData.SetValue(Asset, TEXT("DG_RecipeMd5"), *Recipe.SourceTextMd5);
    MetaData.SetValue(Asset, TEXT("DG_MotionId"), *MotionId);
    MetaData.SetValue(Asset, TEXT("DG_AssetRole"), AssetRole);
    MetaData.SetValue(Asset, TEXT("DG_UsageStatus"),
        TEXT("PRODUCTION_CANDIDATE_HUMAN_REVIEW_REQUIRED"));
    MetaData.SetValue(Asset, TEXT("DG_DerivedFromSyntheticFixture"), TEXT("false"));
    MetaData.SetValue(Asset, TEXT("DG_ExternalPerformance"), TEXT("false"));
    MetaData.SetValue(Asset, TEXT("DG_HumanAnimationApproval"), TEXT("false"));
    MetaData.SetValue(Asset, TEXT("DG_HumanDiscContactApproval"), TEXT("false"));
    if (Recipe.Version.SchemaVersion >= 2)
    {
        MetaData.SetValue(Asset, TEXT("DG_RecipeVersion"),
            *Recipe.Version.Version);
        MetaData.SetValue(Asset, TEXT("DG_AssetRevision"),
            *Recipe.Version.AssetRevision);
        MetaData.SetValue(Asset, TEXT("DG_WorldMotionAuthority"),
            TEXT("GAMEPLAY_PAWN_CAPSULE"));
        MetaData.SetValue(Asset, TEXT("DG_PresentationRootTrajectory"),
            TEXT("true"));
    }
    if (Recipe.Version.SchemaVersion >= 3)
    {
        MetaData.SetValue(Asset, TEXT("DG_BiomechanicalModel"),
            *Recipe.BiomechanicalModel);
        MetaData.SetValue(Asset, TEXT("DG_DiscContactModel"),
            *Recipe.DiscContactModel);
        MetaData.SetValue(Asset, TEXT("DG_RecoveryModel"),
            *Recipe.RecoveryModel);
        MetaData.SetValue(Asset, TEXT("DG_RootMotionEnabled"), TEXT("false"));
    }
    if (Recipe.Version.SchemaVersion >= 4)
    {
        MetaData.SetValue(Asset, TEXT("DG_RotationSpace"),
            *Recipe.RotationSpace);
        MetaData.SetValue(Asset, TEXT("DG_LocalTrackConstruction"),
            Recipe.Version.SchemaVersion >= 5
                ? TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS")
                : TEXT("COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"));
    }
    if (Recipe.Version.SchemaVersion >= 5)
    {
        MetaData.SetValue(Asset, TEXT("DG_RootTrackPolicy"),
            *Recipe.RootTrackPolicy);
        MetaData.SetValue(Asset, TEXT("DG_ComponentRotationBoneCount"), TEXT("7"));
        MetaData.SetValue(Asset, TEXT("DG_LocalRotationBoneCount"), TEXT("31"));
    }
}

UAnimSequence* CreateSequence(const FFamilyRecipe& Family, USkeleton* Skeleton,
    USkeletalMesh* Mesh, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(Family.SequencePath);
    const FString AssetName = FPackageName::ObjectPathToObjectName(Family.SequencePath);
    UPackage* Package = CreatePackage(*PackageName);
    UAnimSequenceFactory* Factory = NewObject<UAnimSequenceFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->PreviewSkeletalMesh = Mesh;
    UAnimSequence* Sequence = Cast<UAnimSequence>(Factory->FactoryCreateNew(
        UAnimSequence::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!Sequence)
    {
        Error = FString::Printf(TEXT("AnimSequenceFactory failed for %s"),
            *Family.SequencePath);
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Sequence);
    return Sequence;
}

bool AuthorSequence(const FRecipe& Recipe, const FFamilyRecipe& Family,
    USkeleton* Skeleton, USkeletalMesh* Mesh, UAnimSequence*& OutSequence,
    TArray<FString>& CreatedFiles, FString& Error)
{
    OutSequence = CreateSequence(Family, Skeleton, Mesh, Error);
    if (!OutSequence) return false;
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
    TArray<TArray<FQuat>> AuthoredLocalRotations;
    if (Recipe.Version.SchemaVersion == 4)
    {
        BuildV4AuthoredLocalRotations(Recipe, Family, RefSkeleton, RefPose,
            AuthoredLocalRotations);
    }
    else if (Recipe.Version.SchemaVersion >= 5)
    {
        BuildMixedSpaceAuthoredLocalRotations(Recipe, Family, RefSkeleton, RefPose,
            AuthoredLocalRotations);
    }
    IAnimationDataController& Controller = OutSequence->GetController();
    Controller.OpenBracket(NSLOCTEXT("DiscGolfProductionMotion",
        "AuthorCandidate", "Author project procedural motion candidate"), false);
    Controller.RemoveAllBoneTracks(false);
    Controller.RemoveAllCurvesOfType(ERawCurveTrackTypes::RCT_Float, false);
    Controller.SetFrameRate(FFrameRate(FrameRate, 1), false);
    Controller.SetNumberOfFrames(FFrameNumber(Family.FrameCount), false);
    for (const FName Bone : Recipe.AnimatedBones)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
        if (!RefPose.IsValidIndex(BoneIndex) || !Controller.AddBoneCurve(Bone, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("DGMaster lacks authoring bone %s"),
                *Bone.ToString());
            return false;
        }
        const FTransform& Reference = RefPose[BoneIndex];
        TArray<FVector3f> Positions;
        TArray<FQuat4f> Rotations;
        TArray<FVector3f> Scales;
        Positions.Reserve(Family.FrameCount + 1);
        Rotations.Reserve(Family.FrameCount + 1);
        Scales.Reserve(Family.FrameCount + 1);
        for (int32 Frame = 0; Frame <= Family.FrameCount; ++Frame)
        {
            const FVector Position = Reference.GetTranslation()
                + TranslationAtFrame(Recipe, Family.Poses, Bone, Frame);
            const FQuat Rotation = Recipe.Version.SchemaVersion >= 4
                ? AuthoredLocalRotations[Frame][BoneIndex]
                : (Reference.GetRotation()
                    * RotationAtFrame(Recipe, Family.Poses, Bone, Frame))
                    .GetNormalized();
            const FVector Scale = Reference.GetScale3D();
            Positions.Emplace(static_cast<float>(Position.X),
                static_cast<float>(Position.Y), static_cast<float>(Position.Z));
            Rotations.Emplace(static_cast<float>(Rotation.X),
                static_cast<float>(Rotation.Y), static_cast<float>(Rotation.Z),
                static_cast<float>(Rotation.W));
            Scales.Emplace(static_cast<float>(Scale.X),
                static_cast<float>(Scale.Y), static_cast<float>(Scale.Z));
        }
        if (!Controller.SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not write track %s"), *Bone.ToString());
            return false;
        }
    }
    for (const FCurveSpec& Curve : Family.Curves)
    {
        const FAnimationCurveIdentifier Id(Curve.Name, ERawCurveTrackTypes::RCT_Float);
        if (!Controller.AddCurve(Id, AACF_DefaultCurve, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not add curve %s"),
                *Curve.Name.ToString());
            return false;
        }
        TArray<FRichCurveKey> Keys;
        for (const TPair<int32, float>& Pair : Curve.Keys)
        {
            FRichCurveKey& Key = Keys.Emplace_GetRef(FrameToTime(Pair.Key), Pair.Value);
            // V3 alpha envelopes are gameplay/presentation semantics rather than
            // free-form splines. Linear interpolation guarantees the authored
            // [0,1] contract continuously; auto cubic tangents can overshoot
            // even when every authored key is bounded.
            Key.InterpMode = Recipe.Version.SchemaVersion >= 3
                ? RCIM_Linear : RCIM_Cubic;
            Key.TangentMode = RCTM_Auto;
        }
        if (!Controller.SetCurveKeys(Id, Keys, false))
        {
            Controller.CloseBracket(false);
            Error = FString::Printf(TEXT("Could not write curve %s"),
                *Curve.Name.ToString());
            return false;
        }
    }
    Controller.CloseBracket(false);
    OutSequence->bLoop = false;
    OutSequence->RateScale = 1.0f;
    OutSequence->bEnableRootMotion = false;
    OutSequence->RootMotionRootLock = ERootMotionRootLock::RefPose;
    OutSequence->SetPreviewMesh(Mesh);
    SetCandidateMetadata(OutSequence, Recipe, Family.MotionId, TEXT("ANIMATION_SEQUENCE"));
    OutSequence->PostEditChange();
    return SaveNewAsset(OutSequence, CreatedFiles, Error);
}

bool SetPhaseProperty(UAnimNotify* Notify, const FString& PhaseName, FString& Error)
{
    FEnumProperty* Property = FindFProperty<FEnumProperty>(Notify->GetClass(), TEXT("Phase"));
    if (!Property || !Property->GetEnum())
    {
        Error = TEXT("Throw phase notify has no reflected Phase property");
        return false;
    }
    int64 Value = Property->GetEnum()->GetValueByNameString(PhaseName);
    if (Value == INDEX_NONE)
    {
        Value = Property->GetEnum()->GetValueByNameString(
            FString::Printf(TEXT("EDGThrowPhase::%s"), *PhaseName));
    }
    if (Value == INDEX_NONE)
    {
        Error = FString::Printf(TEXT("Unknown throw phase %s"), *PhaseName);
        return false;
    }
    Property->GetUnderlyingProperty()->SetIntPropertyValue(
        Property->ContainerPtrToValuePtr<void>(Notify), Value);
    return true;
}

bool PhasePropertyMatches(const UAnimNotify* Notify, const FString& PhaseName)
{
    if (!Notify)
    {
        return false;
    }
    const FEnumProperty* Property = FindFProperty<FEnumProperty>(
        Notify->GetClass(), TEXT("Phase"));
    if (!Property || !Property->GetEnum())
    {
        return false;
    }
    int64 Expected = Property->GetEnum()->GetValueByNameString(PhaseName);
    if (Expected == INDEX_NONE)
    {
        Expected = Property->GetEnum()->GetValueByNameString(
            FString::Printf(TEXT("EDGThrowPhase::%s"), *PhaseName));
    }
    return Expected != INDEX_NONE
        && Property->GetUnderlyingProperty()->GetSignedIntPropertyValue(
            Property->ContainerPtrToValuePtr<void>(Notify)) == Expected;
}

bool AddNotify(UAnimMontage* Montage, UClass* NotifyClass, float Time,
    int32 TrackIndex, const FGuid& Guid, const FString* Phase,
    bool bBranchingPoint, FString& Error)
{
    UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, NotifyClass,
        NAME_None, RF_Transactional);
    if (!Notify || (Phase && !SetPhaseProperty(Notify, *Phase, Error)))
    {
        if (Error.IsEmpty()) Error = TEXT("Could not instantiate production-motion notify");
        return false;
    }
    FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
    Event.Notify = Notify;
    Event.NotifyName = FName(*Notify->GetNotifyName());
    Event.TrackIndex = TrackIndex;
    Event.Guid = Guid;
    Event.MontageTickType = bBranchingPoint
        ? EMontageNotifyTickType::BranchingPoint
        : EMontageNotifyTickType::Queued;
    Event.Link(Montage, Time, 0);
    return true;
}

UAnimMontage* CreateMontage(const FFamilyRecipe& Family, UAnimSequence* Sequence,
    USkeleton* Skeleton, USkeletalMesh* Mesh, FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(Family.MontagePath);
    const FString AssetName = FPackageName::ObjectPathToObjectName(Family.MontagePath);
    UPackage* Package = CreatePackage(*PackageName);
    UAnimMontageFactory* Factory = NewObject<UAnimMontageFactory>();
    Factory->TargetSkeleton = Skeleton;
    Factory->SourceAnimation = Sequence;
    Factory->PreviewSkeletalMesh = Mesh;
    UAnimMontage* Montage = Cast<UAnimMontage>(Factory->FactoryCreateNew(
        UAnimMontage::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!Montage)
    {
        Error = FString::Printf(TEXT("AnimMontageFactory failed for %s"),
            *Family.MontagePath);
        return nullptr;
    }
    FAssetRegistryModule::AssetCreated(Montage);
    return Montage;
}

bool AuthorMontage(const FRecipe& Recipe, const FFamilyRecipe& Family,
    int32 FamilyIndex, UAnimSequence* Sequence, USkeleton* Skeleton,
    USkeletalMesh* Mesh, UAnimMontage*& OutMontage,
    TArray<FString>& CreatedFiles, FString& Error)
{
    OutMontage = CreateMontage(Family, Sequence, Skeleton, Mesh, Error);
    if (!OutMontage) return false;
    OutMontage->SlotAnimTracks.Reset();
    FSlotAnimationTrack& Slot = OutMontage->SlotAnimTracks.AddDefaulted_GetRef();
    Slot.SlotName = DefaultSlot;
    FAnimSegment& Segment = Slot.AnimTrack.AnimSegments.AddDefaulted_GetRef();
    Segment.SetAnimReference(Sequence, true);
    Segment.StartPos = 0.0f;
    Segment.AnimStartTime = 0.0f;
    Segment.AnimEndTime = Family.DurationSeconds;
    Segment.AnimPlayRate = 1.0f;
    Segment.LoopingCount = 1;
    OutMontage->SetCompositeLength(Family.DurationSeconds);
    OutMontage->CompositeSections.Reset();
    FCompositeSection& Section = OutMontage->CompositeSections.AddDefaulted_GetRef();
    Section.SectionName = TEXT("Default");
    Section.Link(OutMontage, 0.0f, 0);
    OutMontage->Notifies.Reset();
    OutMontage->AnimNotifyTracks.Reset();
    FAnimNotifyTrack& PhaseTrack = OutMontage->AnimNotifyTracks.AddDefaulted_GetRef();
    PhaseTrack.TrackName = TEXT("DG Phases");
    PhaseTrack.TrackColor = FLinearColor(0.12f, 0.55f, 0.90f);
    FAnimNotifyTrack& EventTrack = OutMontage->AnimNotifyTracks.AddDefaulted_GetRef();
    EventTrack.TrackName = TEXT("DG Events");
    EventTrack.TrackColor = FLinearColor(0.95f, 0.42f, 0.10f);
    UClass* PhaseClass = LoadObject<UClass>(nullptr, PhaseNotifyClassPath);
    UClass* ReleaseClass = LoadObject<UClass>(nullptr, ReleaseNotifyClassPath);
    UClass* FinishClass = LoadObject<UClass>(nullptr, FinishNotifyClassPath);
    if (!PhaseClass || !ReleaseClass || !FinishClass)
    {
        Error = TEXT("RuntimeFoundation production-motion notify classes did not load");
        return false;
    }
    int32 Ordinal = 1;
    for (const FPhaseSpec& Phase : Family.Phases)
    {
        const FGuid Guid(0xD6190000u + FamilyIndex * 0x100u + Ordinal,
            0xA11E0019u, 0xC0000000u + FamilyIndex * 0x100u + Ordinal,
            0x00000019u);
        if (!AddNotify(OutMontage, PhaseClass, FrameToTime(Phase.Frame), 0,
            Guid, &Phase.Name, false, Error))
        {
            return false;
        }
        ++Ordinal;
    }
    if (!AddNotify(OutMontage, ReleaseClass, FrameToTime(Family.ReleaseFrame), 1,
            FGuid(0xD61900F0u + FamilyIndex, 0xA11E0019u,
                0xC00000F0u + FamilyIndex, 0x00000019u), nullptr, true, Error)
        || !AddNotify(OutMontage, FinishClass, FrameToTime(Family.FinishFrame), 1,
            FGuid(0xD61900FFu + FamilyIndex, 0xA11E0019u,
                0xC00000FFu + FamilyIndex, 0x00000019u), nullptr, true, Error))
    {
        return false;
    }
    OutMontage->SortNotifies();
    OutMontage->RefreshCacheData();
    OutMontage->SetPreviewMesh(Mesh);
    OutMontage->bEnableAutoBlendOut = true;
    if (Recipe.Version.SchemaVersion >= 6)
    {
        OutMontage->BlendOutTriggerTime =
            ProductionMontageBlendOutTriggerTimeSeconds;
    }
    SetCandidateMetadata(OutMontage, Recipe, Family.MotionId, TEXT("ANIMATION_MONTAGE"));
    OutMontage->PostEditChange();
    return SaveNewAsset(OutMontage, CreatedFiles, Error);
}

bool AuthorLibrary(const FRecipe& Recipe, const TArray<UAnimMontage*>& Montages,
    UDiscGolfAnimationLibrary*& OutLibrary, TArray<FString>& CreatedFiles,
    FString& Error)
{
    const FString PackageName = FPackageName::ObjectPathToPackageName(
        Recipe.Version.LibraryPath);
    const FString AssetName = FPackageName::ObjectPathToObjectName(
        Recipe.Version.LibraryPath);
    UPackage* Package = CreatePackage(*PackageName);
    UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
    Factory->DataAssetClass = UDiscGolfAnimationLibrary::StaticClass();
    OutLibrary = Cast<UDiscGolfAnimationLibrary>(Factory->FactoryCreateNew(
        UDiscGolfAnimationLibrary::StaticClass(), Package, FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
    if (!OutLibrary)
    {
        Error = TEXT("DataAssetFactory failed for production motion library");
        return false;
    }
    FAssetRegistryModule::AssetCreated(OutLibrary);
    OutLibrary->Entries.Reset();
    for (int32 Index = 0; Index < Recipe.Families.Num(); ++Index)
    {
        FDGThrowAnimationEntry& Entry = OutLibrary->Entries.AddDefaulted_GetRef();
        Entry.ThrowType = EDGThrowType::Backhand;
        Entry.Handedness = EDGHandedness::Right;
        Entry.MotionFamilyId = FName(*Recipe.Families[Index].Family);
        Entry.StyleId = Recipe.Families[Index].StyleId;
        Entry.Montage = Montages[Index];
        Entry.RecommendedPowerMin = Recipe.Families[Index].PowerMin;
        Entry.RecommendedPowerMax = Recipe.Families[Index].PowerMax;
    }
    SetCandidateMetadata(OutLibrary, Recipe,
        Recipe.Version.LibraryMotionId,
        TEXT("ANIMATION_LIBRARY"));
    OutLibrary->PostEditChange();
    return SaveNewAsset(OutLibrary, CreatedFiles, Error);
}

bool ValidateMetadata(UObject* Asset, const FRecipe& Recipe,
    const FString& MotionId, FString& Error)
{
    if (!Asset)
    {
        Error = TEXT("Production-motion asset is null");
        return false;
    }
    FMetaData& MetaData = Asset->GetOutermost()->GetMetaData();
    TMap<FString, FString> Expected = {
        {TEXT("DG_SourceKind"), TEXT("PROJECT_AUTHORED_PROCEDURAL")},
        {TEXT("DG_RecipeId"), Recipe.Version.RecipeId},
        {TEXT("DG_RecipePath"), Recipe.Version.RecipeRelativePath},
        {TEXT("DG_RecipeMd5"), Recipe.SourceTextMd5},
        {TEXT("DG_MotionId"), MotionId},
        {TEXT("DG_UsageStatus"), TEXT("PRODUCTION_CANDIDATE_HUMAN_REVIEW_REQUIRED")},
        {TEXT("DG_DerivedFromSyntheticFixture"), TEXT("false")},
        {TEXT("DG_ExternalPerformance"), TEXT("false")},
        {TEXT("DG_HumanAnimationApproval"), TEXT("false")},
        {TEXT("DG_HumanDiscContactApproval"), TEXT("false")}
    };
    if (Recipe.Version.SchemaVersion >= 2)
    {
        Expected.Add(TEXT("DG_RecipeVersion"), Recipe.Version.Version);
        Expected.Add(TEXT("DG_AssetRevision"), Recipe.Version.AssetRevision);
        Expected.Add(TEXT("DG_WorldMotionAuthority"),
            TEXT("GAMEPLAY_PAWN_CAPSULE"));
        Expected.Add(TEXT("DG_PresentationRootTrajectory"), TEXT("true"));
    }
    if (Recipe.Version.SchemaVersion >= 3)
    {
        Expected.Add(TEXT("DG_BiomechanicalModel"), Recipe.BiomechanicalModel);
        Expected.Add(TEXT("DG_DiscContactModel"), Recipe.DiscContactModel);
        Expected.Add(TEXT("DG_RecoveryModel"), Recipe.RecoveryModel);
        Expected.Add(TEXT("DG_RootMotionEnabled"), TEXT("false"));
    }
    if (Recipe.Version.SchemaVersion >= 4)
    {
        Expected.Add(TEXT("DG_RotationSpace"), Recipe.RotationSpace);
        Expected.Add(TEXT("DG_LocalTrackConstruction"),
            Recipe.Version.SchemaVersion >= 5
                ? TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS")
                : TEXT("COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"));
    }
    if (Recipe.Version.SchemaVersion >= 5)
    {
        Expected.Add(TEXT("DG_RootTrackPolicy"), Recipe.RootTrackPolicy);
        Expected.Add(TEXT("DG_ComponentRotationBoneCount"), TEXT("7"));
        Expected.Add(TEXT("DG_LocalRotationBoneCount"), TEXT("31"));
    }
    for (const TPair<FString, FString>& Pair : Expected)
    {
        if (MetaData.GetValue(Asset, *Pair.Key) != Pair.Value)
        {
            Error = FString::Printf(TEXT("Metadata %s differs on %s"),
                *Pair.Key, *Asset->GetPathName());
            return false;
        }
    }
    return true;
}

bool ValidateV4ComponentSpaceTracks(const FRecipe& Recipe,
    const FFamilyRecipe& Family, const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& RefPose,
    const TMap<FName, TArray<FTransform>>& TrackTransformsByName,
    FV4ComponentSpaceValidationMetrics& OutMetrics, FString& Error)
{
    constexpr float MinimumSegmentRatio = 0.998f;
    constexpr float MaximumSegmentRatio = 1.002f;
    constexpr float MaximumIntentErrorDegrees = 0.05f;
    constexpr float MaximumComponentDeltaDegrees = 95.0f;
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefPose, RefComponentPose);
    float ObservedMinimumSegmentRatio = TNumericLimits<float>::Max();
    float ObservedMaximumSegmentRatio = 0.0f;

    for (int32 Frame = 0; Frame <= Family.FrameCount; ++Frame)
    {
        TArray<FTransform> LocalPose = RefPose;
        for (const FName Bone : Recipe.AnimatedBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
            const TArray<FTransform>* Transforms = TrackTransformsByName.Find(Bone);
            if (!LocalPose.IsValidIndex(BoneIndex) || !Transforms
                || !Transforms->IsValidIndex(Frame))
            {
                Error = FString::Printf(
                    TEXT("Sequence %s v4 component reconstruction lacks %s frame %d"),
                    *Family.Family, *Bone.ToString(), Frame);
                return false;
            }
            LocalPose[BoneIndex] = (*Transforms)[Frame];
        }
        TArray<FTransform> ComponentPose;
        BuildComponentPose(RefSkeleton, LocalPose, ComponentPose);
        for (int32 BoneIndex = 0; BoneIndex < ComponentPose.Num(); ++BoneIndex)
        {
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            if (ParentIndex != INDEX_NONE)
            {
                const float ReferenceLength = FVector::Distance(
                    RefComponentPose[BoneIndex].GetTranslation(),
                    RefComponentPose[ParentIndex].GetTranslation());
                if (ReferenceLength > 0.01f)
                {
                    const float PosedLength = FVector::Distance(
                        ComponentPose[BoneIndex].GetTranslation(),
                        ComponentPose[ParentIndex].GetTranslation());
                    const float Ratio = PosedLength / ReferenceLength;
                    ObservedMinimumSegmentRatio = FMath::Min(
                        ObservedMinimumSegmentRatio, Ratio);
                    ObservedMaximumSegmentRatio = FMath::Max(
                        ObservedMaximumSegmentRatio, Ratio);
                }
            }
            const FQuat ExpectedComponentRotation = (
                ComponentRotationIntentAtFrame(
                    Recipe, Family, RefSkeleton, BoneIndex, Frame)
                * RefComponentPose[BoneIndex].GetRotation()).GetNormalized();
            const float IntentErrorDegrees = FMath::RadiansToDegrees(
                static_cast<float>(ComponentPose[BoneIndex].GetRotation()
                    .AngularDistance(ExpectedComponentRotation)));
            const float ComponentDeltaDegrees = FMath::RadiansToDegrees(
                static_cast<float>(ComponentPose[BoneIndex].GetRotation()
                    .AngularDistance(
                        RefComponentPose[BoneIndex].GetRotation())));
            OutMetrics.MaximumComponentIntentErrorDegrees = FMath::Max(
                OutMetrics.MaximumComponentIntentErrorDegrees,
                IntentErrorDegrees);
            OutMetrics.MaximumComponentDeltaDegrees = FMath::Max(
                OutMetrics.MaximumComponentDeltaDegrees,
                ComponentDeltaDegrees);
        }
        ++OutMetrics.SampledFrameCount;
    }
    if (ObservedMinimumSegmentRatio == TNumericLimits<float>::Max())
    {
        Error = FString::Printf(
            TEXT("Sequence %s v4 component chain has no measurable segments"),
            *Family.Family);
        return false;
    }
    OutMetrics.MinimumJointSegmentRatio = ObservedMinimumSegmentRatio;
    OutMetrics.MaximumJointSegmentRatio = ObservedMaximumSegmentRatio;
    if (ObservedMinimumSegmentRatio < MinimumSegmentRatio
        || ObservedMaximumSegmentRatio > MaximumSegmentRatio)
    {
        Error = FString::Printf(
            TEXT("Sequence %s v4 component chain changes bone lengths: %.6f..%.6f"),
            *Family.Family, ObservedMinimumSegmentRatio,
            ObservedMaximumSegmentRatio);
        return false;
    }
    if (OutMetrics.MaximumComponentIntentErrorDegrees
            > MaximumIntentErrorDegrees
        || OutMetrics.MaximumComponentDeltaDegrees
            > MaximumComponentDeltaDegrees)
    {
        Error = FString::Printf(
            TEXT("Sequence %s v4 component angle gate failed: intent error %.6f, delta %.3f"),
            *Family.Family, OutMetrics.MaximumComponentIntentErrorDegrees,
            OutMetrics.MaximumComponentDeltaDegrees);
        return false;
    }
    return true;
}

bool ValidateMixedSpaceTracks(const FRecipe& Recipe,
    const FFamilyRecipe& Family, const FReferenceSkeleton& RefSkeleton,
    const TArray<FTransform>& RefPose,
    const TMap<FName, TArray<FTransform>>& TrackTransformsByName,
    FV4ComponentSpaceValidationMetrics& OutMetrics, FString& Error)
{
    constexpr float MinimumSegmentRatio = 0.998f;
    constexpr float MaximumSegmentRatio = 1.002f;
    constexpr float MaximumIntentErrorDegrees = 0.05f;
    constexpr float MaximumRootRotationErrorDegrees = 0.01f;
    constexpr float MaximumAxialComponentDeltaDegrees = 95.0f;
    constexpr float MaximumMajorJointFrameDeltaDegrees = 15.0f;
    // V007's deliberately shaped release extends the throwing forearm faster
    // than the legacy all-joint style cap while remaining inside the fixed
    // source-continuity contract (24 degrees per 60 Hz frame). Keep this
    // exception isolated to the dense v007 Drive lowerarm track; v006 and all
    // other joints retain the existing 15-degree gate.
    constexpr float MaximumV7DriveThrowingLowerArmFrameDeltaDegrees = 24.0f;
    constexpr float MaximumFingerFrameDeltaDegrees = 20.0f;
    TArray<FTransform> RefComponentPose;
    BuildComponentPose(RefSkeleton, RefPose, RefComponentPose);
    const int32 RootIndex = RefSkeleton.FindBoneIndex(TEXT("root"));
    if (!RefPose.IsValidIndex(RootIndex))
    {
        Error = FString::Printf(
            TEXT("%s mixed-space validation lacks the root reference bone"),
            *Recipe.Version.Version);
        return false;
    }
    float ObservedMinimumSegmentRatio = TNumericLimits<float>::Max();
    float ObservedMaximumSegmentRatio = 0.0f;
    TSet<int32> NamedPhaseFrames;
    for (const FPhaseSpec& Phase : Family.Phases)
    {
        NamedPhaseFrames.Add(Phase.Frame);
    }
    NamedPhaseFrames.Add(Family.ReleaseFrame);
    FArmSpatialPoseMetrics ReachBackArm;
    FArmSpatialPoseMetrics ReleaseArm;
    FArmSpatialPoseMetrics FollowThroughArm;
    const bool bV7DriveArmGate = Recipe.Version.SchemaVersion == 7
        && Family.Family == TEXT("Drive");
    const TSet<int32> V7SupportFrames = {44, 64, 84, 118};
    const TSet<int32> V7ThrowingFrames = {79, 80, 81, 84, 94, 118};
    TMap<int32, FArmSpatialPoseMetrics> V7SupportArmByFrame;
    TMap<int32, FArmSpatialPoseMetrics> V7ThrowingArmByFrame;
    FVector PreviousThrowingHandDirection = FVector::ZeroVector;
    bool bHasPreviousThrowingHandDirection = false;

    for (int32 Frame = 0; Frame <= Family.FrameCount; ++Frame)
    {
        TArray<FTransform> LocalPose = RefPose;
        for (const FName Bone : Recipe.AnimatedBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
            const TArray<FTransform>* Transforms = TrackTransformsByName.Find(Bone);
            if (!LocalPose.IsValidIndex(BoneIndex) || !Transforms
                || !Transforms->IsValidIndex(Frame))
            {
                Error = FString::Printf(
                    TEXT("Sequence %s %s mixed-space reconstruction lacks %s frame %d"),
                    *Family.Family, *Recipe.Version.Version, *Bone.ToString(),
                    Frame);
                return false;
            }
            LocalPose[BoneIndex] = (*Transforms)[Frame];
        }
        TArray<FTransform> ComponentPose;
        BuildComponentPose(RefSkeleton, LocalPose, ComponentPose);
        FArmSpatialPoseMetrics CurrentArm;
        if (!MeasureArmSpatialPose(RefSkeleton, ComponentPose, TEXT("r"),
                CurrentArm, Error))
        {
            if (Error.IsEmpty())
            {
                Error = FString::Printf(
                    TEXT("Sequence %s %s has non-finite arm spatial evidence at frame %d"),
                    *Family.Family, *Recipe.Version.Version, Frame);
            }
            return false;
        }
        if (bHasPreviousThrowingHandDirection)
        {
            OutMetrics.MaximumThrowingHandDirectionFrameDeltaDegrees =
                FMath::Max(
                    OutMetrics.MaximumThrowingHandDirectionFrameDeltaDegrees,
                    DirectionDeltaDegrees(PreviousThrowingHandDirection,
                        CurrentArm.DirectionTorsoLocal));
        }
        PreviousThrowingHandDirection = CurrentArm.DirectionTorsoLocal;
        bHasPreviousThrowingHandDirection = true;
        if (bV7DriveArmGate && V7ThrowingFrames.Contains(Frame))
        {
            V7ThrowingArmByFrame.Add(Frame, CurrentArm);
        }
        if (bV7DriveArmGate && V7SupportFrames.Contains(Frame))
        {
            FArmSpatialPoseMetrics SupportArm;
            if (!MeasureArmSpatialPose(RefSkeleton, ComponentPose, TEXT("l"),
                    SupportArm, Error))
            {
                return false;
            }
            V7SupportArmByFrame.Add(Frame, SupportArm);
        }
        if (Frame == Family.Phases[2].Frame
            || Frame == Family.ReleaseFrame
            || Frame == Family.Phases[5].Frame)
        {
            FArmSpatialPoseMetrics* PhaseMetrics =
                Frame == Family.Phases[2].Frame ? &ReachBackArm
                : Frame == Family.ReleaseFrame ? &ReleaseArm
                : &FollowThroughArm;
            *PhaseMetrics = CurrentArm;
            ++OutMetrics.ArmSpatialSampleCount;
        }
        for (int32 BoneIndex = 0; BoneIndex < ComponentPose.Num(); ++BoneIndex)
        {
            const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
            if (ParentIndex != INDEX_NONE)
            {
                const float ReferenceLength = FVector::Distance(
                    RefComponentPose[BoneIndex].GetTranslation(),
                    RefComponentPose[ParentIndex].GetTranslation());
                if (ReferenceLength > 0.01f)
                {
                    const float Ratio = FVector::Distance(
                        ComponentPose[BoneIndex].GetTranslation(),
                        ComponentPose[ParentIndex].GetTranslation()) / ReferenceLength;
                    ObservedMinimumSegmentRatio = FMath::Min(
                        ObservedMinimumSegmentRatio, Ratio);
                    ObservedMaximumSegmentRatio = FMath::Max(
                        ObservedMaximumSegmentRatio, Ratio);
                }
            }
        }

        if (NamedPhaseFrames.Contains(Frame))
        {
            for (const TTuple<FName, FName, FName>& KneeChain : {
                MakeTuple(FName(TEXT("thigh_l")), FName(TEXT("calf_l")),
                    FName(TEXT("foot_l"))),
                MakeTuple(FName(TEXT("thigh_r")), FName(TEXT("calf_r")),
                    FName(TEXT("foot_r")))})
            {
                const int32 ThighIndex = RefSkeleton.FindBoneIndex(
                    KneeChain.Get<0>());
                const int32 CalfIndex = RefSkeleton.FindBoneIndex(
                    KneeChain.Get<1>());
                const int32 FootIndex = RefSkeleton.FindBoneIndex(
                    KneeChain.Get<2>());
                if (!ComponentPose.IsValidIndex(ThighIndex)
                    || !ComponentPose.IsValidIndex(CalfIndex)
                    || !ComponentPose.IsValidIndex(FootIndex))
                {
                    Error = FString::Printf(
                        TEXT("Sequence %s %s knee chain is incomplete at frame %d"),
                        *Family.Family, *Recipe.Version.Version, Frame);
                    return false;
                }
                const FVector Proximal =
                    ComponentPose[ThighIndex].GetTranslation()
                    - ComponentPose[CalfIndex].GetTranslation();
                const FVector Distal = ComponentPose[FootIndex].GetTranslation()
                    - ComponentPose[CalfIndex].GetTranslation();
                if (Proximal.IsNearlyZero(0.001f) || Distal.IsNearlyZero(0.001f))
                {
                    Error = FString::Printf(
                        TEXT("Sequence %s %s knee chain is degenerate at frame %d"),
                        *Family.Family, *Recipe.Version.Version, Frame);
                    return false;
                }
                const float KneeAngleDegrees = FMath::RadiansToDegrees(
                    FMath::Acos(FMath::Clamp(
                        FVector::DotProduct(Proximal.GetSafeNormal(),
                            Distal.GetSafeNormal()), -1.0f, 1.0f)));
                ++OutMetrics.NamedPhaseKneeSampleCount;
                OutMetrics.MinimumNamedPhaseKneeAngleDegrees = FMath::Min(
                    OutMetrics.MinimumNamedPhaseKneeAngleDegrees,
                    KneeAngleDegrees);
                OutMetrics.MaximumNamedPhaseKneeAngleDegrees = FMath::Max(
                    OutMetrics.MaximumNamedPhaseKneeAngleDegrees,
                    KneeAngleDegrees);
            }
        }

        const float RootErrorDegrees = FMath::RadiansToDegrees(
            static_cast<float>(LocalPose[RootIndex].GetRotation().AngularDistance(
                RefPose[RootIndex].GetRotation())));
        OutMetrics.MaximumRootRotationErrorDegrees = FMath::Max(
            OutMetrics.MaximumRootRotationErrorDegrees, RootErrorDegrees);

        for (const FName Bone : Recipe.ComponentRotationBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
            const FQuat Expected = (
                RotationAtFrame(Recipe, Family.Poses, Bone, Frame)
                * RefComponentPose[BoneIndex].GetRotation()).GetNormalized();
            const float ErrorDegrees = FMath::RadiansToDegrees(
                static_cast<float>(ComponentPose[BoneIndex].GetRotation()
                    .AngularDistance(Expected)));
            const float DeltaDegrees = FMath::RadiansToDegrees(
                static_cast<float>(ComponentPose[BoneIndex].GetRotation()
                    .AngularDistance(RefComponentPose[BoneIndex].GetRotation())));
            OutMetrics.MaximumComponentIntentErrorDegrees = FMath::Max(
                OutMetrics.MaximumComponentIntentErrorDegrees, ErrorDegrees);
            OutMetrics.MaximumComponentDeltaDegrees = FMath::Max(
                OutMetrics.MaximumComponentDeltaDegrees, DeltaDegrees);
        }
        for (const FName Bone : Recipe.LocalRotationBones)
        {
            const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
            const FQuat Expected = (
                RefPose[BoneIndex].GetRotation()
                * RotationAtFrame(Recipe, Family.Poses, Bone, Frame))
                .GetNormalized();
            const float ErrorDegrees = FMath::RadiansToDegrees(
                static_cast<float>(LocalPose[BoneIndex].GetRotation()
                    .AngularDistance(Expected)));
            OutMetrics.MaximumLocalIntentErrorDegrees = FMath::Max(
                OutMetrics.MaximumLocalIntentErrorDegrees, ErrorDegrees);
            if (Frame > 0)
            {
                const TArray<FTransform>* Transforms =
                    TrackTransformsByName.Find(Bone);
                const float FrameDeltaDegrees = FMath::RadiansToDegrees(
                    static_cast<float>((*Transforms)[Frame].GetRotation()
                        .AngularDistance((*Transforms)[Frame - 1].GetRotation())));
                OutMetrics.MaximumLocalFrameDeltaDegrees = FMath::Max(
                    OutMetrics.MaximumLocalFrameDeltaDegrees, FrameDeltaDegrees);
                const FString BoneName = Bone.ToString();
                const bool bFinger = BoneName.StartsWith(TEXT("thumb_"))
                    || BoneName.StartsWith(TEXT("index_"))
                    || BoneName.StartsWith(TEXT("middle_"))
                    || BoneName.StartsWith(TEXT("ring_"))
                    || BoneName.StartsWith(TEXT("pinky_"));
                const bool bV7DriveThrowingLowerArm = bV7DriveArmGate
                    && Bone == FName(TEXT("lowerarm_r"));
                const float Limit = bFinger
                    ? MaximumFingerFrameDeltaDegrees
                    : bV7DriveThrowingLowerArm
                        ? MaximumV7DriveThrowingLowerArmFrameDeltaDegrees
                        : MaximumMajorJointFrameDeltaDegrees;
                if (bFinger)
                {
                    OutMetrics.MaximumFingerFrameDeltaDegrees = FMath::Max(
                        OutMetrics.MaximumFingerFrameDeltaDegrees,
                        FrameDeltaDegrees);
                }
                if (FrameDeltaDegrees > Limit)
                {
                    Error = FString::Printf(
                        TEXT("Sequence %s %s local joint %s pops %.3f degrees at frame %d (limit %.3f)"),
                        *Family.Family, *Recipe.Version.Version, *BoneName,
                        FrameDeltaDegrees, Frame, Limit);
                    return false;
                }
            }
        }
        ++OutMetrics.SampledFrameCount;
    }

    if (ObservedMinimumSegmentRatio == TNumericLimits<float>::Max())
    {
        Error = FString::Printf(
            TEXT("Sequence %s %s mixed-space chain has no measurable segments"),
            *Family.Family, *Recipe.Version.Version);
        return false;
    }
    OutMetrics.MinimumJointSegmentRatio = ObservedMinimumSegmentRatio;
    OutMetrics.MaximumJointSegmentRatio = ObservedMaximumSegmentRatio;
    OutMetrics.ReachBackThrowingElbowAngleDegrees =
        ReachBackArm.ElbowAngleDegrees;
    OutMetrics.ReleaseThrowingElbowAngleDegrees = ReleaseArm.ElbowAngleDegrees;
    OutMetrics.FollowThroughThrowingElbowAngleDegrees =
        FollowThroughArm.ElbowAngleDegrees;
    OutMetrics.ReachBackThrowingArmReachRatio = ReachBackArm.ReachRatio;
    OutMetrics.ReleaseThrowingArmReachRatio = ReleaseArm.ReachRatio;
    OutMetrics.FollowThroughThrowingArmReachRatio = FollowThroughArm.ReachRatio;
    OutMetrics.ReachBackThrowingHandTorsoClearanceCm =
        ReachBackArm.HandTorsoClearanceCm;
    OutMetrics.ReleaseThrowingHandTorsoClearanceCm =
        ReleaseArm.HandTorsoClearanceCm;
    OutMetrics.FollowThroughThrowingHandTorsoClearanceCm =
        FollowThroughArm.HandTorsoClearanceCm;
    OutMetrics.ReachBackThrowingArmDirectionTorsoLocal =
        ReachBackArm.DirectionTorsoLocal;
    OutMetrics.ReleaseThrowingArmDirectionTorsoLocal =
        ReleaseArm.DirectionTorsoLocal;
    OutMetrics.FollowThroughThrowingArmDirectionTorsoLocal =
        FollowThroughArm.DirectionTorsoLocal;
    OutMetrics.ReachBackThrowingHandTorsoLocalCm =
        ReachBackArm.HandTorsoLocalCm;
    OutMetrics.ReleaseThrowingHandTorsoLocalCm = ReleaseArm.HandTorsoLocalCm;
    OutMetrics.FollowThroughThrowingHandTorsoLocalCm =
        FollowThroughArm.HandTorsoLocalCm;
    OutMetrics.ReachBackToReleaseArmDirectionDeltaDegrees =
        DirectionDeltaDegrees(ReachBackArm.DirectionTorsoLocal,
            ReleaseArm.DirectionTorsoLocal);
    OutMetrics.ReleaseToFollowThroughArmDirectionDeltaDegrees =
        DirectionDeltaDegrees(ReleaseArm.DirectionTorsoLocal,
            FollowThroughArm.DirectionTorsoLocal);
    OutMetrics.ReachBackToFollowThroughArmDirectionDeltaDegrees =
        DirectionDeltaDegrees(ReachBackArm.DirectionTorsoLocal,
            FollowThroughArm.DirectionTorsoLocal);
    OutMetrics.ReachBackToFollowThroughHandTravelCm = FVector::Distance(
        ReachBackArm.HandComponentLocation,
        FollowThroughArm.HandComponentLocation);
    OutMetrics.ReachBackThrowingHandRadialReachCm = FMath::Sqrt(
        FMath::Square(ReachBackArm.HandTorsoLocalCm.X)
        + FMath::Square(ReachBackArm.HandTorsoLocalCm.Z));
    OutMetrics.MinimumThrowingHandVerticalCm = FMath::Min3(
        ReachBackArm.HandTorsoLocalCm.Y,
        ReleaseArm.HandTorsoLocalCm.Y,
        FollowThroughArm.HandTorsoLocalCm.Y);
    OutMetrics.MinimumReleaseFollowThrowingHandLateralClearanceCm = FMath::Min(
        FMath::Abs(ReleaseArm.HandTorsoLocalCm.X),
        FMath::Abs(FollowThroughArm.HandTorsoLocalCm.X));
    if (ObservedMinimumSegmentRatio < MinimumSegmentRatio
        || ObservedMaximumSegmentRatio > MaximumSegmentRatio
        || OutMetrics.MaximumComponentIntentErrorDegrees
            > MaximumIntentErrorDegrees
        || OutMetrics.MaximumLocalIntentErrorDegrees > MaximumIntentErrorDegrees
        || OutMetrics.MaximumRootRotationErrorDegrees
            > MaximumRootRotationErrorDegrees
        || OutMetrics.MaximumComponentDeltaDegrees
            > MaximumAxialComponentDeltaDegrees
        || OutMetrics.ArmSpatialSampleCount != 3
        || OutMetrics.NamedPhaseKneeSampleCount
            != (RequiredPhases().Num() + 1) * 2
        || OutMetrics.MinimumNamedPhaseKneeAngleDegrees < 130.0f
        || OutMetrics.MaximumNamedPhaseKneeAngleDegrees > 176.0f)
    {
        Error = FString::Printf(
            TEXT("Sequence %s %s mixed-space gate failed: segment %.6f..%.6f, component %.6f, local %.6f, root %.6f, axial delta %.3f, knees %d %.3f..%.3f"),
            *Family.Family, *Recipe.Version.Version,
            ObservedMinimumSegmentRatio,
            ObservedMaximumSegmentRatio,
            OutMetrics.MaximumComponentIntentErrorDegrees,
            OutMetrics.MaximumLocalIntentErrorDegrees,
            OutMetrics.MaximumRootRotationErrorDegrees,
            OutMetrics.MaximumComponentDeltaDegrees,
            OutMetrics.NamedPhaseKneeSampleCount,
            OutMetrics.MinimumNamedPhaseKneeAngleDegrees,
            OutMetrics.MaximumNamedPhaseKneeAngleDegrees);
        return false;
    }
    if (bV7DriveArmGate)
    {
        if (V7SupportArmByFrame.Num() != V7SupportFrames.Num()
            || V7ThrowingArmByFrame.Num() != V7ThrowingFrames.Num())
        {
            Error = FString::Printf(
                TEXT("Sequence %s v7 bilateral arm gate lacks exact samples: support=%d throwing=%d"),
                *Family.Family, V7SupportArmByFrame.Num(),
                V7ThrowingArmByFrame.Num());
            return false;
        }
        const FV7DriveArmGateThresholds& Thresholds =
            V7DriveArmThresholds();
        OutMetrics.bV7DriveArmGateApplied = true;
        OutMetrics.V7SupportArmSampleCount = V7SupportArmByFrame.Num();
        OutMetrics.V7SupportArmMinimumElbowAngleDegrees =
            TNumericLimits<float>::Max();
        OutMetrics.V7PowerPocketMinimumReachRatio =
            TNumericLimits<float>::Max();
        OutMetrics.V7PowerPocketMinimumElbowAngleDegrees =
            TNumericLimits<float>::Max();
        OutMetrics.V7PowerPocketMinimumTorsoForwardCm =
            TNumericLimits<float>::Max();
        OutMetrics.V7PowerPocketMaximumTorsoForwardCm =
            -TNumericLimits<float>::Max();
        OutMetrics.V7PowerPocketMinimumTorsoVerticalCm =
            TNumericLimits<float>::Max();
        for (const TPair<int32, FArmSpatialPoseMetrics>& Pair
            : V7SupportArmByFrame)
        {
            OutMetrics.V7SupportArmMaximumReachRatio = FMath::Max(
                OutMetrics.V7SupportArmMaximumReachRatio,
                Pair.Value.ReachRatio);
            OutMetrics.V7SupportArmMinimumElbowAngleDegrees = FMath::Min(
                OutMetrics.V7SupportArmMinimumElbowAngleDegrees,
                Pair.Value.ElbowAngleDegrees);
            OutMetrics.V7SupportArmMaximumElbowAngleDegrees = FMath::Max(
                OutMetrics.V7SupportArmMaximumElbowAngleDegrees,
                Pair.Value.ElbowAngleDegrees);
        }
        OutMetrics.V7PowerPocketSampleCount = 3;
        for (const int32 Frame : {79, 80, 81})
        {
            const FArmSpatialPoseMetrics& Value =
                V7ThrowingArmByFrame.FindChecked(Frame);
            OutMetrics.V7PowerPocketMinimumReachRatio = FMath::Min(
                OutMetrics.V7PowerPocketMinimumReachRatio, Value.ReachRatio);
            OutMetrics.V7PowerPocketMaximumReachRatio = FMath::Max(
                OutMetrics.V7PowerPocketMaximumReachRatio, Value.ReachRatio);
            OutMetrics.V7PowerPocketMinimumElbowAngleDegrees = FMath::Min(
                OutMetrics.V7PowerPocketMinimumElbowAngleDegrees,
                Value.ElbowAngleDegrees);
            OutMetrics.V7PowerPocketMaximumElbowAngleDegrees = FMath::Max(
                OutMetrics.V7PowerPocketMaximumElbowAngleDegrees,
                Value.ElbowAngleDegrees);
            OutMetrics.V7PowerPocketMaximumAbsoluteTorsoLateralCm = FMath::Max(
                OutMetrics.V7PowerPocketMaximumAbsoluteTorsoLateralCm,
                FMath::Abs(Value.HandTorsoLocalCm.X));
            OutMetrics.V7PowerPocketMinimumTorsoForwardCm = FMath::Min(
                OutMetrics.V7PowerPocketMinimumTorsoForwardCm,
                Value.HandTorsoLocalCm.Z);
            OutMetrics.V7PowerPocketMaximumTorsoForwardCm = FMath::Max(
                OutMetrics.V7PowerPocketMaximumTorsoForwardCm,
                Value.HandTorsoLocalCm.Z);
            OutMetrics.V7PowerPocketMinimumTorsoVerticalCm = FMath::Min(
                OutMetrics.V7PowerPocketMinimumTorsoVerticalCm,
                Value.HandTorsoLocalCm.Y);
            OutMetrics.V7PowerPocketMaximumTorsoVerticalCm = FMath::Max(
                OutMetrics.V7PowerPocketMaximumTorsoVerticalCm,
                Value.HandTorsoLocalCm.Y);
        }
        const FArmSpatialPoseMetrics& V7Release =
            V7ThrowingArmByFrame.FindChecked(84);
        const FArmSpatialPoseMetrics& V7FollowThrough =
            V7ThrowingArmByFrame.FindChecked(94);
        const FArmSpatialPoseMetrics& V7Recovery =
            V7ThrowingArmByFrame.FindChecked(118);
        OutMetrics.V7ReleaseReachRatio = V7Release.ReachRatio;
        OutMetrics.V7ReleaseElbowAngleDegrees = V7Release.ElbowAngleDegrees;
        OutMetrics.V7FollowThroughReachRatio = V7FollowThrough.ReachRatio;
        OutMetrics.V7FollowThroughElbowAngleDegrees =
            V7FollowThrough.ElbowAngleDegrees;
        OutMetrics.V7RecoveryReachRatio = V7Recovery.ReachRatio;
        OutMetrics.V7RecoveryElbowAngleDegrees = V7Recovery.ElbowAngleDegrees;
        OutMetrics.V7RecoveryAbsoluteTorsoLateralCm =
            FMath::Abs(V7Recovery.HandTorsoLocalCm.X);
        OutMetrics.V7RecoveryTorsoVerticalCm = V7Recovery.HandTorsoLocalCm.Y;
        const bool bPowerPocketMaximumElbowPassed =
            IsAtMostWithTolerance(
                OutMetrics.V7PowerPocketMaximumElbowAngleDegrees,
                Thresholds.PowerPocketMaximumElbowAngleDegrees,
                V7DriveArmBoundaryToleranceDegrees);
        const bool bReleaseMinimumElbowPassed =
            IsAtLeastWithTolerance(
                OutMetrics.V7ReleaseElbowAngleDegrees,
                Thresholds.ReleaseMinimumElbowAngleDegrees,
                V7DriveArmBoundaryToleranceDegrees);
        OutMetrics.bArmSpatialGatePassed =
            OutMetrics.V7SupportArmMaximumReachRatio
                <= Thresholds.SupportMaximumReachRatio
            && OutMetrics.V7SupportArmMinimumElbowAngleDegrees
                >= Thresholds.SupportMinimumElbowAngleDegrees
            && OutMetrics.V7SupportArmMaximumElbowAngleDegrees
                <= Thresholds.SupportMaximumElbowAngleDegrees
            && OutMetrics.V7PowerPocketMinimumReachRatio
                >= Thresholds.PowerPocketMinimumReachRatio
            && OutMetrics.V7PowerPocketMaximumReachRatio
                <= Thresholds.PowerPocketMaximumReachRatio
            && OutMetrics.V7PowerPocketMinimumElbowAngleDegrees
                >= Thresholds.PowerPocketMinimumElbowAngleDegrees
            && bPowerPocketMaximumElbowPassed
            && OutMetrics.V7PowerPocketMaximumAbsoluteTorsoLateralCm
                <= Thresholds.PowerPocketMaximumAbsoluteTorsoLateralCm
            && OutMetrics.V7PowerPocketMinimumTorsoForwardCm
                >= Thresholds.PowerPocketMinimumTorsoForwardCm
            && OutMetrics.V7PowerPocketMaximumTorsoForwardCm
                <= Thresholds.PowerPocketMaximumTorsoForwardCm
            && OutMetrics.V7PowerPocketMinimumTorsoVerticalCm
                >= Thresholds.PowerPocketMinimumTorsoVerticalCm
            && OutMetrics.V7PowerPocketMaximumTorsoVerticalCm
                <= Thresholds.PowerPocketMaximumTorsoVerticalCm
            && OutMetrics.V7ReleaseReachRatio
                >= Thresholds.ReleaseMinimumReachRatio
            && OutMetrics.V7ReleaseReachRatio
                <= Thresholds.ReleaseMaximumReachRatio
            && bReleaseMinimumElbowPassed
            && OutMetrics.V7ReleaseElbowAngleDegrees
                <= Thresholds.ReleaseMaximumElbowAngleDegrees
            && OutMetrics.V7FollowThroughReachRatio
                >= Thresholds.FollowThroughMinimumReachRatio
            && OutMetrics.V7FollowThroughReachRatio
                <= Thresholds.FollowThroughMaximumReachRatio
            && OutMetrics.V7FollowThroughElbowAngleDegrees
                >= Thresholds.FollowThroughMinimumElbowAngleDegrees
            && OutMetrics.V7FollowThroughElbowAngleDegrees
                <= Thresholds.FollowThroughMaximumElbowAngleDegrees
            && OutMetrics.V7RecoveryReachRatio
                <= Thresholds.RecoveryMaximumReachRatio
            && OutMetrics.V7RecoveryElbowAngleDegrees
                >= Thresholds.RecoveryMinimumElbowAngleDegrees
            && OutMetrics.V7RecoveryElbowAngleDegrees
                <= Thresholds.RecoveryMaximumElbowAngleDegrees
            && OutMetrics.V7RecoveryAbsoluteTorsoLateralCm
                <= Thresholds.RecoveryMaximumAbsoluteTorsoLateralCm
            && OutMetrics.V7RecoveryTorsoVerticalCm
                <= Thresholds.RecoveryMaximumTorsoVerticalCm;
        if (!OutMetrics.bArmSpatialGatePassed)
        {
            Error = FString::Printf(
                TEXT("Sequence %s v7 bilateral arm gate failed: support reach %.6f elbow %.3f..%.3f, pocket reach %.6f..%.6f elbow %.9f..%.9f lateral %.3f forward %.3f..%.3f vertical %.3f..%.3f, release %.6f/%.9f, follow %.6f/%.3f, recovery %.6f/%.3f lateral %.3f vertical %.3f; boundary predicates pocket_max_elbow<=%.3f+%.3f=%s release_min_elbow>=%.3f-%.3f=%s"),
                *Family.Family,
                OutMetrics.V7SupportArmMaximumReachRatio,
                OutMetrics.V7SupportArmMinimumElbowAngleDegrees,
                OutMetrics.V7SupportArmMaximumElbowAngleDegrees,
                OutMetrics.V7PowerPocketMinimumReachRatio,
                OutMetrics.V7PowerPocketMaximumReachRatio,
                OutMetrics.V7PowerPocketMinimumElbowAngleDegrees,
                OutMetrics.V7PowerPocketMaximumElbowAngleDegrees,
                OutMetrics.V7PowerPocketMaximumAbsoluteTorsoLateralCm,
                OutMetrics.V7PowerPocketMinimumTorsoForwardCm,
                OutMetrics.V7PowerPocketMaximumTorsoForwardCm,
                OutMetrics.V7PowerPocketMinimumTorsoVerticalCm,
                OutMetrics.V7PowerPocketMaximumTorsoVerticalCm,
                OutMetrics.V7ReleaseReachRatio,
                OutMetrics.V7ReleaseElbowAngleDegrees,
                OutMetrics.V7FollowThroughReachRatio,
                OutMetrics.V7FollowThroughElbowAngleDegrees,
                OutMetrics.V7RecoveryReachRatio,
                OutMetrics.V7RecoveryElbowAngleDegrees,
                OutMetrics.V7RecoveryAbsoluteTorsoLateralCm,
                OutMetrics.V7RecoveryTorsoVerticalCm,
                Thresholds.PowerPocketMaximumElbowAngleDegrees,
                V7DriveArmBoundaryToleranceDegrees,
                bPowerPocketMaximumElbowPassed ? TEXT("PASS") : TEXT("FAIL"),
                Thresholds.ReleaseMinimumElbowAngleDegrees,
                V7DriveArmBoundaryToleranceDegrees,
                bReleaseMinimumElbowPassed ? TEXT("PASS") : TEXT("FAIL"));
            return false;
        }
    }
    if (Recipe.Version.SchemaVersion >= 6 && !bV7DriveArmGate)
    {
        const FArmSpatialGateThresholds& Thresholds =
            V6ArmSpatialThresholds();
        const float MinimumElbowAngleDegrees = FMath::Min3(
            ReachBackArm.ElbowAngleDegrees, ReleaseArm.ElbowAngleDegrees,
            FollowThroughArm.ElbowAngleDegrees);
        const float MinimumArmExtensionRatio = FMath::Min3(
            ReachBackArm.ReachRatio, ReleaseArm.ReachRatio,
            FollowThroughArm.ReachRatio);
        const bool bReachBackDirectionalDominance =
            FMath::Abs(ReachBackArm.HandTorsoLocalCm.X)
                >= FMath::Abs(ReachBackArm.HandTorsoLocalCm.Z);
        const bool bFollowThroughDirectionalDominance =
            FMath::Abs(FollowThroughArm.HandTorsoLocalCm.X)
                >= FMath::Abs(FollowThroughArm.HandTorsoLocalCm.Z);
        OutMetrics.bArmSpatialGatePassed =
            MinimumElbowAngleDegrees
                >= Thresholds.MinimumElbowAngleDegrees
            && MinimumArmExtensionRatio
                >= Thresholds.MinimumArmExtensionRatio
            && bReachBackDirectionalDominance
            && bFollowThroughDirectionalDominance
            && OutMetrics.ReachBackThrowingHandRadialReachCm
                >= Thresholds.MinimumReachBackRadialReachCm
            && OutMetrics.MinimumThrowingHandVerticalCm
                >= Thresholds.MinimumHandVerticalCm
            && OutMetrics.MinimumReleaseFollowThrowingHandLateralClearanceCm
                >= Thresholds.MinimumReleaseFollowLateralClearanceCm
            && OutMetrics.MaximumThrowingHandDirectionFrameDeltaDegrees
                <= Thresholds.MaximumDirectionFrameDeltaDegrees;
        if (!OutMetrics.bArmSpatialGatePassed)
        {
            Error = FString::Printf(
                TEXT("Sequence %s v6 arm spatial gate failed: min elbow %.3f (>=%.3f), min extension %.6f (>=%.6f), direction dominance reachback=%s follow=%s, reachback radial %.3f (>=%.3f), min vertical %.3f (>=%.3f), release/follow lateral %.3f (>=%.3f), max direction step %.3f (<=%.3f)"),
                *Family.Family, MinimumElbowAngleDegrees,
                Thresholds.MinimumElbowAngleDegrees,
                MinimumArmExtensionRatio,
                Thresholds.MinimumArmExtensionRatio,
                bReachBackDirectionalDominance ? TEXT("true") : TEXT("false"),
                bFollowThroughDirectionalDominance ? TEXT("true") : TEXT("false"),
                OutMetrics.ReachBackThrowingHandRadialReachCm,
                Thresholds.MinimumReachBackRadialReachCm,
                OutMetrics.MinimumThrowingHandVerticalCm,
                Thresholds.MinimumHandVerticalCm,
                OutMetrics.MinimumReleaseFollowThrowingHandLateralClearanceCm,
                Thresholds.MinimumReleaseFollowLateralClearanceCm,
                OutMetrics.MaximumThrowingHandDirectionFrameDeltaDegrees,
                Thresholds.MaximumDirectionFrameDeltaDegrees);
            return false;
        }
    }
    return true;
}

bool ValidateSequence(const FRecipe& Recipe, const FFamilyRecipe& Family,
    UAnimSequence* Sequence, USkeleton* Skeleton,
    FV4ComponentSpaceValidationMetrics& OutComponentMetrics, FString& Error)
{
    if (!Sequence || Sequence->GetSkeleton() != Skeleton
        || Sequence->bEnableRootMotion || Sequence->bLoop
        || Sequence->RootMotionRootLock != ERootMotionRootLock::RefPose
        || !FMath::IsNearlyEqual(Sequence->RateScale, 1.0f, TimingTolerance)
        || !ValidateMetadata(Sequence, Recipe, Family.MotionId, Error))
    {
        if (Error.IsEmpty()) Error = FString::Printf(
            TEXT("Sequence base contract differs for %s"), *Family.Family);
        return false;
    }
    const TScriptInterface<IAnimationDataModel> Interface =
        Sequence->GetDataModelInterface();
    const IAnimationDataModel* Model = Interface.GetInterface();
    if (!Model || Model->GetFrameRate() != FFrameRate(FrameRate, 1)
        || Model->GetNumberOfFrames() != Family.FrameCount
        || !FMath::IsNearlyEqual(Model->GetPlayLength(),
            Family.DurationSeconds, TimingTolerance))
    {
        Error = FString::Printf(TEXT("Sequence timing differs for %s"), *Family.Family);
        return false;
    }
    TArray<FName> TrackNames;
    Model->GetBoneTrackNames(TrackNames);
    TSet<FName> TrackSet;
    for (const FName TrackName : TrackNames)
    {
        TrackSet.Add(TrackName);
    }
    if (TrackSet.Num() != Recipe.AnimatedBones.Num())
    {
        Error = FString::Printf(TEXT("Sequence track count differs for %s"), *Family.Family);
        return false;
    }
    const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
    const TArray<FTransform>& RefPose = RefSkeleton.GetRefBonePose();
    TArray<TArray<FQuat>> ExpectedLocalRotations;
    if (Recipe.Version.SchemaVersion == 4)
    {
        BuildV4AuthoredLocalRotations(Recipe, Family, RefSkeleton, RefPose,
            ExpectedLocalRotations);
    }
    else if (Recipe.Version.SchemaVersion >= 5)
    {
        BuildMixedSpaceAuthoredLocalRotations(Recipe, Family, RefSkeleton, RefPose,
            ExpectedLocalRotations);
    }
    TMap<FName, TArray<FTransform>> TrackTransformsByName;
    for (const FName Bone : Recipe.AnimatedBones)
    {
        if (!TrackSet.Contains(Bone))
        {
            Error = FString::Printf(TEXT("Sequence %s omits track %s"),
                *Family.Family, *Bone.ToString());
            return false;
        }
        TArray<FTransform> Transforms;
        Model->GetBoneTrackTransforms(Bone, Transforms);
        if (Transforms.Num() != Family.FrameCount + 1)
        {
            Error = FString::Printf(TEXT("Sequence %s track %s key count differs"),
                *Family.Family, *Bone.ToString());
            return false;
        }
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(Bone);
        if (!RefPose.IsValidIndex(BoneIndex))
        {
            Error = FString::Printf(TEXT("Sequence %s track %s has no reference bone"),
                *Family.Family, *Bone.ToString());
            return false;
        }
        const FTransform& Reference = RefPose[BoneIndex];
        for (int32 Frame = 0; Frame < Transforms.Num(); ++Frame)
        {
            const FTransform& Transform = Transforms[Frame];
            if (Transform.ContainsNaN() || !Transform.IsValid())
            {
                Error = FString::Printf(TEXT("Sequence %s contains invalid transforms"),
                    *Family.Family);
                return false;
            }
            if (Recipe.Version.SchemaVersion >= 4)
            {
                const FVector ExpectedPosition = Reference.GetTranslation()
                    + TranslationAtFrame(Recipe, Family.Poses, Bone, Frame);
                const float RotationErrorDegrees = FMath::RadiansToDegrees(
                    static_cast<float>(Transform.GetRotation().AngularDistance(
                        ExpectedLocalRotations[Frame][BoneIndex])));
                if (!Transform.GetTranslation().Equals(ExpectedPosition, 0.0002)
                    || !Transform.GetScale3D().Equals(
                        Reference.GetScale3D(), 0.0002)
                    || RotationErrorDegrees
                        > AuthoredTrackRoundTripRotationToleranceDegrees)
                {
                    Error = FString::Printf(
                        TEXT("Sequence %s track %s frame %d does not preserve the authored rotation-space basis (%.4f degrees)"),
                        *Family.Family, *Bone.ToString(), Frame,
                        RotationErrorDegrees);
                    return false;
                }
            }
        }
        TrackTransformsByName.Add(Bone, MoveTemp(Transforms));
    }
    TArray<FTransform> RootTransforms;
    Model->GetBoneTrackTransforms(TEXT("root"), RootTransforms);
    if (RootTransforms.IsEmpty())
    {
        Error = FString::Printf(TEXT("Sequence %s has no root track"), *Family.Family);
        return false;
    }
    const FVector RootReference = RootTransforms[0].GetTranslation();
    float MinimumRootX = TNumericLimits<float>::Max();
    float MaximumRootX = TNumericLimits<float>::Lowest();
    for (const FTransform& Transform : RootTransforms)
    {
        const FVector Delta = Transform.GetTranslation() - RootReference;
        MinimumRootX = FMath::Min(MinimumRootX, static_cast<float>(Delta.X));
        MaximumRootX = FMath::Max(MaximumRootX, static_cast<float>(Delta.X));
        if (Delta.Size() > Family.MaximumRootTranslationCm + 0.1f)
        {
            Error = FString::Printf(
                TEXT("Sequence %s exceeds bounded presentation-root travel"),
                *Family.Family);
            return false;
        }
    }
    if (FVector::Dist(RootTransforms.Last().GetTranslation(), RootReference)
            > Family.RootReturnToleranceCm + 0.05f
        || MaximumRootX - MinimumRootX + 0.05f
            < Family.MinimumForwardRootExcursionCm)
    {
        Error = FString::Printf(
            TEXT("Sequence %s presentation-root trajectory differs"),
            *Family.Family);
        return false;
    }
    for (const FCurveSpec& Curve : Family.Curves)
    {
        const FAnimationCurveIdentifier Id(Curve.Name, ERawCurveTrackTypes::RCT_Float);
        const FRichCurve* RichCurve = Model->FindRichCurve(Id);
        if (!RichCurve || RichCurve->GetNumKeys() != Curve.Keys.Num())
        {
            Error = FString::Printf(TEXT("Sequence %s curve %s differs"),
                *Family.Family, *Curve.Name.ToString());
            return false;
        }
        for (const TPair<int32, float>& Pair : Curve.Keys)
        {
            if (!FMath::IsNearlyEqual(
                    RichCurve->Eval(FrameToTime(Pair.Key)), Pair.Value,
                    TimingTolerance))
            {
                Error = FString::Printf(
                    TEXT("Sequence %s curve %s values differ"),
                    *Family.Family, *Curve.Name.ToString());
                return false;
            }
        }
        if (Recipe.Version.SchemaVersion >= 3)
        {
            for (const FRichCurveKey& Key : RichCurve->GetConstRefOfKeys())
            {
                if (Key.InterpMode != RCIM_Linear)
                {
                    Error = FString::Printf(
                        TEXT("Sequence %s curve %s is not bounded-linear"),
                        *Family.Family, *Curve.Name.ToString());
                    return false;
                }
            }
            constexpr int32 SamplesPerFrame = 4;
            for (int32 Sample = 0;
                Sample <= Family.FrameCount * SamplesPerFrame; ++Sample)
            {
                const float Time = static_cast<float>(Sample)
                    / (FrameRate * SamplesPerFrame);
                const float Value = RichCurve->Eval(Time);
                if (!FMath::IsFinite(Value)
                    || Value < -TimingTolerance
                    || Value > 1.0f + TimingTolerance)
                {
                    Error = FString::Printf(
                        TEXT("Sequence %s curve %s leaves [0,1] at %.6f"),
                        *Family.Family, *Curve.Name.ToString(), Value);
                    return false;
                }
            }
        }
    }
    if (Recipe.Version.SchemaVersion == 4
        && !ValidateV4ComponentSpaceTracks(Recipe, Family, RefSkeleton, RefPose,
            TrackTransformsByName, OutComponentMetrics, Error))
    {
        return false;
    }
    if (Recipe.Version.SchemaVersion >= 5
        && !ValidateMixedSpaceTracks(Recipe, Family, RefSkeleton, RefPose,
            TrackTransformsByName, OutComponentMetrics, Error))
    {
        return false;
    }
    return true;
}

bool ValidateMontage(const FRecipe& Recipe, const FFamilyRecipe& Family,
    UAnimMontage* Montage, UAnimSequence* Sequence, USkeleton* Skeleton,
    FString& Error)
{
    if (!Montage || Montage->GetSkeleton() != Skeleton
        || !FMath::IsNearlyEqual(Montage->GetPlayLength(),
            Family.DurationSeconds, TimingTolerance)
        || (Recipe.Version.SchemaVersion >= 6
            && (!Montage->bEnableAutoBlendOut
                || !FMath::IsNearlyEqual(
                    Montage->BlendOutTriggerTime,
                    ProductionMontageBlendOutTriggerTimeSeconds,
                    TimingTolerance)))
        || !ValidateMetadata(Montage, Recipe, Family.MotionId, Error))
    {
        if (Error.IsEmpty()) Error = FString::Printf(
            TEXT("Montage base contract differs for %s"), *Family.Family);
        return false;
    }
    if (Montage->SlotAnimTracks.Num() != 1
        || Montage->SlotAnimTracks[0].SlotName != DefaultSlot
        || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() != 1
        || Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()
            != Sequence)
    {
        Error = FString::Printf(TEXT("Montage segment differs for %s"), *Family.Family);
        return false;
    }
    int32 PhaseCount = 0;
    int32 ReleaseCount = 0;
    int32 FinishCount = 0;
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        if (!Event.Notify)
        {
            Error = FString::Printf(TEXT("Montage %s contains a non-native notify"),
                *Family.Family);
            return false;
        }
        const FString ClassPath = Event.Notify->GetClass()->GetPathName();
        if (ClassPath == PhaseNotifyClassPath)
        {
            if (!Family.Phases.IsValidIndex(PhaseCount)
                || !PhasePropertyMatches(Event.Notify,
                    Family.Phases[PhaseCount].Name)
                || Event.MontageTickType
                    != EMontageNotifyTickType::Queued
                || !FMath::IsNearlyEqual(Event.GetTriggerTime(),
                    FrameToTime(Family.Phases[PhaseCount].Frame),
                    TimingTolerance))
            {
                Error = FString::Printf(
                    TEXT("Phase notify order/timing differs for %s"),
                    *Family.Family);
                return false;
            }
            ++PhaseCount;
        }
        else if (ClassPath == ReleaseNotifyClassPath)
        {
            ++ReleaseCount;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint
                || !FMath::IsNearlyEqual(Event.GetTriggerTime(),
                    FrameToTime(Family.ReleaseFrame), TimingTolerance))
            {
                Error = FString::Printf(TEXT("Release branching point differs for %s"),
                    *Family.Family);
                return false;
            }
        }
        else if (ClassPath == FinishNotifyClassPath)
        {
            ++FinishCount;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint
                || !FMath::IsNearlyEqual(Event.GetTriggerTime(),
                    FrameToTime(Family.FinishFrame), TimingTolerance))
            {
                Error = FString::Printf(TEXT("Finish branching point differs for %s"),
                    *Family.Family);
                return false;
            }
        }
        else
        {
            Error = FString::Printf(TEXT("Montage %s contains unexpected notify %s"),
                *Family.Family, *ClassPath);
            return false;
        }
    }
    if (PhaseCount != RequiredPhases().Num() || ReleaseCount != 1
        || FinishCount != 1 || Montage->Notifies.Num() != 9)
    {
        Error = FString::Printf(TEXT("Montage notify cardinality differs for %s"),
            *Family.Family);
        return false;
    }
    return true;
}

bool ValidateLibrary(const FRecipe& Recipe, UDiscGolfAnimationLibrary* Library,
    const TArray<UAnimMontage*>& Montages, FString& Error)
{
    if (!Library || Library->Entries.Num() != Recipe.Families.Num()
        || !ValidateMetadata(Library, Recipe,
            Recipe.Version.LibraryMotionId, Error))
    {
        if (Error.IsEmpty()) Error = TEXT("Production motion library base contract differs");
        return false;
    }
    for (int32 Index = 0; Index < Recipe.Families.Num(); ++Index)
    {
        const FDGThrowAnimationEntry& Entry = Library->Entries[Index];
        const FFamilyRecipe& Family = Recipe.Families[Index];
        if (Entry.ThrowType != EDGThrowType::Backhand
            || Entry.Handedness != EDGHandedness::Right
            || (Recipe.Version.SchemaVersion >= 2
                && Entry.MotionFamilyId != FName(*Family.Family))
            || Entry.StyleId != Family.StyleId
            || Entry.Montage.ToSoftObjectPath() != FSoftObjectPath(Montages[Index])
            || !FMath::IsNearlyEqual(Entry.RecommendedPowerMin,
                Family.PowerMin, TimingTolerance)
            || !FMath::IsNearlyEqual(Entry.RecommendedPowerMax,
                Family.PowerMax, TimingTolerance))
        {
            Error = FString::Printf(TEXT("Production motion library entry differs for %s"),
                *Family.Family);
            return false;
        }
    }
    return true;
}

bool GuardCommandLine(const FRecipeVersionSpec& Version, FString& Error)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    if (!IsRunningCommandlet()
        || !FParse::Param(CommandLine, TEXT("DGAuthorProductionProceduralMotion"))
        || !FParse::Param(CommandLine, TEXT("unattended"))
        || !FParse::Param(CommandLine, TEXT("nop4"))
        || !FString(CommandLine).Contains(TEXT("-run=pythonscript"),
            ESearchCase::IgnoreCase))
    {
        Error = TEXT("Authoring requires PythonScript commandlet, explicit motion switch, -unattended, and -nop4");
        return false;
    }
    FString CommandVersion;
    if (!FParse::Value(CommandLine, TEXT("DGProductionMotionVersion="),
            CommandVersion)
        || CommandVersion.ToLower() != Version.Version)
    {
        Error = TEXT("Authoring requires an exact matching -DGProductionMotionVersion=v1|v2|v3|v4|v5|v6|v7");
        return false;
    }
    FString UserDir;
    if (!FParse::Value(CommandLine, TEXT("UserDir="), UserDir) || UserDir.IsEmpty())
    {
        Error = TEXT("Authoring requires an external -UserDir");
        return false;
    }
    UserDir = FPaths::ConvertRelativePathToFull(UserDir);
    FPaths::NormalizeFilename(UserDir);
    FString Root = RequiredUserDirRoot;
    FPaths::NormalizeFilename(Root);
    if (!Root.EndsWith(TEXT("/"))) Root += TEXT("/");
    const FString Prefix = Root;
    if (!UserDir.StartsWith(Prefix, ESearchCase::IgnoreCase))
    {
        Error = TEXT("Authoring UserDir is outside the Session19ProductionMotion root");
        return false;
    }
    FString Relative = UserDir.Mid(Prefix.Len());
    if (Relative.Contains(TEXT("/")) || Relative.Contains(TEXT("\\")))
    {
        Error = TEXT("Authoring UserDir must be one direct UUID child");
        return false;
    }
    FGuid Guid;
    if (!FGuid::Parse(Relative, Guid)
        || Guid.ToString(EGuidFormats::DigitsWithHyphensLower) != Relative.ToLower())
    {
        Error = TEXT("Authoring UserDir child must be a canonical UUID");
        return false;
    }
    FString ProjectRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeFilename(ProjectRoot);
    if (UserDir.StartsWith(ProjectRoot, ESearchCase::IgnoreCase))
    {
        Error = TEXT("Authoring UserDir must be external to the project");
        return false;
    }
    return true;
}

bool RollbackCreatedFiles(const TArray<FString>& CreatedFiles)
{
    bool bSucceeded = true;
    for (int32 Index = CreatedFiles.Num() - 1; Index >= 0; --Index)
    {
        if (IFileManager::Get().FileExists(*CreatedFiles[Index])
            && !IFileManager::Get().Delete(*CreatedFiles[Index], false, true, true))
        {
            bSucceeded = false;
        }
    }
    return bSucceeded;
}

FString BuildValidationReport(const FRecipeVersionSpec& Version)
{
    FString Error;
    FRecipe Recipe;
    if (!LoadRecipe(Version, Recipe, Error)) return FailureJson(Error);
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
    if (!Skeleton || !Mesh || Mesh->GetSkeleton() != Skeleton)
    {
        return FailureJson(TEXT("DGMaster skeleton/mesh did not load as one authoring pair"));
    }
    TArray<UAnimMontage*> Montages;
    TArray<TSharedPtr<FJsonValue>> FamiliesJson;
    for (const FFamilyRecipe& Family : Recipe.Families)
    {
        UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *Family.SequencePath);
        UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Family.MontagePath);
        FV4ComponentSpaceValidationMetrics ComponentMetrics;
        if (!ValidateSequence(Recipe, Family, Sequence, Skeleton,
                ComponentMetrics, Error)
            || !ValidateMontage(Recipe, Family, Montage, Sequence, Skeleton, Error))
        {
            return FailureJson(Error);
        }
        Montages.Add(Montage);
        const TSharedRef<FJsonObject> FamilyJson = MakeShared<FJsonObject>();
        FamilyJson->SetStringField(TEXT("motion_id"), Family.MotionId);
        FamilyJson->SetStringField(TEXT("family"), Family.Family);
        FamilyJson->SetStringField(TEXT("sequence"), Sequence->GetPathName());
        FamilyJson->SetStringField(TEXT("montage"), Montage->GetPathName());
        FamilyJson->SetNumberField(TEXT("frames"), Family.FrameCount);
        FamilyJson->SetNumberField(TEXT("duration_seconds"), Family.DurationSeconds);
        FamilyJson->SetNumberField(TEXT("release_frame"), Family.ReleaseFrame);
        FamilyJson->SetNumberField(TEXT("finish_frame"), Family.FinishFrame);
        FamilyJson->SetBoolField(TEXT("root_motion_enabled"), false);
        FamilyJson->SetNumberField(TEXT("presentation_root_scale"),
            Family.PresentationRootScale);
        FamilyJson->SetNumberField(TEXT("minimum_forward_root_excursion_cm"),
            Family.MinimumForwardRootExcursionCm);
        FamilyJson->SetNumberField(TEXT("maximum_root_translation_cm"),
            Family.MaximumRootTranslationCm);
        int32 MaximumPoseGap = 0;
        int32 RecoveryPoseCount = 0;
        int32 MinimumRotationChannels = TNumericLimits<int32>::Max();
        for (int32 PoseIndex = 0; PoseIndex < Family.Poses.Num(); ++PoseIndex)
        {
            const FPoseKey& Pose = Family.Poses[PoseIndex];
            if (PoseIndex > 0)
            {
                MaximumPoseGap = FMath::Max(MaximumPoseGap,
                    Pose.Frame - Family.Poses[PoseIndex - 1].Frame);
            }
            if (Pose.Frame >= Family.Phases[5].Frame
                && Pose.Frame <= Family.FinishFrame)
            {
                ++RecoveryPoseCount;
            }
            MinimumRotationChannels = FMath::Min(MinimumRotationChannels,
                Pose.AuthoredRotationChannelCount);
        }
        FamilyJson->SetNumberField(TEXT("pose_key_count"), Family.Poses.Num());
        FamilyJson->SetNumberField(TEXT("maximum_pose_key_gap_frames"),
            MaximumPoseGap);
        FamilyJson->SetNumberField(TEXT("recovery_pose_key_count"),
            RecoveryPoseCount);
        FamilyJson->SetNumberField(
            TEXT("minimum_authored_rotation_channels_per_pose"),
            MinimumRotationChannels);
        FamilyJson->SetNumberField(TEXT("curve_count"), Family.Curves.Num());
        if (Recipe.Version.SchemaVersion >= 3)
        {
            const FBiomechanicalEvents& Events = Family.Biomechanics;
            const TSharedRef<FJsonObject> EventsJson = MakeShared<FJsonObject>();
            EventsJson->SetNumberField(TEXT("disc_reachback_plane_frame"),
                Events.DiscReachbackPlaneFrame);
            EventsJson->SetNumberField(TEXT("weight_shift_frame"),
                Events.WeightShiftFrame);
            EventsJson->SetNumberField(TEXT("brace_compression_frame"),
                Events.BraceCompressionFrame);
            EventsJson->SetNumberField(TEXT("hip_fire_frame"),
                Events.HipFireFrame);
            EventsJson->SetNumberField(TEXT("torso_fire_frame"),
                Events.TorsoFireFrame);
            EventsJson->SetNumberField(TEXT("off_arm_counterbalance_frame"),
                Events.OffArmCounterbalanceFrame);
            EventsJson->SetNumberField(TEXT("shoulder_fire_frame"),
                Events.ShoulderFireFrame);
            EventsJson->SetNumberField(TEXT("elbow_lead_frame"),
                Events.ElbowLeadFrame);
            EventsJson->SetNumberField(TEXT("wrist_lag_frame"),
                Events.WristLagFrame);
            EventsJson->SetNumberField(TEXT("release_frame"),
                Events.ReleaseFrame);
            EventsJson->SetNumberField(TEXT("brace_extension_frame"),
                Events.BraceExtensionFrame);
            EventsJson->SetNumberField(TEXT("gaze_reacquire_frame"),
                Events.GazeReacquireFrame);
            EventsJson->SetNumberField(TEXT("recovery_deceleration_frame"),
                Events.RecoveryDecelerationFrame);
            EventsJson->SetNumberField(TEXT("recovery_recenter_frame"),
                Events.RecoveryRecenterFrame);
            EventsJson->SetNumberField(TEXT("recovery_settle_frame"),
                Events.RecoverySettleFrame);
            FamilyJson->SetObjectField(TEXT("biomechanical_events"), EventsJson);
        }
        if (Recipe.Version.SchemaVersion >= 4)
        {
            FamilyJson->SetStringField(TEXT("rotation_space"),
                Recipe.RotationSpace);
            FamilyJson->SetStringField(TEXT("local_track_construction"),
                Recipe.Version.SchemaVersion >= 5
                    ? TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS")
                    : TEXT("COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"));
            FamilyJson->SetNumberField(TEXT("component_validation_sampled_frames"),
                ComponentMetrics.SampledFrameCount);
            FamilyJson->SetNumberField(TEXT("minimum_joint_segment_ratio"),
                ComponentMetrics.MinimumJointSegmentRatio);
            FamilyJson->SetNumberField(TEXT("maximum_joint_segment_ratio"),
                ComponentMetrics.MaximumJointSegmentRatio);
            FamilyJson->SetNumberField(
                TEXT("maximum_component_intent_error_degrees"),
                ComponentMetrics.MaximumComponentIntentErrorDegrees);
            FamilyJson->SetNumberField(TEXT("maximum_component_delta_degrees"),
                ComponentMetrics.MaximumComponentDeltaDegrees);
            if (Recipe.Version.SchemaVersion >= 5)
            {
                FamilyJson->SetStringField(TEXT("root_track_policy"),
                    Recipe.RootTrackPolicy);
                FamilyJson->SetNumberField(TEXT("component_rotation_bone_count"),
                    Recipe.ComponentRotationBones.Num());
                FamilyJson->SetNumberField(TEXT("local_rotation_bone_count"),
                    Recipe.LocalRotationBones.Num());
                FamilyJson->SetNumberField(
                    TEXT("maximum_local_intent_error_degrees"),
                    ComponentMetrics.MaximumLocalIntentErrorDegrees);
                FamilyJson->SetNumberField(
                    TEXT("maximum_local_frame_delta_degrees"),
                    ComponentMetrics.MaximumLocalFrameDeltaDegrees);
                FamilyJson->SetNumberField(
                    TEXT("maximum_finger_frame_delta_degrees"),
                    ComponentMetrics.MaximumFingerFrameDeltaDegrees);
                FamilyJson->SetNumberField(
                    TEXT("maximum_root_rotation_error_degrees"),
                    ComponentMetrics.MaximumRootRotationErrorDegrees);
                FamilyJson->SetNumberField(
                    TEXT("named_phase_knee_sample_count"),
                    ComponentMetrics.NamedPhaseKneeSampleCount);
                FamilyJson->SetNumberField(
                    TEXT("minimum_named_phase_knee_angle_degrees"),
                    ComponentMetrics.MinimumNamedPhaseKneeAngleDegrees);
                FamilyJson->SetNumberField(
                    TEXT("maximum_named_phase_knee_angle_degrees"),
                    ComponentMetrics.MaximumNamedPhaseKneeAngleDegrees);
                FamilyJson->SetNumberField(TEXT("arm_spatial_sample_count"),
                    ComponentMetrics.ArmSpatialSampleCount);
                FamilyJson->SetNumberField(
                    TEXT("reachback_throwing_elbow_angle_degrees"),
                    ComponentMetrics.ReachBackThrowingElbowAngleDegrees);
                FamilyJson->SetNumberField(
                    TEXT("release_throwing_elbow_angle_degrees"),
                    ComponentMetrics.ReleaseThrowingElbowAngleDegrees);
                FamilyJson->SetNumberField(
                    TEXT("followthrough_throwing_elbow_angle_degrees"),
                    ComponentMetrics.FollowThroughThrowingElbowAngleDegrees);
                FamilyJson->SetNumberField(
                    TEXT("reachback_throwing_arm_reach_ratio"),
                    ComponentMetrics.ReachBackThrowingArmReachRatio);
                FamilyJson->SetNumberField(
                    TEXT("release_throwing_arm_reach_ratio"),
                    ComponentMetrics.ReleaseThrowingArmReachRatio);
                FamilyJson->SetNumberField(
                    TEXT("followthrough_throwing_arm_reach_ratio"),
                    ComponentMetrics.FollowThroughThrowingArmReachRatio);
                FamilyJson->SetNumberField(
                    TEXT("reachback_throwing_hand_torso_clearance_cm"),
                    ComponentMetrics.ReachBackThrowingHandTorsoClearanceCm);
                FamilyJson->SetNumberField(
                    TEXT("release_throwing_hand_torso_clearance_cm"),
                    ComponentMetrics.ReleaseThrowingHandTorsoClearanceCm);
                FamilyJson->SetNumberField(
                    TEXT("followthrough_throwing_hand_torso_clearance_cm"),
                    ComponentMetrics.FollowThroughThrowingHandTorsoClearanceCm);
                auto SetDirection = [&FamilyJson](const TCHAR* Field,
                    const FVector& Direction)
                {
                    TArray<TSharedPtr<FJsonValue>> Values;
                    Values.Add(MakeShared<FJsonValueNumber>(Direction.X));
                    Values.Add(MakeShared<FJsonValueNumber>(Direction.Y));
                    Values.Add(MakeShared<FJsonValueNumber>(Direction.Z));
                    FamilyJson->SetArrayField(Field, Values);
                };
                SetDirection(TEXT("reachback_throwing_arm_direction_torso_local"),
                    ComponentMetrics.ReachBackThrowingArmDirectionTorsoLocal);
                SetDirection(TEXT("release_throwing_arm_direction_torso_local"),
                    ComponentMetrics.ReleaseThrowingArmDirectionTorsoLocal);
                SetDirection(
                    TEXT("followthrough_throwing_arm_direction_torso_local"),
                    ComponentMetrics.FollowThroughThrowingArmDirectionTorsoLocal);
                SetDirection(TEXT("reachback_throwing_hand_torso_local_cm"),
                    ComponentMetrics.ReachBackThrowingHandTorsoLocalCm);
                SetDirection(TEXT("release_throwing_hand_torso_local_cm"),
                    ComponentMetrics.ReleaseThrowingHandTorsoLocalCm);
                SetDirection(TEXT("followthrough_throwing_hand_torso_local_cm"),
                    ComponentMetrics.FollowThroughThrowingHandTorsoLocalCm);
                FamilyJson->SetNumberField(
                    TEXT("reachback_to_release_arm_direction_delta_degrees"),
                    ComponentMetrics.ReachBackToReleaseArmDirectionDeltaDegrees);
                FamilyJson->SetNumberField(
                    TEXT("release_to_followthrough_arm_direction_delta_degrees"),
                    ComponentMetrics.ReleaseToFollowThroughArmDirectionDeltaDegrees);
                FamilyJson->SetNumberField(
                    TEXT("reachback_to_followthrough_arm_direction_delta_degrees"),
                    ComponentMetrics.ReachBackToFollowThroughArmDirectionDeltaDegrees);
                FamilyJson->SetNumberField(
                    TEXT("reachback_to_followthrough_hand_travel_cm"),
                    ComponentMetrics.ReachBackToFollowThroughHandTravelCm);
                FamilyJson->SetNumberField(
                    TEXT("reachback_throwing_arm_collapse_ratio"),
                    1.0f - ComponentMetrics.ReachBackThrowingArmReachRatio);
                FamilyJson->SetNumberField(
                    TEXT("release_throwing_arm_collapse_ratio"),
                    1.0f - ComponentMetrics.ReleaseThrowingArmReachRatio);
                FamilyJson->SetNumberField(
                    TEXT("followthrough_throwing_arm_collapse_ratio"),
                    1.0f - ComponentMetrics.FollowThroughThrowingArmReachRatio);
                FamilyJson->SetNumberField(
                    TEXT("reachback_throwing_hand_radial_reach_cm"),
                    ComponentMetrics.ReachBackThrowingHandRadialReachCm);
                FamilyJson->SetNumberField(
                    TEXT("minimum_throwing_hand_vertical_cm"),
                    ComponentMetrics.MinimumThrowingHandVerticalCm);
                FamilyJson->SetNumberField(
                    TEXT("minimum_release_follow_throwing_hand_lateral_clearance_cm"),
                    ComponentMetrics
                        .MinimumReleaseFollowThrowingHandLateralClearanceCm);
                FamilyJson->SetNumberField(
                    TEXT("maximum_throwing_hand_direction_frame_delta_degrees"),
                    ComponentMetrics
                        .MaximumThrowingHandDirectionFrameDeltaDegrees);
                FamilyJson->SetBoolField(TEXT("arm_spatial_gate_applied"),
                    Recipe.Version.SchemaVersion >= 6);
                FamilyJson->SetBoolField(TEXT("arm_spatial_gate_passed"),
                    ComponentMetrics.bArmSpatialGatePassed);
                const bool bV7DriveArmGate =
                    Recipe.Version.SchemaVersion == 7
                    && Family.Family == TEXT("Drive");
                FamilyJson->SetStringField(TEXT("arm_spatial_gate_contract"),
                    bV7DriveArmGate
                        ? TEXT("V7_DRIVE_BILATERAL_POWER_POCKET_RELEASE_RECOVERY")
                        : Recipe.Version.SchemaVersion >= 6
                            ? TEXT("V6_THROWING_ARM_NAMED_PHASE_SPATIAL")
                            : TEXT("NOT_APPLIED"));
                if (bV7DriveArmGate)
                {
                    const FV7DriveArmGateThresholds& Thresholds =
                        V7DriveArmThresholds();
                    FamilyJson->SetBoolField(TEXT("v7_drive_arm_gate_applied"),
                        ComponentMetrics.bV7DriveArmGateApplied);
                    FamilyJson->SetNumberField(TEXT("v7_support_arm_sample_count"),
                        ComponentMetrics.V7SupportArmSampleCount);
                    FamilyJson->SetNumberField(
                        TEXT("v7_support_arm_maximum_reach_ratio"),
                        ComponentMetrics.V7SupportArmMaximumReachRatio);
                    FamilyJson->SetNumberField(
                        TEXT("v7_support_arm_minimum_elbow_angle_degrees"),
                        ComponentMetrics.V7SupportArmMinimumElbowAngleDegrees);
                    FamilyJson->SetNumberField(
                        TEXT("v7_support_arm_maximum_elbow_angle_degrees"),
                        ComponentMetrics.V7SupportArmMaximumElbowAngleDegrees);
                    FamilyJson->SetNumberField(TEXT("v7_power_pocket_sample_count"),
                        ComponentMetrics.V7PowerPocketSampleCount);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_minimum_reach_ratio"),
                        ComponentMetrics.V7PowerPocketMinimumReachRatio);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_maximum_reach_ratio"),
                        ComponentMetrics.V7PowerPocketMaximumReachRatio);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_minimum_elbow_angle_degrees"),
                        ComponentMetrics.V7PowerPocketMinimumElbowAngleDegrees);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_maximum_elbow_angle_degrees"),
                        ComponentMetrics.V7PowerPocketMaximumElbowAngleDegrees);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_maximum_absolute_torso_lateral_cm"),
                        ComponentMetrics.V7PowerPocketMaximumAbsoluteTorsoLateralCm);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_minimum_torso_forward_cm"),
                        ComponentMetrics.V7PowerPocketMinimumTorsoForwardCm);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_maximum_torso_forward_cm"),
                        ComponentMetrics.V7PowerPocketMaximumTorsoForwardCm);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_minimum_torso_vertical_cm"),
                        ComponentMetrics.V7PowerPocketMinimumTorsoVerticalCm);
                    FamilyJson->SetNumberField(
                        TEXT("v7_power_pocket_maximum_torso_vertical_cm"),
                        ComponentMetrics.V7PowerPocketMaximumTorsoVerticalCm);
                    FamilyJson->SetNumberField(TEXT("v7_release_reach_ratio"),
                        ComponentMetrics.V7ReleaseReachRatio);
                    FamilyJson->SetNumberField(
                        TEXT("v7_release_elbow_angle_degrees"),
                        ComponentMetrics.V7ReleaseElbowAngleDegrees);
                    FamilyJson->SetNumberField(TEXT("v7_followthrough_reach_ratio"),
                        ComponentMetrics.V7FollowThroughReachRatio);
                    FamilyJson->SetNumberField(
                        TEXT("v7_followthrough_elbow_angle_degrees"),
                        ComponentMetrics.V7FollowThroughElbowAngleDegrees);
                    FamilyJson->SetNumberField(TEXT("v7_recovery_reach_ratio"),
                        ComponentMetrics.V7RecoveryReachRatio);
                    FamilyJson->SetNumberField(
                        TEXT("v7_recovery_elbow_angle_degrees"),
                        ComponentMetrics.V7RecoveryElbowAngleDegrees);
                    FamilyJson->SetNumberField(
                        TEXT("v7_recovery_absolute_torso_lateral_cm"),
                        ComponentMetrics.V7RecoveryAbsoluteTorsoLateralCm);
                    FamilyJson->SetNumberField(
                        TEXT("v7_recovery_torso_vertical_cm"),
                        ComponentMetrics.V7RecoveryTorsoVerticalCm);
                    const TSharedRef<FJsonObject> ThresholdJson =
                        MakeShared<FJsonObject>();
                    ThresholdJson->SetNumberField(
                        TEXT("support_maximum_reach_ratio"),
                        Thresholds.SupportMaximumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("support_minimum_elbow_angle_degrees"),
                        Thresholds.SupportMinimumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("support_maximum_elbow_angle_degrees"),
                        Thresholds.SupportMaximumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_minimum_reach_ratio"),
                        Thresholds.PowerPocketMinimumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_maximum_reach_ratio"),
                        Thresholds.PowerPocketMaximumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_minimum_elbow_angle_degrees"),
                        Thresholds.PowerPocketMinimumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_maximum_elbow_angle_degrees"),
                        Thresholds.PowerPocketMaximumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_maximum_absolute_torso_lateral_cm"),
                        Thresholds.PowerPocketMaximumAbsoluteTorsoLateralCm);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_minimum_torso_forward_cm"),
                        Thresholds.PowerPocketMinimumTorsoForwardCm);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_maximum_torso_forward_cm"),
                        Thresholds.PowerPocketMaximumTorsoForwardCm);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_minimum_torso_vertical_cm"),
                        Thresholds.PowerPocketMinimumTorsoVerticalCm);
                    ThresholdJson->SetNumberField(
                        TEXT("power_pocket_maximum_torso_vertical_cm"),
                        Thresholds.PowerPocketMaximumTorsoVerticalCm);
                    ThresholdJson->SetNumberField(
                        TEXT("release_minimum_reach_ratio"),
                        Thresholds.ReleaseMinimumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("release_maximum_reach_ratio"),
                        Thresholds.ReleaseMaximumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("release_minimum_elbow_angle_degrees"),
                        Thresholds.ReleaseMinimumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("release_maximum_elbow_angle_degrees"),
                        Thresholds.ReleaseMaximumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("followthrough_minimum_reach_ratio"),
                        Thresholds.FollowThroughMinimumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("followthrough_maximum_reach_ratio"),
                        Thresholds.FollowThroughMaximumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("followthrough_minimum_elbow_angle_degrees"),
                        Thresholds.FollowThroughMinimumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("followthrough_maximum_elbow_angle_degrees"),
                        Thresholds.FollowThroughMaximumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("recovery_maximum_reach_ratio"),
                        Thresholds.RecoveryMaximumReachRatio);
                    ThresholdJson->SetNumberField(
                        TEXT("recovery_minimum_elbow_angle_degrees"),
                        Thresholds.RecoveryMinimumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("recovery_maximum_elbow_angle_degrees"),
                        Thresholds.RecoveryMaximumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("recovery_maximum_absolute_torso_lateral_cm"),
                        Thresholds.RecoveryMaximumAbsoluteTorsoLateralCm);
                    ThresholdJson->SetNumberField(
                        TEXT("recovery_maximum_torso_vertical_cm"),
                        Thresholds.RecoveryMaximumTorsoVerticalCm);
                    FamilyJson->SetObjectField(
                        TEXT("v7_drive_arm_gate_thresholds"), ThresholdJson);
                }
                else if (Recipe.Version.SchemaVersion >= 6)
                {
                    const FArmSpatialGateThresholds& Thresholds =
                        V6ArmSpatialThresholds();
                    const TSharedRef<FJsonObject> ThresholdJson =
                        MakeShared<FJsonObject>();
                    ThresholdJson->SetNumberField(
                        TEXT("minimum_elbow_angle_degrees"),
                        Thresholds.MinimumElbowAngleDegrees);
                    ThresholdJson->SetNumberField(
                        TEXT("minimum_arm_extension_ratio"),
                        Thresholds.MinimumArmExtensionRatio);
                    ThresholdJson->SetBoolField(
                        TEXT("reachback_throwing_side_directional_dominance"),
                        true);
                    ThresholdJson->SetBoolField(
                        TEXT("followthrough_throwing_side_directional_dominance"),
                        true);
                    ThresholdJson->SetNumberField(
                        TEXT("minimum_reachback_radial_reach_cm"),
                        Thresholds.MinimumReachBackRadialReachCm);
                    ThresholdJson->SetNumberField(
                        TEXT("minimum_hand_vertical_cm"),
                        Thresholds.MinimumHandVerticalCm);
                    ThresholdJson->SetNumberField(
                        TEXT("minimum_release_follow_lateral_clearance_cm"),
                        Thresholds.MinimumReleaseFollowLateralClearanceCm);
                    ThresholdJson->SetNumberField(
                        TEXT("maximum_direction_frame_delta_degrees"),
                        Thresholds.MaximumDirectionFrameDeltaDegrees);
                    FamilyJson->SetObjectField(
                        TEXT("arm_spatial_gate_thresholds"), ThresholdJson);
                }
            }
        }
        FamilyJson->SetBoolField(TEXT("human_animation_approval"), false);
        FamilyJson->SetBoolField(TEXT("human_disc_contact_approval"), false);
        FamiliesJson.Add(MakeShared<FJsonValueObject>(FamilyJson));
    }
    UDiscGolfAnimationLibrary* Library =
        LoadObject<UDiscGolfAnimationLibrary>(nullptr, *Version.LibraryPath);
    if (!ValidateLibrary(Recipe, Library, Montages, Error))
    {
        return FailureJson(Error);
    }
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"),
        Recipe.Version.SchemaVersion >= 7
            ? TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v7")
            : Recipe.Version.SchemaVersion >= 6
            ? TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v6")
            : Recipe.Version.SchemaVersion >= 5
            ? TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v5")
            : Recipe.Version.SchemaVersion >= 4
            ? TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v4")
            : Recipe.Version.SchemaVersion >= 3
                ? TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v3")
                : TEXT("DiscGolfTour.Session19ProductionMotionNativeReport.v2"));
    Root->SetStringField(TEXT("status"),
        TEXT("PASS_PROJECT_AUTHORED_PROCEDURAL_CANDIDATES"));
    Root->SetStringField(TEXT("recipe_version"), Version.Version);
    Root->SetStringField(TEXT("asset_revision"), Version.AssetRevision);
    Root->SetStringField(TEXT("recipe_id"), Version.RecipeId);
    Root->SetStringField(TEXT("recipe_md5"), Recipe.SourceTextMd5);
    Root->SetStringField(TEXT("source_kind"), TEXT("PROJECT_AUTHORED_PROCEDURAL"));
    Root->SetBoolField(TEXT("derived_from_synthetic_fixture"), false);
    Root->SetBoolField(TEXT("external_performance_claim"), false);
    Root->SetNumberField(TEXT("family_count"), Recipe.Families.Num());
    Root->SetNumberField(TEXT("sequence_count"), Recipe.Families.Num());
    Root->SetNumberField(TEXT("montage_count"), Montages.Num());
    Root->SetNumberField(TEXT("library_entry_count"), Library->Entries.Num());
    Root->SetArrayField(TEXT("families"), FamiliesJson);
    Root->SetBoolField(TEXT("runtime_gameplay_release_authority"), false);
    Root->SetStringField(TEXT("world_motion_authority"),
        TEXT("GAMEPLAY_PAWN_CAPSULE"));
    Root->SetBoolField(TEXT("presentation_root_trajectory_enabled"),
        Recipe.bPresentationRootTrajectoryEnabled);
    Root->SetStringField(TEXT("interpolation_mode"), Recipe.InterpolationMode);
    if (Recipe.Version.SchemaVersion >= 3)
    {
        Root->SetStringField(TEXT("biomechanical_model"),
            Recipe.BiomechanicalModel);
        Root->SetStringField(TEXT("disc_contact_model"),
            Recipe.DiscContactModel);
        Root->SetStringField(TEXT("recovery_model"), Recipe.RecoveryModel);
    }
    if (Recipe.Version.SchemaVersion >= 4)
    {
        Root->SetStringField(TEXT("rotation_space"), Recipe.RotationSpace);
        Root->SetStringField(TEXT("local_track_construction"),
            Recipe.Version.SchemaVersion >= 5
                ? TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_BONE_TRACKS")
                : TEXT("COMPONENT_INTENT_CONVERTED_TO_LOCAL_BONE_TRACKS"));
    }
    if (Recipe.Version.SchemaVersion >= 5)
    {
        Root->SetStringField(TEXT("root_track_policy"), Recipe.RootTrackPolicy);
        Root->SetNumberField(TEXT("component_rotation_bone_count"),
            Recipe.ComponentRotationBones.Num());
        Root->SetNumberField(TEXT("local_rotation_bone_count"),
            Recipe.LocalRotationBones.Num());
    }
    Root->SetBoolField(TEXT("human_animation_approval"), false);
    Root->SetBoolField(TEXT("human_disc_contact_approval"), false);
    Root->SetBoolField(TEXT("release_ready"), false);
    return JsonString(Root);
}
}

FString UDiscGolfProductionMotionAuthoringUtility::AuthorProductionMotionAssets()
{
    return AuthorProductionMotionAssetsForVersion(
        DiscGolfProductionMotion::ActiveVersion);
}

FString UDiscGolfProductionMotionAuthoringUtility::
AuthorProductionMotionAssetsForVersion(const FString& RecipeVersion)
{
    using namespace DiscGolfProductionMotionAuthoring;
    FString Error;
    FRecipeVersionSpec Version;
    if (!ResolveRecipeVersion(RecipeVersion, Version, Error))
    {
        return FailureJson(Error);
    }
    if (!GuardCommandLine(Version, Error)) return FailureJson(Error);
    FRecipe Recipe;
    if (!LoadRecipe(Version, Recipe, Error)) return FailureJson(Error);
    for (const FFamilyRecipe& Family : Recipe.Families)
    {
        if (!RefuseTargetCollision(Family.SequencePath, Error)
            || !RefuseTargetCollision(Family.MontagePath, Error))
        {
            return FailureJson(Error);
        }
    }
    if (!RefuseTargetCollision(Version.LibraryPath, Error))
    {
        return FailureJson(Error);
    }
    USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, SkeletonPath);
    USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
    if (!Skeleton || !Mesh || Mesh->GetSkeleton() != Skeleton)
    {
        return FailureJson(TEXT("DGMaster skeleton/mesh did not load as one authoring pair"));
    }
    TArray<FString> CreatedFiles;
    TArray<UAnimMontage*> Montages;
    for (int32 FamilyIndex = 0; FamilyIndex < Recipe.Families.Num(); ++FamilyIndex)
    {
        const FFamilyRecipe& Family = Recipe.Families[FamilyIndex];
        UAnimSequence* Sequence = nullptr;
        UAnimMontage* Montage = nullptr;
        if (!AuthorSequence(Recipe, Family, Skeleton, Mesh, Sequence,
                CreatedFiles, Error)
            || !AuthorMontage(Recipe, Family, FamilyIndex, Sequence, Skeleton,
                Mesh, Montage, CreatedFiles, Error))
        {
            return FailureJson(Error, true, RollbackCreatedFiles(CreatedFiles));
        }
        Montages.Add(Montage);
    }
    UDiscGolfAnimationLibrary* Library = nullptr;
    if (!AuthorLibrary(Recipe, Montages, Library, CreatedFiles, Error))
    {
        return FailureJson(Error, true, RollbackCreatedFiles(CreatedFiles));
    }
    const FString Validation = BuildValidationReport(Version);
    TSharedPtr<FJsonObject> ValidationObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Validation);
    if (!FJsonSerializer::Deserialize(Reader, ValidationObject)
        || !ValidationObject.IsValid()
        || !ValidationObject->GetStringField(TEXT("status")).StartsWith(TEXT("PASS")))
    {
        Error = TEXT("Post-author validation failed: ") + Validation;
        return FailureJson(Error, true, RollbackCreatedFiles(CreatedFiles));
    }
    ValidationObject->SetStringField(TEXT("authoring_status"),
        TEXT("AUTHORED_SEVEN_FRESH_ASSETS"));
    ValidationObject->SetNumberField(TEXT("created_asset_count"), CreatedFiles.Num());
    TArray<TSharedPtr<FJsonValue>> FilesJson;
    for (const FString& Filename : CreatedFiles)
    {
        FilesJson.Add(MakeShared<FJsonValueString>(Filename));
    }
    ValidationObject->SetArrayField(TEXT("created_files"), FilesJson);
    ValidationObject->SetBoolField(TEXT("rollback_partial_writes"), true);
    return JsonString(ValidationObject.ToSharedRef());
}

FString UDiscGolfProductionMotionAuthoringUtility::ValidateProductionMotionAssets()
{
    return ValidateProductionMotionAssetsForVersion(
        DiscGolfProductionMotion::ActiveVersion);
}

FString UDiscGolfProductionMotionAuthoringUtility::
ValidateProductionMotionAssetsForVersion(const FString& RecipeVersion)
{
    using namespace DiscGolfProductionMotionAuthoring;
    FString Error;
    FRecipeVersionSpec Version;
    if (!ResolveRecipeVersion(RecipeVersion, Version, Error))
    {
        return FailureJson(Error);
    }
    return BuildValidationReport(Version);
}
