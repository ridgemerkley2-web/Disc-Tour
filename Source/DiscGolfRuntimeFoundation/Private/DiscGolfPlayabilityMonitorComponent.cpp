#include "DiscGolfPlayabilityMonitorComponent.h"

UDiscGolfPlayabilityMonitorComponent::UDiscGolfPlayabilityMonitorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDiscGolfPlayabilityMonitorComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    AdvanceMonitoring(DeltaTime);
}

void UDiscGolfPlayabilityMonitorComponent::AdvanceMonitoring(float DeltaTime)
{
    if (!bMonitoring || bSuspended || bReportedSoftLock
        || !FMath::IsFinite(DeltaTime) || DeltaTime <= 0.0f)
    {
        return;
    }
    SecondsSinceProgress += DeltaTime;
    if (SecondsSinceProgress < TimeoutSeconds)
    {
        return;
    }
    bReportedSoftLock = true;
    FDGPlayabilityCheckResult Failure;
    Failure.CheckId = FName(*CurrentStateName);
    Failure.Status = EDGPlayabilityStatus::Failed;
    Failure.FailureCode = EDGPlayabilityFailureCode::Timeout;
    Failure.Message = FString::Printf(TEXT("Playability monitor timed out in %s."), *CurrentStateName);
    Failure.bBlocking = true;
    OnPlayabilityFailure.Broadcast(Failure);
}

void UDiscGolfPlayabilityMonitorComponent::ResetMonitor()
{
    bMonitoring = false;
    bSuspended = false;
    bReportedSoftLock = false;
    TimeoutSeconds = 0.0f;
    SecondsSinceProgress = 0.0f;
    CurrentStateName = TEXT("Unknown");
    SetComponentTickEnabled(false);
}

void UDiscGolfPlayabilityMonitorComponent::StartMonitoring(
    const FString& StateName,
    float InTimeoutSeconds)
{
    CurrentStateName = StateName.IsEmpty() ? TEXT("Unknown") : StateName;
    TimeoutSeconds = FMath::Max(
        FMath::IsFinite(InTimeoutSeconds) ? InTimeoutSeconds : 0.0f, 0.001f);
    SecondsSinceProgress = 0.0f;
    bMonitoring = true;
    bSuspended = false;
    bReportedSoftLock = false;
    SetComponentTickEnabled(true);
}

void UDiscGolfPlayabilityMonitorComponent::MarkProgress(const FString& ProgressName)
{
    if (bMonitoring && !ProgressName.IsEmpty())
    {
        CurrentStateName = ProgressName;
        SecondsSinceProgress = 0.0f;
        bReportedSoftLock = false;
    }
}

void UDiscGolfPlayabilityMonitorComponent::StopMonitoring(const FString& ResolvedStateName)
{
    bMonitoring = false;
    bSuspended = false;
    bReportedSoftLock = false;
    TimeoutSeconds = 0.0f;
    SecondsSinceProgress = 0.0f;
    CurrentStateName = ResolvedStateName.IsEmpty() ? TEXT("Unknown") : ResolvedStateName;
    SetComponentTickEnabled(false);
}

void UDiscGolfPlayabilityMonitorComponent::SuspendMonitoring(const FString& Reason)
{
    if (bMonitoring)
    {
        bSuspended = true;
        CurrentStateName = Reason.IsEmpty() ? CurrentStateName : Reason;
    }
}

void UDiscGolfPlayabilityMonitorComponent::ResumeMonitoring(const FString& StateName)
{
    if (bMonitoring && bSuspended)
    {
        bSuspended = false;
        SecondsSinceProgress = 0.0f;
        bReportedSoftLock = false;
        CurrentStateName = StateName.IsEmpty() ? CurrentStateName : StateName;
    }
}
