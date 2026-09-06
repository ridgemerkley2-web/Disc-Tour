#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCharacterRigUnits.h"
#include "../DiscGolfMetaHumanRetargetAnimInstance.h"
#include "Retargeter/IKRetargetOps.h"
#include "Retargeter/IKRetargeter.h"
#include "Retargeter/RetargetOps/CurveRemapOp.h"
#include "Retargeter/RetargetOps/FKChainsOp.h"
#include "Retargeter/RetargetOps/PelvisMotionOp.h"
#include "Retargeter/RetargetOps/RootMotionGeneratorOp.h"
#include "Retargeter/RetargetOps/RunIKRigOp.h"
#include "TwoBoneIK.h"
#include "UObject/UObjectGlobals.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BMetaHumanRetargetPolicyTest,
    "DiscGolfTour.Character.Session8B.MetaHuman.RetargetPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BMetaHumanRetargetPolicyTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    UIKRetargeter* Retargeter = LoadObject<UIKRetargeter>(
        nullptr,
        TEXT("/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman."
             "RTG_DGMaster_To_MetaHuman"));
    TestNotNull(TEXT("The canonical Session 8B retargeter loads"), Retargeter);
    if (!Retargeter)
    {
        return false;
    }

    const UScriptStruct* ExpectedTypes[] = {
        FIKRetargetPelvisMotionOp::StaticStruct(),
        FIKRetargetFKChainsOp::StaticStruct(),
        FIKRetargetRunIKRigOp::StaticStruct(),
        FIKRetargetRootMotionOp::StaticStruct(),
        FIKRetargetCurveRemapOp::StaticStruct(),
    };
    const bool ExpectedEnabled[] = {true, true, false, false, true};
    const TArray<FInstancedStruct>& Ops = Retargeter->GetRetargetOps();
    TestEqual(TEXT("The canonical retargeter has exactly five typed ops"),
        Ops.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes)));
    if (Ops.Num() != static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes)))
    {
        return false;
    }

    for (int32 Index = 0; Index < Ops.Num(); ++Index)
    {
        const FIKRetargetOpBase* Op = Ops[Index].GetPtr<FIKRetargetOpBase>();
        TestNotNull(
            *FString::Printf(TEXT("Retarget op %d is readable"), Index), Op);
        if (!Op)
        {
            continue;
        }
        TestTrue(
            *FString::Printf(TEXT("Retarget op %d has the exact type"), Index),
            Op->GetType() == ExpectedTypes[Index]);
        TestEqual(
            *FString::Printf(TEXT("Retarget op %d has the exact enabled state"), Index),
            Op->IsEnabled(), ExpectedEnabled[Index]);
    }

    const FIKRetargetOpBase* RootMotionOp =
        Ops[3].GetPtr<FIKRetargetOpBase>();
    const FIKRetargetRootMotionOpSettings* RootMotionSettings =
        RootMotionOp
        && RootMotionOp->GetType() == FIKRetargetRootMotionOp::StaticStruct()
            ? static_cast<const FIKRetargetRootMotionOpSettings*>(
                RootMotionOp->GetSettingsConst())
            : nullptr;
    TestNotNull(
        TEXT("The canonical Root Motion settings are readable"),
        RootMotionSettings);
    if (RootMotionSettings)
    {
        TestEqual(
            TEXT("Root Motion reads the DG source root"),
            RootMotionSettings->SourceRoot.BoneName,
            FName(TEXT("root")));
        TestEqual(
            TEXT("Root Motion targets the MetaHuman retarget root"),
            RootMotionSettings->TargetRoot.BoneName,
            FName(TEXT("pelvis")));
        TestEqual(
            TEXT("Root Motion targets the MetaHuman pelvis"),
            RootMotionSettings->TargetPelvis.BoneName,
            FName(TEXT("pelvis")));
        TestTrue(
            TEXT("Root Motion retains CopyFromSourceRoot settings while disabled"),
            RootMotionSettings->RootMotionSource
                == ERootMotionSource::CopyFromSourceRoot);
    }
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BMetaHumanPresentationMathTest,
    "DiscGolfTour.Character.Session8B.MetaHuman.PresentationMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BMetaHumanPresentationMathTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace DiscGolfMetaHumanPresentation;

    TestEqual(TEXT("Packaged acceptance shares the exact runtime correction mode"),
        FString(HandCorrectionMode),
        FString(TEXT(
            "TARGET_RIGHT_ARM_LENGTH_PRESERVING_TWO_BONE_TO_SOURCE_GRIP_TRANSFORM")));
    TestEqual(TEXT("No correction need produces no blend"),
        ComputeCorrectionBlendAlpha(0.0f, 0.0f), 0.0f);
    const float SmallBlend = ComputeCorrectionBlendAlpha(2.0f, 0.0f);
    const float LargeBlend = ComputeCorrectionBlendAlpha(20.0f, 90.0f);
    TestTrue(TEXT("Correction blend grows with error"), LargeBlend > SmallBlend);
    TestEqual(TEXT("Large contact error reaches full correction"), LargeBlend, 1.0f);
    TestEqual(TEXT("Non-finite correction need fails closed"),
        ComputeCorrectionBlendAlpha(std::numeric_limits<float>::quiet_NaN(), 0.0f), 0.0f);

    const FVector Shoulder(0.0, 0.0, 0.0);
    const FVector CurrentHand(50.0, 0.0, 0.0);
    const FVector Capped = ClampEffectorTarget(
        Shoulder, CurrentHand, FVector(150.0, 0.0, 0.0), 5.0f, 200.0f);
    TestTrue(TEXT("Effector correction is capped at eighty centimetres"),
        FMath::IsNearlyEqual(
            FVector::Distance(Capped, CurrentHand), 80.0f, 0.001f));
    const FVector Bounded = ClampEffectorTarget(
        Shoulder, CurrentHand, FVector(100.0, 0.0, 0.0), 5.0f, 55.0f);
    TestTrue(TEXT("Effector remains inside limb reach"), Bounded.Size() < 55.0f);
    const FVector NoClampDesired(52.0, 4.0, 0.0);
    TestTrue(TEXT("In-range target is unchanged"),
        ClampEffectorTarget(
            Shoulder, CurrentHand, NoClampDesired, 5.0f, 80.0f)
            .Equals(NoClampDesired, 0.001f));
    const FVector ReachOnlyDesired(60.0, 0.0, 0.0);
    const FVector ReachOnly = ClampEffectorTarget(
        Shoulder, CurrentHand, ReachOnlyDesired, 5.0f, 55.0f);
    TestTrue(TEXT("Reach-only fixture stays below the distance cap"),
        FVector::Distance(CurrentHand, ReachOnlyDesired) < 80.0f);
    TestTrue(TEXT("Reach-only fixture reaches the safe annulus boundary"),
        FMath::IsNearlyEqual(ReachOnly.Size(), 54.95f, 0.001f));
    TestTrue(TEXT("Reach-bounded fixture also remains inside the correction cap"),
        FVector::Distance(CurrentHand, Bounded) <= 80.001f
            && FMath::IsNearlyEqual(Bounded.Size(), 54.95f, 0.001f));

    const FVector JointConstraintCurrent(30.0, 0.0, 0.0);
    const FVector JointConstraintTarget = ClampEffectorTarget(
        Shoulder,
        JointConstraintCurrent,
        FVector(-100.0, 0.0, 0.0),
        20.0f,
        55.0f);
    TestTrue(TEXT("Annulus projection cannot escape the correction cap"),
        FVector::Distance(
            JointConstraintCurrent, JointConstraintTarget) <= 80.001f);
    TestTrue(TEXT("Joint cap solution remains inside the safe reach annulus"),
        JointConstraintTarget.Size() >= 20.049f
            && JointConstraintTarget.Size() <= 54.951f);

    const FVector OffsetShoulder(100.0, 0.0, 0.0);
    const FVector CoincidentHand = OffsetShoulder;
    const FVector OffsetDesiredHand(40.0, 20.0, 0.0);
    const FVector CoincidentCenterTarget = ClampEffectorTarget(
        OffsetShoulder,
        CoincidentHand,
        OffsetDesiredHand,
        47.95f,
        80.05f);
    const float CoincidentCenterCorrection = FVector::Distance(
        CoincidentHand, CoincidentCenterTarget);
    const float CoincidentCenterReach = FVector::Distance(
        OffsetShoulder, CoincidentCenterTarget);
    TestTrue(TEXT("Coincident shoulder and hand preserve the correction cap"),
        CoincidentCenterCorrection <= 80.001f);
    TestTrue(TEXT("Coincident shoulder and hand remain inside the supplied reach annulus"),
        CoincidentCenterReach >= 47.95f
            && CoincidentCenterReach <= 80.05f);
    TestTrue(TEXT("Coincident-center correction points toward the shoulder-relative desired hand"),
        FVector::DotProduct(
            (CoincidentCenterTarget - OffsetShoulder).GetSafeNormal(),
            (OffsetDesiredHand - OffsetShoulder).GetSafeNormal()) > 0.9999f);

    const FVector V006ReachbackCurrent = FVector::ZeroVector;
    const FVector V006ReachbackDesired(41.888f, 0.0f, 0.0f);
    const FVector V006ReachbackTarget = ClampEffectorTarget(
        Shoulder,
        V006ReachbackCurrent,
        V006ReachbackDesired,
        5.0f,
        60.0f);
    TestTrue(TEXT("Reachable v006 reachback grip is not distance-capped"),
        V006ReachbackTarget.Equals(V006ReachbackDesired, 0.001f));

    const FVector SourceShoulder(10.0f, 20.0f, 30.0f);
    const FVector SourceElbow(40.0f, 20.0f, 30.0f);
    const FVector SourceHand(60.0f, 20.0f, 30.0f);
    const float SourceReachFraction = ComputeSourceArmReachFraction(
        SourceShoulder, SourceElbow, SourceHand);
    TestTrue(TEXT("Source reach fraction uses the actual source chain"),
        FMath::IsNearlyEqual(SourceReachFraction, 1.0f, 0.0001f));
    const FVector UnrelatedTargetGrip(-500.0f, 800.0f, 120.0f);
    const float ContaminatedReachFraction = (
        FVector::Distance(SourceShoulder, SourceElbow)
        + FVector::Distance(SourceElbow, UnrelatedTargetGrip)) > 0.0f
        ? FVector::Distance(SourceShoulder, UnrelatedTargetGrip)
            / (FVector::Distance(SourceShoulder, SourceElbow)
                + FVector::Distance(SourceElbow, UnrelatedTargetGrip))
        : 0.0f;
    TestTrue(TEXT("Target grip offsets cannot change source reach fraction"),
        FMath::IsNearlyEqual(
            ComputeSourceArmReachFraction(
                SourceShoulder, SourceElbow, SourceHand),
            SourceReachFraction,
            0.0001f)
        && !FMath::IsNearlyEqual(
            ContaminatedReachFraction, SourceReachFraction, 0.0001f));

    const FVector CurrentShoulder(0.0f, 0.0f, 0.0f);
    const FVector FullShoulder(12.0f, 6.0f, -3.0f);
    const FVector ShoulderZero = BlendShoulderRelocation(
        CurrentShoulder, FullShoulder, 0.0f);
    const FVector ShoulderHalf = BlendShoulderRelocation(
        CurrentShoulder, FullShoulder, 0.5f);
    const FVector ShoulderFull = BlendShoulderRelocation(
        CurrentShoulder, FullShoulder, 1.0f);
    TestTrue(TEXT("Zero correction alpha preserves the retargeted shoulder"),
        ShoulderZero.Equals(CurrentShoulder, 0.0001f));
    TestTrue(TEXT("Partial shoulder relocation is monotonic and proportional"),
        ShoulderHalf.Equals(FullShoulder * 0.5f, 0.0001f)
            && FVector::Distance(CurrentShoulder, ShoulderZero)
                < FVector::Distance(CurrentShoulder, ShoulderHalf)
            && FVector::Distance(CurrentShoulder, ShoulderHalf)
                < FVector::Distance(CurrentShoulder, ShoulderFull));
    TestTrue(TEXT("Full correction alpha reaches the solved shoulder"),
        ShoulderFull.Equals(FullShoulder, 0.0001f));

    const FQuat CurrentOrientation = FQuat::Identity;
    const FQuat DesiredOrientation(
        FVector::UpVector,
        FMath::DegreesToRadians(120.0f));
    const FQuat AlphaZero = BlendHandOrientation(
        CurrentOrientation, DesiredOrientation, 0.0f);
    const FQuat AlphaHalf = BlendHandOrientation(
        CurrentOrientation, DesiredOrientation, 0.5f);
    const FQuat AlphaFull = BlendHandOrientation(
        CurrentOrientation, DesiredOrientation, 1.0f);
    TestTrue(TEXT("Zero orientation alpha preserves retarget output"),
        FMath::RadiansToDegrees(
            AlphaZero.AngularDistance(CurrentOrientation)) < 0.001f);
    TestTrue(TEXT("Intermediate orientation alpha follows the shortest arc"),
        FMath::IsNearlyEqual(
            FMath::RadiansToDegrees(
                AlphaHalf.AngularDistance(DesiredOrientation)),
            60.0f,
            0.01f));
    TestTrue(TEXT("Full orientation alpha reaches desired grip orientation"),
        FMath::RadiansToDegrees(
            AlphaFull.AngularDistance(DesiredOrientation)) < 0.001f);
    const FQuat RetargetedMetaHumanOrientation(
        FVector::ForwardVector,
        FMath::DegreesToRadians(25.0f));
    const FQuat IncompatibleSourceBoneOrientation(
        FVector::RightVector,
        FMath::DegreesToRadians(140.0f));
    const FQuat FallbackOrientation = SelectAnatomicalHandOrientation(
        RetargetedMetaHumanOrientation,
        IncompatibleSourceBoneOrientation,
        true);
    TestTrue(TEXT("A source-only grip binding preserves target hand-bone axes"),
        FMath::RadiansToDegrees(FallbackOrientation.AngularDistance(
            RetargetedMetaHumanOrientation)) < 0.001f);
    const FQuat SocketBoundOrientation = SelectAnatomicalHandOrientation(
        RetargetedMetaHumanOrientation,
        IncompatibleSourceBoneOrientation,
        false);
    TestTrue(TEXT("A real target grip socket retains full grip orientation authority"),
        FMath::RadiansToDegrees(SocketBoundOrientation.AngularDistance(
            IncompatibleSourceBoneOrientation)) < 0.001f);

    const FQuat OriginalForearmOrientation(
        FVector(1.0, 2.0, 3.0).GetSafeNormal(),
        FMath::DegreesToRadians(37.0f));
    const FQuat OriginalHandLocalOrientation(
        FVector(-2.0, 1.0, 0.5).GetSafeNormal(),
        FMath::DegreesToRadians(23.0f));
    const FQuat OriginalHandOrientation = (
        OriginalForearmOrientation * OriginalHandLocalOrientation)
        .GetNormalized();
    const FQuat IdentityDeltaCarriedHand =
        CarryHandOrientationWithSolvedForearm(
            OriginalForearmOrientation,
            OriginalHandOrientation,
            OriginalForearmOrientation);
    TestTrue(TEXT("An identity forearm solve preserves the original hand orientation"),
        FMath::RadiansToDegrees(IdentityDeltaCarriedHand.AngularDistance(
            OriginalHandOrientation)) < 0.001f);

    const FQuat SolvedForearmOrientation(
        FVector(0.25, -1.0, 2.0).GetSafeNormal(),
        FMath::DegreesToRadians(81.0f));
    const FQuat CarriedHandOrientation =
        CarryHandOrientationWithSolvedForearm(
            OriginalForearmOrientation,
            OriginalHandOrientation,
            SolvedForearmOrientation);
    const FQuat OriginalHandRelativeToForearm = (
        OriginalForearmOrientation.Inverse() * OriginalHandOrientation)
        .GetNormalized();
    const FQuat SolvedHandRelativeToForearm = (
        SolvedForearmOrientation.Inverse() * CarriedHandOrientation)
        .GetNormalized();
    TestTrue(TEXT("Fallback carry preserves the hand-local-to-forearm orientation"),
        FMath::RadiansToDegrees(
            SolvedHandRelativeToForearm.AngularDistance(
                OriginalHandRelativeToForearm)) < 0.001f);

    const FQuat UnnormalizedOriginalForearm(
        OriginalForearmOrientation.X * 3.0,
        OriginalForearmOrientation.Y * 3.0,
        OriginalForearmOrientation.Z * 3.0,
        OriginalForearmOrientation.W * 3.0);
    const FQuat UnnormalizedOriginalHand(
        OriginalHandOrientation.X * 2.0,
        OriginalHandOrientation.Y * 2.0,
        OriginalHandOrientation.Z * 2.0,
        OriginalHandOrientation.W * 2.0);
    const FQuat UnnormalizedSolvedForearm(
        SolvedForearmOrientation.X * 4.0,
        SolvedForearmOrientation.Y * 4.0,
        SolvedForearmOrientation.Z * 4.0,
        SolvedForearmOrientation.W * 4.0);
    const FQuat NormalizedCarriedHand =
        CarryHandOrientationWithSolvedForearm(
            UnnormalizedOriginalForearm,
            UnnormalizedOriginalHand,
            UnnormalizedSolvedForearm);
    TestTrue(TEXT("Fallback carry returns finite output for finite non-unit input"),
        !NormalizedCarriedHand.ContainsNaN());
    TestTrue(TEXT("Fallback carry normalizes its output"),
        NormalizedCarriedHand.IsNormalized());
    const FQuat NonFiniteOrientation(
        std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 1.0);
    const FQuat InvalidCarriedHand =
        CarryHandOrientationWithSolvedForearm(
            OriginalForearmOrientation,
            OriginalHandOrientation,
            NonFiniteOrientation);
    TestTrue(TEXT("Non-finite fallback carry fails closed to normalized identity"),
        !InvalidCarriedHand.ContainsNaN()
            && InvalidCarriedHand.IsNormalized()
            && FMath::RadiansToDegrees(InvalidCarriedHand.AngularDistance(
                FQuat::Identity)) < 0.001f);

    const FVector Pole = BuildStableElbowPole(
        Shoulder, FVector(25.0, 10.0, 0.0), FVector(50.0, 0.0, 0.0),
        FVector::RightVector, 30.0f);
    TestTrue(TEXT("Elbow pole is finite"), !Pole.ContainsNaN());
    TestTrue(TEXT("Elbow pole preserves the existing bend side"), Pole.Y > 0.0f);
    const FVector SourceGuidedPole = BuildSourceGuidedElbowPole(
        Shoulder,
        FVector(25.0, -10.0, 0.0),
        FVector(50.0, 0.0, 0.0),
        FVector(100.0, 100.0, 0.0),
        FVector(125.0, 112.0, 0.0),
        FVector::RightVector,
        30.0f);
    TestTrue(TEXT("Source arm bend direction prevents an inward target elbow flip"),
        SourceGuidedPole.Y > 0.0f);
    const FVector DegenerateGuidePole = BuildSourceGuidedElbowPole(
        Shoulder,
        FVector(25.0, -10.0, 0.0),
        FVector(50.0, 0.0, 0.0),
        FVector(100.0, 100.0, 0.0),
        FVector(100.0, 100.0, 0.0),
        FVector::RightVector,
        30.0f);
    TestTrue(TEXT("Degenerate source elbow guidance falls back deterministically"),
        DegenerateGuidePole.Y < 0.0f);

    const FVector IntermediateEffector = BuildLengthPreservingEffectorTarget(
        Shoulder, CurrentHand, Bounded, 0.5f, 5.0f, 55.0f);
    TestTrue(TEXT("Correction blends the effector target before solving"),
        IntermediateEffector.X > CurrentHand.X
            && IntermediateEffector.X < Bounded.X);
    TestTrue(TEXT("Intermediate effector stays inside the reach annulus"),
        IntermediateEffector.Size() > 5.0f
            && IntermediateEffector.Size() < 55.0f);
    TestEqual(TEXT("Non-finite target blend fails closed to the current hand"),
        BuildLengthPreservingEffectorTarget(
            Shoulder, CurrentHand, Bounded,
            std::numeric_limits<float>::quiet_NaN(), 5.0f, 55.0f),
        CurrentHand);

    FTransform SolvedUpper(FQuat::Identity, Shoulder);
    FTransform SolvedLower(FQuat::Identity, FVector(25.0, 15.0, 0.0));
    FTransform SolvedHand(FQuat::Identity, CurrentHand);
    const FTransform OriginalUpper = SolvedUpper;
    const FTransform OriginalLower = SolvedLower;
    const FTransform OriginalHand = SolvedHand;
    const FVector SolvePole = BuildStableElbowPole(
        SolvedUpper.GetLocation(), SolvedLower.GetLocation(), IntermediateEffector,
        FVector::RightVector,
        FVector::Distance(SolvedUpper.GetLocation(), SolvedLower.GetLocation()));
    AnimationCore::SolveTwoBoneIK(
        SolvedUpper, SolvedLower, SolvedHand, SolvePole, IntermediateEffector,
        false, 1.0, 1.0);
    TestTrue(TEXT("Intermediate-target solve preserves the upper-arm segment"),
        ComputeSegmentLengthRatioError(
            OriginalUpper.GetLocation(), OriginalLower.GetLocation(),
            SolvedUpper.GetLocation(), SolvedLower.GetLocation()) < 0.0001f);
    TestTrue(TEXT("Intermediate-target solve preserves the forearm segment"),
        ComputeSegmentLengthRatioError(
            OriginalLower.GetLocation(), OriginalHand.GetLocation(),
            SolvedLower.GetLocation(), SolvedHand.GetLocation()) < 0.0001f);

    const FVector AlternateElbow(0.0, 30.0, 0.0);
    const FVector AlternateHand(0.0, 50.0, 0.0);
    const FVector BlendedElbow = FMath::Lerp(FVector(30.0, 0.0, 0.0),
        AlternateElbow, 0.5f);
    const FVector BlendedHand = FMath::Lerp(FVector(50.0, 0.0, 0.0),
        AlternateHand, 0.5f);
    TestTrue(TEXT("Independent component-space blending shrinks the upper arm"),
        ComputeSegmentLengthRatioError(
            Shoulder, FVector(30.0, 0.0, 0.0), Shoulder, BlendedElbow) > 0.25f);
    TestTrue(TEXT("Independent component-space blending shrinks the forearm"),
        ComputeSegmentLengthRatioError(
            FVector(30.0, 0.0, 0.0), FVector(50.0, 0.0, 0.0),
            BlendedElbow, BlendedHand) > 0.25f);
    TestEqual(TEXT("Degenerate source segment fails closed"),
        ComputeSegmentLengthRatioError(
            Shoulder, Shoulder, Shoulder, FVector::ForwardVector),
        TNumericLimits<float>::Max());

    TestEqual(TEXT("Zero plant curve remains animated"),
        DiscGolfCharacterRigPresentation::PlantLockBlendAlpha(0.0f), 0.0f);
    TestEqual(TEXT("Full plant curve reaches the lock"),
        DiscGolfCharacterRigPresentation::PlantLockBlendAlpha(1.0f), 1.0f);
    const FTransform Animated(FQuat::Identity, FVector::ZeroVector);
    const FTransform Locked(FQuat::Identity, FVector(10.0, 0.0, 0.0));
    const FTransform Mid = DiscGolfCharacterRigPresentation::BlendPlantTarget(
        Animated, Locked, 0.5f);
    TestTrue(TEXT("Plant blend remains between animation and lock"),
        Mid.GetLocation().X > 0.0 && Mid.GetLocation().X < 10.0);

    TestEqual(TEXT("MetaHuman correction peaks through acceleration"),
        PhaseCorrectionWeight(EDGThrowPhase::Acceleration), 1.0f);
    TestEqual(TEXT("Aim pre-warms exact cosmetic grip contact"),
        PhaseCorrectionWeight(EDGThrowPhase::Aim), 1.0f);
    TestEqual(TEXT("Run-up retains exact cosmetic grip contact"),
        PhaseCorrectionWeight(EDGThrowPhase::RunUp), 1.0f);
    TestEqual(TEXT("Reachback maintains exact cosmetic grip contact"),
        PhaseCorrectionWeight(EDGThrowPhase::ReachBack), 1.0f);
    TestEqual(TEXT("Plant maintains exact cosmetic grip contact"),
        PhaseCorrectionWeight(EDGThrowPhase::Plant), 1.0f);
    TestTrue(TEXT("MetaHuman correction damps after follow-through"),
        PhaseCorrectionWeight(EDGThrowPhase::Recovery)
            < PhaseCorrectionWeight(EDGThrowPhase::FollowThrough));
    const float CorrectionAttack = SmoothCorrectionWeight(0.0f, 1.0f, 1.0f / 60.0f);
    const float CorrectionRelease = SmoothCorrectionWeight(1.0f, 0.0f, 1.0f / 60.0f);
    TestTrue(TEXT("MetaHuman correction attack is continuous"),
        CorrectionAttack > 0.0f && CorrectionAttack < 1.0f);
    TestTrue(TEXT("MetaHuman correction release is damped"),
        CorrectionRelease > 0.0f && CorrectionRelease < 1.0f
            && (1.0f - CorrectionRelease) < CorrectionAttack);
    TestEqual(TEXT("Non-finite MetaHuman correction delta preserves current state"),
        SmoothCorrectionWeight(0.5f, 1.0f,
            std::numeric_limits<float>::quiet_NaN()),
        0.5f);

    float Attack60Hz = 0.0f;
    float Attack120Hz = 0.0f;
    for (int32 Step = 0; Step < 60; ++Step)
    {
        const float Previous = Attack60Hz;
        Attack60Hz = SmoothCorrectionWeight(
            Attack60Hz, 1.0f, 1.0f / 60.0f);
        TestTrue(TEXT("60 Hz contact prewarm remains monotonic"),
            Attack60Hz >= Previous && Attack60Hz <= 1.0f);
    }
    for (int32 Step = 0; Step < 120; ++Step)
    {
        const float Previous = Attack120Hz;
        Attack120Hz = SmoothCorrectionWeight(
            Attack120Hz, 1.0f, 1.0f / 120.0f);
        TestTrue(TEXT("120 Hz contact prewarm remains monotonic"),
            Attack120Hz >= Previous && Attack120Hz <= 1.0f);
    }
    TestTrue(TEXT("Contact prewarm agrees after one second at 60 and 120 Hz"),
        FMath::IsNearlyEqual(Attack60Hz, Attack120Hz, 0.0001f));

    float Release60Hz = 1.0f;
    float Release120Hz = 1.0f;
    for (int32 Step = 0; Step < 60; ++Step)
    {
        const float Previous = Release60Hz;
        Release60Hz = SmoothCorrectionWeight(
            Release60Hz, 0.65f, 1.0f / 60.0f);
        TestTrue(TEXT("60 Hz follow-through release remains monotonic"),
            Release60Hz <= Previous && Release60Hz >= 0.65f);
    }
    for (int32 Step = 0; Step < 120; ++Step)
    {
        const float Previous = Release120Hz;
        Release120Hz = SmoothCorrectionWeight(
            Release120Hz, 0.65f, 1.0f / 120.0f);
        TestTrue(TEXT("120 Hz follow-through release remains monotonic"),
            Release120Hz <= Previous && Release120Hz >= 0.65f);
    }
    TestTrue(TEXT("Follow-through release agrees after one second at 60 and 120 Hz"),
        FMath::IsNearlyEqual(Release60Hz, Release120Hz, 0.0001f));

    FDGThrowIntent Intent;
    Intent.ThrowType = EDGThrowType::Backhand;
    Intent.Power01 = 0.86f;
    Intent.HyzerDegrees = 18.0f;
    Intent.NoseDegrees = 4.0f;
    FDGThrowStyle Style;
    FDGBodyProfile Body;
    const EDGThrowPhase Phases[] = {
        EDGThrowPhase::Idle,
        EDGThrowPhase::Aim,
        EDGThrowPhase::RunUp,
        EDGThrowPhase::ReachBack,
        EDGThrowPhase::Plant,
        EDGThrowPhase::Acceleration,
        EDGThrowPhase::Release,
        EDGThrowPhase::FollowThrough,
        EDGThrowPhase::Recovery,
    };
    for (EDGThrowPhase Phase : Phases)
    {
        const FDGThrowMicroMotionPose Pose =
            DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
                Phase, Intent, Style, Body, EDGHandedness::Right);
        TestTrue(*FString::Printf(TEXT("Phase %d micro-motion stays finite and bounded"),
                static_cast<int32>(Phase)),
            DiscGolfCharacterRigPresentation::IsMicroMotionPoseFiniteAndBounded(Pose));
    }

    const FDGThrowMicroMotionPose Reach =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::ReachBack, Intent, Style, Body, EDGHandedness::Right);
    const FDGThrowMicroMotionPose Plant =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Plant, Intent, Style, Body, EDGHandedness::Right);
    const FDGThrowMicroMotionPose Acceleration =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Acceleration, Intent, Style, Body, EDGHandedness::Right);
    const FDGThrowMicroMotionPose Release =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Release, Intent, Style, Body, EDGHandedness::Right);
    const FDGThrowMicroMotionPose Recovery =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Recovery, Intent, Style, Body, EDGHandedness::Right);
    TestTrue(TEXT("Reachback layers pelvis and torso counter-rotation"),
        Reach.PelvisCounterYawDegrees * Reach.TorsoCoilYawDegrees < 0.0f);
    TestTrue(TEXT("Plant increases brace compression and settles the lead foot"),
        Plant.BraceCompressionAlpha > Reach.BraceCompressionAlpha
            && Plant.PlantFootSettleCm > 0.0f);
    TestTrue(TEXT("Acceleration leads with the shoulder before the elbow"),
        FMath::Abs(Acceleration.ShoulderLeadDegrees)
            > FMath::Abs(Acceleration.ElbowLeadDegrees));
    TestTrue(TEXT("Release transfers the chain from elbow lead into wrist snap"),
        FMath::Abs(Release.ElbowLeadDegrees)
            > FMath::Abs(Release.ShoulderLeadDegrees)
            && Acceleration.WristLagDegrees * Release.WristLagDegrees < 0.0f);
    TestTrue(TEXT("Finger grip opens on the exact Release phase"),
        Acceleration.GripAlpha >= 0.99f && Release.GripAlpha <= 0.10f);
    TestTrue(TEXT("Recovery requests a damped return to relaxed targets"),
        Recovery.RecoveryRelaxAlpha == 1.0f
            && FMath::Abs(Recovery.TorsoCoilYawDegrees)
                < FMath::Abs(Release.TorsoCoilYawDegrees));

    const FDGThrowMicroMotionPose LeftRelease =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Release, Intent, Style, Body, EDGHandedness::Left);
    TestTrue(TEXT("Handedness mirrors yaw and hyzer wrist presentation only"),
        FMath::IsNearlyEqual(LeftRelease.PelvisCounterYawDegrees,
            -Release.PelvisCounterYawDegrees)
            && FMath::IsNearlyEqual(LeftRelease.WristRollDegrees,
                -Release.WristRollDegrees)
            && FMath::IsNearlyEqual(LeftRelease.WristPitchDegrees,
                Release.WristPitchDegrees));

    FDGBodyProfile ShortBody = Body;
    ShortBody.HeightCm = 150.0f;
    ShortBody.WingspanScale = 0.92f;
    FDGBodyProfile TallBody = Body;
    TallBody.HeightCm = 210.0f;
    TallBody.WingspanScale = 1.08f;
    const FDGThrowMicroMotionPose ShortAcceleration =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Acceleration, Intent, Style, ShortBody, EDGHandedness::Right);
    const FDGThrowMicroMotionPose TallAcceleration =
        DiscGolfCharacterRigPresentation::BuildMicroMotionTarget(
            EDGThrowPhase::Acceleration, Intent, Style, TallBody, EDGHandedness::Right);
    TestTrue(TEXT("Body-profile leverage changes the chain within the same bounds"),
        FMath::Abs(TallAcceleration.ShoulderLeadDegrees)
            > FMath::Abs(ShortAcceleration.ShoulderLeadDegrees)
            && DiscGolfCharacterRigPresentation::IsMicroMotionPoseFiniteAndBounded(
                ShortAcceleration)
            && DiscGolfCharacterRigPresentation::IsMicroMotionPoseFiniteAndBounded(
                TallAcceleration));

    const float ReleaseStep = DiscGolfCharacterRigPresentation::PhaseLocalBlendAlpha(
        EDGThrowPhase::Release, 1.0f / 60.0f);
    const float RecoveryStep = DiscGolfCharacterRigPresentation::PhaseLocalBlendAlpha(
        EDGThrowPhase::Recovery, 1.0f / 60.0f);
    TestTrue(TEXT("Release responds faster than recovery damping"),
        ReleaseStep > RecoveryStep && RecoveryStep > 0.0f);
    TestEqual(TEXT("Non-finite phase delta fails closed"),
        DiscGolfCharacterRigPresentation::PhaseLocalBlendAlpha(
            EDGThrowPhase::Acceleration,
            std::numeric_limits<float>::quiet_NaN()),
        0.0f);

    TestFalse(TEXT("A missing production curve preserves the legacy IK path"),
        DiscGolfCharacterRigPresentation::HasProductionFullBodyMotionSignature(0.0f));
    TestTrue(TEXT("The v3 authored gaze curve identifies dense full-body motion"),
        DiscGolfCharacterRigPresentation::HasProductionFullBodyMotionSignature(0.35f));
    TestFalse(TEXT("A malformed production curve fails closed"),
        DiscGolfCharacterRigPresentation::HasProductionFullBodyMotionSignature(
            std::numeric_limits<float>::quiet_NaN()));

    FDGThrowMicroMotionPose IKStress = Release;
    IKStress.DiscPlaneStabilityAlpha = 1.0f;
    IKStress.OffArmBalanceAlpha = 1.0f;
    const FDGUpperLimbIKPolicy ProductionRelease =
        DiscGolfCharacterRigPresentation::BuildUpperLimbIKPolicy(
            true, EDGThrowPhase::Release, IKStress, 1.0f);
    TestTrue(TEXT("Production throw-hand IK cannot replace the authored trajectory"),
        ProductionRelease.bPreserveAuthoredThrowTrajectory
            && !ProductionRelease.bUseSyntheticFollowTarget
            && ProductionRelease.ThrowHandAlpha <= 0.12f);
    TestTrue(TEXT("Production support-hand correction stays doubly bounded"),
        ProductionRelease.SupportHandAlpha <= 0.08f
            && ProductionRelease.SupportTargetBlendAlpha <= 0.10f);

    const FDGUpperLimbIKPolicy ProductionFollow =
        DiscGolfCharacterRigPresentation::BuildUpperLimbIKPolicy(
            true, EDGThrowPhase::FollowThrough, IKStress, 1.0f);
    TestTrue(TEXT("Production follow-through remains on the authored hand arc"),
        !ProductionFollow.bUseSyntheticFollowTarget
            && ProductionFollow.ThrowHandAlpha <= 0.08f
            && ProductionFollow.SupportHandAlpha <= 0.06f);
    IKStress.RecoveryRelaxAlpha = 1.0f;
    const FDGUpperLimbIKPolicy ProductionRecovery =
        DiscGolfCharacterRigPresentation::BuildUpperLimbIKPolicy(
            true, EDGThrowPhase::Recovery, IKStress, 1.0f);
    TestTrue(TEXT("Production recovery damps instead of snapping to a synthetic pose"),
        ProductionRecovery.ThrowHandAlpha <= 0.02f
            && ProductionRecovery.SupportHandAlpha <= 0.01f
            && ProductionRecovery.SupportTargetBlendAlpha == 0.0f);

    const FDGUpperLimbIKPolicy LegacyFollow =
        DiscGolfCharacterRigPresentation::BuildUpperLimbIKPolicy(
            false, EDGThrowPhase::FollowThrough, IKStress, 1.0f);
    TestTrue(TEXT("Legacy montage follow-target behavior is unchanged"),
        LegacyFollow.bUseSyntheticFollowTarget
            && !LegacyFollow.bPreserveAuthoredThrowTrajectory
            && FMath::IsNearlyEqual(LegacyFollow.ThrowHandAlpha, 0.74f)
            && FMath::IsNearlyEqual(LegacyFollow.SupportHandAlpha, 0.90f));

    FDGThrowMicroMotionPose At60Hz;
    FDGThrowMicroMotionPose At120Hz;
    for (int32 Index = 0; Index < 60; ++Index)
    {
        At60Hz = DiscGolfCharacterRigPresentation::BlendMicroMotionPose(
            At60Hz, Acceleration,
            DiscGolfCharacterRigPresentation::PhaseLocalBlendAlpha(
                EDGThrowPhase::Acceleration, 1.0f / 60.0f));
    }
    for (int32 Index = 0; Index < 120; ++Index)
    {
        At120Hz = DiscGolfCharacterRigPresentation::BlendMicroMotionPose(
            At120Hz, Acceleration,
            DiscGolfCharacterRigPresentation::PhaseLocalBlendAlpha(
                EDGThrowPhase::Acceleration, 1.0f / 120.0f));
    }
    TestTrue(TEXT("Micro-motion continuity is render-rate independent"),
        FMath::IsNearlyEqual(At60Hz.TorsoCoilYawDegrees,
            At120Hz.TorsoCoilYawDegrees, 1.0e-4f)
            && FMath::IsNearlyEqual(At60Hz.GripAlpha,
                At120Hz.GripAlpha, 1.0e-4f));
    return !HasAnyErrors();
}

#endif
