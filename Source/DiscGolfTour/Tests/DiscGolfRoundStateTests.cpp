#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseDefinition.h"
#include "../DiscGolfRoundState.h"

namespace
{
    TArray<FDiscGolfHoleBlockoutDefinition> MakeThreeHoles()
    {
        return {
            DiscGolfCourseDefinition::PineRidgeHole1Fallback(),
            DiscGolfCourseDefinition::PineRidgeHole2Fallback(),
            DiscGolfCourseDefinition::PineRidgeHole3Fallback()
        };
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfRoundProgressionTest,
    "DiscGolfTour.Round.ProgressionAndTotals",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfRoundProgressionTest::RunTest(const FString& Parameters)
{
    FDiscGolfRoundState Round;
    FString Error;
    TestTrue(TEXT("Round initializes from contiguous definitions"), DiscGolfRound::Initialize(
        Round, TEXT("PineRidgeChampionship"), TEXT("Championship"),
        FText::FromString(TEXT("Pine Ridge Championship")), MakeThreeHoles(), Error));
    TestEqual(TEXT("Round starts on hole one"), Round.CurrentHoleIndex, 0);
    TestTrue(TEXT("Hole one records"), DiscGolfRound::RecordCurrentHole(Round, 3, 0, Error));
    TestTrue(TEXT("Completed hole can advance"), DiscGolfRound::CanAdvance(Round));
    TestTrue(TEXT("Round advances to hole two"), DiscGolfRound::Advance(Round, Error));
    TestTrue(TEXT("Hole two records with penalty"), DiscGolfRound::RecordCurrentHole(Round, 5, 1, Error));
    TestTrue(TEXT("Round advances to hole three"), DiscGolfRound::Advance(Round, Error));
    TestTrue(TEXT("Final hole records"), DiscGolfRound::RecordCurrentHole(Round, 4, 0, Error));
    TestTrue(TEXT("Final score completes the round"), Round.bRoundComplete);
    TestEqual(TEXT("All three holes are complete"), DiscGolfRound::CompletedHoleCount(Round), 3);
    TestEqual(TEXT("Round total is twelve"), DiscGolfRound::TotalStrokes(Round), 12);
    TestEqual(TEXT("Completed par is eleven"), DiscGolfRound::CompletedPar(Round), 11);
    TestEqual(TEXT("Round is one over"), DiscGolfRound::ScoreToPar(Round), 1);
    TestEqual(TEXT("Positive score label includes plus"), DiscGolfRound::ScoreLabel(1), FString(TEXT("+1")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfRoundGuardrailsTest,
    "DiscGolfTour.Round.Guardrails",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfRoundGuardrailsTest::RunTest(const FString& Parameters)
{
    FDiscGolfRoundState Round;
    FString Error;
    TestTrue(TEXT("Round initializes"), DiscGolfRound::Initialize(
        Round, TEXT("PineRidgeChampionship"), TEXT("Championship"),
        FText::FromString(TEXT("Pine Ridge Championship")), MakeThreeHoles(), Error));
    TestFalse(TEXT("Unplayed hole cannot advance"), DiscGolfRound::Advance(Round, Error));
    TestTrue(TEXT("Guardrail explains completion requirement"), Error.Contains(TEXT("complete")));
    TestTrue(TEXT("Valid score records once"), DiscGolfRound::RecordCurrentHole(Round, 2, 0, Error));
    TestFalse(TEXT("Duplicate score is rejected"), DiscGolfRound::RecordCurrentHole(Round, 2, 0, Error));
    FDiscGolfRoundState PartialFinalRound = Round;
    PartialFinalRound.CurrentHoleIndex = 2;
    TestTrue(TEXT("Direct final-hole score can be recorded for developer preview"),
        DiscGolfRound::RecordCurrentHole(PartialFinalRound, 4, 0, Error));
    TestFalse(TEXT("A partial direct-selection score does not complete the round"), PartialFinalRound.bRoundComplete);
    TestEqual(TEXT("Even-par label is E"), DiscGolfRound::ScoreLabel(0), FString(TEXT("E")));
    TestEqual(TEXT("Negative score label is compact"), DiscGolfRound::ScoreLabel(-2), FString(TEXT("-2")));
    return true;
}

#endif
