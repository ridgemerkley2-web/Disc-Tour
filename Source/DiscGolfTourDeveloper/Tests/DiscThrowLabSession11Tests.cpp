#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "DiscThrowLabSaveGame.h"
#include "DiscThrowLabSubsystem.h"

#include <limits>

namespace
{
UDiscThrowLabSubsystem* MakeThrowLabSubsystem()
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    return NewObject<UDiscThrowLabSubsystem>(GameInstance);
}

void MakeThrowLabCapture(
    const FString& CaptureId,
    EDiscShotContext ShotContext,
    int32 SampleCount,
    float DurationSeconds,
    FResolvedDiscDefinition& OutDisc,
    FThrowRelease& OutRelease,
    TArray<FDiscTrajectorySample>& OutSamples,
    TArray<FDiscGroundTransition>& OutTransitions,
    FDiscFlightTelemetry& OutTelemetry,
    FDiscTrajectorySummary& OutSummary)
{
    OutDisc = FResolvedDiscDefinition();
    OutRelease = FThrowRelease();
    OutSamples.Reset();
    OutTransitions.Reset();
    OutTelemetry = FDiscFlightTelemetry();
    OutSummary = FDiscTrajectorySummary();

    OutDisc.MoldId = TEXT("Apex");
    OutDisc.DisplayName = FText::FromString(TEXT("Apex"));
    OutDisc.Plastic = EDiscPlastic::Tour;

    OutRelease.Direction = FVector::ForwardVector;
    OutRelease.ReleaseSpeedMps = 24.0f;
    OutRelease.SpinRpm = 900.0f;
    OutRelease.ShotContext = ShotContext;

    const int32 BoundaryIndex = FMath::Max(1, SampleCount * 3 / 4);
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        FDiscTrajectorySample& Sample = OutSamples.AddDefaulted_GetRef();
        const float Alpha = static_cast<float>(Index) / static_cast<float>(SampleCount - 1);
        Sample.TimeSeconds = DurationSeconds * Alpha;
        Sample.WorldLocationCm = FVector(Index, Index * 0.25f, 100.0f + 50.0f * FMath::Sin(Alpha * PI));
        Sample.VelocityMps = Index + 1 < SampleCount ? FVector(24.0f * (1.0f - Alpha), 0.0f, 1.0f) : FVector::ZeroVector;
        Sample.SpinRpm = Index + 1 < SampleCount ? 900.0f * (1.0f - Alpha) : 0.0f;
        if (Index >= BoundaryIndex)
        {
            Sample.GroundState = Index + 1 < SampleCount
                ? EDiscGroundState::Sliding : EDiscGroundState::Settled;
            Sample.GroundContactCount = 1;
        }
    }

    FDiscGroundTransition& Contact = OutTransitions.AddDefaulted_GetRef();
    Contact.TimeSeconds = OutSamples[BoundaryIndex].TimeSeconds;
    Contact.WorldLocationCm = OutSamples[BoundaryIndex].WorldLocationCm;
    Contact.FromState = EDiscGroundState::Airborne;
    Contact.ToState = EDiscGroundState::Sliding;
    Contact.GroundContactCount = 1;
    Contact.ImpactSpeedMps = 8.0f;

    OutTelemetry.State = EDiscFlightState::Settled;
    OutTelemetry.GroundState = EDiscGroundState::Settled;
    OutTelemetry.GroundContactCount = 1;
    OutTelemetry.FlightTimeSeconds = DurationSeconds;
    OutTelemetry.Release = OutRelease;

    OutSummary.CaptureId = CaptureId;
    OutSummary.CapturedUtc = TEXT("2026-08-24T12:00:00.000Z");
    OutSummary.SampleCount = OutSamples.Num();
    OutSummary.GroundTransitionCount = OutTransitions.Num();
    OutSummary.DurationSeconds = DurationSeconds;
    OutSummary.AirTimeSeconds = Contact.TimeSeconds;
    OutSummary.AirCarryMeters = 50.0f;
    OutSummary.FinalCarryMeters = 60.0f;
    OutSummary.ApexMeters = 4.0f;
    OutSummary.StartWorldLocationCm = OutSamples[0].WorldLocationCm;
    OutSummary.FinalWorldLocationCm = OutSamples.Last().WorldLocationCm;
    OutSummary.FinalGroundState = EDiscGroundState::Settled;
    OutSummary.GroundContactCount = 1;
}

bool AddCapture(
    UDiscThrowLabSubsystem* Lab,
    const FString& CaptureId,
    EDiscShotContext ShotContext,
    FString& OutError)
{
    FResolvedDiscDefinition Disc;
    FThrowRelease Release;
    TArray<FDiscTrajectorySample> Samples;
    TArray<FDiscGroundTransition> Transitions;
    FDiscFlightTelemetry Telemetry;
    FDiscTrajectorySummary Summary;
    MakeThrowLabCapture(
        CaptureId, ShotContext, 121, 1.0f,
        Disc, Release, Samples, Transitions, Telemetry, Summary);
    return Lab->RecordCompletedThrow(
        Disc, Release, Samples, Transitions, Telemetry, Summary, OutError);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscThrowLabRecordValidationTest,
    "DiscGolfTour.Session11.ThrowLab.RecordValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscThrowLabRecordValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FResolvedDiscDefinition Disc;
    FThrowRelease Release;
    TArray<FDiscTrajectorySample> Samples;
    TArray<FDiscGroundTransition> Transitions;
    FDiscFlightTelemetry Telemetry;
    FDiscTrajectorySummary Summary;
    MakeThrowLabCapture(
        TEXT("throw_lab_record_001"), EDiscShotContext::Drive, 481, 2.0f,
        Disc, Release, Samples, Transitions, Telemetry, Summary);

    FDiscThrowLabRecord Record;
    FString Error;
    TestTrue(TEXT("One completed authoritative capture builds one record"),
        UDiscThrowLabSubsystem::BuildRecord(
            Disc, Release, Samples, Transitions, Telemetry, Summary, Record, Error));
    TestEqual(TEXT("Record identity is the trajectory capture identity"), Record.RecordId, Summary.CaptureId);
    TestEqual(TEXT("Record retains source sample count"), Record.SourceSampleCount, Samples.Num());
    TestTrue(TEXT("Record validates independently"), UDiscThrowLabSubsystem::ValidateRecord(Record, Error));

    Samples[20].SpinRpm = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite solver data is rejected"),
        UDiscThrowLabSubsystem::BuildRecord(
            Disc, Release, Samples, Transitions, Telemetry, Summary, Record, Error));
    TestTrue(TEXT("Finite rejection is explicit"), Error.Contains(TEXT("finite")));

    Samples[20].SpinRpm = 850.0f;
    Summary.CaptureId = TEXT("unsafe/capture");
    TestFalse(TEXT("Unstable capture identity is rejected"),
        UDiscThrowLabSubsystem::BuildRecord(
            Disc, Release, Samples, Transitions, Telemetry, Summary, Record, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscThrowLabReplayBoundTest,
    "DiscGolfTour.Session11.ThrowLab.ReplayBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscThrowLabReplayBoundTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FResolvedDiscDefinition Disc;
    FThrowRelease Release;
    TArray<FDiscTrajectorySample> Samples;
    TArray<FDiscGroundTransition> Transitions;
    FDiscFlightTelemetry Telemetry;
    FDiscTrajectorySummary Summary;
    MakeThrowLabCapture(
        TEXT("throw_lab_replay_001"), EDiscShotContext::Drive, 4801, 20.0f,
        Disc, Release, Samples, Transitions, Telemetry, Summary);

    TArray<FDiscTrajectorySample> FirstPass;
    TArray<FDiscTrajectorySample> SecondPass;
    FString Error;
    TestTrue(TEXT("High-rate source deterministically downsamples"),
        UDiscThrowLabSubsystem::BuildBoundedReplaySamples(
            Samples, Transitions, FirstPass, Error));
    TestTrue(TEXT("Repeated downsample succeeds"),
        UDiscThrowLabSubsystem::BuildBoundedReplaySamples(
            Samples, Transitions, SecondPass, Error));
    TestEqual(TEXT("Replay selection is deterministic"), FirstPass.Num(), SecondPass.Num());
    TestTrue(TEXT("Replay remains at or below 60 Hz"),
        static_cast<float>(FirstPass.Num() - 1) / 20.0f
            <= UDiscThrowLabSubsystem::MaxReplaySampleRateHz + 0.01f);
    TestTrue(TEXT("Replay remains inside the absolute sample bound"),
        FirstPass.Num() <= UDiscThrowLabSubsystem::MaxReplaySamples);
    TestEqual(TEXT("Replay retains first actual sample"), FirstPass[0].TimeSeconds, Samples[0].TimeSeconds);
    TestEqual(TEXT("Replay retains last actual sample"), FirstPass.Last().TimeSeconds, Samples.Last().TimeSeconds);

    const float BoundaryTime = Transitions[0].TimeSeconds;
    TestTrue(TEXT("Replay retains the discrete contact boundary"),
        FirstPass.ContainsByPredicate([BoundaryTime](const FDiscTrajectorySample& Sample)
        {
            return Sample.GroundContactCount == 1
                && FMath::IsNearlyEqual(Sample.TimeSeconds, BoundaryTime, KINDA_SMALL_NUMBER);
        }));
    bool bEverySampleCameFromSource = true;
    for (int32 Index = 0; Index < FirstPass.Num(); ++Index)
    {
        bEverySampleCameFromSource &= Samples.ContainsByPredicate(
            [&FirstPass, Index](const FDiscTrajectorySample& Source)
            {
                return Source.TimeSeconds == FirstPass[Index].TimeSeconds
                    && Source.WorldLocationCm == FirstPass[Index].WorldLocationCm;
            });
        bEverySampleCameFromSource &= FirstPass[Index].TimeSeconds == SecondPass[Index].TimeSeconds;
    }
    TestTrue(TEXT("Replay contains actual source samples only"), bEverySampleCameFromSource);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscThrowLabStorageTest,
    "DiscGolfTour.Session11.ThrowLab.BoundedStoragePinSelectDelete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscThrowLabStorageTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscThrowLabSubsystem* Lab = MakeThrowLabSubsystem();
    FString Error;
    for (int32 Index = 0; Index < UDiscThrowLabSubsystem::MaxRecords; ++Index)
    {
        TestTrue(FString::Printf(TEXT("Capture %d records"), Index),
            AddCapture(Lab, FString::Printf(TEXT("bounded_%03d"), Index),
                EDiscShotContext::Drive, Error));
    }
    TestEqual(TEXT("Storage reaches but does not exceed its bound"),
        Lab->GetRecordCount(), UDiscThrowLabSubsystem::MaxRecords);

    TestTrue(TEXT("Oldest record can be selected"), Lab->SelectRecordByIndex(0, Error));
    bool bPinned = false;
    TestTrue(TEXT("Selected record can be pinned"), Lab->ToggleSelectedPinned(bPinned, Error));
    TestTrue(TEXT("Pin state is reported"), bPinned);
    TestTrue(TEXT("New capture evicts the oldest unpinned record"),
        AddCapture(Lab, TEXT("bounded_064"), EDiscShotContext::Drive, Error));
    TestEqual(TEXT("Storage remains bounded after eviction"),
        Lab->GetRecordCount(), UDiscThrowLabSubsystem::MaxRecords);
    TestEqual(TEXT("Pinned oldest record survives eviction"),
        Lab->GetRecordByIndex(0)->RecordId, FString(TEXT("bounded_000")));
    TestEqual(TEXT("First unpinned record was evicted"),
        Lab->GetRecordByIndex(1)->RecordId, FString(TEXT("bounded_002")));
    TestTrue(TEXT("New capture becomes selected"),
        Lab->GetSelectedRecord() && Lab->GetSelectedRecord()->RecordId == TEXT("bounded_064"));
    TestTrue(TEXT("Selected record can be deleted"), Lab->DeleteSelected(Error));
    TestEqual(TEXT("Delete removes exactly one record"),
        Lab->GetRecordCount(), UDiscThrowLabSubsystem::MaxRecords - 1);
    TestTrue(TEXT("Offset selection wraps"), Lab->SelectRecordByOffset(-1, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscThrowLabComparisonTest,
    "DiscGolfTour.Session11.ThrowLab.ExactTwoRecordComparison",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscThrowLabComparisonTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscThrowLabSubsystem* Lab = MakeThrowLabSubsystem();
    FString Error;
    TestTrue(TEXT("First drive records"), AddCapture(Lab, TEXT("compare_a"), EDiscShotContext::Drive, Error));
    TestTrue(TEXT("Second drive records"), AddCapture(Lab, TEXT("compare_b"), EDiscShotContext::Drive, Error));
    TestTrue(TEXT("Putt records"), AddCapture(Lab, TEXT("compare_putt"), EDiscShotContext::Circle1Putt, Error));

    TestTrue(TEXT("Exactly two compatible records compare"), Lab->SetComparisonByIndices(0, 1, Error));
    TestEqual(TEXT("Compatible comparison is explicit"), Lab->GetComparison().Compatibility,
        EDiscThrowLabComparisonCompatibility::Compatible);
    TestFalse(TEXT("Same record cannot compare with itself"), Lab->SetComparisonByIndices(0, 0, Error));
    TestEqual(TEXT("Same-record incompatibility is explicit"), Lab->GetComparison().Compatibility,
        EDiscThrowLabComparisonCompatibility::SameRecord);
    TestFalse(TEXT("Drive and putt are not presented as comparable"),
        Lab->SetComparisonByIndices(0, 2, Error));
    TestEqual(TEXT("Shot-context incompatibility is explicit"), Lab->GetComparison().Compatibility,
        EDiscThrowLabComparisonCompatibility::ShotContextMismatch);
    TestTrue(TEXT("Status text exposes incompatibility"),
        Lab->GetComparisonText().Contains(TEXT("INCOMPATIBLE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscThrowLabSeparateSaveTest,
    "DiscGolfTour.Session11.ThrowLab.SeparateSchemaV1Save",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscThrowLabSeparateSaveTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("Throw Lab save schema is independently versioned"),
        UDiscThrowLabSaveGame::CurrentSchemaVersion, 1);

    const FString SlotName = FString::Printf(TEXT("DGT_ThrowLab_Automation_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    UDiscThrowLabSubsystem* Source = MakeThrowLabSubsystem();
    UDiscThrowLabSubsystem* Loaded = MakeThrowLabSubsystem();
    FString Error;
    TestTrue(TEXT("Capture records before separate save"),
        AddCapture(Source, TEXT("save_roundtrip_001"), EDiscShotContext::Drive, Error));
    bool bPinned = false;
    TestTrue(TEXT("Pin records before separate save"), Source->ToggleSelectedPinned(bPinned, Error));
    TestTrue(TEXT("Throw Lab writes its own slot"), Source->SaveLabToSlot(SlotName, 0, Error));
    TestTrue(TEXT("Throw Lab loads its own slot"), Loaded->LoadLabFromSlot(SlotName, 0, Error));
    TestEqual(TEXT("Separate save restores one record"), Loaded->GetRecordCount(), 1);
    TestTrue(TEXT("Separate save restores stable selection"),
        Loaded->GetSelectedRecord() && Loaded->GetSelectedRecord()->RecordId == TEXT("save_roundtrip_001"));
    TestTrue(TEXT("Separate save restores pin state"),
        Loaded->GetSelectedRecord() && Loaded->GetSelectedRecord()->bPinned);
    UGameplayStatics::DeleteGameInSlot(SlotName, 0);
    return true;
}

#endif
