#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "DGFrameworkCourseDefinition.h"
#include "DiscGolfCourseAuthoringValidation.h"
#include "UObject/Package.h"

#include <limits>

namespace
{
    FDGCourseZoneDefinition Rectangle(
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

    UDiscGolfCourseDefinition* MakeValidCourse()
    {
        UDiscGolfCourseDefinition* Course =
            NewObject<UDiscGolfCourseDefinition>(GetTransientPackage());
        Course->CourseId = TEXT("strict_validation_test");

        FDGHoleDefinition& Hole = Course->Holes.AddDefaulted_GetRef();
        Hole.HoleId = TEXT("hole_1");
        Hole.HoleNumber = 1;
        Hole.Par = 3;
        Hole.TeeTransform.SetLocation(FVector::ZeroVector);
        Hole.BasketTransform.SetLocation(FVector(10000.0, 0.0, 0.0));
        Hole.DropZoneTransforms.Add(FTransform(FVector(3000.0, -2500.0, 0.0)));
        Hole.Zones = {
            Rectangle(TEXT("tee"), EDGCourseZoneType::TeeSafety, -1000, -1000, 1000, 1000),
            Rectangle(TEXT("fairway"), EDGCourseZoneType::FairwayPrimary, -500, -800, 10500, 800),
            Rectangle(TEXT("green"), EDGCourseZoneType::Green, 9000, -1000, 11000, 1000),
            Rectangle(TEXT("gallery"), EDGCourseZoneType::Spectator, 2500, 2500, 7500, 3200)};
        return Course;
    }

    bool HasCode(const TArray<FDGCourseValidationIssue>& Issues, FName Code)
    {
        return Issues.ContainsByPredicate([Code](const FDGCourseValidationIssue& Issue)
        {
            return Issue.Code == Code;
        });
    }

    bool HasWarningCode(const TArray<FDGCourseValidationIssue>& Issues, FName Code)
    {
        return Issues.ContainsByPredicate([Code](const FDGCourseValidationIssue& Issue)
        {
            return Issue.Code == Code && Issue.Severity == EDGValidationSeverity::Warning;
        });
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCourseValidTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.ValidCourse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCourseValidTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const UDiscGolfCourseDefinition* Course = MakeValidCourse();
    const TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestFalse(TEXT("Complete geometry has no validation errors"),
        UDiscGolfCourseAuthoringValidation::HasErrors(Issues));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCourseTransformReferenceTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.TransformsAndReferences",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCourseTransformReferenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Course = MakeValidCourse();
    FDGHoleDefinition& Hole = Course->Holes[0];
    Hole.TeeTransform.SetLocation(FVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
    FDGMandoDefinition& Mando = Hole.Mandos.AddDefaulted_GetRef();
    Mando.MandoId = TEXT("mando");
    Mando.GatePointACm = FVector(4000, -200, 0);
    Mando.GatePointBCm = FVector(4000, 200, 0);
    Mando.DropZoneIndex = 4;

    const TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("Non-finite tee is rejected"), HasCode(Issues, TEXT("TEE_TRANSFORM_NON_FINITE")));
    TestTrue(TEXT("Invalid mando drop-zone reference is rejected"),
        HasCode(Issues, TEXT("MANDO_DROPZONE_INVALID")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCoursePolygonTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.PolygonIntegrity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCoursePolygonTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Course = MakeValidCourse();
    FDGCourseZoneDefinition BowTie;
    BowTie.ZoneId = TEXT("bow_tie");
    BowTie.ZoneType = EDGCourseZoneType::Rough;
    BowTie.PolygonPointsCm = {
        FVector(2000, 2000, 0), FVector(3000, 3000, 0),
        FVector(2000, 3000, 0), FVector(3000, 2000, 0)};
    Course->Holes[0].Zones.Add(BowTie);

    FDGCourseZoneDefinition Degenerate;
    Degenerate.ZoneId = TEXT("line");
    Degenerate.ZoneType = EDGCourseZoneType::Rough;
    Degenerate.PolygonPointsCm = {
        FVector(0, 5000, 0), FVector(100, 5000, 0), FVector(200, 5000, 0)};
    Course->Holes[0].Zones.Add(Degenerate);

    const TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("Self-intersection is explicit"),
        HasCode(Issues, TEXT("ZONE_POLYGON_SELF_INTERSECTS")));
    TestTrue(TEXT("Degenerate area is explicit"),
        HasCode(Issues, TEXT("ZONE_POLYGON_DEGENERATE")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCourseConflictObstructionTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.ConflictsAndObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCourseConflictObstructionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Course = MakeValidCourse();
    FDGHoleDefinition& Hole = Course->Holes[0];
    Hole.DropZoneTransforms.Reset();
    Hole.Zones.Add(Rectangle(
        TEXT("ob_block"), EDGCourseZoneType::OutOfBounds, -200, -900, 5000, 900));

    const int32 OriginalZoneCount = Hole.Zones.Num();
    const FVector OriginalTee = Hole.TeeTransform.GetLocation();
    const TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("OB requires an explicit viable drop zone"),
        HasCode(Issues, TEXT("OB_DROPZONE_MISSING")));
    TestTrue(TEXT("Tee/forbidden collision is rejected"),
        HasCode(Issues, TEXT("TEE_FORBIDDEN_ZONE_CONFLICT")));
    TestTrue(TEXT("Excess fairway obstruction is rejected"),
        HasCode(Issues, TEXT("FAIRWAY_OBSTRUCTION_EXCESSIVE")));
    TestEqual(TEXT("Validation never edits zone data"), Hole.Zones.Num(), OriginalZoneCount);
    TestTrue(TEXT("Validation never moves the tee"),
        Hole.TeeTransform.GetLocation().Equals(OriginalTee));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCourseWarningTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.VisibleWarnings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCourseWarningTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Course = MakeValidCourse();
    Course->Holes[0].Zones.RemoveAll([](const FDGCourseZoneDefinition& Zone)
    {
        return Zone.ZoneType == EDGCourseZoneType::Spectator;
    });

    FDGCourseZoneDefinition Detailed;
    Detailed.ZoneId = TEXT("detailed_rough");
    Detailed.ZoneType = EDGCourseZoneType::Rough;
    for (int32 Index = 0; Index < 129; ++Index)
    {
        const double Angle = 2.0 * UE_PI * static_cast<double>(Index) / 129.0;
        Detailed.PolygonPointsCm.Add(FVector(
            5000.0 + FMath::Cos(Angle) * 1000.0,
            5000.0 + FMath::Sin(Angle) * 1000.0, 0.0));
    }
    Course->Holes[0].Zones.Add(Detailed);

    const TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("Missing spectator review is a visible warning"),
        HasWarningCode(Issues, TEXT("SPECTATOR_ZONE_MISSING")));
    TestTrue(TEXT("Dense polygon performance concern is a visible warning"),
        HasWarningCode(Issues, TEXT("ZONE_POINT_PERFORMANCE_WARNING")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCourseBoundTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.DeterministicBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCourseBoundTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto AddRegularPolygon = [](FDGHoleDefinition& Hole, FName Id, int32 PointCount)
    {
        FDGCourseZoneDefinition& Zone = Hole.Zones.AddDefaulted_GetRef();
        Zone.ZoneId = Id;
        Zone.ZoneType = EDGCourseZoneType::Rough;
        for (int32 Index = 0; Index < PointCount; ++Index)
        {
            const double Angle = 2.0 * UE_PI * static_cast<double>(Index) / PointCount;
            Zone.PolygonPointsCm.Add(FVector(
                15000.0 + FMath::Cos(Angle) * 1000.0,
                5000.0 + FMath::Sin(Angle) * 1000.0, 0.0));
        }
    };

    UDiscGolfCourseDefinition* Course = MakeValidCourse();
    AddRegularPolygon(Course->Holes[0], TEXT("exact_polygon_bound"), 256);
    TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestFalse(TEXT("Exactly 256 polygon points remain inside the hard bound"),
        HasCode(Issues, TEXT("ZONE_POINT_BUDGET_EXCEEDED")));

    Course = MakeValidCourse();
    AddRegularPolygon(Course->Holes[0], TEXT("oversized"), 257);
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("A 257-point polygon fails closed above the documented bound"),
        HasCode(Issues, TEXT("ZONE_POINT_BUDGET_EXCEEDED")));

    Course = MakeValidCourse();
    FDGHoleDefinition TemplateHole = Course->Holes[0];
    while (Course->Holes.Num() < 36)
    {
        FDGHoleDefinition& Hole = Course->Holes.Add_GetRef(TemplateHole);
        Hole.HoleNumber = Course->Holes.Num();
        Hole.HoleId = FName(*FString::Printf(TEXT("hole_%d"), Hole.HoleNumber));
    }
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestFalse(TEXT("Exactly 36 holes remain inside the hard bound"),
        HasCode(Issues, TEXT("COURSE_HOLE_BUDGET_EXCEEDED")));
    FDGHoleDefinition& ExtraHole = Course->Holes.Add_GetRef(TemplateHole);
    ExtraHole.HoleNumber = 37;
    ExtraHole.HoleId = TEXT("hole_37");
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("A 37th hole fails closed above the documented bound"),
        HasCode(Issues, TEXT("COURSE_HOLE_BUDGET_EXCEEDED")));

    Course = MakeValidCourse();
    FDGHoleDefinition& ZoneHole = Course->Holes[0];
    while (ZoneHole.Zones.Num() < 128)
    {
        const int32 ZoneNumber = ZoneHole.Zones.Num();
        ZoneHole.Zones.Add(Rectangle(
            FName(*FString::Printf(TEXT("rough_%d"), ZoneNumber)),
            EDGCourseZoneType::Rough,
            20000.0 + ZoneNumber * 2000.0, 5000.0,
            21000.0 + ZoneNumber * 2000.0, 6000.0));
    }
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestFalse(TEXT("Exactly 128 zones remain inside the hard bound"),
        HasCode(Issues, TEXT("ZONE_BUDGET_EXCEEDED")));
    ZoneHole.Zones.Add(Rectangle(TEXT("zone_129"), EDGCourseZoneType::Rough,
        300000.0, 5000.0, 301000.0, 6000.0));
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("A 129th zone fails closed above the documented bound"),
        HasCode(Issues, TEXT("ZONE_BUDGET_EXCEEDED")));

    Course = MakeValidCourse();
    FDGHoleDefinition& DropZoneHole = Course->Holes[0];
    while (DropZoneHole.DropZoneTransforms.Num() < 32)
    {
        DropZoneHole.DropZoneTransforms.Add(FTransform(FVector(
            20000.0 + DropZoneHole.DropZoneTransforms.Num() * 500.0, -5000.0, 0.0)));
    }
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestFalse(TEXT("Exactly 32 drop zones remain inside the hard bound"),
        HasCode(Issues, TEXT("DROPZONE_BUDGET_EXCEEDED")));
    DropZoneHole.DropZoneTransforms.Add(FTransform(FVector(40000.0, -5000.0, 0.0)));
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("A 33rd drop zone fails closed above the documented bound"),
        HasCode(Issues, TEXT("DROPZONE_BUDGET_EXCEEDED")));

    Course = MakeValidCourse();
    FDGHoleDefinition& MandoHole = Course->Holes[0];
    while (MandoHole.Mandos.Num() < 32)
    {
        const int32 MandoNumber = MandoHole.Mandos.Num();
        FDGMandoDefinition& Mando = MandoHole.Mandos.AddDefaulted_GetRef();
        Mando.MandoId = FName(*FString::Printf(TEXT("mando_%d"), MandoNumber));
        Mando.GatePointACm = FVector(2000.0 + MandoNumber * 200.0, -200.0, 0.0);
        Mando.GatePointBCm = FVector(2000.0 + MandoNumber * 200.0, 200.0, 0.0);
    }
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestFalse(TEXT("Exactly 32 mandos remain inside the hard bound"),
        HasCode(Issues, TEXT("MANDO_BUDGET_EXCEEDED")));
    FDGMandoDefinition& ExtraMando = MandoHole.Mandos.AddDefaulted_GetRef();
    ExtraMando.MandoId = TEXT("mando_32");
    ExtraMando.GatePointACm = FVector(9000.0, -200.0, 0.0);
    ExtraMando.GatePointBCm = FVector(9000.0, 200.0, 0.0);
    Issues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);
    TestTrue(TEXT("A 33rd mando fails closed above the documented bound"),
        HasCode(Issues, TEXT("MANDO_BUDGET_EXCEEDED")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfStrictCourseDtoValueTest,
    "DiscGolfTour.CourseAuthoring.StrictValidation.DtoValuesFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfStrictCourseDtoValueTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfCourseDefinition* Course = MakeValidCourse();
    FDGHoleDefinition& Hole = Course->Holes[0];
    Hole.Par = 0;
    Hole.PublishedDistanceM = std::numeric_limits<float>::quiet_NaN();
    Hole.ElevationChangeM = std::numeric_limits<float>::infinity();

    FDGCourseZoneDefinition InvalidZone = Rectangle(
        TEXT("invalid_values"), static_cast<EDGCourseZoneType>(255),
        12000, 3000, 13000, 4000);
    InvalidZone.PenaltyStrokes = -2;
    Hole.Zones.Add(InvalidZone);
    Hole.Zones[2].bAffectsVegetation = false;

    FDGMandoDefinition& Mando = Hole.Mandos.AddDefaulted_GetRef();
    Mando.MandoId = TEXT("invalid_penalty");
    Mando.GatePointACm = FVector(5000, -500, 0);
    Mando.GatePointBCm = FVector(5000, 500, 0);
    Mando.RequiredPassDirection = FVector::ZeroVector;
    Mando.MissPenaltyStrokes = -1;

    const int32 OriginalPenalty = Hole.Zones.Last().PenaltyStrokes;
    const EDGCourseZoneType OriginalType = Hole.Zones.Last().ZoneType;
    const int32 OriginalMandoPenalty = Mando.MissPenaltyStrokes;
    const TArray<FDGCourseValidationIssue> Issues =
        UDiscGolfCourseAuthoringValidation::ValidateCourse(Course);

    TestTrue(TEXT("Non-positive par cannot bypass actor conversion"),
        HasCode(Issues, TEXT("HOLE_PAR_INVALID")));
    TestTrue(TEXT("Non-finite published distance is rejected"),
        HasCode(Issues, TEXT("HOLE_PUBLISHED_DISTANCE_NON_FINITE")));
    TestTrue(TEXT("Non-finite elevation is rejected"),
        HasCode(Issues, TEXT("HOLE_ELEVATION_CHANGE_NON_FINITE")));
    TestTrue(TEXT("Unsupported zone enum is rejected"),
        HasCode(Issues, TEXT("ZONE_TYPE_INVALID")));
    TestTrue(TEXT("Negative zone penalty is rejected"),
        HasCode(Issues, TEXT("ZONE_PENALTY_INVALID")));
    TestTrue(TEXT("Hard clearances cannot opt out of vegetation exclusion"),
        HasCode(Issues, TEXT("HARD_CLEARANCE_VEGETATION_DISABLED")));
    TestTrue(TEXT("Zero mando pass direction is rejected"),
        HasCode(Issues, TEXT("MANDO_DIRECTION_INVALID")));
    TestTrue(TEXT("Negative mando penalty is rejected"),
        HasCode(Issues, TEXT("MANDO_PENALTY_INVALID")));
    TestEqual(TEXT("Strict validation does not repair zone penalty"),
        Hole.Zones.Last().PenaltyStrokes, OriginalPenalty);
    TestEqual(TEXT("Strict validation does not repair zone type"),
        static_cast<uint8>(Hole.Zones.Last().ZoneType), static_cast<uint8>(OriginalType));
    TestEqual(TEXT("Strict validation does not repair mando penalty"),
        Hole.Mandos.Last().MissPenaltyStrokes, OriginalMandoPenalty);
    return true;
}

#endif
