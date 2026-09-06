#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../DiscActor.h"
#include "../DiscGolfEnvironmentDataAssets.h"
#include "../DiscGolfEnvironmentController.h"
#include "../DiscGolfEnvironmentZoneActor.h"
#include "../DiscGolfTourGameMode.h"
#include "../DiscGolfVegetationInteractionActor.h"
#include "../DiscGolfCourseDefinition.h"
#include "../DiscFlightComponent.h"
#include "../DiscGolfWindZoneActor.h"
#include "../PineRidgeHole1Environment.h"
#include "../WindDirector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentPresetContractTest,
    "DiscGolfTour.Environment.PresetContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentPresetContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const UDiscGolfForestPreset* Preset = GetDefault<UDiscGolfForestPreset>();
    const UDiscGolfEnvironmentAssetSet* Assets = GetDefault<UDiscGolfEnvironmentAssetSet>();
    TestEqual(TEXT("All production asset categories have an abstraction slot"), Assets->Slots.Num(), 16);
    TestEqual(TEXT("All course vegetation zones have authored defaults"), Preset->ZoneRules.Num(), 6);
    TestTrue(TEXT("Tee tree density is zero"),
        FMath::IsNearlyZero(Preset->ResolveZoneRules(EDiscGolfEnvironmentZoneType::Tee).TreeDensity));
    TestTrue(TEXT("Green tree density is zero"),
        FMath::IsNearlyZero(Preset->ResolveZoneRules(EDiscGolfEnvironmentZoneType::Green).TreeDensity));
    TestTrue(TEXT("Deep rough is denser than the fairway"),
        Preset->ResolveZoneRules(EDiscGolfEnvironmentZoneType::DeepRough).TreeDensity
        > Preset->ResolveZoneRules(EDiscGolfEnvironmentZoneType::Fairway).TreeDensity);
    TestTrue(TEXT("Performance reduces density and range"),
        Preset->Performance.DensityScale < Preset->High.DensityScale
        && Preset->Performance.CullDistanceScale < Preset->High.CullDistanceScale);
    TestTrue(TEXT("Cinematic extends density and range"),
        Preset->Cinematic.DensityScale > Preset->High.DensityScale
        && Preset->Cinematic.CullDistanceScale > Preset->High.CullDistanceScale);
    TestTrue(TEXT("Canopy slows without blocking or stopping"),
        Preset->LightCanopy.VelocityMultiplier > Preset->DenseCanopy.VelocityMultiplier
        && Preset->DenseCanopy.VelocityMultiplier > 0.0f);

    TSet<EDiscGolfEnvironmentAssetCategory> UniqueCategories;
    for (const FDiscGolfEnvironmentAssetSlot& Slot : Assets->Slots)
    {
        UniqueCategories.Add(Slot.Category);
        TestTrue(TEXT("Every environment slot has positive spacing"), Slot.MinimumSpacingCm > 0.0f);
        TestTrue(TEXT("Every environment slot has ordered cull distances"),
            Slot.CullEndCm > Slot.CullStartCm);
    }
    TestEqual(TEXT("Environment slot categories are unique"), UniqueCategories.Num(), 16);
    TestTrue(TEXT("Grass is nonblocking"),
        Assets->Slots[static_cast<int32>(EDiscGolfEnvironmentAssetCategory::Grass)].CollisionMode
        == EDiscGolfEnvironmentCollisionMode::None);
    TestTrue(TEXT("Ferns are nonblocking"),
        Assets->Slots[static_cast<int32>(EDiscGolfEnvironmentAssetCategory::Fern)].CollisionMode
        == EDiscGolfEnvironmentCollisionMode::None);
    TestTrue(TEXT("Logs use physical collision"),
        Assets->Slots[static_cast<int32>(EDiscGolfEnvironmentAssetCategory::Log)].CollisionMode
        == EDiscGolfEnvironmentCollisionMode::SolidBlocking);
    TestTrue(TEXT("Random seed is explicit"), Preset->Clearance.RandomSeed != 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentZoneGeometryTest,
    "DiscGolfTour.Environment.ZoneGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentZoneGeometryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    ADiscGolfEnvironmentZoneActor* Zone = NewObject<ADiscGolfEnvironmentZoneActor>();
    Zone->Shape = EDiscGolfEnvironmentZoneShape::Radial;
    Zone->RadiusCm = 1000.0f;
    Zone->BlendFalloffCm = 300.0f;
    TestTrue(TEXT("Radial green contains a point inside its radius"),
        Zone->ContainsForCategory(FVector(900.0f, 0.0f, 0.0f), EDiscGolfEnvironmentAssetCategory::TreeConiferLarge));
    TestFalse(TEXT("Radial green rejects a point outside its radius"),
        Zone->ContainsForCategory(FVector(1100.0f, 0.0f, 0.0f), EDiscGolfEnvironmentAssetCategory::TreeConiferLarge));
    TestTrue(TEXT("Density is full inside the authored core"), FMath::IsNearlyEqual(
        Zone->GetInfluenceForCategory(FVector(900.0f, 0.0f, 0.0f),
            EDiscGolfEnvironmentAssetCategory::TreeConiferLarge), 1.0f));
    TestTrue(TEXT("Density feathers through the transition instead of forming a wall"),
        FMath::IsNearlyEqual(Zone->GetInfluenceForCategory(FVector(1150.0f, 0.0f, 0.0f),
            EDiscGolfEnvironmentAssetCategory::TreeConiferLarge), 0.5f, 0.01f));
    TestTrue(TEXT("Density reaches zero outside the transition"), FMath::IsNearlyZero(
        Zone->GetInfluenceForCategory(FVector(1350.0f, 0.0f, 0.0f),
            EDiscGolfEnvironmentAssetCategory::TreeConiferLarge)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHole1EnvironmentClearanceTest,
    "DiscGolfTour.Environment.Hole1.ClearanceAndExclusionIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHole1EnvironmentClearanceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    FString Error;
    TestTrue(TEXT("Authored Hole 1 loads"),
        DiscGolfCourseDefinition::LoadPineRidgeHole1(Definition, Source, Error));
    const TArray<FDiscGolfHole1EnvironmentZonePlan> Zones =
        PineRidgeHole1Environment::BuildZonePlan(Definition);
    TestEqual(TEXT("Hole 1 has six ecology/clearance zones"), Zones.Num(), 6);
    TestTrue(TEXT("Tee, green, and fairway clearances validate"),
        PineRidgeHole1Environment::ValidateClearance(Definition, Zones, Error));
    const FDiscGolfHole1EnvironmentZonePlan* Tee = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::Tee; });
    const FDiscGolfHole1EnvironmentZonePlan* Green = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::Green; });
    const FDiscGolfHole1EnvironmentZonePlan* Fairway = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneId == TEXT("H01_PrimaryFairway"); });
    TestTrue(TEXT("Tee exclusion is hard and approximately 35 feet"), Tee
        && Tee->bHardExclusion && FMath::IsNearlyEqual(Tee->RadiusCm, 1067.0f, 1.0f));
    TestTrue(TEXT("Green exclusion protects the basket's 10-meter circle"), Green
        && Green->bHardExclusion && Green->RadiusCm >= 1000.0f);
    TestTrue(TEXT("Trees are set farther back than brush"), Fairway
        && Fairway->TreeSetbackCm > Fairway->BrushSetbackCm);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHole1EnvironmentDeterminismTest,
    "DiscGolfTour.Environment.Hole1.DeterministicGenerationPlan",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHole1EnvironmentDeterminismTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfHoleBlockoutDefinition Definition =
        DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    const TArray<FDiscGolfHole1EnvironmentZonePlan> A =
        PineRidgeHole1Environment::BuildZonePlan(Definition);
    const TArray<FDiscGolfHole1EnvironmentZonePlan> B =
        PineRidgeHole1Environment::BuildZonePlan(Definition);
    TestEqual(TEXT("Repeated generation plans have equal zone counts"), A.Num(), B.Num());
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        TestEqual(TEXT("Zone identity is deterministic"), A[Index].ZoneId, B[Index].ZoneId);
        TestEqual(TEXT("Zone priority is deterministic"), A[Index].Priority, B[Index].Priority);
        TestTrue(TEXT("Zone dimensions are deterministic"),
            FMath::IsNearlyEqual(A[Index].WidthCm, B[Index].WidthCm)
            && FMath::IsNearlyEqual(A[Index].RadiusCm, B[Index].RadiusCm));
    }
    const UDiscGolfForestPreset* Preset = GetDefault<UDiscGolfForestPreset>();
    TestEqual(TEXT("PCG uses a stable nonzero seed"), Preset->Clearance.RandomSeed, 18437);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHole1RepresentativeRoutesTest,
    "DiscGolfTour.Environment.Hole1.RepresentativeFlightRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHole1RepresentativeRoutesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    FString Error;
    TestTrue(TEXT("Authored Hole 1 loads"),
        DiscGolfCourseDefinition::LoadPineRidgeHole1(Definition, Source, Error));
    TestTrue(TEXT("Verified driver carry and both shot shapes fit the authored strategy"),
        PineRidgeHole1Environment::ValidateRepresentativeFlightRoutes(Definition, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentWindSynchronizationTest,
    "DiscGolfTour.Environment.WindSynchronization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentWindSynchronizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    ADiscGolfEnvironmentController* Controller = NewObject<ADiscGolfEnvironmentController>();
    AWindDirector* Wind = NewObject<AWindDirector>();
    UDiscGolfForestPreset* Preset = NewObject<UDiscGolfForestPreset>();
    Preset->Wind.Direction = FVector(3.0f, 4.0f, 0.0f);
    Preset->Wind.SpeedMps = 5.0f;
    Preset->Wind.GustStrengthMps = 1.8f;
    Preset->Wind.GustFrequencyHz = 0.2f;
    Controller->ForestPreset = Preset;
    TestTrue(TEXT("Valid environment wind synchronizes atomically"),
        Controller->SynchronizeWindDirector(Wind));
    TestTrue(TEXT("Disc wind receives normalized foliage direction and speed"),
        Wind->BaseWindMps.Equals(FVector(3.0f, 4.0f, 0.0f), 0.01f));
    TestTrue(TEXT("Disc wind receives shared gust strength"),
        FMath::IsNearlyEqual(Wind->GustAmplitudeMps, 1.8f));
    TestTrue(TEXT("Environment controller does not tick"),
        !Controller->PrimaryActorTick.bCanEverTick);

    const FVector ValidBaseWind = Wind->BaseWindMps;
    const float ValidGustAmplitude = Wind->GustAmplitudeMps;
    const float ValidGustFrequency = Wind->GustFrequencyHz;
    AddExpectedError(
        TEXT("Environment wind synchronization rejected invalid direction/speed data."),
        EAutomationExpectedErrorFlags::Contains, 2);
    Preset->Wind.SpeedMps = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite synchronized wind speed fails closed"),
        Controller->SynchronizeWindDirector(Wind));
    TestTrue(TEXT("Rejected synchronization leaves base wind unchanged"),
        Wind->BaseWindMps.Equals(ValidBaseWind));
    TestTrue(TEXT("Rejected synchronization leaves gust amplitude unchanged"),
        FMath::IsNearlyEqual(Wind->GustAmplitudeMps, ValidGustAmplitude));
    TestTrue(TEXT("Rejected synchronization leaves gust frequency unchanged"),
        FMath::IsNearlyEqual(Wind->GustFrequencyHz, ValidGustFrequency));

    Preset->Wind.SpeedMps = 5.0f;
    Preset->Wind.Direction.X = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite synchronized wind direction fails closed"),
        Controller->SynchronizeWindDirector(Wind));
    TestTrue(TEXT("Invalid direction cannot partially mutate physics wind"),
        Wind->BaseWindMps.Equals(ValidBaseWind)
        && FMath::IsNearlyEqual(Wind->GustAmplitudeMps, ValidGustAmplitude)
        && FMath::IsNearlyEqual(Wind->GustFrequencyHz, ValidGustFrequency));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfWindSafetyContractTest,
    "DiscGolfTour.Environment.Wind.SafetyContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfWindSafetyContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    AWindDirector* Wind = NewObject<AWindDirector>();
    FString Error;
    TestTrue(TEXT("Finite direct runtime configuration is accepted"),
        Wind->TryConfigurePhysicsWind(FVector(4.0f, 1.0f, 0.0f), 2.0f, 0.4f, Error));
    const FVector AcceptedBaseWind = Wind->BaseWindMps;

    TestFalse(TEXT("NaN base wind is rejected"), Wind->TryConfigurePhysicsWind(
        FVector(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f),
        2.0f, 0.4f, Error));
    TestTrue(TEXT("Rejected direct configuration is atomic"),
        Wind->BaseWindMps.Equals(AcceptedBaseWind));
    TestFalse(TEXT("Infinite gust amplitude is rejected"), Wind->TryConfigurePhysicsWind(
        AcceptedBaseWind, std::numeric_limits<float>::infinity(), 0.4f, Error));
    TestFalse(TEXT("Out-of-envelope gust frequency is rejected"),
        Wind->TryConfigurePhysicsWind(AcceptedBaseWind, 2.0f, 2.01f, Error));

    FVector Sample = FVector::ZeroVector;
    Wind->BaseWindMps.X = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Direct property corruption cannot produce a physics sample"),
        Wind->TryGetWindMpsAtSimulationTime(FVector::ZeroVector, 1.0f, Sample, Error));
    TestTrue(TEXT("Rejected samples return a finite calm output"),
        Sample == FVector::ZeroVector);
    TestTrue(TEXT("A valid setter can recover corrupted direct runtime state"),
        Wind->TryConfigurePhysicsWind(AcceptedBaseWind, 2.0f, 0.4f, Error));
    TestFalse(TEXT("Non-finite sample time fails closed"),
        Wind->TryGetWindMpsAtSimulationTime(
            FVector::ZeroVector, std::numeric_limits<float>::quiet_NaN(), Sample, Error));
    TestFalse(TEXT("Non-finite sample location fails closed"),
        Wind->TryGetWindMpsAtSimulationTime(
            FVector(std::numeric_limits<float>::infinity(), 0.0f, 0.0f),
            1.0f, Sample, Error));

    float FirstPhase = 0.0f;
    float RepeatedPhase = 0.0f;
    float SecondPhase = 0.0f;
    TestTrue(TEXT("Accepted shot one produces a deterministic phase"),
        AWindDirector::TryBuildDeterministicShotPhaseOrigin(1, FirstPhase, Error));
    TestTrue(TEXT("Repeating an accepted-shot key reproduces its phase"),
        AWindDirector::TryBuildDeterministicShotPhaseOrigin(1, RepeatedPhase, Error));
    TestTrue(TEXT("Accepted shot two produces a deterministic phase"),
        AWindDirector::TryBuildDeterministicShotPhaseOrigin(2, SecondPhase, Error));
    TestEqual(TEXT("The same accepted-shot key is replay-stable"), FirstPhase, RepeatedPhase);
    TestNotEqual(TEXT("Consecutive accepted throws do not silently restart gust phase"),
        FirstPhase, SecondPhase);
    TestFalse(TEXT("A zero/non-accepted shot key cannot author physics phase"),
        AWindDirector::TryBuildDeterministicShotPhaseOrigin(0, FirstPhase, Error));

    FVector FirstAcceptedWind = FVector::ZeroVector;
    FVector SecondAcceptedWind = FVector::ZeroVector;
    TestTrue(TEXT("Authoritative pre-spawn/HUD seam samples accepted shot one"),
        ADiscGolfTourGameMode::TrySampleWindForAcceptedShot(
            Wind, 1, FVector(500.0f, -250.0f, 100.0f),
            FirstPhase, FirstAcceptedWind, Error));
    TestTrue(TEXT("Authoritative pre-spawn/HUD seam samples accepted shot two"),
        ADiscGolfTourGameMode::TrySampleWindForAcceptedShot(
            Wind, 2, FVector(500.0f, -250.0f, 100.0f),
            SecondPhase, SecondAcceptedWind, Error));
    TestFalse(TEXT("The next accepted-shot HUD preview does not use one reset phase"),
        FirstAcceptedWind.Equals(SecondAcceptedWind, 1.0e-3f));
    TestFalse(TEXT("Authoritative pre-spawn/HUD seam rejects an unaccepted shot key"),
        ADiscGolfTourGameMode::TrySampleWindForAcceptedShot(
            Wind, 0, FVector::ZeroVector,
            RepeatedPhase, FirstAcceptedWind, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfWindZoneRuntimeSafetyTest,
    "DiscGolfTour.Environment.Wind.ZoneRuntimeSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfWindZoneRuntimeSafetyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), TEXT("WindZoneSafetyWorld"));
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Isolated wind-zone test world exists"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    AWindDirector* Wind = World->SpawnActor<AWindDirector>();
    ADiscGolfWindZoneActor* FirstZone = World->SpawnActor<ADiscGolfWindZoneActor>();
    ADiscGolfWindZoneActor* SecondZone = World->SpawnActor<ADiscGolfWindZoneActor>();
    if (!TestNotNull(TEXT("Wind-zone safety director exists"), Wind)
        || !TestNotNull(TEXT("First wind-zone safety actor exists"), FirstZone)
        || !TestNotNull(TEXT("Second wind-zone safety actor exists"), SecondZone))
    {
        return false;
    }

    FDiscGolfWindZoneDefinition FirstDefinition;
    FirstDefinition.ZoneId = TEXT("RuntimeZoneA");
    FirstDefinition.ExtentCm = FVector(1000.0f);
    FirstDefinition.BaseWindScale = 3.0f;
    FirstDefinition.AdditiveWindMps = FVector(25.0f, 0.0f, 0.0f);
    FirstZone->Configure(FirstDefinition, nullptr);

    FDiscGolfWindZoneDefinition SecondDefinition = FirstDefinition;
    SecondDefinition.ZoneId = TEXT("RuntimeZoneB");
    SecondZone->Configure(SecondDefinition, nullptr);
    Wind->RefreshCourseZones();

    FString Error;
    TestTrue(TEXT("Individually bounded wind and zone values validate"),
        Wind->TryConfigurePhysicsWind(
            FVector(25.0f, 0.0f, 0.0f), 0.0f, 0.4f, Error)
        && Wind->ValidatePhysicsWindConfiguration(Error));
    FVector Sample = FVector::ZeroVector;
    TestFalse(TEXT("Cumulative zone output beyond the safe envelope fails closed"),
        Wind->TryGetWindMpsAtSimulationTime(
            FVector::ZeroVector, 1.0f, Sample, Error));
    TestTrue(TEXT("Rejected cumulative zone output never escapes as non-finite physics state"),
        Sample == FVector::ZeroVector);

    SecondZone->BaseWindScale = 1.0f;
    SecondZone->AdditiveWindMps = FVector::ZeroVector;
    TestTrue(TEXT("Bounded cumulative zone output samples successfully"),
        Wind->TryGetWindMpsAtSimulationTime(
            FVector::ZeroVector, 1.0f, Sample, Error));

    SecondZone->ZoneId = FirstZone->ZoneId;
    TestFalse(TEXT("Duplicate live wind-zone ids fail closed"),
        Wind->ValidatePhysicsWindConfiguration(Error));
    SecondZone->ZoneId = NAME_None;
    TestFalse(TEXT("Missing live wind-zone identity fails closed"),
        Wind->ValidatePhysicsWindConfiguration(Error));
    SecondZone->ZoneId = TEXT("RuntimeZoneB");
    SecondZone->AdditiveWindMps.X = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite live zone modifiers fail closed"),
        Wind->ValidatePhysicsWindConfiguration(Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfWindConfigurationSnapshotTest,
    "DiscGolfTour.Environment.Wind.ConfigurationSnapshot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfWindConfigurationSnapshotTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), TEXT("WindSnapshotWorld"));
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Isolated wind snapshot world exists"), World))
    {
        return false;
    }
    if (!TestNotNull(TEXT("Wind snapshot test engine exists"), GEngine))
    {
        World->DestroyWorld(false);
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };

    AWindDirector* Wind = World->SpawnActor<AWindDirector>();
    ADiscGolfWindZoneActor* FirstZone = World->SpawnActor<ADiscGolfWindZoneActor>();
    ADiscGolfWindZoneActor* SecondZone = World->SpawnActor<ADiscGolfWindZoneActor>();
    if (!TestNotNull(TEXT("Wind snapshot director exists"), Wind)
        || !TestNotNull(TEXT("First wind snapshot zone exists"), FirstZone)
        || !TestNotNull(TEXT("Second wind snapshot zone exists"), SecondZone))
    {
        return false;
    }

    FDiscGolfWindZoneDefinition FirstDefinition;
    FirstDefinition.ZoneId = TEXT("SnapshotZoneA");
    FirstDefinition.LocationCm = FVector(-350.0f, 120.0f, 80.0f);
    FirstDefinition.ExtentCm = FVector(900.0f, 700.0f, 300.0f);
    FirstDefinition.BaseWindScale = 0.85f;
    FirstDefinition.AdditiveWindMps = FVector(0.2f, -0.1f, 0.0f);
    FirstZone->Configure(FirstDefinition, nullptr);

    FDiscGolfWindZoneDefinition SecondDefinition;
    SecondDefinition.ZoneId = TEXT("SnapshotZoneB");
    SecondDefinition.LocationCm = FVector(450.0f, -220.0f, 100.0f);
    SecondDefinition.ExtentCm = FVector(500.0f, 650.0f, 250.0f);
    SecondDefinition.BaseWindScale = 1.1f;
    SecondDefinition.AdditiveWindMps = FVector(-0.15f, 0.3f, 0.0f);
    SecondZone->Configure(SecondDefinition, nullptr);
    Wind->RefreshCourseZones();

    FString Error;
    if (!TestTrue(TEXT("Snapshot fixture wind configures"),
        Wind->TryConfigurePhysicsWind(
            FVector(3.5f, 0.75f, 0.0f), 1.25f, 0.35f, Error)))
    {
        return false;
    }

    FDiscGolfPhysicsWindConfigurationSnapshot Accepted;
    FDiscGolfPhysicsWindConfigurationSnapshot Repeated;
    if (!TestTrue(TEXT("Complete ordered physics wind can be frozen"),
        Wind->TryCapturePhysicsWindConfiguration(Accepted, Error))
        || !TestTrue(TEXT("Repeated unchanged wind capture succeeds"),
            Wind->TryCapturePhysicsWindConfiguration(Repeated, Error)))
    {
        return false;
    }
    TestTrue(TEXT("Repeated unchanged wind capture is exactly equal"),
        Accepted.Equals(Repeated));
    TestEqual(TEXT("Snapshot retains both active zones"), Accepted.Zones.Num(), 2);
    if (Accepted.Zones.Num() == 2)
    {
        TestEqual(TEXT("Snapshot retains deterministic zone ordering A"),
            Accepted.Zones[0].ZoneId, FName(TEXT("SnapshotZoneA")));
        TestEqual(TEXT("Snapshot retains deterministic zone ordering B"),
            Accepted.Zones[1].ZoneId, FName(TEXT("SnapshotZoneB")));
    }
    TestTrue(TEXT("Unchanged live wind matches its accepted snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));

    TestTrue(TEXT("A valid director mutation applies"),
        Wind->TryConfigurePhysicsWind(
            FVector(3.75f, 0.75f, 0.0f), 1.25f, 0.35f, Error));
    TestFalse(TEXT("Director mutation invalidates the accepted snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));
    TestTrue(TEXT("Director mutation reports the authoritative freeze violation"),
        Error.Contains(TEXT("changed after authoritative launch")));
    TestTrue(TEXT("Restoring the exact director values succeeds"),
        Wind->TryConfigurePhysicsWind(
            FVector(3.5f, 0.75f, 0.0f), 1.25f, 0.35f, Error));
    TestTrue(TEXT("Exact director restoration matches the snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));

    FirstZone->AdditiveWindMps.Y += 0.25f;
    TestFalse(TEXT("Zone modifier mutation invalidates the accepted snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));
    FirstZone->AdditiveWindMps = FirstDefinition.AdditiveWindMps;
    TestTrue(TEXT("Exact zone modifier restoration matches the snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));

    FirstZone->SetActorLocation(FirstDefinition.LocationCm + FVector(25.0f, 0.0f, 0.0f));
    TestFalse(TEXT("Zone containment-bounds movement invalidates the snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));
    FirstZone->SetActorLocation(FirstDefinition.LocationCm);
    TestTrue(TEXT("Exact zone bounds restoration matches the snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));

    ADiscGolfWindZoneActor* ThirdZone = World->SpawnActor<ADiscGolfWindZoneActor>();
    if (!TestNotNull(TEXT("Third wind snapshot zone exists"), ThirdZone))
    {
        return false;
    }
    FDiscGolfWindZoneDefinition ThirdDefinition;
    ThirdDefinition.ZoneId = TEXT("SnapshotZoneC");
    ThirdDefinition.LocationCm = FVector(1500.0f, 100.0f, 100.0f);
    ThirdDefinition.ExtentCm = FVector(300.0f);
    ThirdZone->Configure(ThirdDefinition, nullptr);
    Wind->RefreshCourseZones();
    TestFalse(TEXT("Refreshed zone membership invalidates the accepted snapshot"),
        Wind->MatchesPhysicsWindConfiguration(Accepted, Error));

    FDiscGolfPhysicsWindConfigurationSnapshot ThreeZoneSnapshot;
    TestTrue(TEXT("Expanded zone membership can be captured"),
        Wind->TryCapturePhysicsWindConfiguration(ThreeZoneSnapshot, Error));
    TestTrue(TEXT("Captured zone destruction succeeds"), ThirdZone->Destroy());
    TestFalse(TEXT("Destroyed captured zone fails closed before a membership refresh"),
        Wind->MatchesPhysicsWindConfiguration(ThreeZoneSnapshot, Error));
    TestTrue(TEXT("Destroyed captured zone reports unavailable membership"),
        Error.Contains(TEXT("unavailable actor")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfWindMidFlightMutationTest,
    "DiscGolfTour.Environment.Wind.MidFlightMutationFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfWindMidFlightMutationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), TEXT("WindMutationFlightWorld"));
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Isolated wind-mutation flight world exists"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    AWindDirector* Wind = World->SpawnActor<AWindDirector>();
    ADiscGolfWindZoneActor* Zone = World->SpawnActor<ADiscGolfWindZoneActor>();
    if (!TestNotNull(TEXT("Wind-mutation director exists"), Wind)
        || !TestNotNull(TEXT("Wind-mutation zone exists"), Zone))
    {
        return false;
    }
    FDiscGolfWindZoneDefinition ZoneDefinition;
    ZoneDefinition.ZoneId = TEXT("MutationZone");
    ZoneDefinition.LocationCm = FVector(0.0f, 0.0f, 2000.0f);
    ZoneDefinition.ExtentCm = FVector(2000.0f);
    ZoneDefinition.BaseWindScale = 0.9f;
    ZoneDefinition.AdditiveWindMps = FVector(0.1f, 0.2f, 0.0f);
    Zone->Configure(ZoneDefinition, nullptr);
    Wind->RefreshCourseZones();

    FString Error;
    if (!TestTrue(TEXT("Wind-mutation fixture configures"),
        Wind->TryConfigurePhysicsWind(
            FVector(3.0f, 0.5f, 0.0f), 1.0f, 0.25f, Error)))
    {
        return false;
    }

    FResolvedDiscDefinition Disc;
    Disc.MoldId = TEXT("WindMutationDisc");
    Disc.DisplayName = FText::FromString(TEXT("Wind Mutation Disc"));
    FThrowRelease Release;
    Release.Direction = FVector::ForwardVector;
    Release.ReleaseSpeedMps = 20.0f;
    Release.SpinRpm = 800.0f;
    Release.EffectiveHyzerDeg = 2.0f;
    Release.EffectiveNoseAngleDeg = 1.0f;
    Release.EffectiveLaunchAngleDeg = 7.0f;
    Release.WindPhaseOriginSeconds = 173.0f;

    ADiscActor* DirectorMutationDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, -200.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Director-mutation disc exists"), DirectorMutationDisc)
        || !TestTrue(TEXT("Director-mutation disc initializes"),
            DirectorMutationDisc->InitializeDisc(Disc, Wind))
        || !TestTrue(TEXT("Director-mutation flight launches"),
            DirectorMutationDisc->Throw(Release)))
    {
        return false;
    }
    UDiscFlightComponent* DirectorMutationFlight =
        DirectorMutationDisc->GetFlightComponent();
    if (!TestNotNull(TEXT("Director-mutation flight exists"), DirectorMutationFlight))
    {
        return false;
    }

    AddExpectedError(
        TEXT("Physics wind configuration changed after authoritative launch"),
        EAutomationExpectedErrorFlags::Contains, 2);
    TestTrue(TEXT("Valid mid-flight director mutation applies"),
        Wind->TryConfigurePhysicsWind(
            FVector(3.25f, 0.5f, 0.0f), 1.0f, 0.25f, Error));
    DirectorMutationFlight->TickComponent(
        1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Director mutation terminates the accepted flight"),
        DirectorMutationFlight->IsFlying());
    TestEqual(TEXT("Director mutation settles fail-closed flight"),
        DirectorMutationFlight->GetFlightState(), EDiscFlightState::Settled);

    ADiscActor* ZoneMutationDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 200.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Zone-mutation disc exists"), ZoneMutationDisc)
        || !TestTrue(TEXT("Zone-mutation disc initializes"),
            ZoneMutationDisc->InitializeDisc(Disc, Wind))
        || !TestTrue(TEXT("Zone-mutation flight launches"),
            ZoneMutationDisc->Throw(Release)))
    {
        return false;
    }
    UDiscFlightComponent* ZoneMutationFlight = ZoneMutationDisc->GetFlightComponent();
    if (!TestNotNull(TEXT("Zone-mutation flight exists"), ZoneMutationFlight))
    {
        return false;
    }

    Zone->AdditiveWindMps.Y += 0.25f;
    ZoneMutationFlight->TickComponent(
        1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Zone mutation terminates the accepted flight"),
        ZoneMutationFlight->IsFlying());
    TestEqual(TEXT("Zone mutation settles fail-closed flight"),
        ZoneMutationFlight->GetFlightState(), EDiscFlightState::Settled);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfFixedStepGustDeterminismTest,
    "DiscGolfTour.Environment.Wind.FixedStepGustDeterminism",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfFixedStepGustDeterminismTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    struct FFlightSnapshot
    {
        FVector LocationCm = FVector::ZeroVector;
        FVector VelocityMps = FVector::ZeroVector;
        FDiscFlightTelemetry Telemetry;
        TArray<FDiscTrajectorySample> Samples;
    };

    const auto RunFlight = [this](int32 RenderFps, FFlightSnapshot& OutSnapshot)
    {
        const FName WorldName = MakeUniqueObjectName(
            GetTransientPackage(), UWorld::StaticClass(),
            FName(*FString::Printf(TEXT("FixedStepGustWorld%d"), RenderFps)));
        UWorld* World = UWorld::CreateWorld(
            EWorldType::Game, false, WorldName, GetTransientPackage());
        if (!TestNotNull(TEXT("Isolated gust test world exists"), World))
        {
            return false;
        }
        ON_SCOPE_EXIT
        {
            World->DestroyWorld(false);
        };

        AWindDirector* Wind = World->SpawnActor<AWindDirector>();
        ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
            FVector(0.0f, 0.0f, 2000.0f), FRotator::ZeroRotator);
        if (!TestNotNull(TEXT("Gust test wind director exists"), Wind)
            || !TestNotNull(TEXT("Gust test disc exists"), DiscActor))
        {
            return false;
        }

        FString WindError;
        if (!TestTrue(TEXT("Nonzero gust fixture configures"),
            Wind->TryConfigurePhysicsWind(
                FVector(4.2f, 1.6f, 0.0f), 3.5f, 1.7f, WindError)))
        {
            return false;
        }

        FResolvedDiscDefinition Disc;
        Disc.MoldId = TEXT("FixedStepGustDisc");
        Disc.DisplayName = FText::FromString(TEXT("Fixed-Step Gust Disc"));
        if (!TestTrue(TEXT("Disc accepts the validated wind director"),
            DiscActor->InitializeDisc(Disc, Wind)))
        {
            return false;
        }
        UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();
        if (!TestNotNull(TEXT("Validated nonzero-gust flight component exists"), Flight))
        {
            return false;
        }
        Flight->FixedStepSeconds = 0.001f;

        FThrowRelease Release;
        Release.Direction = FVector::ForwardVector;
        Release.ReleaseSpeedMps = 21.0f;
        Release.SpinRpm = 850.0f;
        Release.EffectiveHyzerDeg = 3.0f;
        Release.EffectiveNoseAngleDeg = 1.0f;
        Release.EffectiveLaunchAngleDeg = 8.0f;
        Release.WindPhaseOriginSeconds = 137.0f;
        if (!TestTrue(TEXT("Validated nonzero-gust release is accepted"),
            DiscActor->Throw(Release)))
        {
            return false;
        }
        if (!TestTrue(TEXT("Validated nonzero-gust flight launches"),
            Flight->IsFlying()))
        {
            return false;
        }

        constexpr float SimulatedSeconds = 0.5f;
        const int32 RenderFrames = FMath::RoundToInt(
            static_cast<float>(RenderFps) * SimulatedSeconds);
        const float RenderDeltaSeconds = 1.0f / static_cast<float>(RenderFps);
        for (int32 Frame = 0; Frame < RenderFrames && Flight->IsFlying(); ++Frame)
        {
            // Deliberately advance the presentation clock once per render
            // frame. A regression to GetWindMpsAt() inside airborne physics
            // makes the grouped solver steps and final flight state diverge.
            Wind->Tick(RenderDeltaSeconds);
            Flight->TickComponent(
                RenderDeltaSeconds, LEVELTICK_All, nullptr);
        }

        OutSnapshot.LocationCm = DiscActor->GetActorLocation();
        OutSnapshot.VelocityMps = Flight->GetVelocityMps();
        OutSnapshot.Telemetry = Flight->GetTelemetry();
        OutSnapshot.Samples = Flight->GetTrajectorySamples();
        return Flight->IsFlying();
    };

    FFlightSnapshot At30;
    FFlightSnapshot At60;
    FFlightSnapshot At120;
    if (!RunFlight(30, At30) || !RunFlight(60, At60) || !RunFlight(120, At120))
    {
        return false;
    }

    TestTrue(TEXT("30/60 FPS accumulator grouping produces identical flight position"),
        At30.LocationCm.Equals(At60.LocationCm, 1.0e-3f));
    TestTrue(TEXT("60/120 FPS accumulator grouping produces identical flight position"),
        At60.LocationCm.Equals(At120.LocationCm, 1.0e-3f));
    TestTrue(TEXT("30/60 FPS accumulator grouping produces identical flight velocity"),
        At30.VelocityMps.Equals(At60.VelocityMps, 1.0e-5f));
    TestTrue(TEXT("60/120 FPS accumulator grouping produces identical flight velocity"),
        At60.VelocityMps.Equals(At120.VelocityMps, 1.0e-5f));
    TestEqual(TEXT("30/60 FPS retain the same fixed-step sample count"),
        At30.Samples.Num(), At60.Samples.Num());
    TestEqual(TEXT("60/120 FPS retain the same fixed-step sample count"),
        At60.Samples.Num(), At120.Samples.Num());
    TestTrue(TEXT("One-half second at 240 Hz retains the anchored sample cadence"),
        At30.Samples.Num() >= 120 && At30.Samples.Num() <= 121);
    TestTrue(TEXT("The accumulator advances exactly one half-second of solver time"),
        FMath::IsNearlyEqual(At30.Telemetry.FlightTimeSeconds, 0.5f, 1.0e-4f)
        && FMath::IsNearlyEqual(At60.Telemetry.FlightTimeSeconds, 0.5f, 1.0e-4f)
        && FMath::IsNearlyEqual(At120.Telemetry.FlightTimeSeconds, 0.5f, 1.0e-4f));
    TestTrue(TEXT("Telemetry preserves the authored deterministic shot phase"),
        FMath::IsNearlyEqual(At30.Telemetry.WindPhaseOriginSeconds, 137.0f)
        && FMath::IsNearlyEqual(At60.Telemetry.WindPhaseOriginSeconds, 137.0f)
        && FMath::IsNearlyEqual(At120.Telemetry.WindPhaseOriginSeconds, 137.0f));

    bool bObservedTimeVaryingGust = false;
    for (int32 Index = 1; Index < At30.Samples.Num(); ++Index)
    {
        if (!At30.Samples[Index].WindMps.Equals(At30.Samples[0].WindMps, 1.0e-3f))
        {
            bObservedTimeVaryingGust = true;
            break;
        }
    }
    TestTrue(TEXT("The end-to-end flight exercises a time-varying nonzero gust"),
        bObservedTimeVaryingGust && !At30.Samples.IsEmpty()
        && At30.Samples[0].WindMps.Size() > 0.1f);

    if (At30.Samples.Num() == At60.Samples.Num()
        && At60.Samples.Num() == At120.Samples.Num())
    {
        bool bAllWindSamplesMatch = true;
        for (int32 Index = 0; Index < At30.Samples.Num(); ++Index)
        {
            bAllWindSamplesMatch &= At30.Samples[Index].WindMps.Equals(
                At60.Samples[Index].WindMps, 1.0e-5f);
            bAllWindSamplesMatch &= At60.Samples[Index].WindMps.Equals(
                At120.Samples[Index].WindMps, 1.0e-5f);
        }
        TestTrue(TEXT("All authoritative gust samples are frame-group independent"),
            bAllWindSamplesMatch);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfVegetationInteractionContractTest,
    "DiscGolfTour.Environment.VegetationInteractionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfVegetationInteractionContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const ADiscGolfVegetationInteractionActor* Interaction =
        NewObject<ADiscGolfVegetationInteractionActor>();
    TestTrue(TEXT("Canopy/shrub proxy is query-only overlap collision"),
        Interaction->HasValidInteractionContract());
    return true;
}

#endif
