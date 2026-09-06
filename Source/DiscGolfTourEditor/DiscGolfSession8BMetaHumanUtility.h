#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfSession8BMetaHumanUtility.generated.h"

/**
 * Editor-only, transactional authoring and strict inspection for the three
 * canonical Session 8B assembled-MetaHuman runtime assets.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfSession8BMetaHumanUtility
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Authors the exact wrapper, retargeter and backend profile, then validates them. */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString AuthorSession8BMetaHumanAssets();

    /**
     * Strict no-write inspection. It unloads and reloads the exact canonical
     * packages so the returned JSON proves disk state rather than live objects.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString ValidateSession8BMetaHumanAssets();

    /**
     * Changes only the loaded canonical retargeter's Run IK Rig op from the
     * legacy enabled state to the verified fixed-presentation disabled state.
     * The caller owns the exact one-package save and rollback transaction.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString PrepareSession8BMetaHumanRunIKCorrection();

    /**
     * Changes only the loaded canonical retargeter's Root Motion op from the
     * exact legacy enabled state to the fixed-presentation disabled state.
     * The caller owns the exact one-package save and rollback transaction.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString PrepareSession8BMetaHumanRootMotionCorrection();

    /**
     * Changes only the MakeTransform rotation defaults in the loaded canonical
     * wrapper's Configure and Apply graphs from the exact legacy +90 yaw state
     * to the verified identity-relative state. The caller owns the exact
     * one-package save and rollback transaction.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString PrepareSession8BMetaHumanWrapperOrientationCorrection();

    /**
     * Strict no-write diagnosis of the generated Body/Outfit bind contract.
     * It compares every reference bone and CPU-skins every Outfit render LOD
     * once against the Outfit bind pose and once against the Body bind pose.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString DiagnoseSession8BMetaHumanOutfit();

    /**
     * Strict no-write counterfactual for the canonical retargeter's Root Motion
     * op. Two transient processor/profile pairs compare current settings with
     * only Root Motion disabled; canonical UObjects and package bytes are read
     * and verified but never edited or saved.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Character|Session 8B")
    static FString DiagnoseSession8BMetaHumanRootMotionCounterfactual();
};
