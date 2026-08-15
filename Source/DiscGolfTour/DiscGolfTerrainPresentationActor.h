#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfTerrainPresentationActor.generated.h"

class UProceduralMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
struct FDiscGolfHoleBlockoutDefinition;

/**
 * Deterministic visual terrain beneath the sealed authored collision surfaces.
 * The mesh has no collision and cannot participate in lie or flight queries.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfTerrainPresentationActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfTerrainPresentationActor();

    void Configure(const FDiscGolfHoleBlockoutDefinition& Definition, int32 TerrainSeed);
    bool ConfigureCourse(
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        const TArray<int32>& TerrainSeeds,
        float GrassDensityScale,
        float CullDistanceScale);

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetTriangleCount() const { return TriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetConnectorTriangleCount() const { return ConnectorTriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetFairwayTriangleCount() const { return FairwayTriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetShorelineTriangleCount() const { return ShorelineTriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetTrailWearPatchCount() const { return TrailWearPatchCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetTrailWearTriangleCount() const { return TrailWearTriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGrassBladeClusterCount() const { return GrassBladeClusterCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGrassTriangleCount() const { return GrassTriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGrassCardInstanceCount() const { return GrassCardInstanceCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGrassSpeciesVariantCount() const { return GrassSpeciesVariantCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetLitterClusterCount() const { return LitterClusterCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetLitterTriangleCount() const { return LitterTriangleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetLitterVariantCount() const { return LitterVariantCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGrassBladeClusterCountForHole(int32 HoleNumber) const
    {
        return GrassClustersByHole.FindRef(HoleNumber);
    }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetFernClusterCountForHole(int32 HoleNumber) const
    {
        return FernClustersByHole.FindRef(HoleNumber);
    }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetLitterClusterCountForHole(int32 HoleNumber) const
    {
        return LitterClustersByHole.FindRef(HoleNumber);
    }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGroundCoverCullStartDistanceCm() const { return GroundCoverCullStartDistanceCm; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGroundCoverCullEndDistanceCm() const { return GroundCoverCullEndDistanceCm; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    float GetMaxGroundCoverSlopeDegrees() const { return MaxGroundCoverSlopeDegrees; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetGroundMacroVariationSpan() const
    {
        return MaxGroundMacroVariation - MinGroundMacroVariation;
    }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    float GetMaxShorelineErosionOffsetCm() const
    {
        return MaxShorelineErosionOffsetCm;
    }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetShoreRockInstanceCount() const { return ShoreRockInstanceCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetShoreReedClusterCount() const { return ShoreReedClusterCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetShoreReedStemInstanceCount() const { return ShoreReedStemInstanceCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetShoreDeadfallInstanceCount() const { return ShoreDeadfallInstanceCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetShoreAccentVariantCount() const { return ShoreAccentVariantCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    float GetMinShorelineAccentRouteClearanceCm() const
    {
        return MinShorelineAccentRouteClearanceCm;
    }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    int32 GetCourseHoleCount() const { return CourseHoleCount; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsReady() const { return bReady; }

    UFUNCTION(BlueprintPure, Category="Course|Presentation")
    bool IsCollisionInvariant() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> TerrainMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> GrassMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FineGrassInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> SedgeGrassInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FernGrassInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> LeafLitterInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> NeedleLitterInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShoreRockInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShoreReedInstances;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> ShoreDeadfallInstances;

    UPROPERTY(VisibleAnywhere)
    int32 TriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 ConnectorTriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 FairwayTriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 ShorelineTriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 TrailWearPatchCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 TrailWearTriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 GrassBladeClusterCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 GrassTriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 GrassCardInstanceCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 GrassSpeciesVariantCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 LitterClusterCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 LitterTriangleCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 LitterVariantCount = 0;

    UPROPERTY(VisibleAnywhere)
    TMap<int32, int32> GrassClustersByHole;

    UPROPERTY(VisibleAnywhere)
    TMap<int32, int32> FernClustersByHole;

    UPROPERTY(VisibleAnywhere)
    TMap<int32, int32> LitterClustersByHole;

    UPROPERTY(VisibleAnywhere)
    int32 GroundCoverCullStartDistanceCm = 0;

    UPROPERTY(VisibleAnywhere)
    int32 GroundCoverCullEndDistanceCm = 0;

    UPROPERTY(VisibleAnywhere)
    float MaxGroundCoverSlopeDegrees = 0.0f;

    UPROPERTY(VisibleAnywhere)
    int32 MinGroundMacroVariation = 255;

    UPROPERTY(VisibleAnywhere)
    int32 MaxGroundMacroVariation = 0;

    UPROPERTY(VisibleAnywhere)
    float MaxShorelineErosionOffsetCm = 0.0f;

    UPROPERTY(VisibleAnywhere)
    int32 ShoreRockInstanceCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 ShoreReedClusterCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 ShoreReedStemInstanceCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 ShoreDeadfallInstanceCount = 0;

    UPROPERTY(VisibleAnywhere)
    int32 ShoreAccentVariantCount = 0;

    UPROPERTY(VisibleAnywhere)
    float MinShorelineAccentRouteClearanceCm = 0.0f;

    UPROPERTY(VisibleAnywhere)
    int32 CourseHoleCount = 0;

    bool bReady = false;
};
