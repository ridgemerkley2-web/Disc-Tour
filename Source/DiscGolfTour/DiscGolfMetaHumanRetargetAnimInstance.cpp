#include "DiscGolfMetaHumanRetargetAnimInstance.h"

#include "DiscGolfThrowComponent.h"

#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/IKRetargetProcessor.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/CurveRemapOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "TwoBoneIK.h"

namespace
{
constexpr double MinimumLimbLengthCm = 0.01;
// The v007 release proximal chain proves a reachable authored grip can be
// 66.501 cm from the raw retargeted MetaHuman hand even though the requested
// hand remains inside the target arm's safe reach annulus. This
// presentation-only envelope covers that retarget/body-proportion delta; the
// reach annulus and post-solve 0.5% segment gate below remain the anatomical
// authority and prevent stretching.
constexpr float MaximumEffectorCorrectionCm = 80.0f;
constexpr float ReachSafetyMarginCm = 0.05f;
constexpr float ConstraintToleranceCm = 0.001f;
constexpr float MaximumCorrectedSegmentRatioError = 0.005f;

bool IsFiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN()
        && FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

FVector ClampEffectorTargetWithCap(
    const FVector& ShoulderLocation,
    const FVector& CurrentHandLocation,
    const FVector& DesiredHandLocation,
    float MinimumReachCm,
    float MaximumReachCm,
    float MaximumCorrectionCm)
{
    if (!IsFiniteVector(ShoulderLocation) || !IsFiniteVector(CurrentHandLocation)
        || !IsFiniteVector(DesiredHandLocation) || !FMath::IsFinite(MinimumReachCm)
        || !FMath::IsFinite(MaximumReachCm)
        || !FMath::IsFinite(MaximumCorrectionCm)
        || MaximumReachCm <= 0.0f || MaximumCorrectionCm <= 0.0f)
    {
        return CurrentHandLocation;
    }

    const float SafeMinimum = FMath::Max(
        0.0f, MinimumReachCm + ReachSafetyMarginCm);
    const float SafeMaximum = FMath::Max(
        SafeMinimum, MaximumReachCm - ReachSafetyMarginCm);
    auto IsFeasible = [
        &ShoulderLocation,
        &CurrentHandLocation,
        SafeMinimum,
        SafeMaximum,
        MaximumCorrectionCm](const FVector& Candidate)
    {
        if (!IsFiniteVector(Candidate))
        {
            return false;
        }
        const float Reach = FVector::Distance(
            ShoulderLocation, Candidate);
        const float Correction = FVector::Distance(
            CurrentHandLocation, Candidate);
        return Reach >= SafeMinimum - ConstraintToleranceCm
            && Reach <= SafeMaximum + ConstraintToleranceCm
            && Correction
                <= MaximumCorrectionCm + ConstraintToleranceCm;
    };

    if (IsFeasible(DesiredHandLocation))
    {
        return DesiredHandLocation;
    }

    FVector BestCandidate = CurrentHandLocation;
    float BestDistanceToDesired = TNumericLimits<float>::Max();
    bool bFoundCandidate = false;
    auto ConsiderCandidate = [
        &DesiredHandLocation,
        &IsFeasible,
        &BestCandidate,
        &BestDistanceToDesired,
        &bFoundCandidate](const FVector& Candidate)
    {
        if (!IsFeasible(Candidate))
        {
            return;
        }
        const float DistanceToDesired = FVector::DistSquared(
            Candidate, DesiredHandLocation);
        if (!bFoundCandidate || DistanceToDesired < BestDistanceToDesired)
        {
            BestCandidate = Candidate;
            BestDistanceToDesired = DistanceToDesired;
            bFoundCandidate = true;
        }
    };

    // One active constraint: correction sphere only.
    const FVector DesiredCorrection =
        DesiredHandLocation - CurrentHandLocation;
    if (!DesiredCorrection.IsNearlyZero())
    {
        ConsiderCandidate(CurrentHandLocation
            + DesiredCorrection.GetClampedToMaxSize(MaximumCorrectionCm));
    }

    // One active constraint: either boundary of the reach annulus only.
    FVector DesiredDirection =
        (DesiredHandLocation - ShoulderLocation).GetSafeNormal();
    if (DesiredDirection.IsNearlyZero())
    {
        DesiredDirection =
            (CurrentHandLocation - ShoulderLocation).GetSafeNormal();
    }
    if (!DesiredDirection.IsNearlyZero())
    {
        const float DesiredReach = FVector::Distance(
            ShoulderLocation, DesiredHandLocation);
        ConsiderCandidate(ShoulderLocation + DesiredDirection
            * FMath::Clamp(DesiredReach, SafeMinimum, SafeMaximum));
    }

    // Two active constraints: choose the closest-to-desired point on each
    // reach-sphere/correction-sphere intersection circle. This closes the
    // cap-first-then-annulus hole where the second projection could move the
    // hand back outside the correction sphere.
    const FVector ShoulderToCurrent =
        CurrentHandLocation - ShoulderLocation;
    const float CenterDistance = ShoulderToCurrent.Size();
    auto ConsiderSphereIntersection = [
        &ShoulderLocation,
        &CurrentHandLocation,
        &DesiredHandLocation,
        &ShoulderToCurrent,
        CenterDistance,
        MaximumCorrectionCm,
        &ConsiderCandidate](float ReachRadius)
    {
        if (CenterDistance <= UE_KINDA_SMALL_NUMBER)
        {
            if (ReachRadius <= MaximumCorrectionCm + ConstraintToleranceCm)
            {
                FVector Direction =
                    (DesiredHandLocation - ShoulderLocation).GetSafeNormal();
                if (Direction.IsNearlyZero())
                {
                    Direction = FVector::ForwardVector;
                }
                ConsiderCandidate(
                    ShoulderLocation + Direction * ReachRadius);
            }
            return;
        }
        if (CenterDistance
                > ReachRadius + MaximumCorrectionCm
                    + ConstraintToleranceCm
            || CenterDistance
                < FMath::Abs(ReachRadius - MaximumCorrectionCm)
                    - ConstraintToleranceCm)
        {
            return;
        }

        const FVector Axis = ShoulderToCurrent / CenterDistance;
        const float AlongAxis = (
            FMath::Square(ReachRadius)
            - FMath::Square(MaximumCorrectionCm)
            + FMath::Square(CenterDistance))
            / (2.0f * CenterDistance);
        const float CircleRadiusSquared = FMath::Max(
            0.0f,
            FMath::Square(ReachRadius) - FMath::Square(AlongAxis));
        const float CircleRadius = FMath::Sqrt(CircleRadiusSquared);
        const FVector CircleCenter =
            ShoulderLocation + Axis * AlongAxis;
        FVector Perpendicular = DesiredHandLocation - CircleCenter;
        Perpendicular -= Axis * FVector::DotProduct(Perpendicular, Axis);
        if (Perpendicular.IsNearlyZero())
        {
            Perpendicular = FVector::CrossProduct(
                Axis, FVector::UpVector);
            if (Perpendicular.IsNearlyZero())
            {
                Perpendicular = FVector::CrossProduct(
                    Axis, FVector::RightVector);
            }
        }
        ConsiderCandidate(CircleCenter
            + Perpendicular.GetSafeNormal() * CircleRadius);
    };
    ConsiderSphereIntersection(SafeMinimum);
    if (!FMath::IsNearlyEqual(SafeMaximum, SafeMinimum))
    {
        ConsiderSphereIntersection(SafeMaximum);
    }
    ConsiderCandidate(CurrentHandLocation);
    return bFoundCandidate ? BestCandidate : CurrentHandLocation;
}

bool GetUniformWorldScale(
    const FTransform& ComponentToWorld,
    float& OutUniformScale)
{
    const FVector Scale = ComponentToWorld.GetScale3D().GetAbs();
    if (!IsFiniteVector(Scale) || Scale.GetMin() <= UE_KINDA_SMALL_NUMBER
        || Scale.GetMax() - Scale.GetMin() > 0.001f)
    {
        OutUniformScale = 0.0f;
        return false;
    }
    OutUniformScale = (Scale.X + Scale.Y + Scale.Z) / 3.0f;
    return FMath::IsFinite(OutUniformScale)
        && OutUniformScale > UE_KINDA_SMALL_NUMBER;
}

}

namespace DiscGolfMetaHumanPresentation
{
float ComputeCorrectionBlendAlpha(
    float PositionErrorCm,
    float OrientationErrorDegrees)
{
    if (!FMath::IsFinite(PositionErrorCm)
        || !FMath::IsFinite(OrientationErrorDegrees))
    {
        return 0.0f;
    }
    const float PositionWeight = FMath::Clamp(PositionErrorCm / 8.0f, 0.0f, 1.0f);
    const float OrientationWeight = FMath::Clamp(OrientationErrorDegrees / 30.0f, 0.0f, 1.0f);
    const float Need = FMath::Max(PositionWeight, OrientationWeight);
    return Need <= UE_KINDA_SMALL_NUMBER
        ? 0.0f
        : FMath::Clamp(
            0.20f + 0.80f * FMath::SmoothStep(0.0f, 1.0f, Need),
            0.0f,
            1.0f);
}

float PhaseCorrectionWeight(EDGThrowPhase Phase)
{
    switch (Phase)
    {
        // The disc remains in the throwing hand from setup through release.
        // Pre-warm full cosmetic grip authority before ReachBack so the
        // continuity filter is settled by the exact proof frame.
        case EDGThrowPhase::Aim:
        case EDGThrowPhase::RunUp:
        case EDGThrowPhase::ReachBack:
        case EDGThrowPhase::Plant:
        case EDGThrowPhase::Acceleration:
        case EDGThrowPhase::Release:
            return 1.0f;
        case EDGThrowPhase::FollowThrough: return 0.65f;
        case EDGThrowPhase::Recovery: return 0.22f;
        case EDGThrowPhase::Idle:
        default:
            return 0.0f;
    }
}

float SmoothCorrectionWeight(
    float CurrentWeight,
    float TargetWeight,
    float DeltaSeconds)
{
    const float SafeCurrent = FMath::Clamp(
        FMath::IsFinite(CurrentWeight) ? CurrentWeight : 0.0f, 0.0f, 1.0f);
    const float SafeTarget = FMath::Clamp(
        FMath::IsFinite(TargetWeight) ? TargetWeight : 0.0f, 0.0f, 1.0f);
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return SafeCurrent;
    }
    const float HalfLifeSeconds = SafeTarget > SafeCurrent ? 0.045f : 0.11f;
    const float SafeDelta = FMath::Min(DeltaSeconds, 0.10f);
    const float Alpha = FMath::Clamp(
        1.0f - FMath::Exp2(-SafeDelta / HalfLifeSeconds), 0.0f, 1.0f);
    return FMath::Lerp(SafeCurrent, SafeTarget, Alpha);
}

FQuat BlendHandOrientation(
    const FQuat& CurrentOrientation,
    const FQuat& DesiredOrientation,
    float BlendAlpha)
{
    if (CurrentOrientation.ContainsNaN() || DesiredOrientation.ContainsNaN())
    {
        return FQuat::Identity;
    }
    const float SafeAlpha = FMath::Clamp(
        FMath::IsFinite(BlendAlpha) ? BlendAlpha : 0.0f,
        0.0f,
        1.0f);
    return FQuat::Slerp(
        CurrentOrientation.GetNormalized(),
        DesiredOrientation.GetNormalized(),
        SafeAlpha).GetNormalized();
}

FQuat SelectAnatomicalHandOrientation(
    const FQuat& RetargetedTargetOrientation,
    const FQuat& GripBoundDesiredOrientation,
    bool bSourceGripRelativeFallback)
{
    if (RetargetedTargetOrientation.ContainsNaN()
        || GripBoundDesiredOrientation.ContainsNaN())
    {
        return FQuat::Identity;
    }
    return (bSourceGripRelativeFallback
        ? RetargetedTargetOrientation
        : GripBoundDesiredOrientation).GetNormalized();
}

FQuat CarryHandOrientationWithSolvedForearm(
    const FQuat& OriginalForearmOrientation,
    const FQuat& OriginalHandOrientation,
    const FQuat& SolvedForearmOrientation)
{
    auto IsUsableOrientation = [](const FQuat& Orientation)
    {
        const double MagnitudeSquared = Orientation.SizeSquared();
        return !Orientation.ContainsNaN()
            && FMath::IsFinite(MagnitudeSquared)
            && MagnitudeSquared > UE_KINDA_SMALL_NUMBER;
    };
    if (!IsUsableOrientation(OriginalForearmOrientation)
        || !IsUsableOrientation(OriginalHandOrientation)
        || !IsUsableOrientation(SolvedForearmOrientation))
    {
        return FQuat::Identity;
    }

    const FQuat OriginalForearm =
        OriginalForearmOrientation.GetNormalized();
    const FQuat OriginalHand = OriginalHandOrientation.GetNormalized();
    const FQuat SolvedForearm = SolvedForearmOrientation.GetNormalized();
    const FQuat HandRelativeToForearm = (
        OriginalForearm.Inverse() * OriginalHand).GetNormalized();
    return (SolvedForearm * HandRelativeToForearm).GetNormalized();
}

FTransform BuildDesiredHandWorldFromGripBinding(
    const FTransform& DesiredGripWorld,
    const FTransform& BindingHandWorld,
    const FTransform& BindingGripWorld)
{
    if (!DesiredGripWorld.IsValid()
        || !BindingHandWorld.IsValid()
        || !BindingGripWorld.IsValid())
    {
        return BindingHandWorld.IsValid()
            ? BindingHandWorld
            : FTransform::Identity;
    }

    const FQuat BindingHandRotation = BindingHandWorld.GetRotation();
    const FQuat GripRelativeRotation = BindingHandRotation.Inverse()
        * BindingGripWorld.GetRotation();
    const FVector GripOffsetInHand = BindingHandRotation.Inverse().RotateVector(
        BindingGripWorld.GetLocation() - BindingHandWorld.GetLocation());
    const FQuat DesiredHandRotation = (DesiredGripWorld.GetRotation()
        * GripRelativeRotation.Inverse()).GetNormalized();
    return FTransform(
        DesiredHandRotation,
        DesiredGripWorld.GetLocation()
            - DesiredHandRotation.RotateVector(GripOffsetInHand),
        BindingHandWorld.GetScale3D());
}

FVector ClampEffectorTarget(
    const FVector& ShoulderLocation,
    const FVector& CurrentHandLocation,
    const FVector& DesiredHandLocation,
    float MinimumReachCm,
    float MaximumReachCm)
{
    return ClampEffectorTargetWithCap(
        ShoulderLocation,
        CurrentHandLocation,
        DesiredHandLocation,
        MinimumReachCm,
        MaximumReachCm,
        MaximumEffectorCorrectionCm);
}

FVector BuildStableElbowPole(
    const FVector& ShoulderLocation,
    const FVector& CurrentElbowLocation,
    const FVector& DesiredHandLocation,
    const FVector& FallbackBendDirection,
    float UpperLimbLengthCm)
{
    if (!IsFiniteVector(ShoulderLocation) || !IsFiniteVector(CurrentElbowLocation)
        || !IsFiniteVector(DesiredHandLocation) || !IsFiniteVector(FallbackBendDirection)
        || !FMath::IsFinite(UpperLimbLengthCm) || UpperLimbLengthCm <= 0.0f)
    {
        return CurrentElbowLocation;
    }
    const FVector Axis = (DesiredHandLocation - ShoulderLocation).GetSafeNormal();
    if (Axis.IsNearlyZero())
    {
        return CurrentElbowLocation;
    }
    const FVector ShoulderToElbow = CurrentElbowLocation - ShoulderLocation;
    const FVector OnAxis = ShoulderLocation + Axis * FVector::DotProduct(ShoulderToElbow, Axis);
    FVector BendDirection = (CurrentElbowLocation - OnAxis).GetSafeNormal();
    if (BendDirection.IsNearlyZero())
    {
        BendDirection = (FallbackBendDirection
            - Axis * FVector::DotProduct(FallbackBendDirection, Axis)).GetSafeNormal();
    }
    if (BendDirection.IsNearlyZero())
    {
        BendDirection = FVector::CrossProduct(
            Axis, FMath::Abs(Axis.Z) < 0.9f ? FVector::UpVector : FVector::RightVector).GetSafeNormal();
    }
    const float PoleOffset = FMath::Clamp(UpperLimbLengthCm * 0.65f, 5.0f, 45.0f);
    return OnAxis + BendDirection * PoleOffset;
}

FVector BuildSourceGuidedElbowPole(
    const FVector& TargetShoulderLocation,
    const FVector& CurrentTargetElbowLocation,
    const FVector& DesiredTargetHandLocation,
    const FVector& SourceShoulderLocation,
    const FVector& SourceElbowLocation,
    const FVector& FallbackBendDirection,
    float TargetUpperLimbLengthCm)
{
    if (!IsFiniteVector(SourceShoulderLocation)
        || !IsFiniteVector(SourceElbowLocation))
    {
        return BuildStableElbowPole(
            TargetShoulderLocation,
            CurrentTargetElbowLocation,
            DesiredTargetHandLocation,
            FallbackBendDirection,
            TargetUpperLimbLengthCm);
    }

    const FVector SourceUpperArmDirection =
        (SourceElbowLocation - SourceShoulderLocation).GetSafeNormal();
    if (SourceUpperArmDirection.IsNearlyZero())
    {
        return BuildStableElbowPole(
            TargetShoulderLocation,
            CurrentTargetElbowLocation,
            DesiredTargetHandLocation,
            FallbackBendDirection,
            TargetUpperLimbLengthCm);
    }

    const FVector SourceGuidedElbow = TargetShoulderLocation
        + SourceUpperArmDirection * TargetUpperLimbLengthCm;
    return BuildStableElbowPole(
        TargetShoulderLocation,
        SourceGuidedElbow,
        DesiredTargetHandLocation,
        FallbackBendDirection,
        TargetUpperLimbLengthCm);
}

FVector BuildLengthPreservingEffectorTarget(
    const FVector& ShoulderLocation,
    const FVector& CurrentHandLocation,
    const FVector& BoundedDesiredHandLocation,
    float BlendAlpha,
    float MinimumReachCm,
    float MaximumReachCm)
{
    if (!IsFiniteVector(ShoulderLocation)
        || !IsFiniteVector(CurrentHandLocation)
        || !IsFiniteVector(BoundedDesiredHandLocation)
        || !FMath::IsFinite(BlendAlpha))
    {
        return CurrentHandLocation;
    }

    const FVector IntermediateTarget = FMath::Lerp(
        CurrentHandLocation,
        BoundedDesiredHandLocation,
        FMath::Clamp(BlendAlpha, 0.0f, 1.0f));
    return ClampEffectorTarget(
        ShoulderLocation,
        CurrentHandLocation,
        IntermediateTarget,
        MinimumReachCm,
        MaximumReachCm);
}

float ComputeSourceArmReachFraction(
    const FVector& SourceShoulderLocation,
    const FVector& SourceElbowLocation,
    const FVector& SourceHandLocation)
{
    if (!IsFiniteVector(SourceShoulderLocation)
        || !IsFiniteVector(SourceElbowLocation)
        || !IsFiniteVector(SourceHandLocation))
    {
        return 0.0f;
    }
    const float UpperLength = FVector::Distance(
        SourceShoulderLocation, SourceElbowLocation);
    const float LowerLength = FVector::Distance(
        SourceElbowLocation, SourceHandLocation);
    const float MaximumReach = UpperLength + LowerLength;
    if (!FMath::IsFinite(MaximumReach)
        || MaximumReach <= MinimumLimbLengthCm)
    {
        return 0.0f;
    }
    return FMath::Clamp(
        FVector::Distance(SourceShoulderLocation, SourceHandLocation)
            / MaximumReach,
        0.0f,
        1.0f);
}

FVector BlendShoulderRelocation(
    const FVector& CurrentShoulderLocation,
    const FVector& RelocatedShoulderLocation,
    float BlendAlpha)
{
    if (!IsFiniteVector(CurrentShoulderLocation)
        || !IsFiniteVector(RelocatedShoulderLocation)
        || !FMath::IsFinite(BlendAlpha))
    {
        return CurrentShoulderLocation;
    }
    return FMath::Lerp(
        CurrentShoulderLocation,
        RelocatedShoulderLocation,
        FMath::Clamp(BlendAlpha, 0.0f, 1.0f));
}

FVector BuildClavicleAwareShoulderTarget(
    const FVector& ClavicleLocation,
    const FVector& CurrentShoulderLocation,
    const FVector& DesiredHandLocation,
    float DesiredShoulderToHandDistanceCm)
{
    if (!IsFiniteVector(ClavicleLocation)
        || !IsFiniteVector(CurrentShoulderLocation)
        || !IsFiniteVector(DesiredHandLocation)
        || !FMath::IsFinite(DesiredShoulderToHandDistanceCm)
        || DesiredShoulderToHandDistanceCm <= 0.0f)
    {
        return CurrentShoulderLocation;
    }
    const float ClavicleLength = FVector::Distance(
        ClavicleLocation, CurrentShoulderLocation);
    const FVector CenterDelta = DesiredHandLocation - ClavicleLocation;
    const float CenterDistance = CenterDelta.Size();
    if (ClavicleLength <= MinimumLimbLengthCm
        || CenterDistance <= UE_KINDA_SMALL_NUMBER
        || CenterDistance > ClavicleLength + DesiredShoulderToHandDistanceCm
        || CenterDistance < FMath::Abs(
            ClavicleLength - DesiredShoulderToHandDistanceCm))
    {
        return CurrentShoulderLocation;
    }
    const FVector Axis = CenterDelta / CenterDistance;
    const float AlongAxis = (
        FMath::Square(ClavicleLength)
        - FMath::Square(DesiredShoulderToHandDistanceCm)
        + FMath::Square(CenterDistance)) / (2.0f * CenterDistance);
    const FVector CircleCenter = ClavicleLocation + Axis * AlongAxis;
    const float CircleRadius = FMath::Sqrt(FMath::Max(
        0.0f, FMath::Square(ClavicleLength) - FMath::Square(AlongAxis)));
    FVector Radial = CurrentShoulderLocation - CircleCenter;
    Radial -= Axis * FVector::DotProduct(Radial, Axis);
    if (Radial.IsNearlyZero())
    {
        Radial = FVector::CrossProduct(Axis, FVector::UpVector);
        if (Radial.IsNearlyZero())
        {
            Radial = FVector::CrossProduct(Axis, FVector::RightVector);
        }
    }
    const FVector Candidate = CircleCenter + Radial.GetSafeNormal() * CircleRadius;
    return IsFiniteVector(Candidate) ? Candidate : CurrentShoulderLocation;
}

float ComputeSegmentLengthRatioError(
    const FVector& OriginalStart,
    const FVector& OriginalEnd,
    const FVector& CorrectedStart,
    const FVector& CorrectedEnd)
{
    if (!IsFiniteVector(OriginalStart)
        || !IsFiniteVector(OriginalEnd)
        || !IsFiniteVector(CorrectedStart)
        || !IsFiniteVector(CorrectedEnd))
    {
        return TNumericLimits<float>::Max();
    }

    const double OriginalLength = FVector::Distance(OriginalStart, OriginalEnd);
    const double CorrectedLength = FVector::Distance(CorrectedStart, CorrectedEnd);
    if (!FMath::IsFinite(OriginalLength)
        || !FMath::IsFinite(CorrectedLength)
        || OriginalLength <= MinimumLimbLengthCm)
    {
        return TNumericLimits<float>::Max();
    }
    return static_cast<float>(FMath::Abs(CorrectedLength / OriginalLength - 1.0));
}
}

FDiscGolfMetaHumanRetargetAnimInstanceProxy::
FDiscGolfMetaHumanRetargetAnimInstanceProxy(
    UAnimInstance* InAnimInstance,
    FAnimNode_RetargetPoseFromMesh* InRetargetNode)
    : FAnimInstanceProxy(InAnimInstance)
    , RetargetNode(InRetargetNode)
{
}

void FDiscGolfMetaHumanRetargetAnimInstanceProxy::Initialize(
    UAnimInstance* InAnimInstance)
{
    FAnimInstanceProxy::Initialize(InAnimInstance);
    SmoothedPhaseCorrectionWeight = 0.0f;
    if (RetargetNode)
    {
        FAnimationInitializeContext Context(this);
        RetargetNode->Initialize_AnyThread(Context);
    }
}

void FDiscGolfMetaHumanRetargetAnimInstanceProxy::PreUpdate(
    UAnimInstance* InAnimInstance,
    float DeltaSeconds)
{
    FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
    if (RetargetNode && RetargetNode->HasPreUpdate())
    {
        RetargetNode->PreUpdate(InAnimInstance);
    }

    HandCorrectionEvidence = FDiscGolfMetaHumanHandCorrectionEvidence();
    bSourceArmGuideValid = false;
    USkeletalMeshComponent* const TargetMesh = InAnimInstance
        ? InAnimInstance->GetSkelMeshComponent() : nullptr;
    USkeletalMeshComponent* const SourceMesh = RetargetNode
        ? RetargetNode->SourceMeshComponent.Get() : nullptr;
    const UDiscGolfThrowComponent* const ThrowComponent =
        IsValid(SourceMesh) && IsValid(SourceMesh->GetOwner())
        ? SourceMesh->GetOwner()->FindComponentByClass<UDiscGolfThrowComponent>()
        : nullptr;
    const float TargetPhaseCorrectionWeight =
        IsValid(ThrowComponent) && ThrowComponent->bThrowActive
        ? DiscGolfMetaHumanPresentation::PhaseCorrectionWeight(
            ThrowComponent->CurrentPhase)
        : 0.0f;
    SmoothedPhaseCorrectionWeight =
        DiscGolfMetaHumanPresentation::SmoothCorrectionWeight(
            SmoothedPhaseCorrectionWeight,
            TargetPhaseCorrectionWeight,
            DeltaSeconds);
    static const FName SourceHandBone(TEXT("hand_r"));
    static const FName SourceDiscGripBone(TEXT("disc_grip_r"));
    static const FName SourceUpperArmBone(TEXT("upperarm_r"));
    static const FName SourceLowerArmBone(TEXT("lowerarm_r"));
    if (!IsValid(TargetMesh) || !IsValid(SourceMesh)
        || !IsValid(ThrowComponent) || !ThrowComponent->bThrowActive
        || ThrowComponent->GetActiveDiscGripBone() != SourceDiscGripBone
        || SourceMesh->GetBoneIndex(SourceHandBone) == INDEX_NONE
        || SourceMesh->GetBoneIndex(SourceUpperArmBone) == INDEX_NONE
        || SourceMesh->GetBoneIndex(SourceLowerArmBone) == INDEX_NONE
        || !SourceMesh->DoesSocketExist(SourceDiscGripBone))
    {
        return;
    }

    HandCorrectionEvidence.CorrectionMode =
        DiscGolfMetaHumanPresentation::HandCorrectionMode;
    HandCorrectionEvidence.PhaseCorrectionWeight =
        SmoothedPhaseCorrectionWeight;
    const FTransform SourceHandWorld = SourceMesh->GetSocketTransform(SourceHandBone, RTS_World);
    const FTransform SourceGripWorld = SourceMesh->GetSocketTransform(SourceDiscGripBone, RTS_World);
    const FTransform SourceShoulderWorld =
        SourceMesh->GetSocketTransform(SourceUpperArmBone, RTS_World);
    const FTransform SourceElbowWorld =
        SourceMesh->GetSocketTransform(SourceLowerArmBone, RTS_World);
    HandCorrectionEvidence.SourceHandWorldLocation = SourceHandWorld.GetLocation();
    HandCorrectionEvidence.SourceDiscGripWorldLocation = SourceGripWorld.GetLocation();
    TargetComponentToWorldAtPreUpdate = TargetMesh->GetComponentTransform();
    SourceShoulderComponentLocation =
        TargetComponentToWorldAtPreUpdate.InverseTransformPosition(
            SourceShoulderWorld.GetLocation());
    SourceElbowComponentLocation =
        TargetComponentToWorldAtPreUpdate.InverseTransformPosition(
            SourceElbowWorld.GetLocation());
    SourceHandComponentLocation =
        TargetComponentToWorldAtPreUpdate.InverseTransformPosition(
            SourceHandWorld.GetLocation());
    bSourceArmGuideValid = IsFiniteVector(SourceShoulderComponentLocation)
        && IsFiniteVector(SourceElbowComponentLocation)
        && IsFiniteVector(SourceHandComponentLocation)
        && !SourceShoulderComponentLocation.Equals(
            SourceElbowComponentLocation, UE_KINDA_SMALL_NUMBER);
    FTransform DesiredHandWorld = SourceHandWorld;
    if (TargetMesh->DoesSocketExist(SourceDiscGripBone))
    {
        const FTransform TargetHandWorld = TargetMesh->GetSocketTransform(SourceHandBone, RTS_World);
        const FTransform TargetGripWorld = TargetMesh->GetSocketTransform(SourceDiscGripBone, RTS_World);
        DesiredHandWorld =
            DiscGolfMetaHumanPresentation::BuildDesiredHandWorldFromGripBinding(
                SourceGripWorld,
                TargetHandWorld,
                TargetGripWorld);
        HandCorrectionEvidence.GripBindingMode =
            DiscGolfMetaHumanPresentation::TargetSocketGripBindingMode;
        HandCorrectionEvidence.bFullGripTransformAvailable = true;
    }
    else
    {
        // The target asset intentionally remains immutable. Reuse the authored
        // DG source hand->disc_grip_r binding to prove socket provenance. Since
        // DesiredGripWorld is that same source grip, the algebra resolves back
        // to SourceHandWorld without inventing a MetaHuman socket or offset.
        DesiredHandWorld =
            DiscGolfMetaHumanPresentation::BuildDesiredHandWorldFromGripBinding(
                SourceGripWorld,
                SourceHandWorld,
                SourceGripWorld);
        HandCorrectionEvidence.GripBindingMode =
            DiscGolfMetaHumanPresentation::SourceGripRelativeFallbackBindingMode;
        HandCorrectionEvidence.bSourceGripRelativeFallback = true;
    }
    DesiredHandComponentTransform = DesiredHandWorld.GetRelativeTransform(
        TargetComponentToWorldAtPreUpdate);
    HandCorrectionEvidence.DesiredMetaHumanHandWorldLocation =
        DesiredHandWorld.GetLocation();
    HandCorrectionEvidence.SourceHandToDiscGripDistanceCm =
        FVector::Distance(
            HandCorrectionEvidence.SourceHandWorldLocation,
            HandCorrectionEvidence.SourceDiscGripWorldLocation);
    HandCorrectionEvidence.SourceBoneRevisionAtPreUpdate =
        SourceMesh->GetBoneTransformRevisionNumber();
    HandCorrectionEvidence.TargetBoneRevisionBeforeEvaluate =
        TargetMesh->GetBoneTransformRevisionNumber();
    HandCorrectionEvidence.SourceSampleFrameCounter = GFrameCounter;
    HandCorrectionEvidence.bSnapshotValid =
        IsFiniteVector(HandCorrectionEvidence.SourceHandWorldLocation)
        && IsFiniteVector(
            HandCorrectionEvidence.SourceDiscGripWorldLocation)
        && TargetComponentToWorldAtPreUpdate.IsValid()
        && DesiredHandComponentTransform.IsValid()
        && FMath::IsFinite(
            HandCorrectionEvidence.SourceHandToDiscGripDistanceCm);
}

void FDiscGolfMetaHumanRetargetAnimInstanceProxy::CacheBones()
{
    if (RetargetNode && bBoneCachesInvalidated)
    {
        FAnimationCacheBonesContext Context(this);
        RetargetNode->CacheBones_AnyThread(Context);
        bBoneCachesInvalidated = false;
    }
}

bool FDiscGolfMetaHumanRetargetAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
    FAnimInstanceProxy::Evaluate(Output);
    if (!RetargetNode)
    {
        Output.ResetToRefPose();
        return false;
    }
    RetargetNode->Evaluate_AnyThread(Output);

    HandCorrectionEvidence.TargetCorrectionFrameCounter = GFrameCounter;
    if (!HandCorrectionEvidence.bSnapshotValid)
    {
        return true;
    }

    static const FName UpperArmBone(TEXT("upperarm_r"));
    static const FName ClavicleBone(TEXT("clavicle_r"));
    static const FName LowerArmBone(TEXT("lowerarm_r"));
    static const FName HandBone(TEXT("hand_r"));
    const FBoneContainer& BoneContainer = Output.Pose.GetBoneContainer();
    const FCompactPoseBoneIndex UpperArmIndex =
        BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(
            BoneContainer.GetPoseBoneIndexForBoneName(UpperArmBone)));
    const FCompactPoseBoneIndex ClavicleIndex =
        BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(
            BoneContainer.GetPoseBoneIndexForBoneName(ClavicleBone)));
    const FCompactPoseBoneIndex LowerArmIndex =
        BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(
            BoneContainer.GetPoseBoneIndexForBoneName(LowerArmBone)));
    const FCompactPoseBoneIndex HandIndex =
        BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(
            BoneContainer.GetPoseBoneIndexForBoneName(HandBone)));
    if (ClavicleIndex.GetInt() == INDEX_NONE
        || UpperArmIndex.GetInt() == INDEX_NONE
        || LowerArmIndex.GetInt() == INDEX_NONE
        || HandIndex.GetInt() == INDEX_NONE
        || BoneContainer.GetParentBoneIndex(UpperArmIndex).GetInt()
            != ClavicleIndex.GetInt()
        || BoneContainer.GetParentBoneIndex(LowerArmIndex).GetInt()
            != UpperArmIndex.GetInt()
        || BoneContainer.GetParentBoneIndex(HandIndex).GetInt()
            != LowerArmIndex.GetInt())
    {
        return true;
    }

    FCSPose<FCompactPose> ComponentPose;
    ComponentPose.InitPose(Output.Pose);
    FTransform ClavicleTransform =
        ComponentPose.GetComponentSpaceTransform(ClavicleIndex);
    FTransform UpperArmTransform =
        ComponentPose.GetComponentSpaceTransform(UpperArmIndex);
    FTransform LowerArmTransform =
        ComponentPose.GetComponentSpaceTransform(LowerArmIndex);
    FTransform HandTransform =
        ComponentPose.GetComponentSpaceTransform(HandIndex);
    if (!ClavicleTransform.IsValid() || !UpperArmTransform.IsValid() || !LowerArmTransform.IsValid()
        || !HandTransform.IsValid())
    {
        return true;
    }

    HandCorrectionEvidence.MetaHumanHandPreCorrectionWorldLocation =
        TargetComponentToWorldAtPreUpdate.TransformPosition(
            HandTransform.GetLocation());
    HandCorrectionEvidence.MetaHumanHandPreToSourceHandDistanceCm =
        FVector::Distance(
            HandCorrectionEvidence.MetaHumanHandPreCorrectionWorldLocation,
            HandCorrectionEvidence.SourceHandWorldLocation);
    HandCorrectionEvidence.MetaHumanHandPreToDesiredHandDistanceCm =
        FVector::Distance(
            HandCorrectionEvidence.MetaHumanHandPreCorrectionWorldLocation,
            HandCorrectionEvidence.DesiredMetaHumanHandWorldLocation);

    const double UpperLimbLength = FVector::Distance(
        UpperArmTransform.GetLocation(), LowerArmTransform.GetLocation());
    const double LowerLimbLength = FVector::Distance(
        LowerArmTransform.GetLocation(), HandTransform.GetLocation());
    const double MaximumReach = UpperLimbLength + LowerLimbLength;
    const double MinimumReach = FMath::Abs(
        UpperLimbLength - LowerLimbLength);
    if (!FMath::IsFinite(UpperLimbLength)
        || !FMath::IsFinite(LowerLimbLength)
        || UpperLimbLength <= MinimumLimbLengthCm
        || LowerLimbLength <= MinimumLimbLengthCm)
    {
        return true;
    }

    const FVector OriginalClavicleDirection = (
        UpperArmTransform.GetLocation()
        - ClavicleTransform.GetLocation()).GetSafeNormal();
    const float SourceReachFraction =
        DiscGolfMetaHumanPresentation::ComputeSourceArmReachFraction(
            SourceShoulderComponentLocation,
            SourceElbowComponentLocation,
            SourceHandComponentLocation);
    const float DesiredTargetReach = FMath::Clamp(
        SourceReachFraction, 0.0f, 1.0f)
        * static_cast<float>(MaximumReach);
    const FVector FullyRelocatedShoulder =
        DiscGolfMetaHumanPresentation::BuildClavicleAwareShoulderTarget(
            ClavicleTransform.GetLocation(),
            UpperArmTransform.GetLocation(),
            DesiredHandComponentTransform.GetLocation(),
            DesiredTargetReach);
    const float ShoulderRelocationAlpha = FMath::Clamp(
        DiscGolfMetaHumanPresentation::ComputeCorrectionBlendAlpha(
            HandCorrectionEvidence.MetaHumanHandPreToDesiredHandDistanceCm,
            FMath::RadiansToDegrees(HandTransform.GetRotation().AngularDistance(
                DiscGolfMetaHumanPresentation::SelectAnatomicalHandOrientation(
                    HandTransform.GetRotation(),
                    DesiredHandComponentTransform.GetRotation(),
                    HandCorrectionEvidence.bSourceGripRelativeFallback))))
            * HandCorrectionEvidence.PhaseCorrectionWeight,
        0.0f,
        1.0f);
    const FVector RelocatedShoulder =
        DiscGolfMetaHumanPresentation::BlendShoulderRelocation(
            UpperArmTransform.GetLocation(),
            FullyRelocatedShoulder,
            ShoulderRelocationAlpha);
    const FVector RelocatedClavicleDirection = (
        RelocatedShoulder - ClavicleTransform.GetLocation()).GetSafeNormal();
    if (!OriginalClavicleDirection.IsNearlyZero()
        && !RelocatedClavicleDirection.IsNearlyZero())
    {
        ClavicleTransform.SetRotation((
            FQuat::FindBetweenNormals(
                OriginalClavicleDirection, RelocatedClavicleDirection)
            * ClavicleTransform.GetRotation()).GetNormalized());
        const FVector ShoulderDelta =
            RelocatedShoulder - UpperArmTransform.GetLocation();
        UpperArmTransform.SetLocation(RelocatedShoulder);
        // Carry the raw retargeted arm with its relocated shoulder before IK.
        // This keeps both target limb lengths intact; the subsequent solve
        // changes only joint directions.
        LowerArmTransform.AddToTranslation(ShoulderDelta);
        HandTransform.AddToTranslation(ShoulderDelta);
    }

    float UniformWorldScale = 0.0f;
    if (!GetUniformWorldScale(
            TargetComponentToWorldAtPreUpdate, UniformWorldScale))
    {
        return true;
    }

    const FTransform OriginalUpper = UpperArmTransform;
    const FTransform OriginalLower = LowerArmTransform;
    const FTransform OriginalHand = HandTransform;
    const FQuat AnatomicalDesiredHandOrientation =
        DiscGolfMetaHumanPresentation::SelectAnatomicalHandOrientation(
            HandTransform.GetRotation(),
            DesiredHandComponentTransform.GetRotation(),
            HandCorrectionEvidence.bSourceGripRelativeFallback);
    const float PreSolveOrientationError = FMath::RadiansToDegrees(
        HandTransform.GetRotation().AngularDistance(
            AnatomicalDesiredHandOrientation));
    const float BlendAlpha = DiscGolfMetaHumanPresentation::ComputeCorrectionBlendAlpha(
        HandCorrectionEvidence.MetaHumanHandPreToDesiredHandDistanceCm,
        PreSolveOrientationError) * FMath::Clamp(
            HandCorrectionEvidence.PhaseCorrectionWeight, 0.0f, 1.0f);
    HandCorrectionEvidence.DesiredHandOrientationErrorDegrees =
        PreSolveOrientationError;
    HandCorrectionEvidence.AppliedCorrectionBlendAlpha = BlendAlpha;
    HandCorrectionEvidence.RequestedEffectorCorrectionCm =
        HandCorrectionEvidence.MetaHumanHandPreToDesiredHandDistanceCm;
    HandCorrectionEvidence.EffectorCorrectionCapCm =
        MaximumEffectorCorrectionCm;
    HandCorrectionEvidence.bEffectorDistanceCapExceeded =
        HandCorrectionEvidence.RequestedEffectorCorrectionCm
            > MaximumEffectorCorrectionCm + 0.01f;
    if (BlendAlpha <= UE_KINDA_SMALL_NUMBER)
    {
        HandCorrectionEvidence.bReachable = true;
        HandCorrectionEvidence.UpperArmToLowerArmRatioError = 0.0f;
        HandCorrectionEvidence.LowerArmToHandRatioError = 0.0f;
        HandCorrectionEvidence.MaximumCorrectedSegmentRatioError = 0.0f;
        HandCorrectionEvidence.MetaHumanHandPostCorrectionWorldLocation =
            HandCorrectionEvidence.MetaHumanHandPreCorrectionWorldLocation;
        HandCorrectionEvidence.MetaHumanHandPostToSourceHandDistanceCm =
            HandCorrectionEvidence.MetaHumanHandPreToSourceHandDistanceCm;
        HandCorrectionEvidence.MetaHumanHandPostToDesiredHandDistanceCm =
            HandCorrectionEvidence.MetaHumanHandPreToDesiredHandDistanceCm;
        HandCorrectionEvidence.MetaHumanHandPostToDesiredHandOrientationErrorDegrees =
            PreSolveOrientationError;
        return true;
    }

    const float EffectorCorrectionCapComponent =
        MaximumEffectorCorrectionCm / UniformWorldScale;

    const FVector BoundedTarget = ClampEffectorTargetWithCap(
        UpperArmTransform.GetLocation(),
        OriginalHand.GetLocation(),
        DesiredHandComponentTransform.GetLocation(),
        MinimumReach,
        MaximumReach,
        EffectorCorrectionCapComponent);
    HandCorrectionEvidence.BoundedEffectorTargetWorldLocation =
        TargetComponentToWorldAtPreUpdate.TransformPosition(BoundedTarget);
    HandCorrectionEvidence.BoundedEffectorCorrectionCm = FVector::Distance(
        HandCorrectionEvidence.MetaHumanHandPreCorrectionWorldLocation,
        HandCorrectionEvidence.BoundedEffectorTargetWorldLocation);
    HandCorrectionEvidence.bEffectorCorrectionClamped = FVector::Distance(
        HandCorrectionEvidence.BoundedEffectorTargetWorldLocation,
        HandCorrectionEvidence.DesiredMetaHumanHandWorldLocation) > 0.01f;
    const float DesiredReachCm = FVector::Distance(
        UpperArmTransform.GetLocation(),
        DesiredHandComponentTransform.GetLocation());
    HandCorrectionEvidence.bDesiredTargetWithinReachAnnulus =
        DesiredReachCm >= MinimumReach + ReachSafetyMarginCm
        && DesiredReachCm <= MaximumReach - ReachSafetyMarginCm;
    if (HandCorrectionEvidence.BoundedEffectorCorrectionCm
        > MaximumEffectorCorrectionCm + 0.01f)
    {
        return true;
    }

    const FVector BlendedEffectorTarget = FMath::Lerp(
        HandTransform.GetLocation(), BoundedTarget, BlendAlpha);
    const FVector IntermediateTarget = ClampEffectorTargetWithCap(
        UpperArmTransform.GetLocation(),
        OriginalHand.GetLocation(),
        BlendedEffectorTarget,
        MinimumReach,
        MaximumReach,
        EffectorCorrectionCapComponent);
    const FVector CurrentElbowPole = bSourceArmGuideValid
        ? DiscGolfMetaHumanPresentation::BuildSourceGuidedElbowPole(
            UpperArmTransform.GetLocation(), LowerArmTransform.GetLocation(),
            IntermediateTarget, SourceShoulderComponentLocation,
            SourceElbowComponentLocation,
            UpperArmTransform.GetRotation().RotateVector(FVector::RightVector),
            UpperLimbLength)
        : DiscGolfMetaHumanPresentation::BuildStableElbowPole(
            UpperArmTransform.GetLocation(), LowerArmTransform.GetLocation(),
            IntermediateTarget,
            UpperArmTransform.GetRotation().RotateVector(FVector::RightVector),
            UpperLimbLength);
    HandCorrectionEvidence.bSourceGuidedElbowPole = bSourceArmGuideValid;
    AnimationCore::SolveTwoBoneIK(
        UpperArmTransform,
        LowerArmTransform,
        HandTransform,
        CurrentElbowPole,
        IntermediateTarget,
        false,
        1.0,
        1.0);
    const FVector OriginalUpperDirection = (
        OriginalLower.GetLocation()
        - OriginalUpper.GetLocation()).GetSafeNormal();
    const FVector SolvedUpperDirection = (
        LowerArmTransform.GetLocation()
        - UpperArmTransform.GetLocation()).GetSafeNormal();
    const FVector OriginalLowerDirection = (
        OriginalHand.GetLocation()
        - OriginalLower.GetLocation()).GetSafeNormal();
    const FVector SolvedLowerDirection = (
        HandTransform.GetLocation()
        - LowerArmTransform.GetLocation()).GetSafeNormal();
    if (OriginalUpperDirection.IsNearlyZero()
        || SolvedUpperDirection.IsNearlyZero()
        || OriginalLowerDirection.IsNearlyZero()
        || SolvedLowerDirection.IsNearlyZero())
    {
        return true;
    }
    UpperArmTransform.SetRotation((
        FQuat::FindBetweenNormals(
            OriginalUpperDirection, SolvedUpperDirection)
        * OriginalUpper.GetRotation()).GetNormalized());
    LowerArmTransform.SetRotation((
        FQuat::FindBetweenNormals(
            OriginalLowerDirection, SolvedLowerDirection)
        * OriginalLower.GetRotation()).GetNormalized());
    FQuat FinalDesiredHandOrientation = AnatomicalDesiredHandOrientation;
    const bool bCarryFallbackHandWithSolvedForearm =
        HandCorrectionEvidence.bSourceGripRelativeFallback;
    if (bCarryFallbackHandWithSolvedForearm)
    {
        FinalDesiredHandOrientation =
            DiscGolfMetaHumanPresentation::CarryHandOrientationWithSolvedForearm(
                OriginalLower.GetRotation(),
                OriginalHand.GetRotation(),
                LowerArmTransform.GetRotation());
        HandTransform.SetRotation(FinalDesiredHandOrientation);
    }
    else
    {
        // A target-authored socket retains full grip-orientation authority.
        HandTransform.SetRotation(
            DiscGolfMetaHumanPresentation::BlendHandOrientation(
                OriginalHand.GetRotation(),
                AnatomicalDesiredHandOrientation,
                BlendAlpha));
    }
    HandCorrectionEvidence.DesiredHandOrientationErrorDegrees =
        FMath::RadiansToDegrees(OriginalHand.GetRotation().AngularDistance(
            FinalDesiredHandOrientation));
    const FVector SolvedHandWorldLocation =
        TargetComponentToWorldAtPreUpdate.TransformPosition(
            HandTransform.GetLocation());
    HandCorrectionEvidence.UpperArmToLowerArmRatioError =
        DiscGolfMetaHumanPresentation::ComputeSegmentLengthRatioError(
            OriginalUpper.GetLocation(), OriginalLower.GetLocation(),
            UpperArmTransform.GetLocation(), LowerArmTransform.GetLocation());
    HandCorrectionEvidence.LowerArmToHandRatioError =
        DiscGolfMetaHumanPresentation::ComputeSegmentLengthRatioError(
            OriginalLower.GetLocation(), OriginalHand.GetLocation(),
            LowerArmTransform.GetLocation(), HandTransform.GetLocation());
    HandCorrectionEvidence.MaximumCorrectedSegmentRatioError = FMath::Max(
        HandCorrectionEvidence.UpperArmToLowerArmRatioError,
        HandCorrectionEvidence.LowerArmToHandRatioError);
    HandCorrectionEvidence.MetaHumanHandPostCorrectionWorldLocation =
        SolvedHandWorldLocation;
    HandCorrectionEvidence.MetaHumanHandPostToSourceHandDistanceCm =
        FVector::Distance(
            SolvedHandWorldLocation,
            HandCorrectionEvidence.SourceHandWorldLocation);
    HandCorrectionEvidence.MetaHumanHandPostToDesiredHandDistanceCm =
        FVector::Distance(
            SolvedHandWorldLocation,
            HandCorrectionEvidence.DesiredMetaHumanHandWorldLocation);
    HandCorrectionEvidence.MetaHumanHandPostToDesiredHandOrientationErrorDegrees =
        FMath::RadiansToDegrees(HandTransform.GetRotation().AngularDistance(
            FinalDesiredHandOrientation));
    if (!UpperArmTransform.IsValid() || !LowerArmTransform.IsValid()
        || !HandTransform.IsValid()
        || !IsFiniteVector(SolvedHandWorldLocation))
    {
        return true;
    }
    const float SolvedEffectorResidualCm = FVector::Distance(
        HandTransform.GetLocation(), IntermediateTarget) * UniformWorldScale;
    const float TotalAppliedCorrectionCm = FVector::Distance(
        HandCorrectionEvidence.MetaHumanHandPreCorrectionWorldLocation,
        SolvedHandWorldLocation);
    if (!FMath::IsFinite(SolvedEffectorResidualCm)
        || !FMath::IsFinite(TotalAppliedCorrectionCm)
        || SolvedEffectorResidualCm > 0.10f
        || TotalAppliedCorrectionCm
            > MaximumEffectorCorrectionCm + 0.01f)
    {
        return true;
    }
    if (!FMath::IsFinite(HandCorrectionEvidence.MaximumCorrectedSegmentRatioError)
        || HandCorrectionEvidence.MaximumCorrectedSegmentRatioError
            > MaximumCorrectedSegmentRatioError)
    {
        return true;
    }

    HandCorrectionEvidence.bReachable = true;
    TArray<FBoneTransform, TInlineAllocator<4>> CorrectedTransforms;
    CorrectedTransforms.Emplace(ClavicleIndex, ClavicleTransform);
    CorrectedTransforms.Emplace(UpperArmIndex, UpperArmTransform);
    CorrectedTransforms.Emplace(LowerArmIndex, LowerArmTransform);
    CorrectedTransforms.Emplace(HandIndex, HandTransform);
    CorrectedTransforms.Sort(FCompareBoneTransformIndex());
    ComponentPose.LocalBlendCSBoneTransforms(CorrectedTransforms, 1.0f);
    FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(
        ComponentPose, Output.Pose);
    HandCorrectionEvidence.bApplied = true;
    HandCorrectionEvidence.bFallbackHandOrientationCarriedWithSolvedForearm =
        bCarryFallbackHandWithSolvedForearm;
    return true;
}

void FDiscGolfMetaHumanRetargetAnimInstanceProxy::UpdateAnimationNode(
    const FAnimationUpdateContext& InContext)
{
    FAnimInstanceProxy::UpdateAnimationNode(InContext);
    UpdateCounter.Increment();
    if (RetargetNode)
    {
        RetargetNode->Update_AnyThread(InContext);
    }
}

UDiscGolfMetaHumanRetargetAnimInstance::
UDiscGolfMetaHumanRetargetAnimInstance()
{
    bUseMultiThreadedAnimationUpdate = false;
    SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
}

void UDiscGolfMetaHumanRetargetAnimInstance::NativeInitializeAnimation()
{
    FDiscGolfMetaHumanRetargetAnimInstanceProxy& Proxy =
        GetProxyOnGameThread<FDiscGolfMetaHumanRetargetAnimInstanceProxy>();
    Proxy.Initialize(this);
}

FAnimInstanceProxy*
UDiscGolfMetaHumanRetargetAnimInstance::CreateAnimInstanceProxy()
{
    return new FDiscGolfMetaHumanRetargetAnimInstanceProxy(this, &RetargetNode);
}

bool UDiscGolfMetaHumanRetargetAnimInstance::ConfigureAndVerify(
    UIKRetargeter* Retargeter,
    USkeletalMeshComponent* SourceMesh,
    FString& OutStatus)
{
    OutStatus.Reset();
    USkeletalMeshComponent* TargetMesh = GetSkelMeshComponent();
    if (!IsValid(Retargeter) || !Retargeter->HasSourceIKRig()
        || !Retargeter->HasTargetIKRig() || !IsValid(SourceMesh)
        || !SourceMesh->IsRegistered() || !SourceMesh->GetSkeletalMeshAsset()
        || !IsValid(TargetMesh) || !TargetMesh->IsRegistered()
        || !TargetMesh->GetSkeletalMeshAsset()
        || SourceMesh->GetWorld() != TargetMesh->GetWorld())
    {
        OutStatus = TEXT("Retarget source, target, rigs, or world is invalid.");
        return false;
    }

    const UScriptStruct* ExpectedOpTypes[] = {
        FIKRetargetPelvisMotionOp::StaticStruct(),
        FIKRetargetFKChainsOp::StaticStruct(),
        FIKRetargetRunIKRigOp::StaticStruct(),
        FIKRetargetRootMotionOp::StaticStruct(),
        FIKRetargetCurveRemapOp::StaticStruct(),
    };
    const bool ExpectedOpEnabled[] = {true, true, false, false, true};
    const TArray<FInstancedStruct>& RetargetOps = Retargeter->GetRetargetOps();
    bool bExactOpPolicy =
        RetargetOps.Num() == static_cast<int32>(UE_ARRAY_COUNT(ExpectedOpTypes));
    for (int32 Index = 0; bExactOpPolicy && Index < RetargetOps.Num(); ++Index)
    {
        const FIKRetargetOpBase* Op =
            RetargetOps[Index].GetPtr<FIKRetargetOpBase>();
        bExactOpPolicy = Op
            && Op->GetType() == ExpectedOpTypes[Index]
            && Op->IsEnabled() == ExpectedOpEnabled[Index];
    }
    const FIKRetargetOpBase* RootMotionOp = bExactOpPolicy
        ? RetargetOps[3].GetPtr<FIKRetargetOpBase>()
        : nullptr;
    const FIKRetargetRootMotionOpSettings* RootMotionSettings =
        RootMotionOp
        && RootMotionOp->GetType() == FIKRetargetRootMotionOp::StaticStruct()
            ? static_cast<const FIKRetargetRootMotionOpSettings*>(
                RootMotionOp->GetSettingsConst())
            : nullptr;
    bExactOpPolicy = bExactOpPolicy
        && RootMotionSettings
        && RootMotionSettings->SourceRoot.BoneName == FName(TEXT("root"))
        && RootMotionSettings->TargetRoot.BoneName == FName(TEXT("pelvis"))
        && RootMotionSettings->TargetPelvis.BoneName == FName(TEXT("pelvis"))
        && RootMotionSettings->RootMotionSource
            == ERootMotionSource::CopyFromSourceRoot;
    if (!bExactOpPolicy)
    {
        OutStatus = TEXT(
            "The fixed-presentation retargeter must use exact Pelvis/FK/disabled-RunIK/disabled-Root-Motion/Curve policy.");
        return false;
    }

    FDiscGolfMetaHumanRetargetAnimInstanceProxy& Proxy =
        GetProxyOnGameThread<FDiscGolfMetaHumanRetargetAnimInstanceProxy>();
    if (!Proxy.RetargetNode)
    {
        OutStatus = TEXT("Native MetaHuman retarget node is unavailable.");
        return false;
    }

    Proxy.RetargetNode->IKRetargeterAsset = Retargeter;
    Proxy.RetargetNode->RetargetFrom =
        ERetargetSourceMode::CustomSkeletalMeshComponent;
    Proxy.RetargetNode->SourceMeshComponent = SourceMesh;
    Proxy.RetargetNode->bSuppressWarnings = false;
    if (FIKRetargetProcessor* Processor =
            Proxy.RetargetNode->GetRetargetProcessor())
    {
        Processor->SetNeedsInitialized();
    }

    TargetMesh->AddTickPrerequisiteComponent(SourceMesh);
    if (!Proxy.RetargetNode->EnsureProcessorIsInitialized(TargetMesh))
    {
        OutStatus = TEXT("DG-to-MetaHuman retarget processor did not initialize.");
        return false;
    }

    const FIKRetargetProcessor* Processor =
        Proxy.RetargetNode->GetRetargetProcessor();
    if (!Processor || !Processor->IsInitialized()
        || !Processor->WasInitializedWithTheseAssets(
            SourceMesh->GetSkeletalMeshAsset(),
            TargetMesh->GetSkeletalMeshAsset(),
            Retargeter))
    {
        OutStatus = TEXT("Retarget processor initialized against unexpected assets.");
        return false;
    }

    VerifiedRetargeter = Retargeter;
    VerifiedSourceMesh = SourceMesh;
    OutStatus = TEXT(
        "Native DG-to-MetaHuman retarget processor is initialized with the unstable Run IK solve and pelvis-overwriting Root Motion disabled.");
    return true;
}

bool UDiscGolfMetaHumanRetargetAnimInstance::IsConfiguredFor(
    const UIKRetargeter* Retargeter,
    const USkeletalMeshComponent* SourceMesh) const
{
    const USkeletalMeshComponent* TargetMesh = GetSkelMeshComponent();
    if (!IsValid(Retargeter) || Retargeter != VerifiedRetargeter
        || !IsValid(SourceMesh) || SourceMesh != VerifiedSourceMesh
        || !IsValid(TargetMesh) || !TargetMesh->GetSkeletalMeshAsset()
        || !SourceMesh->GetSkeletalMeshAsset())
    {
        return false;
    }
    const FDiscGolfMetaHumanRetargetAnimInstanceProxy& Proxy =
        GetProxyOnGameThread<FDiscGolfMetaHumanRetargetAnimInstanceProxy>();
    if (!Proxy.RetargetNode
        || Proxy.RetargetNode->IKRetargeterAsset != Retargeter
        || Proxy.RetargetNode->SourceMeshComponent.Get() != SourceMesh)
    {
        return false;
    }
    const FIKRetargetProcessor* Processor =
        Proxy.RetargetNode->GetRetargetProcessor();
    return Processor && Processor->IsInitialized()
        && Processor->WasInitializedWithTheseAssets(
            SourceMesh->GetSkeletalMeshAsset(),
            TargetMesh->GetSkeletalMeshAsset(),
            Retargeter);
}

bool UDiscGolfMetaHumanRetargetAnimInstance::
    GetPresentationHandCorrectionEvidence(
        FDiscGolfMetaHumanHandCorrectionEvidence& OutEvidence) const
{
    const FDiscGolfMetaHumanRetargetAnimInstanceProxy& Proxy =
        GetProxyOnGameThread<FDiscGolfMetaHumanRetargetAnimInstanceProxy>();
    OutEvidence = Proxy.HandCorrectionEvidence;
    return OutEvidence.bSnapshotValid;
}
