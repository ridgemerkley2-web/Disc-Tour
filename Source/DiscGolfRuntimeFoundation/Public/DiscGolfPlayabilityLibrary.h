#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfPlayabilityTypes.h"
#include "DiscGolfPlayabilityLibrary.generated.h"

UCLASS()
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfPlayabilityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure)
    static FDGPlayabilityGateReport BuildReport(
        const TArray<FDGPlayabilityCheckResult>& Results,
        FDateTime StartedUtc,
        FDateTime FinishedUtc);

    UFUNCTION(BlueprintPure)
    static bool GatePassed(const FDGPlayabilityGateReport& Report, EDGPlayabilityGateLevel Gate);
};
