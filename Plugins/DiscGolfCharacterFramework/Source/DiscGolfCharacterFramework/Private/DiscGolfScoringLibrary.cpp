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
        Total += (Hole.Strokes + Hole.PenaltyStrokes) - Hole.Par;
    }
    return Total;
}

bool UDiscGolfScoringLibrary::IsRoundComplete(const FDGRoundScorecard& Scorecard)
{
    if (Scorecard.Holes.Num() <= 0)
    {
        return false;
    }

    for (const FDGScorecardHole& Hole : Scorecard.Holes)
    {
        if (!Hole.bComplete)
        {
            return false;
        }
    }

    return true;
}
