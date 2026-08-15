#include "DiscGolfPlayabilityLibrary.h"

bool UDiscGolfPlayabilityLibrary::IsBlockingFailure(
    const FDGPlayabilityCheckResult& Result)
{
    return Result.bBlocking &&
        (Result.Status == EDGPlayabilityStatus::Failed ||
         Result.Status == EDGPlayabilityStatus::Blocked);
}

FDGPlayabilityGateReport UDiscGolfPlayabilityLibrary::BuildReport(
    const TArray<FDGPlayabilityCheckResult>& Results,
    FDateTime StartedUtc,
    FDateTime FinishedUtc)
{
    FDGPlayabilityGateReport Report;
    Report.RunId = FGuid::NewGuid();
    Report.StartedUtc = StartedUtc;
    Report.FinishedUtc = FinishedUtc;
    Report.Results = Results;
    Report.OverallStatus = EDGPlayabilityStatus::Passed;

    for (const FDGPlayabilityCheckResult& Result : Results)
    {
        if (Result.Status == EDGPlayabilityStatus::Passed)
        {
            ++Report.PassedChecks;
        }

        if (Result.Status == EDGPlayabilityStatus::Failed ||
            Result.Status == EDGPlayabilityStatus::Blocked)
        {
            ++Report.FailedChecks;

            if (Result.bBlocking)
            {
                ++Report.BlockingFailures;
                Report.OverallStatus = EDGPlayabilityStatus::Failed;
            }
        }

        if (Result.Status == EDGPlayabilityStatus::Running &&
            Report.OverallStatus != EDGPlayabilityStatus::Failed)
        {
            Report.OverallStatus = EDGPlayabilityStatus::Running;
        }
    }

    return Report;
}

bool UDiscGolfPlayabilityLibrary::GatePassed(
    const FDGPlayabilityGateReport& Report,
    EDGPlayabilityGateLevel ThroughLevel)
{
    for (const FDGPlayabilityCheckResult& Result : Report.Results)
    {
        if (static_cast<uint8>(Result.GateLevel) <= static_cast<uint8>(ThroughLevel) &&
            IsBlockingFailure(Result))
        {
            return false;
        }
    }

    return true;
}
