#include "DiscGolfRoundState.h"

#include "DiscGolfCourseDefinition.h"

bool DiscGolfRound::Initialize(
    FDiscGolfRoundState& OutRound,
    FName CourseId,
    FName LayoutId,
    const FText& CourseName,
    const TArray<FDiscGolfHoleBlockoutDefinition>& HoleDefinitions,
    FString& OutError)
{
    if (CourseId.IsNone() || LayoutId.IsNone() || CourseName.IsEmpty() || HoleDefinitions.IsEmpty())
    {
        OutError = TEXT("round identity or hole definitions are invalid");
        return false;
    }

    FDiscGolfRoundState Round;
    Round.CourseId = CourseId;
    Round.LayoutId = LayoutId;
    Round.CourseName = CourseName;
    for (int32 Index = 0; Index < HoleDefinitions.Num(); ++Index)
    {
        const FDiscGolfHoleBlockoutDefinition& Definition = HoleDefinitions[Index];
        if (Definition.CourseId != CourseId || Definition.LayoutId != LayoutId
            || Definition.HoleNumber != Index + 1 || Definition.Par <= 0)
        {
            OutError = TEXT("round holes must match the course and be contiguous");
            return false;
        }
        FDiscGolfRoundHoleScore Score;
        Score.HoleNumber = Definition.HoleNumber;
        Score.HoleName = Definition.HoleName;
        Score.Par = Definition.Par;
        Round.HoleScores.Add(Score);
    }
    OutRound = MoveTemp(Round);
    OutError.Reset();
    return true;
}

bool DiscGolfRound::RecordCurrentHole(
    FDiscGolfRoundState& Round,
    int32 Strokes,
    int32 PenaltyStrokes,
    FString& OutError)
{
    if (!Round.HoleScores.IsValidIndex(Round.CurrentHoleIndex) || Round.bRoundComplete)
    {
        OutError = TEXT("round has no active hole");
        return false;
    }
    FDiscGolfRoundHoleScore& Score = Round.HoleScores[Round.CurrentHoleIndex];
    if (Score.bCompleted)
    {
        OutError = TEXT("active hole has already been recorded");
        return false;
    }
    if (Strokes <= 0 || PenaltyStrokes < 0 || PenaltyStrokes > Strokes)
    {
        OutError = TEXT("hole score is invalid");
        return false;
    }
    Score.Strokes = Strokes;
    Score.PenaltyStrokes = PenaltyStrokes;
    Score.bCompleted = true;
    Round.bRoundComplete = CompletedHoleCount(Round) == Round.HoleScores.Num();
    OutError.Reset();
    return true;
}

bool DiscGolfRound::CanAdvance(const FDiscGolfRoundState& Round)
{
    return !Round.bRoundComplete
        && Round.HoleScores.IsValidIndex(Round.CurrentHoleIndex)
        && Round.HoleScores[Round.CurrentHoleIndex].bCompleted
        && Round.HoleScores.IsValidIndex(Round.CurrentHoleIndex + 1);
}

bool DiscGolfRound::Advance(FDiscGolfRoundState& Round, FString& OutError)
{
    if (!CanAdvance(Round))
    {
        OutError = TEXT("complete the active hole before advancing");
        return false;
    }
    ++Round.CurrentHoleIndex;
    OutError.Reset();
    return true;
}

int32 DiscGolfRound::CompletedHoleCount(const FDiscGolfRoundState& Round)
{
    int32 Count = 0;
    for (const FDiscGolfRoundHoleScore& Score : Round.HoleScores) if (Score.bCompleted) ++Count;
    return Count;
}

int32 DiscGolfRound::TotalStrokes(const FDiscGolfRoundState& Round)
{
    int32 Total = 0;
    for (const FDiscGolfRoundHoleScore& Score : Round.HoleScores) if (Score.bCompleted) Total += Score.Strokes;
    return Total;
}

int32 DiscGolfRound::TotalPenaltyStrokes(const FDiscGolfRoundState& Round)
{
    int32 Total = 0;
    for (const FDiscGolfRoundHoleScore& Score : Round.HoleScores) if (Score.bCompleted) Total += Score.PenaltyStrokes;
    return Total;
}

int32 DiscGolfRound::CompletedPar(const FDiscGolfRoundState& Round)
{
    int32 Total = 0;
    for (const FDiscGolfRoundHoleScore& Score : Round.HoleScores) if (Score.bCompleted) Total += Score.Par;
    return Total;
}

int32 DiscGolfRound::ScoreToPar(const FDiscGolfRoundState& Round)
{
    return TotalStrokes(Round) - CompletedPar(Round);
}

FString DiscGolfRound::ScoreLabel(int32 ScoreToParValue)
{
    if (ScoreToParValue == 0) return TEXT("E");
    return ScoreToParValue > 0
        ? FString::Printf(TEXT("+%d"), ScoreToParValue)
        : FString::FromInt(ScoreToParValue);
}
