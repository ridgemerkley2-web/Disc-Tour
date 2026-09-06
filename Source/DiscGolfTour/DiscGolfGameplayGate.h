#pragma once

/** Side-effect-free lifecycle guard shared by player and trusted regression launches. */
namespace DiscGolfGameplayGate
{
    inline bool CanLaunchThrow(
        bool bHasActiveHole,
        bool bHasActiveDisc,
        bool bHasReplay,
        bool bFlyoverActive,
        bool bHoleComplete,
        bool bScorecardVisible,
        bool bHoleIntroActive = false)
    {
        return bHasActiveHole && !bHasActiveDisc && !bHasReplay
            && !bFlyoverActive && !bHoleComplete && !bScorecardVisible && !bHoleIntroActive;
    }

    /**
     * Player-only lifecycle guard shared by the synchronous input path and the
     * committed animation-notify path. The latter intentionally has an active
     * throw adapter, so that single condition remains outside this seam.
     */
    inline bool CanCommitPlayerRelease(
        bool bBaseLaunchAllowed,
        bool bMainMenuVisible,
        bool bRegressionActive,
        bool bLieTransitionActive,
        bool bHasPlayerController,
        bool bPresentationDismissReleasePending,
        bool bFreshThrowDownRequiredAfterPresentation,
        bool bControlsMenuOpen,
        bool bCharacterCreatorOpen,
        bool bWorldPaused)
    {
        return bBaseLaunchAllowed
            && !bMainMenuVisible
            && !bRegressionActive
            && !bLieTransitionActive
            && bHasPlayerController
            && !bPresentationDismissReleasePending
            && !bFreshThrowDownRequiredAfterPresentation
            && !bControlsMenuOpen
            && !bCharacterCreatorOpen
            && !bWorldPaused;
    }
}
