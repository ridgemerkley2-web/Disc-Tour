#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "DiscGolfCareerProgressSaveGame.h"
#include "DiscGolfCareerSubsystem.h"
#include "DiscGolfCompetitionRuntime.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfScoringLibrary.h"

namespace
{
UDiscGolfCareerSubsystem* MakeCareerSubsystem()
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    return NewObject<UDiscGolfCareerSubsystem>(GameInstance);
}

FDiscGolfRoundState MakeCompletedPineRidgeRound()
{
    FDiscGolfRoundState Round;
    Round.CourseId = TEXT("PineRidgeChampionship");
    Round.LayoutId = TEXT("Championship");
    Round.CourseName = FText::FromString(TEXT("Pine Ridge Championship"));
    Round.CurrentHoleIndex = 2;
    Round.bRoundComplete = true;

    const int32 Pars[] = { 3, 4, 4 };
    const int32 StrokesIncludingPenalty[] = { 3, 4, 5 };
    const int32 Penalties[] = { 1, 0, 2 };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FDiscGolfRoundHoleScore& Hole = Round.HoleScores.AddDefaulted_GetRef();
        Hole.HoleNumber = Index + 1;
        Hole.HoleName = FText::FromString(FString::Printf(TEXT("Hole %d"), Index + 1));
        Hole.Par = Pars[Index];
        Hole.Strokes = StrokesIncludingPenalty[Index];
        Hole.PenaltyStrokes = Penalties[Index];
        Hole.bCompleted = true;
    }
    return Round;
}

FString ProgressFingerprint(const FDGCareerProgress& Progress)
{
    FString Fingerprint = FString::Printf(
        TEXT("%d|%d|%.6f|%d|"),
        Progress.SeasonNumber,
        Progress.CareerCurrency,
        Progress.PlayerRating,
        Progress.WorldRank);
    for (const FName EventId : Progress.CompletedEventIds)
    {
        Fingerprint += EventId.ToString() + TEXT(";");
    }
    Fingerprint += TEXT("|");
    for (const FDGRoundScorecard& Scorecard : Progress.RoundHistory)
    {
        Fingerprint += DiscGolfCompetitionRuntime::MakeResultId(Scorecard).ToString();
        Fingerprint += FString::Printf(
            TEXT(":%d:%d;"),
            UDiscGolfScoringLibrary::CalculateTotalStrokes(Scorecard),
            UDiscGolfScoringLibrary::CalculateToPar(Scorecard));
    }
    Fingerprint += FString::Printf(TEXT("|%d"), Progress.Sponsorships.Num());
    return Fingerprint;
}

FDGRoundScorecard MakeMinimalScorecard(const FName EventId, const FName CourseId)
{
    FDGRoundScorecard Scorecard;
    Scorecard.EventId = EventId;
    Scorecard.CourseId = CourseId;
    Scorecard.RoundNumber = 1;
    FDGScorecardHole& Hole = Scorecard.Holes.AddDefaulted_GetRef();
    Hole.HoleNumber = 1;
    Hole.Par = 3;
    Hole.Strokes = 3;
    Hole.bComplete = true;
    return Scorecard;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14FallbackEventTest,
    "DiscGolfTour.Session14.CompetitionCareer.GenericFallbackEvent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14FallbackEventTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfCompetitionRuntimeDefinition Event =
        DiscGolfCompetitionRuntime::BuildSourceFallbackEvent();
    FString Error;
    TestTrue(TEXT("Source fallback validates"),
        DiscGolfCompetitionRuntime::ValidateEvent(Event, Error));
    TestEqual(TEXT("Event identity is original and stable"),
        Event.EventId, FName(TEXT("PineRidgeChampionship")));
    TestEqual(TEXT("Only generic brand is active"),
        Event.PresentingBrandId, FName(TEXT("dg_generic")));
    TestEqual(TEXT("Event is StrokePlay"), Event.Format, EDGCompetitionFormat::StrokePlay);
    TestEqual(TEXT("Event has exactly one round"), Event.Rounds.Num(), 1);
    TestEqual(TEXT("Round has exactly three holes"), Event.Rounds[0].HoleCount, 3);
    TestEqual(TEXT("Event has exact Pine Ridge pars"), Event.HolePars, TArray<int32>({ 3, 4, 4 }));

    FDiscGolfCompetitionRuntimeDefinition UnknownBrandMutation = Event;
    UnknownBrandMutation.PresentingBrandId = TEXT("unknown_generic_brand");
    TestFalse(TEXT("Unknown brand activation fails closed"),
        DiscGolfCompetitionRuntime::ValidateEvent(UnknownBrandMutation, Error));
    TestEqual(TEXT("Read-only validation does not normalize the rejected brand"),
        UnknownBrandMutation.PresentingBrandId, FName(TEXT("unknown_generic_brand")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14ScorecardConversionTest,
    "DiscGolfTour.Session14.CompetitionCareer.ScorecardPenaltyConversion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14ScorecardConversionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfCompetitionRuntimeDefinition Event =
        DiscGolfCompetitionRuntime::BuildSourceFallbackEvent();
    const FDiscGolfRoundState Round = MakeCompletedPineRidgeRound();
    FDGRoundScorecard Scorecard;
    FString Error;
    TestTrue(TEXT("Completed project round converts"),
        DiscGolfCompetitionRuntime::BuildCompletedScorecard(Event, Round, Scorecard, Error));
    TestEqual(TEXT("Converted scorecard retains three holes"), Scorecard.Holes.Num(), 3);
    TestEqual(TEXT("Hole one base strokes exclude its recorded penalty"),
        Scorecard.Holes[0].Strokes, 2);
    TestEqual(TEXT("Hole one retains penalty metadata"),
        Scorecard.Holes[0].PenaltyStrokes, 1);
    TestEqual(TEXT("Hole three base strokes exclude both penalty strokes"),
        Scorecard.Holes[2].Strokes, 3);
    TestEqual(TEXT("Plugin total equals project total without double counting"),
        UDiscGolfScoringLibrary::CalculateTotalStrokes(Scorecard),
        DiscGolfRound::TotalStrokes(Round));
    TestEqual(TEXT("Plugin to-par equals project to-par"),
        UDiscGolfScoringLibrary::CalculateToPar(Scorecard),
        DiscGolfRound::ScoreToPar(Round));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14ScorecardAtomicFailureTest,
    "DiscGolfTour.Session14.CompetitionCareer.ScorecardAtomicFailure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14ScorecardAtomicFailureTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfCompetitionRuntimeDefinition Event =
        DiscGolfCompetitionRuntime::BuildSourceFallbackEvent();
    FDiscGolfRoundState Incomplete = MakeCompletedPineRidgeRound();
    Incomplete.bRoundComplete = false;

    FDGRoundScorecard Sentinel;
    Sentinel.EventId = TEXT("SentinelEvent");
    Sentinel.CourseId = TEXT("SentinelCourse");
    Sentinel.RoundNumber = 42;
    Sentinel.Holes = MakeMinimalScorecard(TEXT("SentinelEvent"), TEXT("SentinelCourse")).Holes;
    const FName BeforeEvent = Sentinel.EventId;
    const FName BeforeCourse = Sentinel.CourseId;
    const int32 BeforeRound = Sentinel.RoundNumber;
    const int32 BeforeHoles = Sentinel.Holes.Num();

    FString Error;
    TestFalse(TEXT("Incomplete project round is rejected"),
        DiscGolfCompetitionRuntime::BuildCompletedScorecard(Event, Incomplete, Sentinel, Error));
    TestEqual(TEXT("Failed conversion preserves output event"), Sentinel.EventId, BeforeEvent);
    TestEqual(TEXT("Failed conversion preserves output course"), Sentinel.CourseId, BeforeCourse);
    TestEqual(TEXT("Failed conversion preserves output round"), Sentinel.RoundNumber, BeforeRound);
    TestEqual(TEXT("Failed conversion preserves output holes"), Sentinel.Holes.Num(), BeforeHoles);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14CareerCommitTest,
    "DiscGolfTour.Session14.CompetitionCareer.AtomicCommitAndDuplicateEvent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14CareerCommitTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCareerSubsystem* Career = MakeCareerSubsystem();
    const FDiscGolfCompetitionRuntimeDefinition Event =
        DiscGolfCompetitionRuntime::BuildSourceFallbackEvent();
    const FDiscGolfRoundState Round = MakeCompletedPineRidgeRound();
    FString Error;
    TestTrue(TEXT("Completed event commits exactly once"),
        Career->CommitCompletedRound(Event, Round, Error));
    TestEqual(TEXT("One event identity is retained"),
        Career->GetProgress().CompletedEventIds.Num(), 1);
    TestEqual(TEXT("One scorecard result is retained"),
        Career->GetProgress().RoundHistory.Num(), 1);
    TestEqual(TEXT("Persisted plugin total is exact"),
        UDiscGolfScoringLibrary::CalculateTotalStrokes(
            Career->GetProgress().RoundHistory[0]), 12);
    TestEqual(TEXT("Persisted plugin to-par is exact"),
        UDiscGolfScoringLibrary::CalculateToPar(
            Career->GetProgress().RoundHistory[0]), 1);

    const FString Before = ProgressFingerprint(Career->GetProgress());
    TestFalse(TEXT("Duplicate event result is rejected"),
        Career->CommitCompletedRound(Event, Round, Error));
    TestTrue(TEXT("Duplicate rejection is explicit"), Error.Contains(TEXT("already")));
    TestEqual(TEXT("Duplicate rejection does not mutate career"),
        ProgressFingerprint(Career->GetProgress()), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14CareerValidationTest,
    "DiscGolfTour.Session14.CompetitionCareer.StrictHistoryValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14CareerValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Error;
    FDGCareerProgress DuplicateEvents;
    DuplicateEvents.CompletedEventIds = { TEXT("EventA"), TEXT("EventA") };
    DuplicateEvents.RoundHistory = {
        MakeMinimalScorecard(TEXT("EventA"), TEXT("CourseA")),
        MakeMinimalScorecard(TEXT("EventB"), TEXT("CourseB"))
    };
    TestFalse(TEXT("Duplicate completed events are rejected"),
        UDiscGolfCareerSubsystem::ValidateProgress(DuplicateEvents, Error));
    TestTrue(TEXT("Duplicate event rejection is explicit"), Error.Contains(TEXT("duplicate completed event")));

    FDGCareerProgress DuplicateResults;
    DuplicateResults.CompletedEventIds = { TEXT("EventA"), TEXT("EventB") };
    DuplicateResults.RoundHistory = {
        MakeMinimalScorecard(TEXT("EventA"), TEXT("CourseA")),
        MakeMinimalScorecard(TEXT("EventA"), TEXT("CourseA"))
    };
    TestFalse(TEXT("Duplicate result identities are rejected"),
        UDiscGolfCareerSubsystem::ValidateProgress(DuplicateResults, Error));
    TestTrue(TEXT("Duplicate result rejection is explicit"), Error.Contains(TEXT("duplicate round result")));

    FDGCareerProgress Oversized;
    for (int32 Index = 0; Index <= UDiscGolfCareerSubsystem::MaxCompletedEvents; ++Index)
    {
        const FName EventId(*FString::Printf(TEXT("Event%03d"), Index));
        const FName CourseId(*FString::Printf(TEXT("Course%03d"), Index));
        Oversized.CompletedEventIds.Add(EventId);
        Oversized.RoundHistory.Add(MakeMinimalScorecard(EventId, CourseId));
    }
    TestFalse(TEXT("Career history is absolutely bounded"),
        UDiscGolfCareerSubsystem::ValidateProgress(Oversized, Error));
    TestTrue(TEXT("Bound rejection is explicit"), Error.Contains(TEXT("exceed bounds")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14CareerLoadAtomicityTest,
    "DiscGolfTour.Session14.CompetitionCareer.AtomicLoadAndFutureSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14CareerLoadAtomicityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCareerSubsystem* Career = MakeCareerSubsystem();
    FString Error;
    TestTrue(TEXT("Baseline career commit succeeds"),
        Career->CommitCompletedRound(
            DiscGolfCompetitionRuntime::BuildSourceFallbackEvent(),
            MakeCompletedPineRidgeRound(), Error));
    const FString Before = ProgressFingerprint(Career->GetProgress());

    UDiscGolfCareerProgressSaveGame* Future = NewObject<UDiscGolfCareerProgressSaveGame>();
    Future->SchemaVersion = UDiscGolfCareerProgressSaveGame::CurrentSchemaVersion + 1;
    Future->Progress = Career->GetProgress();
    TestFalse(TEXT("Future career schema is rejected"),
        Career->LoadCareerFromSaveGame(Future, Error));
    TestEqual(TEXT("Future schema rejection is atomic"),
        ProgressFingerprint(Career->GetProgress()), Before);

    UDiscGolfCareerProgressSaveGame* Corrupt = NewObject<UDiscGolfCareerProgressSaveGame>();
    Corrupt->Progress = Career->GetProgress();
    const FName DuplicateEventId = Corrupt->Progress.CompletedEventIds[0];
    const FDGRoundScorecard DuplicateResult = Corrupt->Progress.RoundHistory[0];
    Corrupt->Progress.CompletedEventIds.Add(DuplicateEventId);
    Corrupt->Progress.RoundHistory.Add(DuplicateResult);
    TestFalse(TEXT("Corrupt schema-v1 history is rejected"),
        Career->LoadCareerFromSaveGame(Corrupt, Error));
    TestEqual(TEXT("Corrupt load rejection is atomic"),
        ProgressFingerprint(Career->GetProgress()), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14CareerIsolatedSaveTest,
    "DiscGolfTour.Session14.CompetitionCareer.IsolatedSchemaV1SaveRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14CareerIsolatedSaveTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("Career proof has an independent schema"),
        UDiscGolfCareerProgressSaveGame::CurrentSchemaVersion, 1);

    const FString SlotName = FString::Printf(
        TEXT("DGT_Career_Automation_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    constexpr int32 UserIndex = 0;
    UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

    UDiscGolfCareerSubsystem* Source = MakeCareerSubsystem();
    UDiscGolfCareerSubsystem* Loaded = MakeCareerSubsystem();
    FString Error;
    TestTrue(TEXT("Source career commits before save"),
        Source->CommitCompletedRound(
            DiscGolfCompetitionRuntime::BuildSourceFallbackEvent(),
            MakeCompletedPineRidgeRound(), Error));
    TestTrue(TEXT("Career writes only the disposable explicit slot"),
        Source->SaveCareerToSlot(SlotName, UserIndex, Error));
    TestTrue(TEXT("Career loads only the disposable explicit slot"),
        Loaded->LoadCareerFromSlot(SlotName, UserIndex, Error));
    TestEqual(TEXT("Isolated save restores exact progress"),
        ProgressFingerprint(Loaded->GetProgress()),
        ProgressFingerprint(Source->GetProgress()));

    TestTrue(TEXT("Disposable career save is deleted"),
        UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex));
    TestFalse(TEXT("Disposable career save leaves no slot"),
        UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex));
    return true;
}

#endif
