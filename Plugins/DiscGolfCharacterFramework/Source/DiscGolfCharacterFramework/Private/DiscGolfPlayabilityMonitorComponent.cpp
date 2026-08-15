#include "DiscGolfPlayabilityMonitorComponent.h"

UDiscGolfPlayabilityMonitorComponent::UDiscGolfPlayabilityMonitorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UDiscGolfPlayabilityMonitorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bMonitorSoftLocks)
    {
        return;
    }

    SecondsSinceProgress += DeltaTime;

    if (SecondsSinceProgress >= FMath::Max(1.0f, SoftLockTimeoutSeconds))
    {
        ReportFailure(
            TEXT("soft_lock_watchdog"),
            EDGPlayabilityGateLevel::CoreLoop,
            EDGPlayabilityFailureCode::SoftLockDetected,
            FString::Printf(
                TEXT("No core-gameplay progress for %.1f seconds while in state '%s'."),
                SecondsSinceProgress,
                *CurrentStateName
            ),
            true
        );

        // Prevent emitting every frame. A real test harness may reset or end the test.
        SecondsSinceProgress = 0.0f;
    }
}

void UDiscGolfPlayabilityMonitorComponent::MarkProgress(FString NewStateName)
{
    CurrentStateName = MoveTemp(NewStateName);
    SecondsSinceProgress = 0.0f;
}

void UDiscGolfPlayabilityMonitorComponent::ReportFailure(
    FName CheckId,
    EDGPlayabilityGateLevel GateLevel,
    EDGPlayabilityFailureCode FailureCode,
    FString Message,
    bool bBlocking)
{
    FDGPlayabilityCheckResult Result;
    Result.CheckId = CheckId;
    Result.GateLevel = GateLevel;
    Result.Status = EDGPlayabilityStatus::Failed;
    Result.FailureCode = FailureCode;
    Result.Message = MoveTemp(Message);
    Result.bBlocking = bBlocking;
    OnPlayabilityFailure.Broadcast(Result);
}

void UDiscGolfPlayabilityMonitorComponent::ResetMonitor()
{
    CurrentStateName = TEXT("Unknown");
    SecondsSinceProgress = 0.0f;
}
