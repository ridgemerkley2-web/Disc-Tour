#include "DiscGolfCharacterCreatorWidget.h"

#include "DiscGolfTourPlayerController.h"
#include "Input/Reply.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace DiscGolfCharacterCreatorStyle
{
    // Keep the world visible behind the creator: the existing possessed pawn
    // is the live preview subject, so a full-screen opaque scrim would make the
    // feature technically responsive but visually useless.
    const FLinearColor Scrim(0.006f, 0.014f, 0.012f, 0.24f);
    const FLinearColor Panel(0.018f, 0.055f, 0.047f, 0.97f);
    const FLinearColor PanelRaised(0.028f, 0.085f, 0.072f, 0.18f);
    const FLinearColor Paper(0.92f, 0.96f, 0.94f, 1.0f);
    const FLinearColor Muted(0.58f, 0.70f, 0.66f, 1.0f);
    const FLinearColor Signal(0.17f, 0.88f, 0.68f, 1.0f);
    const FLinearColor Warning(1.0f, 0.68f, 0.22f, 1.0f);
}

void UDiscGolfCharacterCreatorWidget::InitializeCreator(
    ADiscGolfTourPlayerController* InController,
    const FDGBodyProfile& InBody,
    const FDGThrowStyle& InThrowStyle,
    EDGHandedness InHandedness)
{
    OwningDiscGolfController = InController;
    SetDraftProfile(InBody, InThrowStyle, InHandedness);
}

void UDiscGolfCharacterCreatorWidget::SetDraftProfile(
    const FDGBodyProfile& InBody,
    const FDGThrowStyle& InThrowStyle,
    EDGHandedness InHandedness)
{
    DraftBody = InBody;
    DraftThrowStyle = InThrowStyle;
    DraftHandedness = InHandedness;
    InvalidateLayoutAndVolatility();
}

void UDiscGolfCharacterCreatorWidget::GetDraftProfile(
    FDGBodyProfile& OutBody,
    FDGThrowStyle& OutThrowStyle,
    EDGHandedness& OutHandedness) const
{
    OutBody = DraftBody;
    OutThrowStyle = DraftThrowStyle;
    OutHandedness = DraftHandedness;
}

TSharedPtr<SWidget> UDiscGolfCharacterCreatorWidget::GetInitialFocusWidget() const
{
    return InitialFocusButton;
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::RebuildWidget()
{
    using namespace DiscGolfCharacterCreatorStyle;

    RootSlateWidget =
        SNew(SOverlay)
        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
            .BorderBackgroundColor(Scrim)
        ]
        + SOverlay::Slot()
        .Padding(FMargin(28.0f))
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SBox)
                .WidthOverride(760.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(Panel)
                    .Padding(FMargin(26.0f, 20.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("CHARACTER CREATOR")))
                            .ColorAndOpacity(Paper)
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 26))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 2.0f, 0.0f, 14.0f)
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("SESSION 4  //  ONE MASTER SKELETON  //  LIVE CURRENT-PLAYER PREVIEW")))
                            .ColorAndOpacity(Muted)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SAssignNew(InitialFocusButton, SButton)
                                .Text(FText::FromString(TEXT("BASELINE")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandlePreset, FName(TEXT("Baseline")))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("SHORT COMPACT")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandlePreset, FName(TEXT("ShortCompact")))
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("TALL / LONG ARMS")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandlePreset, FName(TEXT("TallLongArms")))
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SNew(SScrollBox)
                            + SScrollBox::Slot()
                            [
                                SNew(SVerticalBox)
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 4.0f)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("BODY")))
                                    .ColorAndOpacity(Signal)
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                                ]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("HEIGHT")), ECreatorField::Height, 150.0f, 210.0f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("WINGSPAN")), ECreatorField::Wingspan, 0.92f, 1.08f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("SHOULDER WIDTH")), ECreatorField::ShoulderWidth, 0.92f, 1.08f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("TORSO LENGTH")), ECreatorField::TorsoLength, 0.94f, 1.06f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("LEG LENGTH")), ECreatorField::LegLength, 0.94f, 1.06f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("HAND SIZE")), ECreatorField::HandScale, 0.94f, 1.06f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("BODY MASS")), ECreatorField::Mass, 45.0f, 160.0f)]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 14.0f, 0.0f, 8.0f)
                                [
                                    SNew(SSeparator)
                                ]
                                + SVerticalBox::Slot()
                                .AutoHeight()
                                .Padding(0.0f, 4.0f)
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("THROW STYLE  //  PRESENTATION ONLY")))
                                    .ColorAndOpacity(Signal)
                                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
                                ]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("RUN-UP INTENSITY")), ECreatorField::RunUp, 0.0f, 1.0f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("REACHBACK")), ECreatorField::ReachBack, 0.0f, 1.0f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("TORSO ROTATION")), ECreatorField::TorsoRotation, 0.0f, 1.0f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("BRACE INTENSITY")), ECreatorField::Brace, 0.0f, 1.0f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("EXPLOSIVENESS")), ECreatorField::Explosiveness, 0.0f, 1.0f)]
                                + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("FOLLOW-THROUGH")), ECreatorField::FollowThrough, 0.0f, 1.0f)]
                            ]
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 12.0f, 0.0f, 0.0f)
                        [
                            SNew(STextBlock)
                            .Text_UObject(this, &UDiscGolfCharacterCreatorWidget::GetStatusText)
                            .ColorAndOpacity(Muted)
                            .AutoWrapText(true)
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 12.0f, 0.0f, 0.0f)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("RESET EDITS")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleReset)
                            ]
                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            [
                                SNew(SSpacer)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("CANCEL")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleCancel)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("APPLY & SAVE")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleApply)
                            ]
                        ]
                    ]
                ]
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .Padding(28.0f, 0.0f, 0.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                .BorderBackgroundColor(PanelRaised)
                .Padding(FMargin(22.0f))
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("CURRENT PLAYER PREVIEW")))
                        .ColorAndOpacity(Paper)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 18.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("The existing possessed golfer is the preview subject. No duplicate player is spawned.")))
                        .ColorAndOpacity(Muted)
                        .AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("HANDEDNESS")))
                        .ColorAndOpacity(Signal)
                        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("RIGHT")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRightHanded)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("LEFT")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleLeftHanded)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text_UObject(this, &UDiscGolfCharacterCreatorWidget::GetHandednessText)
                        .ColorAndOpacity(Paper)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 10.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text_UObject(this, &UDiscGolfCharacterCreatorWidget::GetHandednessLimitationText)
                        .ColorAndOpacity_UObject(this, &UDiscGolfCharacterCreatorWidget::GetHandednessLimitationColor)
                        .AutoWrapText(true)
                    ]
                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SSpacer)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("PREVIEW ANGLE")))
                        .ColorAndOpacity(Muted)
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 8.0f, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("ROTATE LEFT")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRotateLeft)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("ROTATE RIGHT")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRotateRight)
                        ]
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 18.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("ESCAPE / VIEW  CANCEL AND RETURN TO GAMEPLAY")))
                        .ColorAndOpacity(Muted)
                        .AutoWrapText(true)
                    ]
                ]
            ]
        ];

    return RootSlateWidget.ToSharedRef();
}

void UDiscGolfCharacterCreatorWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootSlateWidget.Reset();
    InitialFocusButton.Reset();
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildSliderRow(
    const FText& Label,
    ECreatorField Field,
    float MinValue,
    float MaxValue)
{
    const TWeakObjectPtr<UDiscGolfCharacterCreatorWidget> WeakThis(this);
    return
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .FillWidth(0.36f)
        .VAlign(VAlign_Center)
        .Padding(0.0f, 5.0f, 12.0f, 5.0f)
        [
            SNew(STextBlock)
            .Text(Label)
            .ColorAndOpacity(DiscGolfCharacterCreatorStyle::Paper)
        ]
        + SHorizontalBox::Slot()
        .FillWidth(0.48f)
        .VAlign(VAlign_Center)
        .Padding(0.0f, 5.0f)
        [
            SNew(SSlider)
            .Value_Lambda([WeakThis, Field, MinValue, MaxValue]()
            {
                const UDiscGolfCharacterCreatorWidget* Self = WeakThis.Get();
                if (!Self || MaxValue <= MinValue) return 0.0f;
                return FMath::Clamp((Self->GetFieldValue(Field) - MinValue) / (MaxValue - MinValue), 0.0f, 1.0f);
            })
            .OnValueChanged_Lambda([WeakThis, Field, MinValue, MaxValue](float NormalizedValue)
            {
                if (UDiscGolfCharacterCreatorWidget* Self = WeakThis.Get())
                {
                    Self->HandleSliderChanged(NormalizedValue, Field, MinValue, MaxValue);
                }
            })
        ]
        + SHorizontalBox::Slot()
        .FillWidth(0.16f)
        .HAlign(HAlign_Right)
        .VAlign(VAlign_Center)
        .Padding(12.0f, 5.0f, 0.0f, 5.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([WeakThis, Field]()
            {
                const UDiscGolfCharacterCreatorWidget* Self = WeakThis.Get();
                return Self ? Self->GetFieldValueText(Field) : FText::GetEmpty();
            })
            .ColorAndOpacity(DiscGolfCharacterCreatorStyle::Signal)
        ];
}

float UDiscGolfCharacterCreatorWidget::GetFieldValue(ECreatorField Field) const
{
    switch (Field)
    {
        case ECreatorField::Height: return DraftBody.HeightCm;
        case ECreatorField::Wingspan: return DraftBody.WingspanScale;
        case ECreatorField::ShoulderWidth: return DraftBody.ShoulderWidthScale;
        case ECreatorField::TorsoLength: return DraftBody.TorsoLengthScale;
        case ECreatorField::LegLength: return DraftBody.LegLengthScale;
        case ECreatorField::HandScale: return DraftBody.HandScale;
        case ECreatorField::Mass: return DraftBody.MassKg;
        case ECreatorField::RunUp: return DraftThrowStyle.RunUpIntensity;
        case ECreatorField::ReachBack: return DraftThrowStyle.ReachBackAmount;
        case ECreatorField::TorsoRotation: return DraftThrowStyle.TorsoRotation;
        case ECreatorField::Brace: return DraftThrowStyle.BraceIntensity;
        case ECreatorField::Explosiveness: return DraftThrowStyle.Explosiveness;
        case ECreatorField::FollowThrough: return DraftThrowStyle.FollowThrough;
        default: return 0.0f;
    }
}

void UDiscGolfCharacterCreatorWidget::SetFieldValue(ECreatorField Field, float Value)
{
    switch (Field)
    {
        case ECreatorField::Height: DraftBody.HeightCm = Value; break;
        case ECreatorField::Wingspan: DraftBody.WingspanScale = Value; break;
        case ECreatorField::ShoulderWidth: DraftBody.ShoulderWidthScale = Value; break;
        case ECreatorField::TorsoLength: DraftBody.TorsoLengthScale = Value; break;
        case ECreatorField::LegLength: DraftBody.LegLengthScale = Value; break;
        case ECreatorField::HandScale: DraftBody.HandScale = Value; break;
        case ECreatorField::Mass: DraftBody.MassKg = Value; break;
        case ECreatorField::RunUp: DraftThrowStyle.RunUpIntensity = Value; break;
        case ECreatorField::ReachBack: DraftThrowStyle.ReachBackAmount = Value; break;
        case ECreatorField::TorsoRotation: DraftThrowStyle.TorsoRotation = Value; break;
        case ECreatorField::Brace: DraftThrowStyle.BraceIntensity = Value; break;
        case ECreatorField::Explosiveness: DraftThrowStyle.Explosiveness = Value; break;
        case ECreatorField::FollowThrough: DraftThrowStyle.FollowThrough = Value; break;
        default: break;
    }
}

FText UDiscGolfCharacterCreatorWidget::GetFieldValueText(ECreatorField Field) const
{
    const float Value = GetFieldValue(Field);
    if (Field == ECreatorField::Height)
    {
        return FText::FromString(FString::Printf(TEXT("%.0f CM"), Value));
    }
    if (Field == ECreatorField::Mass)
    {
        return FText::FromString(FString::Printf(TEXT("%.0f KG"), Value));
    }
    if (Field == ECreatorField::Wingspan
        || Field == ECreatorField::ShoulderWidth
        || Field == ECreatorField::TorsoLength
        || Field == ECreatorField::LegLength
        || Field == ECreatorField::HandScale)
    {
        return FText::FromString(FString::Printf(TEXT("%.2f X"), Value));
    }
    return FText::FromString(FString::Printf(TEXT("%.0f%%"), Value * 100.0f));
}

void UDiscGolfCharacterCreatorWidget::HandleSliderChanged(
    float NormalizedValue,
    ECreatorField Field,
    float MinValue,
    float MaxValue)
{
    SetFieldValue(Field, FMath::Lerp(MinValue, MaxValue, FMath::Clamp(NormalizedValue, 0.0f, 1.0f)));
    SubmitPreview();
}

void UDiscGolfCharacterCreatorWidget::SubmitPreview()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->PreviewCharacterCreatorDraft(DraftBody, DraftThrowStyle, DraftHandedness);
    }
}

FReply UDiscGolfCharacterCreatorWidget::HandleRightHanded()
{
    DraftHandedness = EDGHandedness::Right;
    SubmitPreview();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleLeftHanded()
{
    DraftHandedness = EDGHandedness::Left;
    SubmitPreview();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandlePreset(FName PresetId)
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGBodyProfile Body;
        FDGThrowStyle ThrowStyle;
        EDGHandedness Handedness = EDGHandedness::Right;
        if (Controller->LoadCharacterCreatorPreset(PresetId, Body, ThrowStyle, Handedness))
        {
            SetDraftProfile(Body, ThrowStyle, Handedness);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleReset()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGBodyProfile Body;
        FDGThrowStyle ThrowStyle;
        EDGHandedness Handedness = EDGHandedness::Right;
        if (Controller->ResetCharacterCreatorDraft(Body, ThrowStyle, Handedness))
        {
            SetDraftProfile(Body, ThrowStyle, Handedness);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleApply()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->ApplyCharacterCreatorDraft(DraftBody, DraftThrowStyle, DraftHandedness);
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleCancel()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->CancelCharacterCreator();
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleRotateLeft()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->RotateCharacterCreatorPreview(-20.0f);
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleRotateRight()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->RotateCharacterCreatorPreview(20.0f);
    }
    return FReply::Handled();
}

FText UDiscGolfCharacterCreatorWidget::GetStatusText() const
{
    const ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get();
    return FText::FromString(Controller
        ? Controller->GetCharacterCreatorStatusText()
        : FString(TEXT("Character creator controller unavailable.")));
}

FText UDiscGolfCharacterCreatorWidget::GetHandednessText() const
{
    return FText::FromString(DraftHandedness == EDGHandedness::Left
        ? TEXT("LEFT HANDED")
        : TEXT("RIGHT HANDED"));
}

FText UDiscGolfCharacterCreatorWidget::GetHandednessLimitationText() const
{
    return FText::FromString(DraftHandedness == EDGHandedness::Left
        ? TEXT("ANIMATED LHBH IS NOT AVAILABLE IN SESSION 4. GAMEPLAY USES THE EXISTING NON-ANIMATED FALLBACK; NO MIRRORED MONTAGE IS CLAIMED.")
        : TEXT("THE ACCEPTED ANIMATED RHBH PATH REMAINS ACTIVE. HANDEDNESS DOES NOT RECALCULATE POWER, SPIN, AIM, HYZER OR NOSE ANGLE."));
}

FSlateColor UDiscGolfCharacterCreatorWidget::GetHandednessLimitationColor() const
{
    return DraftHandedness == EDGHandedness::Left
        ? FSlateColor(DiscGolfCharacterCreatorStyle::Warning)
        : FSlateColor(DiscGolfCharacterCreatorStyle::Muted);
}
