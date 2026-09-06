#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfAuthenticV008RetargetUtility.generated.h"

/** Append-only, diagnostic-only authentic v008 offline retarget bridge. */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfAuthenticV008RetargetUtility
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Authors exactly one DGMaster RAW sequence when DG_V008_RAW_RETARGET_AUTHORING=1. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Animation|Authentic v008")
    static FString AuthorImg2396RawRetarget();

    /** Read-only exact-path validation. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Animation|Authentic v008")
    static FString ValidateImg2396RawRetarget();

    /** Authors an append-only repaired derivative; never mutates the rejected RAW evidence. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Animation|Authentic v008")
    static FString AuthorImg2396NormalizedRetarget();

    /** Read-only validation of the separately named repaired derivative. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Animation|Authentic v008")
    static FString ValidateImg2396NormalizedRetarget();

    /** Authors an append-only timing crop from NORMALIZED; never retimes or promotes it. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Animation|Authentic v008")
    static FString AuthorImg2396CleanedRetarget();

    /** Cold read-only validation of the separately named CLEANED timing crop. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Animation|Authentic v008")
    static FString ValidateImg2396CleanedRetarget();
};
