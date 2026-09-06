#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_RetargetPoseFromMesh.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfMetaHumanRetargetAnimInstance.generated.h"

class UIKRetargeter;
class USkeletalMeshComponent;

namespace DiscGolfMetaHumanPresentation
{
inline constexpr const TCHAR* HandCorrectionMode =
    TEXT("TARGET_RIGHT_ARM_LENGTH_PRESERVING_TWO_BONE_TO_SOURCE_GRIP_TRANSFORM");
inline constexpr const TCHAR* TargetSocketGripBindingMode =
    TEXT("TARGET_SOCKET");
inline constexpr const TCHAR* SourceGripRelativeFallbackBindingMode =
    TEXT("SOURCE_GRIP_RELATIVE_FALLBACK");

/** Pure presentation math shared by the runtime correction and automation tests. */
DISCGOLFTOUR_API float ComputeCorrectionBlendAlpha(
    float PositionErrorCm,
    float OrientationErrorDegrees);

/** Phase-local authority for the cosmetic grip-transform correction. */
DISCGOLFTOUR_API float PhaseCorrectionWeight(EDGThrowPhase Phase);

/** Frame-rate-independent continuity filter for phase correction changes. */
DISCGOLFTOUR_API float SmoothCorrectionWeight(
    float CurrentWeight,
    float TargetWeight,
    float DeltaSeconds);

/** Clamped, normalized hand-orientation blend shared by runtime and tests. */
DISCGOLFTOUR_API FQuat BlendHandOrientation(
    const FQuat& CurrentOrientation,
    const FQuat& DesiredOrientation,
    float BlendAlpha);

/** Selects target-safe hand orientation without copying incompatible source axes. */
DISCGOLFTOUR_API FQuat SelectAnatomicalHandOrientation(
    const FQuat& RetargetedTargetOrientation,
    const FQuat& GripBoundDesiredOrientation,
    bool bSourceGripRelativeFallback);

/** Carries the hand with a solved forearm while preserving their relative orientation. */
DISCGOLFTOUR_API FQuat CarryHandOrientationWithSolvedForearm(
    const FQuat& OriginalForearmOrientation,
    const FQuat& OriginalHandOrientation,
    const FQuat& SolvedForearmOrientation);

/**
 * Solves the hand transform whose authored hand-to-grip binding places the
 * grip at DesiredGripWorld. This is presentation algebra only; it never adds
 * sockets or mutates either skeleton.
 */
DISCGOLFTOUR_API FTransform BuildDesiredHandWorldFromGripBinding(
    const FTransform& DesiredGripWorld,
    const FTransform& BindingHandWorld,
    const FTransform& BindingGripWorld);

DISCGOLFTOUR_API FVector ClampEffectorTarget(
    const FVector& ShoulderLocation,
    const FVector& CurrentHandLocation,
    const FVector& DesiredHandLocation,
    float MinimumReachCm,
    float MaximumReachCm);

DISCGOLFTOUR_API FVector BuildStableElbowPole(
    const FVector& ShoulderLocation,
    const FVector& CurrentElbowLocation,
    const FVector& DesiredHandLocation,
    const FVector& FallbackBendDirection,
    float UpperLimbLengthCm);

/**
 * Uses the authored source shoulder-to-elbow direction to select the target
 * arm's bend plane while retaining target limb lengths. This prevents the
 * large cosmetic wrist translation from choosing an inward/upward elbow flip.
 */
DISCGOLFTOUR_API FVector BuildSourceGuidedElbowPole(
    const FVector& TargetShoulderLocation,
    const FVector& CurrentTargetElbowLocation,
    const FVector& DesiredTargetHandLocation,
    const FVector& SourceShoulderLocation,
    const FVector& SourceElbowLocation,
    const FVector& FallbackBendDirection,
    float TargetUpperLimbLengthCm);

/**
 * Blends the hand target before solving the arm, then projects that target back
 * into the two-bone reach annulus. This avoids blending component-space joint
 * translations after a solve, which does not preserve limb lengths.
 */
DISCGOLFTOUR_API FVector BuildLengthPreservingEffectorTarget(
    const FVector& ShoulderLocation,
    const FVector& CurrentHandLocation,
    const FVector& BoundedDesiredHandLocation,
    float BlendAlpha,
    float MinimumReachCm,
    float MaximumReachCm);

/** Source-arm extension fraction using source-chain points only. */
DISCGOLFTOUR_API float ComputeSourceArmReachFraction(
    const FVector& SourceShoulderLocation,
    const FVector& SourceElbowLocation,
    const FVector& SourceHandLocation);

/** Monotonic shoulder relocation blend shared by runtime and tests. */
DISCGOLFTOUR_API FVector BlendShoulderRelocation(
    const FVector& CurrentShoulderLocation,
    const FVector& RelocatedShoulderLocation,
    float BlendAlpha);

/** Relative length error for one corrected skeletal segment. */
DISCGOLFTOUR_API float ComputeSegmentLengthRatioError(
    const FVector& OriginalStart,
    const FVector& OriginalEnd,
    const FVector& CorrectedStart,
    const FVector& CorrectedEnd);
}

struct DISCGOLFTOUR_API FDiscGolfMetaHumanHandCorrectionEvidence
{
    FString CorrectionMode = TEXT("INACTIVE");
    FString GripBindingMode = TEXT("INACTIVE");
    FVector SourceHandWorldLocation = FVector::ZeroVector;
    FVector SourceDiscGripWorldLocation = FVector::ZeroVector;
    FVector DesiredMetaHumanHandWorldLocation = FVector::ZeroVector;
    FVector MetaHumanHandPreCorrectionWorldLocation = FVector::ZeroVector;
    FVector BoundedEffectorTargetWorldLocation = FVector::ZeroVector;
    FVector MetaHumanHandPostCorrectionWorldLocation = FVector::ZeroVector;
    float SourceHandToDiscGripDistanceCm = -1.0f;
    float MetaHumanHandPreToSourceHandDistanceCm = -1.0f;
    float MetaHumanHandPostToSourceHandDistanceCm = -1.0f;
    float MetaHumanHandPreToDesiredHandDistanceCm = -1.0f;
    float MetaHumanHandPostToDesiredHandDistanceCm = -1.0f;
    float RequestedEffectorCorrectionCm = -1.0f;
    float BoundedEffectorCorrectionCm = -1.0f;
    float EffectorCorrectionCapCm = -1.0f;
    float DesiredHandOrientationErrorDegrees = -1.0f;
    float MetaHumanHandPostToDesiredHandOrientationErrorDegrees = -1.0f;
    float PhaseCorrectionWeight = 0.0f;
    float AppliedCorrectionBlendAlpha = 0.0f;
    float UpperArmToLowerArmRatioError = -1.0f;
    float LowerArmToHandRatioError = -1.0f;
    float MaximumCorrectedSegmentRatioError = -1.0f;
    uint32 SourceBoneRevisionAtPreUpdate = 0;
    uint32 TargetBoneRevisionBeforeEvaluate = 0;
    uint64 SourceSampleFrameCounter = 0;
    uint64 TargetCorrectionFrameCounter = 0;
    bool bSnapshotValid = false;
    bool bFullGripTransformAvailable = false;
    bool bSourceGripRelativeFallback = false;
    bool bFallbackHandOrientationCarriedWithSolvedForearm = false;
    bool bSourceGuidedElbowPole = false;
    bool bEffectorCorrectionClamped = false;
    bool bEffectorDistanceCapExceeded = false;
    bool bDesiredTargetWithinReachAnnulus = false;
    bool bReachable = false;
    bool bApplied = false;
};

USTRUCT()
struct FDiscGolfMetaHumanRetargetAnimInstanceProxy : public FAnimInstanceProxy
{
    GENERATED_BODY()

    FDiscGolfMetaHumanRetargetAnimInstanceProxy() = default;
    FDiscGolfMetaHumanRetargetAnimInstanceProxy(
        UAnimInstance* InAnimInstance,
        FAnimNode_RetargetPoseFromMesh* InRetargetNode);

    virtual void Initialize(UAnimInstance* InAnimInstance) override;
    virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
    virtual void CacheBones() override;
    virtual bool Evaluate(FPoseContext& Output) override;
    virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;

    FAnimNode_RetargetPoseFromMesh* RetargetNode = nullptr;
    FDiscGolfMetaHumanHandCorrectionEvidence HandCorrectionEvidence;

private:
    FTransform TargetComponentToWorldAtPreUpdate = FTransform::Identity;
    FTransform DesiredHandComponentTransform = FTransform::Identity;
    FVector SourceShoulderComponentLocation = FVector::ZeroVector;
    FVector SourceElbowComponentLocation = FVector::ZeroVector;
    FVector SourceHandComponentLocation = FVector::ZeroVector;
    bool bSourceArmGuideValid = false;
    float SmoothedPhaseCorrectionWeight = 0.0f;
};

/**
 * Native, presentation-only runtime retarget instance. The evaluated DG master
 * mesh remains the source of truth; this instance only copies its current pose
 * through one explicitly verified IK Retargeter.
 */
UCLASS(Transient, NotBlueprintable)
class DISCGOLFTOUR_API UDiscGolfMetaHumanRetargetAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UDiscGolfMetaHumanRetargetAnimInstance();

    bool ConfigureAndVerify(
        UIKRetargeter* Retargeter,
        USkeletalMeshComponent* SourceMesh,
        FString& OutStatus);

    bool IsConfiguredFor(
        const UIKRetargeter* Retargeter,
        const USkeletalMeshComponent* SourceMesh) const;

    bool GetPresentationHandCorrectionEvidence(
        FDiscGolfMetaHumanHandCorrectionEvidence& OutEvidence) const;

protected:
    virtual void NativeInitializeAnimation() override;
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
    UPROPERTY(Transient)
    FAnimNode_RetargetPoseFromMesh RetargetNode;

    UPROPERTY(Transient)
    TObjectPtr<UIKRetargeter> VerifiedRetargeter;

    TWeakObjectPtr<USkeletalMeshComponent> VerifiedSourceMesh;
};
