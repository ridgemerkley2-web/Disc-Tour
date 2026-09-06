#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfPlayabilityTypes.h"
#include "DiscGolfPlayabilityMonitorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGPlayabilityFailureEvent,
    FDGPlayabilityCheckResult,
    Result);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfPlayabilityMonitorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfPlayabilityMonitorComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(BlueprintAssignable) FDGPlayabilityFailureEvent OnPlayabilityFailure;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString CurrentStateName = TEXT("Unknown");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float SecondsSinceProgress = 0.0f;

    UFUNCTION(BlueprintCallable) void ResetMonitor();
    UFUNCTION(BlueprintCallable) void StartMonitoring(const FString& StateName, float InTimeoutSeconds);
    UFUNCTION(BlueprintCallable) void MarkProgress(const FString& ProgressName);
    UFUNCTION(BlueprintCallable) void StopMonitoring(const FString& ResolvedStateName);
    UFUNCTION(BlueprintPure) bool IsMonitoring() const { return bMonitoring && !bSuspended; }
    UFUNCTION(BlueprintPure) bool HasReportedSoftLock() const { return bReportedSoftLock; }
    UFUNCTION(BlueprintCallable) void AdvanceMonitoring(float DeltaTime);
    UFUNCTION(BlueprintCallable) void SuspendMonitoring(const FString& Reason);
    UFUNCTION(BlueprintCallable) void ResumeMonitoring(const FString& StateName);

private:
    float TimeoutSeconds = 0.0f;
    bool bMonitoring = false;
    bool bSuspended = false;
    bool bReportedSoftLock = false;
};
