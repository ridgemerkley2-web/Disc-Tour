#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfPlayabilityTypes.h"
#include "DiscGolfPlayabilityLibrary.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfPlayabilityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Disc Golf|Playability")
    static bool IsBlockingFailure(const FDGPlayabilityCheckResult& Result);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Playability")
    static FDGPlayabilityGateReport BuildReport(
        const TArray<FDGPlayabilityCheckResult>& Results,
        FDateTime StartedUtc,
        FDateTime FinishedUtc
    );

    UFUNCTION(BlueprintPure, Category="Disc Golf|Playability")
    static bool GatePassed(
        const FDGPlayabilityGateReport& Report,
        EDGPlayabilityGateLevel ThroughLevel
    );
};
