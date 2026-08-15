#include "DevCourseBootstrap.h"
#include "DiscGolfTour.h"
#include "BasketActor.h"
#include "DiscGolfHoleActor.h"
#include "DiscGolfCourseSurfaceActor.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfCourseFeatureActor.h"
#include "DiscGolfCoursePresentationDefinition.h"
#include "DiscGolfFoliagePresentationActor.h"
#include "DiscGolfFixturePresentationActor.h"
#include "DiscGolfTerrainPresentationActor.h"
#include "DiscGolfWaterPresentationActor.h"
#include "DiscGolfWorldFixtureActor.h"
#include "DiscGolfWindZoneActor.h"
#include "DiscGolfFlyoverRouteActor.h"
#include "DiscGolfLevelDesignReviewActor.h"
#include "DiscGolfEnvironmentController.h"
#include "DiscGolfEnvironmentDataAssets.h"
#include "DiscGolfEnvironmentZoneActor.h"
#include "PineRidgeHole1Environment.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/EngineTypes.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FLinearColor SurfaceColor(ECourseSurfaceType Surface)
    {
        switch (Surface)
        {
            case ECourseSurfaceType::TeePad: return FLinearColor(0.16f, 0.20f, 0.24f);
            case ECourseSurfaceType::LightRough: return FLinearColor(0.20f, 0.42f, 0.15f);
            case ECourseSurfaceType::DeepRough: return FLinearColor(0.08f, 0.22f, 0.08f);
            case ECourseSurfaceType::Dirt: return FLinearColor(0.35f, 0.20f, 0.08f);
            case ECourseSurfaceType::Rock: return FLinearColor(0.30f, 0.32f, 0.36f);
            case ECourseSurfaceType::OutOfBounds: return FLinearColor(0.38f, 0.04f, 0.04f);
            case ECourseSurfaceType::Hazard: return FLinearColor(0.42f, 0.20f, 0.02f);
            case ECourseSurfaceType::Fairway:
            default: return FLinearColor(0.12f, 0.34f, 0.12f);
        }
    }

    void TintMesh(UStaticMeshComponent* Mesh, const FLinearColor& Color)
    {
        if (!Mesh) return;
        if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
        {
            UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Mesh->GetOwner());
            if (Dynamic)
            {
                Dynamic->SetVectorParameterValue(TEXT("Color"), Color);
                Mesh->SetMaterial(0, Dynamic);
            }
        }
    }

    UMaterialInterface* PresentationMaterial(ECourseSurfaceType Surface)
    {
        const TCHAR* Path = TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeFairway.MI_PineRidgeFairway");
        switch (Surface)
        {
            case ECourseSurfaceType::DeepRough:
            case ECourseSurfaceType::Hazard:
                Path = TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeForestFloor.MI_PineRidgeForestFloor");
                break;
            case ECourseSurfaceType::TeePad:
            case ECourseSurfaceType::Dirt:
            case ECourseSurfaceType::Rock:
                Path = TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgePath.MI_PineRidgePath");
                break;
            default:
                break;
        }
        return LoadObject<UMaterialInterface>(nullptr, Path);
    }

    bool ApplyPresentationSurface(UStaticMeshComponent* Mesh, ECourseSurfaceType Surface, const FVector& Scale)
    {
        if (!Mesh) return false;
        UMaterialInterface* Material = PresentationMaterial(Surface);
        if (!Material) return false;

        UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, Mesh->GetOwner());
        if (!Dynamic) return false;
        // The source cube is 1 m wide. Scale UVs so every ground family keeps its authored 1-2 m texel scale.
        Dynamic->SetScalarParameterValue(TEXT("UTiling"), FMath::Max(1.0f, FMath::Abs(Scale.X) * 0.5f));
        Dynamic->SetScalarParameterValue(TEXT("VTiling"), FMath::Max(1.0f, FMath::Abs(Scale.Y) * 0.5f));
        Mesh->SetMaterial(0, Dynamic);
        return true;
    }
}

ADevCourseBootstrap::ADevCourseBootstrap()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADevCourseBootstrap::TrackActor(AActor* Actor)
{
    if (Actor) SpawnedCourseActors.Add(Actor);
}

AActor* ADevCourseBootstrap::SpawnPrimitive(UStaticMesh* Mesh, const FVector& Location, const FVector& Scale,
    const FRotator& Rotation)
{
    if (!Mesh || !GetWorld()) return nullptr;

    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, Rotation);
    if (!Actor) return nullptr;

    Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Actor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
    Actor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->SetActorScale3D(Scale);
    TrackActor(Actor);
    return Actor;
}

AActor* ADevCourseBootstrap::SpawnCourseSurface(
    UStaticMesh* Mesh,
    const FVector& Location,
    const FVector& Scale,
    ECourseSurfaceType SurfaceType,
    const FRotator& Rotation)
{
    if (!Mesh || !GetWorld()) return nullptr;

    ADiscGolfCourseSurfaceActor* Actor = GetWorld()->SpawnActor<ADiscGolfCourseSurfaceActor>(Location, Rotation);
    if (!Actor) return nullptr;
    Actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    Actor->GetStaticMeshComponent()->SetStaticMesh(Mesh);
    Actor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Actor->GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
    Actor->GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->SetActorScale3D(Scale);
    Actor->SetCourseSurfaceType(SurfaceType);
    if (!ApplyPresentationSurface(Actor->GetStaticMeshComponent(), SurfaceType, Scale))
    {
        TintMesh(Actor->GetStaticMeshComponent(), SurfaceColor(SurfaceType));
    }
    TrackActor(Actor);
    return Actor;
}

void ADevCourseBootstrap::SpawnTree(
    FName FixtureId,
    const FVector& Location,
    float HeightScale,
    bool bHideCollisionProxy)
{
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    ADiscGolfWorldFixtureActor* Trunk = GetWorld()
        ? GetWorld()->SpawnActor<ADiscGolfWorldFixtureActor>() : nullptr;
    if (Trunk)
    {
        Trunk->ConfigureTree(FixtureId, Location, HeightScale, Cylinder);
        TrackActor(Trunk);
        TintMesh(Trunk->GetStaticMeshComponent(), FLinearColor(0.22f, 0.10f, 0.035f));
        // The cylinder remains the exact authored collision proxy but should not poke through production foliage.
        Trunk->GetStaticMeshComponent()->SetHiddenInGame(bHideCollisionProxy);
#if WITH_EDITOR
        Trunk->SetActorLabel(FixtureId.ToString());
#endif
    }
    if (bHideCollisionProxy) return;
    AActor* Crown = SpawnPrimitive(Sphere, Location + FVector(0, 0, 650.0f * HeightScale), FVector(2.3f, 2.3f, 3.2f) * HeightScale);
    if (AStaticMeshActor* CrownMeshActor = Cast<AStaticMeshActor>(Crown))
    {
        CrownMeshActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        TintMesh(CrownMeshActor->GetStaticMeshComponent(), FLinearColor(0.035f, 0.16f, 0.045f));
    }
}

ADiscGolfWorldFixtureActor* ADevCourseBootstrap::SpawnCollisionFixture(
    const FDiscGolfCollisionFixtureDefinition& Definition,
    UStaticMesh* Mesh)
{
    if (!GetWorld() || !Mesh) return nullptr;
    ADiscGolfWorldFixtureActor* Actor = GetWorld()->SpawnActor<ADiscGolfWorldFixtureActor>();
    if (!Actor) return nullptr;
    Actor->Configure(Definition, Mesh);
    TrackActor(Actor);

    FLinearColor Color(0.28f, 0.30f, 0.33f);
    switch (Definition.FixtureType)
    {
        case EDiscGolfFixtureType::DenseGrass: Color = FLinearColor(0.06f, 0.28f, 0.06f); break;
        case EDiscGolfFixtureType::Sign: Color = FLinearColor(0.82f, 0.47f, 0.07f); break;
        case EDiscGolfFixtureType::Rock: Color = FLinearColor(0.28f, 0.30f, 0.33f); break;
        case EDiscGolfFixtureType::Tree: Color = FLinearColor(0.22f, 0.10f, 0.035f); break;
        case EDiscGolfFixtureType::Unknown:
        default: break;
    }
    TintMesh(Actor->GetStaticMeshComponent(), Color);
#if WITH_EDITOR
    Actor->SetActorLabel(Definition.FixtureId.ToString());
#endif
    return Actor;
}

ADiscGolfFixturePresentationActor* ADevCourseBootstrap::SpawnFixturePresentation(
    const FDiscGolfCollisionFixtureDefinition& Definition,
    float GrassDensityScale,
    float CullDistanceScale)
{
    if (!GetWorld()) return nullptr;
    ADiscGolfFixturePresentationActor* Actor =
        GetWorld()->SpawnActor<ADiscGolfFixturePresentationActor>();
    if (!Actor) return nullptr;
    if (!Actor->Configure(Definition, GrassDensityScale, CullDistanceScale))
    {
        Actor->Destroy();
        return nullptr;
    }
    TrackActor(Actor);
#if WITH_EDITOR
    Actor->SetActorLabel(Definition.FixtureId.ToString() + TEXT("_Visual"));
#endif
    return Actor;
}

void ADevCourseBootstrap::SpawnLighting()
{
    if (!GetWorld() || bLightingSpawned) return;
    bLightingSpawned = true;

    ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector::ZeroVector, FRotator(-38.0f, -35.0f, 0.0f));
    if (Sun)
    {
        Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        // Runtime-spawned course art is movable, so the sun must be movable as well.
        // This project's current exposure range uses Unreal's compact development-light baseline.
        Sun->GetLightComponent()->SetIntensity(1.25f);
        Sun->GetLightComponent()->SetLightColor(FLinearColor(1.0f, 0.98f, 0.94f));
        if (UDirectionalLightComponent* Directional =
            Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            Directional->SetAtmosphereSunLight(true);
        }
    }

    ASkyAtmosphere* Atmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>();
    (void)Atmosphere;

    ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>();
    if (Sky)
    {
        Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
        Sky->GetLightComponent()->SetIntensity(0.72f);
        // The authored sun/atmosphere is static during play. Capture it once so
        // traversal does not rebuild draw commands for an unchanged real-time sky.
        Sky->GetLightComponent()->SetRealTimeCapture(false);
        Sky->GetLightComponent()->RecaptureSky();
    }
}

void ADevCourseBootstrap::DestroyGeneratedCourse()
{
    for (AActor* Actor : SpawnedCourseActors)
    {
        if (IsValid(Actor)) Actor->Destroy();
    }
    SpawnedCourseActors.Reset();
    PineRidgeHoles.Reset();
    PineRidgeFlyovers.Reset();
    PineRidgeReviews.Reset();
    PineRidgeFoliage.Reset();
    PineRidgeWater.Reset();
    Hole = nullptr;
    FlyoverRoute = nullptr;
    LevelDesignReview = nullptr;
    FoliagePresentation = nullptr;
    WaterPresentation = nullptr;
    CourseTerrainPresentation = nullptr;
    EnvironmentController = nullptr;
    EnvironmentZones.Reset();
    BuiltCourseId = NAME_None;
}

ADiscGolfHoleActor* ADevCourseBootstrap::BuildPracticeHole()
{
    if (!GetWorld()) return nullptr;

    DestroyGeneratedCourse();
    SpawnLighting();

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    // 160 m x 90 m playable pad with its top surface at world Z=0.
    SpawnCourseSurface(Cube, FVector(7800, 0, -50), FVector(160.0f, 90.0f, 1.0f),
        ECourseSurfaceType::Fairway);
    // Tee pad.
    SpawnCourseSurface(Cube, FVector(0, 0, 3), FVector(1.8f, 0.9f, 0.06f),
        ECourseSurfaceType::TeePad);

    // Raised two-centimeter regression patches keep surface responses testable without authored materials.
    SpawnCourseSurface(Cube, FVector(4200, -2600, 1), FVector(18.0f, 9.0f, 0.02f),
        ECourseSurfaceType::LightRough);
    SpawnCourseSurface(Cube, FVector(5000, 3350, 2), FVector(12.0f, 6.0f, 0.04f),
        ECourseSurfaceType::DeepRough);
    SpawnCourseSurface(Cube, FVector(6500, 2350, 1), FVector(15.0f, 8.0f, 0.02f),
        ECourseSurfaceType::Dirt);
    SpawnCourseSurface(Cube, FVector(8800, -2350, 1), FVector(11.0f, 7.0f, 0.02f),
        ECourseSurfaceType::Rock);

    // Rules fixtures sit well outside the calm baseline. OB takes last-in-bounds
    // relief; hazard adds a stroke but remains playable from the result.
    SpawnCourseSurface(Cube, FVector(6200, -4050, 2), FVector(24.0f, 30.0f, 0.04f),
        ECourseSurfaceType::OutOfBounds);
    SpawnCourseSurface(Cube, FVector(8300, 3900, 2), FVector(20.0f, 18.0f, 0.04f),
        ECourseSurfaceType::Hazard);

    const FVector TreePositions[] =
    {
        FVector(2600, -1000, 0), FVector(3300, 1450, 0), FVector(4600, -1750, 0),
        FVector(5500, 1150, 0), FVector(6200, -850, 0), FVector(7200, 1750, 0),
        FVector(7900, -1600, 0), FVector(9000, 1100, 0), FVector(9700, -1000, 0)
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(TreePositions); ++Index)
    {
        SpawnTree(FName(*FString::Printf(TEXT("PracticeTree%02d"), Index + 1)),
            TreePositions[Index], 0.85f + 0.08f * (Index % 4));
    }

    Hole = GetWorld()->SpawnActor<ADiscGolfHoleActor>();
    if (!Hole) return nullptr;
    TrackActor(Hole);

    Hole->HoleNumber = 1;
    Hole->Par = 3;
    Hole->HoleName = FText::FromString(TEXT("Pine Ridge Opening"));
    Hole->TeeLocation = FVector(0, 0, 0);
    Hole->BasketLocation = FVector(11000, 800, 0);
    Hole->Basket = GetWorld()->SpawnActor<ABasketActor>(Hole->BasketLocation, FRotator::ZeroRotator);
    TrackActor(Hole->Basket);
    Hole->CourseId = TEXT("RegressionCourse");
    Hole->LayoutId = TEXT("Practice");
    Hole->DefinitionVersion = 1;
    Hole->DefinitionSource = TEXT("SOURCE BUILT");
    Hole->SurfaceCount = 8;
    Hole->WorldFixtureCount = UE_ARRAY_COUNT(TreePositions);
    BuiltCourseId = Hole->CourseId;

    // Small basket-base marker improves visibility in the primitive dev course.
    SpawnPrimitive(Cylinder, Hole->BasketLocation + FVector(0, 0, 3), FVector(0.75f, 0.75f, 0.05f));
    return Hole;
}

ADiscGolfHoleActor* ADevCourseBootstrap::BuildCourse(const FName& CourseId, FString& OutError)
{
    if (CourseId == TEXT("PineRidge") || CourseId == TEXT("PineRidgeChampionship"))
    {
        return BuildPineRidgeHole1(OutError);
    }
    if (CourseId == TEXT("Regression") || CourseId == TEXT("RegressionCourse")
        || CourseId == TEXT("Practice"))
    {
        OutError.Reset();
        return BuildPracticeHole();
    }
    OutError = FString::Printf(TEXT("unknown course '%s'"), *CourseId.ToString());
    return nullptr;
}

ADiscGolfHoleActor* ADevCourseBootstrap::BuildPineRidgeHole1(FString& OutError)
{
    return BuildPineRidgeHole(1, OutError);
}

ADiscGolfHoleActor* ADevCourseBootstrap::BuildPineRidgeHole(int32 HoleNumber, FString& OutError)
{
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    if (!DiscGolfCourseDefinition::LoadPineRidgeHole(HoleNumber, Definition, Source, OutError)) return nullptr;
    return BuildAuthoredHole(Definition, Source, OutError);
}

ADiscGolfHoleActor* ADevCourseBootstrap::BuildPersistentPineRidgeCourse(
    const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
    FString& OutError)
{
    if (!GetWorld() || Definitions.Num() != 3)
    {
        OutError = TEXT("persistent Pine Ridge requires one world and all three hole definitions");
        return nullptr;
    }
    DestroyGeneratedCourse();
    SpawnLighting();
    for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
    {
        if (!BuildAuthoredHole(Definition, TEXT("PERSISTENT AUTHORED COURSE"), OutError, false))
        {
            DestroyGeneratedCourse();
            return nullptr;
        }
    }

    if (!BuildPineRidgeHole1Environment(Definitions[0], OutError))
    {
        DestroyGeneratedCourse();
        return nullptr;
    }

    FDiscGolfCoursePresentationDefinition Presentation;
    FString PresentationSource;
    FString PresentationError;
    if (!DiscGolfCoursePresentation::LoadPineRidge(
        Presentation, PresentationSource, PresentationError))
    {
        OutError = FString::Printf(TEXT("persistent ground presentation failed to load: %s"),
            *PresentationError);
        DestroyGeneratedCourse();
        return nullptr;
    }
    const IConsoleVariable* FoliageQuality = IConsoleManager::Get().FindConsoleVariable(
        TEXT("sg.FoliageQuality"));
    const int32 QualityLevel = FoliageQuality ? FoliageQuality->GetInt() : 1;
    const FName QualityTierId = QualityLevel <= 0 ? FName(TEXT("Low"))
        : QualityLevel == 1 ? FName(TEXT("Medium")) : FName(TEXT("High"));
    const FDiscGolfCourseVisualQualityTier* Tier = Presentation.QualityTiers.FindByPredicate(
        [QualityTierId](const FDiscGolfCourseVisualQualityTier& Candidate)
        {
            return Candidate.TierId == QualityTierId;
        });
    TArray<int32> TerrainSeeds;
    for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
    {
        const FDiscGolfHoleVisualPlan* Plan = Presentation.Holes.FindByPredicate(
            [&Definition](const FDiscGolfHoleVisualPlan& Candidate)
            {
                return Candidate.HoleNumber == Definition.HoleNumber;
            });
        if (!Plan)
        {
            OutError = FString::Printf(TEXT("persistent ground plan missing for Hole %d"),
                Definition.HoleNumber);
            DestroyGeneratedCourse();
            return nullptr;
        }
        TerrainSeeds.Add(Plan->FoliageSeed);
    }
    CourseTerrainPresentation = GetWorld()->SpawnActor<ADiscGolfTerrainPresentationActor>();
    if (!Tier || !CourseTerrainPresentation
        || !CourseTerrainPresentation->ConfigureCourse(
            Definitions, TerrainSeeds, Tier->GrassDensityScale,
            Tier->FoliageCullDistanceScale))
    {
        OutError = TEXT("persistent course ground mesh or grass layer failed to configure");
        if (CourseTerrainPresentation) CourseTerrainPresentation->Destroy();
        CourseTerrainPresentation = nullptr;
        DestroyGeneratedCourse();
        return nullptr;
    }
    TrackActor(CourseTerrainPresentation);
    for (TActorIterator<ADiscGolfCourseSurfaceActor> It(GetWorld()); It; ++It)
    {
        const ECourseSurfaceType SurfaceType = It->GetCourseSurfaceType();
        if (SurfaceType == ECourseSurfaceType::Fairway
            || SurfaceType == ECourseSurfaceType::LightRough
            || SurfaceType == ECourseSurfaceType::DeepRough
            || SurfaceType == ECourseSurfaceType::Dirt)
        {
            // The actor and primitive remain the sealed lie/collision authority. Only its
            // blockout render mesh is replaced by the successful continuous presentation.
            It->GetStaticMeshComponent()->SetHiddenInGame(true);
        }
    }
#if WITH_EDITOR
    CourseTerrainPresentation->SetActorLabel(TEXT("PineRidge_PersistentCourseGround"));
#endif
    if (!WritePineRidgeHole1EnvironmentStatistics(Definitions[0], OutError))
    {
        DestroyGeneratedCourse();
        return nullptr;
    }
    if (!ActivatePineRidgeHole(1, OutError))
    {
        DestroyGeneratedCourse();
        return nullptr;
    }
    BuiltCourseId = TEXT("PineRidgeChampionship");
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Persistent Pine Ridge assembled: %d holes coexist in one world."), PineRidgeHoles.Num());
    OutError.Reset();
    return Hole;
}

bool ADevCourseBootstrap::ActivatePineRidgeHole(int32 HoleNumber, FString& OutError)
{
    const TObjectPtr<ADiscGolfHoleActor>* FoundHole = PineRidgeHoles.Find(HoleNumber);
    if (!FoundHole || !IsValid(FoundHole->Get()))
    {
        OutError = FString::Printf(TEXT("persistent Pine Ridge hole %d is unavailable"), HoleNumber);
        return false;
    }
    for (const TPair<int32, TObjectPtr<ADiscGolfLevelDesignReviewActor>>& Pair : PineRidgeReviews)
    {
        if (IsValid(Pair.Value)) Pair.Value->SetReviewVisible(false);
    }
    Hole = FoundHole->Get();
    FlyoverRoute = PineRidgeFlyovers.Contains(HoleNumber) ? PineRidgeFlyovers[HoleNumber] : nullptr;
    LevelDesignReview = PineRidgeReviews.Contains(HoleNumber) ? PineRidgeReviews[HoleNumber] : nullptr;
    FoliagePresentation = PineRidgeFoliage.Contains(HoleNumber) ? PineRidgeFoliage[HoleNumber] : nullptr;
    WaterPresentation = PineRidgeWater.Contains(HoleNumber) ? PineRidgeWater[HoleNumber] : nullptr;
    OutError.Reset();
    return true;
}

ADiscGolfFoliagePresentationActor* ADevCourseBootstrap::GetFoliagePresentationForHole(
    int32 HoleNumber) const
{
    const TObjectPtr<ADiscGolfFoliagePresentationActor>* Found = PineRidgeFoliage.Find(HoleNumber);
    return Found ? Found->Get() : nullptr;
}

bool ADevCourseBootstrap::BuildPineRidgeHole1Environment(
    const FDiscGolfHoleBlockoutDefinition& Definition,
    FString& OutError)
{
    const TArray<FDiscGolfHole1EnvironmentZonePlan> Plans =
        PineRidgeHole1Environment::BuildZonePlan(Definition);
    if (!PineRidgeHole1Environment::ValidateClearance(Definition, Plans, OutError)
        || !PineRidgeHole1Environment::ValidateRepresentativeFlightRoutes(Definition, OutError))
    {
        return false;
    }

    EnvironmentController = GetWorld()->SpawnActor<ADiscGolfEnvironmentController>(
        (Definition.TeeLocationCm + Definition.BasketLocationCm) * 0.5f,
        FRotator::ZeroRotator);
    if (!EnvironmentController)
    {
        OutError = TEXT("could not spawn the persistent-course environment controller");
        return false;
    }
    EnvironmentController->ForestPreset = TSoftObjectPtr<UDiscGolfForestPreset>(FSoftObjectPath(
        TEXT("/Game/Environment/Forest/DA_TemperateMountainForest.DA_TemperateMountainForest")));
    EnvironmentController->CourseExtentCm = FVector(90000.0f, 60000.0f, 10000.0f);
    const IConsoleVariable* FoliageQuality = IConsoleManager::Get().FindConsoleVariable(
        TEXT("sg.FoliageQuality"));
    const int32 QualityLevel = FoliageQuality ? FoliageQuality->GetInt() : 2;
    EnvironmentController->Quality = QualityLevel <= 0
        ? EDiscGolfEnvironmentQuality::Performance
        : QualityLevel >= 3 ? EDiscGolfEnvironmentQuality::Cinematic
        : EDiscGolfEnvironmentQuality::High;
    EnvironmentController->ApplyPreset();
    EnvironmentController->OnConstruction(EnvironmentController->GetActorTransform());
    EnvironmentController->Tags.AddUnique(TEXT("Course.PersistentEnvironment"));
    EnvironmentController->Tags.AddUnique(TEXT("Environment.Benchmark.Hole1"));
    TrackActor(EnvironmentController);
#if WITH_EDITOR
    EnvironmentController->SetActorLabel(TEXT("PineRidge_TemperateMountainForest_Controller"));
#endif

    for (const FDiscGolfHole1EnvironmentZonePlan& Plan : Plans)
    {
        ADiscGolfEnvironmentZoneActor* Zone = GetWorld()->SpawnActor<ADiscGolfEnvironmentZoneActor>(
            Plan.LocationCm, FRotator::ZeroRotator);
        if (!Zone)
        {
            OutError = FString::Printf(TEXT("could not spawn Hole 1 environment zone %s"),
                *Plan.ZoneId.ToString());
            return false;
        }
        Zone->ZoneId = Plan.ZoneId;
        Zone->ZoneType = Plan.ZoneType;
        Zone->Shape = Plan.Shape;
        Zone->Priority = Plan.Priority;
        Zone->bHardExclusion = Plan.bHardExclusion;
        Zone->WidthCm = Plan.WidthCm;
        Zone->RadiusCm = Plan.RadiusCm;
        Zone->BoxExtentCm = Plan.BoxExtentCm;
        Zone->TreeSetbackCm = Plan.TreeSetbackCm;
        Zone->BrushSetbackCm = Plan.BrushSetbackCm;
        Zone->BlendFalloffCm = Plan.BlendFalloffCm;
        if (Plan.Shape == EDiscGolfEnvironmentZoneShape::SplineCorridor
            && Plan.SplinePointsCm.Num() >= 2)
        {
            Zone->Spline->SetSplinePoints(Plan.SplinePointsCm,
                ESplineCoordinateSpace::World, true);
        }
        Zone->OnConstruction(Zone->GetActorTransform());
        Zone->Tags.AddUnique(TEXT("Course.Hole.1"));
        Zone->Tags.AddUnique(TEXT("Environment.Benchmark.Hole1"));
        TrackActor(Zone);
        EnvironmentZones.Add(Zone);
#if WITH_EDITOR
        Zone->SetActorLabel(FString::Printf(TEXT("PineRidge_%s"), *Plan.ZoneId.ToString()));
#endif
    }
    OutError.Reset();
    return true;
}

bool ADevCourseBootstrap::WritePineRidgeHole1EnvironmentStatistics(
    const FDiscGolfHoleBlockoutDefinition& Definition,
    FString& OutError) const
{
    FDiscGolfHole1EnvironmentStatistics Stats;
    Stats.TreeInstances = Definition.Trees.Num();
    if (const ADiscGolfFoliagePresentationActor* Foliage = GetFoliagePresentationForHole(1))
    {
        // These HISM visuals represent the current tree population even though the provisional
        // source mesh is a CC0 fir sapling scan. Separate Sapling-slot PCG remains zero until import.
        Stats.TreeInstances = Foliage->GetVisualInstanceCount();
    }
    if (CourseTerrainPresentation)
    {
        Stats.Ferns = CourseTerrainPresentation->GetFernClusterCountForHole(1);
        Stats.GrassGroundCoverInstances =
            CourseTerrainPresentation->GetGrassBladeClusterCountForHole(1)
            + CourseTerrainPresentation->GetLitterClusterCountForHole(1);
    }
    for (TActorIterator<ADiscGolfFixturePresentationActor> It(GetWorld()); It; ++It)
    {
        if (It->ActorHasTag(TEXT("Course.Hole.1"))
            && It->GetFixtureType() == EDiscGolfFixtureType::DenseGrass)
        {
            Stats.Shrubs += It->GetBrushInstanceCount();
        }
    }
    for (const FDiscGolfCollisionFixtureDefinition& Fixture : Definition.CollisionFixtures)
    {
        if (Fixture.FixtureType == EDiscGolfFixtureType::DenseGrass)
        {
            ++Stats.InteractionVolumes;
        }
        else
        {
            ++Stats.CollisionProxies;
            if (Fixture.FixtureType == EDiscGolfFixtureType::Rock) ++Stats.Rocks;
        }
    }
    Stats.CollisionProxies += Definition.Trees.Num();

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("course"), TEXT("Pine Ridge Championship"));
    Root->SetNumberField(TEXT("holeNumber"), 1);
    Root->SetNumberField(TEXT("par"), Definition.Par);
    Root->SetNumberField(TEXT("distanceFeet"),
        DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition));
    Root->SetStringField(TEXT("visualAcceptance"), TEXT("PENDING_FINAL_FAB_IMPORT"));
    Root->SetStringField(TEXT("assetMode"), TEXT("PROVISIONAL_CC0_AND_ENGINE_CONTENT"));
    Root->SetNumberField(TEXT("treeInstances"), Stats.TreeInstances);
    Root->SetNumberField(TEXT("saplings"), Stats.Saplings);
    Root->SetNumberField(TEXT("shrubs"), Stats.Shrubs);
    Root->SetNumberField(TEXT("ferns"), Stats.Ferns);
    Root->SetNumberField(TEXT("grassGroundCoverInstances"), Stats.GrassGroundCoverInstances);
    Root->SetNumberField(TEXT("logsStumps"), Stats.LogsStumps);
    Root->SetNumberField(TEXT("rocks"), Stats.Rocks);
    Root->SetNumberField(TEXT("collisionProxies"), Stats.CollisionProxies);
    Root->SetNumberField(TEXT("interactionVolumes"), Stats.InteractionVolumes);
    Root->SetNumberField(TEXT("environmentZones"), EnvironmentZones.Num());
    Root->SetNumberField(TEXT("pcgGeneratedInstances"), 0);
    Root->SetBoolField(TEXT("pcgGenerationOnDemand"), true);
    Root->SetBoolField(TEXT("usesHISM"), true);
    Root->SetBoolField(TEXT("environmentActorsTick"), false);
    Root->SetBoolField(TEXT("collisionQualityInvariant"), true);
    Root->SetStringField(TEXT("pcgStatus"),
        TEXT("ON_DEMAND_PENDING_FINAL_ASSET_BINDINGS; runtime fallback presentation active"));

    TArray<TSharedPtr<FJsonValue>> Presets;
    for (const TCHAR* Preset : { TEXT("Performance"), TEXT("High"), TEXT("Cinematic") })
    {
        Presets.Add(MakeShared<FJsonValueString>(Preset));
    }
    Root->SetArrayField(TEXT("validatedScalabilityPresets"), Presets);
    if (CourseTerrainPresentation)
    {
        Root->SetNumberField(TEXT("groundCoverCullStartCm"),
            CourseTerrainPresentation->GetGroundCoverCullStartDistanceCm());
        Root->SetNumberField(TEXT("groundCoverCullEndCm"),
            CourseTerrainPresentation->GetGroundCoverCullEndDistanceCm());
    }

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("could not serialize Hole 1 environment statistics");
        return false;
    }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EnvironmentReports"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory,
        TEXT("PineRidgeHole1EnvironmentStatistics.json"));
    if (!FFileHelper::SaveStringToFile(Json, *Path))
    {
        OutError = FString::Printf(TEXT("could not write %s"), *Path);
        return false;
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("Hole 1 environment statistics: %s"), *Path);
    OutError.Reset();
    return true;
}

ADiscGolfHoleActor* ADevCourseBootstrap::BuildAuthoredHole(
    const FDiscGolfHoleBlockoutDefinition& Definition,
    const FString& Source,
    FString& OutError,
    bool bResetCourse)
{
    if (!GetWorld()) { OutError = TEXT("world unavailable"); return nullptr; }
    if (bResetCourse)
    {
        DestroyGeneratedCourse();
        SpawnLighting();
    }
    Hole = nullptr;
    FlyoverRoute = nullptr;
    LevelDesignReview = nullptr;
    FoliagePresentation = nullptr;
    WaterPresentation = nullptr;

    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    const auto MeshForShape = [Cube, Cylinder, Sphere](EDiscGolfPrimitiveShape Shape)
    {
        switch (Shape)
        {
            case EDiscGolfPrimitiveShape::Cylinder: return Cylinder;
            case EDiscGolfPrimitiveShape::Sphere: return Sphere;
            case EDiscGolfPrimitiveShape::Box:
            default: return Cube;
        }
    };

    ADiscGolfCourseSurfaceActor* WaterAuthority = nullptr;
    const FDiscGolfBlockoutSurfaceDefinition* WaterSurfaceDefinition = nullptr;
    for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Definition.Surfaces)
    {
        AActor* Actor = SpawnCourseSurface(MeshForShape(Surface.Shape), Surface.LocationCm,
            Surface.Scale, Surface.SurfaceType, Surface.Rotation);
        if (Actor) Actor->Tags.AddUnique(FName(*FString::Printf(
            TEXT("Course.Hole.%d"), Definition.HoleNumber)));
        if (Surface.SurfaceId.ToString().Contains(TEXT("LakeWater")))
        {
            if (Actor) Actor->Tags.AddUnique(TEXT("Presentation.Surface.Water"));
            if (ADiscGolfCourseSurfaceActor* Water = Cast<ADiscGolfCourseSurfaceActor>(Actor))
            {
                WaterAuthority = Water;
                WaterSurfaceDefinition = &Surface;
                TintMesh(Water->GetStaticMeshComponent(), FLinearColor(0.03f,0.25f,0.46f));
            }
        }
#if WITH_EDITOR
        if (Actor) Actor->SetActorLabel(Surface.SurfaceId.ToString());
#endif
    }
    FDiscGolfCoursePresentationDefinition Presentation;
    FString PresentationSource;
    FString PresentationError;
    ADiscGolfFoliagePresentationActor* Foliage = nullptr;
    float FixtureGrassDensityScale = 0.0f;
    float FixtureCullDistanceScale = 1.0f;
    bool bFixturePresentationEnabled = false;
    if (DiscGolfCoursePresentation::LoadPineRidge(Presentation, PresentationSource, PresentationError))
    {
        const FDiscGolfHoleVisualPlan* Plan = Presentation.Holes.FindByPredicate(
            [&Definition](const FDiscGolfHoleVisualPlan& Candidate)
            {
                return Candidate.HoleNumber == Definition.HoleNumber;
            });
        const IConsoleVariable* FoliageQuality = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.FoliageQuality"));
        const int32 QualityLevel = FoliageQuality ? FoliageQuality->GetInt() : 1;
        const FName QualityTierId = QualityLevel <= 0 ? FName(TEXT("Low"))
            : QualityLevel == 1 ? FName(TEXT("Medium")) : FName(TEXT("High"));
        const FDiscGolfCourseVisualQualityTier* Tier = Presentation.QualityTiers.FindByPredicate(
            [QualityTierId](const FDiscGolfCourseVisualQualityTier& Candidate)
            {
                return Candidate.TierId == QualityTierId;
            });
        if (Plan && Tier)
        {
            FixtureGrassDensityScale = Tier->GrassDensityScale;
            FixtureCullDistanceScale = Tier->FoliageCullDistanceScale;
            bFixturePresentationEnabled = true;
            if (bResetCourse)
            {
                ADiscGolfTerrainPresentationActor* Terrain =
                    GetWorld()->SpawnActor<ADiscGolfTerrainPresentationActor>();
                if (Terrain)
                {
                    TrackActor(Terrain);
                    Terrain->Configure(Definition, Plan->FoliageSeed);
#if WITH_EDITOR
                    Terrain->SetActorLabel(FString::Printf(
                        TEXT("PineRidge_TerrainRelief_Hole%02d"), Definition.HoleNumber));
#endif
                }
            }
            if (WaterAuthority && WaterSurfaceDefinition)
            {
                WaterPresentation = GetWorld()->SpawnActor<ADiscGolfWaterPresentationActor>();
                if (WaterPresentation)
                {
                    TrackActor(WaterPresentation);
                    if (WaterPresentation->Configure(
                        *WaterSurfaceDefinition, Presentation.WaterMaterialPath))
                    {
                        // The invisible authored cylinder continues to own every trace and penalty decision.
                        WaterAuthority->GetStaticMeshComponent()->SetHiddenInGame(true);
#if WITH_EDITOR
                        WaterPresentation->SetActorLabel(TEXT("PineRidge_GalleryLakeWater"));
#endif
                    }
                }
            }
            Foliage = GetWorld()->SpawnActor<ADiscGolfFoliagePresentationActor>();
            if (Foliage)
            {
                if (Foliage->Configure(Definition, *Plan,
                    Tier->FoliageDensityScale, Tier->FoliageCullDistanceScale))
                {
                    TrackActor(Foliage);
                    FoliagePresentation = Foliage;
#if WITH_EDITOR
                    Foliage->SetActorLabel(FString::Printf(
                        TEXT("PineRidge_Foliage_Hole%02d"), Definition.HoleNumber));
#endif
                }
                else
                {
                    Foliage->Destroy();
                    Foliage = nullptr;
                }
            }
        }
    }
    for (int32 TreeIndex = 0; TreeIndex < Definition.Trees.Num(); ++TreeIndex)
    {
        const FDiscGolfTreeDefinition& Tree = Definition.Trees[TreeIndex];
        if (Foliage) Foliage->AddAuthoredTreeVisual(Tree.LocationCm, Tree.HeightScale, TreeIndex);
        SpawnTree(FName(*FString::Printf(TEXT("H%02d_Tree%02d"),
            Definition.HoleNumber, TreeIndex + 1)),
            Tree.LocationCm, Tree.HeightScale, Foliage != nullptr);
    }
    for (const FDiscGolfCollisionFixtureDefinition& Fixture : Definition.CollisionFixtures)
    {
        ADiscGolfWorldFixtureActor* CollisionProxy =
            SpawnCollisionFixture(Fixture, MeshForShape(Fixture.Shape));
        if (CollisionProxy) CollisionProxy->Tags.AddUnique(FName(*FString::Printf(
            TEXT("Course.Hole.%d"), Definition.HoleNumber)));
        ADiscGolfFixturePresentationActor* FixtureVisual = CollisionProxy && bFixturePresentationEnabled
            ? SpawnFixturePresentation(Fixture, FixtureGrassDensityScale, FixtureCullDistanceScale)
            : nullptr;
        if (FixtureVisual)
        {
            // The authored primitive remains the sole competitive proxy; licensed art is read-only presentation.
            CollisionProxy->GetStaticMeshComponent()->SetHiddenInGame(true);
            FixtureVisual->Tags.AddUnique(FName(*FString::Printf(
                TEXT("Course.Hole.%d"), Definition.HoleNumber)));
        }
    }

    for (const FDiscGolfLandingZoneDefinition& Zone : Definition.LandingZones)
    {
        ADiscGolfCourseFeatureActor* Actor = GetWorld()->SpawnActor<ADiscGolfCourseFeatureActor>();
        if (!Actor) continue; TrackActor(Actor); Actor->GetStaticMeshComponent()->SetStaticMesh(Cylinder);
        Actor->ConfigureLandingZone(Zone); TintMesh(Actor->GetStaticMeshComponent(), FLinearColor(0.08f,0.42f,0.52f));
    }
    LevelDesignReview = GetWorld()->SpawnActor<ADiscGolfLevelDesignReviewActor>();
    if (LevelDesignReview)
    {
        TrackActor(LevelDesignReview);
        LevelDesignReview->Configure(Definition.ShotRoutes, Definition.LandingZones);
#if WITH_EDITOR
        LevelDesignReview->SetActorLabel(FString::Printf(
            TEXT("PineRidge_LevelDesignReview_Hole%02d"), Definition.HoleNumber));
#endif
    }
    for (const FDiscGolfCameraAnchorDefinition& Anchor : Definition.CameraAnchors)
    {
        ADiscGolfCourseFeatureActor* Actor = GetWorld()->SpawnActor<ADiscGolfCourseFeatureActor>();
        if (!Actor) continue; TrackActor(Actor); Actor->GetStaticMeshComponent()->SetStaticMesh(Cylinder);
        Actor->ConfigureCameraAnchor(Anchor); TintMesh(Actor->GetStaticMeshComponent(), FLinearColor(0.10f,0.35f,0.75f));
    }
    for (const FDiscGolfSpectatorBoundaryDefinition& Boundary : Definition.SpectatorBoundaries)
    {
        ADiscGolfCourseFeatureActor* Actor = GetWorld()->SpawnActor<ADiscGolfCourseFeatureActor>();
        if (!Actor) continue; TrackActor(Actor); Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
        Actor->ConfigureSpectatorBoundary(Boundary); TintMesh(Actor->GetStaticMeshComponent(), FLinearColor(0.80f,0.62f,0.08f));
    }
    for (const FDiscGolfWindZoneDefinition& Zone : Definition.WindZones)
    {
        ADiscGolfWindZoneActor* Actor = GetWorld()->SpawnActor<ADiscGolfWindZoneActor>();
        if (!Actor) continue; TrackActor(Actor); Actor->Configure(Zone, Cube);
    }

    FlyoverRoute = GetWorld()->SpawnActor<ADiscGolfFlyoverRouteActor>();
    if (FlyoverRoute)
    {
        TrackActor(FlyoverRoute);
        FlyoverRoute->Configure(Definition.FlyoverPointsCm, Definition.BasketLocationCm);
    }

    Hole = GetWorld()->SpawnActor<ADiscGolfHoleActor>();
    if (!Hole) { OutError = TEXT("could not spawn hole actor"); return nullptr; }
    TrackActor(Hole);
    Hole->HoleNumber = Definition.HoleNumber;
    Hole->Par = Definition.Par;
    Hole->HoleName = Definition.HoleName;
    Hole->TeeLocation = Definition.TeeLocationCm;
    Hole->BasketLocation = Definition.BasketLocationCm;
    Hole->CourseId = Definition.CourseId;
    Hole->LayoutId = Definition.LayoutId;
    Hole->DefinitionVersion = Definition.SchemaVersion;
    Hole->DefinitionSource = Source;
    Hole->SurfaceCount = Definition.Surfaces.Num();
    Hole->WorldFixtureCount = Definition.Trees.Num() + Definition.CollisionFixtures.Num();
    Hole->LandingZoneCount = Definition.LandingZones.Num();
    Hole->ShotRouteCount = Definition.ShotRoutes.Num();
    Hole->CameraAnchorCount = Definition.CameraAnchors.Num();
    Hole->SpectatorBoundaryCount = Definition.SpectatorBoundaries.Num();
    Hole->WindZoneCount = Definition.WindZones.Num();
    Hole->FlyoverRoute = FlyoverRoute;
    Hole->Basket = GetWorld()->SpawnActor<ABasketActor>(Hole->BasketLocation, FRotator::ZeroRotator);
    TrackActor(Hole->Basket);
    if (Hole->Basket)
    {
        Hole->Basket->Tags.AddUnique(FName(*FString::Printf(
            TEXT("Course.Hole.%d"), Definition.HoleNumber)));
        Hole->Basket->Tags.AddUnique(FName(*FString::Printf(
            TEXT("Basket.Hole.%d"), Definition.HoleNumber)));
    }
    SpawnPrimitive(Cylinder, Hole->BasketLocation + FVector(0,0,3), FVector(0.9f,0.9f,0.06f));
    BuiltCourseId = Hole->CourseId;
    PineRidgeHoles.Add(Definition.HoleNumber, Hole);
    if (FlyoverRoute) PineRidgeFlyovers.Add(Definition.HoleNumber, FlyoverRoute);
    if (LevelDesignReview) PineRidgeReviews.Add(Definition.HoleNumber, LevelDesignReview);
    if (FoliagePresentation) PineRidgeFoliage.Add(Definition.HoleNumber, FoliagePresentation);
    if (WaterPresentation) PineRidgeWater.Add(Definition.HoleNumber, WaterPresentation);
    OutError.Reset();
    return Hole;
}
