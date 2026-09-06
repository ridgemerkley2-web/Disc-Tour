#pragma once

#include "CoreMinimal.h"

/** Project-owned presentation modes. None of these modes can mutate gameplay. */
enum class EDiscGolfCameraViewMode : uint8
{
    Aim,
    Tee,
    FollowDisc,
    Landing,
    Basket,
    ShotReplay,
    CharacterCreator,
    FreeCamera,
    CourseFlyover
};

/** Exclusive lifecycle owner for a camera view. */
enum class EDiscGolfCameraViewOwner : uint8
{
    None,
    PlayerSetup,
    LiveShot,
    Replay,
    CharacterCreator,
    FreeCamera,
    CourseFlyover
};

/** Generation-stamped ownership prevents a stale actor callback from stealing the view. */
struct FDiscGolfCameraViewToken
{
    EDiscGolfCameraViewOwner Owner = EDiscGolfCameraViewOwner::None;
    EDiscGolfCameraViewMode Mode = EDiscGolfCameraViewMode::Aim;
    uint32 Generation = 0;

    bool IsValid() const
    {
        return Owner != EDiscGolfCameraViewOwner::None && Generation != 0;
    }
};

struct FDiscGolfCameraViewState
{
    EDiscGolfCameraViewOwner Owner = EDiscGolfCameraViewOwner::None;
    EDiscGolfCameraViewMode Mode = EDiscGolfCameraViewMode::Aim;
    uint32 Generation = 0;
    bool bActive = false;
};

namespace DiscGolfCameraViewContract
{
    DISCGOLFTOUR_API EDiscGolfCameraViewOwner RequiredOwner(EDiscGolfCameraViewMode Mode);

    /**
     * Acquires an idle view or advances the current owner's mode. A different
     * active owner must release first; no implicit preemption is allowed.
     */
    DISCGOLFTOUR_API bool TryAcquire(
        FDiscGolfCameraViewState& InOutState,
        EDiscGolfCameraViewOwner RequestingOwner,
        EDiscGolfCameraViewMode RequestedMode,
        FDiscGolfCameraViewToken& OutToken,
        FString& OutError);

    /** Releases only the exact current generation. Stale callbacks fail closed. */
    DISCGOLFTOUR_API bool TryRelease(
        FDiscGolfCameraViewState& InOutState,
        const FDiscGolfCameraViewToken& Token,
        FString& OutError);

    DISCGOLFTOUR_API bool IsCurrent(
        const FDiscGolfCameraViewState& State,
        const FDiscGolfCameraViewToken& Token);
}
