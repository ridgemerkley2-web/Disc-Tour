#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"

/**
 * Small, game-thread transaction gate for the Session 3 animated RHBH hand-off.
 *
 * The transaction owns no animation or flight calculations. It snapshots the
 * authoritative FThrowCommand produced by UThrowControllerComponent and passes
 * that exact value to one launch callback at most once. The Released transition
 * occurs before the callback is invoked so a re-entrant or duplicate notify
 * cannot request a second gameplay disc.
 */
enum class EDiscGolfRHBHThrowTransactionState : uint8
{
    Idle,
    AwaitingRelease,
    Released,
    Recovered
};

enum class EDiscGolfRHBHThrowRecoveryReason : uint8
{
    None,
    ThrowFinished,
    ThrowFinishedBeforeRelease,
    CancelledBeforeRelease,
    InterruptedBeforeRelease,
    InterruptedAfterRelease,
    WatchdogBeforeRelease,
    WatchdogAfterRelease
};

class DISCGOLFTOUR_API FDiscGolfRHBHThrowTransaction
{
public:
    /** Starts a new animation transaction and snapshots the authoritative command verbatim. */
    bool Begin(const FThrowCommand& InAuthoritativeCommand)
    {
        if (IsActive())
        {
            return false;
        }

        CachedCommand = InAuthoritativeCommand;
        bHasCachedCommand = true;
        State = EDiscGolfRHBHThrowTransactionState::AwaitingRelease;
        RecoveryReason = EDiscGolfRHBHThrowRecoveryReason::None;
        ReleaseCommitCountForAttempt = 0;
        ++AttemptSerial;
        return true;
    }

    /**
     * Commits the one permitted release and invokes the existing authoritative
     * launch path with the cached command. This function deliberately does not
     * accept replacement power, angle, spin, timing, or direction values.
     */
    template <typename LaunchRequestType>
    bool TryCommitRelease(LaunchRequestType&& RequestAuthoritativeLaunch)
    {
        return TryCommitRelease(AttemptSerial,
            Forward<LaunchRequestType>(RequestAuthoritativeLaunch));
    }

    /** Token-aware overload for delegates or timers captured by an older attempt. */
    template <typename LaunchRequestType>
    bool TryCommitRelease(uint64 ExpectedAttemptSerial, LaunchRequestType&& RequestAuthoritativeLaunch)
    {
        if (ExpectedAttemptSerial != AttemptSerial
            || State != EDiscGolfRHBHThrowTransactionState::AwaitingRelease
            || !bHasCachedCommand)
        {
            return false;
        }

        // Commit before invoking external code. A re-entrant notify is rejected.
        State = EDiscGolfRHBHThrowTransactionState::Released;
        ReleaseCommitCountForAttempt = 1;
        ++TotalReleaseCommitCount;
        Forward<LaunchRequestType>(RequestAuthoritativeLaunch)(CachedCommand);
        return true;
    }

    /** Cancels only while the disc is still held. A released throw is never replaced. */
    bool CancelBeforeRelease()
    {
        if (State != EDiscGolfRHBHThrowTransactionState::AwaitingRelease)
        {
            return false;
        }

        Recover(EDiscGolfRHBHThrowRecoveryReason::CancelledBeforeRelease);
        return true;
    }

    bool CancelBeforeRelease(uint64 ExpectedAttemptSerial)
    {
        return ExpectedAttemptSerial == AttemptSerial && CancelBeforeRelease();
    }

    /** Handles the single DG Throw Finished event, including a safe early-finish cancellation. */
    bool Finish()
    {
        if (State == EDiscGolfRHBHThrowTransactionState::AwaitingRelease)
        {
            Recover(EDiscGolfRHBHThrowRecoveryReason::ThrowFinishedBeforeRelease);
            return true;
        }

        if (State == EDiscGolfRHBHThrowTransactionState::Released)
        {
            Recover(EDiscGolfRHBHThrowRecoveryReason::ThrowFinished);
            return true;
        }

        return false;
    }

    bool Finish(uint64 ExpectedAttemptSerial)
    {
        return ExpectedAttemptSerial == AttemptSerial && Finish();
    }

    /**
     * Bounded fail-safe used when a montage or notify never completes. It restores
     * control but never fabricates a release or retries an already released throw.
     */
    bool RecoverFromWatchdog()
    {
        if (State == EDiscGolfRHBHThrowTransactionState::AwaitingRelease)
        {
            Recover(EDiscGolfRHBHThrowRecoveryReason::WatchdogBeforeRelease);
            return true;
        }

        if (State == EDiscGolfRHBHThrowTransactionState::Released)
        {
            Recover(EDiscGolfRHBHThrowRecoveryReason::WatchdogAfterRelease);
            return true;
        }

        return false;
    }

    bool RecoverFromWatchdog(uint64 ExpectedAttemptSerial)
    {
        return ExpectedAttemptSerial == AttemptSerial && RecoverFromWatchdog();
    }

    /** Explicit montage/interruption recovery, distinct from a normal finish or timeout. */
    bool RecoverFromInterruption()
    {
        if (State == EDiscGolfRHBHThrowTransactionState::AwaitingRelease)
        {
            Recover(EDiscGolfRHBHThrowRecoveryReason::InterruptedBeforeRelease);
            return true;
        }

        if (State == EDiscGolfRHBHThrowTransactionState::Released)
        {
            Recover(EDiscGolfRHBHThrowRecoveryReason::InterruptedAfterRelease);
            return true;
        }

        return false;
    }

    bool RecoverFromInterruption(uint64 ExpectedAttemptSerial)
    {
        return ExpectedAttemptSerial == AttemptSerial && RecoverFromInterruption();
    }

    bool IsActive() const
    {
        return State == EDiscGolfRHBHThrowTransactionState::AwaitingRelease
            || State == EDiscGolfRHBHThrowTransactionState::Released;
    }

    bool IsAwaitingRelease() const
    {
        return State == EDiscGolfRHBHThrowTransactionState::AwaitingRelease;
    }

    bool HasCommittedRelease() const
    {
        return ReleaseCommitCountForAttempt == 1;
    }

    EDiscGolfRHBHThrowTransactionState GetState() const { return State; }
    EDiscGolfRHBHThrowRecoveryReason GetRecoveryReason() const { return RecoveryReason; }
    uint64 GetAttemptSerial() const { return AttemptSerial; }
    int32 GetReleaseCommitCountForAttempt() const { return ReleaseCommitCountForAttempt; }
    int32 GetTotalReleaseCommitCount() const { return TotalReleaseCommitCount; }

private:
    void Recover(EDiscGolfRHBHThrowRecoveryReason InReason)
    {
        State = EDiscGolfRHBHThrowTransactionState::Recovered;
        RecoveryReason = InReason;
        bHasCachedCommand = false;
        CachedCommand = FThrowCommand();
    }

    FThrowCommand CachedCommand;
    EDiscGolfRHBHThrowTransactionState State = EDiscGolfRHBHThrowTransactionState::Idle;
    EDiscGolfRHBHThrowRecoveryReason RecoveryReason = EDiscGolfRHBHThrowRecoveryReason::None;
    uint64 AttemptSerial = 0;
    int32 ReleaseCommitCountForAttempt = 0;
    int32 TotalReleaseCommitCount = 0;
    bool bHasCachedCommand = false;
};
