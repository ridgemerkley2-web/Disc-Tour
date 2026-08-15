#pragma once

#include "CoreMinimal.h"

struct FDiscGolfHoleBlockoutDefinition;

struct FDiscGolfCourseVisualQualityTier
{
    FName TierId = NAME_None;
    float FoliageDensityScale = 1.0f;
    float GrassDensityScale = 1.0f;
    float FoliageCullDistanceScale = 1.0f;
    bool bAffectsCollision = false;
    FName CollisionProfileId = NAME_None;
};

struct FDiscGolfHoleVisualPlan
{
    int32 HoleNumber = 1;
    FName TerrainStyleId = NAME_None;
    FName ForestReferenceId = NAME_None;
    int32 FoliageSeed = 1;
    float CanopyDensityScale = 1.0f;
    float ForestDepthCm = 5500.0f;
    float CorridorBufferCm = 400.0f;
    float TeeClearingRadiusCm = 1200.0f;
    float GreenClearingRadiusCm = 1800.0f;
};

/** Presentation-only asset and scalability contract. Competitive collision remains in hole definitions. */
struct FDiscGolfCoursePresentationDefinition
{
    int32 SchemaVersion = 1;
    FName CourseId = NAME_None;
    FName LayoutId = NAME_None;
    FName BiomeId = NAME_None;
    FName CollisionProfileId = NAME_None;
    bool bCollisionInvariantAcrossQuality = true;
    bool bAssetsReady = false;
    FString TerrainMaterialPath;
    FString FoliageSetPath;
    FString WaterMaterialPath;
    TArray<FDiscGolfCourseVisualQualityTier> QualityTiers;
    TArray<FDiscGolfHoleVisualPlan> Holes;
};

namespace DiscGolfCoursePresentation
{
    DISCGOLFTOUR_API FDiscGolfCoursePresentationDefinition PineRidgeFallback();
    DISCGOLFTOUR_API bool ParseJson(
        const FString& Json,
        FDiscGolfCoursePresentationDefinition& OutDefinition,
        FString& OutError);
    DISCGOLFTOUR_API bool Validate(
        const FDiscGolfCoursePresentationDefinition& Definition,
        FString& OutError);
    DISCGOLFTOUR_API bool LoadPineRidge(
        FDiscGolfCoursePresentationDefinition& OutDefinition,
        FString& OutSource,
        FString& OutError);
    DISCGOLFTOUR_API bool CompetitiveCollisionSignatureForQuality(
        const TArray<FDiscGolfHoleBlockoutDefinition>& HoleDefinitions,
        const FDiscGolfCoursePresentationDefinition& Presentation,
        FName QualityTierId,
        uint32& OutSignature,
        FString& OutError);
}
