#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfCharacterCreatorWidget.generated.h"

class ADiscGolfTourPlayerController;
class SButton;
class SVerticalBox;
class SWidget;
class SWidgetSwitcher;

/**
 * Functional native character-creator surface for the Session 4 foundation
 * and Session 6 modular-outfit extension.
 *
 * The widget owns only an editable draft and presentation. The current pawn
 * remains the preview subject, the PlayerController owns open/apply/cancel,
 * and UDiscGolfTourGameInstance remains the sole persistence authority. An
 * empty WBP_DG_CharacterCreator may derive from this class without rebuilding
 * the native Body, Throw Style, or Outfit controls.
 */
UCLASS(Blueprintable)
class DISCGOLFTOUR_API UDiscGolfCharacterCreatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeCreator(
        ADiscGolfTourPlayerController* InController,
        const FDGBodyProfile& InBody,
        const FDGThrowStyle& InThrowStyle,
        EDGHandedness InHandedness,
        const FDGOutfitLoadout& InOutfit);

    void SetDraftProfile(
        const FDGBodyProfile& InBody,
        const FDGThrowStyle& InThrowStyle,
        EDGHandedness InHandedness);

    void GetDraftProfile(
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutThrowStyle,
        EDGHandedness& OutHandedness) const;

    /** First actionable control used when keyboard/gamepad focus enters the creator. */
    TSharedPtr<SWidget> GetInitialFocusWidget() const;

    /** Narrow no-save seam used only by the Session 6 rendered validation runner. */
    void PrepareSession6VisualOutfitEvidence(
        EDGOutfitSlot OutfitSlot,
        const FDGOutfitLoadout& InOutfit);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    enum class ECreatorField : uint8
    {
        Height,
        Wingspan,
        ShoulderWidth,
        TorsoLength,
        LegLength,
        HandScale,
        Mass,
        RunUp,
        ReachBack,
        TorsoRotation,
        Brace,
        Explosiveness,
        FollowThrough
    };

    enum class EOutfitFocusRequest : uint8
    {
        Preserve,
        Category,
        CurrentItem,
        CurrentVariant
    };

    TSharedRef<SWidget> BuildSliderRow(
        const FText& Label,
        ECreatorField Field,
        float MinValue,
        float MaxValue);
    TSharedRef<SWidget> BuildOutfitTab();
    void RebuildOutfitLists();

    float GetFieldValue(ECreatorField Field) const;
    void SetFieldValue(ECreatorField Field, float Value);
    FText GetFieldValueText(ECreatorField Field) const;
    void HandleSliderChanged(float NormalizedValue, ECreatorField Field, float MinValue, float MaxValue);
    void SubmitPreview();

    FReply HandleRightHanded();
    FReply HandleLeftHanded();
    FReply HandlePreset(FName PresetId);
    FReply HandleReset();
    FReply HandleApply();
    FReply HandleCancel();
    FReply HandleRotateLeft();
    FReply HandleRotateRight();
    FReply HandleCreatorTab(int32 TabIndex);
    FReply HandleOutfitSlot(EDGOutfitSlot OutfitSlot);
    FReply HandleOutfitItem(FName ItemId);
    FReply HandleOutfitVariant(FName VariantId);
    FReply HandleResetOutfit();
    FReply HandleRandomizeOutfit();

    FText GetStatusText() const;
    FText GetHandednessText() const;
    FText GetHandednessLimitationText() const;
    FSlateColor GetHandednessLimitationColor() const;

    TWeakObjectPtr<ADiscGolfTourPlayerController> OwningDiscGolfController;
    FDGBodyProfile DraftBody;
    FDGThrowStyle DraftThrowStyle;
    EDGHandedness DraftHandedness = EDGHandedness::Right;
    FDGOutfitLoadout DraftOutfit;
    EDGOutfitSlot SelectedOutfitSlot = EDGOutfitSlot::Top;
    int32 ActiveCreatorTabIndex = 0;
    TSharedPtr<SWidget> RootSlateWidget;
    TSharedPtr<SButton> InitialFocusButton;
    TSharedPtr<SWidgetSwitcher> CreatorTabSwitcher;
    TSharedPtr<SVerticalBox> OutfitItemList;
    TSharedPtr<SVerticalBox> OutfitVariantList;
    TMap<EDGOutfitSlot, TSharedPtr<SButton>> OutfitCategoryButtons;
    TMap<FName, TSharedPtr<SButton>> OutfitItemButtons;
    TMap<FName, TSharedPtr<SButton>> OutfitVariantButtons;
    EOutfitFocusRequest PendingOutfitFocusRequest = EOutfitFocusRequest::Preserve;
};
