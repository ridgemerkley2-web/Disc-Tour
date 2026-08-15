#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseDefinition.h"
#include "../DiscGolfPlayerExperience.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHole1PlayerExperienceMetadataTest,
    "DiscGolfTour.PlayerExperience.Hole1MetadataAndUnits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHole1PlayerExperienceMetadataTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    FString Error;
    TestTrue(TEXT("Hole 1 loads"), DiscGolfCourseDefinition::LoadPineRidgeHole1(Definition, Source, Error));
    const float Feet = DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition);
    TestEqual(TEXT("Hole 1 is par 3"), Definition.Par, 3);
    TestEqual(TEXT("Production distance rounds to 362 feet"), FMath::RoundToInt(Feet), 362);
    TestEqual(TEXT("Imperial display is authoritative geometry"),
        DiscGolfPlayerExperience::FormatDistance(Feet / 3.280839895f, EDiscGolfUnitSystem::Imperial),
        FString(TEXT("362 FT")));
    TestEqual(TEXT("Metric display converts without changing geometry"),
        DiscGolfPlayerExperience::FormatDistance(Feet / 3.280839895f, EDiscGolfUnitSystem::Metric),
        FString(TEXT("110 M")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfScoreTerminologyTest,
    "DiscGolfTour.PlayerExperience.ScoreTerminology",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfScoreTerminologyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("One is always an ace"), DiscGolfPlayerExperience::HoleScoreName(1, 5), FString(TEXT("ACE")));
    TestEqual(TEXT("Birdie label"), DiscGolfPlayerExperience::HoleScoreName(2, 3), FString(TEXT("BIRDIE")));
    TestEqual(TEXT("Par label"), DiscGolfPlayerExperience::HoleScoreName(3, 3), FString(TEXT("PAR")));
    TestEqual(TEXT("Arbitrary over par remains valid"), DiscGolfPlayerExperience::HoleScoreName(12, 3), FString(TEXT("+9")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfLiePresentationClassificationTest,
    "DiscGolfTour.PlayerExperience.LieAndPenaltyPresentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfLiePresentationClassificationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfLieState Lie;
    Lie.LieType = ELieType::LightRough;
    Lie.PlayingSurface = ECourseSurfaceType::LightRough;
    TestEqual(TEXT("Light rough presents as semi-rough"),
        DiscGolfPlayerExperience::ClassifyLanding(Lie, false), EDiscGolfLandingClassification::SemiRough);
    Lie.PenaltyType = EDiscGolfPenaltyType::OutOfBounds;
    Lie.PenaltyStrokes = 1;
    TestEqual(TEXT("OB classification overrides lie surface"),
        DiscGolfPlayerExperience::ClassifyLanding(Lie, false), EDiscGolfLandingClassification::OutOfBounds);
    TestEqual(TEXT("OB presentation name"),
        DiscGolfPlayerExperience::LandingName(EDiscGolfLandingClassification::OutOfBounds),
        FString(TEXT("OUT OF BOUNDS")));
    TestEqual(TEXT("Hole completion presents as green"),
        DiscGolfPlayerExperience::ClassifyLanding(Lie, true), EDiscGolfLandingClassification::Green);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReplayRecordingBoundTest,
    "DiscGolfTour.PlayerExperience.ReplayRecordingBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReplayRecordingBoundTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDiscTrajectorySample> Source;
    for (int32 Index = 0; Index < 5000; ++Index)
    {
        FDiscTrajectorySample& Sample = Source.AddDefaulted_GetRef();
        Sample.TimeSeconds = Index / 240.0f;
        Sample.WorldLocationCm = FVector(Index * 2.0f, FMath::Sin(Index * 0.01f) * 100.0f, 100.0f);
        if (Index >= 3000)
        {
            Sample.GroundState = EDiscGroundState::Sliding;
            Sample.GroundContactCount = 1;
        }
    }
    TArray<FDiscTrajectorySample> Bounded;
    DiscGolfPlayerExperience::BuildBoundedReplaySamples(Source, Bounded, 1800);
    TestTrue(TEXT("Replay stays within memory/sample budget"), Bounded.Num() <= 1800);
    TestEqual(TEXT("Replay retains first frame"), Bounded[0].TimeSeconds, Source[0].TimeSeconds);
    TestEqual(TEXT("Replay retains final frame"), Bounded.Last().TimeSeconds, Source.Last().TimeSeconds);
    TestTrue(TEXT("Replay retains airborne-to-ground boundary"), Bounded.ContainsByPredicate([](const auto& Sample)
        { return Sample.GroundContactCount == 1 && Sample.TimeSeconds <= 3001.0f / 240.0f; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBasketMarkerStateTest,
    "DiscGolfTour.PlayerExperience.BasketMarkerState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBasketMarkerStateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfPlayerSettings Settings;
    TestTrue(TEXT("Obscured basket marker is readable"),
        DiscGolfPlayerExperience::BasketMarkerOpacity(Settings, false, 100.0f) > 0.8f);
    TestTrue(TEXT("Visible basket marker fades"),
        DiscGolfPlayerExperience::BasketMarkerOpacity(Settings, true, 100.0f) < 0.5f);
    TestEqual(TEXT("Marker hides inside close putting range"),
        DiscGolfPlayerExperience::BasketMarkerOpacity(Settings, true, 8.0f), 0.0f);
    Settings.bBasketMarkerVisible = false;
    TestEqual(TEXT("Player visibility preference wins"),
        DiscGolfPlayerExperience::BasketMarkerOpacity(Settings, false, 100.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSettingsNormalizationTest,
    "DiscGolfTour.PlayerExperience.SettingsNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSettingsNormalizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfPlayerSettings Settings;
    Settings.MasterVolume = 4.0f;
    Settings.HudScale = 0.1f;
    Settings.ControllerSensitivity = -10.0f;
    Settings.ResolutionX = 100;
    Settings.Normalize();
    TestEqual(TEXT("Volume clamps"), Settings.MasterVolume, 1.0f);
    TestEqual(TEXT("HUD accessibility scale clamps"), Settings.HudScale, 0.75f);
    TestEqual(TEXT("Controller sensitivity clamps"), Settings.ControllerSensitivity, 0.25f);
    TestEqual(TEXT("Resolution clamps"), Settings.ResolutionX, 1280);
    TestEqual(TEXT("Wind HUD uses player units"),
        DiscGolfPlayerExperience::FormatWind(3.57632f, EDiscGolfUnitSystem::Imperial), FString(TEXT("8 MPH")));
    return true;
}

#endif
