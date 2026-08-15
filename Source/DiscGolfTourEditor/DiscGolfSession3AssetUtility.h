#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfSession3AssetUtility.generated.h"

/**
 * Deterministic editor-only authoring and strict inspection for the temporary
 * Session 3 RHBH animation assets. Runtime throw/flight authority never enters
 * this utility.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfSession3AssetUtility : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Creates or repairs only the owned Session 3 animation, montage, and ABP graph. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 3")
    static FString AuthorSession3Assets();

    /** Read-only strict contract validation. The returned string is JSON. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 3")
    static FString ValidateSession3Assets();
};
