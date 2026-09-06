#include "DiscGolfRoundFlowWidget.h"

#include "DiscGolfTourPlayerController.h"
#include "Input/Reply.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const FLinearColor ForestInk(0.010f, 0.030f, 0.025f, 0.96f);
    const FLinearColor ForestPanel(0.025f, 0.075f, 0.060f, 0.98f);
    const FLinearColor SignalTeal(0.12f, 0.88f, 0.67f, 1.0f);
    const FLinearColor PaperWhite(0.93f, 0.97f, 0.95f, 1.0f);
    const FLinearColor FogGray(0.58f, 0.68f, 0.64f, 1.0f);
    const FLinearColor TournamentAmber(1.0f, 0.66f, 0.16f, 1.0f);
    const FLinearColor PenaltyCoral(1.0f, 0.38f, 0.30f, 1.0f);

    FString ScoreLabel(int32 ScoreToPar)
    {
        if (ScoreToPar == 0)
        {
            return TEXT("E");
        }
        return ScoreToPar > 0
            ? FString::Printf(TEXT("+%d"), ScoreToPar)
            : FString::FromInt(ScoreToPar);
    }
}

void UDiscGolfRoundFlowWidget::InitializeForController(
    ADiscGolfTourPlayerController* InController)
{
    OwningDiscGolfController = InController;
}

bool UDiscGolfRoundFlowWidget::ApplySnapshot(
    const FDGRoundFlowSnapshot& InSnapshot,
    FString* OutError)
{
    FString Error;
    if (!DiscGolfRoundFlow::ValidateSnapshot(InSnapshot, Error))
    {
        if (OutError)
        {
            *OutError = MoveTemp(Error);
        }
        return false;
    }

    CurrentSnapshot = InSnapshot;
    bHasValidSnapshot = true;
    bActionDispatchedForSnapshot = false;
    SetVisibility(CurrentSnapshot.Screen == EDGRoundFlowScreen::Hidden
        ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    RefreshSlateContent();
    if (OutError)
    {
        OutError->Reset();
    }
    return true;
}

bool UDiscGolfRoundFlowWidget::HasFocusTarget() const
{
    return bHasValidSnapshot
        && CurrentSnapshot.Screen != EDGRoundFlowScreen::Hidden
        && InitialFocusButton.IsValid()
        && InitialFocusButton->IsEnabled()
        && InitialFocusButton->SupportsKeyboardFocus();
}

TSharedPtr<SWidget> UDiscGolfRoundFlowWidget::GetInitialFocusWidget() const
{
    return InitialFocusButton;
}

int32 UDiscGolfRoundFlowWidget::GetActionButtonCount() const
{
    return ActionButtons.Num();
}

bool UDiscGolfRoundFlowWidget::HasActionButton(EDGRoundFlowAction Action) const
{
    const TSharedPtr<SButton>* Button = ActionButtons.Find(Action);
    return Button && Button->IsValid();
}

#if WITH_DEV_AUTOMATION_TESTS
TSharedRef<SWidget> UDiscGolfRoundFlowWidget::BuildNativeStructureForTesting()
{
    return RebuildWidget();
}
#endif

TSharedRef<SWidget> UDiscGolfRoundFlowWidget::RebuildWidget()
{
    using namespace DiscGolfRoundFlow;

    TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 8.0f)
        [
            SAssignNew(TitleText, STextBlock)
            .Text(FText::FromString(GetScreenTitle(CurrentSnapshot.Screen)))
            .ColorAndOpacity(SignalTeal)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 14.0f)
        [
            SAssignNew(CourseText, STextBlock)
            .Text(FText::FromString(CurrentSnapshot.CourseName.ToUpper()))
            .ColorAndOpacity(PaperWhite)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            SAssignNew(SummaryText, STextBlock)
            .ColorAndOpacity(FogGray)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        .Padding(0.0f, 0.0f, 0.0f, 14.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SAssignNew(RowsBox, SVerticalBox)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SAssignNew(ActionsBox, SWrapBox)
            .UseAllottedSize(true)
        ];

    TSharedRef<SWidget> Root =
        SAssignNew(RootBorder, SBorder)
        .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(ForestInk)
        .Padding(FMargin(36.0f))
        [
            SNew(SBox)
            .MinDesiredWidth(720.0f)
            .MaxDesiredWidth(980.0f)
            .MinDesiredHeight(420.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush(TEXT("WhiteBrush")))
                .BorderBackgroundColor(ForestPanel)
                .Padding(FMargin(30.0f))
                [
                    Content
                ]
            ]
        ];

    RefreshSlateContent();
    return Root;
}

void UDiscGolfRoundFlowWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootBorder.Reset();
    TitleText.Reset();
    CourseText.Reset();
    SummaryText.Reset();
    RowsBox.Reset();
    ActionsBox.Reset();
    InitialFocusButton.Reset();
    ActionButtons.Reset();
    RenderedRowCount = 0;
}

void UDiscGolfRoundFlowWidget::RefreshSlateContent()
{
    if (!RootBorder.IsValid())
    {
        return;
    }

    const EDGRoundFlowScreen Screen = bHasValidSnapshot
        ? CurrentSnapshot.Screen : EDGRoundFlowScreen::Hidden;
    if (TitleText.IsValid())
    {
        TitleText->SetText(FText::FromString(DiscGolfRoundFlow::GetScreenTitle(Screen)));
    }
    if (CourseText.IsValid())
    {
        CourseText->SetText(FText::FromString(CurrentSnapshot.CourseName.ToUpper()));
    }
    if (SummaryText.IsValid())
    {
        const FString Summary = CurrentSnapshot.Rows.IsEmpty()
            ? FString()
            : FString::Printf(
                TEXT("%d / %d HOLES    %d STROKES    %d PENALTIES    %s TO PAR"),
                CurrentSnapshot.CompletedHoleCount,
                CurrentSnapshot.Rows.Num(),
                CurrentSnapshot.TotalStrokes,
                CurrentSnapshot.TotalPenaltyStrokes,
                *ScoreLabel(CurrentSnapshot.ScoreToPar));
        SummaryText->SetText(FText::FromString(Summary));
        SummaryText->SetColorAndOpacity(CurrentSnapshot.TotalPenaltyStrokes > 0
            ? PenaltyCoral : FogGray);
    }

    RebuildRows();
    RebuildActions();
}

void UDiscGolfRoundFlowWidget::RebuildRows()
{
    RenderedRowCount = 0;
    if (!RowsBox.IsValid())
    {
        return;
    }
    RowsBox->ClearChildren();
    if (!bHasValidSnapshot || CurrentSnapshot.Rows.IsEmpty())
    {
        return;
    }

    RowsBox->AddSlot()
    .AutoHeight()
    .Padding(4.0f, 3.0f, 4.0f, 7.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("HOLE    NAME    PAR    STROKES    PENALTIES    +/-")))
        .ColorAndOpacity(FogGray)
        .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 11))
    ];

    for (int32 Index = 0; Index < CurrentSnapshot.Rows.Num(); ++Index)
    {
        const FDGRoundFlowRow& Row = CurrentSnapshot.Rows[Index];
        const FString Strokes = Row.bCompleted ? FString::FromInt(Row.Strokes) : TEXT("-");
        const FString Penalties = Row.bCompleted ? FString::FromInt(Row.PenaltyStrokes) : TEXT("-");
        const FString Relative = Row.bCompleted ? ScoreLabel(Row.ScoreToPar) : TEXT("-");
        const FString Prefix = Index == CurrentSnapshot.CurrentHoleIndex ? TEXT("●") : TEXT(" ");
        const FString RowText = FString::Printf(
            TEXT("%s %02d    %s    PAR %d    %s STROKES    %s PEN    %s"),
            *Prefix,
            Row.HoleNumber,
            *Row.HoleName,
            Row.Par,
            *Strokes,
            *Penalties,
            *Relative);
        RowsBox->AddSlot()
        .AutoHeight()
        .Padding(4.0f, 7.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(RowText))
            .ColorAndOpacity(Row.bCompleted ? PaperWhite : FogGray)
            .Font(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 13))
        ];
        ++RenderedRowCount;
    }
}

void UDiscGolfRoundFlowWidget::RebuildActions()
{
    InitialFocusButton.Reset();
    ActionButtons.Reset();
    if (!ActionsBox.IsValid())
    {
        return;
    }
    ActionsBox->ClearChildren();
    if (!bHasValidSnapshot)
    {
        return;
    }

    for (const EDGRoundFlowAction Action : CurrentSnapshot.AllowedActions)
    {
        TSharedPtr<SButton> Button;
        ActionsBox->AddSlot()
        .Padding(FMargin(0.0f, 0.0f, 10.0f, 8.0f))
        [
            SAssignNew(Button, SButton)
            .ContentPadding(FMargin(18.0f, 10.0f))
            .Text(FText::FromString(
                DiscGolfRoundFlow::GetActionLabel(CurrentSnapshot.Screen, Action)))
            .OnClicked_UObject(this, &UDiscGolfRoundFlowWidget::HandleActionClicked, Action)
        ];
        ActionButtons.Add(Action, Button);
        if (Action == CurrentSnapshot.InitialFocusAction)
        {
            InitialFocusButton = Button;
        }
    }
    SetActionButtonsEnabled(!bActionDispatchedForSnapshot);
}

void UDiscGolfRoundFlowWidget::SetActionButtonsEnabled(bool bEnabled)
{
    for (const TPair<EDGRoundFlowAction, TSharedPtr<SButton>>& Pair : ActionButtons)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->SetEnabled(bEnabled);
        }
    }
}

FReply UDiscGolfRoundFlowWidget::HandleActionClicked(EDGRoundFlowAction Action)
{
    ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get();
    if (!Controller || bActionDispatchedForSnapshot
        || !DiscGolfRoundFlow::IsActionAllowed(CurrentSnapshot, Action))
    {
        return FReply::Handled();
    }

    bActionDispatchedForSnapshot = true;
    SetActionButtonsEnabled(false);
    if (!Controller->HandleRoundFlowAction(Action))
    {
        bActionDispatchedForSnapshot = false;
        SetActionButtonsEnabled(true);
    }
    return FReply::Handled();
}
