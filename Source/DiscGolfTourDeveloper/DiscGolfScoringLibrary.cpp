#include "DiscGolfScoringLibrary.h"

int32 UDiscGolfScoringLibrary::CalculateTotalStrokes(const FDGRoundScorecard& Scorecard)
{
    int32 Total = 0;
    for (const FDGScorecardHole& Hole : Scorecard.Holes)
    {
        Total += Hole.Strokes + Hole.PenaltyStrokes;
    }
    return Total;
}

int32 UDiscGolfScoringLibrary::CalculateToPar(const FDGRoundScorecard& Scorecard)
{
    int32 Total = 0;
    for (const FDGScorecardHole& Hole : Scorecard.Holes)
    {
        Total += Hole.Strokes + Hole.PenaltyStrokes - Hole.Par;
    }
    return Total;
}

bool UDiscGolfScoringLibrary::IsRoundComplete(const FDGRoundScorecard& Scorecard)
{
    return !Scorecard.Holes.IsEmpty()
        && !Scorecard.Holes.ContainsByPredicate([](const FDGScorecardHole& Hole)
        {
            return !Hole.bComplete;
        });
}
