#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscTrajectorySubsystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

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
    OutTelemetry.GroundState = EDiscGroundState::Settled;
    OutTelemetry.GroundSurface = EGroundSurfaceType::Fairway;
    OutTelemetry.GroundContactCount = 2;
    OutTelemetry.GroundDistanceMeters = 10.0f;
    OutLie.SurfaceAtRest = ECourseSurfaceType::Fairway;
    OutLie.PlayingSurface = ECourseSurfaceType::Fairway;
    OutLie.LieType = ELieType::Fairway;
    OutLie.LieLocationCm = Locations.Last();
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
        TestEqual(TEXT("Trajectory schema is versioned"), Root->GetIntegerField(TEXT("schema_version")), 3);
        TestEqual(TEXT("JSON preserves all samples"), Root->GetArrayField(TEXT("samples")).Num(), 4);
        TestEqual(TEXT("JSON preserves all transitions"), Root->GetArrayField(TEXT("ground_transitions")).Num(), 2);
        TestEqual(TEXT("JSON records shot context"),
            Root->GetObjectField(TEXT("release"))->GetStringField(TEXT("shot_context")), FString(TEXT("Drive")));
        TestEqual(TEXT("JSON records basket outcome"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("last_basket_contact")), FString(TEXT("None")));
        TestEqual(TEXT("JSON records resulting lie"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("resulting_lie_type")), FString(TEXT("Fairway")));
        TestEqual(TEXT("JSON records course surface"),
            Root->GetObjectField(TEXT("summary"))->GetStringField(TEXT("surface_at_rest")), FString(TEXT("Fairway")));
        const TSharedPtr<FJsonObject> FinalTelemetry = Root->GetObjectField(TEXT("final_telemetry"));
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
    TestTrue(TEXT("CSV declares its versioned schema"), Csv.Contains(TEXT("# schema_version=3")));
    TestTrue(TEXT("CSV contains sample rows"), Csv.Contains(TEXT("sample,0,0.000000")));
    TestTrue(TEXT("CSV contains explicit transition rows"), Csv.Contains(TEXT("transition,0,2.000000")));
    TestTrue(TEXT("CSV includes wind columns"), Csv.Contains(TEXT("wind_x_mps,wind_y_mps,wind_z_mps")));
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
    return true;
}

#endif
