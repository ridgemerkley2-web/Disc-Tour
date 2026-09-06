#include "DiscGolfCoursePresentationDefinition.h"

#include "DiscGolfCourseDefinition.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr const TCHAR* PresentationSchema = TEXT("disc_golf_course_presentation");

    bool IsGameAssetPath(const FString& Path)
    {
        return Path.StartsWith(TEXT("/Game/")) && !Path.Contains(TEXT(" "));
    }

    void AddQualityTier(FDiscGolfCoursePresentationDefinition& Definition, const TCHAR* Id,
        float FoliageDensity, float GrassDensity, float CullDistance)
    {
        FDiscGolfCourseVisualQualityTier Tier;
        Tier.TierId = Id;
        Tier.FoliageDensityScale = FoliageDensity;
        Tier.GrassDensityScale = GrassDensity;
        Tier.FoliageCullDistanceScale = CullDistance;
        Tier.CollisionProfileId = Definition.CollisionProfileId;
        Definition.QualityTiers.Add(Tier);
    }

    void AddHolePlan(FDiscGolfCoursePresentationDefinition& Definition, int32 HoleNumber,
        const TCHAR* StyleId, const TCHAR* ForestReferenceId, int32 Seed, float CanopyDensity,
        float ForestDepthCm, float CorridorBufferCm, float TeeClearingRadiusCm,
        float GreenClearingRadiusCm)
    {
        FDiscGolfHoleVisualPlan Plan;
        Plan.HoleNumber = HoleNumber;
        Plan.TerrainStyleId = StyleId;
        Plan.ForestReferenceId = ForestReferenceId;
        Plan.FoliageSeed = Seed;
        Plan.CanopyDensityScale = CanopyDensity;
        Plan.ForestDepthCm = ForestDepthCm;
        Plan.CorridorBufferCm = CorridorBufferCm;
        Plan.TeeClearingRadiusCm = TeeClearingRadiusCm;
        Plan.GreenClearingRadiusCm = GreenClearingRadiusCm;
        Definition.Holes.Add(Plan);
    }

    uint32 CollisionSignature(const TArray<FDiscGolfHoleBlockoutDefinition>& HoleDefinitions)
    {
        uint32 Signature = GetTypeHash(HoleDefinitions.Num());
        for (const FDiscGolfHoleBlockoutDefinition& Hole : HoleDefinitions)
        {
            Signature = HashCombineFast(Signature, GetTypeHash(Hole.HoleNumber));
            Signature = HashCombineFast(Signature, GetTypeHash(Hole.TeeLocationCm));
            Signature = HashCombineFast(Signature, GetTypeHash(Hole.BasketLocationCm));
            for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Hole.Surfaces)
            {
                Signature = HashCombineFast(Signature, GetTypeHash(Surface.SurfaceId));
                Signature = HashCombineFast(Signature, GetTypeHash(static_cast<uint8>(Surface.SurfaceType)));
                Signature = HashCombineFast(Signature, GetTypeHash(static_cast<uint8>(Surface.Shape)));
                Signature = HashCombineFast(Signature, GetTypeHash(Surface.LocationCm));
                Signature = HashCombineFast(Signature, GetTypeHash(Surface.Scale));
                Signature = HashCombineFast(Signature, GetTypeHash(Surface.Rotation.Pitch));
                Signature = HashCombineFast(Signature, GetTypeHash(Surface.Rotation.Yaw));
                Signature = HashCombineFast(Signature, GetTypeHash(Surface.Rotation.Roll));
            }
            // Current authored trees own deterministic trunk collision; visual foliage may only dress around them.
            for (const FDiscGolfTreeDefinition& Tree : Hole.Trees)
            {
                Signature = HashCombineFast(Signature, GetTypeHash(Tree.LocationCm));
                Signature = HashCombineFast(Signature, GetTypeHash(Tree.HeightScale));
            }
            for (const FDiscGolfCollisionFixtureDefinition& Fixture : Hole.CollisionFixtures)
            {
                Signature = HashCombineFast(Signature, GetTypeHash(Fixture.FixtureId));
                Signature = HashCombineFast(Signature, GetTypeHash(static_cast<uint8>(Fixture.FixtureType)));
                Signature = HashCombineFast(Signature, GetTypeHash(static_cast<uint8>(Fixture.Shape)));
                Signature = HashCombineFast(Signature, GetTypeHash(Fixture.LocationCm));
                Signature = HashCombineFast(Signature, GetTypeHash(Fixture.Scale));
                Signature = HashCombineFast(Signature, GetTypeHash(Fixture.Rotation.Pitch));
                Signature = HashCombineFast(Signature, GetTypeHash(Fixture.Rotation.Yaw));
                Signature = HashCombineFast(Signature, GetTypeHash(Fixture.Rotation.Roll));
            }
        }
        return Signature;
    }
}

FDiscGolfCoursePresentationDefinition DiscGolfCoursePresentation::PineRidgeFallback()
{
    FDiscGolfCoursePresentationDefinition Definition;
    Definition.SchemaVersion = 1;
    Definition.CourseId = TEXT("PineRidgeChampionship");
    Definition.LayoutId = TEXT("Championship");
    Definition.BiomeId = TEXT("PacificNorthwestPine");
    Definition.CollisionProfileId = TEXT("PineRidgeCompetitiveV2_Fixtures");
    Definition.bCollisionInvariantAcrossQuality = true;
    Definition.bAssetsReady = false;
    Definition.TerrainMaterialPath = TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeTerrain");
    Definition.FoliageSetPath = TEXT("/Game/Presentation/Course/PineRidge/Foliage/DA_PineRidgeFoliage");
    Definition.WaterMaterialPath = TEXT("/Game/Presentation/Course/PineRidge/Materials/M_GalleryLakeWater");
    AddQualityTier(Definition, TEXT("Low"), 0.50f, 0.35f, 0.75f);
    AddQualityTier(Definition, TEXT("Medium"), 0.85f, 0.75f, 1.00f);
    AddQualityTier(Definition, TEXT("High"), 1.15f, 1.10f, 1.25f);
    AddHolePlan(Definition, 1, TEXT("OpeningPines"), TEXT("OpeningBroadTreeLine"),
        1101, 1.15f, 4200.0f, 610.0f, 1067.0f, 1067.0f);
    AddHolePlan(Definition, 2, TEXT("NeedleCorridor"), TEXT("NeedleCanopyCompression"),
        2202, 1.40f, 4800.0f, 300.0f, 1100.0f, 1500.0f);
    AddHolePlan(Definition, 3, TEXT("GalleryLake"), TEXT("GalleryLakeFrame"),
        3303, 1.25f, 4500.0f, 650.0f, 1500.0f, 2200.0f);
    return Definition;
}

bool DiscGolfCoursePresentation::ParseJson(
    const FString& Json,
    FDiscGolfCoursePresentationDefinition& OutDefinition,
    FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("presentation JSON could not be parsed");
        return false;
    }
    if (Root->GetStringField(TEXT("schema")) != PresentationSchema)
    {
        OutError = TEXT("presentation schema is unsupported");
        return false;
    }

    FDiscGolfCoursePresentationDefinition Definition;
    Definition.SchemaVersion = Root->GetIntegerField(TEXT("schemaVersion"));
    Definition.CourseId = FName(*Root->GetStringField(TEXT("courseId")));
    Definition.LayoutId = FName(*Root->GetStringField(TEXT("layoutId")));
    Definition.BiomeId = FName(*Root->GetStringField(TEXT("biomeId")));
    Definition.CollisionProfileId = FName(*Root->GetStringField(TEXT("collisionProfileId")));
    Definition.bCollisionInvariantAcrossQuality = Root->GetBoolField(TEXT("collisionInvariantAcrossQuality"));
    Definition.bAssetsReady = Root->GetBoolField(TEXT("assetsReady"));

    const TSharedPtr<FJsonObject>* Assets = nullptr;
    if (Root->TryGetObjectField(TEXT("assets"), Assets) && Assets && Assets->IsValid())
    {
        Definition.TerrainMaterialPath = (*Assets)->GetStringField(TEXT("terrainMaterial"));
        Definition.FoliageSetPath = (*Assets)->GetStringField(TEXT("foliageSet"));
        Definition.WaterMaterialPath = (*Assets)->GetStringField(TEXT("waterMaterial"));
    }

    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Root->TryGetArrayField(TEXT("qualityTiers"), Values))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (!Object.IsValid()) continue;
            FDiscGolfCourseVisualQualityTier Tier;
            Tier.TierId = FName(*Object->GetStringField(TEXT("id")));
            Tier.FoliageDensityScale = Object->GetNumberField(TEXT("foliageDensityScale"));
            Tier.GrassDensityScale = Object->GetNumberField(TEXT("grassDensityScale"));
            Tier.FoliageCullDistanceScale = Object->GetNumberField(TEXT("foliageCullDistanceScale"));
            Tier.bAffectsCollision = Object->GetBoolField(TEXT("affectsCollision"));
            Tier.CollisionProfileId = FName(*Object->GetStringField(TEXT("collisionProfileId")));
            Definition.QualityTiers.Add(Tier);
        }
    }
    if (Root->TryGetArrayField(TEXT("holes"), Values))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            const TSharedPtr<FJsonObject> Object = Value->AsObject();
            if (!Object.IsValid()) continue;
            FDiscGolfHoleVisualPlan Plan;
            Plan.HoleNumber = Object->GetIntegerField(TEXT("holeNumber"));
            Plan.TerrainStyleId = FName(*Object->GetStringField(TEXT("terrainStyleId")));
            Plan.ForestReferenceId = FName(*Object->GetStringField(TEXT("forestReferenceId")));
            Plan.FoliageSeed = Object->GetIntegerField(TEXT("foliageSeed"));
            Plan.CanopyDensityScale = Object->GetNumberField(TEXT("canopyDensityScale"));
            Plan.ForestDepthCm = Object->GetNumberField(TEXT("forestDepthCm"));
            Plan.CorridorBufferCm = Object->GetNumberField(TEXT("corridorBufferCm"));
            Plan.TeeClearingRadiusCm = Object->GetNumberField(TEXT("teeClearingRadiusCm"));
            Plan.GreenClearingRadiusCm = Object->GetNumberField(TEXT("greenClearingRadiusCm"));
            Definition.Holes.Add(Plan);
        }
    }

    if (!Validate(Definition, OutError)) return false;
    OutDefinition = MoveTemp(Definition);
    OutError.Reset();
    return true;
}

bool DiscGolfCoursePresentation::Validate(
    const FDiscGolfCoursePresentationDefinition& Definition,
    FString& OutError)
{
    if (Definition.SchemaVersion != 1 || Definition.CourseId.IsNone()
        || Definition.LayoutId.IsNone() || Definition.BiomeId.IsNone()
        || Definition.CollisionProfileId.IsNone())
    {
        OutError = TEXT("presentation identity or schema is invalid");
        return false;
    }
    if (!Definition.bCollisionInvariantAcrossQuality)
    {
        OutError = TEXT("presentation quality must preserve competitive collision");
        return false;
    }
    if (!IsGameAssetPath(Definition.TerrainMaterialPath)
        || !IsGameAssetPath(Definition.FoliageSetPath)
        || !IsGameAssetPath(Definition.WaterMaterialPath))
    {
        OutError = TEXT("presentation asset paths must be stable /Game paths");
        return false;
    }
    if (Definition.QualityTiers.Num() != 3)
    {
        OutError = TEXT("presentation requires Low, Medium, and High quality tiers");
        return false;
    }
    TSet<FName> TierIds;
    for (const FDiscGolfCourseVisualQualityTier& Tier : Definition.QualityTiers)
    {
        TierIds.Add(Tier.TierId);
        if (Tier.TierId.IsNone() || Tier.bAffectsCollision
            || Tier.CollisionProfileId != Definition.CollisionProfileId
            || !FMath::IsFinite(Tier.FoliageDensityScale)
            || !FMath::IsFinite(Tier.GrassDensityScale)
            || !FMath::IsFinite(Tier.FoliageCullDistanceScale)
            || Tier.FoliageDensityScale < 0.0f || Tier.FoliageDensityScale > 1.5f
            || Tier.GrassDensityScale < 0.0f || Tier.GrassDensityScale > 1.5f
            || Tier.FoliageCullDistanceScale < 0.25f || Tier.FoliageCullDistanceScale > 2.0f)
        {
            OutError = TEXT("presentation quality tier is unsafe or changes collision");
            return false;
        }
    }
    if (!TierIds.Contains(TEXT("Low")) || !TierIds.Contains(TEXT("Medium"))
        || !TierIds.Contains(TEXT("High")) || TierIds.Num() != 3)
    {
        OutError = TEXT("presentation quality tier IDs are incomplete or duplicated");
        return false;
    }
    if (Definition.Holes.Num() != 3)
    {
        OutError = TEXT("presentation contract must cover all three holes");
        return false;
    }
    for (int32 Index = 0; Index < Definition.Holes.Num(); ++Index)
    {
        const FDiscGolfHoleVisualPlan& Hole = Definition.Holes[Index];
        if (Hole.HoleNumber != Index + 1 || Hole.TerrainStyleId.IsNone()
            || Hole.ForestReferenceId.IsNone() || Hole.FoliageSeed <= 0
            || !FMath::IsFinite(Hole.CanopyDensityScale)
            || !FMath::IsFinite(Hole.ForestDepthCm)
            || !FMath::IsFinite(Hole.CorridorBufferCm)
            || !FMath::IsFinite(Hole.TeeClearingRadiusCm)
            || !FMath::IsFinite(Hole.GreenClearingRadiusCm)
            || Hole.CanopyDensityScale < 0.0f || Hole.CanopyDensityScale > 1.5f
            || Hole.ForestDepthCm < 2500.0f || Hole.ForestDepthCm > 10000.0f
            || Hole.CorridorBufferCm < 0.0f || Hole.CorridorBufferCm > 2000.0f
            || Hole.TeeClearingRadiusCm < 500.0f || Hole.TeeClearingRadiusCm > 4000.0f
            || Hole.GreenClearingRadiusCm < 500.0f || Hole.GreenClearingRadiusCm > 5000.0f)
        {
            OutError = TEXT("presentation hole coverage is invalid or non-contiguous");
            return false;
        }
    }
    OutError.Reset();
    return true;
}

bool DiscGolfCoursePresentation::LoadPineRidge(
    FDiscGolfCoursePresentationDefinition& OutDefinition,
    FString& OutSource,
    FString& OutError)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/PineRidgePresentation.json"));
    FString Json;
    FString ParseError;
    if (FFileHelper::LoadFileToString(Json, *Path) && ParseJson(Json, OutDefinition, ParseError)
        && OutDefinition.CourseId == TEXT("PineRidgeChampionship")
        && OutDefinition.LayoutId == TEXT("Championship"))
    {
        OutSource = TEXT("AUTHORED JSON");
        OutError.Reset();
        return true;
    }
    if (ParseError.IsEmpty() && IFileManager::Get().FileExists(*Path))
    {
        ParseError = TEXT("authored presentation identity does not match Pine Ridge");
    }
    OutDefinition = PineRidgeFallback();
    if (!Validate(OutDefinition, OutError)) return false;
    OutSource = TEXT("SOURCE FALLBACK");
    OutError = ParseError.IsEmpty() ? TEXT("authored presentation JSON missing; using source fallback") : ParseError;
    return true;
}

bool DiscGolfCoursePresentation::CompetitiveCollisionSignatureForQuality(
    const TArray<FDiscGolfHoleBlockoutDefinition>& HoleDefinitions,
    const FDiscGolfCoursePresentationDefinition& Presentation,
    FName QualityTierId,
    uint32& OutSignature,
    FString& OutError)
{
    if (HoleDefinitions.IsEmpty() || !Validate(Presentation, OutError)) return false;
    const FDiscGolfCourseVisualQualityTier* Tier = Presentation.QualityTiers.FindByPredicate(
        [QualityTierId](const FDiscGolfCourseVisualQualityTier& Candidate)
        {
            return Candidate.TierId == QualityTierId;
        });
    if (!Tier || Tier->bAffectsCollision || Tier->CollisionProfileId != Presentation.CollisionProfileId)
    {
        OutError = TEXT("quality tier cannot resolve the invariant collision profile");
        return false;
    }
    OutSignature = CollisionSignature(HoleDefinitions);
    OutError.Reset();
    return true;
}
