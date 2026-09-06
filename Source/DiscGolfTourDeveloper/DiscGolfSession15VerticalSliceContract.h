#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscGolfRoundState.h"
#include "DiscThrowLabTypes.h"
#include "DiscGolfDiscTypes.h"

/** One completed production-authority shot in the bounded Hole 1 proof. */
struct DISCGOLFTOURDEVELOPER_API FDiscGolfSession15ShotEvidence
{
    EDiscShotContext StartContext = EDiscShotContext::Drive;
    EDiscShotContext ResultingContext = EDiscShotContext::Drive;
    FVector StartLieLocationCm = FVector::ZeroVector;
    FVector FinalWorldLocationCm = FVector::ZeroVector;
    FVector ResultingLieLocationCm = FVector::ZeroVector;
    float StartDistanceToBasketMeters = 0.0f;
    float RemainingDistanceToBasketMeters = 0.0f;
    int32 StrokeAfterShot = 0;
    int32 ActualTrajectorySampleCount = 0;
    EDiscGolfPenaltyType PenaltyType = EDiscGolfPenaltyType::None;
    int32 PenaltyStrokes = 0;
    bool bHoledOut = false;
    FDiscFlightTelemetry FinalTelemetry;
};

/** Counts and ownership assertions observed around the existing launch lifecycle. */
struct DISCGOLFTOURDEVELOPER_API FDiscGolfSession15AuthorityEvidence
{
    int32 ThrowAttemptCount = 0;
    int32 CommittedReleaseCount = 0;
    int32 AcceptedLaunchCount = 0;
    int32 GameplayDiscSpawnCount = 0;
    int32 StrokeDelta = 0;
    bool bUsedExistingGripReleaseAuthority = false;
    bool bUsedExistingFixedStepFlightAuthority = false;
    bool bUsedExistingRoundAndLieAuthority = false;
    bool bAlternateSolverOrScoringAuthorityObserved = false;
    bool bLifecycleQuiescentAfterCompletion = false;
};

/** Truthful boundary between a measured technical pass and production approval. */
struct DISCGOLFTOURDEVELOPER_API FDiscGolfSession15ReadinessEvidence
{
    bool bRenderedIntegratedPerformanceMeasured = false;
    bool bPerformanceBudgetPassed = false;
    bool bFinalVisualArtApproved = false;
    bool bMeasuredFlightCalibrationApproved = false;
    bool bAuthoredProductionAudioPresent = false;
    bool bTechnicalAcceptancePassed = false;
    bool bPolishedSliceApproved = false;
    bool bPublicReleaseReady = false;
};

/**
 * Side-effect-free value evidence for the Session 15 static contract. It consumes
 * existing authorities and observations only; it never launches, scores, saves,
 * resimulates, or mutates the runtime world.
 */
struct DISCGOLFTOURDEVELOPER_API FDiscGolfSession15VerticalSliceEvidence
{
    int32 ContractVersion = 1;

    FName ActivePresentingBrandId = NAME_None;
    FName ActiveEquipmentBrandId = NAME_None;
    FName FrozenDormantDonorBrandId = NAME_None;
    bool bPremiumDonorDormant = false;

    FDiscGolfHoleBlockoutDefinition Hole;
    FDGDiscInstance TeeDriver;
    FDGDiscInstance Putter;
    FDiscGolfSession15AuthorityEvidence Authority;
    TArray<FDiscGolfSession15ShotEvidence> Shots;
    FDiscGolfRoundState Round;

    TArray<FDiscTrajectorySample> ActualReplaySource;
    TArray<FDiscGroundTransition> ActualReplayTransitions;
    FDiscThrowLabRecord ThrowLabRecord;

    int32 PlayerProfileSchemaVersion = 0;
    FDiscGolfPlayerSettings PlayerSettings;
    FString ExternalUserDir;
    bool bProfileRoundTripExact = false;
    bool bProductionProfileUntouched = false;

    FDiscGolfSession15ReadinessEvidence Readiness;
};

namespace DiscGolfSession15VerticalSliceContract
{
    inline constexpr int32 CurrentContractVersion = 1;
    inline constexpr float PineRidgeHole1DistanceFeet = 361.855072f;

    DISCGOLFTOURDEVELOPER_API FName GenericBrandId();
    DISCGOLFTOURDEVELOPER_API FName DormantPremiumDonorBrandId();
    DISCGOLFTOURDEVELOPER_API FString ExternalUserDirRoot();
    DISCGOLFTOURDEVELOPER_API bool IsExternalGuidUserDir(const FString& UserDir);

    DISCGOLFTOURDEVELOPER_API bool ValidateEvidence(
        const FDiscGolfSession15VerticalSliceEvidence& Evidence,
        FString& OutError);

    /** Commits only after full validation; failure leaves OutAccepted byte-for-value unchanged. */
    DISCGOLFTOURDEVELOPER_API bool ValidateAndCommitEvidence(
        const FDiscGolfSession15VerticalSliceEvidence& Candidate,
        FDiscGolfSession15VerticalSliceEvidence& OutAccepted,
        FString& OutError);
}
