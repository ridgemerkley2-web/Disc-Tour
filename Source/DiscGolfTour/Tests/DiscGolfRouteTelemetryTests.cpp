#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfRouteTelemetry.h"

namespace
{
    FDiscGolfShotRouteDefinition MakeRoute(FName Id, EDiscGolfShotRouteType Type, float Y = 0.0f)
    {
        FDiscGolfShotRouteDefinition Route;
        Route.RouteId = Id;
        Route.Label = FText::FromName(Id);
        Route.RouteType = Type;
        Route.LandingZoneId = TEXT("Landing");
        Route.CorridorWidthCm = 1000.0f;
        Route.WaypointsCm = { FVector(0.0f, Y, 0.0f), FVector(1000.0f, Y, 0.0f), FVector(2000.0f, Y, 0.0f) };
        return Route;
    }

    FDiscGolfLandingZoneDefinition MakeLandingZone()
    {
        FDiscGolfLandingZoneDefinition Zone;
        Zone.ZoneId = TEXT("Landing");
        Zone.LocationCm = FVector(2000.0f, 0.0f, 0.0f);
        Zone.ExtentCm = FVector(300.0f, 200.0f, 50.0f);
        return Zone;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfRouteTelemetryGeometryTest,
    "DiscGolfTour.LevelDesign.RouteTelemetry.Geometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfRouteTelemetryGeometryTest::RunTest(const FString& Parameters)
{
    const FDiscGolfShotRouteDefinition Route = MakeRoute(
        TEXT("Primary"), EDiscGolfShotRouteType::Primary);
    const FDiscGolfLandingZoneDefinition Zone = MakeLandingZone();
    TArray<FDiscTrajectorySample> Samples;
    for (const FVector& Location : { FVector(0.0f, 0.0f, 0.0f), FVector(1000.0f, 400.0f, 0.0f), FVector(1800.0f, 800.0f, 0.0f) })
    {
        FDiscTrajectorySample Sample;
        Sample.WorldLocationCm = Location;
        Samples.Add(Sample);
    }

    const FDiscGolfRouteTelemetryEvaluation Hit = DiscGolfRouteTelemetry::EvaluateRouteShot(
        Route, Zone, Samples, FVector(2000.0f, 100.0f, 0.0f));
    TestTrue(TEXT("Final location inside the authored landing zone is a hit"), Hit.bLandingZoneHit);
    TestEqual(TEXT("Landing hit is classified inside"), Hit.MissSide, FString(TEXT("Inside")));
    TestEqual(TEXT("All trajectory samples are counted"), Hit.CorridorSampleCount, 3);
    TestEqual(TEXT("Two samples remain inside the half-width corridor"), Hit.CorridorSamplesInside, 2);
    TestTrue(TEXT("Maximum outside deviation is three meters"),
        FMath::IsNearlyEqual(Hit.MaximumCorridorDeviationMeters, 3.0f, 0.001f));
    TestTrue(TEXT("Adherence is two thirds"),
        FMath::IsNearlyEqual(Hit.CorridorAdherencePercent, 200.0f / 3.0f, 0.01f));

    const FDiscGolfRouteTelemetryEvaluation Short = DiscGolfRouteTelemetry::EvaluateRouteShot(
        Route, Zone, {}, FVector(1200.0f, 0.0f, 0.0f));
    TestFalse(TEXT("A short finish misses the landing zone"), Short.bLandingZoneHit);
    TestEqual(TEXT("Longitudinal miss is classified short"), Short.MissSide, FString(TEXT("Short")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfRouteTelemetryReportTest,
    "DiscGolfTour.LevelDesign.RouteTelemetry.Report",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfRouteTelemetryReportTest::RunTest(const FString& Parameters)
{
    FDiscGolfRouteTelemetrySession Session;
    Session.SessionId = TEXT("UnitSession");
    Session.StartedUtc = TEXT("2026-08-13T00:00:00Z");
    Session.UpdatedUtc = Session.StartedUtc;
    Session.CourseId = TEXT("PineRidgeChampionship");
    Session.LayoutId = TEXT("Championship");
    Session.CollisionProfileId = TEXT("PineRidgeCompetitiveV2_Fixtures");
    Session.ActiveRouteId = TEXT("Primary");
    Session.Routes = {
        MakeRoute(TEXT("Primary"), EDiscGolfShotRouteType::Primary),
        MakeRoute(TEXT("Attack"), EDiscGolfShotRouteType::RiskReward, 300.0f),
        MakeRoute(TEXT("Bailout"), EDiscGolfShotRouteType::Bailout, -300.0f)
    };
    Session.LandingZones = { MakeLandingZone() };

    for (const FDiscGolfShotRouteDefinition& Route : Session.Routes)
    {
        for (int32 Index = 0; Index < Session.TargetAttemptsPerRoute; ++Index)
        {
            FDiscGolfRouteTelemetryAttempt Attempt;
            Attempt.AttemptNumber = Session.Attempts.Num() + 1;
            Attempt.RouteId = Route.RouteId;
            Attempt.RecordedUtc = Session.StartedUtc;
            Attempt.CorridorSampleCount = 10;
            Attempt.CorridorSamplesInside = 8;
            Attempt.CorridorAdherencePercent = 80.0f;
            Attempt.RemainingDistanceMeters = 70.0f;
            Session.Attempts.Add(Attempt);
        }
    }
    TestTrue(TEXT("Twenty attempts for each of three routes completes the session"),
        DiscGolfRouteTelemetry::IsComplete(Session));

    FString Json;
    FString Error;
    TestTrue(TEXT("Complete session serializes"),
        DiscGolfRouteTelemetry::SerializeSession(Session, Json, Error));
    TestTrue(TEXT("Report declares the stable schema"), Json.Contains(TEXT("disc_golf_route_telemetry")));
    TestTrue(TEXT("Report declares completion"), Json.Contains(TEXT("\"complete\": true")));

    FDiscGolfRouteTelemetrySession Loaded;
    TestTrue(TEXT("Serialized session deserializes"),
        DiscGolfRouteTelemetry::DeserializeSession(Json, Loaded, Error));
    TestTrue(TEXT("Deserialized identity is compatible"), DiscGolfRouteTelemetry::IsCompatible(
        Loaded, Session.CourseId, Session.LayoutId, Session.CollisionProfileId, 2));
    TestEqual(TEXT("All sixty attempts survive report round trip"), Loaded.Attempts.Num(), 60);
    TestEqual(TEXT("Last attempt retains its route"), Loaded.Attempts.Last().RouteId, FName(TEXT("Bailout")));
    return true;
}

#endif
