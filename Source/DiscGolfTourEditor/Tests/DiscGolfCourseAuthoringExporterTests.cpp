#if WITH_DEV_AUTOMATION_TESTS

#include "DiscGolfCourseAuthoringExporter.h"

#include "DGFrameworkCourseDefinition.h"
#include "DiscGolfCourseAuthoringActors.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"

namespace
{
    FDGCourseZoneDefinition ExportRectangle(
        FName Id, EDGCourseZoneType Type, double MinX, double MinY, double MaxX, double MaxY)
    {
        FDGCourseZoneDefinition Zone;
        Zone.ZoneId = Id;
        Zone.ZoneType = Type;
        Zone.PolygonPointsCm = {
            FVector(MinX, MinY, 0.0), FVector(MaxX, MinY, 0.0),
            FVector(MaxX, MaxY, 0.0), FVector(MinX, MaxY, 0.0)};
        return Zone;
    }

    FDGHoleDefinition MakeExportHole(int32 HoleNumber, FName HoleId, double YOffset)
    {
        FDGHoleDefinition Hole;
        Hole.HoleId = HoleId;
        Hole.HoleNumber = HoleNumber;
        Hole.Par = 3;
        Hole.TeeTransform.SetLocation(FVector(0.0, YOffset, 0.0));
        Hole.BasketTransform.SetLocation(FVector(10000.0, YOffset, 0.0));
        Hole.Zones = {
            ExportRectangle(*FString::Printf(TEXT("tee_%d"), HoleNumber),
                EDGCourseZoneType::TeeSafety, -1000, YOffset - 1000, 1000, YOffset + 1000),
            ExportRectangle(*FString::Printf(TEXT("fairway_%d"), HoleNumber),
                EDGCourseZoneType::FairwayPrimary, -500, YOffset - 800, 10500, YOffset + 800),
            ExportRectangle(*FString::Printf(TEXT("green_%d"), HoleNumber),
                EDGCourseZoneType::Green, 9000, YOffset - 1000, 11000, YOffset + 1000),
            ExportRectangle(*FString::Printf(TEXT("gallery_%d"), HoleNumber),
                EDGCourseZoneType::Spectator, 2500, YOffset + 2500, 7500, YOffset + 3200)};
        return Hole;
    }

    UDiscGolfCourseDefinition* MakeSentinelTarget(UObject* Outer = GetTransientPackage())
    {
        UDiscGolfCourseDefinition* Target =
            NewObject<UDiscGolfCourseDefinition>(Outer);
        Target->CourseId = TEXT("sentinel_course");
        Target->DisplayName = FText::FromString(TEXT("Sentinel"));
        Target->BrandId = TEXT("sentinel_brand");
        Target->BiomeId = TEXT("sentinel_biome");
        Target->Holes.Add(MakeExportHole(9, TEXT("sentinel_hole"), 40000.0));
        return Target;
    }

    bool HasExportCode(const TArray<FDGCourseValidationIssue>& Issues, FName Code)
    {
        return Issues.ContainsByPredicate([Code](const FDGCourseValidationIssue& Issue)
        {
            return Issue.Severity == EDGValidationSeverity::Error && Issue.Code == Code;
        });
    }

    bool SentinelIsUnchanged(const UDiscGolfCourseDefinition& Target)
    {
        return Target.CourseId == TEXT("sentinel_course")
            && Target.DisplayName.ToString() == TEXT("Sentinel")
            && Target.BrandId == TEXT("sentinel_brand")
            && Target.BiomeId == TEXT("sentinel_biome")
            && Target.Holes.Num() == 1
            && Target.Holes[0].HoleId == TEXT("sentinel_hole")
            && Target.Holes[0].HoleNumber == 9;
    }

    ADiscGolfGameplayZoneAuthoringActor* SpawnExportZone(
        UWorld& World,
        const FVector& CourseOriginWorld,
        FName Id,
        EDGCourseZoneType Type,
        double MinX,
        double MinY,
        double MaxX,
        double MaxY)
    {
        ADiscGolfGameplayZoneAuthoringActor* Zone =
            World.SpawnActor<ADiscGolfGameplayZoneAuthoringActor>();
        Zone->SetActorLocation(CourseOriginWorld);
        Zone->StableId = Id;
        Zone->ZoneType = Type;
        Zone->ZoneSpline->ClearSplinePoints(false);
        Zone->ZoneSpline->AddSplinePoint(FVector(MinX, MinY, 0.0),
            ESplineCoordinateSpace::Local, false);
        Zone->ZoneSpline->AddSplinePoint(FVector(MaxX, MinY, 0.0),
            ESplineCoordinateSpace::Local, false);
        Zone->ZoneSpline->AddSplinePoint(FVector(MaxX, MaxY, 0.0),
            ESplineCoordinateSpace::Local, false);
        Zone->ZoneSpline->AddSplinePoint(FVector(MinX, MaxY, 0.0),
            ESplineCoordinateSpace::Local, false);
        Zone->ZoneSpline->SetClosedLoop(true, false);
        Zone->ZoneSpline->UpdateSpline();
        return Zone;
    }

    ADiscGolfHoleAuthoringActor* SpawnExportRoot(
        UWorld& World, int32 HoleNumber, FName HoleId, const FVector& CourseOriginWorld)
    {
        ADiscGolfHoleAuthoringActor* Root = World.SpawnActor<ADiscGolfHoleAuthoringActor>();
        Root->SetActorLocation(CourseOriginWorld);
        Root->HoleNumber = HoleNumber;
        Root->HoleId = HoleId;
        Root->Tee = World.SpawnActor<ADiscGolfTeeAuthoringActor>();
        Root->Tee->SetActorLocation(CourseOriginWorld);
        Root->Basket = World.SpawnActor<ADiscGolfBasketAuthoringActor>();
        Root->Basket->SetActorLocation(CourseOriginWorld + FVector(10000.0, 0.0, 0.0));
        Root->Zones = {
            SpawnExportZone(World, CourseOriginWorld,
                *FString::Printf(TEXT("tee_%d"), HoleNumber),
                EDGCourseZoneType::TeeSafety, -1000, -1000, 1000, 1000),
            SpawnExportZone(World, CourseOriginWorld,
                *FString::Printf(TEXT("fairway_%d"), HoleNumber),
                EDGCourseZoneType::FairwayPrimary, -500, -800, 10500, 800),
            SpawnExportZone(World, CourseOriginWorld,
                *FString::Printf(TEXT("green_%d"), HoleNumber),
                EDGCourseZoneType::Green, 9000, -1000, 11000, 1000),
            SpawnExportZone(World, CourseOriginWorld,
                *FString::Printf(TEXT("gallery_%d"), HoleNumber),
                EDGCourseZoneType::Spectator, 2500, 2500, 7500, 3200)};
        return Root;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseExporterAtomicUpdateTest,
    "DiscGolfTour.Session13.CourseAuthoring.Exporter.AtomicSortedUpdate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseExporterAtomicUpdateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Target = MakeSentinelTarget();
    TArray<FDGHoleDefinition> ReverseAuthoringOrder = {
        MakeExportHole(2, TEXT("hole_02"), 20000.0),
        MakeExportHole(1, TEXT("hole_01"), 0.0)};
    TArray<FDGCourseValidationIssue> Issues;
    FString Error;

    TestTrue(TEXT("Strictly valid candidate commits"),
        DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
            Target, TEXT("pine_ridge_authoring"), FText::FromString(TEXT("Pine Ridge Authoring")),
            TEXT("dg_generic"), TEXT("temperate_forest"),
            ReverseAuthoringOrder, Issues, Error));
    TestFalse(TEXT("Successful export has no error"), !Error.IsEmpty());
    TestEqual(TEXT("Metadata commits after validation"), Target->CourseId,
        FName(TEXT("pine_ridge_authoring")));
    TestEqual(TEXT("All holes commit together"), Target->Holes.Num(), 2);
    TestEqual(TEXT("Hole roots/definitions sort by hole number"),
        Target->Holes[0].HoleId, FName(TEXT("hole_01")));
    TestEqual(TEXT("Second stable hole follows deterministically"),
        Target->Holes[1].HoleId, FName(TEXT("hole_02")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseExporterActorRootTest,
    "DiscGolfTour.Session13.CourseAuthoring.Exporter.ActorRootsSortAndConvert",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseExporterActorRootTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), TEXT("CourseExporterActorWorld"));
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Isolated authoring world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    ADiscGolfHoleAuthoringActor* Hole2 = SpawnExportRoot(
        *World, 2, TEXT("hole_02"), FVector(50000.0, 20000.0, 100.0));
    ADiscGolfHoleAuthoringActor* Hole1 = SpawnExportRoot(
        *World, 1, TEXT("hole_01"), FVector(-30000.0, -10000.0, 50.0));
    UDiscGolfCourseDefinition* Target = MakeSentinelTarget();
    TArray<FDGCourseValidationIssue> Issues;
    FString Error;

    TestTrue(TEXT("Placed roots convert through the fail-closed actor bridge"),
        UDiscGolfCourseAuthoringExporter::UpdateCourseDefinition(
            Target, TEXT("placed_roots"), FText::FromString(TEXT("Placed Roots")),
            TEXT("dg_generic"), TEXT("temperate_forest"),
            {Hole2, Hole1}, Issues, Error));
    TestEqual(TEXT("Reversed root selection order is inert"),
        Target->Holes[0].HoleId, FName(TEXT("hole_01")));
    TestEqual(TEXT("Each root exports course-origin-relative tee data"),
        Target->Holes[0].TeeTransform.GetLocation(), FVector::ZeroVector);
    TestEqual(TEXT("Each root exports course-origin-relative basket data"),
        Target->Holes[1].BasketTransform.GetLocation(), FVector(10000.0, 0.0, 0.0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseExporterDuplicateIdentityTest,
    "DiscGolfTour.Session13.CourseAuthoring.Exporter.DuplicateIdentityFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseExporterDuplicateIdentityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Target = MakeSentinelTarget();
    TArray<FDGHoleDefinition> Duplicates = {
        MakeExportHole(1, TEXT("duplicate_hole"), 0.0),
        MakeExportHole(1, TEXT("duplicate_hole"), 20000.0)};
    TArray<FDGCourseValidationIssue> Issues;
    FString Error;

    TestFalse(TEXT("Duplicate authored identity is rejected before commit"),
        DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
            Target, TEXT("replacement"), FText::FromString(TEXT("Replacement")),
            TEXT("new_brand"), TEXT("new_biome"), Duplicates, Issues, Error));
    TestTrue(TEXT("Duplicate number is explicit"),
        HasExportCode(Issues, TEXT("HOLE_NUMBER_DUPLICATE")));
    TestTrue(TEXT("Duplicate ID is explicit"),
        HasExportCode(Issues, TEXT("HOLE_ID_DUPLICATE")));
    TestTrue(TEXT("Duplicate failure does not mutate any target field"),
        SentinelIsUnchanged(*Target));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseExporterStrictFailureTest,
    "DiscGolfTour.Session13.CourseAuthoring.Exporter.StrictValidationFailsClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseExporterStrictFailureTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Target = MakeSentinelTarget();
    FDGHoleDefinition Invalid = MakeExportHole(1, TEXT("invalid_hole"), 0.0);
    Invalid.Zones.RemoveAll([](const FDGCourseZoneDefinition& Zone)
    {
        return Zone.ZoneType == EDGCourseZoneType::TeeSafety;
    });
    TArray<FDGCourseValidationIssue> Issues;
    FString Error;

    TestFalse(TEXT("Strict geometry error prevents commit"),
        DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
            Target, TEXT("replacement"), FText::FromString(TEXT("Replacement")),
            TEXT("new_brand"), TEXT("new_biome"), {Invalid}, Issues, Error));
    TestTrue(TEXT("Strict validator issue is returned"),
        HasExportCode(Issues, TEXT("TEE_SAFETY_ZONE_MISSING")));
    TestTrue(TEXT("Validator failure does not mutate any target field"),
        SentinelIsUnchanged(*Target));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseExporterInvalidDtoDirtyStateTest,
    "DiscGolfTour.Session13.CourseAuthoring.Exporter.InvalidDtoKeepsPackageClean",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseExporterInvalidDtoDirtyStateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FString PackageName = FString::Printf(TEXT("/Temp/DGCourseExporter_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    UPackage* TargetPackage = CreatePackage(*PackageName);
    UDiscGolfCourseDefinition* Target = MakeSentinelTarget(TargetPackage);
    TargetPackage->SetDirtyFlag(false);

    FDGHoleDefinition Invalid = MakeExportHole(1, TEXT("invalid_values"), 0.0);
    Invalid.PublishedDistanceM = -25.0f;
    Invalid.Zones[0].PenaltyStrokes = -1;
    TArray<FDGCourseValidationIssue> Issues;
    FString Error;

    TestFalse(TEXT("Invalid direct DTO values fail strict validation before commit"),
        DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
            Target, TEXT("replacement"), FText::FromString(TEXT("Replacement")),
            TEXT("new_brand"), TEXT("new_biome"), {Invalid}, Issues, Error));
    TestTrue(TEXT("Negative distance validation reaches the exporter caller"),
        HasExportCode(Issues, TEXT("HOLE_PUBLISHED_DISTANCE_NEGATIVE")));
    TestTrue(TEXT("Negative zone penalty validation reaches the exporter caller"),
        HasExportCode(Issues, TEXT("ZONE_PENALTY_INVALID")));
    TestTrue(TEXT("Invalid DTO export does not mutate any target field"),
        SentinelIsUnchanged(*Target));
    TestFalse(TEXT("Invalid DTO export does not dirty the target package"),
        TargetPackage->IsDirty());
    return true;
}

#endif
