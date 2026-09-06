#include "DiscGolfCompetitionRuntime.h"

#include "DiscGolfRoundState.h"
#include "DiscGolfScoringLibrary.h"

namespace
{
const FName Session14EventId(TEXT("PineRidgeChampionship"));
const FName Session14CourseId(TEXT("PineRidgeChampionship"));
const FName Session14TeeSetId(TEXT("Championship"));
const FName GenericBrandId(TEXT("dg_generic"));

bool IsStableId(const FName Id)
{
    if (Id.IsNone())
    {
        return false;
    }

    const FString Text = Id.ToString();
    if (Text.IsEmpty() || Text.Len() > 64)
    {
        return false;
    }
    for (const TCHAR Character : Text)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
        {
            return false;
        }
    }
    return true;
}
}

FDiscGolfCompetitionRuntimeDefinition DiscGolfCompetitionRuntime::BuildSourceFallbackEvent()
{
    FDiscGolfCompetitionRuntimeDefinition Event;
    Event.SchemaVersion = CurrentSchemaVersion;
    Event.EventId = Session14EventId;
    Event.DisplayName = FText::FromString(TEXT("Pine Ridge Championship"));
    Event.PresentingBrandId = GenericBrandId;
    Event.Format = EDGCompetitionFormat::StrokePlay;

    FDGTournamentRoundDefinition& Round = Event.Rounds.AddDefaulted_GetRef();
    Round.RoundNumber = 1;
    Round.CourseId = Session14CourseId;
    Round.TeeSetId = Session14TeeSetId;
    Round.HoleCount = Session14HoleCount;

    Event.HolePars = { 3, 4, 4 };
    return Event;
}

bool DiscGolfCompetitionRuntime::ValidateEvent(
    const FDiscGolfCompetitionRuntimeDefinition& Event,
    FString& OutError)
{
    if (Event.SchemaVersion != CurrentSchemaVersion)
    {
        OutError = TEXT("competition event is not schema v1");
        return false;
    }
    if (!IsStableId(Event.EventId) || Event.EventId != Session14EventId
        || Event.DisplayName.IsEmpty() || Event.DisplayName.ToString().Len() > 64)
    {
        OutError = TEXT("competition event identity is not the bounded original fallback");
        return false;
    }
    if (Event.PresentingBrandId != GenericBrandId)
    {
        OutError = TEXT("competition event brand must remain dg_generic");
        return false;
    }
    if (Event.Format != EDGCompetitionFormat::StrokePlay || Event.Rounds.Num() != 1)
    {
        OutError = TEXT("competition event must contain exactly one StrokePlay round");
        return false;
    }

    const FDGTournamentRoundDefinition& Round = Event.Rounds[0];
    if (Round.RoundNumber != 1 || Round.CourseId != Session14CourseId
        || Round.TeeSetId != Session14TeeSetId || Round.HoleCount != Session14HoleCount)
    {
        OutError = TEXT("competition round must be Pine Ridge Championship, Championship tees, three holes");
        return false;
    }
    if (Event.HolePars.Num() != Session14HoleCount)
    {
        OutError = TEXT("competition event must publish exactly three hole pars");
        return false;
    }
    for (int32 Index = 0; Index < Event.HolePars.Num(); ++Index)
    {
        if (Event.HolePars[Index] < 1 || Event.HolePars[Index] > 9)
        {
            OutError = FString::Printf(TEXT("competition hole %d par is invalid"), Index + 1);
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool DiscGolfCompetitionRuntime::ValidateCompletedScorecard(
    const FDiscGolfCompetitionRuntimeDefinition& Event,
    const FDGRoundScorecard& Scorecard,
    FString& OutError)
{
    if (!ValidateEvent(Event, OutError))
    {
        return false;
    }

    const FDGTournamentRoundDefinition& EventRound = Event.Rounds[0];
    if (Scorecard.EventId != Event.EventId || Scorecard.CourseId != EventRound.CourseId
        || Scorecard.RoundNumber != EventRound.RoundNumber
        || Scorecard.Holes.Num() != EventRound.HoleCount)
    {
        OutError = TEXT("scorecard identity or hole count does not match the event");
        return false;
    }

    for (int32 Index = 0; Index < Scorecard.Holes.Num(); ++Index)
    {
        const FDGScorecardHole& Hole = Scorecard.Holes[Index];
        if (Hole.HoleNumber != Index + 1 || Hole.Par != Event.HolePars[Index]
            || Hole.Strokes < 0 || Hole.Strokes > 99
            || Hole.PenaltyStrokes < 0 || Hole.PenaltyStrokes > 99
            || Hole.Strokes + Hole.PenaltyStrokes <= 0
            || Hole.Strokes + Hole.PenaltyStrokes > 99 || !Hole.bComplete)
        {
            OutError = FString::Printf(TEXT("scorecard hole %d is invalid or incomplete"), Index + 1);
            return false;
        }
    }
    if (!UDiscGolfScoringLibrary::IsRoundComplete(Scorecard))
    {
        OutError = TEXT("scorecard is not complete");
        return false;
    }

    OutError.Reset();
    return true;
}

bool DiscGolfCompetitionRuntime::BuildCompletedScorecard(
    const FDiscGolfCompetitionRuntimeDefinition& Event,
    const FDiscGolfRoundState& Round,
    FDGRoundScorecard& OutScorecard,
    FString& OutError)
{
    if (!ValidateEvent(Event, OutError))
    {
        return false;
    }

    const FDGTournamentRoundDefinition& EventRound = Event.Rounds[0];
    if (!Round.bRoundComplete || Round.CourseId != EventRound.CourseId
        || Round.LayoutId != EventRound.TeeSetId
        || Round.HoleScores.Num() != EventRound.HoleCount
        || DiscGolfRound::CompletedHoleCount(Round) != EventRound.HoleCount)
    {
        OutError = TEXT("project round is incomplete or does not match the event");
        return false;
    }

    FDGRoundScorecard Candidate;
    Candidate.EventId = Event.EventId;
    Candidate.CourseId = EventRound.CourseId;
    Candidate.RoundNumber = EventRound.RoundNumber;
    for (int32 Index = 0; Index < Round.HoleScores.Num(); ++Index)
    {
        const FDiscGolfRoundHoleScore& Source = Round.HoleScores[Index];
        if (!Source.bCompleted || Source.HoleNumber != Index + 1
            || Source.Par != Event.HolePars[Index] || Source.Strokes <= 0
            || Source.Strokes > 99 || Source.PenaltyStrokes < 0
            || Source.PenaltyStrokes > Source.Strokes)
        {
            OutError = FString::Printf(TEXT("project round hole %d is invalid"), Index + 1);
            return false;
        }

        FDGScorecardHole& Target = Candidate.Holes.AddDefaulted_GetRef();
        Target.HoleNumber = Source.HoleNumber;
        Target.Par = Source.Par;
        // Project Strokes already contains penalties. The framework scorer adds these fields.
        Target.Strokes = Source.Strokes - Source.PenaltyStrokes;
        Target.PenaltyStrokes = Source.PenaltyStrokes;
        Target.bComplete = true;
    }

    if (!ValidateCompletedScorecard(Event, Candidate, OutError))
    {
        return false;
    }
    if (UDiscGolfScoringLibrary::CalculateTotalStrokes(Candidate)
            != DiscGolfRound::TotalStrokes(Round)
        || UDiscGolfScoringLibrary::CalculateToPar(Candidate)
            != DiscGolfRound::ScoreToPar(Round))
    {
        OutError = TEXT("framework score totals do not exactly match project round authority");
        return false;
    }

    OutScorecard = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

FName DiscGolfCompetitionRuntime::MakeResultId(const FDGRoundScorecard& Scorecard)
{
    if (!IsStableId(Scorecard.EventId) || !IsStableId(Scorecard.CourseId)
        || Scorecard.RoundNumber <= 0)
    {
        return NAME_None;
    }
    return FName(*FString::Printf(
        TEXT("%s_%s_Round%d"),
        *Scorecard.EventId.ToString(),
        *Scorecard.CourseId.ToString(),
        Scorecard.RoundNumber));
}
