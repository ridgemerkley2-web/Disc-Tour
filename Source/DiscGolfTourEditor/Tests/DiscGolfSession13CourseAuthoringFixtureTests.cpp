#if WITH_DEV_AUTOMATION_TESTS

#include "DGFrameworkCourseDefinition.h"
#include "DiscGolfCourseAuthoringExporter.h"
#include "DiscGolfCourseAuthoringPcgAdapter.h"
#include "DiscGolfCourseAuthoringValidation.h"
#include "DiscGolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

namespace
{
    FDGCourseZoneDefinition Rectangle(
        FName Id,
        EDGCourseZoneType Type,
        double MinX,
        double MinY,
        double MaxX,
        double MaxY)
    {
        FDGCourseZoneDefinition Zone;
        Zone.ZoneId = Id;
        Zone.ZoneType = Type;
        Zone.bAffectsVegetation = true;
        Zone.PolygonPointsCm = {
            FVector(MinX, MinY, 0.0), FVector(MaxX, MinY, 0.0),
            FVector(MaxX, MaxY, 0.0), FVector(MinX, MaxY, 0.0)};
        return Zone;
    }

    FDGHoleDefinition MakeCompleteSession13Hole(
        const FDiscGolfHoleBlockoutDefinition& RuntimeHole)
    {
        FDGHoleDefinition Hole;
        Hole.HoleId = TEXT("PineRidge_Validated_Hole_01");
        Hole.HoleNumber = RuntimeHole.HoleNumber;
        Hole.Par = RuntimeHole.Par;
        Hole.PublishedDistanceM = FVector::Dist2D(
            RuntimeHole.TeeLocationCm, RuntimeHole.BasketLocationCm) / 100.0f;
        Hole.TeeTransform.SetLocation(RuntimeHole.TeeLocationCm);
        Hole.BasketTransform.SetLocation(RuntimeHole.BasketLocationCm);
        Hole.DropZoneTransforms.Add(FTransform(FVector(5000.0, 5000.0, 0.0)));

        FDGMandoDefinition& Mando = Hole.Mandos.AddDefaulted_GetRef();
        Mando.MandoId = TEXT("Mando_01");
        Mando.GatePointACm = FVector(4000.0, -300.0, 0.0);
        Mando.GatePointBCm = FVector(4000.0, 300.0, 0.0);
        Mando.RequiredPassDirection = FVector::ForwardVector;
        Mando.DropZoneIndex = 0;

        // All ten source types are present without ambiguous forbidden overlaps.
        Hole.Zones = {
            Rectangle(TEXT("01_Tee"), EDGCourseZoneType::TeeSafety,
                -1000.0, -1000.0, 1000.0, 1000.0),
            Rectangle(TEXT("02_FairwayPrimary"), EDGCourseZoneType::FairwayPrimary,
                -500.0, -700.0, 11500.0, 1500.0),
            Rectangle(TEXT("03_FairwaySecondary"), EDGCourseZoneType::FairwaySecondary,
                1500.0, 1000.0, 4500.0, 1500.0),
            Rectangle(TEXT("04_Rough"), EDGCourseZoneType::Rough,
                -1000.0, 1800.0, 3500.0, 2400.0),
            Rectangle(TEXT("05_DeepRough"), EDGCourseZoneType::DeepRough,
                4000.0, 1800.0, 8500.0, 2400.0),
            Rectangle(TEXT("06_Green"), EDGCourseZoneType::Green,
                10000.0, -200.0, 12000.0, 1800.0),
            Rectangle(TEXT("07_OB"), EDGCourseZoneType::OutOfBounds,
                0.0, 3000.0, 2000.0, 3600.0),
            Rectangle(TEXT("08_Water"), EDGCourseZoneType::WaterHazard,
                2500.0, 3000.0, 4000.0, 3600.0),
            Rectangle(TEXT("09_Spectator"), EDGCourseZoneType::Spectator,
                6000.0, 3000.0, 8000.0, 3600.0),
            Rectangle(TEXT("10_NoSpawn"), EDGCourseZoneType::NoSpawn,
                10000.0, 3000.0, 12000.0, 3600.0)};
        return Hole;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13ValidatedOneHoleWorkflowTest,
    "DiscGolfTour.Session13.CourseAuthoring.ValidatedOneHoleWorkflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13ValidatedOneHoleWorkflowTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfHoleBlockoutDefinition RuntimeHole =
        DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    UDiscGolfCourseDefinition* Target =
        NewObject<UDiscGolfCourseDefinition>(GetTransientPackage());
    const TArray<FDGHoleDefinition> SourceHoles = {MakeCompleteSession13Hole(RuntimeHole)};
    const int32 SourceZoneCount = SourceHoles[0].Zones.Num();

    TestEqual(TEXT("Fixture is bound to runtime Pine Ridge Hole 1"), RuntimeHole.HoleNumber, 1);
    TestEqual(TEXT("Runtime fixture preserves authored strategic collision"),
        RuntimeHole.CollisionFixtures.Num(), 3);
    TestEqual(TEXT("Runtime fixture preserves authored strategy trees"),
        RuntimeHole.Trees.Num(), 12);
    TestEqual(TEXT("Runtime fixture preserves complete blockout surfaces"),
        RuntimeHole.Surfaces.Num(), 11);

    TArray<FDGCourseValidationIssue> Issues;
    FString Error;
    TestTrue(TEXT("Complete one-hole authoring DTO updates atomically"),
        DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
            Target, TEXT("session13_test_course"), FText::FromString(TEXT("Session 13 Test Course")),
            TEXT("dg_generic"), TEXT("temperate_forest"), SourceHoles, Issues, Error));
    TestFalse(TEXT("Strict validation reports no errors"),
        UDiscGolfCourseAuthoringValidation::HasErrors(Issues));
    TestEqual(TEXT("One validated hole reaches the supplementary asset"), Target->Holes.Num(), 1);
    TestEqual(TEXT("All ten gameplay-zone types survive export"), Target->Holes[0].Zones.Num(), 10);

    TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry> Plan;
    TestTrue(TEXT("Validated gameplay zones produce a deterministic PCG value plan"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Target->Holes[0].Zones, Plan, Error));
    TestEqual(TEXT("PCG seam maps every gameplay-zone type"), Plan.Num(), 10);
    TestEqual(TEXT("Plan is stable-ID ordered"), Plan[0].StableZoneId, FName(TEXT("01_Tee")));
    int32 HardExclusions = 0;
    for (const FDiscGolfCourseAuthoringPcgZonePlanEntry& Entry : Plan)
    {
        HardExclusions += Entry.bHardExclusion ? 1 : 0;
    }
    TestEqual(TEXT("Five safety footprints remain quality-invariant hard exclusions"),
        HardExclusions, 5);

    TestEqual(TEXT("Exporter and PCG adapter do not mutate gameplay input"),
        SourceHoles[0].Zones.Num(), SourceZoneCount);
    TestEqual(TEXT("Mando keeps its explicit drop-zone reference"),
        SourceHoles[0].Mandos[0].DropZoneIndex, 0);
    return true;
}

#endif
