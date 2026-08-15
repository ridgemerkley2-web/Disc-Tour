#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfPlayabilityTypes.h"
#include "DiscGolfPlayabilityMonitorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnPlayabilityFailure,
    FDGPlayabilityCheckResult,
    Result
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfPlayabilityMonitorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfPlayabilityMonitorComponent();

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Playability")
    float SoftLockTimeoutSeconds = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Playability")
    bool bMonitorSoftLocks = true;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Playability")
    FString CurrentStateName = TEXT("Unknown");

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Playability")
    float SecondsSinceProgress = 0.0f;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Playability")
    FDGOnPlayabilityFailure OnPlayabilityFailure;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Playability")
    void MarkProgress(FString NewStateName);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Playability")
    void ReportFailure(
        FName CheckId,
        EDGPlayabilityGateLevel GateLevel,
        EDGPlayabilityFailureCode FailureCode,
        FString Message,
        bool bBlocking
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Playability")
    void ResetMonitor();
};
