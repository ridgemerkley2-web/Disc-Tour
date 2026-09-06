#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"
#include "../DiscGolfRoundFlowPresentation.h"
#include "../DiscGolfRoundFlowWidget.h"

namespace
{
    FDGRoundFlowRow MakeRow(int32 HoleNumber, const TCHAR* Name, int32 Par)
    {
        FDGRoundFlowRow Row;
        Row.HoleNumber = HoleNumber;
        Row.HoleName = Name;
        Row.Par = Par;
        return Row;
    }

    void CompleteRow(FDGRoundFlowRow& Row, int32 Strokes, int32 Penalties)
    {
        Row.Strokes = Strokes;
        Row.PenaltyStrokes = Penalties;
        Row.bCompleted = true;
        Row.ScoreToPar = Strokes - Row.Par;
    }

    void RefreshTotals(FDGRoundFlowState& State)
    {
        State.CompletedHoleCount = 0;
        State.CompletedPar = 0;
        State.TotalStrokes = 0;
        State.TotalPenaltyStrokes = 0;
        for (const FDGRoundFlowRow& Row : State.Rows)
        {
            if (!Row.bCompleted)
            {
                continue;
            }
            ++State.CompletedHoleCount;
            State.CompletedPar += Row.Par;
            State.TotalStrokes += Row.Strokes;
            State.TotalPenaltyStrokes += Row.PenaltyStrokes;
        }
        State.ScoreToPar = State.TotalStrokes - State.CompletedPar;
    }

    FDGRoundFlowState MakeLiveState()
    {
        FDGRoundFlowState State;
        State.CourseName = TEXT("Pine Ridge");
        State.CurrentHoleIndex = 0;
        State.Rows = {
            MakeRow(1, TEXT("Opening"), 3),
            MakeRow(2, TEXT("Needle Gate"), 4),
            MakeRow(3, TEXT("Gallery Lake"), 4) };
        RefreshTotals(State);
        return State;
    }

    FDGRoundFlowState MakeFrontEndState()
    {
        FDGRoundFlowState State = MakeLiveState();
        State.bMainMenuVisible = true;
        return State;
    }

    FDGRoundFlowState MakeLiveScorecardState()
    {
        FDGRoundFlowState State = MakeLiveState();
        State.bScorecardVisible = true;
        return State;
    }

    FDGRoundFlowState MakeHoleCompleteState()
    {
        FDGRoundFlowState State = MakeLiveState();
        CompleteRow(State.Rows[0], 2, 0);
        State.bScorecardVisible = true;
        State.bHoleComplete = true;
        RefreshTotals(State);
        return State;
    }

    FDGRoundFlowState MakeRoundResultsState()
    {
        FDGRoundFlowState State = MakeLiveState();
        CompleteRow(State.Rows[0], 2, 0);
        CompleteRow(State.Rows[1], 5, 1);
        CompleteRow(State.Rows[2], 3, 0);
        State.CurrentHoleIndex = 2;
        State.bScorecardVisible = true;
        State.bHoleComplete = true;
        State.bRoundComplete = true;
        RefreshTotals(State);
        return State;
    }

    bool ResolveState(
        FAutomationTestBase& Test,
        const FDGRoundFlowState& State,
        FDGRoundFlowSnapshot& OutSnapshot)
    {
        FString Error;
        const bool bResolved = DiscGolfRoundFlow::Resolve(State, OutSnapshot, Error);
        Test.TestTrue(*FString::Printf(TEXT("State resolves: %s"), *Error), bResolved);
        return bResolved;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowFrontEndTest,
    "DiscGolfTour.Session17.RoundFlow.Presentation.FrontEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowFrontEndTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, MakeFrontEndState(), Snapshot)) return false;
    TestTrue(TEXT("Front end screen selected"), Snapshot.Screen == EDGRoundFlowScreen::FrontEnd);
    TestEqual(TEXT("Front end has exactly two actions"), Snapshot.AllowedActions.Num(), 2);
    TestTrue(TEXT("Continue is first and focused"),
        Snapshot.AllowedActions[0] == EDGRoundFlowAction::StartOrContinue
        && Snapshot.InitialFocusAction == EDGRoundFlowAction::StartOrContinue);
    TestTrue(TEXT("Settings is available"),
        DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::OpenSettings));
    TestFalse(TEXT("Scorecard close is not a front-end command"),
        DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::CloseScorecard));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowLiveScorecardTest,
    "DiscGolfTour.Session17.RoundFlow.Presentation.LiveScorecard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowLiveScorecardTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, MakeLiveScorecardState(), Snapshot)) return false;
    TestTrue(TEXT("Live scorecard selected"), Snapshot.Screen == EDGRoundFlowScreen::LiveScorecard);
    TestEqual(TEXT("Live scorecard exposes one action"), Snapshot.AllowedActions.Num(), 1);
    TestTrue(TEXT("Close is the live action"),
        Snapshot.InitialFocusAction == EDGRoundFlowAction::CloseScorecard
        && DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::CloseScorecard));
    TestEqual(TEXT("All three exact rows retained"), Snapshot.Rows.Num(), 3);
    TestEqual(TEXT("No fabricated completed rows"), Snapshot.CompletedHoleCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowHoleCompleteTest,
    "DiscGolfTour.Session17.RoundFlow.Presentation.HoleComplete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowHoleCompleteTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, MakeHoleCompleteState(), Snapshot)) return false;
    TestTrue(TEXT("Hole-complete screen selected"), Snapshot.Screen == EDGRoundFlowScreen::HoleComplete);
    TestEqual(TEXT("Completed-hole count is exact"), Snapshot.CompletedHoleCount, 1);
    TestEqual(TEXT("Completed par is exact"), Snapshot.CompletedPar, 3);
    TestEqual(TEXT("Total strokes are exact"), Snapshot.TotalStrokes, 2);
    TestEqual(TEXT("Relative score is exact"), Snapshot.ScoreToPar, -1);
    TestTrue(TEXT("Advance is focused"),
        Snapshot.InitialFocusAction == EDGRoundFlowAction::AdvanceOrRestart);
    TestTrue(TEXT("Close remains available"),
        DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::CloseScorecard));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowRoundResultsTest,
    "DiscGolfTour.Session17.RoundFlow.Presentation.RoundResults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowRoundResultsTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, MakeRoundResultsState(), Snapshot)) return false;
    TestTrue(TEXT("Round-results screen selected"), Snapshot.Screen == EDGRoundFlowScreen::RoundResults);
    TestEqual(TEXT("All rows completed"), Snapshot.CompletedHoleCount, 3);
    TestEqual(TEXT("Completed par is authoritative"), Snapshot.CompletedPar, 11);
    TestEqual(TEXT("Strokes already include penalties"), Snapshot.TotalStrokes, 10);
    TestEqual(TEXT("Penalty total is separate display evidence"), Snapshot.TotalPenaltyStrokes, 1);
    TestEqual(TEXT("Score does not double-count penalties"), Snapshot.ScoreToPar, -1);
    TestTrue(TEXT("Restart is available"),
        DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::AdvanceOrRestart));
    TestTrue(TEXT("Return to front end is available"),
        DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::ReturnToMainMenu));
    TestFalse(TEXT("Final scorecard cannot close into a locked round"),
        DiscGolfRoundFlow::IsActionAllowed(Snapshot, EDGRoundFlowAction::CloseScorecard));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowContradictionTest,
    "DiscGolfTour.Session17.RoundFlow.Presentation.RejectContradictions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowContradictionTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Existing;
    if (!ResolveState(*this, MakeFrontEndState(), Existing)) return false;

    FDGRoundFlowState Contradictory = MakeFrontEndState();
    Contradictory.bScorecardVisible = true;
    FString Error;
    TestFalse(TEXT("Front end and scorecard are mutually exclusive"),
        DiscGolfRoundFlow::Resolve(Contradictory, Existing, Error));
    TestTrue(TEXT("Failed resolve is atomic"), Existing.Screen == EDGRoundFlowScreen::FrontEnd);

    FDGRoundFlowState StaleTotals = MakeHoleCompleteState();
    ++StaleTotals.TotalStrokes;
    TestFalse(TEXT("Stale totals fail closed"),
        DiscGolfRoundFlow::Resolve(StaleTotals, Existing, Error));

    FDGRoundFlowState InvalidRow = MakeHoleCompleteState();
    ++InvalidRow.Rows[0].ScoreToPar;
    TestFalse(TEXT("Inexact row score fails closed"),
        DiscGolfRoundFlow::Resolve(InvalidRow, Existing, Error));

    FDGRoundFlowState IncompleteRound = MakeRoundResultsState();
    IncompleteRound.bRoundComplete = false;
    TestFalse(TEXT("All rows complete without round completion fails closed"),
        DiscGolfRoundFlow::Resolve(IncompleteRound, Existing, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowActionWhitelistTest,
    "DiscGolfTour.Session17.RoundFlow.ActionWhitelist.Exact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowActionWhitelistTest::RunTest(const FString& Parameters)
{
    TArray<EDGRoundFlowAction> Actions;
    EDGRoundFlowAction Focus = EDGRoundFlowAction::StartOrContinue;
    FString Error;
    TestFalse(TEXT("Invalid screen enum is rejected"),
        DiscGolfRoundFlow::GetActionWhitelist(
            static_cast<EDGRoundFlowScreen>(255), Actions, Focus, Error));
    TestTrue(TEXT("Invalid screen clears actions and focus"),
        Actions.IsEmpty() && Focus == EDGRoundFlowAction::None);

    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, MakeHoleCompleteState(), Snapshot)) return false;
    Snapshot.AllowedActions.Add(EDGRoundFlowAction::OpenSettings);
    TestFalse(TEXT("Extra action invalidates the snapshot"),
        DiscGolfRoundFlow::ValidateSnapshot(Snapshot, Error));
    TestFalse(TEXT("Out-of-range action is never allowed"),
        DiscGolfRoundFlow::IsActionAllowed(
            Snapshot, static_cast<EDGRoundFlowAction>(255)));
    TestTrue(TEXT("Continue hands UI ownership back to gameplay"),
        DiscGolfRoundFlow::TransitionsToGameplay(
            EDGRoundFlowAction::StartOrContinue));
    TestTrue(TEXT("Closing a live scorecard hands ownership back to gameplay"),
        DiscGolfRoundFlow::TransitionsToGameplay(
            EDGRoundFlowAction::CloseScorecard));
    TestTrue(TEXT("Advancing or restarting hands ownership back to gameplay"),
        DiscGolfRoundFlow::TransitionsToGameplay(
            EDGRoundFlowAction::AdvanceOrRestart));
    TestFalse(TEXT("Settings remains UI-owned"),
        DiscGolfRoundFlow::TransitionsToGameplay(
            EDGRoundFlowAction::OpenSettings));
    TestFalse(TEXT("Returning to the main menu remains UI-owned"),
        DiscGolfRoundFlow::TransitionsToGameplay(
            EDGRoundFlowAction::ReturnToMainMenu));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowWidgetFocusTest,
    "DiscGolfTour.Session17.RoundFlow.Widget.FocusAndStructure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowWidgetFocusTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, MakeFrontEndState(), Snapshot)) return false;

    TStrongObjectPtr<UDiscGolfRoundFlowWidget> Widget(
        NewObject<UDiscGolfRoundFlowWidget>());
    Widget->InitializeForController(nullptr);
    FString Error;
    TestTrue(TEXT("Widget accepts validated front-end snapshot"),
        Widget->ApplySnapshot(Snapshot, &Error));
    const TSharedRef<SWidget> Root = Widget->BuildNativeStructureForTesting();
    TestTrue(TEXT("Native root was constructed"), Root->GetType() != NAME_None);
    TestTrue(TEXT("Initial target exists"), Widget->HasFocusTarget());
    TestTrue(TEXT("Initial target supports keyboard focus"),
        Widget->GetInitialFocusWidget().IsValid()
        && Widget->GetInitialFocusWidget()->SupportsKeyboardFocus());
    TestEqual(TEXT("Two front-end buttons rendered"), Widget->GetActionButtonCount(), 2);
    TestEqual(TEXT("Three authoritative rows rendered"), Widget->GetRenderedRowCount(), 3);
    TestTrue(TEXT("Continue button rendered"),
        Widget->HasActionButton(EDGRoundFlowAction::StartOrContinue));
    TestTrue(TEXT("Settings button rendered"),
        Widget->HasActionButton(EDGRoundFlowAction::OpenSettings));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowWidgetAtomicTest,
    "DiscGolfTour.Session17.RoundFlow.Widget.ApplySnapshotAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowWidgetAtomicTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowSnapshot Valid;
    if (!ResolveState(*this, MakeRoundResultsState(), Valid)) return false;
    TStrongObjectPtr<UDiscGolfRoundFlowWidget> Widget(
        NewObject<UDiscGolfRoundFlowWidget>());
    Widget->InitializeForController(nullptr);
    const TSharedRef<SWidget> Root = Widget->BuildNativeStructureForTesting();
    TestTrue(TEXT("Native results structure was constructed"), Root->GetType() != NAME_None);
    FString Error;
    TestTrue(TEXT("Valid results snapshot applies"), Widget->ApplySnapshot(Valid, &Error));
    TestEqual(TEXT("Results actions rendered"), Widget->GetActionButtonCount(), 2);

    FDGRoundFlowSnapshot Invalid = Valid;
    Invalid.TotalStrokes += 1;
    TestFalse(TEXT("Invalid replacement is rejected"), Widget->ApplySnapshot(Invalid, &Error));
    TestTrue(TEXT("Prior screen remains applied"),
        Widget->GetAppliedSnapshot().Screen == EDGRoundFlowScreen::RoundResults);
    TestEqual(TEXT("Prior exact total remains applied"),
        Widget->GetAppliedSnapshot().TotalStrokes, 10);
    TestEqual(TEXT("Prior button structure remains"), Widget->GetActionButtonCount(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDGRoundFlowWidgetHiddenTest,
    "DiscGolfTour.Session17.RoundFlow.Widget.HiddenStructure",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDGRoundFlowWidgetHiddenTest::RunTest(const FString& Parameters)
{
    FDGRoundFlowState State;
    FDGRoundFlowSnapshot Snapshot;
    if (!ResolveState(*this, State, Snapshot)) return false;
    TestTrue(TEXT("Empty state resolves hidden"), Snapshot.Screen == EDGRoundFlowScreen::Hidden);

    TStrongObjectPtr<UDiscGolfRoundFlowWidget> Widget(
        NewObject<UDiscGolfRoundFlowWidget>());
    FString Error;
    TestTrue(TEXT("Hidden snapshot applies"), Widget->ApplySnapshot(Snapshot, &Error));
    const TSharedRef<SWidget> Root = Widget->BuildNativeStructureForTesting();
    TestTrue(TEXT("Native hidden structure was constructed"), Root->GetType() != NAME_None);
    TestTrue(TEXT("Hidden widget is collapsed"),
        Widget->GetVisibility() == ESlateVisibility::Collapsed);
    TestFalse(TEXT("Hidden widget has no focus target"), Widget->HasFocusTarget());
    TestEqual(TEXT("Hidden widget has no buttons"), Widget->GetActionButtonCount(), 0);
    TestEqual(TEXT("Hidden widget has no rows"), Widget->GetRenderedRowCount(), 0);
    return true;
}

#endif
