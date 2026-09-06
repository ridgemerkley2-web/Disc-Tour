#include "DiscGolfCharacterRigUnits.h"

#include "Rigs/RigHierarchy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DiscGolfCharacterRigUnits)

namespace DiscGolfCharacterRig
{
constexpr float BaselineHeightCm = 183.0f;
constexpr float BaselineMassKg = 82.0f;

constexpr float DefaultRunUp = 0.65f;
constexpr float DefaultReachBack = 0.80f;
constexpr float DefaultTorsoRotation = 0.75f;
constexpr float DefaultBrace = 0.75f;
constexpr float DefaultExplosiveness = 0.60f;
constexpr float DefaultFollowThrough = 0.80f;

const FName RootBone(TEXT("root"));
const FName PelvisBone(TEXT("pelvis"));
const FName Spine04Bone(TEXT("spine_04"));
const FName LeftFootBone(TEXT("foot_l"));
const FName RightFootBone(TEXT("foot_r"));
const FName LeftBallBone(TEXT("ball_l"));
const FName RightBallBone(TEXT("ball_r"));
const FName LeftHandBone(TEXT("hand_l"));
const FName RightHandBone(TEXT("hand_r"));
const FName LeftUpperArmBone(TEXT("upperarm_l"));
const FName RightUpperArmBone(TEXT("upperarm_r"));
const FName LeftLowerArmBone(TEXT("lowerarm_l"));
const FName RightLowerArmBone(TEXT("lowerarm_r"));
const FName LeftClavicleBone(TEXT("clavicle_l"));
const FName RightClavicleBone(TEXT("clavicle_r"));
const FName LeftThighBone(TEXT("thigh_l"));
const FName RightThighBone(TEXT("thigh_r"));
const FName LeftCalfBone(TEXT("calf_l"));
const FName RightCalfBone(TEXT("calf_r"));
const FName NeckBone(TEXT("neck_01"));
const FName HeadBone(TEXT("head"));

struct FProfileFactors
{
    float Height = 1.0f;
    float Wingspan = 1.0f;
    float Shoulder = 1.0f;
    float Torso = 1.0f;
    float Leg = 1.0f;
    float Hand = 1.0f;
    float Arm = 1.0f;
    float Build = 1.0f;
    float Head = 1.0f;
    float Foot = 1.0f;
};

float Sanitize(float Value, float Fallback, float Minimum, float Maximum, bool& bOutClamped)
{
    if (!FMath::IsFinite(Value))
    {
        bOutClamped = true;
        return Fallback;
    }
    const float Clamped = FMath::Clamp(Value, Minimum, Maximum);
    bOutClamped |= !FMath::IsNearlyEqual(Value, Clamped, UE_KINDA_SMALL_NUMBER);
    return Clamped;
}

float SignedStyleDelta(float Value, float DefaultValue)
{
    const float Denominator = Value >= DefaultValue
        ? FMath::Max(1.0f - DefaultValue, UE_SMALL_NUMBER)
        : FMath::Max(DefaultValue, UE_SMALL_NUMBER);
    return FMath::Clamp((Value - DefaultValue) / Denominator, -1.0f, 1.0f);
}

bool StartsWith(const FName Name, const TCHAR* Prefix)
{
    return Name.ToString().StartsWith(Prefix, ESearchCase::CaseSensitive);
}

bool IsBodyBone(const FName Name)
{
    if (Name == RootBone || StartsWith(Name, TEXT("ik_")))
    {
        return false;
    }
    return true;
}

bool IsSpineBone(const FName Name)
{
    return StartsWith(Name, TEXT("spine_"));
}

bool IsFingerBone(const FName Name)
{
    return StartsWith(Name, TEXT("thumb_")) || StartsWith(Name, TEXT("index_")) ||
        StartsWith(Name, TEXT("middle_")) || StartsWith(Name, TEXT("ring_")) ||
        StartsWith(Name, TEXT("pinky_"));
}

bool IsArmLongBoneOffset(const FName Name)
{
    // A child's local offset measures the length of the bone above it.
    return StartsWith(Name, TEXT("lowerarm_")) || StartsWith(Name, TEXT("hand_")) ||
        StartsWith(Name, TEXT("upperarm_twist_")) || StartsWith(Name, TEXT("lowerarm_twist_"));
}

bool IsLegLongBoneOffset(const FName Name)
{
    return StartsWith(Name, TEXT("calf_")) || StartsWith(Name, TEXT("foot_")) ||
        StartsWith(Name, TEXT("ball_")) || StartsWith(Name, TEXT("thigh_twist_"));
}

FVector ScaleOffset(const FName BoneName, const FVector& Offset, const FProfileFactors& Factors)
{
    if (BoneName == PelvisBone)
    {
        return Offset * Factors.Leg;
    }
    if (IsSpineBone(BoneName) || BoneName == TEXT("neck_01"))
    {
        return Offset * Factors.Torso;
    }
    if (BoneName == TEXT("head"))
    {
        return Offset * Factors.Head;
    }
    if (StartsWith(BoneName, TEXT("clavicle_")) ||
        (StartsWith(BoneName, TEXT("upperarm_")) && !StartsWith(BoneName, TEXT("upperarm_twist_"))))
    {
        return Offset * Factors.Shoulder;
    }
    if (IsArmLongBoneOffset(BoneName))
    {
        return Offset * Factors.Arm;
    }
    if (IsFingerBone(BoneName) || StartsWith(BoneName, TEXT("disc_grip_")))
    {
        return Offset * Factors.Hand;
    }
    if (StartsWith(BoneName, TEXT("thigh_")) && !StartsWith(BoneName, TEXT("thigh_twist_")))
    {
        return FVector(Offset.X * Factors.Build, Offset.Y * Factors.Build, Offset.Z * Factors.Leg);
    }
    if (IsLegLongBoneOffset(BoneName))
    {
        return Offset * Factors.Leg;
    }
    return Offset;
}

FVector ShapeScale(const FName BoneName, const FProfileFactors& Factors)
{
    if (BoneName == PelvisBone)
    {
        return FVector(Factors.Build, 1.0f, Factors.Build);
    }
    if (IsSpineBone(BoneName))
    {
        // DG master bones use their local Y axis as the authored length axis.
        return FVector(Factors.Shoulder, Factors.Torso, Factors.Build);
    }
    if (BoneName == TEXT("head") || BoneName == TEXT("neck_01"))
    {
        return FVector(Factors.Head);
    }
    if (StartsWith(BoneName, TEXT("upperarm_")) || StartsWith(BoneName, TEXT("lowerarm_")))
    {
        return FVector(Factors.Build, Factors.Arm, Factors.Build);
    }
    if (StartsWith(BoneName, TEXT("hand_")) || IsFingerBone(BoneName))
    {
        return FVector(Factors.Hand);
    }
    if (StartsWith(BoneName, TEXT("thigh_")) || StartsWith(BoneName, TEXT("calf_")))
    {
        return FVector(Factors.Build, Factors.Leg, Factors.Build);
    }
    if (StartsWith(BoneName, TEXT("foot_")) || StartsWith(BoneName, TEXT("ball_")))
    {
        return FVector(Factors.Foot);
    }
    return FVector::OneVector;
}

float ReadCurveAlpha(const URigHierarchy* Hierarchy, const TCHAR* Name)
{
    return FMath::Clamp(Hierarchy->GetCurveValue(FRigElementKey(FName(Name), ERigElementType::Curve)), 0.0f, 1.0f);
}

void SetFloatControl(URigHierarchy* Hierarchy, const TCHAR* Name, float Value)
{
    const FRigElementKey Key(FName(Name), ERigElementType::Control);
    if (Hierarchy->Contains(Key))
    {
        Hierarchy->SetControlValueByIndex(
            Hierarchy->GetIndex(Key),
            FRigControlValue::Make<float>(Value),
            ERigControlValueType::Current,
            false,
            false);
    }
}

void SetTransformControl(URigHierarchy* Hierarchy, const TCHAR* Name, const FTransform& Value)
{
    const FRigElementKey Key(FName(Name), ERigElementType::Control);
    if (Hierarchy->Contains(Key) && !Value.ContainsNaN())
    {
        Hierarchy->SetGlobalTransform(Key, Value, false, false, false, false);
    }
}

struct FPresentationHandTargets
{
    FTransform RelaxedLeft = FTransform::Identity;
    FTransform RelaxedRight = FTransform::Identity;
    FTransform BalanceLeft = FTransform::Identity;
    FTransform BalanceRight = FTransform::Identity;
    FTransform FollowThroughLeft = FTransform::Identity;
    FTransform FollowThroughRight = FTransform::Identity;
    bool bValid = false;
};

FTransform BuildHandTarget(
    const FTransform& SourceHand,
    const FVector& ShoulderLocation,
    const FVector& TargetLocation)
{
    FTransform Result = SourceHand;
    const FVector SourceDirection = (SourceHand.GetLocation() - ShoulderLocation).GetSafeNormal();
    const FVector TargetDirection = (TargetLocation - ShoulderLocation).GetSafeNormal();
    if (!SourceDirection.IsNearlyZero() && !TargetDirection.IsNearlyZero())
    {
        const FQuat DirectionDelta = FQuat::FindBetweenNormals(SourceDirection, TargetDirection);
        Result.SetRotation((DirectionDelta * SourceHand.GetRotation()).GetNormalized());
    }
    Result.SetLocation(TargetLocation);
    return Result;
}

FPresentationHandTargets BuildPresentationHandTargets(
    const TMap<FName, FTransform>& Globals,
    float HeightScale)
{
    FPresentationHandTargets Result;
    const FTransform* Pelvis = Globals.Find(PelvisBone);
    const FTransform* Spine = Globals.Find(Spine04Bone);
    const FTransform* LeftShoulder = Globals.Find(LeftUpperArmBone);
    const FTransform* RightShoulder = Globals.Find(RightUpperArmBone);
    const FTransform* LeftHand = Globals.Find(LeftHandBone);
    const FTransform* RightHand = Globals.Find(RightHandBone);
    if (!Pelvis || !Spine || !LeftShoulder || !RightShoulder || !LeftHand || !RightHand)
    {
        return Result;
    }

    const FVector Up = (Spine->GetLocation() - Pelvis->GetLocation()).GetSafeNormal();
    const FVector Right = (RightShoulder->GetLocation() - LeftShoulder->GetLocation()).GetSafeNormal();
    const FVector Forward = FVector::CrossProduct(Right, Up).GetSafeNormal();
    if (Up.IsNearlyZero() || Right.IsNearlyZero() || Forward.IsNearlyZero())
    {
        return Result;
    }

    const float Scale = FMath::Clamp(HeightScale, 0.80f, 1.20f);
    const FVector PelvisLocation = Pelvis->GetLocation();
    const FVector RelaxedCenter = PelvisLocation + Forward * (7.0f * Scale) - Up * (13.0f * Scale);
    const FVector BalanceCenter = PelvisLocation + Forward * (13.0f * Scale) + Up * (21.0f * Scale);
    // Finish at the opposite hip rather than at shoulder height. This makes the
    // release resolve into a readable diagonal follow-through instead of
    // preserving the prototype clip's broad horizontal arm silhouette.
    const FVector FollowCenter = PelvisLocation + Forward * (5.0f * Scale) + Up * (8.0f * Scale);

    const FVector RelaxedLeftLocation = RelaxedCenter - Right * (24.0f * Scale);
    const FVector RelaxedRightLocation = RelaxedCenter + Right * (24.0f * Scale);
    const FVector BalanceLeftLocation = BalanceCenter - Right * (29.0f * Scale);
    const FVector BalanceRightLocation = BalanceCenter + Right * (29.0f * Scale);
    const FVector FollowThroughLeftLocation = FollowCenter + Right * (12.0f * Scale);
    const FVector FollowThroughRightLocation = FollowCenter - Right * (12.0f * Scale);

    Result.RelaxedLeft = BuildHandTarget(*LeftHand, LeftShoulder->GetLocation(), RelaxedLeftLocation);
    Result.RelaxedRight = BuildHandTarget(*RightHand, RightShoulder->GetLocation(), RelaxedRightLocation);
    Result.BalanceLeft = BuildHandTarget(*LeftHand, LeftShoulder->GetLocation(), BalanceLeftLocation);
    Result.BalanceRight = BuildHandTarget(*RightHand, RightShoulder->GetLocation(), BalanceRightLocation);
    Result.FollowThroughLeft = BuildHandTarget(
        *LeftHand, LeftShoulder->GetLocation(), FollowThroughLeftLocation);
    Result.FollowThroughRight = BuildHandTarget(
        *RightHand, RightShoulder->GetLocation(), FollowThroughRightLocation);
    Result.bValid = !Result.RelaxedLeft.ContainsNaN() && !Result.RelaxedRight.ContainsNaN() &&
        !Result.BalanceLeft.ContainsNaN() && !Result.BalanceRight.ContainsNaN() &&
        !Result.FollowThroughLeft.ContainsNaN() && !Result.FollowThroughRight.ContainsNaN();
    return Result;
}

FTransform BlendPresentationTarget(
    const FTransform& From,
    const FTransform& To,
    float Alpha)
{
    FTransform Result;
    Result.Blend(From, To, FMath::Clamp(Alpha, 0.0f, 1.0f));
    return Result;
}

FQuat StyleRotation(const FName BoneName, float ReachbackAlpha, float BraceAlpha,
    float ReleaseAlpha, float FollowThroughAlpha, float ReachDelta, float TorsoDelta,
    float BraceDelta, float ExplosiveDelta, float FollowDelta,
    EDGHandedness Handedness, const FDGThrowMicroMotionPose& MicroMotion)
{
    const bool bLeftHanded = Handedness == EDGHandedness::Left;
    const FName ThrowClavicle = bLeftHanded ? LeftClavicleBone : RightClavicleBone;
    const FName SupportClavicle = bLeftHanded ? RightClavicleBone : LeftClavicleBone;
    const FName ThrowUpperArm = bLeftHanded ? LeftUpperArmBone : RightUpperArmBone;
    const FName ThrowLowerArm = bLeftHanded ? LeftLowerArmBone : RightLowerArmBone;
    const FName ThrowHand = bLeftHanded ? LeftHandBone : RightHandBone;
    const FName LeadThigh = bLeftHanded ? RightThighBone : LeftThighBone;
    const FName LeadCalf = bLeftHanded ? RightCalfBone : LeftCalfBone;
    FQuat Result = FQuat::Identity;
    if (BoneName == PelvisBone)
    {
        // A vertical pelvis translation would be canceled by the foot-grounding
        // correction below. Local counter-yaw plus a small brace pitch stays
        // visible while PBIK preserves the accepted foot contacts.
        const float BracePitch = BraceDelta * 4.0f * BraceAlpha
            + MicroMotion.BraceCompressionAlpha * 1.4f;
        return FQuat(FVector::UpVector,
                   FMath::DegreesToRadians(MicroMotion.PelvisCounterYawDegrees))
            * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(BracePitch));
    }
    if (BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03") || BoneName == Spine04Bone)
    {
        const float Distribution = BoneName == Spine04Bone ? 0.45f : (BoneName == TEXT("spine_03") ? 0.35f : 0.20f);
        float YawDegrees = TorsoDelta * 8.0f * Distribution
            * FMath::Max(ReachbackAlpha, ReleaseAlpha);
        YawDegrees += FollowDelta * 10.0f * Distribution * FollowThroughAlpha;
        YawDegrees += MicroMotion.TorsoCoilYawDegrees * Distribution;
        const float PitchDegrees = MicroMotion.TorsoForwardPitchDegrees * Distribution;
        Result = FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDegrees))
            * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(PitchDegrees));
    }
    else if (BoneName == ThrowClavicle)
    {
        const float Degrees = ReachDelta * 6.0f * ReachbackAlpha
            + ExplosiveDelta * 4.0f * ReleaseAlpha
            + MicroMotion.ShoulderLeadDegrees * 0.35f;
        Result = FQuat(FVector::UpVector, FMath::DegreesToRadians(Degrees));
    }
    else if (BoneName == SupportClavicle)
    {
        const float CounterBalance = -MicroMotion.TorsoCoilYawDegrees
            * MicroMotion.OffArmBalanceAlpha * 0.10f;
        Result = FQuat(FVector::UpVector, FMath::DegreesToRadians(CounterBalance));
    }
    else if (BoneName == ThrowUpperArm)
    {
        Result = FQuat(FVector::UpVector,
            FMath::DegreesToRadians(MicroMotion.ShoulderLeadDegrees * 0.65f));
    }
    else if (BoneName == ThrowLowerArm)
    {
        Result = FQuat(FVector::ForwardVector,
            FMath::DegreesToRadians(MicroMotion.ElbowLeadDegrees));
    }
    else if (BoneName == ThrowHand)
    {
        Result = FQuat(FVector::UpVector,
            FMath::DegreesToRadians(MicroMotion.WristLagDegrees));
    }
    else if (BoneName == LeadThigh)
    {
        Result = FQuat(FVector::ForwardVector,
            FMath::DegreesToRadians(MicroMotion.BraceCompressionAlpha * 1.8f));
    }
    else if (BoneName == LeadCalf)
    {
        Result = FQuat(FVector::ForwardVector,
            FMath::DegreesToRadians(-MicroMotion.BraceCompressionAlpha * 2.8f));
    }
    else if (BoneName == NeckBone || BoneName == HeadBone)
    {
        const float Distribution = BoneName == HeadBone ? 0.65f : 0.35f;
        const float CounterYaw = -(MicroMotion.PelvisCounterYawDegrees
            + MicroMotion.TorsoCoilYawDegrees) * MicroMotion.GazeStabilizationAlpha
            * Distribution * 0.38f;
        Result = FQuat(FVector::UpVector, FMath::DegreesToRadians(CounterYaw));
    }
    else if (IsFingerBone(BoneName))
    {
        const bool bThrowFinger = BoneName.ToString().EndsWith(
            bLeftHanded ? TEXT("_l") : TEXT("_r"), ESearchCase::CaseSensitive);
        if (bThrowFinger)
        {
            // Only a restrained additive curl is applied. The sharp semantic
            // change from Acceleration to Release is driven by the exact phase
            // notify, never by guessed montage time.
            const float CurlDegrees = (MicroMotion.GripAlpha - 0.50f) * 7.0f;
            Result = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(CurlDegrees));
        }
    }
    return Result;
}
} // namespace DiscGolfCharacterRig

namespace DiscGolfCharacterRigPresentation
{
float PlantLockBlendAlpha(float PlantCurveAlpha)
{
    if (!FMath::IsFinite(PlantCurveAlpha))
    {
        return 0.0f;
    }
    return FMath::SmoothStep(0.05f, 0.95f, FMath::Clamp(PlantCurveAlpha, 0.0f, 1.0f));
}

FTransform BlendPlantTarget(const FTransform& AnimatedTarget, const FTransform& LockedTarget,
    float PlantCurveAlpha)
{
    if (!AnimatedTarget.IsValid() || !LockedTarget.IsValid())
    {
        return AnimatedTarget;
    }
    FTransform Result;
    Result.Blend(AnimatedTarget, LockedTarget, PlantLockBlendAlpha(PlantCurveAlpha));
    return Result;
}

float PhaseLocalBlendAlpha(EDGThrowPhase Phase, float DeltaSeconds)
{
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0.0f)
    {
        return 0.0f;
    }

    float HalfLifeSeconds = 0.0f;
    switch (Phase)
    {
        case EDGThrowPhase::Idle: HalfLifeSeconds = 0.08f; break;
        case EDGThrowPhase::Aim: HalfLifeSeconds = 0.09f; break;
        case EDGThrowPhase::RunUp: HalfLifeSeconds = 0.055f; break;
        case EDGThrowPhase::ReachBack: HalfLifeSeconds = 0.045f; break;
        case EDGThrowPhase::Plant: HalfLifeSeconds = 0.035f; break;
        case EDGThrowPhase::Acceleration: HalfLifeSeconds = 0.025f; break;
        case EDGThrowPhase::Release: HalfLifeSeconds = 0.012f; break;
        case EDGThrowPhase::FollowThrough: HalfLifeSeconds = 0.045f; break;
        case EDGThrowPhase::Recovery: HalfLifeSeconds = 0.13f; break;
        default: return 0.0f;
    }

    // Exponential interpolation is stable across render rates. Capping one
    // contribution prevents a debugger pause from becoming a one-frame snap.
    const float SafeDelta = FMath::Min(DeltaSeconds, 0.10f);
    return FMath::Clamp(1.0f - FMath::Exp2(-SafeDelta / HalfLifeSeconds), 0.0f, 1.0f);
}

bool IsMicroMotionPoseFiniteAndBounded(const FDGThrowMicroMotionPose& Pose)
{
    return FMath::IsFinite(Pose.PelvisCounterYawDegrees)
        && FMath::Abs(Pose.PelvisCounterYawDegrees) <= 6.0f
        && FMath::IsFinite(Pose.TorsoCoilYawDegrees)
        && FMath::Abs(Pose.TorsoCoilYawDegrees) <= 8.0f
        && FMath::IsFinite(Pose.TorsoForwardPitchDegrees)
        && FMath::Abs(Pose.TorsoForwardPitchDegrees) <= 5.0f
        && FMath::IsFinite(Pose.ShoulderLeadDegrees)
        && FMath::Abs(Pose.ShoulderLeadDegrees) <= 7.0f
        && FMath::IsFinite(Pose.ElbowLeadDegrees)
        && FMath::Abs(Pose.ElbowLeadDegrees) <= 7.0f
        && FMath::IsFinite(Pose.WristLagDegrees)
        && FMath::Abs(Pose.WristLagDegrees) <= 5.0f
        && FMath::IsFinite(Pose.WristRollDegrees)
        && FMath::Abs(Pose.WristRollDegrees) <= 6.0f
        && FMath::IsFinite(Pose.WristPitchDegrees)
        && FMath::Abs(Pose.WristPitchDegrees) <= 4.0f
        && FMath::IsFinite(Pose.BraceCompressionAlpha)
        && Pose.BraceCompressionAlpha >= 0.0f
        && Pose.BraceCompressionAlpha <= 1.0f
        && FMath::IsFinite(Pose.PlantFootSettleCm)
        && Pose.PlantFootSettleCm >= 0.0f
        && Pose.PlantFootSettleCm <= 0.8f
        && FMath::IsFinite(Pose.GripAlpha)
        && Pose.GripAlpha >= 0.0f
        && Pose.GripAlpha <= 1.0f
        && FMath::IsFinite(Pose.OffArmBalanceAlpha)
        && Pose.OffArmBalanceAlpha >= 0.0f
        && Pose.OffArmBalanceAlpha <= 1.0f
        && FMath::IsFinite(Pose.GazeStabilizationAlpha)
        && Pose.GazeStabilizationAlpha >= 0.0f
        && Pose.GazeStabilizationAlpha <= 1.0f
        && FMath::IsFinite(Pose.DiscPlaneStabilityAlpha)
        && Pose.DiscPlaneStabilityAlpha >= 0.0f
        && Pose.DiscPlaneStabilityAlpha <= 1.0f
        && FMath::IsFinite(Pose.RecoveryRelaxAlpha)
        && Pose.RecoveryRelaxAlpha >= 0.0f
        && Pose.RecoveryRelaxAlpha <= 1.0f
        && FMath::IsFinite(Pose.WaggleAlpha)
        && Pose.WaggleAlpha >= 0.0f
        && Pose.WaggleAlpha <= 1.0f;
}

bool HasProductionFullBodyMotionSignature(float AuthoredGazeCurveAlpha)
{
    // DG_GazeTargetAlpha is unique to the dense full-body production recipes
    // and is deliberately non-zero at every authored frame. A missing, stale,
    // or malformed value keeps the legacy path rather than guessing identity.
    return FMath::IsFinite(AuthoredGazeCurveAlpha)
        && AuthoredGazeCurveAlpha > 0.01f
        && AuthoredGazeCurveAlpha <= 1.0f;
}

FDGUpperLimbIKPolicy BuildUpperLimbIKPolicy(
    bool bProductionFullBodyMotion,
    EDGThrowPhase Phase,
    const FDGThrowMicroMotionPose& MicroMotion,
    float ReachStyleContribution)
{
    const auto SafeUnit = [](float Value)
    {
        return FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.0f, 0.0f, 1.0f);
    };
    const float DiscPlane = SafeUnit(MicroMotion.DiscPlaneStabilityAlpha);
    const float OffArm = SafeUnit(MicroMotion.OffArmBalanceAlpha);
    const float Recovery = SafeUnit(MicroMotion.RecoveryRelaxAlpha);
    const float Reach = FMath::Clamp(
        FMath::IsFinite(ReachStyleContribution)
            ? FMath::Abs(ReachStyleContribution) : 0.0f,
        0.0f, 1.0f);
    const bool bFollowThrough = Phase == EDGThrowPhase::FollowThrough;
    const bool bRecovery = Phase == EDGThrowPhase::Recovery;

    FDGUpperLimbIKPolicy Policy;
    if (!bProductionFullBodyMotion)
    {
        // Preserve the pre-existing prototype/legacy presentation path exactly.
        Policy.ThrowHandAlpha = bFollowThrough || bRecovery
            ? FMath::Lerp(0.96f, 0.74f, Recovery)
            : FMath::Clamp(0.08f + DiscPlane * 0.16f + Reach * 0.18f,
                0.0f, 0.36f);
        Policy.SupportHandAlpha = FMath::Clamp(
            0.45f + 0.45f * OffArm, 0.45f, 0.90f);
        Policy.SupportTargetBlendAlpha = OffArm;
        Policy.bUseSyntheticFollowTarget = bFollowThrough || bRecovery;
        return Policy;
    }

    Policy.bPreserveAuthoredThrowTrajectory = true;
    Policy.bUseSyntheticFollowTarget = false;
    Policy.ThrowHandAlpha = FMath::Clamp(
        0.03f + DiscPlane * 0.07f + Reach * 0.02f, 0.0f, 0.12f);
    Policy.SupportHandAlpha = FMath::Clamp(
        0.02f + OffArm * 0.06f, 0.0f, 0.08f);
    Policy.SupportTargetBlendAlpha = FMath::Clamp(OffArm * 0.10f, 0.0f, 0.10f);

    if (bFollowThrough)
    {
        Policy.ThrowHandAlpha = FMath::Min(Policy.ThrowHandAlpha, 0.08f);
        Policy.SupportHandAlpha = FMath::Min(Policy.SupportHandAlpha, 0.06f);
    }
    else if (bRecovery)
    {
        Policy.ThrowHandAlpha = FMath::Lerp(0.06f, 0.02f, Recovery);
        Policy.SupportHandAlpha = FMath::Lerp(0.04f, 0.01f, Recovery);
        Policy.SupportTargetBlendAlpha *= 1.0f - Recovery;
    }
    return Policy;
}

FDGThrowMicroMotionPose BuildMicroMotionTarget(
    EDGThrowPhase Phase,
    const FDGThrowIntent& Intent,
    const FDGThrowStyle& Style,
    const FDGBodyProfile& Body,
    EDGHandedness Handedness)
{
    FDGThrowMicroMotionPose Pose;
    if ((Handedness != EDGHandedness::Right && Handedness != EDGHandedness::Left)
        || Intent.ThrowType != EDGThrowType::Backhand)
    {
        return Pose;
    }

    switch (Phase)
    {
        case EDGThrowPhase::Aim:
            Pose.PelvisCounterYawDegrees = -0.35f;
            Pose.TorsoCoilYawDegrees = 0.65f;
            Pose.TorsoForwardPitchDegrees = 0.25f;
            Pose.ShoulderLeadDegrees = -0.30f;
            Pose.ElbowLeadDegrees = 0.20f;
            Pose.BraceCompressionAlpha = 0.08f;
            Pose.GripAlpha = 0.86f;
            Pose.OffArmBalanceAlpha = 0.45f;
            Pose.GazeStabilizationAlpha = 1.0f;
            Pose.DiscPlaneStabilityAlpha = 0.68f;
            Pose.WaggleAlpha = 1.0f;
            break;
        case EDGThrowPhase::RunUp:
            Pose.PelvisCounterYawDegrees = -0.80f;
            Pose.TorsoCoilYawDegrees = 1.20f;
            Pose.TorsoForwardPitchDegrees = 0.50f;
            Pose.ShoulderLeadDegrees = -0.55f;
            Pose.ElbowLeadDegrees = 0.35f;
            Pose.BraceCompressionAlpha = 0.18f;
            Pose.GripAlpha = 0.92f;
            Pose.OffArmBalanceAlpha = 0.62f;
            Pose.GazeStabilizationAlpha = 0.95f;
            Pose.DiscPlaneStabilityAlpha = 0.75f;
            Pose.WaggleAlpha = 0.48f;
            break;
        case EDGThrowPhase::ReachBack:
            Pose.PelvisCounterYawDegrees = -2.20f;
            Pose.TorsoCoilYawDegrees = 4.20f;
            Pose.TorsoForwardPitchDegrees = 1.00f;
            Pose.ShoulderLeadDegrees = -2.20f;
            Pose.ElbowLeadDegrees = -0.80f;
            Pose.WristLagDegrees = -1.10f;
            Pose.BraceCompressionAlpha = 0.38f;
            Pose.GripAlpha = 0.98f;
            Pose.OffArmBalanceAlpha = 0.82f;
            Pose.GazeStabilizationAlpha = 0.90f;
            Pose.DiscPlaneStabilityAlpha = 0.92f;
            Pose.WaggleAlpha = 0.08f;
            break;
        case EDGThrowPhase::Plant:
            Pose.PelvisCounterYawDegrees = -1.35f;
            Pose.TorsoCoilYawDegrees = 3.10f;
            Pose.TorsoForwardPitchDegrees = 1.35f;
            Pose.ShoulderLeadDegrees = -1.20f;
            Pose.ElbowLeadDegrees = -0.45f;
            Pose.WristLagDegrees = -1.60f;
            Pose.BraceCompressionAlpha = 0.96f;
            Pose.PlantFootSettleCm = 0.48f;
            Pose.GripAlpha = 0.99f;
            Pose.OffArmBalanceAlpha = 0.90f;
            Pose.GazeStabilizationAlpha = 1.0f;
            Pose.DiscPlaneStabilityAlpha = 1.0f;
            break;
        case EDGThrowPhase::Acceleration:
            Pose.PelvisCounterYawDegrees = 1.45f;
            Pose.TorsoCoilYawDegrees = 2.15f;
            Pose.TorsoForwardPitchDegrees = 1.10f;
            Pose.ShoulderLeadDegrees = 2.90f;
            Pose.ElbowLeadDegrees = 1.25f;
            Pose.WristLagDegrees = -2.20f;
            Pose.BraceCompressionAlpha = 1.0f;
            Pose.PlantFootSettleCm = 0.55f;
            Pose.GripAlpha = 1.0f;
            Pose.OffArmBalanceAlpha = 0.96f;
            Pose.GazeStabilizationAlpha = 1.0f;
            Pose.DiscPlaneStabilityAlpha = 1.0f;
            break;
        case EDGThrowPhase::Release:
            Pose.PelvisCounterYawDegrees = 2.45f;
            Pose.TorsoCoilYawDegrees = 3.45f;
            Pose.TorsoForwardPitchDegrees = 0.75f;
            Pose.ShoulderLeadDegrees = 2.15f;
            Pose.ElbowLeadDegrees = 3.85f;
            Pose.WristLagDegrees = 2.65f;
            Pose.BraceCompressionAlpha = 0.82f;
            Pose.PlantFootSettleCm = 0.42f;
            Pose.GripAlpha = 0.08f;
            Pose.OffArmBalanceAlpha = 0.93f;
            Pose.GazeStabilizationAlpha = 0.96f;
            Pose.DiscPlaneStabilityAlpha = 1.0f;
            break;
        case EDGThrowPhase::FollowThrough:
            Pose.PelvisCounterYawDegrees = 2.85f;
            Pose.TorsoCoilYawDegrees = 4.65f;
            Pose.TorsoForwardPitchDegrees = 0.25f;
            Pose.ShoulderLeadDegrees = 1.75f;
            Pose.ElbowLeadDegrees = 2.90f;
            Pose.WristLagDegrees = 0.75f;
            Pose.BraceCompressionAlpha = 0.45f;
            Pose.PlantFootSettleCm = 0.20f;
            Pose.GripAlpha = 0.03f;
            Pose.OffArmBalanceAlpha = 0.84f;
            Pose.GazeStabilizationAlpha = 0.82f;
            Pose.DiscPlaneStabilityAlpha = 0.72f;
            break;
        case EDGThrowPhase::Recovery:
            Pose.PelvisCounterYawDegrees = 0.25f;
            Pose.TorsoCoilYawDegrees = 0.45f;
            Pose.TorsoForwardPitchDegrees = 0.10f;
            Pose.ShoulderLeadDegrees = 0.12f;
            Pose.ElbowLeadDegrees = 0.08f;
            Pose.BraceCompressionAlpha = 0.08f;
            Pose.GripAlpha = 0.28f;
            Pose.OffArmBalanceAlpha = 0.28f;
            Pose.GazeStabilizationAlpha = 0.88f;
            Pose.DiscPlaneStabilityAlpha = 0.18f;
            Pose.RecoveryRelaxAlpha = 1.0f;
            break;
        case EDGThrowPhase::Idle:
            Pose.GripAlpha = 0.45f;
            Pose.GazeStabilizationAlpha = 0.65f;
            Pose.RecoveryRelaxAlpha = 1.0f;
            break;
        default:
            return FDGThrowMicroMotionPose();
    }

    const auto FiniteClamp = [](float Value, float Fallback, float Minimum, float Maximum)
    {
        return FMath::Clamp(FMath::IsFinite(Value) ? Value : Fallback, Minimum, Maximum);
    };
    const float Power = FiniteClamp(Intent.Power01, 0.0f, 0.0f, 1.0f);
    const float HeightScale = FiniteClamp(Body.HeightCm, 183.0f, 150.0f, 210.0f) / 183.0f;
    const float WingspanScale = FiniteClamp(Body.WingspanScale, 1.0f, 0.92f, 1.08f);
    const float ShoulderScale = FiniteClamp(Body.ShoulderWidthScale, 1.0f, 0.92f, 1.08f);
    const float TorsoLengthScale = FiniteClamp(Body.TorsoLengthScale, 1.0f, 0.94f, 1.06f);
    const float LegLengthScale = FiniteClamp(Body.LegLengthScale, 1.0f, 0.94f, 1.06f);
    const float HandScale = FiniteClamp(Body.HandScale, 1.0f, 0.94f, 1.06f);
    const float ExpectedMassKg = 82.0f * FMath::Square(HeightScale);
    const float BodyMassScale = FMath::Clamp(FMath::Sqrt(
        FiniteClamp(Body.MassKg, 82.0f, 45.0f, 160.0f)
            / FMath::Max(ExpectedMassKg, 1.0f)), 0.88f, 1.12f);
    const float LeverageScale = FMath::Clamp(
        HeightScale * (0.72f * WingspanScale + 0.28f * ShoulderScale), 0.85f, 1.18f);
    const float PowerScale = 0.45f + 0.55f * Power;
    const float RunUpScale = 0.80f + 0.40f * FiniteClamp(Style.RunUpIntensity, 0.65f, 0.0f, 1.0f);
    const float ReachScale = 0.80f + 0.40f * FiniteClamp(Style.ReachBackAmount, 0.80f, 0.0f, 1.0f);
    const float TorsoScale = 0.80f + 0.40f * FiniteClamp(Style.TorsoRotation, 0.75f, 0.0f, 1.0f);
    const float BraceScale = 0.75f + 0.50f * FiniteClamp(Style.BraceIntensity, 0.75f, 0.0f, 1.0f);
    const float ExplosiveScale = 0.75f + 0.50f * FiniteClamp(Style.Explosiveness, 0.60f, 0.0f, 1.0f);
    const float FollowScale = 0.75f + 0.50f * FiniteClamp(Style.FollowThrough, 0.80f, 0.0f, 1.0f);
    const float HandSign = Handedness == EDGHandedness::Left ? -1.0f : 1.0f;

    Pose.PelvisCounterYawDegrees = FMath::Clamp(
        Pose.PelvisCounterYawDegrees * HandSign * PowerScale * RunUpScale, -6.0f, 6.0f);
    const float PhaseTorsoScale = Phase == EDGThrowPhase::FollowThrough ? FollowScale : ReachScale;
    Pose.TorsoCoilYawDegrees = FMath::Clamp(
        Pose.TorsoCoilYawDegrees * HandSign * PowerScale * TorsoScale
            * PhaseTorsoScale * TorsoLengthScale,
        -8.0f, 8.0f);
    Pose.TorsoForwardPitchDegrees = FMath::Clamp(
        Pose.TorsoForwardPitchDegrees * PowerScale * BraceScale, -5.0f, 5.0f);
    Pose.ShoulderLeadDegrees = FMath::Clamp(
        Pose.ShoulderLeadDegrees * HandSign * PowerScale * ExplosiveScale * LeverageScale,
        -7.0f, 7.0f);
    Pose.ElbowLeadDegrees = FMath::Clamp(
        Pose.ElbowLeadDegrees * HandSign * PowerScale * ExplosiveScale * LeverageScale,
        -7.0f, 7.0f);
    Pose.WristLagDegrees = FMath::Clamp(
        Pose.WristLagDegrees * HandSign * PowerScale * ExplosiveScale, -5.0f, 5.0f);
    Pose.BraceCompressionAlpha = FMath::Clamp(
        Pose.BraceCompressionAlpha * BraceScale * BodyMassScale, 0.0f, 1.0f);
    Pose.PlantFootSettleCm = FMath::Clamp(
        Pose.PlantFootSettleCm * HeightScale * LegLengthScale
            * BraceScale * BodyMassScale,
        0.0f, 0.8f);
    Pose.WristRollDegrees = FMath::Clamp(
        FiniteClamp(Intent.HyzerDegrees, 0.0f, -34.0f, 34.0f)
            * 0.12f * Pose.DiscPlaneStabilityAlpha * HandSign / HandScale,
        -6.0f, 6.0f);
    Pose.WristPitchDegrees = FMath::Clamp(
        FiniteClamp(Intent.NoseDegrees, 0.0f, -7.0f, 11.0f)
            * 0.18f * Pose.DiscPlaneStabilityAlpha / HandScale,
        -4.0f, 4.0f);
    return IsMicroMotionPoseFiniteAndBounded(Pose)
        ? Pose
        : FDGThrowMicroMotionPose();
}

FDGThrowMicroMotionPose BlendMicroMotionPose(
    const FDGThrowMicroMotionPose& Current,
    const FDGThrowMicroMotionPose& Target,
    float Alpha)
{
    const FDGThrowMicroMotionPose SafeCurrent = IsMicroMotionPoseFiniteAndBounded(Current)
        ? Current
        : FDGThrowMicroMotionPose();
    if (!IsMicroMotionPoseFiniteAndBounded(Target))
    {
        return SafeCurrent;
    }
    const float SafeAlpha = FMath::Clamp(FMath::IsFinite(Alpha) ? Alpha : 0.0f, 0.0f, 1.0f);
    FDGThrowMicroMotionPose Result;
#define DG_BLEND_MICRO_FIELD(Field) Result.Field = FMath::Lerp(SafeCurrent.Field, Target.Field, SafeAlpha)
    DG_BLEND_MICRO_FIELD(PelvisCounterYawDegrees);
    DG_BLEND_MICRO_FIELD(TorsoCoilYawDegrees);
    DG_BLEND_MICRO_FIELD(TorsoForwardPitchDegrees);
    DG_BLEND_MICRO_FIELD(ShoulderLeadDegrees);
    DG_BLEND_MICRO_FIELD(ElbowLeadDegrees);
    DG_BLEND_MICRO_FIELD(WristLagDegrees);
    DG_BLEND_MICRO_FIELD(WristRollDegrees);
    DG_BLEND_MICRO_FIELD(WristPitchDegrees);
    DG_BLEND_MICRO_FIELD(BraceCompressionAlpha);
    DG_BLEND_MICRO_FIELD(PlantFootSettleCm);
    DG_BLEND_MICRO_FIELD(GripAlpha);
    DG_BLEND_MICRO_FIELD(OffArmBalanceAlpha);
    DG_BLEND_MICRO_FIELD(GazeStabilizationAlpha);
    DG_BLEND_MICRO_FIELD(DiscPlaneStabilityAlpha);
    DG_BLEND_MICRO_FIELD(RecoveryRelaxAlpha);
    DG_BLEND_MICRO_FIELD(WaggleAlpha);
#undef DG_BLEND_MICRO_FIELD
    return Result;
}
}

FRigUnit_DGApplyCharacterProfile::FRigUnit_DGApplyCharacterProfile() = default;

FRigUnit_DGApplyCharacterProfile_Execute()
{
    using namespace DiscGolfCharacterRig;

    bApplied = false;
    bInputsClamped = false;
    URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
    if (!Hierarchy)
    {
        return;
    }

    FDGBodyProfile Body = BodyProfile;
    Body.HeightCm = Sanitize(Body.HeightCm, BaselineHeightCm, 150.0f, 210.0f, bInputsClamped);
    Body.WingspanScale = Sanitize(Body.WingspanScale, 1.0f, 0.92f, 1.08f, bInputsClamped);
    Body.ShoulderWidthScale = Sanitize(Body.ShoulderWidthScale, 1.0f, 0.92f, 1.08f, bInputsClamped);
    Body.TorsoLengthScale = Sanitize(Body.TorsoLengthScale, 1.0f, 0.94f, 1.06f, bInputsClamped);
    Body.LegLengthScale = Sanitize(Body.LegLengthScale, 1.0f, 0.94f, 1.06f, bInputsClamped);
    Body.HandScale = Sanitize(Body.HandScale, 1.0f, 0.94f, 1.06f, bInputsClamped);
    Body.MassKg = Sanitize(Body.MassKg, BaselineMassKg, 45.0f, 160.0f, bInputsClamped);

    FDGThrowStyle Style = ThrowStyle;
    Style.RunUpIntensity = Sanitize(Style.RunUpIntensity, DefaultRunUp, 0.0f, 1.0f, bInputsClamped);
    Style.ReachBackAmount = Sanitize(Style.ReachBackAmount, DefaultReachBack, 0.0f, 1.0f, bInputsClamped);
    Style.TorsoRotation = Sanitize(Style.TorsoRotation, DefaultTorsoRotation, 0.0f, 1.0f, bInputsClamped);
    Style.BraceIntensity = Sanitize(Style.BraceIntensity, DefaultBrace, 0.0f, 1.0f, bInputsClamped);
    Style.Explosiveness = Sanitize(Style.Explosiveness, DefaultExplosiveness, 0.0f, 1.0f, bInputsClamped);
    Style.FollowThrough = Sanitize(Style.FollowThrough, DefaultFollowThrough, 0.0f, 1.0f, bInputsClamped);

    FDGThrowIntent Intent = ThrowIntent;
    Intent.Power01 = Sanitize(Intent.Power01, 0.0f, 0.0f, 1.0f, bInputsClamped);
    Intent.HyzerDegrees = Sanitize(Intent.HyzerDegrees, 0.0f, -45.0f, 45.0f, bInputsClamped);
    Intent.NoseDegrees = Sanitize(Intent.NoseDegrees, 0.0f, -20.0f, 20.0f, bInputsClamped);
    Intent.AimYawDegrees = Sanitize(Intent.AimYawDegrees, 0.0f, -45.0f, 45.0f, bInputsClamped);
    const double RawAbsoluteTime = ExecuteContext.GetAbsoluteTime<double>();
    const double AbsoluteTime = FMath::IsFinite(RawAbsoluteTime)
        ? FMath::Clamp(RawAbsoluteTime, 0.0, 86400.0) : 0.0;
    bInputsClamped |= !FMath::IsFinite(RawAbsoluteTime);
    const float Breath = FMath::Sin(static_cast<float>(AbsoluteTime) * 1.65f);
    const float Gaze = FMath::Sin(static_cast<float>(AbsoluteTime) * 0.37f + 0.8f);

    FProfileFactors Factors;
    Factors.Height = Body.HeightCm / BaselineHeightCm;
    Factors.Wingspan = FMath::Clamp(Factors.Height * Body.WingspanScale, 0.70f, 1.35f);
    Factors.Shoulder = FMath::Clamp(Factors.Height * Body.ShoulderWidthScale, 0.70f, 1.35f);
    Factors.Torso = FMath::Clamp(Factors.Height * Body.TorsoLengthScale, 0.70f, 1.35f);
    Factors.Leg = FMath::Clamp(Factors.Height * Body.LegLengthScale, 0.70f, 1.35f);
    Factors.Hand = FMath::Clamp(FMath::Sqrt(Factors.Height) * Body.HandScale, 0.75f, 1.30f);
    Factors.Head = FMath::Clamp(FMath::Pow(Factors.Height, 0.25f), 0.90f, 1.10f);
    Factors.Foot = FMath::Clamp(FMath::Sqrt(Factors.Height), 0.88f, 1.12f);

    const float ExpectedMassAtHeight = BaselineMassKg * FMath::Square(Factors.Height);
    Factors.Build = FMath::Clamp(FMath::Pow(Body.MassKg / FMath::Max(ExpectedMassAtHeight, 1.0f), 0.25f), 0.88f, 1.16f);

    // Solve arm length from the desired total half-span. This avoids multiplying
    // shoulder and wingspan controls into an unstable double scale.
    constexpr float BaselineHalfSpanCm = 81.0f;
    constexpr float BaselineShoulderContributionCm = 19.0f;
    constexpr float BaselineLongArmContributionCm = 56.0f;
    constexpr float BaselineGripContributionCm = 6.0f;
    const float DesiredLongArmCm = BaselineHalfSpanCm * Factors.Wingspan -
        BaselineShoulderContributionCm * Factors.Shoulder -
        BaselineGripContributionCm * Factors.Hand;
    Factors.Arm = FMath::Clamp(DesiredLongArmCm / BaselineLongArmContributionCm, 0.70f, 1.35f);

    HeightScale = Factors.Height;
    ArmLengthScale = Factors.Arm;
    LegLengthScale = Factors.Leg;

    SetFloatControl(Hierarchy, TEXT("dg_height_cm"), Body.HeightCm);
    SetFloatControl(Hierarchy, TEXT("dg_wingspan_scale"), Body.WingspanScale);
    SetFloatControl(Hierarchy, TEXT("dg_shoulder_width_scale"), Body.ShoulderWidthScale);
    SetFloatControl(Hierarchy, TEXT("dg_torso_length_scale"), Body.TorsoLengthScale);
    SetFloatControl(Hierarchy, TEXT("dg_leg_length_scale"), Body.LegLengthScale);
    SetFloatControl(Hierarchy, TEXT("dg_hand_scale"), Body.HandScale);

    const float ReachbackAlpha = ReadCurveAlpha(Hierarchy, TEXT("DG_ReachbackAlpha"));
    const float BraceAlpha = ReadCurveAlpha(Hierarchy, TEXT("DG_BraceAlpha"));
    const float ReleaseAlpha = ReadCurveAlpha(Hierarchy, TEXT("DG_ReleaseApproachAlpha"));
    const float FollowThroughAlpha = ReadCurveAlpha(Hierarchy, TEXT("DG_FollowThroughAlpha"));
    const float LeftPlantAlpha = ReadCurveAlpha(Hierarchy, TEXT("DG_FootPlant_L"));
    const float RightPlantAlpha = ReadCurveAlpha(Hierarchy, TEXT("DG_FootPlant_R"));
    const float AuthoredGazeCurveAlpha = ReadCurveAlpha(
        Hierarchy, TEXT("DG_GazeTargetAlpha"));
    const bool bProductionFullBodyMotion = bThrowActive
        && DiscGolfCharacterRigPresentation::HasProductionFullBodyMotionSignature(
            AuthoredGazeCurveAlpha);

    const float RunUpDelta = SignedStyleDelta(Style.RunUpIntensity, DefaultRunUp);
    const float ReachDelta = SignedStyleDelta(Style.ReachBackAmount, DefaultReachBack);
    const float TorsoDelta = SignedStyleDelta(Style.TorsoRotation, DefaultTorsoRotation);
    const float BraceDelta = SignedStyleDelta(Style.BraceIntensity, DefaultBrace);
    const float ExplosiveDelta = SignedStyleDelta(Style.Explosiveness, DefaultExplosiveness);
    const float FollowDelta = SignedStyleDelta(Style.FollowThrough, DefaultFollowThrough);

    const bool bTimeDiscontinuity = WorkData.LastAbsoluteTime >= 0.0 &&
        (AbsoluteTime + UE_KINDA_SMALL_NUMBER < WorkData.LastAbsoluteTime ||
         AbsoluteTime - WorkData.LastAbsoluteTime > 0.25);
    if (!bThrowActive)
    {
        WorkData.SmoothedMicroMotion = FDGThrowMicroMotionPose();
        WorkData.LastThrowPhase = EDGThrowPhase::Idle;
        WorkData.bMicroMotionInitialized = false;
    }
    else
    {
        const FDGThrowMicroMotionPose TargetMicroMotion =
            DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
                ThrowPhase, Intent, Style, Body, Handedness);
        if (!WorkData.bMicroMotionInitialized || bTimeDiscontinuity)
        {
            WorkData.SmoothedMicroMotion =
                DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
                    EDGThrowPhase::Idle, Intent, Style, Body, Handedness);
            WorkData.bMicroMotionInitialized = true;
        }
        const float PresentationDeltaSeconds = WorkData.LastAbsoluteTime >= 0.0
            ? static_cast<float>(FMath::Max(0.0, AbsoluteTime - WorkData.LastAbsoluteTime))
            : (1.0f / 60.0f);
        const float MicroBlendAlpha =
            DiscGolfCharacterRigPresentation::PhaseLocalBlendAlpha(
                ThrowPhase, PresentationDeltaSeconds);
        WorkData.SmoothedMicroMotion =
            DiscGolfCharacterRigPresentation::BlendMicroMotionPose(
                WorkData.SmoothedMicroMotion, TargetMicroMotion, MicroBlendAlpha);
        // The disc becomes authoritative at the branching release notify. Do
        // not leave visually closed fingers around a disc that has already
        // launched just because render-rate damping has not caught up yet.
        if (ThrowPhase == EDGThrowPhase::Release
            || ThrowPhase == EDGThrowPhase::FollowThrough)
        {
            WorkData.SmoothedMicroMotion.GripAlpha = FMath::Min(
                WorkData.SmoothedMicroMotion.GripAlpha,
                TargetMicroMotion.GripAlpha);
        }
        WorkData.LastThrowPhase = ThrowPhase;
    }
    const FDGThrowMicroMotionPose& MicroMotion = WorkData.SmoothedMicroMotion;

    const TArray<FRigElementKey> BoneKeys = Hierarchy->GetAllKeys(true, ERigElementType::Bone);
    TMap<FName, FTransform> SourceGlobals;
    TMap<FName, FTransform> TargetGlobals;
    SourceGlobals.Reserve(BoneKeys.Num());
    TargetGlobals.Reserve(BoneKeys.Num());
    for (const FRigElementKey& Key : BoneKeys)
    {
        SourceGlobals.Add(Key.Name, Hierarchy->GetGlobalTransform(Key, false));
    }

    for (const FRigElementKey& Key : BoneKeys)
    {
        const FName BoneName = Key.Name;
        const FTransform& SourceGlobal = SourceGlobals.FindChecked(BoneName);
        if (!IsBodyBone(BoneName))
        {
            TargetGlobals.Add(BoneName, SourceGlobal);
            continue;
        }

        const FRigElementKey ParentKey = Hierarchy->GetFirstParent(Key);
        const FTransform* SourceParent = SourceGlobals.Find(ParentKey.Name);
        const FTransform* TargetParent = TargetGlobals.Find(ParentKey.Name);
        if (!SourceParent || !TargetParent)
        {
            TargetGlobals.Add(BoneName, SourceGlobal);
            continue;
        }

        FVector ParentLocalOffset = SourceParent->GetRotation().UnrotateVector(
            SourceGlobal.GetLocation() - SourceParent->GetLocation());
        ParentLocalOffset = ScaleOffset(BoneName, ParentLocalOffset, Factors);
        if (BoneName == PelvisBone && bThrowActive)
        {
            ParentLocalOffset.Y += RunUpDelta * 3.0f * ReachbackAlpha;
        }

        const FQuat SourceLocalRotation = SourceParent->GetRotation().Inverse() * SourceGlobal.GetRotation();
        FQuat VisualDelta = bThrowActive
            ? StyleRotation(BoneName, ReachbackAlpha, BraceAlpha, ReleaseAlpha,
                FollowThroughAlpha, ReachDelta, TorsoDelta, BraceDelta,
                ExplosiveDelta, FollowDelta, Handedness, MicroMotion)
            : FQuat::Identity;
        if (!bThrowActive && (BoneName == TEXT("spine_03") || BoneName == Spine04Bone))
        {
            VisualDelta = FQuat(FVector::ForwardVector,
                FMath::DegreesToRadians(Breath * (BoneName == Spine04Bone ? 0.45f : 0.25f))) * VisualDelta;
        }
        if (!bThrowActive && BoneName == TEXT("head"))
        {
            VisualDelta = FQuat(FVector::UpVector, FMath::DegreesToRadians(Gaze * 1.2f)) * VisualDelta;
        }
        const FQuat TargetLocalRotation = (VisualDelta * SourceLocalRotation).GetNormalized();

        FTransform Target;
        Target.SetLocation(TargetParent->GetLocation() + TargetParent->GetRotation().RotateVector(ParentLocalOffset));
        Target.SetRotation((TargetParent->GetRotation() * TargetLocalRotation).GetNormalized());
        Target.SetScale3D(SourceGlobal.GetScale3D() * ShapeScale(BoneName, Factors));
        TargetGlobals.Add(BoneName, Target);
    }

    // Preserve the input pose's average ball/toe height. This makes body height
    // come from segment proportions and pelvis compensation rather than an
    // actor-wide scale, while the accepted PBIK handles the planted foot.
    const FTransform* SourceBallL = SourceGlobals.Find(LeftBallBone);
    const FTransform* SourceBallR = SourceGlobals.Find(RightBallBone);
    const FTransform* TargetBallL = TargetGlobals.Find(LeftBallBone);
    const FTransform* TargetBallR = TargetGlobals.Find(RightBallBone);
    float GroundCorrectionZ = 0.0f;
    if (SourceBallL && SourceBallR && TargetBallL && TargetBallR)
    {
        const float SourceZ = 0.5f * (SourceBallL->GetLocation().Z + SourceBallR->GetLocation().Z);
        const float TargetZ = 0.5f * (TargetBallL->GetLocation().Z + TargetBallR->GetLocation().Z);
        GroundCorrectionZ = SourceZ - TargetZ;
    }
    for (TPair<FName, FTransform>& Pair : TargetGlobals)
    {
        if (IsBodyBone(Pair.Key))
        {
            Pair.Value.AddToTranslation(FVector(0.0f, 0.0f, GroundCorrectionZ));
        }
    }

    for (const FRigElementKey& Key : BoneKeys)
    {
        if (!IsBodyBone(Key.Name))
        {
            continue;
        }
        const FTransform* Target = TargetGlobals.Find(Key.Name);
        if (!Target || Target->ContainsNaN() || Target->GetScale3D().GetMin() <= UE_SMALL_NUMBER)
        {
            bApplied = false;
            return;
        }
        Hierarchy->SetGlobalTransform(Key, *Target, false, false, false, false);
    }

    if (!bThrowActive || bTimeDiscontinuity)
    {
        WorkData.bLeftFootLocked = false;
        WorkData.bRightFootLocked = false;
    }

    // A cancelled/interrupted montage may leave a non-zero reachback curve for
    // one evaluation. Clear both controls first so idle can never inherit the
    // previous throw's hand-effector weight.
    SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_l"), 0.0f);
    SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_r"), 0.0f);

    const FPresentationHandTargets PresentationTargets =
        BuildPresentationHandTargets(TargetGlobals, Factors.Height);

    if (!bThrowActive && PresentationTargets.bValid)
    {
        // The base AnimBP intentionally starts from LocalRefPose. A symmetric,
        // reachable PBIK target turns that development T-pose into an original
        // relaxed stance without adding an animation asset or touching gameplay.
        SetTransformControl(Hierarchy, TEXT("ctrl_hand_l"), PresentationTargets.RelaxedLeft);
        SetTransformControl(Hierarchy, TEXT("ctrl_hand_r"), PresentationTargets.RelaxedRight);
        SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_l"), 0.90f);
        SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_r"), 0.90f);
    }

    if (bThrowActive)
    {
        const FTransform& SourceFootL = TargetGlobals.FindChecked(LeftFootBone);
        const FTransform& SourceFootR = TargetGlobals.FindChecked(RightFootBone);
        if (LeftPlantAlpha > 0.05f && !WorkData.bLeftFootLocked)
        {
            WorkData.LeftFootLock = SourceFootL;
            WorkData.bLeftFootLocked = true;
        }
        else if (LeftPlantAlpha <= 0.02f)
        {
            WorkData.bLeftFootLocked = false;
        }
        if (RightPlantAlpha > 0.05f && !WorkData.bRightFootLocked)
        {
            WorkData.RightFootLock = SourceFootR;
            WorkData.bRightFootLocked = true;
        }
        else if (RightPlantAlpha <= 0.02f)
        {
            WorkData.bRightFootLocked = false;
        }

        FTransform LeftFootTarget = WorkData.bLeftFootLocked
            ? DiscGolfCharacterRigPresentation::BlendPlantTarget(
                TargetGlobals.FindChecked(LeftFootBone), WorkData.LeftFootLock, LeftPlantAlpha)
            : TargetGlobals.FindChecked(LeftFootBone);
        FTransform RightFootTarget = WorkData.bRightFootLocked
            ? DiscGolfCharacterRigPresentation::BlendPlantTarget(
                TargetGlobals.FindChecked(RightFootBone), WorkData.RightFootLock, RightPlantAlpha)
            : TargetGlobals.FindChecked(RightFootBone);
        const FVector BodyUp = (TargetGlobals.FindChecked(Spine04Bone).GetLocation()
            - TargetGlobals.FindChecked(PelvisBone).GetLocation()).GetSafeNormal();
        if (!BodyUp.IsNearlyZero())
        {
            if (Handedness == EDGHandedness::Left)
            {
                RightFootTarget.AddToTranslation(-BodyUp * MicroMotion.PlantFootSettleCm
                    * DiscGolfCharacterRigPresentation::PlantLockBlendAlpha(RightPlantAlpha));
            }
            else
            {
                LeftFootTarget.AddToTranslation(-BodyUp * MicroMotion.PlantFootSettleCm
                    * DiscGolfCharacterRigPresentation::PlantLockBlendAlpha(LeftPlantAlpha));
            }
        }
        SetTransformControl(Hierarchy, TEXT("ctrl_foot_l"), LeftFootTarget);
        SetTransformControl(Hierarchy, TEXT("ctrl_foot_r"), RightFootTarget);

        const bool bLeftHanded = Handedness == EDGHandedness::Left;
        const FName ThrowHandBone = bLeftHanded ? LeftHandBone : RightHandBone;
        const FName SupportHandBone = bLeftHanded ? RightHandBone : LeftHandBone;
        FTransform ThrowHandTarget = TargetGlobals.FindChecked(ThrowHandBone);
        const FVector ReachDirection = (ThrowHandTarget.GetLocation() -
            TargetGlobals.FindChecked(Spine04Bone).GetLocation()).GetSafeNormal();
        const float PowerPresentationScale = 0.65f + 0.35f * Intent.Power01;
        const float ReachStyleContribution = FMath::Abs(ReachDelta) * ReachbackAlpha;
        if (!bProductionFullBodyMotion)
        {
            ThrowHandTarget.AddToTranslation(ReachDirection * ReachDelta * ReachbackAlpha
                * PowerPresentationScale * 8.0f);
        }
        const float Waggle = MicroMotion.WaggleAlpha
            * FMath::Sin(static_cast<float>(AbsoluteTime) * 5.2f) * 0.65f;
        const FQuat IntentRotation = FQuat(FVector::UpVector,
                FMath::DegreesToRadians(Intent.AimYawDegrees * 0.10f + Waggle))
            * FQuat(FVector::ForwardVector,
                FMath::DegreesToRadians(MicroMotion.WristRollDegrees))
            * FQuat(FVector::RightVector,
                FMath::DegreesToRadians(MicroMotion.WristPitchDegrees));
        ThrowHandTarget.SetRotation(
            (IntentRotation * ThrowHandTarget.GetRotation()).GetNormalized());

        if (PresentationTargets.bValid)
        {
            // The authored montage curves are evaluated in this rig even when a
            // host AnimBP does not bind the optional ThrowPhase enum pin.
            const bool bFollowThrough = ThrowPhase == EDGThrowPhase::FollowThrough ||
                FollowThroughAlpha > 0.05f;
            const bool bRecovery = ThrowPhase == EDGThrowPhase::Recovery;
            const EDGThrowPhase IKPhase = bRecovery
                ? EDGThrowPhase::Recovery
                : (bFollowThrough ? EDGThrowPhase::FollowThrough : ThrowPhase);
            const FDGUpperLimbIKPolicy IKPolicy =
                DiscGolfCharacterRigPresentation::BuildUpperLimbIKPolicy(
                    bProductionFullBodyMotion, IKPhase, MicroMotion,
                    ReachStyleContribution);
            const FTransform SupportRelaxed = bLeftHanded
                ? PresentationTargets.RelaxedRight : PresentationTargets.RelaxedLeft;
            const FTransform SupportBalance = bLeftHanded
                ? PresentationTargets.BalanceRight : PresentationTargets.BalanceLeft;
            FTransform SupportTarget = BlendPresentationTarget(
                SupportRelaxed, SupportBalance, MicroMotion.OffArmBalanceAlpha);
            const FTransform ThrowFollowTarget = bLeftHanded
                ? PresentationTargets.FollowThroughLeft
                : PresentationTargets.FollowThroughRight;
            const FTransform ThrowRelaxedTarget = bLeftHanded
                ? PresentationTargets.RelaxedLeft : PresentationTargets.RelaxedRight;
            FTransform FinalThrowTarget = IKPolicy.bUseSyntheticFollowTarget
                ? ThrowFollowTarget
                : ThrowHandTarget;
            if (IKPolicy.bPreserveAuthoredThrowTrajectory)
            {
                // Dense production animation already contains the off-arm arc
                // and follow-through. Keep its hand positions, allowing only a
                // tiny support-target bias and wrist/contact correction.
                SupportTarget = BlendPresentationTarget(
                    TargetGlobals.FindChecked(SupportHandBone), SupportTarget,
                    IKPolicy.SupportTargetBlendAlpha);
                FinalThrowTarget = ThrowHandTarget;
            }
            else if (bRecovery)
            {
                FinalThrowTarget = BlendPresentationTarget(
                    ThrowFollowTarget, ThrowRelaxedTarget, MicroMotion.RecoveryRelaxAlpha);
                SupportTarget = BlendPresentationTarget(
                    SupportTarget, SupportRelaxed, MicroMotion.RecoveryRelaxAlpha);
            }

            if (bLeftHanded)
            {
                SetTransformControl(Hierarchy, TEXT("ctrl_hand_l"), FinalThrowTarget);
                SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_l"),
                    IKPolicy.ThrowHandAlpha);
                SetTransformControl(Hierarchy, TEXT("ctrl_hand_r"), SupportTarget);
                SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_r"),
                    IKPolicy.SupportHandAlpha);
            }
            else
            {
                SetTransformControl(Hierarchy, TEXT("ctrl_hand_r"), FinalThrowTarget);
                SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_r"),
                    IKPolicy.ThrowHandAlpha);
                SetTransformControl(Hierarchy, TEXT("ctrl_hand_l"), SupportTarget);
                SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_l"),
                    IKPolicy.SupportHandAlpha);
            }
        }
        else
        {
            SetTransformControl(Hierarchy,
                bLeftHanded ? TEXT("ctrl_hand_l") : TEXT("ctrl_hand_r"), ThrowHandTarget);
            SetFloatControl(Hierarchy,
                bLeftHanded ? TEXT("dg_hand_ik_alpha_l") : TEXT("dg_hand_ik_alpha_r"),
                bProductionFullBodyMotion
                    ? DiscGolfCharacterRigPresentation::BuildUpperLimbIKPolicy(
                        true, ThrowPhase, MicroMotion, ReachStyleContribution)
                        .ThrowHandAlpha
                    : FMath::Clamp(
                        0.08f + MicroMotion.DiscPlaneStabilityAlpha * 0.16f,
                        0.0f, 0.24f));
        }
    }

    WorkData.LastAbsoluteTime = AbsoluteTime;
    bApplied = true;
}
