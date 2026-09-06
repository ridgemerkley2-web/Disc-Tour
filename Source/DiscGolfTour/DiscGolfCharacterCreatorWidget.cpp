#include "DiscGolfCharacterCreatorWidget.h"

#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfTourPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
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
    DraftCustomization = DiscGolfFullCharacterRuntime::MakeDefaultCustomization();
    DraftCustomization.Body = InBody;
    DraftCustomization.ThrowStyle = InThrowStyle;
    DraftCustomization.Identity.Handedness = InHandedness;
    DraftCustomization.Outfit = DiscGolfOutfitRuntime::NormalizeForPersistence(InOutfit);
    SetDraftCustomization(DraftCustomization);
}

void UDiscGolfCharacterCreatorWidget::InitializeFullCreator(
    ADiscGolfTourPlayerController* InController,
    const FDGFullCharacterCustomization& InCharacter)
{
    OwningDiscGolfController = InController;
    SetDraftCustomization(InCharacter);
}

void UDiscGolfCharacterCreatorWidget::SetDraftProfile(
    const FDGBodyProfile& InBody,
    const FDGThrowStyle& InThrowStyle,
    EDGHandedness InHandedness)
{
    DraftBody = InBody;
    DraftThrowStyle = InThrowStyle;
    DraftHandedness = InHandedness;
    SyncLegacyDraftSlices();
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

void UDiscGolfCharacterCreatorWidget::SetDraftCustomization(
    const FDGFullCharacterCustomization& InCharacter)
{
    DraftCustomization = InCharacter;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(DraftCustomization);
    DraftBody = DraftCustomization.Body;
    DraftThrowStyle = DraftCustomization.ThrowStyle;
    DraftHandedness = DraftCustomization.Identity.Handedness;
    DraftOutfit = DraftCustomization.Outfit;
    RebuildOutfitLists();
    InvalidateLayoutAndVolatility();
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
    ActiveCreatorTabIndex = 6;
    SelectedOutfitSlot = OutfitSlot;
    DraftOutfit = DiscGolfOutfitRuntime::NormalizeForPersistence(InOutfit);
    DraftCustomization.Outfit = DraftOutfit;
    PendingOutfitFocusRequest = EOutfitFocusRequest::CurrentItem;
    RebuildOutfitLists();
    InvalidateLayoutAndVolatility();
}

void UDiscGolfCharacterCreatorWidget::PrepareSession7VisualEvidence(
    int32 TabIndex,
    const FDGFullCharacterCustomization& InCharacter)
{
    SetDraftCustomization(InCharacter);
    ActiveCreatorTabIndex = FMath::Clamp(TabIndex, 0, 6);
    if (ActiveCreatorTabIndex == 6)
    {
        PendingOutfitFocusRequest = EOutfitFocusRequest::CurrentItem;
        RebuildOutfitLists();
    }
    if (FSlateApplication::IsInitialized())
    {
        if (const TSharedPtr<SButton>* TabButton = CreatorTabButtons.Find(ActiveCreatorTabIndex))
        {
            if (TabButton->IsValid())
            {
                FSlateApplication::Get().SetKeyboardFocus(*TabButton, EFocusCause::SetDirectly);
            }
        }
    }
    InvalidateLayoutAndVolatility();
}

void UDiscGolfCharacterCreatorWidget::GetSession7VisibleControlIds(
    TArray<FString>& OutControlIds) const
{
    OutControlIds.Reset();
    switch (FMath::Clamp(ActiveCreatorTabIndex, 0, 6))
    {
        case 0:
            OutControlIds = {
                TEXT("DisplayName"), TEXT("Handedness"), TEXT("Voice"),
                TEXT("Pronouns") };
            break;
        case 1:
            OutControlIds = {
                TEXT("Height"), TEXT("Wingspan"), TEXT("ShoulderWidth"),
                TEXT("TorsoLength"), TEXT("LegLength"), TEXT("HandScale"),
                TEXT("Mass"), TEXT("Muscularity"), TEXT("BodyFat"),
                TEXT("Chest"), TEXT("Waist"), TEXT("Hips"), TEXT("Arms"),
                TEXT("Legs"), TEXT("BodyPresets") };
            break;
        case 2:
            OutControlIds = {
                TEXT("FacePresets"), TEXT("HeadWidth"), TEXT("HeadHeight"),
                TEXT("BrowHeight"), TEXT("BrowDepth"), TEXT("EyeSize"),
                TEXT("EyeSpacing"), TEXT("EyeDepth"), TEXT("NoseWidth"),
                TEXT("NoseLength"), TEXT("NoseBridge"), TEXT("CheekWidth"),
                TEXT("CheekFullness"), TEXT("JawWidth"), TEXT("JawHeight"),
                TEXT("ChinWidth"), TEXT("ChinLength"), TEXT("MouthWidth"),
                TEXT("LipFullness"), TEXT("EarSize"), TEXT("EarAngle") };
            break;
        case 3:
            OutControlIds = {
                TEXT("HairStyle"), TEXT("FacialHair"), TEXT("Eyebrow"),
                TEXT("HairColor"), TEXT("FacialHairColor"),
                TEXT("EyebrowColor") };
            break;
        case 4:
            OutControlIds = {
                TEXT("SkinTone"), TEXT("EyeColor"), TEXT("Complexion"),
                TEXT("Freckles"), TEXT("SunExposure"), TEXT("Scar"),
                TEXT("Tattoo") };
            break;
        case 5:
            OutControlIds = {
                TEXT("RunUp"), TEXT("ReachBack"), TEXT("TorsoRotation"),
                TEXT("Brace"), TEXT("Explosiveness"), TEXT("FollowThrough") };
            break;
        case 6:
            OutControlIds = {
                TEXT("Headwear"), TEXT("Eyewear"), TEXT("Top"),
                TEXT("Outerwear"), TEXT("Bottom"), TEXT("Socks"),
                TEXT("Footwear"), TEXT("Glove"), TEXT("Wrist"), TEXT("Bag"),
                TEXT("Accessory"), TEXT("Item"), TEXT("Variant") };
            break;
        default:
            break;
    }
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::RebuildWidget()
{
    using namespace DiscGolfCharacterCreatorStyle;

    CreatorTabButtons.Reset();
    const TArray<FString> TabLabels = {
        TEXT("IDENTITY"), TEXT("BODY"), TEXT("FACE"), TEXT("HAIR"),
        TEXT("APPEARANCE"), TEXT("THROW STYLE"), TEXT("OUTFIT") };
    TSharedRef<SWrapBox> TabBar = SNew(SWrapBox).UseAllottedSize(true);
    for (int32 Index = 0; Index < TabLabels.Num(); ++Index)
    {
        TSharedPtr<SButton> TabButton;
        TabBar->AddSlot()
        .Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
        [
            SAssignNew(TabButton, SButton)
            .Text_Lambda([this, Index, Label = TabLabels[Index]]()
            {
                return FText::FromString(
                    FString(ActiveCreatorTabIndex == Index ? TEXT("● ") : TEXT(""))
                    + Label);
            })
            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleCreatorTab, Index)
        ];
        CreatorTabButtons.Add(Index, TabButton);
        if (Index == 0)
        {
            InitialFocusButton = TabButton;
        }
    }

    TSharedRef<SHorizontalBox> BackendSelector =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(SButton)
            .Text_Lambda([this]()
            {
                return FText::FromString(IsDGMasterBackendSelected()
                    ? TEXT("● DGMASTER / PROXY")
                    : TEXT("DGMASTER / PROXY"));
            })
            .OnClicked_UObject(
                this,
                &UDiscGolfCharacterCreatorWidget::HandleBackendSelection,
                FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId))
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            SNew(SButton)
            .Text_UObject(
                this,
                &UDiscGolfCharacterCreatorWidget::GetMetaHumanBackendButtonText)
            .IsEnabled_Lambda([this]()
            {
                const ADiscGolfTourPlayerController* Controller =
                    OwningDiscGolfController.Get();
                return Controller
                    && Controller->IsCharacterCreatorMetaHumanBackendAvailable();
            })
            .OnClicked_UObject(
                this,
                &UDiscGolfCharacterCreatorWidget::HandleBackendSelection,
                FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId))
        ]
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text_UObject(
                this,
                &UDiscGolfCharacterCreatorWidget::GetBackendStatusText)
            .ColorAndOpacity(Muted)
            .AutoWrapText(true)
        ];

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
                .WidthOverride(900.0f)
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
                            .Text(FText::FromString(TEXT("SESSION 8B  //  OPTIONAL VISUAL BACKEND  //  ONE EXISTING PLAYER")))
                            .ColorAndOpacity(Muted)
                            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            BackendSelector
                        ]
                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, 0.0f, 0.0f, 12.0f)
                        [
                            TabBar
                        ]
                        + SVerticalBox::Slot()
                        .FillHeight(1.0f)
                        [
                            SAssignNew(CreatorTabSwitcher, SWidgetSwitcher)
                            .WidgetIndex_Lambda([this]() { return ActiveCreatorTabIndex; })
                            + SWidgetSwitcher::Slot()
                            [
                                BuildIdentityTab()
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                BuildBackendAwareProxyTab(BuildBodyTab())
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                BuildBackendAwareProxyTab(BuildFaceTab())
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                BuildBackendAwareProxyTab(BuildHairTab())
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                BuildBackendAwareProxyTab(BuildAppearanceTab())
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                BuildThrowStyleTab()
                            ]
                            + SWidgetSwitcher::Slot()
                            [
                                BuildBackendAwareProxyTab(BuildOutfitTab())
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
                                .Text_UObject(this, &UDiscGolfCharacterCreatorWidget::GetCurrentLockText)
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleToggleCurrentCategoryLock)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("RESET CURRENT TAB")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleResetCurrentTab)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("RESET ALL")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleResetAll)
                            ]
                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                            [
                                SNew(SButton)
                                .Text(FText::FromString(TEXT("RANDOMIZE")))
                                .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRandomize)
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
                                .Text(FText::FromString(TEXT("APPLY / SAVE & CONTINUE")))
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
                    .Padding(0.0f, 0.0f, 0.0f, 14.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("PROXY / DO NOT SHIP. Five face targets are visibly prepared; fifteen remain data/UI mapped for production art.")))
                        .ColorAndOpacity(Warning)
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
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("ROTATE RIGHT")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRotateRight)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(0.0f, 0.0f, 8.0f, 0.0f)
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("ZOOM IN")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleZoomIn)
                        ]
                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SButton)
                            .Text(FText::FromString(TEXT("ZOOM OUT")))
                            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleZoomOut)
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

TSharedRef<SWidget>
UDiscGolfCharacterCreatorWidget::BuildBackendAwareProxyTab(
    const TSharedRef<SWidget>& ProxyTab)
{
    using namespace DiscGolfCharacterCreatorStyle;
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 10.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(
                TEXT("CURATED REALISTIC PRESET: proxy-only controls are preserved for DGMaster fallback and are intentionally disabled for this backend.")))
            .ColorAndOpacity(Warning)
            .AutoWrapText(true)
            .Visibility_Lambda([this]()
            {
                return IsDGMasterBackendSelected()
                    ? EVisibility::Collapsed : EVisibility::Visible;
            })
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SBox)
            .IsEnabled_Lambda([this]()
            {
                return IsDGMasterBackendSelected();
            })
            [
                ProxyTab
            ]
        ];
}

void UDiscGolfCharacterCreatorWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootSlateWidget.Reset();
    InitialFocusButton.Reset();
    CreatorTabSwitcher.Reset();
    CreatorTabButtons.Reset();
    OutfitItemList.Reset();
    OutfitVariantList.Reset();
    OutfitCategoryButtons.Reset();
    OutfitItemButtons.Reset();
    OutfitVariantButtons.Reset();
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildIdentityTab()
{
    using namespace DiscGolfCharacterCreatorStyle;
    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("IDENTITY")))
                .ColorAndOpacity(Signal)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("DISPLAY NAME  //  32 CHARACTERS MAX")))
                .ColorAndOpacity(Paper)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 12.0f)
            [
                SNew(SEditableTextBox)
                .Text_Lambda([this]()
                {
                    return FText::FromString(DraftCustomization.Identity.DisplayName);
                })
                .OnTextCommitted_UObject(
                    this,
                    &UDiscGolfCharacterCreatorWidget::HandleDisplayNameCommitted)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("HANDEDNESS")))
                .ColorAndOpacity(Paper)
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f, 0.0f, 12.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text_Lambda([this]()
                    {
                        return FText::FromString(
                            DraftCustomization.Identity.Handedness == EDGHandedness::Right
                                ? TEXT("● RIGHT") : TEXT("RIGHT"));
                    })
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleRightHanded)
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text_Lambda([this]()
                    {
                        return FText::FromString(
                            DraftCustomization.Identity.Handedness == EDGHandedness::Left
                                ? TEXT("● LEFT") : TEXT("LEFT"));
                    })
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleLeftHanded)
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                BuildCosmeticOptionGroup(
                    FText::FromString(TEXT("VOICE ID  //  DATA HOOK ONLY")),
                    EDGCosmeticKind::Voice)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                BuildCosmeticOptionGroup(
                    FText::FromString(TEXT("PRONOUN SET")),
                    EDGCosmeticKind::PronounSet)
            ]
        ];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildBodyTab()
{
    using namespace DiscGolfCharacterCreatorStyle;
    return SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("BODY  //  ACCEPTED CONTROL RIG LIMITS")))
                .ColorAndOpacity(Signal)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("BASELINE")))
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandlePreset, FName(TEXT("Baseline")))
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("SHORT COMPACT")))
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandlePreset, FName(TEXT("ShortCompact")))
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("TALL / LONG ARMS")))
                    .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandlePreset, FName(TEXT("TallLongArms")))
                ]
            ]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("HEIGHT")), ECreatorField::Height, 150.0f, 210.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("WINGSPAN")), ECreatorField::Wingspan, 0.92f, 1.08f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("SHOULDER WIDTH")), ECreatorField::ShoulderWidth, 0.92f, 1.08f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("TORSO LENGTH")), ECreatorField::TorsoLength, 0.94f, 1.06f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("LEG LENGTH")), ECreatorField::LegLength, 0.94f, 1.06f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("HAND SIZE")), ECreatorField::HandScale, 0.94f, 1.06f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("BODY MASS")), ECreatorField::Mass, 45.0f, 160.0f)]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("BODY BUILD")))
                .ColorAndOpacity(Paper)
                .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
            ]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("MUSCULARITY")), ECreatorField::Muscularity, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("BODY FAT")), ECreatorField::BodyFat, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("CHEST")), ECreatorField::Chest, -1.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("WAIST")), ECreatorField::Waist, -1.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("HIPS")), ECreatorField::Hips, -1.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("ARMS")), ECreatorField::Arms, -1.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("LEGS")), ECreatorField::Legs, -1.0f, 1.0f)]
        ];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildFaceTab()
{
    using namespace DiscGolfCharacterCreatorStyle;
    TSharedRef<SVerticalBox> Controls = SNew(SVerticalBox);
    Controls->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 5.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("FACE  //  20-CHANNEL CONTRACT")))
        .ColorAndOpacity(Signal)
        .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
    ];
    Controls->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("Five channels visibly deform this generated proxy. The other fifteen are persisted and UI-mapped for later production art.")))
        .ColorAndOpacity(Warning)
        .AutoWrapText(true)
    ];
    Controls->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(SButton).Text(FText::FromString(TEXT("DEFAULT")))
            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleFacePreset, FName(TEXT("face_default")))
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(SButton).Text(FText::FromString(TEXT("SQUARE")))
            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleFacePreset, FName(TEXT("face_square")))
        ]
        + SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(SButton).Text(FText::FromString(TEXT("NARROW")))
            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleFacePreset, FName(TEXT("face_narrow")))
        ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SButton).Text(FText::FromString(TEXT("ROUND")))
            .OnClicked_UObject(this, &UDiscGolfCharacterCreatorWidget::HandleFacePreset, FName(TEXT("face_round")))
        ]
    ];
    for (FName MorphKey : DiscGolfFullCharacterRuntime::GetFaceMorphKeys())
    {
        Controls->AddSlot().AutoHeight()[BuildFaceSliderRow(MorphKey)];
    }
    return SNew(SScrollBox) + SScrollBox::Slot()[Controls];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildHairTab()
{
    using namespace DiscGolfCharacterCreatorStyle;
    static const TArray<FLinearColor> HairColors = {
        FLinearColor(0.03f, 0.02f, 0.015f), FLinearColor(0.16f, 0.07f, 0.025f),
        FLinearColor(0.42f, 0.22f, 0.08f), FLinearColor(0.72f, 0.55f, 0.30f),
        FLinearColor(0.56f, 0.08f, 0.03f), FLinearColor(0.58f, 0.58f, 0.60f) };
    return SNew(SScrollBox) + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("HAIR  //  GENERATED PROXY ATTACHMENTS")))
                .ColorAndOpacity(Signal).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]
            + SVerticalBox::Slot().AutoHeight()[BuildCosmeticOptionGroup(FText::FromString(TEXT("HAIR STYLE")), EDGCosmeticKind::Hair)]
            + SVerticalBox::Slot().AutoHeight()[BuildColorSwatches(FText::FromString(TEXT("HAIR COLOR")), EColorField::Hair, HairColors)]
            + SVerticalBox::Slot().AutoHeight()[BuildCosmeticOptionGroup(FText::FromString(TEXT("FACIAL HAIR")), EDGCosmeticKind::FacialHair)]
            + SVerticalBox::Slot().AutoHeight()[BuildColorSwatches(FText::FromString(TEXT("FACIAL HAIR COLOR")), EColorField::FacialHair, HairColors)]
            + SVerticalBox::Slot().AutoHeight()[BuildCosmeticOptionGroup(FText::FromString(TEXT("EYEBROWS")), EDGCosmeticKind::Eyebrow)]
            + SVerticalBox::Slot().AutoHeight()[BuildColorSwatches(FText::FromString(TEXT("EYEBROW COLOR")), EColorField::Eyebrow, HairColors)]
        ];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildAppearanceTab()
{
    using namespace DiscGolfCharacterCreatorStyle;
    static const TArray<FLinearColor> SkinColors = {
        FLinearColor(0.94f, 0.73f, 0.57f), FLinearColor(0.76f, 0.51f, 0.35f),
        FLinearColor(0.55f, 0.35f, 0.24f), FLinearColor(0.34f, 0.20f, 0.13f),
        FLinearColor(0.17f, 0.09f, 0.055f) };
    static const TArray<FLinearColor> EyeColors = {
        FLinearColor(0.12f, 0.22f, 0.17f), FLinearColor(0.10f, 0.22f, 0.38f),
        FLinearColor(0.27f, 0.16f, 0.08f), FLinearColor(0.38f, 0.34f, 0.15f),
        FLinearColor(0.42f, 0.44f, 0.46f) };
    return SNew(SScrollBox) + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("APPEARANCE  //  HEAD PROXY MATERIAL")))
                .ColorAndOpacity(Signal).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("The accepted master body has no editable surface slot. Skin and eye swatches are visibly proven on the modular head only.")))
                .ColorAndOpacity(Warning).AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight()[BuildColorSwatches(FText::FromString(TEXT("SKIN TONE")), EColorField::Skin, SkinColors)]
            + SVerticalBox::Slot().AutoHeight()[BuildColorSwatches(FText::FromString(TEXT("EYE COLOR")), EColorField::Eye, EyeColors)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("COMPLEXION")), ECreatorField::Complexion, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("FRECKLES")), ECreatorField::Freckles, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("SUN EXPOSURE")), ECreatorField::SunExposure, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildCosmeticOptionGroup(FText::FromString(TEXT("SCAR HOOK")), EDGCosmeticKind::Scar)]
            + SVerticalBox::Slot().AutoHeight()[BuildCosmeticOptionGroup(FText::FromString(TEXT("TATTOO HOOK")), EDGCosmeticKind::Tattoo)]
        ];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildThrowStyleTab()
{
    using namespace DiscGolfCharacterCreatorStyle;
    return SNew(SScrollBox) + SScrollBox::Slot()
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("THROW STYLE  //  PRESENTATION ONLY")))
                .ColorAndOpacity(Signal).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(TEXT("These values shape animation presentation. Power, spin, aim, release, disc physics and flight remain authoritative gameplay systems.")))
                .ColorAndOpacity(Warning).AutoWrapText(true)
            ]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("RUN-UP INTENSITY")), ECreatorField::RunUp, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("REACHBACK")), ECreatorField::ReachBack, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("TORSO ROTATION")), ECreatorField::TorsoRotation, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("BRACE INTENSITY")), ECreatorField::Brace, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("EXPLOSIVENESS")), ECreatorField::Explosiveness, 0.0f, 1.0f)]
            + SVerticalBox::Slot().AutoHeight()[BuildSliderRow(FText::FromString(TEXT("FOLLOW-THROUGH")), ECreatorField::FollowThrough, 0.0f, 1.0f)]
        ];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildFaceSliderRow(FName MorphKey)
{
    const TWeakObjectPtr<UDiscGolfCharacterCreatorWidget> WeakThis(this);
    const bool bProxyVisible =
        DiscGolfFullCharacterRuntime::GetVisibleProxyFaceMorphKeys().Contains(MorphKey);
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Center).Padding(0.0f, 4.0f, 12.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text(FText::Format(
                FText::FromString(bProxyVisible ? TEXT("{0}  [PROXY VISIBLE]") : TEXT("{0}  [DATA / UI]")),
                DiscGolfFullCharacterRuntime::GetFaceMorphDisplayName(MorphKey)))
            .ColorAndOpacity(bProxyVisible
                ? DiscGolfCharacterCreatorStyle::Paper
                : DiscGolfCharacterCreatorStyle::Muted)
        ]
        + SHorizontalBox::Slot().FillWidth(0.46f).VAlign(VAlign_Center).Padding(0.0f, 4.0f)
        [
            SNew(SSlider)
            .Value_Lambda([WeakThis, MorphKey]()
            {
                const UDiscGolfCharacterCreatorWidget* Self = WeakThis.Get();
                const float* Value = Self
                    ? Self->DraftCustomization.Face.MorphValues.Find(MorphKey) : nullptr;
                return (FMath::Clamp(Value ? *Value : 0.0f, -1.0f, 1.0f) + 1.0f) * 0.5f;
            })
            .OnValueChanged_Lambda([WeakThis, MorphKey](float NormalizedValue)
            {
                if (UDiscGolfCharacterCreatorWidget* Self = WeakThis.Get())
                {
                    Self->HandleFaceSliderChanged(NormalizedValue, MorphKey);
                }
            })
        ]
        + SHorizontalBox::Slot().FillWidth(0.12f).HAlign(HAlign_Right).VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text_Lambda([WeakThis, MorphKey]()
            {
                const UDiscGolfCharacterCreatorWidget* Self = WeakThis.Get();
                const float* Value = Self
                    ? Self->DraftCustomization.Face.MorphValues.Find(MorphKey) : nullptr;
                return FText::FromString(FString::Printf(TEXT("%+.2f"), Value ? *Value : 0.0f));
            })
            .ColorAndOpacity(DiscGolfCharacterCreatorStyle::Signal)
        ];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildCosmeticOptionGroup(
    const FText& Label,
    EDGCosmeticKind Kind)
{
    using namespace DiscGolfCharacterCreatorStyle;
    TSharedRef<SWrapBox> OptionsBox = SNew(SWrapBox).UseAllottedSize(true);
    const ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get();
    const TArray<FDiscGolfCosmeticOption> Options = Controller
        ? Controller->GetCharacterCreatorCosmeticOptions(Kind)
        : TArray<FDiscGolfCosmeticOption>();
    for (const FDiscGolfCosmeticOption& Option : Options)
    {
        OptionsBox->AddSlot().Padding(FMargin(0.0f, 0.0f, 6.0f, 6.0f))
        [
            SNew(SButton)
            .IsEnabled(Option.bCompatible)
            .Text_Lambda([this, Kind, ItemId = Option.ItemId, Name = Option.DisplayName]()
            {
                const FString Prefix = GetSelectedCosmeticId(Kind) == ItemId
                    ? TEXT("● ") : TEXT("");
                return FText::FromString(Prefix + Name.ToString());
            })
            .ToolTipText(FText::FromString(Option.CompatibilityReason))
            .OnClicked_UObject(
                this,
                &UDiscGolfCharacterCreatorWidget::HandleCosmeticSelection,
                Kind,
                Option.ItemId)
        ];
    }
    if (Options.IsEmpty())
    {
        OptionsBox->AddSlot()
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Catalog option unavailable; safe default retained.")))
            .ColorAndOpacity(Warning)
        ];
    }
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock).Text(Label).ColorAndOpacity(Paper)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
        ]
        + SVerticalBox::Slot().AutoHeight()[OptionsBox];
}

TSharedRef<SWidget> UDiscGolfCharacterCreatorWidget::BuildColorSwatches(
    const FText& Label,
    EColorField Field,
    TConstArrayView<FLinearColor> Colors)
{
    using namespace DiscGolfCharacterCreatorStyle;
    TSharedRef<SHorizontalBox> Swatches = SNew(SHorizontalBox);
    for (const FLinearColor Color : Colors)
    {
        Swatches->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            SNew(SButton)
            .ContentPadding(FMargin(3.0f))
            .ToolTipText(FText::FromString(Color.ToFColor(true).ToHex()))
            .OnClicked_Lambda([this, Field, Color]()
            {
                return HandleColorSelection(Field, Color);
            })
            [
                SNew(SBox).WidthOverride(42.0f).HeightOverride(26.0f)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(Color)
                ]
            ]
        ];
    }
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this, Label, Field]()
            {
                FLinearColor Current = FLinearColor::Black;
                switch (Field)
                {
                    case EColorField::Hair: Current = DraftCustomization.Hair.HairColor; break;
                    case EColorField::FacialHair: Current = DraftCustomization.Hair.FacialHairColor; break;
                    case EColorField::Eyebrow: Current = DraftCustomization.Hair.EyebrowColor; break;
                    case EColorField::Skin: Current = DraftCustomization.Appearance.SkinTone; break;
                    case EColorField::Eye: Current = DraftCustomization.Appearance.EyeColor; break;
                    default: break;
                }
                return FText::Format(
                    FText::FromString(TEXT("{0}  //  SELECTED #{1}")),
                    Label,
                    FText::FromString(Current.ToFColor(true).ToHex()));
            })
            .ColorAndOpacity(Paper)
        ]
        + SVerticalBox::Slot().AutoHeight()[Swatches];
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
        case ECreatorField::Muscularity: return DraftCustomization.BodyBuild.Muscularity;
        case ECreatorField::BodyFat: return DraftCustomization.BodyBuild.BodyFat;
        case ECreatorField::Chest: return DraftCustomization.BodyBuild.Chest;
        case ECreatorField::Waist: return DraftCustomization.BodyBuild.Waist;
        case ECreatorField::Hips: return DraftCustomization.BodyBuild.Hips;
        case ECreatorField::Arms: return DraftCustomization.BodyBuild.Arms;
        case ECreatorField::Legs: return DraftCustomization.BodyBuild.Legs;
        case ECreatorField::RunUp: return DraftThrowStyle.RunUpIntensity;
        case ECreatorField::ReachBack: return DraftThrowStyle.ReachBackAmount;
        case ECreatorField::TorsoRotation: return DraftThrowStyle.TorsoRotation;
        case ECreatorField::Brace: return DraftThrowStyle.BraceIntensity;
        case ECreatorField::Explosiveness: return DraftThrowStyle.Explosiveness;
        case ECreatorField::FollowThrough: return DraftThrowStyle.FollowThrough;
        case ECreatorField::Complexion: return DraftCustomization.Appearance.Complexion;
        case ECreatorField::Freckles: return DraftCustomization.Appearance.Freckles;
        case ECreatorField::SunExposure: return DraftCustomization.Appearance.SunExposure;
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
        case ECreatorField::Muscularity: DraftCustomization.BodyBuild.Muscularity = Value; break;
        case ECreatorField::BodyFat: DraftCustomization.BodyBuild.BodyFat = Value; break;
        case ECreatorField::Chest: DraftCustomization.BodyBuild.Chest = Value; break;
        case ECreatorField::Waist: DraftCustomization.BodyBuild.Waist = Value; break;
        case ECreatorField::Hips: DraftCustomization.BodyBuild.Hips = Value; break;
        case ECreatorField::Arms: DraftCustomization.BodyBuild.Arms = Value; break;
        case ECreatorField::Legs: DraftCustomization.BodyBuild.Legs = Value; break;
        case ECreatorField::RunUp: DraftThrowStyle.RunUpIntensity = Value; break;
        case ECreatorField::ReachBack: DraftThrowStyle.ReachBackAmount = Value; break;
        case ECreatorField::TorsoRotation: DraftThrowStyle.TorsoRotation = Value; break;
        case ECreatorField::Brace: DraftThrowStyle.BraceIntensity = Value; break;
        case ECreatorField::Explosiveness: DraftThrowStyle.Explosiveness = Value; break;
        case ECreatorField::FollowThrough: DraftThrowStyle.FollowThrough = Value; break;
        case ECreatorField::Complexion: DraftCustomization.Appearance.Complexion = Value; break;
        case ECreatorField::Freckles: DraftCustomization.Appearance.Freckles = Value; break;
        case ECreatorField::SunExposure: DraftCustomization.Appearance.SunExposure = Value; break;
        default: break;
    }
    SyncLegacyDraftSlices();
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
    SyncLegacyDraftSlices();
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        if (Controller->PreviewFullCharacterCreatorDraft(DraftCustomization))
        {
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
        }
    }
    RebuildOutfitLists();
}

void UDiscGolfCharacterCreatorWidget::SyncLegacyDraftSlices()
{
    DraftCustomization.Body = DraftBody;
    DraftCustomization.ThrowStyle = DraftThrowStyle;
    DraftCustomization.Identity.Handedness = DraftHandedness;
    DraftCustomization.Outfit = DraftOutfit;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(DraftCustomization);
    DraftBody = DraftCustomization.Body;
    DraftThrowStyle = DraftCustomization.ThrowStyle;
    DraftHandedness = DraftCustomization.Identity.Handedness;
    DraftOutfit = DraftCustomization.Outfit;
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
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
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
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleResetCurrentTab()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGFullCharacterCustomization Updated;
        if (Controller->ResetCharacterCreatorCurrentTab(
                ActiveCreatorTabIndex, Updated))
        {
            SetDraftCustomization(Updated);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleResetAll()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGFullCharacterCustomization Updated;
        if (Controller->ResetCharacterCreatorAll(Updated))
        {
            SetDraftCustomization(Updated);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleRandomize()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGFullCharacterCustomization Updated;
        if (Controller->RandomizeCharacterCreatorDraft(RandomizeLocks, Updated))
        {
            SetDraftCustomization(Updated);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleToggleCurrentCategoryLock()
{
    SetCurrentCategoryLocked(!IsCurrentCategoryLocked());
    InvalidateLayoutAndVolatility();
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleApply()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        SyncLegacyDraftSlices();
        Controller->ApplyFullCharacterCreatorDraft(DraftCustomization);
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
    ActiveCreatorTabIndex = FMath::Clamp(TabIndex, 0, 6);
    if (ActiveCreatorTabIndex == 6)
    {
        RebuildOutfitLists();
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleBackendSelection(FName BackendId)
{
    if (ADiscGolfTourPlayerController* Controller =
            OwningDiscGolfController.Get())
    {
        FDGFullCharacterCustomization Updated;
        if (Controller->SelectCharacterCreatorBackend(BackendId, Updated))
        {
            SetDraftCustomization(Updated);
        }
    }
    InvalidateLayoutAndVolatility();
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
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
            DraftOutfit = Updated;
            DraftCustomization.Outfit = Updated;
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
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
            DraftOutfit = Updated;
            DraftCustomization.Outfit = Updated;
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
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
            DraftOutfit = Updated;
            DraftCustomization.Outfit = Updated;
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
            SetDraftCustomization(Controller->GetCharacterCreatorDraftCustomization());
            DraftOutfit = Updated;
            DraftCustomization.Outfit = Updated;
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

FReply UDiscGolfCharacterCreatorWidget::HandleZoomIn()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->ZoomCharacterCreatorPreview(-40.0f);
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleZoomOut()
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        Controller->ZoomCharacterCreatorPreview(40.0f);
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleFacePreset(FName PresetId)
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGFullCharacterCustomization Updated;
        if (Controller->ApplyCharacterCreatorFacePreset(PresetId, Updated))
        {
            SetDraftCustomization(Updated);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleCosmeticSelection(
    EDGCosmeticKind Kind,
    FName ItemId)
{
    if (ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get())
    {
        FDGFullCharacterCustomization Updated;
        if (Controller->SelectCharacterCreatorCosmetic(Kind, ItemId, Updated))
        {
            SetDraftCustomization(Updated);
        }
    }
    return FReply::Handled();
}

FReply UDiscGolfCharacterCreatorWidget::HandleColorSelection(
    EColorField Field,
    FLinearColor Color)
{
    Color.A = 1.0f;
    switch (Field)
    {
        case EColorField::Hair: DraftCustomization.Hair.HairColor = Color; break;
        case EColorField::FacialHair: DraftCustomization.Hair.FacialHairColor = Color; break;
        case EColorField::Eyebrow: DraftCustomization.Hair.EyebrowColor = Color; break;
        case EColorField::Skin: DraftCustomization.Appearance.SkinTone = Color; break;
        case EColorField::Eye: DraftCustomization.Appearance.EyeColor = Color; break;
        default: break;
    }
    SubmitPreview();
    return FReply::Handled();
}

void UDiscGolfCharacterCreatorWidget::HandleDisplayNameCommitted(
    const FText& Text,
    ETextCommit::Type CommitType)
{
    if (CommitType == ETextCommit::Default)
    {
        return;
    }
    DraftCustomization.Identity.DisplayName = Text.ToString();
    SubmitPreview();
}

void UDiscGolfCharacterCreatorWidget::HandleFaceSliderChanged(
    float NormalizedValue,
    FName MorphKey)
{
    if (!DiscGolfFullCharacterRuntime::GetFaceMorphKeys().Contains(MorphKey))
    {
        return;
    }
    DraftCustomization.Face.MorphValues.FindOrAdd(MorphKey) =
        FMath::Lerp(-1.0f, 1.0f, FMath::Clamp(NormalizedValue, 0.0f, 1.0f));
    SubmitPreview();
}

FName UDiscGolfCharacterCreatorWidget::GetSelectedCosmeticId(
    EDGCosmeticKind Kind) const
{
    switch (Kind)
    {
        case EDGCosmeticKind::Hair: return DraftCustomization.Hair.HairStyleId;
        case EDGCosmeticKind::FacialHair: return DraftCustomization.Hair.FacialHairId;
        case EDGCosmeticKind::Eyebrow: return DraftCustomization.Hair.EyebrowId;
        case EDGCosmeticKind::Scar: return DraftCustomization.Appearance.ScarId;
        case EDGCosmeticKind::Tattoo:
            return DraftCustomization.Appearance.TattooIds.IsEmpty()
                ? FName(TEXT("tattoo_none"))
                : DraftCustomization.Appearance.TattooIds[0];
        case EDGCosmeticKind::Voice: return DraftCustomization.Identity.VoiceId;
        case EDGCosmeticKind::PronounSet: return DraftCustomization.Identity.PronounSetId;
        default: return NAME_None;
    }
}

FText UDiscGolfCharacterCreatorWidget::GetStatusText() const
{
    const ADiscGolfTourPlayerController* Controller = OwningDiscGolfController.Get();
    return FText::FromString(Controller
        ? Controller->GetCharacterCreatorStatusText()
        : FString(TEXT("Character creator controller unavailable.")));
}

FText UDiscGolfCharacterCreatorWidget::GetBackendStatusText() const
{
    if (IsDGMasterBackendSelected())
    {
        return FText::FromString(
            TEXT("DGMaster remains the animation, release, disc, flight, and fallback authority."));
    }
    const ADiscGolfTourPlayerController* Controller =
        OwningDiscGolfController.Get();
    return FText::FromString(Controller
        ? Controller->GetCharacterCreatorBackendStatusText()
        : FString(TEXT("Realistic character availability cannot be verified.")));
}

FText UDiscGolfCharacterCreatorWidget::GetMetaHumanBackendButtonText() const
{
    const bool bSelected = DraftCustomization.AvatarBackendId
        == FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId);
    const ADiscGolfTourPlayerController* Controller =
        OwningDiscGolfController.Get();
    const bool bAvailable = Controller
        && Controller->IsCharacterCreatorMetaHumanBackendAvailable();
    return FText::FromString(FString(bSelected ? TEXT("● ") : TEXT(""))
        + (bAvailable
            ? TEXT("REALISTIC / CURATED")
            : TEXT("REALISTIC / UNAVAILABLE")));
}

bool UDiscGolfCharacterCreatorWidget::IsDGMasterBackendSelected() const
{
    return DraftCustomization.AvatarBackendId
        != FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId);
}

FText UDiscGolfCharacterCreatorWidget::GetCurrentLockText() const
{
    return FText::FromString(IsCurrentCategoryLocked()
        ? TEXT("UNLOCK TAB") : TEXT("LOCK TAB"));
}

bool UDiscGolfCharacterCreatorWidget::IsCurrentCategoryLocked() const
{
    switch (FMath::Clamp(ActiveCreatorTabIndex, 0, 6))
    {
        case 0: return RandomizeLocks.bIdentity;
        case 1: return RandomizeLocks.bBody;
        case 2: return RandomizeLocks.bFace;
        case 3: return RandomizeLocks.bHair;
        case 4: return RandomizeLocks.bAppearance;
        case 5: return RandomizeLocks.bThrowStyle;
        case 6: return RandomizeLocks.bOutfit;
        default: return false;
    }
}

void UDiscGolfCharacterCreatorWidget::SetCurrentCategoryLocked(bool bLocked)
{
    switch (FMath::Clamp(ActiveCreatorTabIndex, 0, 6))
    {
        case 0: RandomizeLocks.bIdentity = bLocked; break;
        case 1: RandomizeLocks.bBody = bLocked; break;
        case 2: RandomizeLocks.bFace = bLocked; break;
        case 3: RandomizeLocks.bHair = bLocked; break;
        case 4: RandomizeLocks.bAppearance = bLocked; break;
        case 5: RandomizeLocks.bThrowStyle = bLocked; break;
        case 6: RandomizeLocks.bOutfit = bLocked; break;
        default: break;
    }
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
