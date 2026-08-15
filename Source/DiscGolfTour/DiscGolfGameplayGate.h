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
}
