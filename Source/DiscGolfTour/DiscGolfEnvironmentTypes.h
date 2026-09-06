#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "DiscGolfEnvironmentTypes.generated.h"

class UMaterialInterface;
class UMaterialParameterCollection;

UENUM(BlueprintType)
enum class EDiscGolfEnvironmentAssetCategory : uint8
{
    TreeConiferLarge,
    TreeConiferMedium,
    TreeConiferYoung,
    TreeDeciduousLarge,
    TreeDeciduousMedium,
    Sapling,
    Shrub,
    Fern,
    Grass,
    GroundCover,
    Log,
    Stump,
    RockSmall,
    RockLarge,
    ForestDebris,
    LeafLitter
};

UENUM(BlueprintType)
enum class EDiscGolfEnvironmentCollisionMode : uint8
{
    None,
    TrunkOrBranchBlocking,
    SolidBlocking,
    CanopyOverlap,
    ShrubOverlap
};

UENUM(BlueprintType)
enum class EDiscGolfEnvironmentZoneType : uint8
{
    Tee,
    Fairway,
    SemiRough,
    DeepRough,
    Green,
    OBNatural
};

UENUM(BlueprintType)
enum class EDiscGolfEnvironmentQuality : uint8
{
    Performance,
    High,
    Cinematic
};

UENUM(BlueprintType)
enum class EDiscGolfEnvironmentZoneShape : uint8
{
    SplineCorridor,
    Radial,
    Box
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentMeshVariant
{
    GENERATED_BODY()

    /** Visual mesh only. Tree leaf cards should not own blocking disc collision. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset")
    TSoftObjectPtr<UStaticMesh> VisualMesh;

    /** Optional separate simple mesh for trunks, large branches, rocks, logs, or stumps. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset")
    TSoftObjectPtr<UStaticMesh> CollisionProxyMesh;

    /** Optional non-blocking canopy/shrub proxy consumed by an interaction-volume pass. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset")
    TSoftObjectPtr<UStaticMesh> InteractionProxyMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset", meta=(ClampMin="0.01"))
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset")
    FVector2D UniformScaleRange = FVector2D(0.90f, 1.10f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Optimization")
    bool bNaniteSuitable = false;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentAssetSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset")
    EDiscGolfEnvironmentAssetCategory Category = EDiscGolfEnvironmentAssetCategory::GroundCover;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Asset")
    TArray<FDiscGolfEnvironmentMeshVariant> Variants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collision")
    EDiscGolfEnvironmentCollisionMode CollisionMode = EDiscGolfEnvironmentCollisionMode::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0"))
    float MinimumSpacingCm = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="0.0"))
    float CullStartCm = 15000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="0.0"))
    float CullEndCm = 25000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="0.0"))
    float ShadowCullDistanceCm = 12000.0f;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentZoneRules
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Zone")
    EDiscGolfEnvironmentZoneType ZoneType = EDiscGolfEnvironmentZoneType::Fairway;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Density", meta=(ClampMin="0.0", ClampMax="2.0"))
    float TreeDensity = 0.2f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Density", meta=(ClampMin="0.0", ClampMax="2.0"))
    float SaplingDensity = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Density", meta=(ClampMin="0.0", ClampMax="2.0"))
    float ShrubDensity = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Density", meta=(ClampMin="0.0", ClampMax="2.0"))
    float FernDensity = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Density", meta=(ClampMin="0.0", ClampMax="2.0"))
    float GrassDensity = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Density", meta=(ClampMin="0.0", ClampMax="2.0"))
    float DebrisDensity = 0.05f;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentCourseClearance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clearance", meta=(ClampMin="200.0"))
    float FairwayWidthCm = 1200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clearance", meta=(ClampMin="0.0"))
    float TreeSetbackCm = 550.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clearance", meta=(ClampMin="0.0"))
    float BrushSetbackCm = 250.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clearance", meta=(ClampMin="300.0"))
    float GreenRadiusCm = 1000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Clearance", meta=(ClampMin="300.0"))
    float TeeRadiusCm = 850.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0.0", ClampMax="3.0"))
    float VegetationDensity = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement", meta=(ClampMin="100.0"))
    float MinimumTreeSpacingCm = 520.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Placement")
    int32 RandomSeed = 18437;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentQualitySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality", meta=(ClampMin="0.0", ClampMax="2.0"))
    float DensityScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality", meta=(ClampMin="0.1", ClampMax="3.0"))
    float CullDistanceScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality", meta=(ClampMin="0.0", ClampMax="3.0"))
    float ShadowDistanceScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality")
    bool bAllowNaniteForSuitableSolids = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality")
    bool bEnableCanopyInteractionVolumes = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality")
    bool bEnableGrassShadows = false;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentWindSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind")
    FVector Direction = FVector(1.0f, 0.25f, 0.0f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0", ClampMax="25.0"))
    float SpeedMps = 2.8f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.0", ClampMax="15.0"))
    float GustStrengthMps = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind", meta=(ClampMin="0.01", ClampMax="2.0"))
    float GustFrequencyHz = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Wind")
    TSoftObjectPtr<UMaterialParameterCollection> FoliageWindCollection;
};

/** Swappable surface/material layer for the biome. Marketplace paths belong in preset instances. */
USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentMaterialSlots
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    TSoftObjectPtr<UMaterialInterface> ForestFloor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    TSoftObjectPtr<UMaterialInterface> OpenFairway;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    TSoftObjectPtr<UMaterialInterface> CreekWater;

    /** Optional foliage master used by imported assets that support the shared wind collection. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Materials")
    TSoftObjectPtr<UMaterialInterface> FoliageWindMaster;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentInteractionProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Interaction", meta=(ClampMin="0.05", ClampMax="1.0"))
    float VelocityMultiplier = 0.82f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Interaction", meta=(ClampMin="0.05", ClampMax="1.0"))
    float SpinMultiplier = 0.92f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Interaction", meta=(ClampMin="0.0", ClampMax="2.0"))
    float ReentryCooldownSeconds = 0.12f;
};

USTRUCT(BlueprintType)
struct FDiscGolfEnvironmentSpeciesWeight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species")
    EDiscGolfEnvironmentAssetCategory Category = EDiscGolfEnvironmentAssetCategory::TreeConiferLarge;

    /** Relative biome frequency. Per-mesh weights are configured inside the asset-set slot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Species", meta=(ClampMin="0.0", ClampMax="3.0"))
    float Weight = 1.0f;
};
