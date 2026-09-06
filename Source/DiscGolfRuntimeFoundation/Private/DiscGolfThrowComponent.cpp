#include "DiscGolfThrowComponent.h"

#include "AnimNotify_DiscRelease.h"
#include "AnimNotify_ThrowFinished.h"
#include "AnimNotify_ThrowPhase.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "DiscGolfCharacterProfile.h"

namespace
{
constexpr EDGThrowPhase AuthoredPhases[] = {
    EDGThrowPhase::Aim,
    EDGThrowPhase::RunUp,
    EDGThrowPhase::ReachBack,
    EDGThrowPhase::Plant,
    EDGThrowPhase::Acceleration,
    EDGThrowPhase::FollowThrough,
    EDGThrowPhase::Recovery
};

constexpr EDGThrowPhase RuntimePhases[] = {
    EDGThrowPhase::Aim,
    EDGThrowPhase::RunUp,
    EDGThrowPhase::ReachBack,
    EDGThrowPhase::Plant,
    EDGThrowPhase::Acceleration,
    EDGThrowPhase::Release,
    EDGThrowPhase::FollowThrough,
    EDGThrowPhase::Recovery
};

int32 FindPhaseIndex(const EDGThrowPhase* Phases, int32 PhaseCount, EDGThrowPhase Phase)
{
    for (int32 Index = 0; Index < PhaseCount; ++Index)
    {
        if (Phases[Index] == Phase)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}
}

UDiscGolfThrowComponent::UDiscGolfThrowComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDiscGolfThrowComponent::SetThrowIntent(const FDGThrowIntent& Intent)
{
    // Aim controls may continue to refine presentation before the immutable
    // gameplay command is captured. A committed authored throw owns its copy.
    if (!bThrowActive || (!bThrowCommitted && CurrentPhase == EDGThrowPhase::Aim))
    {
        CurrentIntent = Intent;
        // Gameplay direction is world-space, while the rig consumes this field
        // as a local cosmetic delta. No authored local offset exists yet.
        CurrentIntent.AimYawDegrees = 0.0f;
    }
}

void UDiscGolfThrowComponent::BeginAimPreview()
{
    if (bThrowActive)
    {
        return;
    }

    ResetLifecycle();
    bThrowActive = true;
    NotifyThrowPhase(EDGThrowPhase::Aim);
}

void UDiscGolfThrowComponent::BeginThrow()
{
    if (bThrowActive)
    {
        return;
    }

    ResetLifecycle();
    bThrowActive = true;
    bThrowCommitted = true;
    NotifyThrowPhase(EDGThrowPhase::Aim);
}

void UDiscGolfThrowComponent::CancelThrow()
{
    if (!bThrowActive && CurrentPhase == EDGThrowPhase::Idle)
    {
        ResetLifecycle();
        return;
    }

    bThrowActive = false;
    ResetLifecycle();
    if (CurrentPhase != EDGThrowPhase::Idle)
    {
        CurrentPhase = EDGThrowPhase::Idle;
        OnThrowPhaseChanged.Broadcast(CurrentPhase);
    }
}

void UDiscGolfThrowComponent::NotifyThrowPhase(EDGThrowPhase Phase)
{
    if (!bThrowActive || !CanAcceptAuthoredPhase(Phase) || CurrentPhase == Phase)
    {
        return;
    }

    CurrentPhase = Phase;
    OnThrowPhaseChanged.Broadcast(CurrentPhase);
}

void UDiscGolfThrowComponent::NotifyDiscRelease(USkeletalMeshComponent* CharacterMesh)
{
    if (!bThrowActive
        || !bThrowCommitted
        || bDiscReleaseNotified
        || bThrowFinishedNotified
        || CurrentPhase != EDGThrowPhase::Acceleration
        || !IsValid(CharacterMesh))
    {
        return;
    }

    FDGReleaseData ReleaseData;
    const FName GripBone = GetActiveDiscGripBone();
    ReleaseData.GripWorldTransform = CharacterMesh->DoesSocketExist(GripBone)
        ? CharacterMesh->GetSocketTransform(GripBone)
        : CharacterMesh->GetComponentTransform();

    // Latch before broadcasting so a re-entrant or duplicate branching point
    // cannot ask the gameplay adapter to launch twice.
    bDiscReleaseNotified = true;
    NotifyThrowPhase(EDGThrowPhase::Release);
    OnDiscRelease.Broadcast(ReleaseData);
}

void UDiscGolfThrowComponent::NotifyThrowFinished()
{
    if (!bThrowActive
        || !bThrowCommitted
        || bThrowFinishedNotified
        || CurrentPhase != EDGThrowPhase::Recovery)
    {
        return;
    }

    // The explicit phase notifies own FollowThrough and Recovery. Keep the
    // component active through the montage's remaining recovery frames; the
    // pawn closes the presentation when the montage actually ends.
    bThrowFinishedNotified = true;
    OnThrowFinished.Broadcast();
}

void UDiscGolfThrowComponent::NotifyRecoveryComplete()
{
    if (!bThrowActive || !bThrowCommitted || !bThrowFinishedNotified)
    {
        return;
    }

    bThrowActive = false;
    ResetLifecycle();
    if (CurrentPhase != EDGThrowPhase::Idle)
    {
        CurrentPhase = EDGThrowPhase::Idle;
        OnThrowPhaseChanged.Broadcast(CurrentPhase);
    }
}

bool UDiscGolfThrowComponent::BindCommittedMontageInstance(
    int64 AttemptSerial,
    int32 MontageInstanceId,
    UAnimMontage* Montage)
{
    if (!bThrowActive
        || !bThrowCommitted
        || bThrowFinishedNotified
        || AttemptSerial <= 0
        || MontageInstanceId == INDEX_NONE
        || !IsAuthoredMontageLifecycleSafe(Montage)
        || ActivePresentationAttemptSerial > 0
        || ActiveMontageInstanceId != INDEX_NONE
        || ActiveAuthoredMontage)
    {
        return false;
    }

    ActivePresentationAttemptSerial = AttemptSerial;
    ActiveMontageInstanceId = MontageInstanceId;
    ActiveAuthoredMontage = Montage;
    return true;
}

void UDiscGolfThrowComponent::NotifyThrowPhaseFromMontage(
    EDGThrowPhase Phase,
    int32 MontageInstanceId,
    const UAnimMontage* SourceMontage)
{
    if (IsExpectedMontageInstance(MontageInstanceId, SourceMontage))
    {
        NotifyThrowPhase(Phase);
    }
}

void UDiscGolfThrowComponent::NotifyDiscReleaseFromMontage(
    USkeletalMeshComponent* CharacterMesh,
    int32 MontageInstanceId,
    const UAnimMontage* SourceMontage)
{
    if (!IsExpectedMontageInstance(MontageInstanceId, SourceMontage))
    {
        return;
    }

    // Montage phase notifies are queued, while release is a branching point.
    // A large animation tick can therefore deliver the valid bound release
    // before its earlier queued Acceleration callback. The validated authored
    // timeline proves those milestones were crossed, so replay only the missing
    // ordered presentation phases; release authority still fires exactly once.
    if (CurrentPhase != EDGThrowPhase::Acceleration
        && !ReconcileQueuedPhasesThrough(EDGThrowPhase::Acceleration))
    {
        return;
    }
    NotifyDiscRelease(CharacterMesh);
}

void UDiscGolfThrowComponent::NotifyThrowFinishedFromMontage(
    int32 MontageInstanceId,
    const UAnimMontage* SourceMontage)
{
    if (!IsExpectedMontageInstance(MontageInstanceId, SourceMontage)
        || !bDiscReleaseNotified)
    {
        return;
    }

    // ThrowFinished is also a branching point. If the same tick crossed the
    // queued FollowThrough and Recovery markers, reconcile those already
    // authored milestones before completing gameplay ownership recovery.
    if (CurrentPhase != EDGThrowPhase::Recovery
        && !ReconcileQueuedPhasesThrough(EDGThrowPhase::Recovery))
    {
        return;
    }
    NotifyThrowFinished();
}

bool UDiscGolfThrowComponent::IsAuthoredMontageLifecycleSafe(
    const UAnimMontage* Montage)
{
    if (!IsValid(Montage)
        || !FMath::IsFinite(Montage->GetPlayLength())
        || Montage->GetPlayLength() <= 0.0f)
    {
        return false;
    }

    constexpr int32 AuthoredPhaseCount = UE_ARRAY_COUNT(AuthoredPhases);
    int32 PhaseCounts[AuthoredPhaseCount] = {};
    float PhaseTimes[AuthoredPhaseCount];
    for (float& PhaseTime : PhaseTimes)
    {
        PhaseTime = -1.0f;
    }

    int32 ReleaseCount = 0;
    int32 FinishCount = 0;
    float ReleaseTime = -1.0f;
    float FinishTime = -1.0f;
    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        const float EventTime = Event.GetTriggerTime();
        if (!FMath::IsFinite(EventTime) || EventTime < 0.0f)
        {
            return false;
        }

        if (const UAnimNotify_ThrowPhase* PhaseNotify =
                Cast<UAnimNotify_ThrowPhase>(Event.Notify))
        {
            const int32 PhaseIndex = FindPhaseIndex(
                AuthoredPhases, AuthoredPhaseCount, PhaseNotify->Phase);
            if (PhaseIndex == INDEX_NONE
                || Event.MontageTickType != EMontageNotifyTickType::Queued)
            {
                return false;
            }
            ++PhaseCounts[PhaseIndex];
            PhaseTimes[PhaseIndex] = EventTime;
        }
        else if (Cast<UAnimNotify_DiscRelease>(Event.Notify))
        {
            ++ReleaseCount;
            ReleaseTime = EventTime;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint)
            {
                return false;
            }
        }
        else if (Cast<UAnimNotify_ThrowFinished>(Event.Notify))
        {
            ++FinishCount;
            FinishTime = EventTime;
            if (Event.MontageTickType != EMontageNotifyTickType::BranchingPoint)
            {
                return false;
            }
        }
    }

    if (ReleaseCount != 1 || FinishCount != 1)
    {
        return false;
    }
    for (int32 Index = 0; Index < AuthoredPhaseCount; ++Index)
    {
        if (PhaseCounts[Index] != 1
            || (Index > 0 && PhaseTimes[Index - 1] >= PhaseTimes[Index]))
        {
            return false;
        }
    }

    const int32 AccelerationIndex = FindPhaseIndex(
        AuthoredPhases, AuthoredPhaseCount, EDGThrowPhase::Acceleration);
    const int32 FollowThroughIndex = FindPhaseIndex(
        AuthoredPhases, AuthoredPhaseCount, EDGThrowPhase::FollowThrough);
    const int32 RecoveryIndex = FindPhaseIndex(
        AuthoredPhases, AuthoredPhaseCount, EDGThrowPhase::Recovery);
    return PhaseTimes[AccelerationIndex] < ReleaseTime
        && ReleaseTime < PhaseTimes[FollowThroughIndex]
        && PhaseTimes[RecoveryIndex] < FinishTime
        && FinishTime <= Montage->GetPlayLength() + UE_KINDA_SMALL_NUMBER;
}

FName UDiscGolfThrowComponent::GetActiveDiscGripBone() const
{
    return CharacterProfile && CharacterProfile->Handedness == EDGHandedness::Left
        ? FName(TEXT("disc_grip_l"))
        : FName(TEXT("disc_grip_r"));
}

void UDiscGolfThrowComponent::ResetLifecycle()
{
    ActivePresentationAttemptSerial = 0;
    ActiveMontageInstanceId = INDEX_NONE;
    ActiveAuthoredMontage = nullptr;
    bThrowCommitted = false;
    bDiscReleaseNotified = false;
    bThrowFinishedNotified = false;
}

bool UDiscGolfThrowComponent::IsExpectedMontageInstance(
    int32 MontageInstanceId,
    const UAnimMontage* SourceMontage) const
{
    return bThrowActive
        && bThrowCommitted
        && ActivePresentationAttemptSerial > 0
        && ActiveMontageInstanceId != INDEX_NONE
        && MontageInstanceId == ActiveMontageInstanceId
        && IsValid(SourceMontage)
        && ActiveAuthoredMontage == SourceMontage
        && IsAuthoredMontageLifecycleSafe(ActiveAuthoredMontage);
}

bool UDiscGolfThrowComponent::ReconcileQueuedPhasesThrough(
    EDGThrowPhase TargetPhase)
{
    constexpr int32 RuntimePhaseCount = UE_ARRAY_COUNT(RuntimePhases);
    const int32 CurrentIndex = FindPhaseIndex(
        RuntimePhases, RuntimePhaseCount, CurrentPhase);
    const int32 TargetIndex = FindPhaseIndex(
        RuntimePhases, RuntimePhaseCount, TargetPhase);
    if (CurrentIndex == INDEX_NONE
        || TargetIndex == INDEX_NONE
        || CurrentIndex > TargetIndex)
    {
        return false;
    }

    for (int32 Index = CurrentIndex + 1; Index <= TargetIndex; ++Index)
    {
        // Release is never synthesized: its branching point alone owns the
        // authoritative gameplay handoff and latches before broadcasting.
        if (RuntimePhases[Index] == EDGThrowPhase::Release)
        {
            return false;
        }
        NotifyThrowPhase(RuntimePhases[Index]);
        if (CurrentPhase != RuntimePhases[Index])
        {
            return false;
        }
    }
    return CurrentPhase == TargetPhase;
}

bool UDiscGolfThrowComponent::CanAcceptAuthoredPhase(EDGThrowPhase Phase) const
{
    if (Phase == EDGThrowPhase::Idle)
    {
        return false;
    }

    // Pre-commit aim/waggle accepts no montage phases. This also rejects a
    // queued notify left over from a previously interrupted montage.
    if (!bThrowCommitted)
    {
        return Phase == EDGThrowPhase::Aim;
    }

    switch (Phase)
    {
        case EDGThrowPhase::Aim:
            return CurrentPhase == EDGThrowPhase::Idle
                || CurrentPhase == EDGThrowPhase::Aim;
        case EDGThrowPhase::RunUp:
            return !bDiscReleaseNotified && CurrentPhase == EDGThrowPhase::Aim;
        case EDGThrowPhase::ReachBack:
            return !bDiscReleaseNotified && CurrentPhase == EDGThrowPhase::RunUp;
        case EDGThrowPhase::Plant:
            return !bDiscReleaseNotified && CurrentPhase == EDGThrowPhase::ReachBack;
        case EDGThrowPhase::Acceleration:
            return !bDiscReleaseNotified && CurrentPhase == EDGThrowPhase::Plant;
        case EDGThrowPhase::Release:
            return bDiscReleaseNotified
                && CurrentPhase == EDGThrowPhase::Acceleration;
        case EDGThrowPhase::FollowThrough:
            return bDiscReleaseNotified && CurrentPhase == EDGThrowPhase::Release;
        case EDGThrowPhase::Recovery:
            return bDiscReleaseNotified && CurrentPhase == EDGThrowPhase::FollowThrough;
        default:
            return false;
    }
}
