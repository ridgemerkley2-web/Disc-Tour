#include "DiscGolfEnvironmentController.h"

#include "DiscGolfTour.h"
#include "DiscGolfEnvironmentDataAssets.h"
#include "DiscGolfEnvironmentZoneActor.h"
#include "WindDirector.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

namespace
{
    float CategoryDensity(
        const FDiscGolfEnvironmentZoneRules& Rules,
        EDiscGolfEnvironmentAssetCategory Category)
    {
        switch (Category)
        {
            case EDiscGolfEnvironmentAssetCategory::TreeConiferLarge:
            case EDiscGolfEnvironmentAssetCategory::TreeConiferMedium:
            case EDiscGolfEnvironmentAssetCategory::TreeConiferYoung:
            case EDiscGolfEnvironmentAssetCategory::TreeDeciduousLarge:
            case EDiscGolfEnvironmentAssetCategory::TreeDeciduousMedium:
                return Rules.TreeDensity;
            case EDiscGolfEnvironmentAssetCategory::Sapling:
                return Rules.SaplingDensity;
            case EDiscGolfEnvironmentAssetCategory::Shrub:
                return Rules.ShrubDensity;
            case EDiscGolfEnvironmentAssetCategory::Fern:
                return Rules.FernDensity;
            case EDiscGolfEnvironmentAssetCategory::Grass:
            case EDiscGolfEnvironmentAssetCategory::GroundCover:
                return Rules.GrassDensity;
            case EDiscGolfEnvironmentAssetCategory::Log:
            case EDiscGolfEnvironmentAssetCategory::Stump:
            case EDiscGolfEnvironmentAssetCategory::RockSmall:
            case EDiscGolfEnvironmentAssetCategory::RockLarge:
            case EDiscGolfEnvironmentAssetCategory::ForestDebris:
            case EDiscGolfEnvironmentAssetCategory::LeafLitter:
            default:
                return Rules.DebrisDensity;
        }
    }
}

ADiscGolfEnvironmentController::ADiscGolfEnvironmentController()
{
    PrimaryActorTick.bCanEverTick = false;
    GenerationBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("GenerationBounds"));
    SetRootComponent(GenerationBounds);
    GenerationBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GenerationBounds->SetGenerateOverlapEvents(false);
    GenerationBounds->SetCanEverAffectNavigation(false);
    GenerationBounds->SetHiddenInGame(true);

    Tags.AddUnique(TEXT("Environment.Controller"));
}

void ADiscGolfEnvironmentController::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerationBounds->SetBoxExtent(CourseExtentCm.GetAbs());
}

void ADiscGolfEnvironmentController::BeginPlay()
{
    Super::BeginPlay();
    if (bSynchronizeDiscFlightWind && GetWorld())
    {
        for (TActorIterator<AWindDirector> It(GetWorld()); It; ++It)
        {
            SynchronizeWindDirector(*It);
            break;
        }
    }
}

bool ADiscGolfEnvironmentController::SynchronizeWindDirector(AWindDirector* WindDirector) const
{
    const UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    if (!Preset || !WindDirector)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Environment wind synchronization rejected a missing preset or wind director."));
        return false;
    }

    const FDiscGolfEnvironmentWindSettings& Settings = Preset->Wind;
    const bool bDirectionFinite = FMath::IsFinite(Settings.Direction.X)
        && FMath::IsFinite(Settings.Direction.Y)
        && FMath::IsFinite(Settings.Direction.Z)
        && FMath::IsFinite(Settings.Direction.SizeSquared());
    if (!bDirectionFinite
        || !FMath::IsFinite(Settings.SpeedMps)
        || Settings.SpeedMps < 0.0f
        || (Settings.SpeedMps > 0.0f && Settings.Direction.IsNearlyZero()))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Environment wind synchronization rejected invalid direction/speed data."));
        return false;
    }

    const FVector Direction = Preset->Wind.Direction.GetSafeNormal(
        UE_SMALL_NUMBER, FVector::ForwardVector);
    FString WindError;
    if (!WindDirector->TryConfigurePhysicsWind(
        Direction * Settings.SpeedMps,
        Settings.GustStrengthMps,
        Settings.GustFrequencyHz,
        WindError))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Environment wind synchronization rejected invalid preset data: %s"),
            *WindError);
        return false;
    }

    if (UMaterialParameterCollection* WindCollection =
        Preset->Wind.FoliageWindCollection.LoadSynchronous())
    {
        UKismetMaterialLibrary::SetVectorParameterValue(
            const_cast<ADiscGolfEnvironmentController*>(this), WindCollection,
            TEXT("WindDirection"), FLinearColor(Direction.X, Direction.Y, Direction.Z, 0.0f));
        UKismetMaterialLibrary::SetScalarParameterValue(
            const_cast<ADiscGolfEnvironmentController*>(this), WindCollection,
            TEXT("WindSpeedMps"), Preset->Wind.SpeedMps);
        UKismetMaterialLibrary::SetScalarParameterValue(
            const_cast<ADiscGolfEnvironmentController*>(this), WindCollection,
            TEXT("GustStrengthMps"), Preset->Wind.GustStrengthMps);
    }
    return true;
}

float ADiscGolfEnvironmentController::EvaluateDensity(
    const FVector& WorldLocation,
    EDiscGolfEnvironmentAssetCategory Category) const
{
    const UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    if (!Preset || !GetWorld()) return 0.0f;

    TArray<const ADiscGolfEnvironmentZoneActor*> Zones;
    for (TActorIterator<ADiscGolfEnvironmentZoneActor> It(GetWorld()); It; ++It)
    {
        Zones.Add(*It);
    }
    return EvaluateDensityFromZones(WorldLocation, Category, Preset, Zones);
}

float ADiscGolfEnvironmentController::EvaluateDensityFromZones(
    const FVector& WorldLocation,
    EDiscGolfEnvironmentAssetCategory Category,
    const UDiscGolfForestPreset* Preset,
    const TArray<const ADiscGolfEnvironmentZoneActor*>& Zones) const
{
    if (!Preset) return 0.0f;

    TArray<const ADiscGolfEnvironmentZoneActor*> OrderedZones;
    for (const ADiscGolfEnvironmentZoneActor* Zone : Zones)
    {
        if (IsValid(Zone)) OrderedZones.Add(Zone);
    }
    OrderedZones.Sort([](const ADiscGolfEnvironmentZoneActor& A,
        const ADiscGolfEnvironmentZoneActor& B)
    {
        return A.Priority < B.Priority;
    });

    float ZoneDensity = CategoryDensity(
        Preset->ResolveZoneRules(EDiscGolfEnvironmentZoneType::DeepRough), Category);
    for (const ADiscGolfEnvironmentZoneActor* Zone : OrderedZones)
    {
        const float Influence = Zone->GetInfluenceForCategory(WorldLocation, Category);
        if (Influence <= 0.0f) continue;
        const float TargetDensity = Zone->bHardExclusion ? 0.0f
            : CategoryDensity(Preset->ResolveZoneRules(Zone->ZoneType), Category);
        ZoneDensity = FMath::Lerp(ZoneDensity, TargetDensity, Influence);
    }
    const float QualityScale = Preset->ResolveQuality(Quality).DensityScale;
    return FMath::Clamp(ZoneDensity * Preset->Clearance.VegetationDensity
        * Preset->ResolveSpeciesWeight(Category) * QualityScale, 0.0f, 1.0f);
}

bool ADiscGolfEnvironmentController::HasProductionConfiguration() const
{
#if WITH_EDITORONLY_DATA
    const UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    const UDiscGolfEnvironmentAssetSet* Assets = Preset
        ? Preset->AssetSet.LoadSynchronous() : nullptr;
    return Preset && Assets && IsProductionConfigurationComplete(
        Assets->Slots.Num(),
        Assets->GetPopulatedSlotCount(),
        !Preset->ForestGraph.IsNull());
#else
    // Shipping never contains or evaluates the authoring graph property.
    return false;
#endif
}

bool ADiscGolfEnvironmentController::IsProductionConfigurationComplete(
    int32 BindingSlotCount,
    int32 PopulatedSlotCount,
    bool bHasAuthoringGraph)
{
    constexpr int32 RequiredBindingSlotCount = 16;
    return BindingSlotCount == RequiredBindingSlotCount
        && PopulatedSlotCount == RequiredBindingSlotCount
        && bHasAuthoringGraph;
}
