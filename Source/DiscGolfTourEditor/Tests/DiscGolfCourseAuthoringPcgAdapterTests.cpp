#if WITH_DEV_AUTOMATION_TESTS

#include "DiscGolfCourseAuthoringPcgAdapter.h"
#include "Misc/AutomationTest.h"

#include <limits>

namespace
{
    FDGCourseZoneDefinition MakeZone(FName Id, EDGCourseZoneType Type, float Offset = 0.0f)
    {
        FDGCourseZoneDefinition Zone;
        Zone.ZoneId = Id;
        Zone.ZoneType = Type;
        Zone.PolygonPointsCm = {
            FVector(Offset, 0.0f, 0.0f),
            FVector(Offset + 300.0f, 0.0f, 0.0f),
            FVector(Offset + 150.0f, 300.0f, 0.0f)};
        return Zone;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringPcgMappingTest,
    "DiscGolfTour.Session13.CourseAuthoring.PcgAdapter.ExactMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringPcgMappingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    struct FExpected
    {
        EDGCourseZoneType Source;
        EDiscGolfEnvironmentZoneType Target;
        int32 Priority;
        bool bHardExclusion;
        bool bExcludeTreesFromFlightLine;
    };
    const FExpected Expected[] = {
        {EDGCourseZoneType::TeeSafety, EDiscGolfEnvironmentZoneType::Tee, 100, true, false},
        {EDGCourseZoneType::FairwayPrimary, EDiscGolfEnvironmentZoneType::Fairway, 50, false, true},
        {EDGCourseZoneType::FairwaySecondary, EDiscGolfEnvironmentZoneType::Fairway, 45, false, true},
        {EDGCourseZoneType::Rough, EDiscGolfEnvironmentZoneType::SemiRough, 30, false, false},
        {EDGCourseZoneType::DeepRough, EDiscGolfEnvironmentZoneType::DeepRough, 10, false, false},
        {EDGCourseZoneType::Green, EDiscGolfEnvironmentZoneType::Green, 100, true, false},
        {EDGCourseZoneType::OutOfBounds, EDiscGolfEnvironmentZoneType::OBNatural, 20, false, false},
        {EDGCourseZoneType::WaterHazard, EDiscGolfEnvironmentZoneType::OBNatural, 110, true, false},
        {EDGCourseZoneType::Spectator, EDiscGolfEnvironmentZoneType::OBNatural, 120, true, false},
        {EDGCourseZoneType::NoSpawn, EDiscGolfEnvironmentZoneType::OBNatural, 130, true, false}};
    TestEqual(TEXT("The contract explicitly covers ten source zone types"),
        static_cast<int32>(UE_ARRAY_COUNT(Expected)), 10);

    for (const FExpected& Value : Expected)
    {
        FDiscGolfCourseAuthoringPcgZonePolicy Policy;
        FString Error;
        TestTrue(TEXT("Every declared source type has an explicit policy"),
            DiscGolfCourseAuthoringPcgAdapter::TryGetZonePolicy(Value.Source, Policy, Error));
        TestEqual(TEXT("Environment mapping is exact"), Policy.EnvironmentZoneType, Value.Target);
        TestEqual(TEXT("Priority mapping is exact"), Policy.Priority, Value.Priority);
        TestEqual(TEXT("Hard-clearance mapping is exact"), Policy.bHardExclusion, Value.bHardExclusion);
        TestEqual(TEXT("Tree flight-line mapping is exact"),
            Policy.bExcludeTreesFromFlightLine, Value.bExcludeTreesFromFlightLine);
    }

    FDiscGolfCourseAuthoringPcgZonePolicy RejectedPolicy;
    FString Error;
    TestFalse(TEXT("Unknown enum values fail closed"),
        DiscGolfCourseAuthoringPcgAdapter::TryGetZonePolicy(
            static_cast<EDGCourseZoneType>(255), RejectedPolicy, Error));
    TestTrue(TEXT("Unknown type failure is actionable"), Error.Contains(TEXT("unsupported")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringPcgDeterminismTest,
    "DiscGolfTour.Session13.CourseAuthoring.PcgAdapter.DeterminismAndAuthority",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringPcgDeterminismTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDGCourseZoneDefinition> Zones;
    Zones.Add(MakeZone(TEXT("Zone_Z"), EDGCourseZoneType::Green, 300.0f));
    Zones.Add(MakeZone(TEXT("Zone_A"), EDGCourseZoneType::FairwayPrimary, 0.0f));
    Zones.Add(MakeZone(TEXT("Zone_M"), EDGCourseZoneType::Rough, 150.0f));
    Zones[1].PenaltyStrokes = 7;
    Zones[2].bAffectsVegetation = false;
    const TArray<FDGCourseZoneDefinition> OriginalZones = Zones;

    TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry> Plan;
    FString Error;
    TestTrue(TEXT("A complete value plan builds"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Zones, Plan, Error));
    TestEqual(TEXT("Every gameplay zone remains explicitly represented"), Plan.Num(), 3);
    TestEqual(TEXT("Plan order is stable-ID based"), Plan[0].StableZoneId, FName(TEXT("Zone_A")));
    TestEqual(TEXT("Plan order is stable-ID based"), Plan[1].StableZoneId, FName(TEXT("Zone_M")));
    TestEqual(TEXT("Plan order is stable-ID based"), Plan[2].StableZoneId, FName(TEXT("Zone_Z")));
    TestFalse(TEXT("A non-vegetation gameplay zone remains inert for decorative PCG"),
        Plan[1].bApplyToDecorativePcg);
    TestTrue(TEXT("Green clearance is a quality-independent hard exclusion"),
        Plan[2].bHardExclusion);
    TestTrue(TEXT("Primary fairway excludes tree scattering from the flight line"),
        Plan[0].bExcludeTreesFromFlightLine);
    TestFalse(TEXT("Fairway tree protection does not become an all-decoration exclusion"),
        Plan[0].bHardExclusion);
    TestEqual(TEXT("Penalty remains owned by gameplay authoring"), Zones[1].PenaltyStrokes, 7);
    TestEqual(TEXT("Source zone order is not mutated"), Zones[0].ZoneId, OriginalZones[0].ZoneId);
    TestEqual(TEXT("Source geometry is not mutated"),
        Zones[0].PolygonPointsCm[0], OriginalZones[0].PolygonPointsCm[0]);

    Algo::Reverse(Zones);
    TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry> ReversedPlan;
    TestTrue(TEXT("Selection/array order cannot change plan identity"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Zones, ReversedPlan, Error));
    for (int32 Index = 0; Index < Plan.Num(); ++Index)
    {
        TestEqual(TEXT("Stable order survives reversed input"),
            ReversedPlan[Index].StableZoneId, Plan[Index].StableZoneId);
        TestEqual(TEXT("Stable geometry survives reversed input"),
            ReversedPlan[Index].PolygonPointsCm, Plan[Index].PolygonPointsCm);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringPcgFailClosedTest,
    "DiscGolfTour.Session13.CourseAuthoring.PcgAdapter.FailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringPcgFailClosedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Error;
    TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry> Plan;

    TArray<FDGCourseZoneDefinition> Duplicate = {
        MakeZone(TEXT("Same"), EDGCourseZoneType::Rough),
        MakeZone(TEXT("Same"), EDGCourseZoneType::FairwayPrimary, 200.0f)};
    Plan.AddDefaulted();
    TestFalse(TEXT("Duplicate stable IDs are rejected"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Duplicate, Plan, Error));
    TestEqual(TEXT("Duplicate failure returns zero output"), Plan.Num(), 0);

    TArray<FDGCourseZoneDefinition> NonFinite = {
        MakeZone(TEXT("Nan"), EDGCourseZoneType::Rough)};
    NonFinite[0].PolygonPointsCm[1].X = std::numeric_limits<float>::quiet_NaN();
    Plan.AddDefaulted();
    TestFalse(TEXT("Non-finite source geometry is rejected"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(NonFinite, Plan, Error));
    TestEqual(TEXT("Non-finite failure returns zero output"), Plan.Num(), 0);

    TArray<FDGCourseZoneDefinition> Unknown = {
        MakeZone(TEXT("Unknown"), static_cast<EDGCourseZoneType>(254))};
    Plan.AddDefaulted();
    TestFalse(TEXT("Unknown source configuration is rejected"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Unknown, Plan, Error));
    TestEqual(TEXT("Unknown failure returns zero output"), Plan.Num(), 0);

    TArray<FDGCourseZoneDefinition> NegativePenalty = {
        MakeZone(TEXT("NegativePenalty"), EDGCourseZoneType::OutOfBounds)};
    NegativePenalty[0].PenaltyStrokes = -1;
    Plan.AddDefaulted();
    TestFalse(TEXT("Negative gameplay penalties cannot bypass the decorative seam"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(NegativePenalty, Plan, Error));
    TestEqual(TEXT("Negative penalty failure returns zero output"), Plan.Num(), 0);
    TestTrue(TEXT("Negative penalty failure is actionable"),
        Error.Contains(TEXT("negative gameplay penalty")));

    TArray<FDGCourseZoneDefinition> Ambiguous = {
        MakeZone(TEXT("UnsafeGreen"), EDGCourseZoneType::Green)};
    Ambiguous[0].bAffectsVegetation = false;
    Plan.AddDefaulted();
    TestFalse(TEXT("Hard clearance cannot ambiguously opt out of vegetation handling"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Ambiguous, Plan, Error));
    TestEqual(TEXT("Ambiguous failure returns zero output"), Plan.Num(), 0);

    TArray<FDGCourseZoneDefinition> Degenerate = {
        MakeZone(TEXT("Flat"), EDGCourseZoneType::Rough)};
    Degenerate[0].PolygonPointsCm = {
        FVector(0.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f)};
    Plan.AddDefaulted();
    TestFalse(TEXT("Ambiguous zero-area polygons are rejected"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(Degenerate, Plan, Error));
    TestEqual(TEXT("Degenerate failure returns zero output"), Plan.Num(), 0);

    TArray<FDGCourseZoneDefinition> BowTie = {
        MakeZone(TEXT("BowTie"), EDGCourseZoneType::FairwayPrimary)};
    BowTie[0].PolygonPointsCm = {
        FVector(0.0f, 0.0f, 0.0f), FVector(400.0f, 400.0f, 0.0f),
        FVector(0.0f, 400.0f, 0.0f), FVector(400.0f, 0.0f, 0.0f)};
    Plan.AddDefaulted();
    TestFalse(TEXT("Bow-tie polygons are rejected before they reach PCG"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(BowTie, Plan, Error));
    TestEqual(TEXT("Bow-tie failure returns zero output"), Plan.Num(), 0);
    TestTrue(TEXT("Bow-tie failure identifies the crossing"),
        Error.Contains(TEXT("self-intersects")));

    TArray<FDGCourseZoneDefinition> TooSmall = {
        MakeZone(TEXT("TooSmall"), EDGCourseZoneType::Rough)};
    TooSmall[0].PolygonPointsCm = {
        FVector(0.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 0.0f),
        FVector(50.0f, 100.0f, 0.0f)};
    Plan.AddDefaulted();
    TestFalse(TEXT("Polygons below 10,000 square centimeters are rejected"),
        DiscGolfCourseAuthoringPcgAdapter::BuildPlan(TooSmall, Plan, Error));
    TestEqual(TEXT("Minimum-area failure returns zero output"), Plan.Num(), 0);
    TestTrue(TEXT("Minimum-area failure reports the bounded threshold"),
        Error.Contains(TEXT("10000")));
    return true;
}

#endif
