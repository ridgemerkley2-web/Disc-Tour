#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "../DiscGolfMath.h"
#include "../DiscGolfTourGameMode.h"
#include "../DiscTrajectorySubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <limits>

namespace
{
void MakeReferenceCapture(
    FResolvedDiscDefinition& OutDisc,
    FThrowRelease& OutRelease,
    TArray<FDiscTrajectorySample>& OutSamples,
    TArray<FDiscGroundTransition>& OutTransitions,
    FDiscFlightTelemetry& OutTelemetry,
    FDiscGolfLieState& OutLie)
{
    OutDisc.MoldId = TEXT("Apex");
    OutDisc.DisplayName = FText::FromString(TEXT("Apex"));
    OutDisc.Plastic = EDiscPlastic::Tour;
    OutRelease.Direction = FVector::ForwardVector;
    OutRelease.ReleaseSpeedMps = 26.5f;
    OutRelease.SpinRpm = 915.0f;
    OutRelease.WindPhaseOriginSeconds = 137.25f;

    const TArray<FVector> Locations = {
        FVector(100.0f, 200.0f, 150.0f),
        FVector(5100.0f, 1200.0f, 1150.0f),
        FVector(8100.0f, 1700.0f, 10.0f),
        FVector(9100.0f, 1800.0f, 10.0f)};
    for (int32 Index = 0; Index < Locations.Num(); ++Index)
    {
        FDiscTrajectorySample Sample;
        Sample.TimeSeconds = static_cast<float>(Index);
        Sample.WorldLocationCm = Locations[Index];
        Sample.VelocityMps = Index < 2 ? FVector(20.0f, 1.0f, 2.0f) : FVector::ZeroVector;
        Sample.WindMps = FVector(1.0f, 0.0f, 0.0f);
        Sample.SpinRpm = Index < 3 ? 800.0f : 0.0f;
        if (Index == 2)
        {
            Sample.GroundState = EDiscGroundState::Skipping;
            Sample.GroundContactCount = 1;
        }
        else if (Index == 3)
        {
            Sample.GroundState = EDiscGroundState::Settled;
            Sample.GroundContactCount = 2;
        }
        OutSamples.Add(Sample);
    }

    FDiscGroundTransition Skip;
    Skip.TimeSeconds = 2.0f;
    Skip.WorldLocationCm = Locations[2];
    Skip.FromState = EDiscGroundState::Airborne;
    Skip.ToState = EDiscGroundState::Skipping;
    Skip.GroundContactCount = 1;
    Skip.ImpactSpeedMps = 18.0f;
    OutTransitions.Add(Skip);

    FDiscGroundTransition Settle;
    Settle.TimeSeconds = 3.0f;
    Settle.WorldLocationCm = Locations[3];
    Settle.FromState = EDiscGroundState::Sliding;
    Settle.ToState = EDiscGroundState::Settled;
    Settle.GroundContactCount = 2;
    OutTransitions.Add(Settle);

    OutTelemetry.FlightTimeSeconds = 3.0f;
    OutTelemetry.WindPhaseOriginSeconds = OutRelease.WindPhaseOriginSeconds;
    OutTelemetry.GroundState = EDiscGroundState::Settled;
    OutTelemetry.GroundSurface = EGroundSurfaceType::Fairway;
    OutTelemetry.GroundContactCount = 2;
    OutTelemetry.GroundDistanceMeters = 10.0f;
    OutTelemetry.Release = OutRelease;
    OutLie.SurfaceAtRest = ECourseSurfaceType::Fairway;
    OutLie.PlayingSurface = ECourseSurfaceType::Fairway;
    OutLie.LieType = ELieType::Fairway;
    OutLie.LieLocationCm = Locations.Last();
}

TArray<FDiscTrajectorySummary> MakePassingCanonicalRegressionSuite()
{
    struct FScenario
    {
        const TCHAR* PresetId;
        int32 RenderFps;
        bool bMustHoleOut;
    };
    constexpr FScenario Scenarios[] = {
        {TEXT("ApexCalm30"), 30, false},
        {TEXT("ApexCalm60"), 60, false},
        {TEXT("ApexCalm120"), 120, false},
        {TEXT("ApexForehandCalm60"), 60, false},
        {TEXT("TouchCircle1Center"), 60, true},
        {TEXT("TouchCircle2Center"), 60, true}};

    TArray<FDiscTrajectorySummary> Summaries;
    int32 ScenarioIndex = 0;
    for (const FScenario& Scenario : Scenarios)
    {
        FDiscTrajectorySummary Summary;
        Summary.PresetId = FName(Scenario.PresetId);
        Summary.RenderFps = Scenario.RenderFps;
        Summary.WindPhaseOriginSeconds = 37.0f * static_cast<float>(++ScenarioIndex);
        Summary.bWasRegression = true;
        Summary.bRegressionPassed = true;
        Summary.SampleCount = 1201;
        Summary.DurationSeconds = 5.0f;
        Summary.AirCarryMeters = 80.0f;
        Summary.FinalCarryMeters = 90.0f;
        Summary.ApexMeters = 10.0f;
        Summary.AirTimeSeconds = 5.0f;
        Summary.LateralMeters = 2.0f;
        Summary.GroundDistanceMeters = 8.0f;
        Summary.GroundContactCount = 2;
        Summary.FinalGroundState = EDiscGroundState::Settled;
        Summary.bHoledOut = Scenario.bMustHoleOut;
        Summary.LastBasketContact = Scenario.bMustHoleOut
            ? EBasketContactResult::Caught
            : EBasketContactResult::None;
        Summaries.Add(Summary);
    }
    return Summaries;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectorySummaryTest,
    "DiscGolfTour.Trajectory.Summary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectorySummaryTest::RunTest(const FString& Parameters)
{
    FResolvedDiscDefinition Disc;
    FThrowRelease Release;
    TArray<FDiscTrajectorySample> Samples;
    TArray<FDiscGroundTransition> Transitions;
    FDiscFlightTelemetry Telemetry;
    FDiscGolfLieState Lie;
    MakeReferenceCapture(Disc, Release, Samples, Transitions, Telemetry, Lie);

    const FDiscTrajectorySummary Summary = UDiscTrajectorySubsystem::BuildSummary(
        Samples, Transitions, Telemetry, Release, Lie, false, TEXT("TestPreset"), 60);
    TestEqual(TEXT("Every fixed-rate sample is counted"), Summary.SampleCount, 4);
    TestEqual(TEXT("Every discrete ground transition is counted"), Summary.GroundTransitionCount, 2);
    TestTrue(TEXT("Air carry uses the first ground sample"), FMath::IsNearlyEqual(Summary.AirCarryMeters, 80.0f));
    TestTrue(TEXT("Final carry uses the release direction"), FMath::IsNearlyEqual(Summary.FinalCarryMeters, 90.0f));
    TestTrue(TEXT("Lateral displacement uses the release-right axis"), FMath::IsNearlyEqual(Summary.LateralMeters, 16.0f));
    TestTrue(TEXT("Apex is relative to release height"), FMath::IsNearlyEqual(Summary.ApexMeters, 10.0f));
    TestTrue(TEXT("Air time ends at first ground contact"), FMath::IsNearlyEqual(Summary.AirTimeSeconds, 2.0f));
    TestEqual(TEXT("Summary preserves release handedness"), Summary.Handedness, Release.Handedness);
    TestTrue(TEXT("Summary preserves deterministic wind phase provenance"),
        FMath::IsNearlyEqual(
            Summary.WindPhaseOriginSeconds, Release.WindPhaseOriginSeconds));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectorySerializationTest,
    "DiscGolfTour.Trajectory.Serialization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectorySerializationTest::RunTest(const FString& Parameters)
{
    FResolvedDiscDefinition Disc;
    FThrowRelease Release;
    TArray<FDiscTrajectorySample> Samples;
    TArray<FDiscGroundTransition> Transitions;
    FDiscFlightTelemetry Telemetry;
    FDiscGolfLieState Lie;
    MakeReferenceCapture(Disc, Release, Samples, Transitions, Telemetry, Lie);
    Release.Handedness = EDGHandedness::Left;
    FDiscTrajectorySummary Summary = UDiscTrajectorySubsystem::BuildSummary(
        Samples, Transitions, Telemetry, Release, Lie, false, TEXT("TestPreset"), 60);
    Summary.CaptureId = TEXT("test_capture");
    Summary.CapturedUtc = TEXT("2026-08-11T00:00:00Z");

    const FString Json = UDiscTrajectorySubsystem::BuildJson(Disc, Release, Samples, Transitions, Telemetry, Summary);
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    TestTrue(TEXT("Exported JSON parses"), FJsonSerializer::Deserialize(Reader, Root));
    TestTrue(TEXT("Exported JSON root exists"), Root.IsValid());
    if (Root.IsValid())
    {
        TestEqual(TEXT("Trajectory schema is versioned"), Root->GetIntegerField(TEXT("schema_version")), 5);
        TestEqual(TEXT("JSON preserves all samples"), Root->GetArrayField(TEXT("samples")).Num(), 4);
        TestEqual(TEXT("JSON preserves all transitions"), Root->GetArrayField(TEXT("ground_transitions")).Num(), 2);
        TestEqual(TEXT("JSON records shot context"),
            Root->GetObjectField(TEXT("release"))->GetStringField(TEXT("shot_context")), FString(TEXT("Drive")));
        TestEqual(TEXT("JSON records handedness provenance"),
            Root->GetObjectField(TEXT("release"))->GetStringField(TEXT("handedness")), FString(TEXT("Left")));
        TestEqual(TEXT("JSON summary records matching handedness provenance"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("handedness")), FString(TEXT("Left")));
        TestTrue(TEXT("JSON release records deterministic wind phase provenance"),
            FMath::IsNearlyEqual(
                Root->GetObjectField(TEXT("release"))->GetNumberField(TEXT("wind_phase_origin_s")),
                static_cast<double>(Release.WindPhaseOriginSeconds)));
        TestTrue(TEXT("JSON summary records matching wind phase provenance"),
            FMath::IsNearlyEqual(
                Root->GetObjectField(TEXT("summary"))->GetNumberField(TEXT("wind_phase_origin_s")),
                static_cast<double>(Release.WindPhaseOriginSeconds)));
        TestEqual(TEXT("JSON records basket outcome"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("last_basket_contact")), FString(TEXT("None")));
        TestEqual(TEXT("JSON records resulting lie"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("resulting_lie_type")), FString(TEXT("Fairway")));
        TestEqual(TEXT("JSON records course surface"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("surface_at_rest")), FString(TEXT("Fairway")));
        const TSharedPtr<FJsonObject> FinalTelemetry = Root->GetObjectField(TEXT("final_telemetry"));
        TestTrue(TEXT("JSON telemetry records matching wind phase provenance"),
            FMath::IsNearlyEqual(
                FinalTelemetry->GetNumberField(TEXT("wind_phase_origin_s")),
                static_cast<double>(Release.WindPhaseOriginSeconds)));
        TestEqual(TEXT("JSON records fixture contact count"),
            FinalTelemetry->GetIntegerField(TEXT("fixture_contact_count")), 0);
        TestEqual(TEXT("JSON records the last fixture type"),
            FinalTelemetry->GetStringField(TEXT("last_fixture_type")), FString(TEXT("Unknown")));
        TestTrue(TEXT("JSON records fixture entry velocity"),
            FinalTelemetry->HasTypedField<EJson::Object>(TEXT("last_fixture_entry_velocity_mps")));
        TestTrue(TEXT("JSON records fixture exit spin"),
            FinalTelemetry->HasTypedField<EJson::Number>(TEXT("last_fixture_exit_spin_rpm")));
    }

    const FString Csv = UDiscTrajectorySubsystem::BuildCsv(Disc, Release, Samples, Transitions, Summary);
    TestTrue(TEXT("CSV declares its versioned schema"), Csv.Contains(TEXT("# schema_version=5")));
    TestTrue(TEXT("CSV records handedness provenance"), Csv.Contains(TEXT("# handedness=Left")));
    TestTrue(TEXT("CSV records deterministic wind phase provenance"),
        Csv.Contains(TEXT("# wind_phase_origin_s=137.250000")));
    TestTrue(TEXT("CSV contains sample rows"), Csv.Contains(TEXT("sample,0,0.000000")));
    TestTrue(TEXT("CSV contains explicit transition rows"), Csv.Contains(TEXT("transition,0,2.000000")));
    TestTrue(TEXT("CSV includes wind columns"), Csv.Contains(TEXT("wind_x_mps,wind_y_mps,wind_z_mps")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectoryRegressionPresetCommandContractTest,
    "DiscGolfTour.Trajectory.RegressionPresetCommandContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectoryRegressionPresetCommandContractTest::RunTest(const FString& Parameters)
{
    const FPhysicsRegressionPreset Valid;
    TestTrue(TEXT("Default regression preset command is accepted"),
        UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(Valid));
    TestEqual(TEXT("Regression presets use the canonical right-handed baseline"),
        Valid.MakeCommand(FVector::ForwardVector).Handedness, EDGHandedness::Right);

    const auto TestRejectedScalar = [this, &Valid](
        const TCHAR* Label, float FPhysicsRegressionPreset::* Field, float Value)
    {
        FPhysicsRegressionPreset Invalid = Valid;
        Invalid.*Field = Value;
        TestFalse(Label, UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(Invalid));
    };
    constexpr float Outside = 0.001f;
    TestRejectedScalar(TEXT("Power below the command contract is rejected"),
        &FPhysicsRegressionPreset::Power01, DiscGolfMath::ThrowCommandMinimumPower01 - Outside);
    TestRejectedScalar(TEXT("Power above the command contract is rejected"),
        &FPhysicsRegressionPreset::Power01, DiscGolfMath::ThrowCommandMaximumPower01 + Outside);
    TestRejectedScalar(TEXT("Finite hyzer below the command contract is rejected"),
        &FPhysicsRegressionPreset::HyzerDeg, DiscGolfMath::ThrowCommandMinimumHyzerDeg - Outside);
    TestRejectedScalar(TEXT("Finite hyzer above the command contract is rejected"),
        &FPhysicsRegressionPreset::HyzerDeg, DiscGolfMath::ThrowCommandMaximumHyzerDeg + Outside);
    TestRejectedScalar(TEXT("Finite nose angle below the command contract is rejected"),
        &FPhysicsRegressionPreset::NoseAngleDeg, DiscGolfMath::ThrowCommandMinimumNoseAngleDeg - Outside);
    TestRejectedScalar(TEXT("Finite nose angle above the command contract is rejected"),
        &FPhysicsRegressionPreset::NoseAngleDeg, DiscGolfMath::ThrowCommandMaximumNoseAngleDeg + Outside);
    TestRejectedScalar(TEXT("Finite launch angle below the command contract is rejected"),
        &FPhysicsRegressionPreset::LaunchAngleDeg, DiscGolfMath::ThrowCommandMinimumLaunchAngleDeg - Outside);
    TestRejectedScalar(TEXT("Finite launch angle above the command contract is rejected"),
        &FPhysicsRegressionPreset::LaunchAngleDeg, DiscGolfMath::ThrowCommandMaximumLaunchAngleDeg + Outside);
    TestRejectedScalar(TEXT("Timing below the command contract is rejected"),
        &FPhysicsRegressionPreset::TimingError, DiscGolfMath::ThrowCommandMinimumTimingError - Outside);
    TestRejectedScalar(TEXT("Timing above the command contract is rejected"),
        &FPhysicsRegressionPreset::TimingError, DiscGolfMath::ThrowCommandMaximumTimingError + Outside);

    FPhysicsRegressionPreset Invalid = Valid;
    Invalid.MoldId = NAME_None;
    TestFalse(TEXT("Missing mold identity is rejected"),
        UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(Invalid));
    Invalid = Valid;
    Invalid.Plastic = static_cast<EDiscPlastic>(255);
    TestFalse(TEXT("Unknown plastic is rejected"),
        UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(Invalid));
    Invalid = Valid;
    Invalid.ThrowStyle = static_cast<EThrowStyle>(255);
    TestFalse(TEXT("Unknown throw style is rejected"),
        UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(Invalid));
    Invalid = Valid;
    Invalid.ShotContext = static_cast<EDiscShotContext>(255);
    TestFalse(TEXT("Unknown shot context is rejected"),
        UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(Invalid));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectoryRegressionLaunchFailureSummaryTest,
    "DiscGolfTour.Trajectory.RegressionLaunchFailureSummary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectoryRegressionLaunchFailureSummaryTest::RunTest(const FString& Parameters)
{
    FPhysicsRegressionPreset Preset;
    Preset.PresetId = TEXT("RejectedLaunch");
    Preset.RenderFps = 120;
    const FDiscTrajectorySummary Summary =
        UDiscTrajectorySubsystem::MakeRegressionLaunchFailureSummary(
            Preset, EDGHandedness::Right, TEXT("LaunchThrow returned false"));

    TestEqual(TEXT("Failure retains preset identity"), Summary.PresetId, Preset.PresetId);
    TestEqual(TEXT("Failure retains render rate"), Summary.RenderFps, Preset.RenderFps);
    TestEqual(TEXT("Failure retains handedness"), Summary.Handedness, EDGHandedness::Right);
    TestTrue(TEXT("Failure is marked as regression evidence"), Summary.bWasRegression);
    TestFalse(TEXT("Rejected launch cannot pass"), Summary.bRegressionPassed);
    TestEqual(TEXT("Rejected launch has one explicit failure"), Summary.RegressionFailures.Num(), 1);
    TestTrue(TEXT("Rejected launch preserves its failure reason"),
        Summary.RegressionFailures[0].Contains(TEXT("LaunchThrow returned false")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectoryRegressionEnvelopeTest,
    "DiscGolfTour.Trajectory.RegressionEnvelope",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectoryRegressionEnvelopeTest::RunTest(const FString& Parameters)
{
    FPhysicsRegressionPreset Preset;
    Preset.PresetId = TEXT("Envelope");
    Preset.Expected.MinAirCarryMeters = 75.0f;
    Preset.Expected.MaxAirCarryMeters = 85.0f;
    Preset.Expected.MinFinalCarryMeters = 85.0f;
    Preset.Expected.MaxFinalCarryMeters = 95.0f;
    Preset.Expected.MinApexMeters = 9.0f;
    Preset.Expected.MaxApexMeters = 11.0f;
    Preset.Expected.MinAirTimeSeconds = 1.9f;
    Preset.Expected.MaxAirTimeSeconds = 2.1f;
    Preset.Expected.MinLateralMeters = 15.0f;
    Preset.Expected.MaxLateralMeters = 17.0f;
    Preset.Expected.MinGroundContacts = 1;
    Preset.Expected.MaxGroundContacts = 3;
    Preset.Expected.ExpectedHoledOut = 1;
    Preset.Expected.bRequireBasketContact = true;
    Preset.Expected.ExpectedBasketContact = EBasketContactResult::Caught;

    FDiscTrajectorySummary Passing;
    Passing.AirCarryMeters = 80.0f;
    Passing.FinalCarryMeters = 90.0f;
    Passing.ApexMeters = 10.0f;
    Passing.AirTimeSeconds = 2.0f;
    Passing.LateralMeters = 16.0f;
    Passing.GroundContactCount = 2;
    Passing.FinalGroundState = EDiscGroundState::Settled;
    Passing.bHoledOut = true;
    Passing.BasketContactCount = 1;
    Passing.LastBasketContact = EBasketContactResult::Caught;
    TestTrue(TEXT("In-envelope capture passes"), UDiscTrajectorySubsystem::EvaluateRegression(Preset, Passing));
    TestTrue(TEXT("Passing result has no failure details"), Passing.RegressionFailures.IsEmpty());

    FDiscTrajectorySummary Failing = Passing;
    Failing.FinalCarryMeters = 120.0f;
    Failing.FinalGroundState = EDiscGroundState::Sliding;
    TestFalse(TEXT("Out-of-envelope capture fails"), UDiscTrajectorySubsystem::EvaluateRegression(Preset, Failing));
    TestEqual(TEXT("Every violated contract is reported"), Failing.RegressionFailures.Num(), 2);

    FPhysicsRegressionPreset NonFiniteEnvelope = Preset;
    NonFiniteEnvelope.Expected.MinAirCarryMeters = std::numeric_limits<float>::quiet_NaN();
    FDiscTrajectorySummary NonFiniteEnvelopeResult = Passing;
    TestFalse(TEXT("Non-finite envelope bounds fail closed"),
        UDiscTrajectorySubsystem::EvaluateRegression(NonFiniteEnvelope, NonFiniteEnvelopeResult));
    TestTrue(TEXT("Non-finite envelope failure is explicit"),
        NonFiniteEnvelopeResult.RegressionFailures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("air_carry_m envelope"));
        }));

    FDiscTrajectorySummary NonFiniteResult = Passing;
    NonFiniteResult.FinalCarryMeters = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite capture metrics fail closed"),
        UDiscTrajectorySubsystem::EvaluateRegression(Preset, NonFiniteResult));
    TestTrue(TEXT("Non-finite capture failure is explicit"),
        NonFiniteResult.RegressionFailures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("final_carry_m result is non-finite"));
        }));

    FPhysicsRegressionPreset UnknownEnumPreset = Preset;
    UnknownEnumPreset.Expected.ExpectedFinalGroundState = static_cast<EDiscGroundState>(255);
    FDiscTrajectorySummary UnknownEnumResult = Passing;
    TestFalse(TEXT("Unknown expected enum values fail closed"),
        UDiscTrajectorySubsystem::EvaluateRegression(UnknownEnumPreset, UnknownEnumResult));
    TestTrue(TEXT("Unknown expected enum failure is explicit"),
        UnknownEnumResult.RegressionFailures.Contains(TEXT("expected final_ground_state enum is unknown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectoryRegressionSuiteContractTest,
    "DiscGolfTour.Trajectory.RegressionSuiteContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectoryRegressionSuiteContractTest::RunTest(const FString& Parameters)
{
    TArray<FString> Failures;
    TArray<FDiscTrajectorySummary> Suite = MakePassingCanonicalRegressionSuite();
    TestTrue(TEXT("Exact canonical six-scenario suite passes"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    TestTrue(TEXT("Canonical suite has no suite-level failures"), Failures.IsEmpty());

    Suite = MakePassingCanonicalRegressionSuite();
    Suite.Pop();
    TestFalse(TEXT("A five-scenario subset cannot pass"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    TestTrue(TEXT("Missing scenario is reported"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("TouchCircle2Center is missing"));
        }));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite.Last().PresetId = Suite[0].PresetId;
    Suite.Last().RenderFps = Suite[0].RenderFps;
    TestFalse(TEXT("Duplicate identity cannot replace a canonical scenario"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    TestTrue(TEXT("Duplicate identity is reported"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("Duplicate regression scenario identity"));
        }));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite.Last().PresetId = TEXT("DevelopmentFallbackScenario");
    TestFalse(TEXT("Unknown or fallback identities cannot produce acceptance"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    TestTrue(TEXT("Unknown identity is reported"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("Unknown regression scenario identity"));
        }));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].RenderFps = 60;
    TestFalse(TEXT("Canonical identity with the wrong render rate cannot pass"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].Handedness = EDGHandedness::Left;
    TestFalse(TEXT("Left-handed results cannot masquerade as the canonical right-handed baseline"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    TestTrue(TEXT("Canonical baseline handedness mismatch is reported"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("handedness Left expected Right"));
        }));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].GroundDistanceMeters = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite frame-comparison metrics cannot pass"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].WindPhaseOriginSeconds = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite wind phase provenance cannot pass"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[1].WindPhaseOriginSeconds = Suite[0].WindPhaseOriginSeconds;
    TestFalse(TEXT("A regression suite cannot reuse one wind phase for two accepted throws"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].RegressionFailures.Add(TEXT("stale failure"));
    TestFalse(TEXT("A summary cannot claim PASS while retaining failures"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].LastBasketContact = static_cast<EBasketContactResult>(255);
    TestFalse(TEXT("Unknown summary enums cannot pass"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].SampleCount = 1;
    TestFalse(TEXT("A summary without a real sampled trajectory cannot pass"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    TestTrue(TEXT("Invalid sample count is reported"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("sample_count"));
        }));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[4].bHoledOut = false;
    TestFalse(TEXT("The Circle 1 canonical putt must hole out"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[5].LastBasketContact = EBasketContactResult::ChainDeflection;
    TestFalse(TEXT("The Circle 2 canonical putt must finish Caught"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));

    Suite = MakePassingCanonicalRegressionSuite();
    Suite[0].FinalCarryMeters += 1.0f;
    TestFalse(TEXT("Frame-rate disagreement still fails the canonical suite"),
        UDiscTrajectorySubsystem::EvaluateRegressionSuite(Suite, Failures));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectoryRegressionReportAcceptanceTest,
    "DiscGolfTour.Trajectory.RegressionReportAcceptance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectoryRegressionReportAcceptanceTest::RunTest(const FString& Parameters)
{
    const TArray<FDiscTrajectorySummary> Suite = MakePassingCanonicalRegressionSuite();
    TArray<FString> Failures;
    TestTrue(TEXT("Authoritative presentation-isolated evidence passes report acceptance"),
        UDiscTrajectorySubsystem::EvaluateRegressionReportAcceptance(
            Suite, true, true, Failures));
    TestTrue(TEXT("Accepted report evidence has no failures"), Failures.IsEmpty());

    TestFalse(TEXT("Presentation-contaminated evidence cannot pass report acceptance"),
        UDiscTrajectorySubsystem::EvaluateRegressionReportAcceptance(
            Suite, true, false, Failures));
    TestTrue(TEXT("Presentation isolation failure is reportable"),
        Failures.Contains(TEXT("Regression emitted presentation audio events")));

    TestFalse(TEXT("Fallback preset evidence cannot pass report acceptance"),
        UDiscTrajectorySubsystem::EvaluateRegressionReportAcceptance(
            Suite, false, true, Failures));
    TestTrue(TEXT("Fallback evidence is labeled diagnostic-only"),
        Failures.ContainsByPredicate([](const FString& Failure)
        {
            return Failure.Contains(TEXT("diagnostic-only"));
        }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscTrajectoryRegressionEvidenceSeamsTest,
    "DiscGolfTour.Trajectory.RegressionEvidenceSeams",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscTrajectoryRegressionEvidenceSeamsTest::RunTest(const FString& Parameters)
{
    const uint64 FullTraceSequence =
        static_cast<uint64>(DiscGolfPresentationAudio::MaximumTraceEvents);
    TestTrue(TEXT("An unchanged full presentation trace sequence remains isolated"),
        ADiscGolfTourGameMode::IsPresentationSequenceUnchanged(
            FullTraceSequence, FullTraceSequence));
    TestFalse(TEXT("A replacement event at full trace capacity is still detected"),
        ADiscGolfTourGameMode::IsPresentationSequenceUnchanged(
            FullTraceSequence, FullTraceSequence + 1));

    TestTrue(TEXT("An external reset cancels an active regression"),
        ADiscGolfTourGameMode::ShouldCancelRegressionForReset(true, false));
    TestFalse(TEXT("The suite's own between-preset reset is exempt"),
        ADiscGolfTourGameMode::ShouldCancelRegressionForReset(true, true));
    TestFalse(TEXT("An idle reset is not a regression cancellation"),
        ADiscGolfTourGameMode::ShouldCancelRegressionForReset(false, false));

    const uint8 AbcBytes[] = {0x61, 0x62, 0x63};
    TArray<uint8> Abc;
    Abc.Append(AbcBytes, UE_ARRAY_COUNT(AbcBytes));
    TestEqual(TEXT("Raw-byte SHA-1 seam matches the standard abc vector"),
        UDiscTrajectorySubsystem::ComputeRegressionPresetFileSha1(Abc),
        FString(TEXT("A9993E364706816ABA3E25717850C26C9CD0D89D")));

    TArray<uint8> PresetBytes;
    const FString PresetPath = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Data/PhysicsRegressionPresets.json"));
    TestTrue(TEXT("Canonical preset raw bytes are available"),
        FFileHelper::LoadFileToArray(PresetBytes, *PresetPath));
    const FString PresetSha1 =
        UDiscTrajectorySubsystem::ComputeRegressionPresetFileSha1(PresetBytes);
    TestTrue(TEXT("Current raw preset file matches the frozen authority digest"),
        UDiscTrajectorySubsystem::IsRegressionPresetDigestAuthoritative(PresetSha1));
    FString MutatedSha1 = PresetSha1;
    if (!MutatedSha1.IsEmpty())
    {
        MutatedSha1[0] = MutatedSha1[0] == TCHAR('0') ? TCHAR('1') : TCHAR('0');
    }
    TestFalse(TEXT("A one-nibble preset digest mutation is diagnostic-only"),
        UDiscTrajectorySubsystem::IsRegressionPresetDigestAuthoritative(MutatedSha1));
    return true;
}

#endif
