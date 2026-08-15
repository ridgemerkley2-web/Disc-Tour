#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfPresentationAudio.h"

#include <limits>

namespace
{
void TestBoundedEvent(FAutomationTestBase& Test, const TCHAR* Context, const FDiscGolfPresentationAudioEvent& Event)
{
    Test.TestTrue(FString::Printf(TEXT("%s resolves as a valid contract event"), Context), Event.IsValid());
    Test.TestTrue(FString::Printf(TEXT("%s intensity is finite"), Context), FMath::IsFinite(Event.Intensity01));
    Test.TestTrue(FString::Printf(TEXT("%s intensity is bounded"), Context),
        Event.Intensity01 >= 0.0f && Event.Intensity01 <= 1.0f);
    Test.TestTrue(FString::Printf(TEXT("%s pitch is finite"), Context), FMath::IsFinite(Event.PitchMultiplier));
    Test.TestTrue(FString::Printf(TEXT("%s pitch is bounded"), Context),
        Event.PitchMultiplier >= DiscGolfPresentationAudio::MinimumPitchMultiplier
        && Event.PitchMultiplier <= DiscGolfPresentationAudio::MaximumPitchMultiplier);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPresentationAudioReleaseVocabularyTest,
    "DiscGolfTour.Presentation.Audio.ReleaseVocabulary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPresentationAudioReleaseVocabularyTest::RunTest(const FString& Parameters)
{
    const FDiscGolfPresentationAudioEvent Perfect = DiscGolfPresentationAudio::ResolveThrowRelease(
        EReleaseGrade::Perfect, EReleaseTiming::OnTime, 1.0f, 30.0f);
    const FDiscGolfPresentationAudioEvent PerfectAgain = DiscGolfPresentationAudio::ResolveThrowRelease(
        EReleaseGrade::Perfect, EReleaseTiming::OnTime, 1.0f, 30.0f);
    const FDiscGolfPresentationAudioEvent LatePoor = DiscGolfPresentationAudio::ResolveThrowRelease(
        EReleaseGrade::Poor, EReleaseTiming::Late, 0.0f, 8.0f);

    TestBoundedEvent(*this, TEXT("Perfect release"), Perfect);
    TestBoundedEvent(*this, TEXT("Poor late release"), LatePoor);
    TestEqual(TEXT("Release ID is stable"), Perfect.EventId, FName(TEXT("Presentation.Throw.Release.Perfect.OnTime")));
    TestEqual(TEXT("Release label is stable"), Perfect.Label, FString(TEXT("Perfect On-Time Release")));
    TestEqual(TEXT("Identical input produces identical intensity"), Perfect.Intensity01, PerfectAgain.Intensity01);
    TestEqual(TEXT("Identical input produces identical pitch"), Perfect.PitchMultiplier, PerfectAgain.PitchMultiplier);
    TestTrue(TEXT("Better release carries greater presentation intensity"), Perfect.Intensity01 > LatePoor.Intensity01);
    TestFalse(TEXT("Impossible perfect-early combination is rejected"),
        DiscGolfPresentationAudio::ResolveThrowRelease(
            EReleaseGrade::Perfect, EReleaseTiming::Early, 1.0f, 30.0f).IsValid());
    TestFalse(TEXT("Unknown release grade is rejected"),
        DiscGolfPresentationAudio::ResolveThrowRelease(
            static_cast<EReleaseGrade>(255), EReleaseTiming::OnTime, 1.0f, 30.0f).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPresentationAudioFlightSanitizationTest,
    "DiscGolfTour.Presentation.Audio.FlightSanitization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPresentationAudioFlightSanitizationTest::RunTest(const FString& Parameters)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const FDiscGolfPresentationAudioEvent Malformed = DiscGolfPresentationAudio::ResolveAirborneFlight(
        NaN, Infinity, -1000.0f);
    const FDiscGolfPresentationAudioEvent Extreme = DiscGolfPresentationAudio::ResolveAirborneFlight(
        100000.0f, -100000.0f, 1000.0f);

    TestBoundedEvent(*this, TEXT("Malformed airborne telemetry"), Malformed);
    TestBoundedEvent(*this, TEXT("Extreme airborne telemetry"), Extreme);
    TestEqual(TEXT("Airborne ID does not vary with telemetry"),
        Malformed.EventId, FName(TEXT("Presentation.Flight.Airborne")));
    TestTrue(TEXT("Extreme finite telemetry saturates without escaping bounds"),
        Extreme.Intensity01 >= Malformed.Intensity01);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPresentationAudioGroundVocabularyTest,
    "DiscGolfTour.Presentation.Audio.GroundVocabulary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPresentationAudioGroundVocabularyTest::RunTest(const FString& Parameters)
{
    const FDiscGolfPresentationAudioEvent FairwayContact = DiscGolfPresentationAudio::ResolveGroundContact(
        EGroundSurfaceType::Fairway, 12.0f, 18.0f);
    const FDiscGolfPresentationAudioEvent RockContact = DiscGolfPresentationAudio::ResolveGroundContact(
        EGroundSurfaceType::Rock, 12.0f, 18.0f);
    const FDiscGolfPresentationAudioEvent Skip = DiscGolfPresentationAudio::ResolveGroundState(
        EDiscGroundState::Skipping, EGroundSurfaceType::Dirt, 9.0f);
    const FDiscGolfPresentationAudioEvent Settled = DiscGolfPresentationAudio::ResolveGroundState(
        EDiscGroundState::Settled, EGroundSurfaceType::Rough, -25.0f);
    const FDiscGolfPresentationAudioEvent HazardWater =
        DiscGolfPresentationAudio::ResolveCourseSurfaceContact(
            EGroundSurfaceType::Rough, ECourseSurfaceType::Hazard, 10.0f, 22.0f, TEXT("Water"));
    const FDiscGolfPresentationAudioEvent DryHazard =
        DiscGolfPresentationAudio::ResolveCourseSurfaceContact(
            EGroundSurfaceType::Rough, ECourseSurfaceType::Hazard, 10.0f, 22.0f);
    const FDiscGolfPresentationAudioEvent DeepRough =
        DiscGolfPresentationAudio::ResolveCourseSurfaceContact(
            EGroundSurfaceType::Rough, ECourseSurfaceType::DeepRough, 10.0f, 22.0f);

    TestBoundedEvent(*this, TEXT("Fairway contact"), FairwayContact);
    TestBoundedEvent(*this, TEXT("Rock contact"), RockContact);
    TestBoundedEvent(*this, TEXT("Dirt skip"), Skip);
    TestBoundedEvent(*this, TEXT("Rough settle"), Settled);
    TestBoundedEvent(*this, TEXT("Hazard or water contact"), HazardWater);
    TestBoundedEvent(*this, TEXT("Dry hazard contact"), DryHazard);
    TestBoundedEvent(*this, TEXT("Deep rough course contact"), DeepRough);
    TestEqual(TEXT("Surface is represented in the contact ID"),
        RockContact.EventId, FName(TEXT("Presentation.Ground.Contact.Rock")));
    TestEqual(TEXT("State and surface are represented in the state ID"),
        Skip.EventId, FName(TEXT("Presentation.Ground.State.Skipping.Dirt")));
    TestTrue(TEXT("Rock contact is pitched above fairway contact"),
        RockContact.PitchMultiplier > FairwayContact.PitchMultiplier);
    TestEqual(TEXT("Hazard and water retain rich course identity"), HazardWater.EventId,
        FName(TEXT("Presentation.Course.Contact.HazardWater.Rough")));
    TestNotEqual(TEXT("Hazard and deep rough do not collapse to one presentation event"),
        HazardWater.EventId, DeepRough.EventId);
    TestEqual(TEXT("Dry hazard retains a distinct event"), DryHazard.EventId,
        FName(TEXT("Presentation.Course.Contact.HazardDry.Rough")));
    TestNotEqual(TEXT("Dry hazard and water do not share an event"),
        DryHazard.EventId, HazardWater.EventId);
    TestFalse(TEXT("Airborne state is owned by the flight vocabulary"),
        DiscGolfPresentationAudio::ResolveGroundState(
            EDiscGroundState::Airborne, EGroundSurfaceType::Fairway, 10.0f).IsValid());
    TestFalse(TEXT("Unknown surface is rejected"),
        DiscGolfPresentationAudio::ResolveGroundContact(
            static_cast<EGroundSurfaceType>(255), 10.0f, 15.0f).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPresentationAudioOutcomeVocabularyTest,
    "DiscGolfTour.Presentation.Audio.BasketAndPenaltyVocabulary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPresentationAudioOutcomeVocabularyTest::RunTest(const FString& Parameters)
{
    const FDiscGolfPresentationAudioEvent Catch = DiscGolfPresentationAudio::ResolveBasketOutcome(
        EBasketContactResult::Caught, 8.0f);
    const FDiscGolfPresentationAudioEvent Band = DiscGolfPresentationAudio::ResolveBasketOutcome(
        EBasketContactResult::BandRejection, 14.0f);
    const FDiscGolfPresentationAudioEvent Ob = DiscGolfPresentationAudio::ResolvePenalty(
        EDiscGolfPenaltyType::OutOfBounds, 1);
    const FDiscGolfPresentationAudioEvent Hazard = DiscGolfPresentationAudio::ResolvePenalty(
        EDiscGolfPenaltyType::Hazard, 1000);

    TestBoundedEvent(*this, TEXT("Basket catch"), Catch);
    TestBoundedEvent(*this, TEXT("Band rejection"), Band);
    TestBoundedEvent(*this, TEXT("Out-of-bounds penalty"), Ob);
    TestBoundedEvent(*this, TEXT("Sanitized hazard penalty"), Hazard);
    TestEqual(TEXT("Caught outcome has stable ID"), Catch.EventId, FName(TEXT("Presentation.Basket.Caught")));
    TestEqual(TEXT("Penalty type has stable ID"), Ob.EventId, FName(TEXT("Presentation.Penalty.OutOfBounds")));
    TestFalse(TEXT("No basket contact emits no event"),
        DiscGolfPresentationAudio::ResolveBasketOutcome(EBasketContactResult::None, 5.0f).IsValid());
    TestFalse(TEXT("No penalty emits no event"),
        DiscGolfPresentationAudio::ResolvePenalty(EDiscGolfPenaltyType::None, 1).IsValid());
    TestFalse(TEXT("A zero-stroke penalty is invalid"),
        DiscGolfPresentationAudio::ResolvePenalty(EDiscGolfPenaltyType::Hazard, 0).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPresentationAudioRoundVocabularyTest,
    "DiscGolfTour.Presentation.Audio.HoleAndRoundVocabulary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPresentationAudioRoundVocabularyTest::RunTest(const FString& Parameters)
{
    const FDiscGolfPresentationAudioEvent Advance = DiscGolfPresentationAudio::ResolveHoleTransition(1, 2, 3);
    const FDiscGolfPresentationAudioEvent Return = DiscGolfPresentationAudio::ResolveHoleTransition(3, 1, 3);
    const FDiscGolfPresentationAudioEvent HoleStart = DiscGolfPresentationAudio::ResolveHoleStart(2, 3, 4);
    const FDiscGolfPresentationAudioEvent HoleComplete = DiscGolfPresentationAudio::ResolveHoleCompletion(2, 3, 4);
    const FDiscGolfPresentationAudioEvent UnderPar = DiscGolfPresentationAudio::ResolveRoundCompletion(3, 3, -8);
    const FDiscGolfPresentationAudioEvent EvenPar = DiscGolfPresentationAudio::ResolveRoundCompletion(3, 3, 0);
    const FDiscGolfPresentationAudioEvent OverPar = DiscGolfPresentationAudio::ResolveRoundCompletion(3, 3, 5);

    TestBoundedEvent(*this, TEXT("Hole advance"), Advance);
    TestBoundedEvent(*this, TEXT("Hole return"), Return);
    TestBoundedEvent(*this, TEXT("Hole start"), HoleStart);
    TestBoundedEvent(*this, TEXT("Hole completion"), HoleComplete);
    TestBoundedEvent(*this, TEXT("Under-par completion"), UnderPar);
    TestBoundedEvent(*this, TEXT("Even-par completion"), EvenPar);
    TestBoundedEvent(*this, TEXT("Over-par completion"), OverPar);
    TestEqual(TEXT("Advance ID is stable"),
        Advance.EventId, FName(TEXT("Presentation.Hole.Transition.Advance")));
    TestEqual(TEXT("Under-par result selects its semantic ID"),
        UnderPar.EventId, FName(TEXT("Presentation.Round.Complete.UnderPar")));
    TestTrue(TEXT("Under-par completion is pitched above over-par completion"),
        UnderPar.PitchMultiplier > OverPar.PitchMultiplier);
    TestFalse(TEXT("Self-transition is rejected"),
        DiscGolfPresentationAudio::ResolveHoleTransition(2, 2, 3).IsValid());
    TestFalse(TEXT("Out-of-range transition is rejected"),
        DiscGolfPresentationAudio::ResolveHoleTransition(3, 4, 3).IsValid());
    TestFalse(TEXT("Partial round cannot emit completion"),
        DiscGolfPresentationAudio::ResolveRoundCompletion(2, 3, -4).IsValid());
    TestFalse(TEXT("Out-of-range hole start is rejected"),
        DiscGolfPresentationAudio::ResolveHoleStart(4, 3, 4).IsValid());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPresentationAudioContractValidationTest,
    "DiscGolfTour.Presentation.Audio.ContractValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPresentationAudioContractValidationTest::RunTest(const FString& Parameters)
{
    FDiscGolfPresentationAudioEvent Event = DiscGolfPresentationAudio::ResolveAirborneFlight(18.0f, 700.0f, 0.4f);
    TestTrue(TEXT("Resolver output passes contract validation"), Event.IsValid());

    Event.Intensity01 = 1.01f;
    TestFalse(TEXT("Out-of-bounds intensity fails validation"), Event.IsValid());
    Event = DiscGolfPresentationAudio::ResolveAirborneFlight(18.0f, 700.0f, 0.4f);
    Event.PitchMultiplier = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite pitch fails validation"), Event.IsValid());
    Event = DiscGolfPresentationAudio::ResolveAirborneFlight(18.0f, 700.0f, 0.4f);
    Event.EventId = NAME_None;
    TestFalse(TEXT("Missing semantic ID fails validation"), Event.IsValid());
    Event = DiscGolfPresentationAudio::ResolveAirborneFlight(18.0f, 700.0f, 0.4f);
    Event.ContractVersion = 999;
    TestFalse(TEXT("Unknown contract version fails validation"), Event.IsValid());

    FDiscGolfPresentationAudioContext Context;
    Context.ShotSequence = 7;
    Context.HoleNumber = 3;
    Context.WorldLocationCm = FVector(1200.0f, -300.0f, 88.0f);
    Context.EventTimeSeconds = 1.25f;
    Context.bReplayPresentation = false;
    Context.CourseSurface = ECourseSurfaceType::Hazard;
    Context.Penalty = EDiscGolfPenaltyType::Hazard;
    const FDiscGolfPresentationAudioEvent Contextual = DiscGolfPresentationAudio::WithContext(
        DiscGolfPresentationAudio::ResolvePenalty(EDiscGolfPenaltyType::Hazard, 1), Context);
    TestTrue(TEXT("Valid lifecycle context attaches to an event"), Contextual.IsValid());
    TestTrue(TEXT("Context flag is set"), Contextual.bHasContext);
    TestEqual(TEXT("Shot sequence survives context attachment"), Contextual.Context.ShotSequence, 7);
    TestTrue(TEXT("Semantic dedupe key includes context"),
        Contextual.DedupeKey().Contains(TEXT("H3|S7|R0|T1250")));
    TestTrue(TEXT("Normal play permits presentation events"),
        DiscGolfPresentationAudio::ShouldEmit(Contextual, false));
    TestFalse(TEXT("Regression play suppresses presentation events"),
        DiscGolfPresentationAudio::ShouldEmit(Contextual, true));

    TArray<FDiscGolfPresentationAudioEvent> Trace;
    TestTrue(TEXT("First contextual event enters trace"),
        DiscGolfPresentationAudio::AppendBoundedTrace(Trace, Contextual, 3));
    TestFalse(TEXT("Immediate duplicate is suppressed"),
        DiscGolfPresentationAudio::AppendBoundedTrace(Trace, Contextual, 3));
    for (int32 Index = 0; Index < 5; ++Index)
    {
        Context.EventTimeSeconds = 2.0f + static_cast<float>(Index);
        const FDiscGolfPresentationAudioEvent Next = DiscGolfPresentationAudio::WithContext(
            DiscGolfPresentationAudio::ResolveAirborneFlight(15.0f, 600.0f, 0.5f), Context);
        DiscGolfPresentationAudio::AppendBoundedTrace(Trace, Next, 3);
    }
    TestEqual(TEXT("Trace remains bounded"), Trace.Num(), 3);
    TestTrue(TEXT("Trace retains newest event"),
        Trace.Last().DedupeKey().Contains(TEXT("T6000")));

    Context.EventTimeSeconds = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite lifecycle context is rejected"),
        DiscGolfPresentationAudio::WithContext(
            DiscGolfPresentationAudio::ResolveReplay(true), Context).IsValid());
    TestBoundedEvent(*this, TEXT("Replay start"), DiscGolfPresentationAudio::ResolveReplay(true));
    TestBoundedEvent(*this, TEXT("Flyover stop"), DiscGolfPresentationAudio::ResolveFlyover(false));
    return true;
}

#endif
