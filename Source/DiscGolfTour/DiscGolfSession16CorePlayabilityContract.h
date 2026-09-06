#pragma once

#include "CoreMinimal.h"
#include "DiscGolfPlayabilityTypes.h"

/** Immutable metadata for one prepared v1.5 playability check. */
struct DISCGOLFTOUR_API FDiscGolfSession16RequiredCheck
{
    FName CheckId = NAME_None;
    EDGPlayabilityGateLevel GateLevel = EDGPlayabilityGateLevel::Smoke;
    EDGPlayabilityFailureCode FailureCode = EDGPlayabilityFailureCode::UnexpectedError;
    float TimeoutSeconds = 0.0f;
};

/**
 * Side-effect-free observation supplied by the existing runtime authority.
 * The contract assigns gate and failure metadata; callers cannot self-certify it.
 */
struct DISCGOLFTOUR_API FDiscGolfSession16ObservedCheck
{
    FName CheckId = NAME_None;
    EDGPlayabilityStatus Status = EDGPlayabilityStatus::NotRun;
    FString Evidence;
    float DurationSeconds = 0.0f;
};

namespace DiscGolfSession16CorePlayabilityContract
{
    inline constexpr int32 RequiredCheckCount = 44;
    inline constexpr int32 FailureCodeCount = 44;

    /** Exact ordered DG_CorePlayabilityGate v1 catalog: 11 / 16 / 7 / 10. */
    DISCGOLFTOUR_API const TArray<FDiscGolfSession16RequiredCheck>& RequiredChecks();

    /** Complete ordered EDGPlayabilityFailureCode catalog excluding None. */
    DISCGOLFTOUR_API const TArray<EDGPlayabilityFailureCode>& FailureCodes();

    DISCGOLFTOUR_API const FDiscGolfSession16RequiredCheck* FindRequiredCheck(
        FName CheckId);

    /**
     * Converts actual observations to authoritative framework results. Requires
     * exactly one observation for every prepared check. Failure is transactional.
     */
    DISCGOLFTOUR_API bool BuildReportFromObservations(
        const TArray<FDiscGolfSession16ObservedCheck>& Observations,
        FDateTime StartedUtc,
        FDateTime FinishedUtc,
        FDGPlayabilityGateReport& OutReport,
        FString& OutError);

    /** Validates exact catalog coverage, metadata, counters, and report semantics. */
    DISCGOLFTOUR_API bool ValidateReport(
        const FDGPlayabilityGateReport& Report,
        FString& OutError);
}
