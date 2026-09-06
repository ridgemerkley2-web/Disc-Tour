#include "DiscGolfRoundFlowPresentation.h"

namespace
{
    constexpr int32 MaxPresentedHoles = 36;
    constexpr int32 MaxPresentedPar = 10;
    constexpr int32 MaxPresentedStrokes = 100;

    bool IsKnownScreen(EDGRoundFlowScreen Screen)
    {
        switch (Screen)
        {
            case EDGRoundFlowScreen::Hidden:
            case EDGRoundFlowScreen::FrontEnd:
            case EDGRoundFlowScreen::LiveScorecard:
            case EDGRoundFlowScreen::HoleComplete:
            case EDGRoundFlowScreen::RoundResults:
                return true;
            default:
                return false;
        }
    }

    bool IsKnownAction(EDGRoundFlowAction Action)
    {
        switch (Action)
        {
            case EDGRoundFlowAction::None:
            case EDGRoundFlowAction::StartOrContinue:
            case EDGRoundFlowAction::OpenSettings:
            case EDGRoundFlowAction::CloseScorecard:
            case EDGRoundFlowAction::AdvanceOrRestart:
            case EDGRoundFlowAction::ReturnToMainMenu:
                return true;
            default:
                return false;
        }
    }

    bool ValidateRoundProjection(
        const FString& CourseName,
        int32 CurrentHoleIndex,
        const TArray<FDGRoundFlowRow>& Rows,
        int32 ExpectedCompletedHoleCount,
        int32 ExpectedCompletedPar,
        int32 ExpectedTotalStrokes,
        int32 ExpectedPenaltyStrokes,
        int32 ExpectedScoreToPar,
        FString& OutError)
    {
        if (Rows.Num() > MaxPresentedHoles)
        {
            OutError = FString::Printf(
                TEXT("round-flow row count %d exceeds the bounded maximum %d"),
                Rows.Num(), MaxPresentedHoles);
            return false;
        }
        if (Rows.IsEmpty())
        {
            if (CurrentHoleIndex != INDEX_NONE || !CourseName.IsEmpty()
                || ExpectedCompletedHoleCount != 0 || ExpectedCompletedPar != 0
                || ExpectedTotalStrokes != 0 || ExpectedPenaltyStrokes != 0
                || ExpectedScoreToPar != 0)
            {
                OutError = TEXT("empty round-flow rows carried course, index, or total evidence");
                return false;
            }
            OutError.Reset();
            return true;
        }
        if (CourseName.TrimStartAndEnd().IsEmpty())
        {
            OutError = TEXT("round-flow rows require a non-empty course name");
            return false;
        }
        if (!Rows.IsValidIndex(CurrentHoleIndex))
        {
            OutError = TEXT("round-flow current-hole index is outside the row set");
            return false;
        }

        TSet<int32> HoleNumbers;
        int32 CompletedHoleCount = 0;
        int32 CompletedPar = 0;
        int32 TotalStrokes = 0;
        int32 TotalPenaltyStrokes = 0;
        for (const FDGRoundFlowRow& Row : Rows)
        {
            if (Row.HoleNumber <= 0 || HoleNumbers.Contains(Row.HoleNumber))
            {
                OutError = TEXT("round-flow hole numbers must be positive and unique");
                return false;
            }
            HoleNumbers.Add(Row.HoleNumber);
            if (Row.HoleName.TrimStartAndEnd().IsEmpty())
            {
                OutError = TEXT("round-flow rows require non-empty hole names");
                return false;
            }
            if (Row.Par <= 0 || Row.Par > MaxPresentedPar)
            {
                OutError = TEXT("round-flow row par is outside the supported range");
                return false;
            }
            if (Row.bCompleted)
            {
                if (Row.Strokes <= 0 || Row.Strokes > MaxPresentedStrokes
                    || Row.PenaltyStrokes < 0 || Row.PenaltyStrokes > Row.Strokes)
                {
                    OutError = TEXT("completed round-flow row has invalid stroke evidence");
                    return false;
                }
                if (Row.ScoreToPar != Row.Strokes - Row.Par)
                {
                    OutError = TEXT("completed round-flow row score-to-par is not exact");
                    return false;
                }
                ++CompletedHoleCount;
                CompletedPar += Row.Par;
                TotalStrokes += Row.Strokes;
                TotalPenaltyStrokes += Row.PenaltyStrokes;
            }
            else if (Row.Strokes != 0 || Row.PenaltyStrokes != 0 || Row.ScoreToPar != 0)
            {
                OutError = TEXT("incomplete round-flow row carried score evidence");
                return false;
            }
        }

        const int32 DerivedScoreToPar = TotalStrokes - CompletedPar;
        if (CompletedHoleCount != ExpectedCompletedHoleCount
            || CompletedPar != ExpectedCompletedPar
            || TotalStrokes != ExpectedTotalStrokes
            || TotalPenaltyStrokes != ExpectedPenaltyStrokes
            || DerivedScoreToPar != ExpectedScoreToPar)
        {
            OutError = FString::Printf(
                TEXT("round-flow totals disagree with rows (holes=%d/%d par=%d/%d strokes=%d/%d penalties=%d/%d score=%d/%d)"),
                ExpectedCompletedHoleCount, CompletedHoleCount,
                ExpectedCompletedPar, CompletedPar,
                ExpectedTotalStrokes, TotalStrokes,
                ExpectedPenaltyStrokes, TotalPenaltyStrokes,
                ExpectedScoreToPar, DerivedScoreToPar);
            return false;
        }

        OutError.Reset();
        return true;
    }

    bool ActionsMatchExactly(
        const TArray<EDGRoundFlowAction>& Actual,
        const TArray<EDGRoundFlowAction>& Expected)
    {
        if (Actual.Num() != Expected.Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < Actual.Num(); ++Index)
        {
            if (Actual[Index] != Expected[Index])
            {
                return false;
            }
        }
        return true;
    }
}

bool FDGRoundFlowRow::operator==(const FDGRoundFlowRow& Other) const
{
    return HoleNumber == Other.HoleNumber
        && HoleName == Other.HoleName
        && Par == Other.Par
        && Strokes == Other.Strokes
        && PenaltyStrokes == Other.PenaltyStrokes
        && bCompleted == Other.bCompleted
        && ScoreToPar == Other.ScoreToPar;
}

bool DiscGolfRoundFlow::GetActionWhitelist(
    EDGRoundFlowScreen Screen,
    TArray<EDGRoundFlowAction>& OutActions,
    EDGRoundFlowAction& OutInitialFocus,
    FString& OutError)
{
    OutActions.Reset();
    OutInitialFocus = EDGRoundFlowAction::None;
    switch (Screen)
    {
        case EDGRoundFlowScreen::Hidden:
            break;
        case EDGRoundFlowScreen::FrontEnd:
            OutActions = {
                EDGRoundFlowAction::StartOrContinue,
                EDGRoundFlowAction::OpenSettings };
            OutInitialFocus = EDGRoundFlowAction::StartOrContinue;
            break;
        case EDGRoundFlowScreen::LiveScorecard:
            OutActions = { EDGRoundFlowAction::CloseScorecard };
            OutInitialFocus = EDGRoundFlowAction::CloseScorecard;
            break;
        case EDGRoundFlowScreen::HoleComplete:
            OutActions = {
                EDGRoundFlowAction::CloseScorecard,
                EDGRoundFlowAction::AdvanceOrRestart };
            OutInitialFocus = EDGRoundFlowAction::AdvanceOrRestart;
            break;
        case EDGRoundFlowScreen::RoundResults:
            OutActions = {
                EDGRoundFlowAction::AdvanceOrRestart,
                EDGRoundFlowAction::ReturnToMainMenu };
            OutInitialFocus = EDGRoundFlowAction::AdvanceOrRestart;
            break;
        default:
            OutError = TEXT("round-flow screen enum is outside the supported range");
            return false;
    }

    OutError.Reset();
    return true;
}

bool DiscGolfRoundFlow::ValidateState(
    const FDGRoundFlowState& State,
    FString& OutError)
{
    if (State.bMainMenuVisible && State.bScorecardVisible)
    {
        OutError = TEXT("front end and scorecard cannot be visible together");
        return false;
    }
    if (State.bRoundComplete && !State.bHoleComplete)
    {
        OutError = TEXT("round completion requires current-hole completion");
        return false;
    }
    if (!ValidateRoundProjection(
        State.CourseName,
        State.CurrentHoleIndex,
        State.Rows,
        State.CompletedHoleCount,
        State.CompletedPar,
        State.TotalStrokes,
        State.TotalPenaltyStrokes,
        State.ScoreToPar,
        OutError))
    {
        return false;
    }
    if ((State.bMainMenuVisible || State.bScorecardVisible) && State.Rows.IsEmpty())
    {
        OutError = TEXT("visible round-flow surface requires authored round rows");
        return false;
    }
    if (!State.Rows.IsEmpty())
    {
        const bool bCurrentRowComplete = State.Rows[State.CurrentHoleIndex].bCompleted;
        if (bCurrentRowComplete != State.bHoleComplete)
        {
            OutError = TEXT("current row completion contradicts the hole-complete state");
            return false;
        }
        if (State.bRoundComplete && State.CompletedHoleCount != State.Rows.Num())
        {
            OutError = TEXT("round-complete state requires every presented row to be complete");
            return false;
        }
        if (!State.bRoundComplete && State.CompletedHoleCount == State.Rows.Num())
        {
            OutError = TEXT("all rows are complete while round-complete state is false");
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool DiscGolfRoundFlow::ValidateSnapshot(
    const FDGRoundFlowSnapshot& Snapshot,
    FString& OutError)
{
    if (!IsKnownScreen(Snapshot.Screen))
    {
        OutError = TEXT("round-flow snapshot screen is outside the supported range");
        return false;
    }
    if (!ValidateRoundProjection(
        Snapshot.CourseName,
        Snapshot.CurrentHoleIndex,
        Snapshot.Rows,
        Snapshot.CompletedHoleCount,
        Snapshot.CompletedPar,
        Snapshot.TotalStrokes,
        Snapshot.TotalPenaltyStrokes,
        Snapshot.ScoreToPar,
        OutError))
    {
        return false;
    }
    if (Snapshot.Screen != EDGRoundFlowScreen::Hidden && Snapshot.Rows.IsEmpty())
    {
        OutError = TEXT("visible round-flow snapshot requires authored round rows");
        return false;
    }

    TArray<EDGRoundFlowAction> ExpectedActions;
    EDGRoundFlowAction ExpectedFocus = EDGRoundFlowAction::None;
    if (!GetActionWhitelist(Snapshot.Screen, ExpectedActions, ExpectedFocus, OutError))
    {
        return false;
    }
    for (const EDGRoundFlowAction Action : Snapshot.AllowedActions)
    {
        if (!IsKnownAction(Action) || Action == EDGRoundFlowAction::None)
        {
            OutError = TEXT("round-flow snapshot contains an invalid action");
            return false;
        }
    }
    if (!ActionsMatchExactly(Snapshot.AllowedActions, ExpectedActions)
        || Snapshot.InitialFocusAction != ExpectedFocus)
    {
        OutError = TEXT("round-flow snapshot action whitelist or initial focus drifted");
        return false;
    }

    if (!Snapshot.Rows.IsEmpty())
    {
        const bool bCurrentRowComplete = Snapshot.Rows[Snapshot.CurrentHoleIndex].bCompleted;
        if (Snapshot.Screen == EDGRoundFlowScreen::LiveScorecard && bCurrentRowComplete)
        {
            OutError = TEXT("live scorecard cannot present a completed current hole");
            return false;
        }
        if (Snapshot.Screen == EDGRoundFlowScreen::HoleComplete
            && (!bCurrentRowComplete || Snapshot.CompletedHoleCount == Snapshot.Rows.Num()))
        {
            OutError = TEXT("hole-complete screen requires a completed current hole and unfinished round");
            return false;
        }
        if (Snapshot.Screen == EDGRoundFlowScreen::RoundResults
            && Snapshot.CompletedHoleCount != Snapshot.Rows.Num())
        {
            OutError = TEXT("round-results screen requires every row to be complete");
            return false;
        }
    }

    OutError.Reset();
    return true;
}

bool DiscGolfRoundFlow::Resolve(
    const FDGRoundFlowState& State,
    FDGRoundFlowSnapshot& OutSnapshot,
    FString& OutError)
{
    if (!ValidateState(State, OutError))
    {
        return false;
    }

    FDGRoundFlowSnapshot Candidate;
    Candidate.Screen = State.bMainMenuVisible
        ? EDGRoundFlowScreen::FrontEnd
        : State.bScorecardVisible
            ? State.bRoundComplete
                ? EDGRoundFlowScreen::RoundResults
                : State.bHoleComplete
                    ? EDGRoundFlowScreen::HoleComplete
                    : EDGRoundFlowScreen::LiveScorecard
            : EDGRoundFlowScreen::Hidden;
    Candidate.CourseName = State.CourseName;
    Candidate.CurrentHoleIndex = State.CurrentHoleIndex;
    Candidate.Rows = State.Rows;
    Candidate.CompletedHoleCount = State.CompletedHoleCount;
    Candidate.CompletedPar = State.CompletedPar;
    Candidate.TotalStrokes = State.TotalStrokes;
    Candidate.TotalPenaltyStrokes = State.TotalPenaltyStrokes;
    Candidate.ScoreToPar = State.ScoreToPar;
    if (!GetActionWhitelist(
        Candidate.Screen,
        Candidate.AllowedActions,
        Candidate.InitialFocusAction,
        OutError)
        || !ValidateSnapshot(Candidate, OutError))
    {
        return false;
    }

    OutSnapshot = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool DiscGolfRoundFlow::IsActionAllowed(
    const FDGRoundFlowSnapshot& Snapshot,
    EDGRoundFlowAction Action)
{
    if (!IsKnownAction(Action) || Action == EDGRoundFlowAction::None)
    {
        return false;
    }
    FString Error;
    return ValidateSnapshot(Snapshot, Error)
        && Snapshot.AllowedActions.Contains(Action);
}

bool DiscGolfRoundFlow::TransitionsToGameplay(EDGRoundFlowAction Action)
{
    return Action == EDGRoundFlowAction::StartOrContinue
        || Action == EDGRoundFlowAction::CloseScorecard
        || Action == EDGRoundFlowAction::AdvanceOrRestart;
}

FString DiscGolfRoundFlow::GetScreenTitle(EDGRoundFlowScreen Screen)
{
    switch (Screen)
    {
        case EDGRoundFlowScreen::Hidden: return TEXT("");
        case EDGRoundFlowScreen::FrontEnd: return TEXT("DISC GOLF TOUR");
        case EDGRoundFlowScreen::LiveScorecard: return TEXT("TOUR SCORECARD");
        case EDGRoundFlowScreen::HoleComplete: return TEXT("HOLE COMPLETE");
        case EDGRoundFlowScreen::RoundResults: return TEXT("FINAL SCORECARD");
        default: return TEXT("");
    }
}

FString DiscGolfRoundFlow::GetActionLabel(
    EDGRoundFlowScreen Screen,
    EDGRoundFlowAction Action)
{
    switch (Action)
    {
        case EDGRoundFlowAction::StartOrContinue:
            return TEXT("START / CONTINUE ROUND");
        case EDGRoundFlowAction::OpenSettings:
            return TEXT("SETTINGS & CONTROLS");
        case EDGRoundFlowAction::CloseScorecard:
            return TEXT("CLOSE SCORECARD");
        case EDGRoundFlowAction::AdvanceOrRestart:
            return Screen == EDGRoundFlowScreen::RoundResults
                ? TEXT("RESTART ROUND") : TEXT("NEXT HOLE");
        case EDGRoundFlowAction::ReturnToMainMenu:
            return TEXT("RETURN TO MAIN MENU");
        case EDGRoundFlowAction::None:
        default:
            return TEXT("");
    }
}
