#pragma once

#include "CoreMinimal.h"
#include "DiscGolfEnvironmentTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfEnvironmentAssetBinder.generated.h"

class UDiscGolfEnvironmentAssetSet;
class UStaticMesh;

UENUM(BlueprintType)
enum class EDiscGolfEnvironmentBindingStatus : uint8
{
    Ready,
    Missing,
    NeedsCollision,
    NeedsLodReview,
    NeedsWindBinding,
    ScaleWarning,
    Ambiguous
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentBindingCandidate
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath AssetPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath CollisionProxyPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FSoftObjectPath InteractionProxyPath;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ClassificationScore = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 LodCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 MaterialCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 SimpleCollisionShapeCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bNaniteEnabled = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bHasWindParameters = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bFoliageSuitable = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector BoundsSizeCm = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<EDiscGolfEnvironmentBindingStatus> Statuses;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Notes;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentBindingProposal
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    EDiscGolfEnvironmentAssetCategory Category = EDiscGolfEnvironmentAssetCategory::GroundCover;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bRetainedExistingBinding = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDiscGolfEnvironmentBindingCandidate> Candidates;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentBindingScan
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FString> VendorContentRoots;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FDiscGolfEnvironmentBindingProposal> Proposals;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ReportPath;
};

/** Separates successful report generation from the stricter production-readiness decision. */
USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentValidationResult
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bReportGenerated = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bStructurallyComplete = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bProductionReady = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString ReportPath;
};

/** Editor-only, approval-gated bridge between imported Fab content and the runtime asset set. */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfEnvironmentAssetBinder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Environment Assets")
    static FDiscGolfEnvironmentBindingScan ScanEnvironmentAssets(
        const TArray<FString>& VendorContentRoots,
        UDiscGolfEnvironmentAssetSet* ExistingAssetSet);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Environment Assets")
    static FDiscGolfEnvironmentBindingScan ProposeBindings(
        const TArray<FString>& VendorContentRoots,
        UDiscGolfEnvironmentAssetSet* ExistingAssetSet);

    /** C++ tooling/test entry point that keeps non-production reports out of Saved/Developer. */
    static FDiscGolfEnvironmentBindingScan ProposeBindingsToReport(
        const TArray<FString>& VendorContentRoots,
        UDiscGolfEnvironmentAssetSet* ExistingAssetSet,
        const FString& SavedRelativeReportPath);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Environment Assets")
    static bool ValidateEnvironmentAssets(
        UDiscGolfEnvironmentAssetSet* AssetSet,
        FString& OutReportPath);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Environment Assets")
    static FDiscGolfEnvironmentValidationResult ValidateEnvironmentAssetReadiness(
        UDiscGolfEnvironmentAssetSet* AssetSet);

    /** C++ tooling/test entry point that writes to a caller-selected path below Saved. */
    static FDiscGolfEnvironmentValidationResult ValidateEnvironmentAssetReadinessToReport(
        UDiscGolfEnvironmentAssetSet* AssetSet,
        const FString& SavedRelativeReportPath);

    /**
     * Replaces a category with only the explicitly approved paths, preserves metadata for
     * already-present meshes, and saves persistent assets. Scanning never mutates the data asset.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Environment Assets")
    static bool ApplyApprovedBindings(
        UDiscGolfEnvironmentAssetSet* AssetSet,
        EDiscGolfEnvironmentAssetCategory Category,
        const TArray<FSoftObjectPath>& ApprovedVisualMeshes,
        FString& OutError);

    static TArray<EDiscGolfEnvironmentBindingStatus> ValidateMeshForCategory(
        EDiscGolfEnvironmentAssetCategory Category,
        const UStaticMesh* Mesh,
        FString& OutNotes);

    /**
     * Scores only the path below its configured vendor root. This prevents product/root names
     * such as "SpruceForest" or "Stump_Scanned" from classifying every mesh in the pack.
     * Demo, example, showcase, map, and mannequin subtrees are always excluded.
     */
    static float ScoreAssetPathForCategory(
        const FString& AssetPath,
        const TArray<FString>& VendorContentRoots,
        EDiscGolfEnvironmentAssetCategory Category);

    /**
     * Returns soft height fitness for conifer strata after semantic-anchor classification.
     * Zero means the mesh must not be proposed for that stratum. Mature results remain
     * advisory and require explicit visual approval.
     */
    static float ScoreTreeBoundsForCategory(
        EDiscGolfEnvironmentAssetCategory Category,
        const FVector& BoundsSizeCm);

    /** Validates the complete assigned variant against its slot's collision policy. */
    static TArray<EDiscGolfEnvironmentBindingStatus> ValidateVariantForSlot(
        const FDiscGolfEnvironmentAssetSlot& Slot,
        const FDiscGolfEnvironmentMeshVariant& Variant,
        FString& OutNotes);

private:
    static bool WriteReport(
        const FDiscGolfEnvironmentBindingScan& Scan,
        FString& OutReportPath,
        const FString& SavedRelativeReportPath,
        bool bReadinessEvaluated,
        bool bStructurallyComplete,
        bool bProductionReady);
};
