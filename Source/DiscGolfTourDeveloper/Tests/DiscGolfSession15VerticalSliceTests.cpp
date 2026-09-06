#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DiscBagComponent.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfSession15VerticalSliceContract.h"
#include "DiscThrowLabSubsystem.h"

namespace
{
bool SameSample(
    const FDiscTrajectorySample& A,
    const FDiscTrajectorySample& B)
{
    return A.TimeSeconds == B.TimeSeconds
        && A.WorldLocationCm == B.WorldLocationCm
        && A.VelocityMps == B.VelocityMps
        && A.DiscNormalWorld == B.DiscNormalWorld
        && A.WindMps == B.WindMps
        && A.SpinRpm == B.SpinRpm
        && A.AngleOfAttackDeg == B.AngleOfAttackDeg
        && A.GroundState == B.GroundState
        && A.GroundSurface == B.GroundSurface
        && A.CourseSurface == B.CourseSurface
        && A.GroundContactCount == B.GroundContactCount;
}

bool SameSampleArray(
    const TArray<FDiscTrajectorySample>& A,
    const TArray<FDiscTrajectorySample>& B)
{
    if (A.Num() != B.Num()) return false;
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (!SameSample(A[Index], B[Index])) return false;
    }
    return true;
}

bool BuildReplayEvidence(
    FDiscGolfSession15VerticalSliceEvidence& OutEvidence,
    FString& OutError)
{
    constexpr int32 SampleCount = 121;
    constexpr float DurationSeconds = 2.0f;
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(SampleCount - 1);
        FDiscTrajectorySample& Sample = OutEvidence.ActualReplaySource.AddDefaulted_GetRef();
        Sample.TimeSeconds = Alpha * DurationSeconds;
        Sample.WorldLocationCm = FVector(700.0f * Alpha, 0.0f, 100.0f + 40.0f * Alpha);
        Sample.VelocityMps = Index + 1 == SampleCount
            ? FVector::ZeroVector : FVector(3.5f, 0.0f, 0.2f);
        Sample.DiscNormalWorld = FVector::UpVector;
        Sample.WindMps = FVector(0.5f, 0.25f, 0.0f);
        Sample.SpinRpm = 950.0f * (1.0f - Alpha);
        Sample.GroundState = Index + 1 == SampleCount
            ? EDiscGroundState::Settled : EDiscGroundState::Airborne;
        Sample.GroundContactCount = Index + 1 == SampleCount ? 1 : 0;
    }

    FDiscGroundTransition& Transition =
        OutEvidence.ActualReplayTransitions.AddDefaulted_GetRef();
    Transition.TimeSeconds = DurationSeconds;
    Transition.WorldLocationCm = OutEvidence.ActualReplaySource.Last().WorldLocationCm;
    Transition.FromState = EDiscGroundState::Airborne;
    Transition.ToState = EDiscGroundState::Settled;
    Transition.GroundContactCount = 1;
    Transition.ImpactSpeedMps = 3.5f;

    FResolvedDiscDefinition Disc;
    Disc.DiscInstanceId = OutEvidence.TeeDriver.InstanceId;
    Disc.MoldId = TEXT("Apex");
    Disc.DisplayName = FText::FromString(TEXT("Apex"));
    Disc.Plastic = EDiscPlastic::Tour;

    FThrowRelease Release;
    Release.ReleaseSpeedMps = 20.0f;
    Release.SpinRpm = 1000.0f;
    Release.Direction = FVector::ForwardVector;
    Release.ShotContext = EDiscShotContext::Drive;

    FDiscFlightTelemetry Telemetry;
    Telemetry.State = EDiscFlightState::Settled;
    Telemetry.GroundState = EDiscGroundState::Settled;
    Telemetry.GroundContactCount = 1;
    Telemetry.FlightTimeSeconds = DurationSeconds;
    Telemetry.CarryMeters = 7.0f;
    Telemetry.Release = Release;

    FDiscTrajectorySummary Summary;
    Summary.CaptureId = TEXT("Session15ActualReplayProof");
    Summary.CapturedUtc = TEXT("2026-08-24T20:00:00.000Z");
    Summary.SampleCount = SampleCount;
    Summary.GroundTransitionCount = 1;
    Summary.DurationSeconds = DurationSeconds;
    Summary.AirTimeSeconds = DurationSeconds;
    Summary.AirCarryMeters = 7.0f;
    Summary.FinalCarryMeters = 7.0f;
    Summary.ApexMeters = 0.4f;
    Summary.StartWorldLocationCm = OutEvidence.ActualReplaySource[0].WorldLocationCm;
    Summary.FinalWorldLocationCm = OutEvidence.ActualReplaySource.Last().WorldLocationCm;
    Summary.FinalGroundState = EDiscGroundState::Settled;
    Summary.GroundContactCount = 1;
    Summary.ResultingLieLocationCm = Summary.FinalWorldLocationCm;

    return UDiscThrowLabSubsystem::BuildRecord(
        Disc,
        Release,
        OutEvidence.ActualReplaySource,
        OutEvidence.ActualReplayTransitions,
        Telemetry,
        Summary,
        OutEvidence.ThrowLabRecord,
        OutError);
}

bool BuildValidEvidence(
    FDiscGolfSession15VerticalSliceEvidence& OutEvidence,
    FString& OutError)
{
    OutEvidence = FDiscGolfSession15VerticalSliceEvidence();
    OutEvidence.ContractVersion =
        DiscGolfSession15VerticalSliceContract::CurrentContractVersion;
    OutEvidence.ActivePresentingBrandId =
        DiscGolfSession15VerticalSliceContract::GenericBrandId();
    OutEvidence.ActiveEquipmentBrandId =
        DiscGolfSession15VerticalSliceContract::GenericBrandId();
    OutEvidence.FrozenDormantDonorBrandId =
        DiscGolfSession15VerticalSliceContract::DormantPremiumDonorBrandId();
    OutEvidence.bPremiumDonorDormant = true;
    OutEvidence.Hole = DiscGolfCourseDefinition::PineRidgeHole1Fallback();

    const FDGDiscBagLoadout Defaults = UDiscBagComponent::BuildDefaultLoadout();
    OutEvidence.TeeDriver = Defaults.Discs[0];
    OutEvidence.Putter = Defaults.Discs.Last();
    OutEvidence.Putter.PlasticId = TEXT("Base");

    OutEvidence.Authority.ThrowAttemptCount = 3;
    OutEvidence.Authority.CommittedReleaseCount = 3;
    OutEvidence.Authority.AcceptedLaunchCount = 3;
    OutEvidence.Authority.GameplayDiscSpawnCount = 3;
    OutEvidence.Authority.StrokeDelta = 3;
    OutEvidence.Authority.bUsedExistingGripReleaseAuthority = true;
    OutEvidence.Authority.bUsedExistingFixedStepFlightAuthority = true;
    OutEvidence.Authority.bUsedExistingRoundAndLieAuthority = true;
    OutEvidence.Authority.bLifecycleQuiescentAfterCompletion = true;

    const FVector HorizontalDirection = FVector(
        OutEvidence.Hole.BasketLocationCm.X - OutEvidence.Hole.TeeLocationCm.X,
        OutEvidence.Hole.BasketLocationCm.Y - OutEvidence.Hole.TeeLocationCm.Y,
        0.0f).GetSafeNormal();
    const FVector Circle2Lie =
        OutEvidence.Hole.BasketLocationCm - HorizontalDirection * 1400.0f;
    const FVector Circle1Lie =
        OutEvidence.Hole.BasketLocationCm - HorizontalDirection * 700.0f;

    auto AddShot = [&OutEvidence](
        EDiscShotContext StartContext,
        EDiscShotContext ResultingContext,
        const FVector& Start,
        const FVector& Finish,
        int32 Stroke,
        bool bHoledOut)
    {
        FDiscGolfSession15ShotEvidence& Shot =
            OutEvidence.Shots.AddDefaulted_GetRef();
        Shot.StartContext = StartContext;
        Shot.ResultingContext = ResultingContext;
        Shot.StartLieLocationCm = Start;
        Shot.FinalWorldLocationCm = Finish;
        Shot.ResultingLieLocationCm = Finish;
        Shot.StartDistanceToBasketMeters = FVector::Dist2D(
            Start, OutEvidence.Hole.BasketLocationCm) / 100.0f;
        Shot.RemainingDistanceToBasketMeters = FVector::Dist2D(
            Finish, OutEvidence.Hole.BasketLocationCm) / 100.0f;
        Shot.StrokeAfterShot = Stroke;
        Shot.ActualTrajectorySampleCount = 240 + Stroke;
        Shot.bHoledOut = bHoledOut;
        Shot.FinalTelemetry.State = EDiscFlightState::Settled;
        Shot.FinalTelemetry.GroundState = EDiscGroundState::Settled;
        if (bHoledOut)
        {
            Shot.FinalTelemetry.LastBasketContact = EBasketContactResult::Caught;
            Shot.FinalTelemetry.BasketContactCount = 1;
        }
    };
    AddShot(
        EDiscShotContext::Drive,
        EDiscShotContext::Circle2Putt,
        OutEvidence.Hole.TeeLocationCm,
        Circle2Lie,
        1,
        false);
    AddShot(
        EDiscShotContext::Circle2Putt,
        EDiscShotContext::Circle1Putt,
        Circle2Lie,
        Circle1Lie,
        2,
        false);
    AddShot(
        EDiscShotContext::Circle1Putt,
        EDiscShotContext::Circle1Putt,
        Circle1Lie,
        OutEvidence.Hole.BasketLocationCm + FVector(0.0f, 0.0f, 101.0f),
        3,
        true);

    const TArray<FDiscGolfHoleBlockoutDefinition> Holes = {
        DiscGolfCourseDefinition::PineRidgeHole1Fallback(),
        DiscGolfCourseDefinition::PineRidgeHole2Fallback(),
        DiscGolfCourseDefinition::PineRidgeHole3Fallback()
    };
    if (!DiscGolfRound::Initialize(
            OutEvidence.Round,
            TEXT("PineRidgeChampionship"),
            TEXT("Championship"),
            FText::FromString(TEXT("Pine Ridge Championship")),
            Holes,
            OutError)
        || !DiscGolfRound::RecordCurrentHole(OutEvidence.Round, 3, 0, OutError))
    {
        return false;
    }

    if (!BuildReplayEvidence(OutEvidence, OutError))
    {
        return false;
    }

    OutEvidence.PlayerProfileSchemaVersion = DiscGolfSaveSchema::CurrentVersion;
    OutEvidence.PlayerSettings.bHighContrastUI = true;
    OutEvidence.PlayerSettings.bSubtitles = true;
    OutEvidence.PlayerSettings.bReducedMotion = true;
    OutEvidence.PlayerSettings.Normalize();
    OutEvidence.ExternalUserDir =
        TEXT("C:/DGTour_TestRuns/Session15/12345678-1234-4abc-8def-1234567890ab");
    OutEvidence.bProfileRoundTripExact = true;
    OutEvidence.bProductionProfileUntouched = true;

    OutEvidence.Readiness.bRenderedIntegratedPerformanceMeasured = true;
    OutEvidence.Readiness.bPerformanceBudgetPassed = true;
    OutEvidence.Readiness.bTechnicalAcceptancePassed = true;
    OutEvidence.Readiness.bFinalVisualArtApproved = false;
    OutEvidence.Readiness.bMeasuredFlightCalibrationApproved = false;
    OutEvidence.Readiness.bAuthoredProductionAudioPresent = false;
    OutEvidence.Readiness.bPolishedSliceApproved = false;
    OutEvidence.Readiness.bPublicReleaseReady = false;
    OutError.Reset();
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15OriginalGenericBrandTest,
    "DiscGolfTour.Session15.Contract.OriginalGenericBrandPremiumDormant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15OriginalGenericBrandTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    TestTrue(TEXT("The original generic substitution validates"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    TestEqual(TEXT("The only active brand is generic"),
        Evidence.ActivePresentingBrandId, FName(TEXT("dg_generic")));
    TestEqual(TEXT("The frozen donor identity remains explicit"),
        Evidence.FrozenDormantDonorBrandId,
        DiscGolfSession15VerticalSliceContract::DormantPremiumDonorBrandId());
    Evidence.bPremiumDonorDormant = false;
    TestFalse(TEXT("Premium activation is rejected"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15PineRidgeIdentityTest,
    "DiscGolfTour.Session15.Contract.PineRidgeHole1IdentityAndValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15PineRidgeIdentityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    TestTrue(TEXT("Pine Ridge Hole 1 passes its existing validator"),
        DiscGolfCourseDefinition::Validate(Evidence.Hole, Error));
    TestEqual(TEXT("Hole identity is original Pine Ridge Opening"),
        Evidence.Hole.HoleName.ToString(), FString(TEXT("Pine Ridge Opening")));
    TestEqual(TEXT("Hole is par three"), Evidence.Hole.Par, 3);
    TestTrue(TEXT("Measured distance remains 361.855 feet"), FMath::IsNearlyEqual(
        DiscGolfCourseDefinition::MeasuredDistanceFeet(Evidence.Hole),
        DiscGolfSession15VerticalSliceContract::PineRidgeHole1DistanceFeet,
        0.001f));
    Evidence.Hole.Par = 4;
    TestFalse(TEXT("A mutated par cannot satisfy the Session 15 contract"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15StableEquipmentTest,
    "DiscGolfTour.Session15.Contract.StableDriverAndPutterInstances",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15StableEquipmentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    TestEqual(TEXT("The tee disc is Apex"), Evidence.TeeDriver.DiscDefinitionId, FName(TEXT("Apex")));
    TestEqual(TEXT("The tee disc is Tour plastic"), Evidence.TeeDriver.PlasticId, FName(TEXT("Tour")));
    TestEqual(TEXT("The putter is Touch"), Evidence.Putter.DiscDefinitionId, FName(TEXT("Touch")));
    TestEqual(TEXT("The putter is Base plastic"), Evidence.Putter.PlasticId, FName(TEXT("Base")));
    TestTrue(TEXT("Both player instances have stable distinct identities"),
        Evidence.TeeDriver.InstanceId.IsValid()
        && Evidence.Putter.InstanceId.IsValid()
        && Evidence.TeeDriver.InstanceId != Evidence.Putter.InstanceId);
    TestTrue(TEXT("Stable equipment validates with the complete contract"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    Evidence.Putter.InstanceId = FGuid::NewGuid();
    TestFalse(TEXT("A replacement putter identity is rejected"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15AuthorityAtomicTest,
    "DiscGolfTour.Session15.Contract.AuthorityLifecycleAtomicFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15AuthorityAtomicTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Candidate;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Candidate, Error));
    FDiscGolfSession15VerticalSliceEvidence Accepted;
    Accepted.ContractVersion = 77;
    Accepted.ActivePresentingBrandId = TEXT("sentinel");
    Candidate.Authority.AcceptedLaunchCount = 2;
    TestFalse(TEXT("Mismatched authority counts fail closed"),
        DiscGolfSession15VerticalSliceContract::ValidateAndCommitEvidence(
            Candidate, Accepted, Error));
    TestEqual(TEXT("Atomic failure preserves the prior contract version"),
        Accepted.ContractVersion, 77);
    TestEqual(TEXT("Atomic failure preserves the prior identity"),
        Accepted.ActivePresentingBrandId, FName(TEXT("sentinel")));
    TestTrue(TEXT("Failure reports a reason"), !Error.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15NaturalHoleTest,
    "DiscGolfTour.Session15.Contract.NaturalHoleCompletionAndScore",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15NaturalHoleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    TestTrue(TEXT("Tee to Circle 2 to Circle 1 to caught validates"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    TestEqual(TEXT("The hole uses three actual shots"), Evidence.Shots.Num(), 3);
    TestEqual(TEXT("The final basket telemetry is caught"),
        Evidence.Shots.Last().FinalTelemetry.LastBasketContact,
        EBasketContactResult::Caught);
    TestEqual(TEXT("One Hole 1 score is complete"),
        DiscGolfRound::CompletedHoleCount(Evidence.Round), 1);
    TestEqual(TEXT("The natural par score is internally consistent"),
        DiscGolfRound::ScoreToPar(Evidence.Round), 0);
    Evidence.Shots[1].StartLieLocationCm += FVector(100.0f, 0.0f, 0.0f);
    TestFalse(TEXT("A discontinuous teleported lie is rejected"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15ReplayProvenanceTest,
    "DiscGolfTour.Session15.Contract.ReplayThrowLabActualSampleProvenance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15ReplayProvenanceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    const TArray<FDiscTrajectorySample> SourceBefore = Evidence.ActualReplaySource;
    TestTrue(TEXT("Actual-source replay and Throw Lab record validate"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    TestTrue(TEXT("Validation leaves the actual source immutable"),
        SameSampleArray(SourceBefore, Evidence.ActualReplaySource));
    TestTrue(TEXT("Throw Lab remains within its retained sample bound"),
        Evidence.ThrowLabRecord.ReplaySamples.Num() <= UDiscThrowLabSubsystem::MaxReplaySamples);
    Evidence.ThrowLabRecord.ReplaySamples[1].WorldLocationCm.X += 0.5f;
    TestFalse(TEXT("A synthetic or mutated retained sample is rejected"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15PersistenceTest,
    "DiscGolfTour.Session15.Contract.SettingsSchema10IsolatedPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15PersistenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    TestEqual(TEXT("Profile evidence uses schema 10"),
        Evidence.PlayerProfileSchemaVersion, DiscGolfSaveSchema::CurrentVersion);
    TestTrue(TEXT("The external GUID user domain is accepted"),
        DiscGolfSession15VerticalSliceContract::IsExternalGuidUserDir(Evidence.ExternalUserDir));
    TestTrue(TEXT("Normalized isolated persistence validates"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    Evidence.ExternalUserDir = TEXT("C:/DGTour/Saved");
    TestFalse(TEXT("The project save domain is rejected"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession15TruthfulnessTest,
    "DiscGolfTour.Session15.Contract.TruthfulTechnicalAcceptanceBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession15TruthfulnessTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfSession15VerticalSliceEvidence Evidence;
    FString Error;
    TestTrue(TEXT("The complete baseline evidence builds"), BuildValidEvidence(Evidence, Error));
    TestTrue(TEXT("Measured performance permits a truthful technical pass"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    TestTrue(TEXT("Technical acceptance is explicit"),
        Evidence.Readiness.bTechnicalAcceptancePassed);
    TestFalse(TEXT("Final visual art is not falsely approved"),
        Evidence.Readiness.bFinalVisualArtApproved);
    TestFalse(TEXT("Measured calibration is not falsely approved"),
        Evidence.Readiness.bMeasuredFlightCalibrationApproved);
    TestFalse(TEXT("Silent semantic routing is not called authored audio"),
        Evidence.Readiness.bAuthoredProductionAudioPresent);
    TestFalse(TEXT("The bounded technical pass is not called polished"),
        Evidence.Readiness.bPolishedSliceApproved);
    TestFalse(TEXT("The bounded technical pass is not called release-ready"),
        Evidence.Readiness.bPublicReleaseReady);
    Evidence.Readiness.bPolishedSliceApproved = true;
    TestFalse(TEXT("A false polished claim is rejected"),
        DiscGolfSession15VerticalSliceContract::ValidateEvidence(Evidence, Error));
    return true;
}

#endif
