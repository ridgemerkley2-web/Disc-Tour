#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfSession4AssetUtility.generated.h"

/** Editor-only, deterministic authoring and strict inspection for Session 4. */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfSession4AssetUtility : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Authors only the owned CR, ABP, and empty creator-widget wiring. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 4")
    static FString AuthorSession4Assets();

    /** Read-only strict contract validation. The returned string is JSON. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 4")
    static FString ValidateSession4Assets();
};
