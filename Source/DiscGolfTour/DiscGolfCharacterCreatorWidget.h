#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfCharacterCreatorWidget.generated.h"

class ADiscGolfTourPlayerController;
class SButton;
class SWidget;

/**
 * Functional native character-creator surface for the Session 4 foundation.
 *
 * The widget owns only an editable draft and presentation. The current pawn
 * remains the preview subject, the PlayerController owns open/apply/cancel,
 * and UDiscGolfTourGameInstance remains the sole persistence authority. An
 * empty WBP_DG_CharacterCreator may derive from this class without rebuilding
 * the native Body and Throw Style controls.
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
        EDGHandedness InHandedness);

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

    TSharedRef<SWidget> BuildSliderRow(
        const FText& Label,
        ECreatorField Field,
        float MinValue,
        float MaxValue);

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

    FText GetStatusText() const;
    FText GetHandednessText() const;
    FText GetHandednessLimitationText() const;
    FSlateColor GetHandednessLimitationColor() const;

    TWeakObjectPtr<ADiscGolfTourPlayerController> OwningDiscGolfController;
    FDGBodyProfile DraftBody;
    FDGThrowStyle DraftThrowStyle;
    EDGHandedness DraftHandedness = EDGHandedness::Right;
    TSharedPtr<SWidget> RootSlateWidget;
    TSharedPtr<SButton> InitialFocusButton;
};
