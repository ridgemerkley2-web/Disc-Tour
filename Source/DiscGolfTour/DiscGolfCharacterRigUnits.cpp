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

FQuat StyleRotation(const FName BoneName, float ReachbackAlpha, float BraceAlpha,
    float ReleaseAlpha, float FollowThroughAlpha, float ReachDelta, float TorsoDelta,
    float BraceDelta, float ExplosiveDelta, float FollowDelta)
{
    if (BoneName == PelvisBone)
    {
        // A vertical pelvis translation would be canceled by the foot-grounding
        // correction below. A small local brace pitch stays visible while PBIK
        // preserves the accepted foot contacts.
        return FQuat(FVector::ForwardVector,
            FMath::DegreesToRadians(BraceDelta * 4.0f * BraceAlpha));
    }
    float Degrees = 0.0f;
    if (BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03") || BoneName == Spine04Bone)
    {
        const float Distribution = BoneName == Spine04Bone ? 0.45f : (BoneName == TEXT("spine_03") ? 0.35f : 0.20f);
        Degrees += TorsoDelta * 8.0f * Distribution * FMath::Max(ReachbackAlpha, ReleaseAlpha);
        Degrees += FollowDelta * 10.0f * Distribution * FollowThroughAlpha;
    }
    if (BoneName == TEXT("clavicle_r"))
    {
        Degrees += ReachDelta * 6.0f * ReachbackAlpha;
        Degrees += ExplosiveDelta * 4.0f * ReleaseAlpha;
    }
    return FQuat(FVector::UpVector, FMath::DegreesToRadians(Degrees));
}
} // namespace DiscGolfCharacterRig

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

    const float RunUpDelta = SignedStyleDelta(Style.RunUpIntensity, DefaultRunUp);
    const float ReachDelta = SignedStyleDelta(Style.ReachBackAmount, DefaultReachBack);
    const float TorsoDelta = SignedStyleDelta(Style.TorsoRotation, DefaultTorsoRotation);
    const float BraceDelta = SignedStyleDelta(Style.BraceIntensity, DefaultBrace);
    const float ExplosiveDelta = SignedStyleDelta(Style.Explosiveness, DefaultExplosiveness);
    const float FollowDelta = SignedStyleDelta(Style.FollowThrough, DefaultFollowThrough);

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
        const FQuat VisualDelta = bThrowActive
            ? StyleRotation(BoneName, ReachbackAlpha, BraceAlpha, ReleaseAlpha,
                FollowThroughAlpha, ReachDelta, TorsoDelta, BraceDelta,
                ExplosiveDelta, FollowDelta)
            : FQuat::Identity;
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

    const double AbsoluteTime = ExecuteContext.GetAbsoluteTime<double>();
    const bool bTimeDiscontinuity = WorkData.LastAbsoluteTime >= 0.0 &&
        (AbsoluteTime + UE_KINDA_SMALL_NUMBER < WorkData.LastAbsoluteTime ||
         AbsoluteTime - WorkData.LastAbsoluteTime > 0.25);
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

    if (bThrowActive)
    {
        const FTransform& SourceFootL = SourceGlobals.FindChecked(LeftFootBone);
        const FTransform& SourceFootR = SourceGlobals.FindChecked(RightFootBone);
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

        SetTransformControl(Hierarchy, TEXT("ctrl_foot_l"), WorkData.bLeftFootLocked
            ? WorkData.LeftFootLock : TargetGlobals.FindChecked(LeftFootBone));
        SetTransformControl(Hierarchy, TEXT("ctrl_foot_r"), WorkData.bRightFootLocked
            ? WorkData.RightFootLock : TargetGlobals.FindChecked(RightFootBone));

        FTransform RightHandTarget = TargetGlobals.FindChecked(RightHandBone);
        const FVector ReachDirection = (RightHandTarget.GetLocation() -
            TargetGlobals.FindChecked(Spine04Bone).GetLocation()).GetSafeNormal();
        RightHandTarget.AddToTranslation(ReachDirection * ReachDelta * ReachbackAlpha * 8.0f);
        SetTransformControl(Hierarchy, TEXT("ctrl_hand_r"), RightHandTarget);
        SetTransformControl(Hierarchy, TEXT("ctrl_hand_l"), TargetGlobals.FindChecked(LeftHandBone));
        SetFloatControl(Hierarchy, TEXT("dg_hand_ik_alpha_r"),
            FMath::Clamp(FMath::Abs(ReachDelta) * ReachbackAlpha * 0.35f, 0.0f, 0.35f));
    }

    WorkData.LastAbsoluteTime = AbsoluteTime;
    (void)Handedness; // Session 4 stores handedness but does not claim an LHBH montage.
    (void)ThrowPhase;
    bApplied = true;
}
