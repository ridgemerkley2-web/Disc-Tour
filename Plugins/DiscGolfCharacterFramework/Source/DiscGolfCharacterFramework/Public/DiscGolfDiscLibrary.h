#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfDiscLibrary.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfDiscLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Disc Golf|Disc")
    static EDGDiscWearStage GetWearStage(const FDGDiscInstance& Disc);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Disc")
    static FDGDiscFlightNumbers ApplyPresentationWear(
        const FDGDiscFlightNumbers& BaseNumbers,
        float Wear01,
        float PlasticStabilityOffset
    );
};
