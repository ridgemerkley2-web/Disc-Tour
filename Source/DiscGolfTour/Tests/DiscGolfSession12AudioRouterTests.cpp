#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfPresentationAudioRouterComponent.h"

#include <limits>

namespace
{
FDiscGolfPresentationAudioEvent WithTestContext(
    const FDiscGolfPresentationAudioEvent& Event,
    ECourseSurfaceType Surface = ECourseSurfaceType::Fairway,
    EBasketContactResult Basket = EBasketContactResult::None,
    bool bReplay = false)
{
    FDiscGolfPresentationAudioContext Context;
    Context.ShotSequence = 2;
    Context.HoleNumber = 1;
    Context.WorldLocationCm = FVector(1200.0f, -800.0f, 110.0f);
    Context.EventTimeSeconds = 1.25f;
    Context.bReplayPresentation = bReplay;
    Context.CourseSurface = Surface;
    Context.BasketResult = Basket;
    return DiscGolfPresentationAudio::WithContext(Event, Context);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AudioRouteMappingTest,
    "DiscGolfTour.Session12.Audio.SemanticProviderMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AudioRouteMappingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfPresentationAudioRoute Route;

    const FDiscGolfPresentationAudioEvent Release = WithTestContext(
        DiscGolfPresentationAudio::ResolveThrowRelease(
            EReleaseGrade::Perfect, EReleaseTiming::OnTime, 1.0f, 30.0f));
    TestTrue(TEXT("Release semantic event routes"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(Release, Route, 30.0f, 1050.0f));
    TestEqual(TEXT("Release maps to plugin value DTO type"),
        Route.Payload.EventType, EDGAudioEventType::ThrowRelease);
    TestEqual(TEXT("Semantic ID survives the lossy compatibility DTO"),
        Route.EventId, Release.EventId);
    TestEqual(TEXT("Release speed remains SI meters per second"), Route.Payload.SpeedMps, 30.0f);
    TestEqual(TEXT("Release spin remains RPM"), Route.Payload.SpinRpm, 1050.0f);

    const FDiscGolfPresentationAudioEvent Flight = WithTestContext(
        DiscGolfPresentationAudio::ResolveAirborneFlight(24.0f, 900.0f, 0.3f));
    TestTrue(TEXT("Flight semantic event routes"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(Flight, Route, 24.0f, 900.0f));
    TestEqual(TEXT("Airborne semantic intent maps to a flight loop request"),
        Route.Payload.EventType, EDGAudioEventType::DiscFlightLoop);

    const FDiscGolfPresentationAudioEvent Water = WithTestContext(
        DiscGolfPresentationAudio::ResolveCourseSurfaceContact(
            EGroundSurfaceType::Rough, ECourseSurfaceType::Hazard,
            10.0f, 22.0f, TEXT("Water")), ECourseSurfaceType::Hazard);
    TestTrue(TEXT("Water contact routes"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(Water, Route, 10.0f));
    TestEqual(TEXT("Water identity survives as a typed provider surface"),
        Route.Payload.Surface, EDGImpactSurface::Water);
    TestEqual(TEXT("Ground contact maps to an impact request"),
        Route.Payload.EventType, EDGAudioEventType::DiscImpact);

    const FDiscGolfPresentationAudioEvent Chains = WithTestContext(
        DiscGolfPresentationAudio::ResolveBasketOutcome(EBasketContactResult::Caught, 8.0f),
        ECourseSurfaceType::Fairway, EBasketContactResult::Caught);
    TestTrue(TEXT("Basket catch routes"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(Chains, Route, 8.0f));
    TestEqual(TEXT("Caught basket maps to chain response"),
        Route.Payload.EventType, EDGAudioEventType::BasketChains);
    TestEqual(TEXT("Caught basket uses chain surface"),
        Route.Payload.Surface, EDGImpactSurface::Chain);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AudioRouteSanitizationTest,
    "DiscGolfTour.Session12.Audio.PayloadSanitization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AudioRouteSanitizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const FDiscGolfPresentationAudioEvent Event = WithTestContext(
        DiscGolfPresentationAudio::ResolveAirborneFlight(20.0f, 700.0f, 0.5f));
    FDiscGolfPresentationAudioRoute Route;
    TestTrue(TEXT("A valid semantic trigger accepts malformed optional modulation data"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(
            Event, Route, NaN, -Infinity, Infinity, NaN, Infinity, NaN));
    TestTrue(TEXT("Sanitized route validates"), Route.IsValid());
    TestEqual(TEXT("Non-finite speed becomes zero"), Route.Payload.SpeedMps, 0.0f);
    TestEqual(TEXT("Non-finite spin becomes zero"), Route.Payload.SpinRpm, 0.0f);
    TestEqual(TEXT("Non-finite wobble becomes zero"), Route.Payload.WobbleDegrees, 0.0f);
    TestEqual(TEXT("Non-finite wetness becomes zero"), Route.Payload.Wetness01, 0.0f);
    TestEqual(TEXT("Non-finite listener distance becomes zero"), Route.ListenerDistanceCm, 0.0f);
    TestEqual(TEXT("Non-finite settings gain uses safe unity fallback"), Route.OutputGain01, 1.0f);

    TestTrue(TEXT("Extreme finite values clamp"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(
            Event, Route, 1000.0f, -10000.0f, -200.0f, 5.0f, 50000000.0f, -5.0f));
    TestEqual(TEXT("Speed clamps"), Route.Payload.SpeedMps,
        UDiscGolfPresentationAudioRouterComponent::MaximumSpeedMps);
    TestEqual(TEXT("Absolute spin clamps"), Route.Payload.SpinRpm,
        UDiscGolfPresentationAudioRouterComponent::MaximumSpinRpm);
    TestEqual(TEXT("Absolute wobble clamps"), Route.Payload.WobbleDegrees,
        UDiscGolfPresentationAudioRouterComponent::MaximumWobbleDegrees);
    TestEqual(TEXT("Wetness clamps"), Route.Payload.Wetness01, 1.0f);
    TestEqual(TEXT("Listener distance clamps"), Route.ListenerDistanceCm,
        UDiscGolfPresentationAudioRouterComponent::MaximumListenerDistanceCm);
    TestEqual(TEXT("Settings gain clamps"), Route.OutputGain01, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AudioSilentFallbackTest,
    "DiscGolfTour.Session12.Audio.SilentFallbackAndDedupe",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AudioSilentFallbackTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfPresentationAudioRouterComponent* Router =
        NewObject<UDiscGolfPresentationAudioRouterComponent>();
    TestNotNull(TEXT("Project-owned audio router constructs without a world"), Router);
    if (!Router) return false;
    TestFalse(TEXT("Audio router never ticks"), Router->PrimaryComponentTick.bCanEverTick);
    TestEqual(TEXT("No authored fallback assets are required"), Router->FallbackSounds.Num(), 0);

    const FDiscGolfPresentationAudioEvent Event = WithTestContext(
        DiscGolfPresentationAudio::ResolveThrowRelease(
            EReleaseGrade::Great, EReleaseTiming::Early, 0.8f, 24.0f));
    TestTrue(TEXT("Missing assets are a truthful successful semantic route"),
        Router->ConsumeSemanticEvent(Event, 24.0f, 800.0f));
    TestEqual(TEXT("Silent route is retained for provider diagnostics"), Router->GetRouteTraceCount(), 1);
    TestFalse(TEXT("No asset cannot claim a ready fallback"), Router->GetLastRoute().bFallbackSoundReady);
    TestTrue(TEXT("Status explicitly reports silent fallback"),
        Router->GetRouterStatusText().Contains(TEXT("SILENT ASSET FALLBACK")));
    TestFalse(TEXT("Immediate duplicate is suppressed"),
        Router->ConsumeSemanticEvent(Event, 24.0f, 800.0f));
    TestEqual(TEXT("Duplicate does not grow the route trace"), Router->GetRouteTraceCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AudioRouteBoundTest,
    "DiscGolfTour.Session12.Audio.RouteTraceBound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AudioRouteBoundTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfPresentationAudioRouterComponent* Router =
        NewObject<UDiscGolfPresentationAudioRouterComponent>();
    for (int32 Index = 0;
        Index < UDiscGolfPresentationAudioRouterComponent::MaximumRouteTrace + 20;
        ++Index)
    {
        FDiscGolfPresentationAudioContext Context;
        Context.ShotSequence = Index + 1;
        Context.HoleNumber = 1;
        Context.WorldLocationCm = FVector(static_cast<float>(Index), 0.0f, 0.0f);
        Context.EventTimeSeconds = static_cast<float>(Index) * 0.1f;
        const FDiscGolfPresentationAudioEvent Event = DiscGolfPresentationAudio::WithContext(
            DiscGolfPresentationAudio::ResolveAirborneFlight(20.0f, 700.0f, 0.5f), Context);
        TestTrue(FString::Printf(TEXT("Unique route %d is accepted"), Index),
            Router->ConsumeSemanticEvent(Event, 20.0f, 700.0f));
    }
    TestEqual(TEXT("Provider diagnostics stay bounded"), Router->GetRouteTraceCount(),
        UDiscGolfPresentationAudioRouterComponent::MaximumRouteTrace);
    TestEqual(TEXT("Bounded trace retains the newest semantic shot"),
        Router->GetLastRoute().EventId, FName(TEXT("Presentation.Flight.Airborne")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AudioInvalidEventAtomicTest,
    "DiscGolfTour.Session12.Audio.InvalidEventIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AudioInvalidEventAtomicTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfPresentationAudioRoute Output;
    Output.EventId = TEXT("Sentinel.Unchanged");
    FDiscGolfPresentationAudioEvent Invalid =
        DiscGolfPresentationAudio::ResolveAirborneFlight(20.0f, 700.0f, 0.5f);
    Invalid.ContractVersion = 999;
    TestFalse(TEXT("Unknown semantic contract version is rejected"),
        UDiscGolfPresentationAudioRouterComponent::BuildRoute(Invalid, Output));
    TestEqual(TEXT("Rejected build does not mutate caller output"),
        Output.EventId, FName(TEXT("Sentinel.Unchanged")));

    UDiscGolfPresentationAudioRouterComponent* Router =
        NewObject<UDiscGolfPresentationAudioRouterComponent>();
    TestFalse(TEXT("Router rejects invalid semantic input"), Router->ConsumeSemanticEvent(Invalid));
    TestEqual(TEXT("Rejected semantic input leaves trace empty"), Router->GetRouteTraceCount(), 0);
    return true;
}

#endif
