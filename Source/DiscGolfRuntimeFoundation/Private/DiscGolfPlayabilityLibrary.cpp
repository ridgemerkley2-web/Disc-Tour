#include "DiscGolfPlayabilityLibrary.h"

namespace
{
bool IsKnownGate(const EDGPlayabilityGateLevel Gate)
{
    switch (Gate)
    {
        case EDGPlayabilityGateLevel::Smoke:
        case EDGPlayabilityGateLevel::CoreLoop:
        case EDGPlayabilityGateLevel::Round:
        case EDGPlayabilityGateLevel::Persistence:
            return true;
        default:
            return false;
    }
}

bool IsKnownStatus(const EDGPlayabilityStatus Status)
{
    switch (Status)
    {
        case EDGPlayabilityStatus::NotRun:
        case EDGPlayabilityStatus::Running:
        case EDGPlayabilityStatus::Passed:
        case EDGPlayabilityStatus::Failed:
        case EDGPlayabilityStatus::Blocked:
            return true;
        default:
            return false;
    }
}
}

FDGPlayabilityGateReport UDiscGolfPlayabilityLibrary::BuildReport(
    const TArray<FDGPlayabilityCheckResult>& Results,
    FDateTime StartedUtc,
    FDateTime FinishedUtc)
{
    FDGPlayabilityGateReport Report;
    Report.Results.Append(Results);
    Report.RunId = FGuid::NewGuid();
    Report.StartedUtc = MoveTemp(StartedUtc);
    Report.FinishedUtc = MoveTemp(FinishedUtc);

    bool bHasUnfinishedCheck = false;
    bool bHasActiveCheck = false;
    bool bHasInvalidCheck = false;
    for (const FDGPlayabilityCheckResult& Check : Report.Results)
    {
        switch (Check.Status)
        {
        case EDGPlayabilityStatus::Passed:
            ++Report.PassedChecks;
            break;
        case EDGPlayabilityStatus::Failed:
        case EDGPlayabilityStatus::Blocked:
            ++Report.FailedChecks;
            Report.BlockingFailures += static_cast<int32>(Check.bBlocking);
            break;
        case EDGPlayabilityStatus::Running:
            bHasActiveCheck = true;
            break;
        case EDGPlayabilityStatus::NotRun:
            bHasUnfinishedCheck = true;
            break;
        default:
            bHasInvalidCheck = true;
            break;
        }
    }

    Report.OverallStatus = EDGPlayabilityStatus::Passed;
    if (bHasInvalidCheck || Report.BlockingFailures > 0)
    {
        Report.OverallStatus = EDGPlayabilityStatus::Failed;
    }
    else if (bHasActiveCheck)
    {
        Report.OverallStatus = EDGPlayabilityStatus::Running;
    }
    else if (bHasUnfinishedCheck || Report.Results.IsEmpty())
    {
        Report.OverallStatus = EDGPlayabilityStatus::NotRun;
    }
    return Report;
}

bool UDiscGolfPlayabilityLibrary::GatePassed(
    const FDGPlayabilityGateReport& Report,
    EDGPlayabilityGateLevel Gate)
{
    if (!IsKnownGate(Gate) || Report.Results.IsEmpty())
    {
        return false;
    }

    int32 MatchingChecks = 0;
    int32 PassingChecks = 0;
    int32 PassedChecks = 0;
    int32 FailedChecks = 0;
    int32 BlockingFailures = 0;
    bool bHasUnfinishedCheck = false;
    bool bHasActiveCheck = false;
    for (const FDGPlayabilityCheckResult& Check : Report.Results)
    {
        if (!IsKnownGate(Check.GateLevel) || !IsKnownStatus(Check.Status))
        {
            return false;
        }
        switch (Check.Status)
        {
            case EDGPlayabilityStatus::Passed:
                ++PassedChecks;
                break;
            case EDGPlayabilityStatus::Failed:
            case EDGPlayabilityStatus::Blocked:
                ++FailedChecks;
                BlockingFailures += static_cast<int32>(Check.bBlocking);
                break;
            case EDGPlayabilityStatus::Running:
                bHasActiveCheck = true;
                break;
            case EDGPlayabilityStatus::NotRun:
                bHasUnfinishedCheck = true;
                break;
            default:
                return false;
        }
        const bool bAtRequestedGate = Check.GateLevel == Gate;
        MatchingChecks += static_cast<int32>(bAtRequestedGate);
        PassingChecks += static_cast<int32>(
            bAtRequestedGate && Check.Status == EDGPlayabilityStatus::Passed);
    }

    EDGPlayabilityStatus ExpectedOverall = EDGPlayabilityStatus::Passed;
    if (BlockingFailures > 0)
    {
        ExpectedOverall = EDGPlayabilityStatus::Failed;
    }
    else if (bHasActiveCheck)
    {
        ExpectedOverall = EDGPlayabilityStatus::Running;
    }
    else if (bHasUnfinishedCheck)
    {
        ExpectedOverall = EDGPlayabilityStatus::NotRun;
    }
    if (Report.PassedChecks != PassedChecks
        || Report.FailedChecks != FailedChecks
        || Report.BlockingFailures != BlockingFailures
        || Report.OverallStatus != ExpectedOverall)
    {
        return false;
    }
    return MatchingChecks > 0 && MatchingChecks == PassingChecks;
}
