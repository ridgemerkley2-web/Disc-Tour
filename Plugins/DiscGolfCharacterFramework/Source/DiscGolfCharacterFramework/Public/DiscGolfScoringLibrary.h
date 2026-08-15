#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfScoringLibrary.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfScoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Disc Golf|Scoring")
    static int32 CalculateTotalStrokes(const FDGRoundScorecard& Scorecard);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Scoring")
    static int32 CalculateToPar(const FDGRoundScorecard& Scorecard);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Scoring")
    static bool IsRoundComplete(const FDGRoundScorecard& Scorecard);
};
