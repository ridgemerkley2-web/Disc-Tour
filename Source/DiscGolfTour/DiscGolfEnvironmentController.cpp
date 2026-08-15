#include "DiscGolfEnvironmentController.h"

#include "DiscGolfEnvironmentDataAssets.h"
#include "DiscGolfEnvironmentZoneActor.h"
#include "WindDirector.h"
#include "Components/BoxComponent.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "PCGComponent.h"
#include "PCGGraph.h"

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

    PCGComponent = CreateDefaultSubobject<UPCGComponent>(TEXT("ForestPCG"));
    PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
    PCGComponent->bGenerateOnDropWhenTriggerOnDemand = false;
    PCGComponent->bIsComponentPartitioned = true;

    Tags.AddUnique(TEXT("Environment.Controller"));
}

void ADiscGolfEnvironmentController::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    GenerationBounds->SetBoxExtent(CourseExtentCm.GetAbs());
    ApplyPreset();
}

void ADiscGolfEnvironmentController::BeginPlay()
{
    Super::BeginPlay();
    ApplyPreset();
    if (bSynchronizeDiscFlightWind && GetWorld())
    {
        for (TActorIterator<AWindDirector> It(GetWorld()); It; ++It)
        {
            SynchronizeWindDirector(*It);
            break;
        }
    }
}

void ADiscGolfEnvironmentController::ApplyPreset()
{
    UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    if (!Preset || !PCGComponent) return;
    PCGComponent->Seed = Preset->Clearance.RandomSeed;
    if (UPCGGraphInterface* Graph = Preset->ForestGraph.LoadSynchronous())
    {
        PCGComponent->SetGraph(Graph);
    }
}

void ADiscGolfEnvironmentController::GenerateForest()
{
    ApplyPreset();
    if (PCGComponent && PCGComponent->GetGraph())
    {
        PCGComponent->GenerateLocal(EPCGComponentGenerationTrigger::GenerateOnDemand, true,
            PCGHiGenGrid::UninitializedGridSize());
    }
}

void ADiscGolfEnvironmentController::CleanupForest()
{
    if (PCGComponent) PCGComponent->CleanupLocal(true);
}

void ADiscGolfEnvironmentController::SynchronizeWindDirector(AWindDirector* WindDirector) const
{
    const UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    if (!Preset || !WindDirector) return;
    const FVector Direction = Preset->Wind.Direction.GetSafeNormal(
        UE_SMALL_NUMBER, FVector::ForwardVector);
    WindDirector->BaseWindMps = Direction * Preset->Wind.SpeedMps;
    WindDirector->GustAmplitudeMps = Preset->Wind.GustStrengthMps;
    WindDirector->GustFrequencyHz = Preset->Wind.GustFrequencyHz;
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
    const UDiscGolfForestPreset* Preset = ForestPreset.LoadSynchronous();
    const UDiscGolfEnvironmentAssetSet* Assets = Preset
        ? Preset->AssetSet.LoadSynchronous() : nullptr;
    return Preset && Assets && Assets->GetPopulatedSlotCount() > 0
        && !Preset->ForestGraph.IsNull();
}
