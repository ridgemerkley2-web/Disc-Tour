#pragma once

#include "CoreMinimal.h"

/** The mutually exclusive round-flow surfaces owned by presentation code. */
enum class EDGRoundFlowScreen : uint8
{
    Hidden,
    FrontEnd,
    LiveScorecard,
    HoleComplete,
    RoundResults
};

/** Commands that presentation may request from the existing gameplay authorities. */
enum class EDGRoundFlowAction : uint8
{
    None,
    StartOrContinue,
    OpenSettings,
    CloseScorecard,
    AdvanceOrRestart,
    ReturnToMainMenu
};

/** Immutable-by-convention copy of one authoritative round row. */
struct DISCGOLFTOUR_API FDGRoundFlowRow
{
    int32 HoleNumber = 0;
    FString HoleName;
    int32 Par = 0;
    int32 Strokes = 0;
    int32 PenaltyStrokes = 0;
    bool bCompleted = false;
    int32 ScoreToPar = 0;

    bool operator==(const FDGRoundFlowRow& Other) const;
};

/**
 * Read-only input copied from GameMode/round authority before presentation resolves.
 * Totals are carried explicitly so the resolver can fail closed on stale or partial
 * projections instead of silently recomputing a display that disagrees with gameplay.
 */
struct DISCGOLFTOUR_API FDGRoundFlowState
{
    bool bMainMenuVisible = false;
    bool bScorecardVisible = false;
    bool bHoleComplete = false;
    bool bRoundComplete = false;
    FString CourseName;
    int32 CurrentHoleIndex = INDEX_NONE;
    TArray<FDGRoundFlowRow> Rows;
    int32 CompletedHoleCount = 0;
    int32 CompletedPar = 0;
    int32 TotalStrokes = 0;
    int32 TotalPenaltyStrokes = 0;
    int32 ScoreToPar = 0;
};

/** Validated value object consumed by the widget; it owns no gameplay state. */
struct DISCGOLFTOUR_API FDGRoundFlowSnapshot
{
    EDGRoundFlowScreen Screen = EDGRoundFlowScreen::Hidden;
    FString CourseName;
    int32 CurrentHoleIndex = INDEX_NONE;
    TArray<FDGRoundFlowRow> Rows;
    int32 CompletedHoleCount = 0;
    int32 CompletedPar = 0;
    int32 TotalStrokes = 0;
    int32 TotalPenaltyStrokes = 0;
    int32 ScoreToPar = 0;
    TArray<EDGRoundFlowAction> AllowedActions;
    EDGRoundFlowAction InitialFocusAction = EDGRoundFlowAction::None;
};

namespace DiscGolfRoundFlow
{
    /** Validates state, resolves one screen, and assigns OutSnapshot atomically. */
    DISCGOLFTOUR_API bool Resolve(
        const FDGRoundFlowState& State,
        FDGRoundFlowSnapshot& OutSnapshot,
        FString& OutError);

    DISCGOLFTOUR_API bool ValidateState(
        const FDGRoundFlowState& State,
        FString& OutError);

    DISCGOLFTOUR_API bool ValidateSnapshot(
        const FDGRoundFlowSnapshot& Snapshot,
        FString& OutError);

    DISCGOLFTOUR_API bool IsActionAllowed(
        const FDGRoundFlowSnapshot& Snapshot,
        EDGRoundFlowAction Action);

    /** True when a UI-owned command synchronously hands control back to gameplay. */
    DISCGOLFTOUR_API bool TransitionsToGameplay(EDGRoundFlowAction Action);

    /** Returns the exact stable whitelist and initial focus for a known screen. */
    DISCGOLFTOUR_API bool GetActionWhitelist(
        EDGRoundFlowScreen Screen,
        TArray<EDGRoundFlowAction>& OutActions,
        EDGRoundFlowAction& OutInitialFocus,
        FString& OutError);

    DISCGOLFTOUR_API FString GetScreenTitle(EDGRoundFlowScreen Screen);
    DISCGOLFTOUR_API FString GetActionLabel(
        EDGRoundFlowScreen Screen,
        EDGRoundFlowAction Action);
}
