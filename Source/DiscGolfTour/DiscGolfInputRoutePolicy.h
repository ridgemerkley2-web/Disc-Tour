#pragma once

#include "CoreMinimal.h"

/**
 * Project-owned input routes. These describe who may consume input; they never
 * own gameplay, score, replay, camera, or save state.
 */
enum class EDiscGolfInputRoute : uint8
{
    Gameplay,
    AimThrow,
    CharacterCreator,
    Replay,
    UI
};

/**
 * Result of routing one raw key edge through the hole-introduction confirm
 * barrier. The barrier owns only the physical press/repeat/release lifecycle;
 * it never owns presentation or gameplay state.
 */
enum class EDiscGolfPresentationDismissInputDisposition : uint8
{
    PassThrough,
    DismissAndConsume,
    ConsumeHeldEdge,
    ForwardReleaseAndConsume
};

/**
 * Holds a shared confirm/throw key closed until its matching release edge.
 * Unreal may reconcile a first-seen IE_Repeat into IE_Pressed, so consuming
 * only the dismissal press is not sufficient once the intro closes.
 */
struct FDiscGolfPresentationDismissInputBarrier
{
    EDiscGolfPresentationDismissInputDisposition Route(
        const FKey& Key,
        EInputEvent Event,
        bool bBarrierProtectedKey,
        bool bDismissPresentation)
    {
        const bool bDownEdge = Event == IE_Pressed
            || Event == IE_Repeat
            || Event == IE_DoubleClick;
        const bool bFreshDownEdge = Event == IE_Pressed
            || Event == IE_DoubleClick;

        // FlushPressedKeys/input-mode transitions can leave a first-seen
        // IE_Repeat queued after the matching release transaction has already
        // completed. UPlayerInput reconciles that repeat into IE_Pressed, which
        // would otherwise start a throw without a new player press. Keep the
        // released key quarantined until an explicit fresh press/double-click
        // proves a new physical transaction has begun.
        if (ReleasedKeysAwaitingFreshPress.Contains(Key))
        {
            if (Event == IE_Repeat || Event == IE_Released)
            {
                return EDiscGolfPresentationDismissInputDisposition::ConsumeHeldEdge;
            }
            if (Event == IE_Pressed || Event == IE_DoubleClick)
            {
                ReleasedKeysAwaitingFreshPress.Remove(Key);
            }
        }

        if (FPendingKeyState* State = PendingReleaseKeys.Find(Key))
        {
            if (Event == IE_Released)
            {
                if (State->bReleaseQueued)
                {
                    return EDiscGolfPresentationDismissInputDisposition::ConsumeHeldEdge;
                }
                State->bReleaseQueued = true;
                return EDiscGolfPresentationDismissInputDisposition::ForwardReleaseAndConsume;
            }
            if (bFreshDownEdge && State->bReleaseQueued)
            {
                // A release callback may already be queued for the next tick.
                // Move this new down edge to a new generation so that stale
                // completion cannot reopen gameplay while the key is down.
                State->Generation = AllocateGeneration();
                State->bReleaseQueued = false;
            }
            return EDiscGolfPresentationDismissInputDisposition::ConsumeHeldEdge;
        }

        if (IsPending() && bBarrierProtectedKey && bDownEdge)
        {
            Arm(Key);
            return EDiscGolfPresentationDismissInputDisposition::ConsumeHeldEdge;
        }
        if (bDismissPresentation && bBarrierProtectedKey && bDownEdge)
        {
            Arm(Key);
            return EDiscGolfPresentationDismissInputDisposition::DismissAndConsume;
        }
        return EDiscGolfPresentationDismissInputDisposition::PassThrough;
    }

    void Arm(const FKey& Key)
    {
        if (!Key.IsValid() || PendingReleaseKeys.Contains(Key))
        {
            return;
        }
        FPendingKeyState& State = PendingReleaseKeys.Add(Key);
        State.Generation = AllocateGeneration();
    }

    uint32 GetReleaseGeneration(const FKey& Key) const
    {
        const FPendingKeyState* State = PendingReleaseKeys.Find(Key);
        return State && State->bReleaseQueued ? State->Generation : 0;
    }

    bool CompleteRelease(const FKey& Key, uint32 Generation)
    {
        const FPendingKeyState* State = PendingReleaseKeys.Find(Key);
        if (!State || !State->bReleaseQueued || State->Generation != Generation)
        {
            return false;
        }
        PendingReleaseKeys.Remove(Key);
        ReleasedKeysAwaitingFreshPress.Add(Key);
        return true;
    }

    void Reset()
    {
        PendingReleaseKeys.Reset();
        ReleasedKeysAwaitingFreshPress.Reset();
        NextGeneration = 1;
    }

    bool IsPending() const
    {
        return !PendingReleaseKeys.IsEmpty();
    }

    bool IsPending(const FKey& Key) const
    {
        return PendingReleaseKeys.Contains(Key);
    }

private:
    struct FPendingKeyState
    {
        uint32 Generation = 0;
        bool bReleaseQueued = false;
    };

    uint32 AllocateGeneration()
    {
        const uint32 Generation = NextGeneration++;
        if (NextGeneration == 0)
        {
            NextGeneration = 1;
        }
        return Generation;
    }

    TMap<FKey, FPendingKeyState> PendingReleaseKeys;
    TSet<FKey> ReleasedKeysAwaitingFreshPress;
    uint32 NextGeneration = 1;
};

struct FDiscGolfInputRouteContext
{
    bool bCharacterCreatorOpen = false;
    bool bUiOpen = false;
    bool bReplayActive = false;
    bool bAimThrowActive = false;
};

namespace DiscGolfInputRoutePolicy
{
    inline EDiscGolfInputRoute Resolve(const FDiscGolfInputRouteContext& Context)
    {
        if (Context.bCharacterCreatorOpen)
        {
            return EDiscGolfInputRoute::CharacterCreator;
        }
        if (Context.bUiOpen)
        {
            return EDiscGolfInputRoute::UI;
        }
        if (Context.bReplayActive)
        {
            return EDiscGolfInputRoute::Replay;
        }
        if (Context.bAimThrowActive)
        {
            return EDiscGolfInputRoute::AimThrow;
        }
        return EDiscGolfInputRoute::Gameplay;
    }

    /**
     * Gameplay may enter shot setup, but once the timing transaction starts it
     * accepts only aim/throw input. Creator, replay, and UI routes are isolated.
     */
    inline bool AllowsAction(EDiscGolfInputRoute ActiveRoute, EDiscGolfInputRoute ActionRoute)
    {
        if (ActiveRoute == EDiscGolfInputRoute::Gameplay)
        {
            return ActionRoute == EDiscGolfInputRoute::Gameplay
                || ActionRoute == EDiscGolfInputRoute::AimThrow;
        }
        return ActiveRoute == ActionRoute;
    }
}
