#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAIGolferProfile.h"
#include "DiscGolfAITypes.h"
#include "DiscGolfTypes.h"

/**
 * One completed project-owned flight that may be considered by the AI planner.
 * The adapter deliberately accepts measurements instead of predicting flight.
 */
struct DISCGOLFTOURDEVELOPER_API FDiscGolfAIMeasuredShotOutcome
{
    FThrowCommand Command;
    FVector StartWorldLocationCm = FVector::ZeroVector;
    FVector FinalWorldLocationCm = FVector::ZeroVector;
    FDiscFlightTelemetry FinalTelemetry;
};

namespace DiscGolfAIPlannerAdapter
{
    /** Builds the bounded, unbranded Session 14 proof profile without mutating OutProfile on failure. */
    DISCGOLFTOURDEVELOPER_API bool BuildGenericProofProfile(
        UObject* Outer,
        UDiscGolfAIGolferProfile*& OutProfile,
        FString& OutError);

    DISCGOLFTOURDEVELOPER_API bool ValidateProfile(
        const UDiscGolfAIGolferProfile& Profile,
        FString& OutError);

    DISCGOLFTOURDEVELOPER_API bool ValidateContext(
        const FDGAIShotContext& Context,
        FString& OutError);

    DISCGOLFTOURDEVELOPER_API bool ValidateCandidate(
        const UDiscGolfAIGolferProfile& Profile,
        const FDGAIShotCandidate& Candidate,
        FString& OutError);

    /** Converts one real completed outcome into a planner DTO; output is atomic. */
    DISCGOLFTOURDEVELOPER_API bool BuildCandidateFromMeasuredOutcome(
        const UDiscGolfAIGolferProfile& Profile,
        const FDGAIShotContext& Context,
        const FDiscGolfAIMeasuredShotOutcome& Outcome,
        FDGAIShotCandidate& OutCandidate,
        FString& OutError);

    /**
     * Stable-sorts validated measurements, delegates utility scoring/selection to
     * UDiscGolfAIShotPlannerComponent, and translates the winner back to the
     * existing project command. Both outputs remain unchanged on any failure.
     */
    DISCGOLFTOURDEVELOPER_API bool SelectMeasuredShot(
        const UDiscGolfAIGolferProfile& Profile,
        const FDGAIShotContext& Context,
        const TArray<FDiscGolfAIMeasuredShotOutcome>& Outcomes,
        FThrowCommand& OutCommand,
        FDGAIShotCandidate& OutCandidate,
        FString& OutError);
}
