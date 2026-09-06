#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfScoringLibrary.generated.h"

UCLASS()
class DISCGOLFTOURDEVELOPER_API UDiscGolfScoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure) static int32 CalculateTotalStrokes(const FDGRoundScorecard& Scorecard);
    UFUNCTION(BlueprintPure) static int32 CalculateToPar(const FDGRoundScorecard& Scorecard);
    UFUNCTION(BlueprintPure) static bool IsRoundComplete(const FDGRoundScorecard& Scorecard);
};
