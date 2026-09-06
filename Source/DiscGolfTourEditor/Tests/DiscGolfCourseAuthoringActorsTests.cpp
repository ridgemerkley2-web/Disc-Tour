#if WITH_DEV_AUTOMATION_TESTS

#include "DiscGolfCourseAuthoringActors.h"
#include "Misc/AutomationTest.h"

#include <limits>

namespace
{
    FTransform WorldTransformAt(const FTransform& Origin, const FVector& CourseRelativeLocation)
    {
        return FTransform(FQuat::Identity, Origin.TransformPosition(CourseRelativeLocation));
    }

    FDiscGolfHoleAuthoringSnapshot MinimalSnapshot()
    {
        FDiscGolfHoleAuthoringSnapshot Snapshot;
        Snapshot.HoleId = TEXT("Hole_01");
        Snapshot.HoleNumber = 1;
        Snapshot.Par = 3;
        Snapshot.PublishedDistanceM = 100.0f;
        Snapshot.ElevationChangeM = 2.0f;
        Snapshot.Tee = {TEXT("Tee"), FTransform(FVector::ZeroVector)};
        Snapshot.Basket = {TEXT("Basket"), FTransform(FVector(10000.0f, 0.0f, 0.0f))};
        return Snapshot;
    }

    FDGHoleDefinition SentinelDefinition()
    {
        FDGHoleDefinition Sentinel;
        Sentinel.HoleId = TEXT("Sentinel");
        Sentinel.HoleNumber = 99;
        Sentinel.Par = 6;
        Sentinel.PublishedDistanceM = 777.0f;
        Sentinel.ElevationChangeM = -11.0f;
        Sentinel.DropZoneTransforms.Add(FTransform(FVector(1.0f, 2.0f, 3.0f)));
        return Sentinel;
    }

    bool IsSentinelDefinition(const FDGHoleDefinition& Definition)
    {
        return Definition.HoleId == TEXT("Sentinel")
            && Definition.HoleNumber == 99
            && Definition.Par == 6
            && Definition.PublishedDistanceM == 777.0f
            && Definition.ElevationChangeM == -11.0f
            && Definition.DropZoneTransforms.Num() == 1
            && Definition.DropZoneTransforms[0].GetLocation().Equals(FVector(1.0f, 2.0f, 3.0f));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringConversionTest,
    "DiscGolfTour.Session13.CourseAuthoring.ActorConversion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringConversionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FTransform Origin(FRotator(0.0f, 90.0f, 0.0f), FVector(10000.0f, -3000.0f, 200.0f));
    FDiscGolfHoleAuthoringSnapshot Snapshot;
    Snapshot.HoleId = TEXT("Hole_07");
    Snapshot.HoleNumber = 7;
    Snapshot.Par = 4;
    Snapshot.PublishedDistanceM = 142.5f;
    Snapshot.ElevationChangeM = -3.0f;
    Snapshot.CourseOriginWorldTransform = Origin;
    Snapshot.Tee = {TEXT("Tee"), WorldTransformAt(Origin, FVector(0.0f, 0.0f, 10.0f))};
    Snapshot.Basket = {TEXT("Basket"), WorldTransformAt(Origin, FVector(14000.0f, 1000.0f, -290.0f))};
    // Reverse input order verifies stable IDs, rather than array/selection order, own identity.
    Snapshot.DropZones.Add({TEXT("Drop_B"), WorldTransformAt(Origin, FVector(9000.0f, 1500.0f, 0.0f))});
    Snapshot.DropZones.Add({TEXT("Drop_A"), WorldTransformAt(Origin, FVector(6000.0f, -1000.0f, 0.0f))});

    FDiscGolfAuthoringMandoRecord Mando;
    Mando.StableId = TEXT("Mando_A");
    Mando.GatePointAWorldCm = Origin.TransformPosition(FVector(5000.0f, -400.0f, 0.0f));
    Mando.GatePointBWorldCm = Origin.TransformPosition(FVector(5000.0f, 400.0f, 0.0f));
    Mando.RequiredPassDirectionWorld = Origin.TransformVectorNoScale(FVector::ForwardVector);
    Mando.DropZoneId = TEXT("Drop_B");
    Snapshot.Mandos.Add(Mando);

    FDiscGolfAuthoringZoneRecord Zone;
    Zone.StableId = TEXT("OB_Left");
    Zone.ZoneType = EDGCourseZoneType::OutOfBounds;
    Zone.PenaltyStrokes = 1;
    Zone.PolygonPointsWorldCm = {
        Origin.TransformPosition(FVector(-100.0f, -2000.0f, 0.0f)),
        Origin.TransformPosition(FVector(15000.0f, -2000.0f, 0.0f)),
        Origin.TransformPosition(FVector(15000.0f, -1500.0f, 0.0f))};
    Snapshot.Zones.Add(Zone);

    FDGHoleDefinition Definition;
    FString Error;
    TestTrue(TEXT("Complete value snapshot converts"),
        DiscGolfCourseAuthoring::ToHoleDefinition(Snapshot, Definition, Error));
    TestEqual(TEXT("Stable hole identity is retained"), Definition.HoleId, FName(TEXT("Hole_07")));
    TestEqual(TEXT("Tee is course-origin relative"), Definition.TeeTransform.GetLocation(), FVector(0.0f, 0.0f, 10.0f));
    TestEqual(TEXT("Basket is course-origin relative"), Definition.BasketTransform.GetLocation(), FVector(14000.0f, 1000.0f, -290.0f));
    TestEqual(TEXT("Drop zones convert once"), Definition.DropZoneTransforms.Num(), 2);
    TestEqual(TEXT("Drop zones sort by stable ID"), Definition.DropZoneTransforms[0].GetLocation(), FVector(6000.0f, -1000.0f, 0.0f));
    TestEqual(TEXT("Mando drop-zone index follows stable sorting"), Definition.Mandos[0].DropZoneIndex, 1);
    TestEqual(TEXT("Mando pass direction rotates into course space"), Definition.Mandos[0].RequiredPassDirection, FVector::ForwardVector);
    TestEqual(TEXT("Zone points are course-origin relative"), Definition.Zones[0].PolygonPointsCm[0], FVector(-100.0f, -2000.0f, 0.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringIdentityBoundsTest,
    "DiscGolfTour.Session13.CourseAuthoring.IdentityAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringIdentityBoundsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfHoleAuthoringSnapshot Snapshot;
    Snapshot.HoleId = TEXT("BoundsHole");
    Snapshot.Tee = {TEXT("Tee"), FTransform(FVector(-50.0f, 0.0f, 10.0f))};
    Snapshot.Basket = {TEXT("Basket"), FTransform(FVector(500.0f, 200.0f, 30.0f))};
    Snapshot.DropZones.Add({TEXT("Drop_A"), FTransform(FVector(100.0f, -300.0f, 20.0f))});
    FDiscGolfAuthoringZoneRecord Zone;
    Zone.StableId = TEXT("Water_A");
    Zone.PolygonPointsWorldCm = {
        FVector(0.0f, -100.0f, -5.0f), FVector(900.0f, -100.0f, -5.0f), FVector(900.0f, 400.0f, -5.0f)};
    Snapshot.Zones.Add(Zone);

    FDGHoleDefinition Definition;
    FString Error;
    TestTrue(TEXT("Bounds snapshot converts"),
        DiscGolfCourseAuthoring::ToHoleDefinition(Snapshot, Definition, Error));
    const FBox Bounds = DiscGolfCourseAuthoring::CalculateCourseRelativeBounds(Definition);
    TestTrue(TEXT("Definition bounds are valid"), Bounds.IsValid != 0);
    TestEqual(TEXT("Bounds minimum includes all typed geometry"), Bounds.Min, FVector(-50.0f, -300.0f, -5.0f));
    TestEqual(TEXT("Bounds maximum includes all typed geometry"), Bounds.Max, FVector(900.0f, 400.0f, 30.0f));

    Snapshot.DropZones.Add({TEXT("Drop_A"), FTransform(FVector(200.0f, 0.0f, 0.0f))});
    TestFalse(TEXT("Duplicate stable identities fail closed"),
        DiscGolfCourseAuthoring::ToHoleDefinition(Snapshot, Definition, Error));
    TestTrue(TEXT("Identity failure names the duplicate"), Error.Contains(TEXT("Drop_A")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringInvalidValueTest,
    "DiscGolfTour.Session13.CourseAuthoring.InvalidValuesFailBeforeMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringInvalidValueTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto ExpectFailureWithoutMutation = [this](
        const TCHAR* Context, const FDiscGolfHoleAuthoringSnapshot& Snapshot)
    {
        FDGHoleDefinition Definition = SentinelDefinition();
        FString Error;
        TestFalse(Context, DiscGolfCourseAuthoring::ToHoleDefinition(
            Snapshot, Definition, Error));
        TestTrue(TEXT("Failed conversion leaves the caller's output intact"),
            IsSentinelDefinition(Definition));
        TestTrue(TEXT("Failed conversion reports an explicit reason"), !Error.IsEmpty());
    };

    FDiscGolfHoleAuthoringSnapshot Invalid = MinimalSnapshot();
    Invalid.PublishedDistanceM = -1.0f;
    ExpectFailureWithoutMutation(TEXT("Negative published distance is rejected"), Invalid);

    Invalid = MinimalSnapshot();
    Invalid.PublishedDistanceM = std::numeric_limits<float>::quiet_NaN();
    ExpectFailureWithoutMutation(TEXT("Non-finite published distance is rejected"), Invalid);

    Invalid = MinimalSnapshot();
    Invalid.ElevationChangeM = std::numeric_limits<float>::infinity();
    ExpectFailureWithoutMutation(TEXT("Non-finite elevation is rejected"), Invalid);

    Invalid = MinimalSnapshot();
    FDiscGolfAuthoringZoneRecord InvalidZone;
    InvalidZone.StableId = TEXT("InvalidZone");
    InvalidZone.ZoneType = static_cast<EDGCourseZoneType>(255);
    InvalidZone.PolygonPointsWorldCm = {
        FVector::ZeroVector, FVector(1000.0f, 0.0f, 0.0f), FVector(0.0f, 1000.0f, 0.0f)};
    Invalid.Zones.Add(InvalidZone);
    ExpectFailureWithoutMutation(TEXT("Unsupported zone type is rejected"), Invalid);

    Invalid = MinimalSnapshot();
    InvalidZone.ZoneType = EDGCourseZoneType::OutOfBounds;
    InvalidZone.PenaltyStrokes = -1;
    Invalid.Zones.Add(InvalidZone);
    ExpectFailureWithoutMutation(TEXT("Negative zone penalty is rejected"), Invalid);

    Invalid = MinimalSnapshot();
    InvalidZone.PenaltyStrokes = 0;
    InvalidZone.bIsClosedLoop = false;
    Invalid.Zones.Add(InvalidZone);
    ExpectFailureWithoutMutation(TEXT("Open gameplay-zone spline is rejected"), Invalid);

    Invalid = MinimalSnapshot();
    FDiscGolfAuthoringMandoRecord InvalidMando;
    InvalidMando.StableId = TEXT("InvalidMando");
    InvalidMando.GatePointAWorldCm = FVector(5000.0f, -500.0f, 0.0f);
    InvalidMando.GatePointBWorldCm = FVector(5000.0f, 500.0f, 0.0f);
    InvalidMando.RequiredPassDirectionWorld = FVector::ForwardVector;
    InvalidMando.MissPenaltyStrokes = -1;
    Invalid.Mandos.Add(InvalidMando);
    ExpectFailureWithoutMutation(TEXT("Negative mando miss penalty is rejected"), Invalid);

    FDiscGolfAuthoringZoneRecord RawInvalidZone = InvalidZone;
    RawInvalidZone.PenaltyStrokes = -1;
    const FDGCourseZoneDefinition RawZone =
        DiscGolfCourseAuthoring::ToCourseZoneDefinition(RawInvalidZone, FTransform::Identity);
    TestEqual(TEXT("Zone helper never silently repairs a negative penalty"),
        RawZone.PenaltyStrokes, -1);
    const FDGMandoDefinition RawMando = DiscGolfCourseAuthoring::ToMandoDefinition(
        InvalidMando, FTransform::Identity, INDEX_NONE);
    TestEqual(TEXT("Mando helper never silently repairs a negative penalty"),
        RawMando.MissPenaltyStrokes, -1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseAuthoringActorClassPolicyTest,
    "DiscGolfTour.Session13.CourseAuthoring.ActorClassesAreEditorOnlyAndTickFree",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseAuthoringActorClassPolicyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    auto VerifyPolicy = [this](const TCHAR* TypeName, const AActor& Actor)
    {
        TestTrue(*FString::Printf(TEXT("%s is editor-only"), TypeName),
            Actor.bIsEditorOnlyActor);
        TestFalse(*FString::Printf(TEXT("%s cannot tick"), TypeName),
            Actor.PrimaryActorTick.bCanEverTick);
        TestFalse(*FString::Printf(TEXT("%s tick starts disabled"), TypeName),
            Actor.PrimaryActorTick.bStartWithTickEnabled);
    };

    VerifyPolicy(TEXT("Hole authoring actor"), *GetDefault<ADiscGolfHoleAuthoringActor>());
    VerifyPolicy(TEXT("Tee authoring actor"), *GetDefault<ADiscGolfTeeAuthoringActor>());
    VerifyPolicy(TEXT("Basket authoring actor"), *GetDefault<ADiscGolfBasketAuthoringActor>());
    VerifyPolicy(TEXT("Drop-zone authoring actor"), *GetDefault<ADiscGolfDropZoneAuthoringActor>());
    VerifyPolicy(TEXT("Mando authoring actor"), *GetDefault<ADiscGolfMandoGateAuthoringActor>());
    VerifyPolicy(TEXT("Gameplay-zone authoring actor"),
        *GetDefault<ADiscGolfGameplayZoneAuthoringActor>());
    return true;
}

#endif
