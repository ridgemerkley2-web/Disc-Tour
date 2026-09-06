#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfProductionMotionAuthoringUtility.generated.h"

/**
 * Guarded editor-only authoring for the project-authored procedural motion candidates.
 * The recipe is independent of the Session 3/5 synthetic fixtures and this utility
 * never owns gameplay release, flight, pawn world motion, or approval state.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfProductionMotionAuthoringUtility
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Creates the active accepted generation only in the guarded commandlet workflow. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Production Motion")
    static FString AuthorProductionMotionAssets();

    /** Strict read-only validation of the active accepted generation. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Production Motion")
    static FString ValidateProductionMotionAssets();

    /**
     * Creates one immutable recipe generation. RecipeVersion must be exactly
     * "v1" through "v7" and must match -DGProductionMotionVersion on the guarded
     * command line. Existing packages are always refused. Authoring v7 does not
     * change the independently selected active runtime generation.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Production Motion")
    static FString AuthorProductionMotionAssetsForVersion(
        const FString& RecipeVersion);

    /** Strict read-only validation for one explicitly selected generation. */
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Production Motion")
    static FString ValidateProductionMotionAssetsForVersion(
        const FString& RecipeVersion);
};
