#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "Units/RigUnit.h"
#include "DiscGolfCharacterRigUnits.generated.h"

/**
 * Small additive pose controls layered over the authored montage before PBIK.
 * Values are presentation-only and deliberately use degrees/centimetres rather
 * than any gameplay release or solver units.
 */
USTRUCT()
struct DISCGOLFTOUR_API FDGThrowMicroMotionPose
{
    GENERATED_BODY()

    UPROPERTY()
    float PelvisCounterYawDegrees = 0.0f;

    UPROPERTY()
    float TorsoCoilYawDegrees = 0.0f;

    UPROPERTY()
    float TorsoForwardPitchDegrees = 0.0f;

    UPROPERTY()
    float ShoulderLeadDegrees = 0.0f;

    UPROPERTY()
    float ElbowLeadDegrees = 0.0f;

    UPROPERTY()
    float WristLagDegrees = 0.0f;

    UPROPERTY()
    float WristRollDegrees = 0.0f;

    UPROPERTY()
    float WristPitchDegrees = 0.0f;

    UPROPERTY()
    float BraceCompressionAlpha = 0.0f;

    UPROPERTY()
    float PlantFootSettleCm = 0.0f;

    UPROPERTY()
    float GripAlpha = 0.0f;

    UPROPERTY()
    float OffArmBalanceAlpha = 0.0f;

    UPROPERTY()
    float GazeStabilizationAlpha = 0.0f;

    UPROPERTY()
    float DiscPlaneStabilityAlpha = 0.0f;

    UPROPERTY()
    float RecoveryRelaxAlpha = 0.0f;

    UPROPERTY()
    float WaggleAlpha = 0.0f;
};

/**
 * Bounded PBIK policy for the two hand effectors. Production full-body motion
 * already owns the arm trajectory, so its IK is only allowed to make a small
 * orientation/contact correction instead of replacing the authored pose.
 */
struct DISCGOLFTOUR_API FDGUpperLimbIKPolicy
{
    float ThrowHandAlpha = 0.0f;
    float SupportHandAlpha = 0.0f;
    float SupportTargetBlendAlpha = 0.0f;
    bool bPreserveAuthoredThrowTrajectory = false;
    bool bUseSyntheticFollowTarget = false;
};

namespace DiscGolfCharacterRigPresentation
{
/** Smooth, bounded plant weighting for presentation-only foot targets. */
DISCGOLFTOUR_API float PlantLockBlendAlpha(float PlantCurveAlpha);

DISCGOLFTOUR_API FTransform BlendPlantTarget(
    const FTransform& AnimatedTarget,
    const FTransform& LockedTarget,
    float PlantCurveAlpha);

/** Frame-rate-independent blend amount selected by the current authored phase. */
DISCGOLFTOUR_API float PhaseLocalBlendAlpha(
    EDGThrowPhase Phase,
    float DeltaSeconds);

/** Builds one finite, bounded additive target from cosmetic intent/profile data. */
DISCGOLFTOUR_API FDGThrowMicroMotionPose BuildMicroMotionTarget(
    EDGThrowPhase Phase,
    const FDGThrowIntent& Intent,
    const FDGThrowStyle& Style,
    const FDGBodyProfile& Body,
    EDGHandedness Handedness);

/** Component-wise finite blend used for phase transitions and live setup edits. */
DISCGOLFTOUR_API FDGThrowMicroMotionPose BlendMicroMotionPose(
    const FDGThrowMicroMotionPose& Current,
    const FDGThrowMicroMotionPose& Target,
    float Alpha);

DISCGOLFTOUR_API bool IsMicroMotionPoseFiniteAndBounded(
    const FDGThrowMicroMotionPose& Pose);

/** V3+ full-body recipes carry this non-zero curve for their entire lifetime. */
DISCGOLFTOUR_API bool HasProductionFullBodyMotionSignature(
    float AuthoredGazeCurveAlpha);

/** Pure, testable hand-effector policy; legacy montage behavior is preserved. */
DISCGOLFTOUR_API FDGUpperLimbIKPolicy BuildUpperLimbIKPolicy(
    bool bProductionFullBodyMotion,
    EDGThrowPhase Phase,
    const FDGThrowMicroMotionPose& MicroMotion,
    float ReachStyleContribution);
}

/**
 * Persistent, presentation-only state used by the Session 4 proportion unit.
 * It deliberately contains no throw or disc-flight authority.
 */
USTRUCT()
struct FDGCharacterProfileRigWorkData
{
    GENERATED_BODY()

    UPROPERTY()
    bool bLeftFootLocked = false;

    UPROPERTY()
    bool bRightFootLocked = false;

    UPROPERTY()
    FTransform LeftFootLock = FTransform::Identity;

    UPROPERTY()
    FTransform RightFootLock = FTransform::Identity;

    UPROPERTY()
    double LastAbsoluteTime = -1.0;

    UPROPERTY()
    FDGThrowMicroMotionPose SmoothedMicroMotion;

    UPROPERTY()
    EDGThrowPhase LastThrowPhase = EDGThrowPhase::Idle;

    UPROPERTY()
    bool bMicroMotionInitialized = false;
};

/**
 * Applies bounded body proportions and restrained throw-style presentation to
 * the pose already transferred into CR_DG_Master. The accepted PBIK unit runs
 * after this node and remains responsible for final limb correction.
 *
 * This unit may move the visual skeleton and its IK targets. It never changes
 * montage time, animation notifies, the throw command, or disc-flight values.
 */
USTRUCT(BlueprintType, meta=(DisplayName="DG Apply Character Profile", Category="Disc Golf|Character", Keywords="Body Profile Proportion Throw Style", NodeColor="0.10 0.38 0.52"))
struct DISCGOLFTOUR_API FRigUnit_DGApplyCharacterProfile : public FRigUnitMutable
{
    GENERATED_BODY()

    FRigUnit_DGApplyCharacterProfile();

    RIGVM_METHOD()
    virtual void Execute() override;

    UPROPERTY(meta=(Input))
    FDGBodyProfile BodyProfile;

    UPROPERTY(meta=(Input))
    FDGThrowStyle ThrowStyle;

    UPROPERTY(meta=(Input))
    FDGThrowIntent ThrowIntent;

    UPROPERTY(meta=(Input))
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(meta=(Input))
    EDGThrowPhase ThrowPhase = EDGThrowPhase::Idle;

    UPROPERTY(meta=(Input))
    bool bThrowActive = false;

    UPROPERTY(meta=(Output))
    bool bApplied = false;

    UPROPERTY(meta=(Output))
    bool bInputsClamped = false;

    UPROPERTY(meta=(Output))
    float HeightScale = 1.0f;

    UPROPERTY(meta=(Output))
    float ArmLengthScale = 1.0f;

    UPROPERTY(meta=(Output))
    float LegLengthScale = 1.0f;

    UPROPERTY(Transient)
    FDGCharacterProfileRigWorkData WorkData;
};
