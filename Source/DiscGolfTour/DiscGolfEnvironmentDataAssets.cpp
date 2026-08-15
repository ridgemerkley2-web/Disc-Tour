#include "DiscGolfEnvironmentDataAssets.h"

namespace
{
    FDiscGolfEnvironmentAssetSlot MakeSlot(
        EDiscGolfEnvironmentAssetCategory Category,
        EDiscGolfEnvironmentCollisionMode Collision,
        float SpacingCm,
        float CullStartCm,
        float CullEndCm)
    {
        FDiscGolfEnvironmentAssetSlot Slot;
        Slot.Category = Category;
        Slot.CollisionMode = Collision;
        Slot.MinimumSpacingCm = SpacingCm;
        Slot.CullStartCm = CullStartCm;
        Slot.CullEndCm = CullEndCm;
        Slot.ShadowCullDistanceCm = FMath::Min(CullStartCm, 18000.0f);
        return Slot;
    }

    FDiscGolfEnvironmentZoneRules MakeZone(
        EDiscGolfEnvironmentZoneType Type,
        float Trees, float Saplings, float Shrubs, float Ferns, float Grass, float Debris)
    {
        FDiscGolfEnvironmentZoneRules Rules;
        Rules.ZoneType = Type;
        Rules.TreeDensity = Trees;
        Rules.SaplingDensity = Saplings;
        Rules.ShrubDensity = Shrubs;
        Rules.FernDensity = Ferns;
        Rules.GrassDensity = Grass;
        Rules.DebrisDensity = Debris;
        return Rules;
    }
}

UDiscGolfEnvironmentAssetSet::UDiscGolfEnvironmentAssetSet()
{
    Slots = {
        MakeSlot(EDiscGolfEnvironmentAssetCategory::TreeConiferLarge, EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking, 760.0f, 28000.0f, 50000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::TreeConiferMedium, EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking, 620.0f, 25000.0f, 45000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking, 440.0f, 19000.0f, 34000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::TreeDeciduousLarge, EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking, 800.0f, 28000.0f, 50000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium, EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking, 600.0f, 23000.0f, 42000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::Sapling, EDiscGolfEnvironmentCollisionMode::ShrubOverlap, 260.0f, 13000.0f, 25000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::Shrub, EDiscGolfEnvironmentCollisionMode::ShrubOverlap, 180.0f, 10000.0f, 22000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::Fern, EDiscGolfEnvironmentCollisionMode::None, 80.0f, 7000.0f, 16000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::Grass, EDiscGolfEnvironmentCollisionMode::None, 35.0f, 4500.0f, 12500.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::GroundCover, EDiscGolfEnvironmentCollisionMode::None, 45.0f, 5000.0f, 14000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::Log, EDiscGolfEnvironmentCollisionMode::SolidBlocking, 500.0f, 18000.0f, 32000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::Stump, EDiscGolfEnvironmentCollisionMode::SolidBlocking, 350.0f, 15000.0f, 28000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::RockSmall, EDiscGolfEnvironmentCollisionMode::SolidBlocking, 220.0f, 13000.0f, 26000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::RockLarge, EDiscGolfEnvironmentCollisionMode::SolidBlocking, 600.0f, 23000.0f, 42000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::ForestDebris, EDiscGolfEnvironmentCollisionMode::None, 90.0f, 7000.0f, 16000.0f),
        MakeSlot(EDiscGolfEnvironmentAssetCategory::LeafLitter, EDiscGolfEnvironmentCollisionMode::None, 45.0f, 5000.0f, 14000.0f)
    };
}

bool UDiscGolfEnvironmentAssetSet::HasPopulatedSlot(EDiscGolfEnvironmentAssetCategory Category) const
{
    const FDiscGolfEnvironmentAssetSlot* Slot = Slots.FindByPredicate(
        [Category](const FDiscGolfEnvironmentAssetSlot& Candidate)
        {
            return Candidate.Category == Category;
        });
    return Slot && Slot->Variants.ContainsByPredicate(
        [](const FDiscGolfEnvironmentMeshVariant& Variant)
        {
            return !Variant.VisualMesh.IsNull();
        });
}

int32 UDiscGolfEnvironmentAssetSet::GetPopulatedSlotCount() const
{
    int32 Count = 0;
    for (const FDiscGolfEnvironmentAssetSlot& Slot : Slots)
    {
        if (HasPopulatedSlot(Slot.Category)) ++Count;
    }
    return Count;
}

FPrimaryAssetId UDiscGolfEnvironmentAssetSet::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("EnvironmentAssetSet"), AssetSetId);
}

UDiscGolfForestPreset::UDiscGolfForestPreset()
{
    ZoneRules = {
        MakeZone(EDiscGolfEnvironmentZoneType::Tee,       0.0f, 0.0f, 0.0f, 0.0f, 0.25f, 0.0f),
        MakeZone(EDiscGolfEnvironmentZoneType::Fairway,   0.08f, 0.04f, 0.06f, 0.08f, 0.72f, 0.03f),
        MakeZone(EDiscGolfEnvironmentZoneType::SemiRough, 0.42f, 0.55f, 0.62f, 0.75f, 0.90f, 0.35f),
        MakeZone(EDiscGolfEnvironmentZoneType::DeepRough, 1.00f, 1.15f, 1.20f, 1.25f, 1.05f, 1.10f),
        MakeZone(EDiscGolfEnvironmentZoneType::Green,     0.0f, 0.02f, 0.03f, 0.04f, 0.48f, 0.0f),
        MakeZone(EDiscGolfEnvironmentZoneType::OBNatural, 1.20f, 1.25f, 1.35f, 1.30f, 1.10f, 1.30f)
    };

    SpeciesMix = {
        { EDiscGolfEnvironmentAssetCategory::TreeConiferLarge, 1.00f },
        { EDiscGolfEnvironmentAssetCategory::TreeConiferMedium, 0.90f },
        { EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 0.72f },
        { EDiscGolfEnvironmentAssetCategory::TreeDeciduousLarge, 0.32f },
        { EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium, 0.48f },
        { EDiscGolfEnvironmentAssetCategory::Sapling, 0.80f }
    };

    LightCanopy.VelocityMultiplier = 0.88f;
    LightCanopy.SpinMultiplier = 0.96f;
    DenseCanopy.VelocityMultiplier = 0.66f;
    DenseCanopy.SpinMultiplier = 0.82f;
    Shrub.VelocityMultiplier = 0.74f;
    Shrub.SpinMultiplier = 0.88f;

    Performance.DensityScale = 0.58f;
    Performance.CullDistanceScale = 0.72f;
    Performance.ShadowDistanceScale = 0.62f;
    Performance.bAllowNaniteForSuitableSolids = true;
    Performance.bEnableCanopyInteractionVolumes = true;
    Performance.bEnableGrassShadows = false;

    High.DensityScale = 1.0f;
    High.CullDistanceScale = 1.0f;
    High.ShadowDistanceScale = 1.0f;
    High.bAllowNaniteForSuitableSolids = true;
    High.bEnableCanopyInteractionVolumes = true;
    High.bEnableGrassShadows = false;

    Cinematic.DensityScale = 1.30f;
    Cinematic.CullDistanceScale = 1.35f;
    Cinematic.ShadowDistanceScale = 1.45f;
    Cinematic.bAllowNaniteForSuitableSolids = true;
    Cinematic.bEnableCanopyInteractionVolumes = true;
    Cinematic.bEnableGrassShadows = true;
}

FDiscGolfEnvironmentQualitySettings UDiscGolfForestPreset::ResolveQuality(
    EDiscGolfEnvironmentQuality Quality) const
{
    switch (Quality)
    {
        case EDiscGolfEnvironmentQuality::Performance: return Performance;
        case EDiscGolfEnvironmentQuality::Cinematic: return Cinematic;
        case EDiscGolfEnvironmentQuality::High:
        default: return High;
    }
}

FDiscGolfEnvironmentZoneRules UDiscGolfForestPreset::ResolveZoneRules(
    EDiscGolfEnvironmentZoneType ZoneType) const
{
    if (const FDiscGolfEnvironmentZoneRules* Found = ZoneRules.FindByPredicate(
        [ZoneType](const FDiscGolfEnvironmentZoneRules& Rules)
        {
            return Rules.ZoneType == ZoneType;
        }))
    {
        return *Found;
    }
    FDiscGolfEnvironmentZoneRules Fallback;
    Fallback.ZoneType = ZoneType;
    return Fallback;
}

float UDiscGolfForestPreset::ResolveSpeciesWeight(
    EDiscGolfEnvironmentAssetCategory Category) const
{
    if (const FDiscGolfEnvironmentSpeciesWeight* Found = SpeciesMix.FindByPredicate(
        [Category](const FDiscGolfEnvironmentSpeciesWeight& Entry)
        {
            return Entry.Category == Category;
        }))
    {
        return FMath::Max(0.0f, Found->Weight);
    }
    return 1.0f;
}

FPrimaryAssetId UDiscGolfForestPreset::GetPrimaryAssetId() const
{
    return FPrimaryAssetId(TEXT("ForestPreset"), PresetId);
}
