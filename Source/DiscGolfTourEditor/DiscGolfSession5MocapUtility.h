#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfSession5MocapUtility.generated.h"

/**
 * Editor-only authoring and read-only inspection for the Session 5 synthetic
 * mocap pipeline fixture. The fixture is explicitly test-only and never owns
 * runtime release, throw-command, disc-flight, or profile authority.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfSession5MocapUtility : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Authors missing fixture packages, then applies the strict contract. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 5")
    static FString AuthorSession5Fixture();

    /** Strict reflected inspection only. The returned string is JSON. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 5")
    static FString ValidateSession5Fixture();

    /** Read-only local/component track evidence for the legacy import-scale compatibility gate. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Session 5")
    static FString ProbeSession5UnitCompatibility();
};
