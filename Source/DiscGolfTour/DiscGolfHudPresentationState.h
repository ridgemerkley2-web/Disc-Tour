#pragma once

enum class EDiscGolfHudPresentationMode : unsigned char
{
    Play,
    Flight,
    Review,
    Replay,
    Complete
};

struct FDiscGolfHudPresentationContext
{
    bool bActiveDisc = false;
    bool bReplayActive = false;
    bool bComplete = false;
    bool bHasLastRelease = false;
    bool bTimingActive = false;
    float LastReleaseAgeSeconds = 0.0f;
};

struct FDiscGolfHudPresentationState
{
    EDiscGolfHudPresentationMode Mode = EDiscGolfHudPresentationMode::Play;
    bool bShowCurrentLie = true;
    bool bShowShotSetup = true;
    bool bShowFlightStrip = false;
    bool bShowShotFeedback = false;
    bool bShowReplayChrome = false;
    bool bShowHelp = true;
};

namespace DiscGolfHudPresentation
{
    constexpr float ShotFeedbackDurationSeconds = 3.0f;

    inline bool IsShotFeedbackVisible(const FDiscGolfHudPresentationContext& Context)
    {
        return Context.bHasLastRelease
            && Context.LastReleaseAgeSeconds >= 0.0f
            && Context.LastReleaseAgeSeconds <= ShotFeedbackDurationSeconds
            && !Context.bTimingActive;
    }

    inline FDiscGolfHudPresentationState Resolve(const FDiscGolfHudPresentationContext& Context)
    {
        FDiscGolfHudPresentationState State;
        const bool bRecentFeedback = IsShotFeedbackVisible(Context);

        if (Context.bReplayActive)
        {
            State.Mode = EDiscGolfHudPresentationMode::Replay;
            State.bShowCurrentLie = false;
            State.bShowShotSetup = false;
            State.bShowReplayChrome = true;
            State.bShowHelp = false;
            return State;
        }

        if (Context.bComplete)
        {
            State.Mode = EDiscGolfHudPresentationMode::Complete;
            State.bShowShotSetup = false;
            State.bShowHelp = false;
            return State;
        }

        if (Context.bActiveDisc)
        {
            State.Mode = EDiscGolfHudPresentationMode::Flight;
            State.bShowCurrentLie = false;
            State.bShowShotSetup = false;
            State.bShowFlightStrip = true;
            State.bShowShotFeedback = bRecentFeedback;
            State.bShowHelp = false;
            return State;
        }

        if (bRecentFeedback)
        {
            State.Mode = EDiscGolfHudPresentationMode::Review;
            State.bShowShotSetup = false;
            State.bShowShotFeedback = true;
            State.bShowHelp = false;
        }

        return State;
    }
}
