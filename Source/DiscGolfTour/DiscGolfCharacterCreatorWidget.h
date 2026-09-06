#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfCharacterCreatorWidget.generated.h"

class ADiscGolfTourPlayerController;
class SButton;
class SEditableTextBox;
class SVerticalBox;
class SWidget;
class SWidgetSwitcher;

/**
 * Functional native character-creator surface for the accepted character
 * foundation, modular outfits, and Session 7 full-character data contract.
 *
 * The widget owns only an editable draft and presentation. The current pawn
 * remains the preview subject, the PlayerController owns open/apply/cancel,
 * and UDiscGolfTourGameInstance remains the sole persistence authority. An
 * empty WBP_DG_CharacterCreator may derive from this class without rebuilding
 * the native seven-tab editor.
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

    void InitializeFullCreator(
        ADiscGolfTourPlayerController* InController,
        const FDGFullCharacterCustomization& InCharacter);

    void SetDraftProfile(
        const FDGBodyProfile& InBody,
        const FDGThrowStyle& InThrowStyle,
        EDGHandedness InHandedness);

    void GetDraftProfile(
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutThrowStyle,
        EDGHandedness& OutHandedness) const;

    void SetDraftCustomization(
        const FDGFullCharacterCustomization& InCharacter);
    const FDGFullCharacterCustomization& GetDraftCustomization() const
    {
        return DraftCustomization;
    }

    /** First actionable control used when keyboard/gamepad focus enters the creator. */
    TSharedPtr<SWidget> GetInitialFocusWidget() const;

    /** Narrow no-save seam used only by the Session 6 rendered validation runner. */
    void PrepareSession6VisualOutfitEvidence(
        EDGOutfitSlot OutfitSlot,
        const FDGOutfitLoadout& InOutfit);
    void PrepareSession7VisualEvidence(
        int32 TabIndex,
        const FDGFullCharacterCustomization& InCharacter);
    int32 GetActiveCreatorTabIndex() const { return ActiveCreatorTabIndex; }
    void GetSession7VisibleControlIds(TArray<FString>& OutControlIds) const;

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
        Muscularity,
        BodyFat,
        Chest,
        Waist,
        Hips,
        Arms,
        Legs,
        RunUp,
        ReachBack,
        TorsoRotation,
        Brace,
        Explosiveness,
        FollowThrough,
        Complexion,
        Freckles,
        SunExposure
    };

    enum class EColorField : uint8
    {
        Hair,
        FacialHair,
        Eyebrow,
        Skin,
        Eye
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
    TSharedRef<SWidget> BuildIdentityTab();
    TSharedRef<SWidget> BuildBodyTab();
    TSharedRef<SWidget> BuildFaceTab();
    TSharedRef<SWidget> BuildHairTab();
    TSharedRef<SWidget> BuildAppearanceTab();
    TSharedRef<SWidget> BuildThrowStyleTab();
    TSharedRef<SWidget> BuildFaceSliderRow(FName MorphKey);
    TSharedRef<SWidget> BuildCosmeticOptionGroup(
        const FText& Label,
        EDGCosmeticKind Kind);
    TSharedRef<SWidget> BuildColorSwatches(
        const FText& Label,
        EColorField Field,
        TConstArrayView<FLinearColor> Colors);
    TSharedRef<SWidget> BuildBackendAwareProxyTab(
        const TSharedRef<SWidget>& ProxyTab);
    void RebuildOutfitLists();

    float GetFieldValue(ECreatorField Field) const;
    void SetFieldValue(ECreatorField Field, float Value);
    FText GetFieldValueText(ECreatorField Field) const;
    void HandleSliderChanged(float NormalizedValue, ECreatorField Field, float MinValue, float MaxValue);
    void SubmitPreview();
    void SyncLegacyDraftSlices();

    FReply HandleRightHanded();
    FReply HandleLeftHanded();
    FReply HandlePreset(FName PresetId);
    FReply HandleReset();
    FReply HandleResetCurrentTab();
    FReply HandleResetAll();
    FReply HandleRandomize();
    FReply HandleToggleCurrentCategoryLock();
    FReply HandleApply();
    FReply HandleCancel();
    FReply HandleRotateLeft();
    FReply HandleRotateRight();
    FReply HandleZoomIn();
    FReply HandleZoomOut();
    FReply HandleCreatorTab(int32 TabIndex);
    FReply HandleBackendSelection(FName BackendId);
    FReply HandleOutfitSlot(EDGOutfitSlot OutfitSlot);
    FReply HandleOutfitItem(FName ItemId);
    FReply HandleOutfitVariant(FName VariantId);
    FReply HandleResetOutfit();
    FReply HandleRandomizeOutfit();
    FReply HandleFacePreset(FName PresetId);
    FReply HandleCosmeticSelection(EDGCosmeticKind Kind, FName ItemId);
    FReply HandleColorSelection(EColorField Field, FLinearColor Color);
    void HandleDisplayNameCommitted(
        const FText& Text,
        ETextCommit::Type CommitType);
    void HandleFaceSliderChanged(float NormalizedValue, FName MorphKey);
    FName GetSelectedCosmeticId(EDGCosmeticKind Kind) const;

    FText GetStatusText() const;
    FText GetBackendStatusText() const;
    FText GetMetaHumanBackendButtonText() const;
    bool IsDGMasterBackendSelected() const;
    FText GetHandednessText() const;
    FText GetHandednessLimitationText() const;
    FSlateColor GetHandednessLimitationColor() const;
    FText GetCurrentLockText() const;
    bool IsCurrentCategoryLocked() const;
    void SetCurrentCategoryLocked(bool bLocked);

    TWeakObjectPtr<ADiscGolfTourPlayerController> OwningDiscGolfController;
    FDGBodyProfile DraftBody;
    FDGThrowStyle DraftThrowStyle;
    EDGHandedness DraftHandedness = EDGHandedness::Right;
    FDGOutfitLoadout DraftOutfit;
    FDGFullCharacterCustomization DraftCustomization;
    FDiscGolfCharacterRandomizeLocks RandomizeLocks;
    EDGOutfitSlot SelectedOutfitSlot = EDGOutfitSlot::Top;
    int32 ActiveCreatorTabIndex = 0;
    TSharedPtr<SWidget> RootSlateWidget;
    TSharedPtr<SButton> InitialFocusButton;
    TSharedPtr<SWidgetSwitcher> CreatorTabSwitcher;
    TMap<int32, TSharedPtr<SButton>> CreatorTabButtons;
    TSharedPtr<SVerticalBox> OutfitItemList;
    TSharedPtr<SVerticalBox> OutfitVariantList;
    TMap<EDGOutfitSlot, TSharedPtr<SButton>> OutfitCategoryButtons;
    TMap<FName, TSharedPtr<SButton>> OutfitItemButtons;
    TMap<FName, TSharedPtr<SButton>> OutfitVariantButtons;
    EOutfitFocusRequest PendingOutfitFocusRequest = EOutfitFocusRequest::Preserve;
};
