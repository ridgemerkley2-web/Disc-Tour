#include "DiscGolfHUD.h"
#include "DiscActor.h"
#include "DiscBagComponent.h"
#include "DiscCatalogSubsystem.h"
#include "DiscFlightComponent.h"
#include "DiscGolfHoleActor.h"
#include "DiscGolfHudPresentationState.h"
#include "DiscGolfMath.h"
#include "DiscGolfCourseRules.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolferPawn.h"
#include "ThrowControllerComponent.h"
#include "WindDirector.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

namespace DiscGolfHudStyle
{
    constexpr float ReferenceWidth = 1280.0f;
    constexpr float ReferenceHeight = 720.0f;
    constexpr float SafeMargin = 24.0f;

    // "Forest Broadcast" is an original palette: course-derived greens and warm paper
    // neutrals carry the broadcast layer, while amber is reserved for competitive moments.
    const FLinearColor Scrim(0.008f, 0.016f, 0.014f, 0.96f);
    const FLinearColor Panel(0.020f, 0.045f, 0.040f, 0.94f);
    const FLinearColor PanelRaised(0.030f, 0.072f, 0.062f, 0.96f);
    const FLinearColor PanelMuted(0.018f, 0.035f, 0.032f, 0.90f);
    const FLinearColor Divider(0.16f, 0.30f, 0.26f, 0.88f);
    const FLinearColor SignalTeal(0.18f, 0.78f, 0.64f, 1.0f);
    const FLinearColor MistTeal(0.46f, 0.75f, 0.66f, 1.0f);
    const FLinearColor TournamentAmber(0.96f, 0.67f, 0.20f, 1.0f);
    const FLinearColor SuccessMint(0.38f, 0.88f, 0.56f, 1.0f);
    const FLinearColor PenaltyCoral(0.96f, 0.34f, 0.29f, 1.0f);
    const FLinearColor PaperWhite(0.96f, 0.95f, 0.89f, 1.0f);
    const FLinearColor FogGray(0.69f, 0.77f, 0.71f, 1.0f);
    const FLinearColor SlateGray(0.43f, 0.54f, 0.49f, 1.0f);

    struct FViewportLayout
    {
        float Scale = 1.0f;
        float OriginX = 0.0f;
        float OriginY = 0.0f;

        explicit FViewportLayout(const UCanvas* Canvas)
        {
            if (!Canvas)
            {
                return;
            }

            Scale = FMath::Max(0.01f, FMath::Min(Canvas->ClipX / ReferenceWidth, Canvas->ClipY / ReferenceHeight));
            OriginX = (Canvas->ClipX - ReferenceWidth * Scale) * 0.5f;
            OriginY = (Canvas->ClipY - ReferenceHeight * Scale) * 0.5f;
        }

        float X(float LogicalX) const { return OriginX + LogicalX * Scale; }
        float Y(float LogicalY) const { return OriginY + LogicalY * Scale; }
        float U(float LogicalUnits) const { return LogicalUnits * Scale; }
    };

    bool FitsLogicalWidth(UCanvas* Canvas, UFont* Font, const FString& Value, float FontScale, float MaxLogicalWidth)
    {
        if (!Canvas || !Font || Value.IsEmpty()) return true;
        float TextWidth = 0.0f;
        float TextHeight = 0.0f;
        Canvas->StrLen(Font, Value, TextWidth, TextHeight);
        return TextWidth * FontScale <= MaxLogicalWidth;
    }

    FString EllipsizeToLogicalWidth(UCanvas* Canvas, UFont* Font, const FString& Value, float FontScale, float MaxLogicalWidth)
    {
        if (FitsLogicalWidth(Canvas, Font, Value, FontScale, MaxLogicalWidth)) return Value;

        const FString Suffix = TEXT("...");
        int32 Low = 0;
        int32 High = Value.Len();
        while (Low < High)
        {
            const int32 Mid = (Low + High + 1) / 2;
            const FString Candidate = Value.Left(Mid).TrimEnd() + Suffix;
            if (FitsLogicalWidth(Canvas, Font, Candidate, FontScale, MaxLogicalWidth))
            {
                Low = Mid;
            }
            else
            {
                High = Mid - 1;
            }
        }
        return Value.Left(Low).TrimEnd() + Suffix;
    }

    void WrapToTwoLogicalLines(
        UCanvas* Canvas,
        UFont* Font,
        const FString& Value,
        float FontScale,
        float MaxLogicalWidth,
        FString& OutFirstLine,
        FString& OutSecondLine)
    {
        OutFirstLine = Value;
        OutSecondLine.Reset();
        if (FitsLogicalWidth(Canvas, Font, Value, FontScale, MaxLogicalWidth)) return;

        TArray<FString> Words;
        Value.ParseIntoArrayWS(Words);
        OutFirstLine.Reset();
        int32 NextWordIndex = 0;
        for (; NextWordIndex < Words.Num(); ++NextWordIndex)
        {
            const FString Candidate = OutFirstLine.IsEmpty()
                ? Words[NextWordIndex]
                : OutFirstLine + TEXT(" ") + Words[NextWordIndex];
            if (!OutFirstLine.IsEmpty() && !FitsLogicalWidth(Canvas, Font, Candidate, FontScale, MaxLogicalWidth))
            {
                break;
            }
            OutFirstLine = Candidate;
        }

        FString Remaining;
        for (; NextWordIndex < Words.Num(); ++NextWordIndex)
        {
            if (!Remaining.IsEmpty()) Remaining += TEXT(" ");
            Remaining += Words[NextWordIndex];
        }
        OutFirstLine = EllipsizeToLogicalWidth(Canvas, Font, OutFirstLine, FontScale, MaxLogicalWidth);
        OutSecondLine = EllipsizeToLogicalWidth(Canvas, Font, Remaining, FontScale, MaxLogicalWidth);
    }
}

FString ADiscGolfHUD::PlasticToString(uint8 PlasticValue) const
{
    switch (static_cast<EDiscPlastic>(PlasticValue))
    {
        case EDiscPlastic::Base: return TEXT("Base");
        case EDiscPlastic::Crystal: return TEXT("Crystal");
        case EDiscPlastic::Tour:
        default: return TEXT("Tour");
    }
}

FString ADiscGolfHUD::ThrowStyleToString(uint8 ThrowStyleValue) const
{
    return static_cast<EThrowStyle>(ThrowStyleValue) == EThrowStyle::Forehand ? TEXT("RHFH") : TEXT("RHBH");
}

FString ADiscGolfHUD::LieToString(uint8 LieValue) const
{
    switch (static_cast<ELieType>(LieValue))
    {
        case ELieType::Tee: return TEXT("Tee");
        case ELieType::Circle1: return TEXT("Circle 1");
        case ELieType::Circle2: return TEXT("Circle 2");
        case ELieType::LightRough: return TEXT("Light Rough");
        case ELieType::DeepRough: return TEXT("Deep Rough");
        case ELieType::Hazard: return TEXT("Hazard");
        case ELieType::Fairway:
        default: return TEXT("Fairway");
    }
}

FString ADiscGolfHUD::ReleaseGradeToString(EReleaseGrade Grade) const
{
    switch (Grade)
    {
        case EReleaseGrade::Perfect: return TEXT("PERFECT");
        case EReleaseGrade::Great: return TEXT("GREAT");
        case EReleaseGrade::Good: return TEXT("GOOD");
        case EReleaseGrade::Poor:
        default: return TEXT("POOR");
    }
}

FString ADiscGolfHUD::ReleaseTimingToString(EReleaseTiming Timing) const
{
    switch (Timing)
    {
        case EReleaseTiming::Early: return TEXT("EARLY");
        case EReleaseTiming::Late: return TEXT("LATE");
        case EReleaseTiming::OnTime:
        default: return TEXT("ON TIME");
    }
}

FLinearColor ADiscGolfHUD::ReleaseGradeColor(EReleaseGrade Grade) const
{
    switch (Grade)
    {
        case EReleaseGrade::Perfect: return DiscGolfHudStyle::TournamentAmber;
        case EReleaseGrade::Great: return DiscGolfHudStyle::SuccessMint;
        case EReleaseGrade::Good: return DiscGolfHudStyle::SignalTeal;
        case EReleaseGrade::Poor:
        default: return DiscGolfHudStyle::PenaltyCoral;
    }
}

FString ADiscGolfHUD::GroundStateToString(EDiscGroundState State) const
{
    switch (State)
    {
        case EDiscGroundState::Impact: return TEXT("IMPACT");
        case EDiscGroundState::Skipping: return TEXT("SKIP");
        case EDiscGroundState::Sliding: return TEXT("SLIDE");
        case EDiscGroundState::EdgeRolling: return TEXT("EDGE ROLL");
        case EDiscGroundState::Settled: return TEXT("SETTLED");
        case EDiscGroundState::Airborne:
        default: return TEXT("AIRBORNE");
    }
}

FString ADiscGolfHUD::GroundSurfaceToString(EGroundSurfaceType Surface) const
{
    switch (Surface)
    {
        case EGroundSurfaceType::Rough: return TEXT("ROUGH");
        case EGroundSurfaceType::Dirt: return TEXT("DIRT");
        case EGroundSurfaceType::Rock: return TEXT("ROCK");
        case EGroundSurfaceType::TeePad: return TEXT("TEE PAD");
        case EGroundSurfaceType::Fairway:
        default: return TEXT("FAIRWAY");
    }
}

FString ADiscGolfHUD::ShotContextToString(EDiscShotContext Context) const
{
    switch (Context)
    {
        case EDiscShotContext::Circle1Putt: return TEXT("CIRCLE 1 PUTT");
        case EDiscShotContext::Circle2Putt: return TEXT("CIRCLE 2 PUTT");
        case EDiscShotContext::Drive:
        default: return TEXT("DRIVE");
    }
}

FString ADiscGolfHUD::BasketContactToString(EBasketContactResult Result) const
{
    switch (Result)
    {
        case EBasketContactResult::Caught: return TEXT("CENTER CHAINS - MADE");
        case EBasketContactResult::ChainDeflection: return TEXT("WEAK CHAINS - REJECTED");
        case EBasketContactResult::BandRejection: return TEXT("TOP BAND - REJECTED");
        case EBasketContactResult::TrayRejection: return TEXT("TRAY - REJECTED");
        case EBasketContactResult::None:
        default: return TEXT("NO BASKET CONTACT");
    }
}

void ADiscGolfHUD::DrawControlsMenu(
    const ADiscGolfTourPlayerController* PlayerController,
    UFont* Medium,
    UFont* Small)
{
    if (!PlayerController || !Canvas) return;
    if (PlayerController->IsSettingsPageOpen())
    {
        DrawSettingsMenu(PlayerController, Medium, Small);
        return;
    }

    using namespace DiscGolfHudStyle;
    const FViewportLayout Layout(Canvas);
    const auto Rect = [this, &Layout](const FLinearColor& Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, Layout.X(X), Layout.Y(Y), Layout.U(W), Layout.U(H));
    };
    const auto Text = [this, &Layout](const FString& Value, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
    {
        DrawText(Value, Color, Layout.X(X), Layout.Y(Y), Font, Scale * Layout.Scale, false);
    };

    DrawRect(Scrim, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
    Rect(PanelRaised, SafeMargin, 20.0f, ReferenceWidth - SafeMargin * 2.0f, 72.0f);
    Rect(SignalTeal, SafeMargin, 20.0f, 6.0f, 72.0f);
    Text(TEXT("CONTROLS"), PaperWhite, 50.0f, 33.0f, Medium, 1.05f);
    Text(TEXT("ENHANCED INPUT  //  PROFILE CHANGES SAVE AUTOMATICALLY"), MistTeal, 50.0f, 66.0f, Small, 0.80f);
    Text(TEXT("TAB / SHOULDERS  SETTINGS"), SlateGray, 1010.0f, 48.0f, Small, 0.72f);

    TArray<FDiscGolfControlBindingRow> Rows;
    PlayerController->GetControlBindingRows(Rows);
    const int32 SelectedRow = PlayerController->GetSelectedControlIndex();
    const int32 SelectedBinding = PlayerController->GetSelectedBindingIndex();
    const float RowTop = 110.0f;
    // Twenty-two actions fit inside the 1280x720 title/footer safe frame.
    const float RowHeight = 21.0f;
    const float TableX = 42.0f;
    const float TableW = ReferenceWidth - 84.0f;
    Rect(PanelMuted, TableX, RowTop - 9.0f, TableW, Rows.Num() * RowHeight + 20.0f);
    Text(TEXT("ACTION"), SlateGray, 62.0f, RowTop - 3.0f, Small, 0.66f);
    Text(TEXT("ACTIVE BINDINGS"), SlateGray, 302.0f, RowTop - 3.0f, Small, 0.66f);

    for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
    {
        const float RowY = RowTop + 14.0f + RowIndex * RowHeight;
        const bool bSelected = RowIndex == SelectedRow;
        if (bSelected)
        {
            Rect(FLinearColor(0.04f, 0.27f, 0.22f, 0.96f), TableX + 4.0f, RowY - 3.0f, TableW - 8.0f, RowHeight - 1.0f);
            Rect(SignalTeal, TableX + 4.0f, RowY - 3.0f, 4.0f, RowHeight - 1.0f);
        }
        else if ((RowIndex & 1) != 0)
        {
            Rect(FLinearColor(0.03f, 0.065f, 0.055f, 0.52f), TableX + 4.0f, RowY - 3.0f, TableW - 8.0f, RowHeight - 1.0f);
        }

        Text(Rows[RowIndex].DisplayName, bSelected ? PaperWhite : FogGray, 62.0f, RowY, Small, 0.88f);

        FString Bindings;
        for (int32 BindingIndex = 0; BindingIndex < Rows[RowIndex].BindingLabels.Num(); ++BindingIndex)
        {
            if (!Bindings.IsEmpty()) Bindings += TEXT("    ");
            const FString& Label = Rows[RowIndex].BindingLabels[BindingIndex];
            Bindings += bSelected && BindingIndex == SelectedBinding
                ? FString::Printf(TEXT("> [%s] <"), *Label)
                : FString::Printf(TEXT("[%s]"), *Label);
        }
        Text(Bindings, bSelected ? TournamentAmber : FogGray, 302.0f, RowY, Small, 0.82f);
    }

    const float FooterY = 606.0f;
    const FString Status = PlayerController->GetControlsStatusText();
    Rect(PanelRaised, SafeMargin, FooterY - 11.0f, ReferenceWidth - SafeMargin * 2.0f, 101.0f);
    Rect(PlayerController->IsWaitingForControlBinding() ? TournamentAmber : SuccessMint, SafeMargin, FooterY - 11.0f, 6.0f, 101.0f);
    Text(Status, PlayerController->IsWaitingForControlBinding() ? TournamentAmber : SuccessMint, 50.0f, FooterY, Small, 0.88f);
    Text(TEXT("UP/DOWN SELECT ACTION     LEFT/RIGHT SELECT BINDING     ENTER/BOTTOM REBIND     R/TOP RESET ACTION"), FogGray, 50.0f, FooterY + 29.0f, Small, 0.75f);
    Text(TEXT("BACKSPACE/LEFT SHOULDER RESET ALL     ESCAPE/VIEW RETURN TO GAME     * CUSTOMIZED"), FogGray, 50.0f, FooterY + 52.0f, Small, 0.75f);
}

void ADiscGolfHUD::DrawSettingsMenu(
    const ADiscGolfTourPlayerController* PlayerController,
    UFont* Medium,
    UFont* Small)
{
    if (!PlayerController || !Canvas) return;
    using namespace DiscGolfHudStyle;
    const FViewportLayout Layout(Canvas);
    const auto Rect = [this, &Layout](const FLinearColor& Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, Layout.X(X), Layout.Y(Y), Layout.U(W), Layout.U(H));
    };
    const auto Text = [this, &Layout](const FString& Value, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
    {
        DrawText(Value, Color, Layout.X(X), Layout.Y(Y), Font, Scale * Layout.Scale, false);
    };

    DrawRect(Scrim, 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
    Rect(PanelRaised, SafeMargin, 20.0f, ReferenceWidth - SafeMargin * 2.0f, 72.0f);
    Rect(SignalTeal, SafeMargin, 20.0f, 6.0f, 72.0f);
    Text(TEXT("PLAYER SETTINGS"), PaperWhite, 50.0f, 33.0f, Medium, 1.05f);
    Text(TEXT("ACCESSIBILITY, PRESENTATION, AUDIO AND DISPLAY  //  SAVES AUTOMATICALLY"), MistTeal, 50.0f, 66.0f, Small, 0.78f);
    Text(TEXT("TAB / SHOULDERS  CONTROLS"), SlateGray, 1015.0f, 48.0f, Small, 0.72f);

    TArray<FDiscGolfSettingRow> Rows;
    PlayerController->GetSettingRows(Rows);
    const int32 Selected = PlayerController->GetSelectedControlIndex();
    constexpr float ColumnWidth = 568.0f;
    constexpr float RowHeight = 47.0f;
    constexpr int32 RowsPerColumn = 9;
    for (int32 Index = 0; Index < Rows.Num(); ++Index)
    {
        const int32 Column = Index / RowsPerColumn;
        const int32 Row = Index % RowsPerColumn;
        const float X = 42.0f + Column * 610.0f;
        const float Y = 120.0f + Row * RowHeight;
        const bool bSelected = Index == Selected;
        Rect(bSelected ? FLinearColor(0.04f, 0.27f, 0.22f, 0.96f) : PanelMuted,
            X, Y, ColumnWidth, 38.0f);
        if (bSelected) Rect(SignalTeal, X, Y, 4.0f, 38.0f);
        Text(Rows[Index].Label, bSelected ? PaperWhite : FogGray, X + 18.0f, Y + 11.0f, Small, 0.82f);
        Text(FString::Printf(TEXT("<  %s  >"), *Rows[Index].Value),
            bSelected ? TournamentAmber : MistTeal, X + 300.0f, Y + 11.0f, Small, 0.82f);
    }

    Rect(PanelRaised, SafeMargin, 576.0f, ReferenceWidth - SafeMargin * 2.0f, 112.0f);
    Rect(SuccessMint, SafeMargin, 576.0f, 6.0f, 112.0f);
    Text(PlayerController->GetControlsStatusText(), SuccessMint, 50.0f, 591.0f, Small, 0.88f);
    Text(TEXT("UP / DOWN  SELECT     LEFT / RIGHT  ADJUST     TAB / SHOULDERS  CONTROLS"), FogGray, 50.0f, 625.0f, Small, 0.78f);
    Text(TEXT("ESCAPE / VIEW  RETURN TO GAME"), FogGray, 50.0f, 650.0f, Small, 0.78f);
}

void ADiscGolfHUD::DrawScorecard(const ADiscGolfTourGameMode* GameMode, UFont* Medium, UFont* Small)
{
    if (!GameMode || !Canvas) return;

    using namespace DiscGolfHudStyle;
    const FViewportLayout Layout(Canvas);
    const auto Rect = [this, &Layout](const FLinearColor& Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, Layout.X(X), Layout.Y(Y), Layout.U(W), Layout.U(H));
    };
    const auto Text = [this, &Layout](const FString& Value, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
    {
        DrawText(Value, Color, Layout.X(X), Layout.Y(Y), Font, Scale * Layout.Scale, false);
    };

    const FDiscGolfRoundState Round = GameMode->GetRoundState();
    const bool bRoundComplete = GameMode->IsRoundComplete();
    constexpr float PanelW = 790.0f;
    constexpr float PanelH = 464.0f;
    constexpr float PanelX = (ReferenceWidth - PanelW) * 0.5f;
    constexpr float PanelY = (ReferenceHeight - PanelH) * 0.5f;
    const FLinearColor Accent = bRoundComplete ? TournamentAmber : SignalTeal;

    // The modal scrim is physical-viewport UI, not reference-frame UI. This also covers
    // letterbox/pillarbox gutters on ultrawide and 4:3 canvases.
    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.54f), 0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY);
    Rect(Panel, PanelX, PanelY, PanelW, PanelH);
    Rect(Accent, PanelX, PanelY, PanelW, 6.0f);
    Rect(PanelRaised, PanelX + 18.0f, PanelY + 20.0f, PanelW - 36.0f, 92.0f);
    Text(bRoundComplete ? TEXT("FINAL SCORECARD") : TEXT("TOUR SCORECARD"), PaperWhite,
        PanelX + 38.0f, PanelY + 34.0f, Medium, 1.06f);
    Text(Round.CourseName.ToString().ToUpper(), MistTeal, PanelX + 38.0f, PanelY + 69.0f, Small, 0.84f);

    const int32 CompletedHoles = DiscGolfRound::CompletedHoleCount(Round);
    const int32 TotalStrokes = DiscGolfRound::TotalStrokes(Round);
    const int32 TotalPenalties = DiscGolfRound::TotalPenaltyStrokes(Round);
    const FString RelativeScore = DiscGolfRound::ScoreLabel(DiscGolfRound::ScoreToPar(Round));
    const float SummaryX = PanelX + PanelW - 354.0f;
    Text(FString::Printf(TEXT("%d / %d"), CompletedHoles, Round.HoleScores.Num()), PaperWhite, SummaryX, PanelY + 38.0f, Medium, 0.86f);
    Text(TEXT("HOLES"), SlateGray, SummaryX, PanelY + 70.0f, Small, 0.66f);
    Text(FString::FromInt(TotalStrokes), PaperWhite, SummaryX + 88.0f, PanelY + 38.0f, Medium, 0.86f);
    Text(TEXT("STROKES"), SlateGray, SummaryX + 88.0f, PanelY + 70.0f, Small, 0.66f);
    Text(FString::FromInt(TotalPenalties), TotalPenalties > 0 ? PenaltyCoral : PaperWhite, SummaryX + 184.0f, PanelY + 38.0f, Medium, 0.86f);
    Text(TEXT("PENALTIES"), SlateGray, SummaryX + 184.0f, PanelY + 70.0f, Small, 0.66f);
    Text(RelativeScore, TournamentAmber, SummaryX + 286.0f, PanelY + 38.0f, Medium, 0.92f);
    Text(TEXT("TO PAR"), SlateGray, SummaryX + 286.0f, PanelY + 70.0f, Small, 0.66f);

    const float HeaderY = PanelY + 138.0f;
    Rect(FLinearColor(0.045f, 0.105f, 0.088f, 1.0f), PanelX + 18.0f, HeaderY, PanelW - 36.0f, 31.0f);
    Text(TEXT("HOLE"), SlateGray, PanelX + 38.0f, HeaderY + 8.0f, Small, 0.72f);
    Text(TEXT("NAME"), SlateGray, PanelX + 118.0f, HeaderY + 8.0f, Small, 0.72f);
    Text(TEXT("PAR"), SlateGray, PanelX + PanelW - 248.0f, HeaderY + 8.0f, Small, 0.72f);
    Text(TEXT("STROKES"), SlateGray, PanelX + PanelW - 174.0f, HeaderY + 8.0f, Small, 0.72f);
    Text(TEXT("+/-"), SlateGray, PanelX + PanelW - 66.0f, HeaderY + 8.0f, Small, 0.72f);

    for (int32 Index = 0; Index < Round.HoleScores.Num(); ++Index)
    {
        const FDiscGolfRoundHoleScore& Score = Round.HoleScores[Index];
        const float RowY = HeaderY + 42.0f + Index * 50.0f;
        if (Index == Round.CurrentHoleIndex)
        {
            Rect(FLinearColor(0.045f, 0.28f, 0.21f, 0.80f), PanelX + 18.0f, RowY - 9.0f, PanelW - 36.0f, 40.0f);
            Rect(SignalTeal, PanelX + 18.0f, RowY - 9.0f, 4.0f, 40.0f);
        }
        else if ((Index & 1) != 0)
        {
            Rect(FLinearColor(0.025f, 0.060f, 0.052f, 0.72f), PanelX + 18.0f, RowY - 9.0f, PanelW - 36.0f, 40.0f);
        }
        const FString StrokesText = Score.bCompleted ? FString::FromInt(Score.Strokes) : TEXT("-");
        const FString RelativeText = Score.bCompleted ? DiscGolfRound::ScoreLabel(Score.ScoreToPar()) : TEXT("-");
        Text(FString::Printf(TEXT("%02d"), Score.HoleNumber), PaperWhite, PanelX + 40.0f, RowY, Small, 0.94f);
        Text(Score.HoleName.ToString(), PaperWhite, PanelX + 118.0f, RowY, Small, 0.94f);
        Text(FString::FromInt(Score.Par), PaperWhite, PanelX + PanelW - 240.0f, RowY, Small, 0.94f);
        Text(StrokesText, Score.bCompleted ? SuccessMint : SlateGray, PanelX + PanelW - 144.0f, RowY, Small, 0.94f);
        Text(RelativeText, Score.bCompleted ? TournamentAmber : SlateGray, PanelX + PanelW - 66.0f, RowY, Small, 0.94f);
    }

    const FString Footer = GameMode->IsRoundComplete()
        ? TEXT("N / RIGHT TRIGGER  RESTART ROUND     TAB / LEFT TRIGGER  CLOSE")
        : GameMode->IsHoleComplete()
            ? TEXT("N / RIGHT TRIGGER  NEXT HOLE     TAB / LEFT TRIGGER  CLOSE")
            : TEXT("TAB / LEFT TRIGGER  CLOSE");
    Rect(Divider, PanelX + 18.0f, PanelY + PanelH - 58.0f, PanelW - 36.0f, 1.0f);
    Text(Footer, FogGray, PanelX + 38.0f, PanelY + PanelH - 37.0f, Small, 0.80f);
    Text(bRoundComplete ? TEXT("ROUND LOCKED") : TEXT("LIVE ROUND"), Accent,
        PanelX + PanelW - 136.0f, PanelY + PanelH - 37.0f, Small, 0.70f);
}

void ADiscGolfHUD::DrawProductionHUD(
    const ADiscGolfTourGameMode* GameMode,
    ADiscGolferPawn* Golfer,
    UFont* Medium,
    UFont* Small)
{
    if (!GameMode || !Golfer || !Canvas || !GameMode->GetActiveHole()) return;
    using namespace DiscGolfHudStyle;
    const FDiscGolfPlayerSettings Settings = GameMode->GetPlayerSettings();
    if (!Settings.bHudVisible) return;

    const FViewportLayout Layout(Canvas);
    const float HudScale = Settings.HudScale;
    const float TextScale = Settings.TextScale;
    const auto Rect = [this, &Layout](const FLinearColor& Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, Layout.X(X), Layout.Y(Y), Layout.U(W), Layout.U(H));
    };
    const auto Text = [this, &Layout, TextScale](const FString& Value, const FLinearColor& Color,
        float X, float Y, UFont* Font, float Scale)
    {
        DrawText(Value, Color, Layout.X(X), Layout.Y(Y), Font,
            Scale * TextScale * Layout.Scale, false);
    };

    const ADiscGolfHoleActor* Hole = GameMode->GetActiveHole();
    const UDiscBagComponent* Bag = Golfer->GetDiscBag();
    const UThrowControllerComponent* Throw = Golfer->GetThrowController();
    const FDiscGolfRoundState Round = GameMode->GetRoundState();
    const FString CourseName = !Round.CourseName.IsEmpty()
        ? Round.CourseName.ToString().ToUpper()
        : Hole->CourseId.ToString().ToUpper();
    const FString RoundScore = GameMode->HasAuthoredRound() ? GameMode->GetRoundScoreLabel() : TEXT("E");
    const float OfficialDistanceMeters = Hole->GetMeasuredDistanceFeet() / 3.280839895f;
    const FString OfficialDistance = DiscGolfPlayerExperience::FormatDistance(
        OfficialDistanceMeters, Settings.Units);
    const FString RemainingDistance = DiscGolfPlayerExperience::FormatDistance(
        GameMode->GetBasketDistanceMeters(), Settings.Units, GameMode->GetCurrentLieType() != ELieType::Tee);

    if (GameMode->IsHoleIntroVisible())
    {
        const float Progress = GameMode->GetHoleIntroProgress01();
        const float Fade = FMath::Clamp(FMath::Min(Progress / 0.16f, (1.0f - Progress) / 0.20f), 0.0f, 1.0f);
        const FLinearColor IntroPanel(Panel.R, Panel.G, Panel.B, 0.96f * Fade);
        const FLinearColor IntroText(PaperWhite.R, PaperWhite.G, PaperWhite.B, Fade);
        Rect(IntroPanel, 352.0f, 224.0f, 576.0f, 264.0f);
        Rect(FLinearColor(SignalTeal.R, SignalTeal.G, SignalTeal.B, Fade), 352.0f, 224.0f, 576.0f, 6.0f);
        Text(CourseName, FLinearColor(MistTeal.R, MistTeal.G, MistTeal.B, Fade), 430.0f, 258.0f, Small, 0.80f);
        Text(FString::Printf(TEXT("HOLE %d"), Hole->HoleNumber), IntroText, 430.0f, 296.0f, Medium, 1.78f);
        Text(Hole->HoleName.ToString().ToUpper(), IntroText, 430.0f, 356.0f, Medium, 0.92f);
        Text(FString::Printf(TEXT("PAR %d     %s"), Hole->Par, *OfficialDistance),
            FLinearColor(TournamentAmber.R, TournamentAmber.G, TournamentAmber.B, Fade),
            430.0f, 397.0f, Medium, 0.92f);
        Text(TEXT("SPACE / BOTTOM  CONTINUE     L / RIGHT STICK  PREVIEW HOLE"),
            FLinearColor(FogGray.R, FogGray.G, FogGray.B, Fade), 430.0f, 450.0f, Small, 0.68f);
        return;
    }

    // Compact tournament scorebug: no implementation state, raw solver data, or asset diagnostics.
    Rect(Panel, SafeMargin, 22.0f, 378.0f * HudScale, 104.0f * HudScale);
    Rect(SignalTeal, SafeMargin, 22.0f, 5.0f, 104.0f * HudScale);
    Text(CourseName, MistTeal, 44.0f, 34.0f, Small, 0.72f);
    Text(FString::Printf(TEXT("HOLE %d"), Hole->HoleNumber), PaperWhite, 44.0f, 57.0f, Medium, 1.00f);
    Text(FString::Printf(TEXT("PAR %d   %s"), Hole->Par, *OfficialDistance), FogGray,
        44.0f, 94.0f, Small, 0.82f);
    Rect(PanelRaised, 326.0f, 38.0f, 56.0f, 70.0f);
    Text(RoundScore, TournamentAmber, 343.0f, 57.0f, Medium, 1.08f);

    // Authoritative environment wind, formatted in the player's selected units.
    if (AWindDirector* Wind = GameMode->GetWindDirector())
    {
        const FVector WindMps = Wind->GetWindMpsAt(Golfer->GetActorLocation());
        Rect(PanelMuted, 1092.0f, 22.0f, 164.0f, 64.0f);
        Text(TEXT("WIND"), SlateGray, 1110.0f, 32.0f, Small, 0.64f);
        Text(DiscGolfPlayerExperience::FormatWind(WindMps.Size2D(), Settings.Units),
            PaperWhite, 1158.0f, 31.0f, Small, 0.82f);
        const FVector2D Direction(WindMps.X, -WindMps.Y);
        const FVector2D Unit = Direction.GetSafeNormal();
        Draw2DLine(Layout.X(1174.0f), Layout.Y(64.0f),
            Layout.X(1174.0f + Unit.X * 28.0f), Layout.Y(64.0f + Unit.Y * 18.0f),
            SignalTeal.ToFColor(true));
    }

    // Subtle target identification. It fades once the physical basket is easy to read.
    if (!GameMode->IsHoleComplete())
    {
        const float MarkerOpacity = DiscGolfPlayerExperience::BasketMarkerOpacity(
            Settings, GameMode->IsBasketDirectlyVisible(), GameMode->GetBasketDistanceMeters());
        FVector2D Screen;
        APlayerController* PC = GetOwningPlayerController();
        if (MarkerOpacity > 0.0f && PC && PC->ProjectWorldLocationToScreen(
            Hole->BasketLocation + FVector(0.0f, 0.0f, 130.0f), Screen, true))
        {
            const FLinearColor MarkerBase = Settings.bHighContrastBasketMarker
                ? TournamentAmber : SignalTeal;
            const FLinearColor Marker(MarkerBase.R, MarkerBase.G, MarkerBase.B, MarkerOpacity);
            const float Radius = 10.0f * Layout.Scale * HudScale;
            Draw2DLine(Screen.X - Radius, Screen.Y, Screen.X, Screen.Y - Radius, Marker.ToFColor(true));
            Draw2DLine(Screen.X, Screen.Y - Radius, Screen.X + Radius, Screen.Y, Marker.ToFColor(true));
            Draw2DLine(Screen.X + Radius, Screen.Y, Screen.X, Screen.Y + Radius, Marker.ToFColor(true));
            Draw2DLine(Screen.X, Screen.Y + Radius, Screen.X - Radius, Screen.Y, Marker.ToFColor(true));
            DrawText(DiscGolfPlayerExperience::FormatDistance(GameMode->GetBasketDistanceMeters(), Settings.Units),
                Marker, Screen.X - Radius * 2.2f, Screen.Y + Radius + Layout.U(5.0f), Small,
                0.72f * TextScale * Layout.Scale, false);
        }
    }

    if (GameMode->IsCourseFlyoverActive())
    {
        Rect(Panel, 430.0f, 626.0f, 420.0f, 60.0f);
        Text(TEXT("HOLE PREVIEW"), PaperWhite, 452.0f, 643.0f, Medium, 0.86f);
        Text(TEXT("L / RIGHT STICK  SKIP"), FogGray, 670.0f, 651.0f, Small, 0.68f);
        return;
    }

    if (GameMode->IsInstantReplayActive())
    {
        Rect(Panel, 430.0f, 620.0f, 420.0f, 66.0f);
        Rect(TournamentAmber, 430.0f, 620.0f, 5.0f, 66.0f);
        Text(FString::Printf(TEXT("LAST THROW REPLAY  //  %s"), *GameMode->GetReplayCameraLabel()),
            PaperWhite, 452.0f, 635.0f, Medium, 0.72f);
        Rect(Divider, 452.0f, 669.0f, 340.0f, 3.0f);
        Rect(TournamentAmber, 452.0f, 669.0f, 340.0f * GameMode->GetReplayProgress01(), 3.0f);
        Text(TEXT("T / LB  CAMERA     V / RB  EXIT"), FogGray, 654.0f, 641.0f, Small, 0.58f);
        return;
    }

    const FDiscGolfShotPresentationResult Result = GameMode->GetLastShotPresentationResult();
    if (GameMode->IsHoleComplete())
    {
        Rect(Panel, 438.0f, 232.0f, 404.0f, 244.0f);
        Rect(TournamentAmber, 438.0f, 232.0f, 404.0f, 6.0f);
        Text(GameMode->GetHoleCompletionLabel(), TournamentAmber, 506.0f, 274.0f, Medium, 1.50f);
        Text(FString::Printf(TEXT("%d"), GameMode->GetStrokes()), PaperWhite, 614.0f, 340.0f, Medium, 1.84f);
        Text(FString::Printf(TEXT("HOLE %d COMPLETE"), Hole->HoleNumber), MistTeal, 545.0f, 411.0f, Small, 0.78f);
        Text(TEXT("V / RIGHT SHOULDER  WATCH REPLAY     TAB / LEFT TRIGGER  SCORECARD"),
            FogGray, 474.0f, 447.0f, Small, 0.62f);
        return;
    }

    if (Result.bValid && GameMode->GetLastShotResultAgeSeconds() < 4.5f)
    {
        const bool bPenalty = Result.Penalty != EDiscGolfPenaltyType::None;
        const FLinearColor Accent = bPenalty ? PenaltyCoral : SuccessMint;
        Rect(Panel, 874.0f, 536.0f, 382.0f, 150.0f);
        Rect(Accent, 874.0f, 536.0f, 5.0f, 150.0f);
        Text(DiscGolfPlayerExperience::LandingName(Result.Landing), Accent, 898.0f, 554.0f, Medium, 0.88f);
        Text(DiscGolfPlayerExperience::FormatDistance(Result.TotalMeters, Settings.Units),
            PaperWhite, 898.0f, 590.0f, Medium, 1.10f);
        Text(FString::Printf(TEXT("CARRY %s     %s"),
            *DiscGolfPlayerExperience::FormatDistance(Result.CarryMeters, Settings.Units),
            *DiscGolfPlayerExperience::FormatDistance(Result.RemainingMeters, Settings.Units, true)),
            FogGray, 898.0f, 633.0f, Small, 0.67f);
        if (bPenalty) Text(TEXT("+1 PENALTY APPLIED"), PenaltyCoral, 898.0f, 660.0f, Small, 0.68f);
    }

    if (GameMode->GetActiveDisc())
    {
        Rect(PanelMuted, 36.0f, 632.0f, 250.0f, 54.0f);
        Text(FString::Printf(TEXT("SHOT %d  IN FLIGHT"), FMath::Max(1, GameMode->GetStrokes())),
            PaperWhite, 54.0f, 650.0f, Medium, 0.76f);
        return;
    }

    FResolvedDiscDefinition SelectedDisc;
    const UDiscCatalogSubsystem* Catalog = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDiscCatalogSubsystem>() : nullptr;
    const bool bHasDisc = Catalog && Bag
        && Catalog->ResolveDisc(Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(), SelectedDisc);
    Rect(Panel, 36.0f, 554.0f, 432.0f * HudScale, 132.0f * HudScale);
    Rect(TournamentAmber, 36.0f, 554.0f, 5.0f, 132.0f * HudScale);
    const FString Context = GameMode->GetCurrentLieType() == ELieType::Circle1 ? TEXT("C1")
        : GameMode->GetCurrentLieType() == ELieType::Circle2 ? TEXT("C2")
        : FString::Printf(TEXT("SHOT %d"), GameMode->GetStrokes() + 1);
    Text(Context, TournamentAmber, 58.0f, 569.0f, Small, 0.72f);
    Text(RemainingDistance, PaperWhite, 138.0f, 568.0f, Medium, 0.98f);
    if (bHasDisc)
    {
        Text(SelectedDisc.DisplayName.ToString().ToUpper(), PaperWhite, 58.0f, 607.0f, Medium, 0.88f);
        Text(FString::Printf(TEXT("%s  //  %d | %d | %.0f | %.0f  //  %.0f G"),
            *DiscGolfPlayerExperience::DiscClassName(SelectedDisc.Speed), SelectedDisc.Speed,
            SelectedDisc.Glide, SelectedDisc.Turn, SelectedDisc.Fade, SelectedDisc.Aero.MassKg * 1000.0f),
            MistTeal, 58.0f, 640.0f, Small, 0.67f);
    }
    if (Throw)
    {
        Text(FString::Printf(TEXT("%s     POWER %.0f%%     HYZER %.0f     NOSE %.0f"),
            *ThrowStyleToString(static_cast<uint8>(Throw->GetThrowStyle())), Throw->GetPower01() * 100.0f,
            Throw->GetHyzerDeg(), Throw->GetNoseDeg()), FogGray, 58.0f, 665.0f, Small, 0.63f);
        if (Throw->IsTimingActive())
        {
            Rect(PanelRaised, 488.0f, 634.0f, 304.0f, 52.0f);
            Rect(Divider, 510.0f, 662.0f, 260.0f, 6.0f);
            Rect(TournamentAmber, 510.0f + Throw->GetTimingNeedle01() * 256.0f, 655.0f, 4.0f, 20.0f);
            Text(TEXT("RELEASE TIMING"), PaperWhite, 510.0f, 642.0f, Small, 0.66f);
        }
    }

    if (Settings.bAimingIndicatorVisible)
    {
        const float CenterX = Canvas->ClipX * 0.5f;
        const float CenterY = Canvas->ClipY * 0.60f;
        Draw2DLine(CenterX - Layout.U(20.0f), CenterY, CenterX + Layout.U(20.0f), CenterY,
            FColor(190, 220, 202, 170));
        Draw2DLine(CenterX, CenterY - Layout.U(8.0f), CenterX, CenterY + Layout.U(8.0f),
            FColor(190, 220, 202, 170));
    }

    Text(TEXT("L / RIGHT STICK  PREVIEW     V / RIGHT SHOULDER  REPLAY     TAB / LEFT TRIGGER  SCORECARD     ESC / VIEW  SETTINGS"),
        FLinearColor(0.70f, 0.76f, 0.72f, 0.80f), 493.0f, 697.0f, Small, 0.57f);
}

void ADiscGolfHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas || !GetWorld()) return;

    UFont* Medium = GEngine ? GEngine->GetMediumFont() : nullptr;
    UFont* Small = GEngine ? GEngine->GetSmallFont() : nullptr;
    if (const ADiscGolfTourPlayerController* PlayerController = Cast<ADiscGolfTourPlayerController>(GetOwningPlayerController()))
    {
        if (PlayerController->IsControlsMenuOpen())
        {
            DrawControlsMenu(PlayerController, Medium, Small);
            return;
        }
    }

    ADiscGolfTourGameMode* GM = GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>();
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!GM || !Golfer || !GM->GetActiveHole()) return;

    UDiscBagComponent* Bag = Golfer->GetDiscBag();
    UThrowControllerComponent* Throw = Golfer->GetThrowController();
    ADiscGolfHoleActor* Hole = GM->GetActiveHole();

    if (GM->IsScorecardVisible())
    {
        DrawScorecard(GM, Medium, Small);
        return;
    }

    if (!GM->IsDeveloperHudVisible())
    {
        DrawProductionHUD(GM, Golfer, Medium, Small);
        return;
    }

    FDiscGolfHudPresentationContext PresentationContext;
    PresentationContext.bActiveDisc = GM->GetActiveDisc() != nullptr;
    PresentationContext.bReplayActive = GM->IsInstantReplayActive();
    PresentationContext.bComplete = GM->IsHoleComplete();
    PresentationContext.bHasLastRelease = GM->HasLastRelease();
    PresentationContext.bTimingActive = Throw->IsTimingActive();
    PresentationContext.LastReleaseAgeSeconds = GM->GetLastReleaseAgeSeconds();
    const FDiscGolfHudPresentationState PresentationState = DiscGolfHudPresentation::Resolve(PresentationContext);

    using namespace DiscGolfHudStyle;
    const FViewportLayout Layout(Canvas);
    const auto Rect = [this, &Layout](const FLinearColor& Color, float X, float Y, float W, float H)
    {
        DrawRect(Color, Layout.X(X), Layout.Y(Y), Layout.U(W), Layout.U(H));
    };
    const auto Text = [this, &Layout](const FString& Value, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
    {
        DrawText(Value, Color, Layout.X(X), Layout.Y(Y), Font, Scale * Layout.Scale, false);
    };

    constexpr float Left = SafeMargin;
    constexpr float MainWidth = 510.0f;
    constexpr float ScorebugWidth = 660.0f;
    constexpr float CourseStatusScale = 0.68f;
    const bool bCompactScorebug = PresentationState.Mode == EDiscGolfHudPresentationMode::Flight
        || PresentationState.Mode == EDiscGolfHudPresentationMode::Replay;
    const float ActiveScorebugWidth = bCompactScorebug ? MainWidth : ScorebugWidth;
    const FString HoleCountText = GM->HasAuthoredRound()
        ? FString::Printf(TEXT(" OF %d"), GM->GetRoundState().HoleScores.Num()) : FString();
    const FString RoundScoreText = GM->HasAuthoredRound() ? GM->GetRoundScoreLabel() : TEXT("--");
    FString CourseStatusLine1;
    FString CourseStatusLine2;
    WrapToTwoLogicalLines(Canvas, Small, GM->GetCourseStatusText(), CourseStatusScale,
        ActiveScorebugWidth - 44.0f, CourseStatusLine1, CourseStatusLine2);

    const float ScorebugHeight = bCompactScorebug ? 116.0f : 152.0f;
    const TCHAR* ModeLabel = PresentationState.Mode == EDiscGolfHudPresentationMode::Flight
        ? TEXT("LIVE FLIGHT")
        : PresentationState.Mode == EDiscGolfHudPresentationMode::Replay
            ? TEXT("REPLAY")
            : PresentationState.Mode == EDiscGolfHudPresentationMode::Review
                ? TEXT("SHOT REVIEW")
                : PresentationState.Mode == EDiscGolfHudPresentationMode::Complete
                    ? TEXT("FINAL") : TEXT("TOUR PLAY");
    Rect(Panel, Left, SafeMargin, ActiveScorebugWidth, ScorebugHeight);
    Rect(PresentationState.Mode == EDiscGolfHudPresentationMode::Replay ? TournamentAmber : SignalTeal,
        Left, SafeMargin, 6.0f, ScorebugHeight);
    Text(FString::Printf(TEXT("%s  //  HOLE %d%s"), ModeLabel, Hole->HoleNumber, *HoleCountText),
        PresentationState.Mode == EDiscGolfHudPresentationMode::Replay ? TournamentAmber : SignalTeal,
        Left + 22.0f, 38.0f, Small, 0.72f);
    Text(Hole->HoleName.ToString().ToUpper(), PaperWhite, Left + 22.0f, 59.0f, Medium, 1.04f);
    Text(FString::Printf(TEXT("PAR %d     %.0f FT"), Hole->Par, Hole->GetMeasuredDistanceFeet()),
        FogGray, Left + 22.0f, 96.0f, Small, 0.82f);
    Rect(PanelRaised, Left + ActiveScorebugWidth - 126.0f, 48.0f, 104.0f, 52.0f);
    Text(TEXT("TO PAR"), SlateGray, Left + ActiveScorebugWidth - 108.0f, 56.0f, Small, 0.64f);
    Text(RoundScoreText, TournamentAmber, Left + ActiveScorebugWidth - 108.0f, 73.0f, Medium, 0.88f);
    const FLinearColor CourseStatusColor = Hole->IsAuthoredBlockout() ? SuccessMint : FogGray;
    if (!bCompactScorebug)
    {
        Text(CourseStatusLine1, CourseStatusColor, Left + 22.0f, 116.0f, Small, CourseStatusScale);
        if (!CourseStatusLine2.IsEmpty())
        {
            Text(CourseStatusLine2, CourseStatusColor, Left + 22.0f, 133.0f, Small, CourseStatusScale);
        }
        Text(TEXT("N NEXT   //   TAB SCORECARD   //   K COURSE   //   L FLYOVER"), SlateGray,
            Left + 22.0f, 155.0f, Small, 0.64f);
    }

    if (PresentationState.bShowCurrentLie)
    {
        const FString PenaltyText = GM->GetPenaltyStrokes() > 0
            ? FString::Printf(TEXT("  //  %d PENALTY"), GM->GetPenaltyStrokes())
            : FString();
        const FString Status = GM->IsHoleComplete()
            ? FString::Printf(TEXT("%s  //  %d STROKES%s  //  %s"), GM->IsRoundComplete() ? TEXT("ROUND COMPLETE") : TEXT("HOLED OUT"), GM->GetStrokes(),
                *PenaltyText,
                GM->IsRoundComplete() ? TEXT("N RESTART") : TEXT("N NEXT HOLE"))
            : FString::Printf(TEXT("%d STROKES%s  //  %s LIE"), GM->GetStrokes(), *PenaltyText,
                *LieToString(static_cast<uint8>(GM->GetCurrentLieType())));

        Rect(PanelRaised, Left, 186.0f, MainWidth, 76.0f);
        Rect(GM->IsHoleComplete() ? TournamentAmber : SuccessMint, Left, 186.0f, 5.0f, 76.0f);
        Text(GM->IsHoleComplete() ? TEXT("HOLE STATUS") : TEXT("CURRENT LIE"), SlateGray,
            Left + 20.0f, 197.0f, Small, 0.66f);
        Text(Status, GM->IsHoleComplete() ? TournamentAmber : PaperWhite, Left + 20.0f, 215.0f, Medium, 0.84f);
        const FDiscGolfLieState LieState = GM->GetCurrentLieState();
        const bool bPenaltyLie = LieState.PenaltyType != EDiscGolfPenaltyType::None;
        Text(GM->GetLieRulesStatusText(), bPenaltyLie ? PenaltyCoral : SuccessMint,
            Left + 20.0f, 242.0f, Small, 0.76f);
    }

    if (PresentationState.bShowShotSetup)
    {
        Rect(Panel, Left, 272.0f, MainWidth, 132.0f);
        Text(TEXT("SHOT SETUP"), MistTeal, Left + 18.0f, 284.0f, Small, 0.68f);
        Text(Golfer->GetGolferPresentationStatusText(), SlateGray, Left + 250.0f, 284.0f, Small, 0.56f);
        float SetupY = 306.0f;
        FResolvedDiscDefinition Selected;
        if (UDiscCatalogSubsystem* Catalog = GetWorld()->GetGameInstance()->GetSubsystem<UDiscCatalogSubsystem>())
        {
            if (Catalog->ResolveDisc(Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(), Selected))
            {
                Text(FString::Printf(TEXT("%s  //  %d / %d / %.0f / %.0f  //  %s  //  %s"),
                    *Selected.DisplayName.ToString(), Selected.Speed, Selected.Glide, Selected.Turn, Selected.Fade,
                    *PlasticToString(static_cast<uint8>(Selected.Plastic)),
                    Catalog->IsUsingPrimaryAssets() ? TEXT("DATA ASSET") : TEXT("SOURCE FALLBACK")),
                    TournamentAmber, Left + 18.0f, SetupY, Small, 0.82f);
            }
        }

        SetupY += 23.0f;
        Text(FString::Printf(TEXT("%s  //  POWER %.0f%%  //  HYZER %+.1f  //  NOSE %+.1f"),
            *ThrowStyleToString(static_cast<uint8>(Throw->GetThrowStyle())), Throw->GetPower01() * 100.0f,
            Throw->GetHyzerDeg(), Throw->GetNoseDeg()), PaperWhite, Left + 18.0f, SetupY, Small, 0.84f);

        if (Throw->IsPutting())
        {
            const float EstimatedRange = DiscGolfMath::EstimatedPuttRangeMeters(Throw->GetPower01());
            const float PaceDelta = EstimatedRange - GM->GetBasketDistanceMeters();
            const float AimErrorCm = GM->GetAimErrorAtBasketCm();
            const FString AimText = FMath::Abs(AimErrorCm) < 2.0f
                ? TEXT("AIM CENTER")
                : FString::Printf(TEXT("BASKET %.0f CM %s"), FMath::Abs(AimErrorCm), AimErrorCm > 0.0f ? TEXT("LEFT") : TEXT("RIGHT"));
            SetupY += 23.0f;
            Text(FString::Printf(TEXT("%s  //  %.1f M  //  PACE %+.1f M  //  %s  //  LAUNCH %.1f"),
                *ShotContextToString(Throw->GetShotContext()), GM->GetBasketDistanceMeters(), PaceDelta,
                *AimText, Throw->GetLaunchAngleDeg()),
                SuccessMint, Left + 18.0f, SetupY, Small, 0.74f);
        }

        if (AWindDirector* Wind = GM->GetWindDirector())
        {
            const FVector WindMps = Wind->GetWindMpsAt(Golfer->GetActorLocation());
            const FName WindZone = Wind->GetActiveZoneIdAt(Golfer->GetActorLocation());
            SetupY += 23.0f;
            Text(FString::Printf(TEXT("WIND %.1f MPH  //  VECTOR [%.1f, %.1f] M/S  //  %s"),
                WindMps.Size2D() * 2.23694f, WindMps.X, WindMps.Y,
                WindZone.IsNone() ? TEXT("GLOBAL") : *WindZone.ToString()),
                MistTeal, Left + 18.0f, SetupY, Small, 0.74f);
        }
    }

    if (GM->IsRouteTelemetryActive()
        && PresentationState.Mode != EDiscGolfHudPresentationMode::Flight
        && PresentationState.Mode != EDiscGolfHudPresentationMode::Replay)
    {
        constexpr float TelemetryX = ReferenceWidth - SafeMargin - 520.0f;
        constexpr float TelemetryY = SafeMargin;
        Rect(Panel, TelemetryX, TelemetryY, 520.0f, 176.0f);
        Rect(TournamentAmber, TelemetryX, TelemetryY, 6.0f, 176.0f);
        Text(TEXT("NEEDLE GATE // ROUTE TELEMETRY"), TournamentAmber,
            TelemetryX + 20.0f, TelemetryY + 12.0f, Small, 0.68f);
        Text(GM->GetRouteTelemetryRouteLabel(), PaperWhite,
            TelemetryX + 20.0f, TelemetryY + 34.0f, Medium, 0.78f);
        Text(GM->GetRouteTelemetryProgressText(), SuccessMint,
            TelemetryX + 20.0f, TelemetryY + 62.0f, Small, 0.72f);
        FString IntentLine1;
        FString IntentLine2;
        WrapToTwoLogicalLines(Canvas, Small, GM->GetRouteTelemetryIntentText(), 0.62f,
            480.0f, IntentLine1, IntentLine2);
        Text(IntentLine1, FogGray, TelemetryX + 20.0f, TelemetryY + 83.0f, Small, 0.62f);
        if (!IntentLine2.IsEmpty())
        {
            Text(IntentLine2, FogGray, TelemetryX + 20.0f, TelemetryY + 100.0f, Small, 0.62f);
        }
        Text(GM->GetRouteTelemetryLastResultText(), MistTeal,
            TelemetryX + 20.0f, TelemetryY + 121.0f, Small, 0.58f);
        Text(TEXT("BEFORE TEE: DGT_TelemetryTradeoffUnderstood 0/1"), SlateGray,
            TelemetryX + 20.0f, TelemetryY + 139.0f, Small, 0.48f);
        Text(TEXT("AFTER LIE: DGT_TelemetryNextShotClear 0/1 // R RESET"), SlateGray,
            TelemetryX + 20.0f, TelemetryY + 156.0f, Small, 0.48f);
    }

    if (GM->IsDeveloperHudVisible())
    {
        const FLinearColor TrajectoryColor = GM->GetTrajectoryStatusText().Contains(TEXT("FAIL"))
            ? PenaltyCoral
            : GM->IsPhysicsRegressionRunning()
                ? TournamentAmber
                : SuccessMint;

        Rect(PanelMuted, Left, 414.0f, MainWidth, 114.0f);
        Rect(Divider, Left, 414.0f, 5.0f, 114.0f);
        Text(TEXT("TUNING OVERLAY  //  DEV"), SlateGray, Left + 18.0f, 426.0f, Small, 0.64f);
        const EDiscGolfPerformanceBudgetState PerformanceState = GM->GetPerformanceBudgetState();
        const FLinearColor PerformanceColor = PerformanceState == EDiscGolfPerformanceBudgetState::Fail
            ? PenaltyCoral : PerformanceState == EDiscGolfPerformanceBudgetState::Warning
                ? TournamentAmber : PerformanceState == EDiscGolfPerformanceBudgetState::Pass ? SuccessMint : SlateGray;
        Text(GM->GetPerformanceStatusText(), PerformanceColor, Left + 166.0f, 426.0f, Small, 0.52f);
        Text(GM->GetTrajectoryStatusText(), TrajectoryColor, Left + 18.0f, 447.0f, Small, 0.72f);
        Text(FString::Printf(TEXT("PHYSICS  //  %s  //  C CYCLE  //  G RUN  //  H SUITE"),
            *GM->GetSelectedRegressionPresetName()), FogGray, Left + 18.0f, 468.0f, Small, 0.68f);
        Text(GM->GetPresentationStatusText(), GM->IsInstantReplayActive() ? TournamentAmber : SignalTeal,
            Left + 18.0f, 489.0f, Small, 0.70f);
        Text(GM->GetBroadcastCameraStatusText(), GM->IsBroadcastCameraActive() ? SuccessMint : FogGray,
            Left + 18.0f, 510.0f, Small, 0.68f);
    }

    if (PresentationState.bShowReplayChrome)
    {
        constexpr float ReplayPanelX = ReferenceWidth - SafeMargin - 524.0f;
        constexpr float ReplayPanelY = 526.0f;
        constexpr float ReplayBarX = ReplayPanelX + 18.0f;
        constexpr float ReplayBarY = ReplayPanelY + 42.0f;
        constexpr float ReplayBarW = 488.0f;
        const float ReplayProgress = GM->GetReplayProgress01();
        Rect(Panel, ReplayPanelX, ReplayPanelY, 524.0f, 76.0f);
        Rect(TournamentAmber, ReplayPanelX, ReplayPanelY, 5.0f, 76.0f);
        Text(FString::Printf(TEXT("INSTANT REPLAY  //  %.2fX  //  %.1f / %.1f S"),
            GM->GetReplayPlaybackRate(), ReplayProgress * GM->GetReplayDurationSeconds(), GM->GetReplayDurationSeconds()),
            TournamentAmber, ReplayBarX, ReplayPanelY + 12.0f, Small, 0.76f);
        Rect(FLinearColor(0.10f, 0.14f, 0.18f, 1.0f), ReplayBarX, ReplayBarY, ReplayBarW, 8.0f);
        Rect(TournamentAmber, ReplayBarX, ReplayBarY, ReplayBarW * ReplayProgress, 8.0f);
        Text(TEXT("V / RIGHT SHOULDER TO CANCEL"), FogGray,
            ReplayBarX + ReplayBarW - 184.0f, ReplayBarY + 15.0f, Small, 0.56f);
    }

    if (Throw->IsTimingActive())
    {
        const float BarX = Left;
        const float BarY = 570.0f;
        const float BarW = MainWidth;
        const float Ideal = Throw->GetIdealTiming01();
        const float MissSpan = Throw->GetTimingMissSpan01();
        const auto DrawTimingBand = [&Rect, BarX, BarY, BarW, Ideal, MissSpan](float ErrorRadius, const FLinearColor& Color)
        {
            const float Min01 = FMath::Clamp(Ideal - MissSpan * ErrorRadius, 0.0f, 1.0f);
            const float Max01 = FMath::Clamp(Ideal + MissSpan * ErrorRadius, 0.0f, 1.0f);
            Rect(Color, BarX + BarW * Min01, BarY, BarW * (Max01 - Min01), 18.0f);
        };

        Rect(Panel, BarX, BarY - 38.0f, BarW, 78.0f);
        Rect(FLinearColor(0.10f, 0.12f, 0.14f, 1.0f), BarX, BarY, BarW, 18.0f);
        DrawTimingBand(DiscGolfMath::ReleaseGoodError, FLinearColor(0.08f, 0.38f, 0.31f, 0.95f));
        DrawTimingBand(DiscGolfMath::ReleaseGreatError, FLinearColor(0.16f, 0.62f, 0.32f, 0.95f));
        DrawTimingBand(DiscGolfMath::ReleasePerfectError, FLinearColor(0.92f, 0.62f, 0.12f, 0.98f));
        Rect(PaperWhite, BarX + BarW * Throw->GetTimingNeedle01() - 2.0f, BarY - 4.0f, 4.0f, 26.0f);
        const float PreviewError = DiscGolfMath::NormalizeTimingError(Throw->GetTimingNeedle01(), Ideal, MissSpan);
        const EReleaseGrade PreviewGrade = DiscGolfMath::ReleaseGrade(PreviewError);
        Text(FString::Printf(TEXT("%s  //  PRESS THROW AGAIN TO RELEASE"), *ReleaseGradeToString(PreviewGrade)),
            ReleaseGradeColor(PreviewGrade), BarX + 14.0f, BarY - 27.0f, Small, 0.78f);
        Text(TEXT("EARLY                              PERFECT                              LATE"),
            FogGray, BarX + 8.0f, BarY + 24.0f, Small, 0.59f);
    }

    if (PresentationState.bShowShotFeedback)
    {
        const FThrowRelease Release = GM->GetLastRelease();
        constexpr float ReleasePanelW = 400.0f;
        constexpr float ReleasePanelX = ReferenceWidth - SafeMargin - ReleasePanelW;
        constexpr float ReleasePanelY = SafeMargin;
        const bool bLieModified = !FMath::IsNearlyEqual(Release.LiePowerMultiplier, 1.0f)
            || !FMath::IsNearlyEqual(Release.LieTimingErrorMultiplier, 1.0f);
        const float ReleasePanelH = bLieModified ? 174.0f : 150.0f;
        Rect(Panel, ReleasePanelX, ReleasePanelY, ReleasePanelW, ReleasePanelH);
        Rect(ReleaseGradeColor(Release.Grade), ReleasePanelX, ReleasePanelY, 6.0f, ReleasePanelH);
        Text(TEXT("SHOT FEEDBACK"), SlateGray, ReleasePanelX + 20.0f, ReleasePanelY + 12.0f, Small, 0.56f);
        Text(FString::Printf(TEXT("%s  //  %s  //  %.0f%%"),
            *ReleaseGradeToString(Release.Grade), *ReleaseTimingToString(Release.Timing), Release.Quality01 * 100.0f),
            ReleaseGradeColor(Release.Grade), ReleasePanelX + 20.0f, ReleasePanelY + 33.0f, Medium, 0.78f);
        Text(FString::Printf(TEXT("%.1f M/S  //  %.0f RPM"), Release.ReleaseSpeedMps, Release.SpinRpm),
            PaperWhite, ReleasePanelX + 20.0f, ReleasePanelY + 68.0f, Small, 0.78f);
        Text(FString::Printf(TEXT("AIM %+.1f  //  HYZER %+.1f  //  NOSE %+.1f  //  LAUNCH %+.1f"),
            Release.AimOffsetDeg, Release.HyzerOffsetDeg, Release.NoseOffsetDeg, Release.LaunchOffsetDeg),
            FogGray, ReleasePanelX + 20.0f, ReleasePanelY + 93.0f, Small, 0.66f);
        Text(FString::Printf(TEXT("SPEED %.0f%%  //  SPIN %.0f%% OF CLEAN RELEASE"),
            Release.SpeedMultiplier * 100.0f, Release.SpinMultiplier * 100.0f),
            FogGray, ReleasePanelX + 20.0f, ReleasePanelY + 116.0f, Small, 0.66f);
        if (Release.ShotContext != EDiscShotContext::Drive)
        {
            Text(ShotContextToString(Release.ShotContext), SuccessMint,
                ReleasePanelX + 258.0f, ReleasePanelY + 68.0f, Small, 0.60f);
        }
        if (bLieModified)
        {
            Text(FString::Printf(TEXT("LIE  //  %.0f%% POWER  //  %.0f%% TIMING SENSITIVITY"),
                Release.LiePowerMultiplier * 100.0f, Release.LieTimingErrorMultiplier * 100.0f),
                TournamentAmber, ReleasePanelX + 20.0f, ReleasePanelY + 140.0f, Small, 0.61f);
        }
    }

    if (GM->IsRoundComplete())
    {
        constexpr float CompleteW = 400.0f;
        constexpr float CompleteX = ReferenceWidth - SafeMargin - CompleteW;
        constexpr float CompleteY = SafeMargin;
        Rect(PanelRaised, CompleteX, CompleteY, CompleteW, 104.0f);
        Rect(TournamentAmber, CompleteX, CompleteY, CompleteW, 5.0f);
        Text(TEXT("ROUND COMPLETE"), TournamentAmber, CompleteX + 22.0f, CompleteY + 20.0f, Medium, 0.92f);
        Text(FString::Printf(TEXT("FINAL  %s  //  %d STROKES"), *GM->GetRoundScoreLabel(),
            DiscGolfRound::TotalStrokes(GM->GetRoundState())),
            PaperWhite, CompleteX + 22.0f, CompleteY + 54.0f, Small, 0.78f);
        Text(TEXT("N RESTART ROUND   //   TAB FINAL SCORECARD"), FogGray,
            CompleteX + 22.0f, CompleteY + 79.0f, Small, 0.61f);
    }

    if (PresentationState.bShowFlightStrip)
    {
        ADiscActor* Disc = GM->GetActiveDisc();
        if (!Disc) return;
        const FDiscFlightTelemetry& Telemetry = Disc->GetFlightComponent()->GetTelemetry();
        const bool bHasGroundContact = Telemetry.GroundContactCount > 0;
        const float FlightPanelY = bHasGroundContact ? 626.0f : 650.0f;
        const float FlightPanelH = bHasGroundContact ? 70.0f : 46.0f;
        Rect(Panel, Left, FlightPanelY, 780.0f, FlightPanelH);
        Rect(SignalTeal, Left, FlightPanelY, 5.0f, FlightPanelH);
        Text(FString::Printf(TEXT("FLIGHT  //  %.1f M/S  //  %.0f RPM  //  %.1f M CARRY  //  AOA %.1f  //  %s"),
            Telemetry.SpeedMps, Telemetry.SpinRpm, Telemetry.CarryMeters, Telemetry.AngleOfAttackDeg,
            *ReleaseGradeToString(Telemetry.Release.Grade)), PaperWhite, Left + 18.0f, FlightPanelY + 12.0f, Small, 0.74f);
        if (Telemetry.GroundContactCount > 0)
        {
            Text(FString::Printf(TEXT("GROUND  //  %s / %s / %s  //  %d IMPACTS  //  %.1f M/S @ %.1f  //  EDGE %.1f  //  %.1f M"),
                *GroundStateToString(Telemetry.GroundState), *GroundSurfaceToString(Telemetry.GroundSurface),
                *DiscGolfCourseRules::SurfaceName(Telemetry.CourseSurface),
                Telemetry.GroundContactCount, Telemetry.LastImpactSpeedMps, Telemetry.LastImpactIncidenceDeg,
                Telemetry.LastDiscEdgeAngleDeg, Telemetry.GroundDistanceMeters),
                TournamentAmber, Left + 18.0f, FlightPanelY + 37.0f, Small, 0.62f);
        }
    }
    else if (PresentationState.bShowHelp && !Throw->IsTimingActive())
    {
        const bool bHasLastTelemetry = GM->HasLastFlightTelemetry();
        const float HelpPanelY = bHasLastTelemetry ? 608.0f : 636.0f;
        const float HelpPanelH = bHasLastTelemetry ? 88.0f : 60.0f;
        Rect(PanelMuted, Left, HelpPanelY, ReferenceWidth - SafeMargin * 2.0f, HelpPanelH);
        Rect(Divider, Left, HelpPanelY, 5.0f, HelpPanelH);
        if (GM->HasLastFlightTelemetry())
        {
            const FDiscFlightTelemetry Last = GM->GetLastFlightTelemetry();
            const FString LastResult = Last.BasketContactCount > 0
                ? FString::Printf(TEXT("BASKET  %s   contacts %d"), *BasketContactToString(Last.LastBasketContact), Last.BasketContactCount)
                : FString::Printf(TEXT("LAST GROUND  %s / %s / %s   impacts %d   hit %.1f m/s @ %.1f deg   ground %.1f m"),
                    *GroundStateToString(Last.GroundState), *GroundSurfaceToString(Last.GroundSurface),
                    *DiscGolfCourseRules::SurfaceName(Last.CourseSurface),
                    Last.GroundContactCount, Last.LastImpactSpeedMps, Last.LastImpactIncidenceDeg, Last.GroundDistanceMeters);
            Text(LastResult.ToUpper(), TournamentAmber, Left + 18.0f, HelpPanelY + 11.0f, Small, 0.72f);
        }
        const float HelpTextY = bHasLastTelemetry ? HelpPanelY + 37.0f : HelpPanelY + 10.0f;
        Text(TEXT("KEYBOARD  //  A/D AIM   W/S POWER   Q/E HYZER   Z/X NOSE   F BH/FH   1-5 DISC   SPACE X2 THROW   N NEXT   TAB CARD   R RESET"),
            FogGray, Left + 18.0f, HelpTextY, Small, 0.66f);
        Text(TEXT("CONTROLLER  //  LS/RS SETUP   BOTTOM X2 THROW   TRIGGERS CARD/NEXT   SHOULDERS TRACER/REPLAY   D-PAD/TOP DISCS   VIEW CONTROLS"),
            MistTeal, Left + 18.0f, HelpTextY + 23.0f, Small, 0.64f);
    }
}
