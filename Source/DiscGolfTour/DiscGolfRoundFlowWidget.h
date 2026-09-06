#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DiscGolfRoundFlowPresentation.h"
#include "DiscGolfRoundFlowWidget.generated.h"

class ADiscGolfTourPlayerController;
class SBorder;
class SButton;
class STextBlock;
class SVerticalBox;
class SWrapBox;
class SWidget;

/**
 * Code-owned round-flow surface that remains usable when no authored UI asset is
 * present. It displays validated copies only and routes every command back to the
 * PlayerController; it never mutates scoring, round, save, or throw state itself.
 */
UCLASS(Blueprintable)
class DISCGOLFTOUR_API UDiscGolfRoundFlowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeForController(ADiscGolfTourPlayerController* InController);

    /** Applies a complete validated snapshot atomically. */
    bool ApplySnapshot(
        const FDGRoundFlowSnapshot& InSnapshot,
        FString* OutError = nullptr);

    bool HasFocusTarget() const;
    TSharedPtr<SWidget> GetInitialFocusWidget() const;

    /** Stable inspection seams for focused structure tests and runtime evidence. */
    int32 GetActionButtonCount() const;
    int32 GetRenderedRowCount() const { return RenderedRowCount; }
    bool HasActionButton(EDGRoundFlowAction Action) const;
    const FDGRoundFlowSnapshot& GetAppliedSnapshot() const { return CurrentSnapshot; }

#if WITH_DEV_AUTOMATION_TESTS
    /** Builds only this class's native Slate tree; avoids requiring a viewport in structure tests. */
    TSharedRef<SWidget> BuildNativeStructureForTesting();
#endif

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    void RefreshSlateContent();
    void RebuildRows();
    void RebuildActions();
    void SetActionButtonsEnabled(bool bEnabled);
    FReply HandleActionClicked(EDGRoundFlowAction Action);

    TWeakObjectPtr<ADiscGolfTourPlayerController> OwningDiscGolfController;
    FDGRoundFlowSnapshot CurrentSnapshot;
    bool bHasValidSnapshot = false;
    bool bActionDispatchedForSnapshot = false;
    int32 RenderedRowCount = 0;

    TSharedPtr<SBorder> RootBorder;
    TSharedPtr<STextBlock> TitleText;
    TSharedPtr<STextBlock> CourseText;
    TSharedPtr<STextBlock> SummaryText;
    TSharedPtr<SVerticalBox> RowsBox;
    TSharedPtr<SWrapBox> ActionsBox;
    TSharedPtr<SButton> InitialFocusButton;
    TMap<EDGRoundFlowAction, TSharedPtr<SButton>> ActionButtons;
};
