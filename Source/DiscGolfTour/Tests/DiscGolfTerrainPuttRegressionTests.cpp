#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "../DiscGolfCourseDefinition.h"
#include "../DiscGolfCoursePresentationDefinition.h"
#include "../DiscGolfMath.h"
#include "../DiscGolfTerrainPresentationActor.h"
#include "../DiscGolfTerrainPresentationMath.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

#include <limits>

namespace
{
constexpr float NullPathPawnHeightCm = 88.0f;
constexpr float NullPathDiscHeightCm = 35.0f;
constexpr float NullPathForwardOffsetCm = 70.0f;
constexpr float HistoricalTerrainBiasCm = 13.0f;

UWorld* MakeTerrainPuttWorld()
{
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), TEXT("TerrainPuttRegressionWorld"));
    return UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
}

bool LoadWorldPlacedPineRidge(
    FAutomationTestBase& Test,
    TArray<FDiscGolfHoleBlockoutDefinition>& OutDefinitions,
    TArray<int32>& OutTerrainSeeds)
{
    FDiscGolfCourseManifestDefinition Manifest;
    FString Source;
    FString Error;
    if (!DiscGolfCourseDefinition::LoadPineRidgeCourseManifest(
        Manifest, Source, Error))
    {
        Test.AddError(FString::Printf(
            TEXT("Pine Ridge manifest failed to load: %s"), *Error));
        return false;
    }

    OutDefinitions.Reset(Manifest.Holes.Num());
    for (const FDiscGolfCourseManifestHoleEntry& Entry : Manifest.Holes)
    {
        FDiscGolfHoleBlockoutDefinition LocalDefinition;
        if (!DiscGolfCourseDefinition::LoadPineRidgeHole(
            Entry.HoleNumber, LocalDefinition, Source, Error))
        {
            Test.AddError(FString::Printf(
                TEXT("Pine Ridge Hole %d failed to load: %s"),
                Entry.HoleNumber, *Error));
            return false;
        }

        FDiscGolfHoleBlockoutDefinition WorldDefinition;
        if (!DiscGolfCourseDefinition::PlaceHoleInCourse(
            LocalDefinition, Entry, WorldDefinition, Error))
        {
            Test.AddError(FString::Printf(
                TEXT("Pine Ridge Hole %d failed world placement: %s"),
                Entry.HoleNumber, *Error));
            return false;
        }
        OutDefinitions.Add(MoveTemp(WorldDefinition));
    }

    FDiscGolfCoursePresentationDefinition Presentation;
    if (!DiscGolfCoursePresentation::LoadPineRidge(
        Presentation, Source, Error))
    {
        Test.AddError(FString::Printf(
            TEXT("Pine Ridge presentation failed to load: %s"), *Error));
        return false;
    }

    OutTerrainSeeds.Reset(OutDefinitions.Num());
    for (const FDiscGolfHoleBlockoutDefinition& Definition : OutDefinitions)
    {
        const FDiscGolfHoleVisualPlan* Plan = Presentation.Holes.FindByPredicate(
            [&Definition](const FDiscGolfHoleVisualPlan& Candidate)
            {
                return Candidate.HoleNumber == Definition.HoleNumber;
            });
        if (!Plan)
        {
            Test.AddError(FString::Printf(
                TEXT("Pine Ridge Hole %d has no terrain seed"),
                Definition.HoleNumber));
            return false;
        }
        OutTerrainSeeds.Add(Plan->FoliageSeed);
    }
    return true;
}

bool TraceTerrainGround(
    UWorld* World,
    const ADiscGolfTerrainPresentationActor* Terrain,
    const FVector& QueryLocationCm,
    FVector& OutGroundLocationCm)
{
    if (!World || !Terrain) return false;

    FHitResult Hit;
    FCollisionQueryParams Params(FName(TEXT("TerrainPuttRegressionTrace")), true);
    Params.bReturnFaceIndex = true;
    const FVector TraceStart(
        QueryLocationCm.X, QueryLocationCm.Y, QueryLocationCm.Z + 10000.0f);
    const FVector TraceEnd(
        QueryLocationCm.X, QueryLocationCm.Y, QueryLocationCm.Z - 10000.0f);
    if (!World->LineTraceSingleByChannel(
        Hit, TraceStart, TraceEnd, ECC_Visibility, Params)
        || Hit.GetActor() != Terrain
        || !Terrain->IsBaseTerrainCollisionFace(Hit.FaceIndex))
    {
        return false;
    }

    OutGroundLocationCm = Hit.ImpactPoint;
    return true;
}

FBasketContactEvaluation EvaluatePerfectNullPathPutt(
    const FVector& BasketBaseCm,
    const FVector& LieGroundCm,
    float DistanceCm,
    float AdditionalReleaseHeightCm = 0.0f)
{
    const FVector Forward = FVector(
        BasketBaseCm.X - LieGroundCm.X,
        BasketBaseCm.Y - LieGroundCm.Y,
        0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);

    FThrowCommand Command;
    Command.MoldId = TEXT("Touch");
    Command.ShotContext = EDiscShotContext::Circle1Putt;
    Command.Direction = Forward;
    Command.Power01 = DiscGolfMath::RecommendedPuttPower01(DistanceCm / 100.0f);
    Command.HyzerDeg = 0.0f;
    Command.NoseAngleDeg = 1.0f;
    Command.LaunchAngleDeg = DiscGolfMath::RecommendedPuttLaunchAngleDeg(
        Command.ShotContext, DistanceCm / 100.0f);
    Command.TimingError = 0.0f;

    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);
    const FVector VelocityMps = DiscGolfMath::LaunchDirectionFromFlat(
        Release.Direction, Release.EffectiveLaunchAngleDeg)
        * Release.ReleaseSpeedMps;
    const FVector SpawnLocationCm = LieGroundCm
        + Forward * NullPathForwardOffsetCm
        + FVector(0.0f, 0.0f,
            NullPathPawnHeightCm + NullPathDiscHeightCm
                + AdditionalReleaseHeightCm);
    return DiscGolfMath::EvaluateBasketContact(
        SpawnLocationCm - BasketBaseCm, VelocityMps);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfTerrainPathGroundNormalizationTest,
    "DiscGolfTour.Presentation.Terrain.PathGroundAnchorNormalization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfTerrainPathGroundNormalizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FVector EqualGroundStart(0.0f, 0.0f, 10.0f);
    const FVector EqualGroundEnd(1000.0f, 0.0f, 110.0f);
    const TArray<FVector> EqualClearanceSource = {
        FVector(25.0f, 0.0f, 30.0f),
        FVector(400.0f, 100.0f, 75.0f),
        FVector(980.0f, 0.0f, 130.0f)
    };
    const TArray<FVector> EqualClearanceSourceCopy = EqualClearanceSource;
    TArray<FVector> EqualNormalized;
    TestTrue(TEXT("An equal-clearance path normalizes"),
        DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
            EqualClearanceSource,
            EqualGroundStart,
            EqualGroundEnd,
            EqualNormalized));
    if (TestEqual(TEXT("Equal-clearance normalization preserves point count"),
            EqualNormalized.Num(), EqualClearanceSource.Num())
        && EqualNormalized.Num() == EqualClearanceSource.Num())
    {
        TestTrue(TEXT("Equal-clearance normalization pins the exact start anchor"),
            EqualNormalized[0] == EqualGroundStart);
        TestTrue(TEXT("Equal-clearance normalization pins the exact end anchor"),
            EqualNormalized.Last() == EqualGroundEnd);
        TestTrue(TEXT("Equal clearance preserves the interior authored grade"),
            FMath::IsNearlyEqual(EqualNormalized[1].Z, 55.0f, KINDA_SMALL_NUMBER));
        TestTrue(TEXT("Equal clearance preserves the interior XY"),
            FVector2D(EqualNormalized[1]).Equals(
                FVector2D(EqualClearanceSource[1]), KINDA_SMALL_NUMBER));
    }
    TestTrue(TEXT("Successful normalization leaves its source immutable"),
        EqualClearanceSource == EqualClearanceSourceCopy);

    const FVector NonUniformGroundStart(0.0f, 0.0f, 100.0f);
    const FVector NonUniformGroundEnd(1000.0f, 0.0f, 300.0f);
    const TArray<FVector> NonUniformClearanceSource = {
        FVector(40.0f, 0.0f, 130.0f),
        FVector(250.0f, 0.0f, 205.0f),
        FVector(960.0f, 0.0f, 310.0f)
    };
    TArray<FVector> NonUniformNormalized;
    TestTrue(TEXT("A non-uniform-clearance path normalizes"),
        DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
            NonUniformClearanceSource,
            NonUniformGroundStart,
            NonUniformGroundEnd,
            NonUniformNormalized));
    if (TestEqual(TEXT("Non-uniform normalization preserves point count"),
            NonUniformNormalized.Num(), NonUniformClearanceSource.Num())
        && NonUniformNormalized.Num() == NonUniformClearanceSource.Num())
    {
        TestTrue(TEXT("Non-uniform normalization pins the exact start anchor"),
            NonUniformNormalized[0] == NonUniformGroundStart);
        TestTrue(TEXT("Non-uniform normalization pins the exact end anchor"),
            NonUniformNormalized.Last() == NonUniformGroundEnd);
        // Endpoint XY is pinned before measuring distance: the interior point
        // is 25% along the 1000 cm path, so its 30->10 cm clearance is 25 cm.
        TestTrue(TEXT("Non-uniform clearance is removed by planar path distance"),
            FMath::IsNearlyEqual(
                NonUniformNormalized[1].Z, 180.0f, KINDA_SMALL_NUMBER));
    }

    const TArray<FVector> OutputSentinel = { FVector(7.0f, 8.0f, 9.0f) };
    TArray<FVector> RejectedOutput = OutputSentinel;
    const TArray<FVector> DegeneratePath = {
        FVector(0.0f, 0.0f, 10.0f), FVector(0.0f, 0.0f, 20.0f) };
    TestFalse(TEXT("A path with no planar extent is rejected"),
        DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
            DegeneratePath,
            FVector(0.0f, 0.0f, 0.0f),
            FVector(0.0f, 0.0f, 30.0f),
            RejectedOutput));
    TestTrue(TEXT("Degenerate rejection leaves output unchanged"),
        RejectedOutput == OutputSentinel);

    TArray<FVector> NonFinitePath = EqualClearanceSource;
    NonFinitePath[1].Z = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("A non-finite path is rejected"),
        DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
            NonFinitePath,
            EqualGroundStart,
            EqualGroundEnd,
            RejectedOutput));
    TestTrue(TEXT("Non-finite rejection leaves output unchanged"),
        RejectedOutput == OutputSentinel);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeTerrainPuttAlignmentTest,
    "DiscGolfTour.Physics.Putting.PineRidgeTerrainTapInAlignment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeTerrainPuttAlignmentTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeTerrainPuttWorld();
    if (!TestNotNull(TEXT("Terrain-putt regression world exists"), World)) return false;
    if (!TestNotNull(TEXT("Terrain-putt regression engine exists"), GEngine))
    {
        World->DestroyWorld(false);
        return false;
    }

    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        if (World->HasBegunPlay())
        {
            World->EndPlay(EEndPlayReason::Quit);
        }
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };

    World->InitializeActorsForPlay(FURL());
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!TestNotNull(TEXT("Terrain-putt world settings exist"), WorldSettings)) return false;
    WorldSettings->NotifyBeginPlay();

    TArray<FDiscGolfHoleBlockoutDefinition> Definitions;
    TArray<int32> TerrainSeeds;
    if (!LoadWorldPlacedPineRidge(*this, Definitions, TerrainSeeds)) return false;
    if (!TestEqual(TEXT("Pine Ridge contributes exactly three placed holes"),
        Definitions.Num(), 3))
    {
        return false;
    }

    ADiscGolfTerrainPresentationActor* Terrain =
        World->SpawnActor<ADiscGolfTerrainPresentationActor>();
    if (!TestNotNull(TEXT("World-placed Pine Ridge terrain exists"), Terrain)) return false;
    if (!TestTrue(TEXT("World-placed Pine Ridge terrain configures with production seeds"),
        Terrain->ConfigureCourse(Definitions, TerrainSeeds, 0.0f, 1.0f)))
    {
        return false;
    }
    World->UpdateWorldComponents(true, false);

    // The 4 m procedural grid linearly approximates the path centerline. Keep
    // enough room for that interpolation while still rejecting the historical
    // +13 cm pin-height bias.
    constexpr float AnchorHeightToleranceCm = 2.0f;
    for (const FDiscGolfHoleBlockoutDefinition& Definition : Definitions)
    {
        const struct
        {
            const TCHAR* Label;
            const FVector* Location;
        } Anchors[] = {
            { TEXT("tee"), &Definition.TeeLocationCm },
            { TEXT("basket"), &Definition.BasketLocationCm }
        };

        for (const auto& Anchor : Anchors)
        {
            FVector GroundLocation;
            const FString TraceLabel = FString::Printf(
                TEXT("Hole %d %s traces the base terrain"),
                Definition.HoleNumber, Anchor.Label);
            if (!TestTrue(*TraceLabel,
                TraceTerrainGround(World, Terrain, *Anchor.Location, GroundLocation)))
            {
                continue;
            }

            const float HeightErrorCm = GroundLocation.Z - Anchor.Location->Z;
            AddInfo(FString::Printf(
                TEXT("Hole %d %s terrain offset: %.4f cm"),
                Definition.HoleNumber, Anchor.Label, HeightErrorCm));
            TestTrue(*FString::Printf(
                TEXT("Hole %d %s terrain height matches its authored anchor"),
                Definition.HoleNumber, Anchor.Label),
                FMath::Abs(HeightErrorCm) <= AnchorHeightToleranceCm);
        }
    }

    const FDiscGolfHoleBlockoutDefinition& OpeningHole = Definitions[0];
    const FDiscGolfShotRouteDefinition* OpeningPrimaryRoute =
        OpeningHole.ShotRoutes.FindByPredicate(
            [](const FDiscGolfShotRouteDefinition& Route)
            {
                return Route.RouteType == EDiscGolfShotRouteType::Primary;
            });
    if (TestNotNull(TEXT("Hole 1 primary route exists"), OpeningPrimaryRoute))
    {
        TArray<FVector> NormalizedPrimaryPoints;
        if (TestTrue(TEXT("Hole 1 primary route normalizes for terrain"),
                DiscGolfTerrainPresentationMath::TryNormalizePathToGroundAnchors(
                    OpeningPrimaryRoute->WaypointsCm,
                    OpeningHole.TeeLocationCm,
                    OpeningHole.BasketLocationCm,
                    NormalizedPrimaryPoints))
            && TestTrue(TEXT("Hole 1 primary route has an interior control point"),
                NormalizedPrimaryPoints.Num() >= 3))
        {
            const FVector PrimaryInterior = NormalizedPrimaryPoints[1];
            FVector PrimaryInteriorGround;
            if (TestTrue(TEXT("Hole 1 primary interior traces the base terrain"),
                    TraceTerrainGround(
                        World, Terrain, PrimaryInterior, PrimaryInteriorGround)))
            {
                const float PrimaryInteriorHeightErrorCm =
                    PrimaryInteriorGround.Z - PrimaryInterior.Z;
                AddInfo(FString::Printf(
                    TEXT("Hole 1 primary interior terrain offset: %.4f cm"),
                    PrimaryInteriorHeightErrorCm));
                TestTrue(TEXT("Primary interior removes the route clearance, not only endpoints"),
                    FMath::Abs(PrimaryInteriorHeightErrorCm) <= 5.0f);
            }
        }
    }

    // Exercise an interior control point on the H2-to-H3 connector as a guard
    // against reintroducing an endpoint-only height bias into shared paths.
    const FVector ConnectorStart = Definitions[1].BasketLocationCm;
    const FVector ConnectorEnd = Definitions[2].TeeLocationCm;
    const FVector2D ConnectorDelta(
        ConnectorEnd.X - ConnectorStart.X,
        ConnectorEnd.Y - ConnectorStart.Y);
    const FVector2D ConnectorSide = FVector2D(
        -ConnectorDelta.Y, ConnectorDelta.X).GetSafeNormal();
    const FVector ConnectorBend(
        ConnectorSide.X * -900.0f,
        ConnectorSide.Y * -900.0f,
        0.0f);
    const FVector ConnectorControlPoint =
        FMath::Lerp(ConnectorStart, ConnectorEnd, 0.34f) + ConnectorBend;
    FVector ConnectorGround;
    if (TestTrue(TEXT("H2-to-H3 connector control point traces the base terrain"),
        TraceTerrainGround(
            World, Terrain, ConnectorControlPoint, ConnectorGround)))
    {
        const float ConnectorHeightErrorCm =
            ConnectorGround.Z - ConnectorControlPoint.Z;
        AddInfo(FString::Printf(
            TEXT("H2-to-H3 connector control-point terrain offset: %.4f cm"),
            ConnectorHeightErrorCm));
        TestTrue(TEXT("Connector control-point interpolation remains locally aligned"),
            FMath::Abs(ConnectorHeightErrorCm) <= 5.0f);
    }

    const FDiscGolfHoleBlockoutDefinition& GalleryLake = Definitions[2];
    const FDiscGolfShotRouteDefinition* PrimaryRoute =
        GalleryLake.ShotRoutes.FindByPredicate(
            [](const FDiscGolfShotRouteDefinition& Route)
            {
                return Route.RouteType == EDiscGolfShotRouteType::Primary;
            });
    if (!TestNotNull(TEXT("Gallery Lake primary route exists"), PrimaryRoute)
        || !TestTrue(TEXT("Gallery Lake primary route reaches its basket"),
            PrimaryRoute->WaypointsCm.Num() >= 2))
    {
        return false;
    }

    const FVector BasketBaseCm = GalleryLake.BasketLocationCm;
    const FVector PreviousRoutePoint =
        PrimaryRoute->WaypointsCm[PrimaryRoute->WaypointsCm.Num() - 2];
    const FVector ApproachDirection = FVector(
        BasketBaseCm.X - PreviousRoutePoint.X,
        BasketBaseCm.Y - PreviousRoutePoint.Y,
        0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);

    const float TapInDistancesCm[] = { 3.0f * 30.48f, 4.574f * 30.48f };
    for (const float DistanceCm : TapInDistancesCm)
    {
        const FVector LieQuery = BasketBaseCm - ApproachDirection * DistanceCm;
        FVector LieGround;
        const FString DistanceLabel = FString::Printf(
            TEXT("Gallery Lake %.3f-foot lie traces the base terrain"),
            DistanceCm / 30.48f);
        if (!TestTrue(*DistanceLabel,
            TraceTerrainGround(World, Terrain, LieQuery, LieGround)))
        {
            continue;
        }

        const FBasketContactEvaluation Aligned = EvaluatePerfectNullPathPutt(
            BasketBaseCm, LieGround, DistanceCm);
        TestEqual(*FString::Printf(
            TEXT("Aligned Gallery Lake %.3f-foot perfect null-path putt catches"),
            DistanceCm / 30.48f),
            Aligned.Result, EBasketContactResult::Caught);

        const FBasketContactEvaluation HistoricalOffset = EvaluatePerfectNullPathPutt(
            BasketBaseCm, LieGround, DistanceCm,
            HistoricalTerrainBiasCm);
        TestEqual(*FString::Printf(
            TEXT("Historical +13 cm Gallery Lake %.3f-foot release hits the band"),
            DistanceCm / 30.48f),
            HistoricalOffset.Result, EBasketContactResult::BandRejection);
        TestTrue(*FString::Printf(
            TEXT("Historical Gallery Lake %.3f-foot line predicts top-band height"),
            DistanceCm / 30.48f),
            HistoricalOffset.PredictedHeightCm >= 139.0f);
    }

    return true;
}

#endif
