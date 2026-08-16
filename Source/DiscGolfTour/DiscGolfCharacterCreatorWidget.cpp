#include "DiscGolfCharacterCreatorWidget.h"

#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfTourPlayerController.h"
#include "Framework/Application/SlateApplication.h"
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
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
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
    EDGHandedness InHandedness,
    const FDGOutfitLoadout& InOutfit)
{
    OwningDiscGolfController = InController;
    DraftOutfit = DiscGolfOutfitRuntime::NormalizeForPersistence(InOutfit);
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
    RebuildOutfitLists();
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

void UDiscGolfCharacterCreatorWidget::PrepareSession6VisualOutfitEvidence(
    EDGOutfitSlot OutfitSlot,
    const FDGOutfitLoadout& InOutfit)
{
    if (!DiscGolfOutfitRuntime::GetOrderedSlots().Contains(OutfitSlot))
    {
        OutfitSlot = EDGOutfitSlot::Top;
    }
    ActiveCreatorTabIndex = 1;
    SelectedOutfitSlot = OutfitSlot;
    DraftOutfit = DiscGolfOutfitRuntime::NormalizeForPersistence(InOutfit);
    PendingOutfitFocusRequest = EOutfitFocusRequest::CurrentItem;
    RebuildOutfitLists();
    InvalidateLayoutAndVolatility();
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
                            .Text(FText::FromString(TEXT("SESSION 6  //  ONE MASTER SKELETON  //  MODULAR OUTFIT PREVIEW")))
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
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("BODY & THROW")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleCreatorTab, 0)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("OUTFIT")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleCreatorTab, 1)
                            ]
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
                            SAssignNew(CreatorTabSwitcher, SWidgetSwitcher)
                            .WidgetIndex_Lambda([this]() { return ActiveCreatorTabIndex; })
                            + SWidgetSwitcher::Slot()
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
                            + SWidgetSwitcher::Slot()
                            [
                                BuildOutfitTab()
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

    RebuildOutfitLists();
    return RootSlateWidget.ToSharedRef();
}

void UDiscGolfCharacterCreatorWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootSlateWidget.Reset();
    InitialFocusButton.Reset();
    CreatorTabSwitcher.Reset();
    OutfitItemList.Reset();
    OutfitVariantList.Reset();
    OutfitCategoryButtons.Reset();
    OutfitItemButtons.Reset();
    OutfitVariantButtons.Reset();
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildOutfitTab()
{
    using namespace DiscGolfCharacterCreatorStyle;

    OutfitCategoryButtons.Reset();
    TSharedRef<SWrapBox> CategoryBox = SNew(SWrapBox).UseAllottedSize(true);
    for (EDGOutfitSlot OutfitSlot : DiscGolfOutfitRuntime::GetOrderedSlots())
    {
        TSharedPtr<SButton> CategoryButton;
        CategoryBox->AddSlot()
        .Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
        [
            SAssignNew(CategoryButton, SButton)
            .Text(DiscGolfOutfitRuntime::GetSlotDisplayName(OutfitSlot))
            .OnClicked_UObject(
                this,
                &UDiscGolfCharacterCreatorWidget::HandleOutfitSlot,
                OutfitSlot)
        ];
        OutfitCategoryButtons.Add(OutfitSlot, CategoryButton);
    }

    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("MODULAR OUTFIT  //  GENERIC PROXY CONTENT")))
                .ColorAndOpacity(Signal)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("All Session 6 wardrobe art is unbranded validation content and is not production-ready.")))
                .ColorAndOpacity(Warning)
                .AutoWrapText(true)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                CategoryBox
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 8.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text_Lambda([this]()
                {
                    return FText::Format(
                        FText::FromString(TEXT("{0} ITEMS")),
                        DiscGolfOutfitRuntime::GetSlotDisplayName(SelectedOutfitSlot));
                })
                .ColorAndOpacity(Paper)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(OutfitItemList, SVerticalBox)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 12.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("COLOR / MATERIAL VARIANT")))
                .ColorAndOpacity(Paper)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SAssignNew(OutfitVariantList, SVerticalBox)
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 14.0f, 0.0f, 0.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("RESET OUTFIT")))
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleResetOutfit)
                ]
                + SHorizontalBox::Slot()
                .AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("RANDOMIZE OUTFIT")))
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRandomizeOutfit)
                ]
            ]
        ];
}

void UDiscGolfCharacterCreatorWidget::RebuildOutfitLists()
{
    if (!OutfitItemList.IsValid() || !OutfitVariantList.IsValid())
    {
        return;
    }

    bool bHadFocusedItem = false;
    bool bHadFocusedVariant = false;
    FName PreviouslyFocusedItem = NAME_None;
    FName PreviouslyFocusedVariant = NAME_None;
    if (FSlateApplication::IsInitialized())
    {
        const TSharedPtr<SWidget> FocusedWidget =
            FSlateApplication::Get().GetKeyboardFocusedWidget();
        for (const TPair<FName, TSharedPtr<SButton>>& Pair : OutfitItemButtons)
        {
            if (Pair.Value.IsValid() && Pair.Value.Get() == FocusedWidget.Get())
            {
                bHadFocusedItem = true;
                PreviouslyFocusedItem = Pair.Key;
                break;
            }
        }
        for (const TPair<FName, TSharedPtr<SButton>>& Pair : OutfitVariantButtons)
        {
            if (Pair.Value.IsValid() && Pair.Value.Get() == FocusedWidget.Get())
            {
                bHadFocusedVariant = true;
                PreviouslyFocusedVariant = Pair.Key;
                break;
            }
        }
    }

    OutfitItemList->ClearChildren();
    OutfitVariantList->ClearChildren();
    OutfitItemButtons.Reset();
    OutfitVariantButtons.Reset();

    const FDGEquippedOutfitEntry* SelectedEntry =
        DiscGolfOutfitRuntime::FindEntryForSlot(DraftOutfit, SelectedOutfitSlot);
    const bool bNoneSelected = SelectedEntry == nullptr;
    TSharedPtr<SButton> NoneButton;
    OutfitItemList->AddSlot()
    .AutoHeight()
    .Padding(0.0f, 1.0f)
    [
        SAssignNew(NoneButton, SButton)
        .Text(FText::FromString(bNoneSelected ? TEXT("● NONE") : TEXT("NONE")))
        .OnClicked_UObject(
            this,
            &UDiscGolfCharacterCreatorWidget::HandleOutfitItem,
            FName())
    ];
    OutfitItemButtons.Add(NAME_None, NoneButton);

    const ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get();
    const TArray<FDiscGolfOutfitOption> Options = Controller
        ? Controller->GetCharacterCreatorOutfitOptions(SelectedOutfitSlot)
        : TArray<FDiscGolfOutfitOption>();
    for (const FDiscGolfOutfitOption& Option : Options)
    {
        const bool bSelected = SelectedEntry && SelectedEntry->ItemId == Option.ItemId;
        FString Label = bSelected ? TEXT("● ") : FString();
        Label += Option.DisplayName.ToString();
        if (!Option.bCompatible && !Option.CompatibilityReason.IsEmpty())
        {
            Label += TEXT("  [") + Option.CompatibilityReason + TEXT("]");
        }
        TSharedPtr<SButton> ItemButton;
        OutfitItemList->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 1.0f)
        [
            SAssignNew(ItemButton, SButton)
            .IsEnabled(Option.bCompatible)
            .Text(FText::FromString(Label))
            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleOutfitItem, Option.ItemId)
        ];
        OutfitItemButtons.Add(Option.ItemId, ItemButton);
    }

    if (!SelectedEntry)
    {
        OutfitVariantList->AddSlot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Select an item to choose a variant.")))
            .ColorAndOpacity(DiscGolfCharacterCreatorStyle::Muted)
        ];
    }
    else
    {
        const FDiscGolfOutfitOption* SelectedOption = Options.FindByPredicate(
            [SelectedEntry](const FDiscGolfOutfitOption& Option)
            {
                return Option.ItemId == SelectedEntry->ItemId;
            });
        if (!SelectedOption || SelectedOption->VariantIds.IsEmpty())
        {
            OutfitVariantList->AddSlot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("No valid variants are available.")))
                .ColorAndOpacity(DiscGolfCharacterCreatorStyle::Warning)
            ];
        }
        else
        {
            for (FName VariantId : SelectedOption->VariantIds)
            {
                const bool bSelected = SelectedEntry->VariantId == VariantId;
                TSharedPtr<SButton> VariantButton;
                OutfitVariantList->AddSlot()
                .AutoHeight()
                .Padding(0.0f, 1.0f)
                [
                    SAssignNew(VariantButton, SButton)
                    .Text(FText::FromString(
                        FString(bSelected ? TEXT("● ") : TEXT("")) + VariantId.ToString()))
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleOutfitVariant, VariantId)
                ];
                OutfitVariantButtons.Add(VariantId, VariantButton);
            }
        }
    }

    const auto FindItemButton = [this](FName ItemId) -> TSharedPtr<SButton>
    {
        const TSharedPtr<SButton>* Found = OutfitItemButtons.Find(ItemId);
        return Found ? *Found : TSharedPtr<SButton>();
    };
    const auto FindVariantButton = [this](FName VariantId) -> TSharedPtr<SButton>
    {
        const TSharedPtr<SButton>* Found = OutfitVariantButtons.Find(VariantId);
        return Found ? *Found : TSharedPtr<SButton>();
    };

    const EOutfitFocusRequest FocusRequest = PendingOutfitFocusRequest;
    TSharedPtr<SButton> FocusTarget;
    switch (FocusRequest)
    {
        case EOutfitFocusRequest::Category:
            if (const TSharedPtr<SButton>* Found = OutfitCategoryButtons.Find(SelectedOutfitSlot))
            {
                FocusTarget = *Found;
            }
            break;
        case EOutfitFocusRequest::CurrentItem:
            FocusTarget = FindItemButton(SelectedEntry ? SelectedEntry->ItemId : NAME_None);
            break;
        case EOutfitFocusRequest::CurrentVariant:
            if (SelectedEntry)
            {
                FocusTarget = FindVariantButton(SelectedEntry->VariantId);
            }
            break;
        case EOutfitFocusRequest::Preserve:
        default:
            if (bHadFocusedItem)
            {
                FocusTarget = FindItemButton(PreviouslyFocusedItem);
            }
            else if (bHadFocusedVariant)
            {
                FocusTarget = FindVariantButton(PreviouslyFocusedVariant);
            }
            break;
    }
    PendingOutfitFocusRequest = EOutfitFocusRequest::Preserve;

    if (!FocusTarget.IsValid()
        && (FocusRequest == EOutfitFocusRequest::CurrentVariant
            || bHadFocusedItem || bHadFocusedVariant))
    {
        FocusTarget = FindItemButton(SelectedEntry ? SelectedEntry->ItemId : NAME_None);
    }
    const bool bMustRestoreDynamicFocus =
        FocusRequest == EOutfitFocusRequest::CurrentItem
        || FocusRequest == EOutfitFocusRequest::CurrentVariant
        || bHadFocusedItem
        || bHadFocusedVariant;
    if (bMustRestoreDynamicFocus
        && (!FocusTarget.IsValid() || !FocusTarget->IsEnabled()))
    {
        FocusTarget = FindItemButton(NAME_None);
    }
    if (FocusTarget.IsValid() && FocusTarget->IsEnabled()
        && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetKeyboardFocus(FocusTarget, EFocusCause::Navigation);
    }
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
    RebuildOutfitLists();
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

FReply UDiscGolfCharacterCreatorWidget::HandleCreatorTab(int32 TabIndex)
{
    ActiveCreatorTabIndex = FMath::Clamp(TabIndex, 0, 1);
    if (ActiveCreatorTabIndex == 1)
    {
        RebuildOutfitLists();
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleOutfitSlot(EDGOutfitSlot OutfitSlot)
{
    SelectedOutfitSlot = OutfitSlot;
    PendingOutfitFocusRequest = EOutfitFocusRequest::Category;
    RebuildOutfitLists();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleOutfitItem(FName ItemId)
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FName VariantId = NAME_None;
        if (!ItemId.IsNone())
        {
            const TArray<FDiscGolfOutfitOption> Options =
                Controller->GetCharacterCreatorOutfitOptions(SelectedOutfitSlot);
            if (const FDiscGolfOutfitOption* Option = Options.FindByPredicate(
                    [ItemId](const FDiscGolfOutfitOption& Candidate)
                    {
                        return Candidate.ItemId == ItemId;
                    }))
            {
                VariantId = Option->VariantIds.Contains(FName(TEXT("Default")))
                    ? FName(TEXT("Default"))
                    : (Option->VariantIds.IsEmpty() ? NAME_None : Option->VariantIds[0]);
            }
        }

        FDGOutfitLoadout Updated;
        if (Controller->PreviewCharacterCreatorOutfitSelection(
                SelectedOutfitSlot, ItemId, VariantId, Updated))
        {
            DraftOutfit = Updated;
            PendingOutfitFocusRequest = EOutfitFocusRequest::CurrentItem;
        }
    }
    RebuildOutfitLists();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleOutfitVariant(FName VariantId)
{
    const FDGEquippedOutfitEntry* Entry =
        DiscGolfOutfitRuntime::FindEntryForSlot(DraftOutfit, SelectedOutfitSlot);
    if (!Entry)
    {
        return FReply::Handled();
    }

    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGOutfitLoadout Updated;
        if (Controller->PreviewCharacterCreatorOutfitSelection(
                SelectedOutfitSlot, Entry->ItemId, VariantId, Updated))
        {
            DraftOutfit = Updated;
            PendingOutfitFocusRequest = EOutfitFocusRequest::CurrentVariant;
        }
    }
    RebuildOutfitLists();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleResetOutfit()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGOutfitLoadout Updated;
        if (Controller->ResetCharacterCreatorOutfit(Updated))
        {
            DraftOutfit = Updated;
        }
    }
    RebuildOutfitLists();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleRandomizeOutfit()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGOutfitLoadout Updated;
        if (Controller->RandomizeCharacterCreatorOutfit(Updated))
        {
            DraftOutfit = Updated;
        }
    }
    RebuildOutfitLists();
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
