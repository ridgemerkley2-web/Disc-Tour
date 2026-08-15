#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfEnvironmentDataAssets.h"
#include "../DiscGolfEnvironmentController.h"
#include "../DiscGolfEnvironmentZoneActor.h"
#include "../DiscGolfVegetationInteractionActor.h"
#include "../DiscGolfCourseDefinition.h"
#include "../PineRidgeHole1Environment.h"
#include "../WindDirector.h"

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
    Controller->SynchronizeWindDirector(Wind);
    TestTrue(TEXT("Disc wind receives normalized foliage direction and speed"),
        Wind->BaseWindMps.Equals(FVector(3.0f, 4.0f, 0.0f), 0.01f));
    TestTrue(TEXT("Disc wind receives shared gust strength"),
        FMath::IsNearlyEqual(Wind->GustAmplitudeMps, 1.8f));
    TestTrue(TEXT("Environment controller does not tick"),
        !Controller->PrimaryActorTick.bCanEverTick);
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
