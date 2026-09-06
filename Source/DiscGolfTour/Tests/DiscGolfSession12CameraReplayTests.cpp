#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "../DiscGolfCameraViewContract.h"
#include "../DiscGolfPresentationMath.h"
#include "../DiscReplayActor.h"
#include "Engine/World.h"

#include <limits>

namespace
{
void MakeActualReplayCapture(
    TArray<FDiscTrajectorySample>& OutSamples,
    TArray<FDiscGroundTransition>& OutTransitions)
{
    constexpr int32 SampleCount = 2401;
    constexpr float SampleRateHz = 240.0f;
    OutSamples.Reset(SampleCount);
    OutTransitions.Reset();
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        FDiscTrajectorySample& Sample = OutSamples.AddDefaulted_GetRef();
        Sample.TimeSeconds = static_cast<float>(Index) / SampleRateHz;
        Sample.WorldLocationCm = FVector(
            static_cast<float>(Index) * 3.0f,
            FMath::Sin(static_cast<float>(Index) * 0.01f) * 80.0f,
            250.0f - static_cast<float>(Index) * 0.04f);
        Sample.VelocityMps = FVector(20.0f, 1.0f, -0.5f);
        Sample.DiscNormalWorld = FVector::UpVector;
        Sample.WindMps = FVector(2.0f, 0.0f, 0.0f);
        Sample.SpinRpm = FMath::Max(0.0f, 900.0f - static_cast<float>(Index) * 0.2f);
        Sample.AngleOfAttackDeg = 2.0f;
        if (Index >= 800)
        {
            Sample.GroundState = EDiscGroundState::Impact;
            Sample.GroundContactCount = 1;
        }
        if (Index >= 1200) Sample.GroundSurface = EGroundSurfaceType::Dirt;
        if (Index >= 1600) Sample.CourseSurface = ECourseSurfaceType::Hazard;
        if (Index >= 2000) Sample.GroundState = EDiscGroundState::Sliding;
    }

    FDiscGroundTransition& Transition = OutTransitions.AddDefaulted_GetRef();
    Transition.TimeSeconds = OutSamples[2000].TimeSeconds;
    Transition.WorldLocationCm = OutSamples[2000].WorldLocationCm;
    Transition.FromState = EDiscGroundState::Impact;
    Transition.ToState = EDiscGroundState::Sliding;
    Transition.Surface = EGroundSurfaceType::Dirt;
    Transition.CourseSurface = ECourseSurfaceType::Hazard;
    Transition.GroundContactCount = 1;
    Transition.ImpactSpeedMps = 8.0f;
}

bool ContainsActualTime(const TArray<FDiscTrajectorySample>& Samples, float TimeSeconds)
{
    return Samples.ContainsByPredicate([TimeSeconds](const FDiscTrajectorySample& Sample)
    {
        return Sample.TimeSeconds == TimeSeconds;
    });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12ActualReplaySelectionTest,
    "DiscGolfTour.Session12.CameraReplay.ActualSampleSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12ActualReplaySelectionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDiscTrajectorySample> Source;
    TArray<FDiscGroundTransition> Transitions;
    MakeActualReplayCapture(Source, Transitions);

    FDiscActualReplaySelectionPolicy Policy;
    Policy.MaxSourceSamples = 3000;
    Policy.MaxOutputSamples = 1000;
    Policy.MaxSampleRateHz = 60.0f;
    Policy.MaxDurationSeconds = 20.0f;

    TArray<FDiscTrajectorySample> First;
    TArray<FDiscTrajectorySample> Second;
    FString Error;
    TestTrue(TEXT("Strict actual-sample selection succeeds"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Source, Transitions, Policy, First, Error));
    TestTrue(TEXT("Strict selection is deterministic"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Source, Transitions, Policy, Second, Error));
    TestEqual(TEXT("Deterministic selections have the same count"), First.Num(), Second.Num());
    TestTrue(TEXT("Selection stays inside the absolute bound"), First.Num() <= Policy.MaxOutputSamples);
    TestTrue(TEXT("Selection stays at or below nominal 60 Hz"),
        static_cast<float>(First.Num() - 1) / 10.0f <= Policy.MaxSampleRateHz + 0.01f);
    TestEqual(TEXT("Release endpoint is exact"), First[0].TimeSeconds, Source[0].TimeSeconds);
    TestEqual(TEXT("Final endpoint is exact"), First.Last().TimeSeconds, Source.Last().TimeSeconds);
    TestTrue(TEXT("Ground-state boundary is retained"),
        ContainsActualTime(First, Source[800].TimeSeconds));
    TestTrue(TEXT("Physical-surface boundary is retained"),
        ContainsActualTime(First, Source[1200].TimeSeconds));
    TestTrue(TEXT("Course-surface boundary is retained"),
        ContainsActualTime(First, Source[1600].TimeSeconds));
    TestTrue(TEXT("Explicit transition boundary is retained"),
        ContainsActualTime(First, Transitions[0].TimeSeconds));

    bool bOnlyActualSourceSamples = true;
    for (int32 Index = 0; Index < First.Num(); ++Index)
    {
        bOnlyActualSourceSamples &= Source.ContainsByPredicate([&First, Index](const auto& Candidate)
        {
            return Candidate.TimeSeconds == First[Index].TimeSeconds
                && Candidate.WorldLocationCm == First[Index].WorldLocationCm
                && Candidate.VelocityMps == First[Index].VelocityMps
                && Candidate.DiscNormalWorld == First[Index].DiscNormalWorld
                && Candidate.WindMps == First[Index].WindMps
                && Candidate.SpinRpm == First[Index].SpinRpm
                && Candidate.AngleOfAttackDeg == First[Index].AngleOfAttackDeg
                && Candidate.GroundState == First[Index].GroundState
                && Candidate.GroundSurface == First[Index].GroundSurface
                && Candidate.CourseSurface == First[Index].CourseSurface
                && Candidate.GroundContactCount == First[Index].GroundContactCount;
        });
        bOnlyActualSourceSamples &= First[Index].TimeSeconds == Second[Index].TimeSeconds;
    }
    TestTrue(TEXT("No synthetic trajectory samples are created"), bOnlyActualSourceSamples);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12ActualReplayValidationTest,
    "DiscGolfTour.Session12.CameraReplay.ActualSampleValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12ActualReplayValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDiscTrajectorySample> Samples;
    TArray<FDiscGroundTransition> Transitions;
    MakeActualReplayCapture(Samples, Transitions);
    FDiscActualReplaySelectionPolicy Policy;
    Policy.MaxSourceSamples = 3000;

    FString Error;
    TestTrue(TEXT("Finite ordered capture is accepted"),
        DiscGolfPresentationMath::ValidateActualReplaySource(
            Samples, Transitions, Policy, Error));
    TArray<FDiscTrajectorySample> Bounded;
    TestTrue(TEXT("Finite capture produces a bounded actor input"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Samples, Transitions, Policy, Bounded, Error));
    TestTrue(TEXT("Replay actor accepts the bounded native capture contract"),
        ADiscReplayActor::ValidateReplayInput(Bounded, Error));

    FDiscTrajectorySample TerminalSettled = Samples.Last();
    TerminalSettled.GroundState = EDiscGroundState::Settled;
    TerminalSettled.VelocityMps = FVector::ZeroVector;
    TerminalSettled.SpinRpm = 0.0f;
    Samples.Add(TerminalSettled);
    TestTrue(TEXT("Native same-time terminal state at the same position is accepted"),
        DiscGolfPresentationMath::ValidateActualReplaySource(
            Samples, Transitions, Policy, Error));
    TestTrue(TEXT("Native terminal cluster produces bounded actor input"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Samples, Transitions, Policy, Bounded, Error));
    TestTrue(TEXT("Canonical terminal output remains strictly ordered"),
        ADiscReplayActor::ValidateReplayInput(Bounded, Error));
    TestEqual(TEXT("Later actual terminal state is retained"),
        Bounded.Last().GroundState, EDiscGroundState::Settled);

    MakeActualReplayCapture(Samples, Transitions);
    Samples.Last().TimeSeconds = Samples[Samples.Num() - 2].TimeSeconds;
    Samples.Last().WorldLocationCm += FVector(1.0f, 0.0f, 0.0f);
    TestFalse(TEXT("Same-time samples at different positions fail closed"),
        DiscGolfPresentationMath::ValidateActualReplaySource(
            Samples, Transitions, Policy, Error));

    MakeActualReplayCapture(Samples, Transitions);
    Samples[20].VelocityMps.X = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite samples fail closed"),
        DiscGolfPresentationMath::ValidateActualReplaySource(
            Samples, Transitions, Policy, Error));
    TestTrue(TEXT("Non-finite rejection is explicit"), Error.Contains(TEXT("finite")));

    MakeActualReplayCapture(Samples, Transitions);
    TestTrue(TEXT("Ordered source can be bounded before actor validation"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Samples, Transitions, Policy, Bounded, Error));
    Bounded[100].TimeSeconds = Bounded[99].TimeSeconds;
    TestFalse(TEXT("Duplicate timestamps fail closed"),
        ADiscReplayActor::ValidateReplayInput(Bounded, Error));
    TestTrue(TEXT("Time-order rejection is explicit"), Error.Contains(TEXT("time ordered")));

    MakeActualReplayCapture(Samples, Transitions);
    Transitions[0].TimeSeconds = Samples.Last().TimeSeconds + 1.0f;
    TestFalse(TEXT("Out-of-window transitions fail closed"),
        DiscGolfPresentationMath::ValidateActualReplaySource(
            Samples, Transitions, Policy, Error));

    Samples.SetNum(2);
    Transitions.Reset();
    Samples[0].TimeSeconds = 0.0f;
    Samples[1].TimeSeconds = 0.01f;
    TestFalse(TEXT("Two samples inside one nominal 60 Hz interval fail closed"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Samples, Transitions, Policy, Bounded, Error));
    TestTrue(TEXT("Short-capture rejection names the sample-rate bound"),
        Error.Contains(TEXT("sample-rate")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12ReplayControlMathTest,
    "DiscGolfTour.Session12.CameraReplay.PlaybackControlMath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12ReplayControlMathTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("Playback rate clamps low"),
        ADiscReplayActor::NormalizePlaybackRate(-10.0f), ADiscReplayActor::MinimumPlaybackRate);
    TestEqual(TEXT("Playback rate clamps high"),
        ADiscReplayActor::NormalizePlaybackRate(10.0f), ADiscReplayActor::MaximumPlaybackRate);
    TestEqual(TEXT("Non-finite playback rate resolves to the safe default"),
        ADiscReplayActor::NormalizePlaybackRate(std::numeric_limits<float>::quiet_NaN()), 0.75f);
    TestEqual(TEXT("Half speed cycles to three-quarter speed"),
        ADiscReplayActor::NextPlaybackRate(0.50f), 0.75f);
    TestEqual(TEXT("Three-quarter speed cycles to real time"),
        ADiscReplayActor::NextPlaybackRate(0.75f), 1.0f);
    TestEqual(TEXT("Real time cycles to one-and-a-half speed"),
        ADiscReplayActor::NextPlaybackRate(1.0f), 1.5f);
    TestEqual(TEXT("Top preset wraps to half speed"),
        ADiscReplayActor::NextPlaybackRate(1.5f), 0.50f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12ReplayActorControlTest,
    "DiscGolfTour.Session12.CameraReplay.ReplayActorControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12ReplayActorControlTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), TEXT("Session12ReplayControlWorld"));
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("Isolated replay test world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    ADiscReplayActor* Replay = World->SpawnActor<ADiscReplayActor>();
    if (!TestNotNull(TEXT("Replay actor spawns in the isolated world"), Replay)) return false;

    TArray<FDiscTrajectorySample> Source;
    TArray<FDiscGroundTransition> Transitions;
    MakeActualReplayCapture(Source, Transitions);
    FDiscActualReplaySelectionPolicy Policy;
    Policy.MaxSourceSamples = 3000;
    TArray<FDiscTrajectorySample> Bounded;
    FString Error;
    if (!TestTrue(TEXT("Actor fixture is bounded from actual samples"),
        DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Source, Transitions, Policy, Bounded, Error)))
    {
        return false;
    }

    Replay->SetReducedMotion(true);
    TestTrue(TEXT("Reduced motion disables lag through actor state"), Replay->IsReducedMotion());
    TestTrue(TEXT("Replay initializes from the validated actual capture"),
        Replay->InitializeReplay(Bounded, 0.75f));
    TestTrue(TEXT("Initialized replay is playing"), Replay->IsReplayPlaying());
    Replay->PauseReplay();
    TestTrue(TEXT("Pause state is explicit"), Replay->IsReplayPaused());
    TestFalse(TEXT("Paused replay does not report playing"), Replay->IsReplayPlaying());

    const float Midpoint = Replay->GetDurationSeconds() * 0.5f;
    TestTrue(TEXT("Paused replay can seek within its actual capture"), Replay->SeekReplay(Midpoint));
    TestTrue(TEXT("Seek updates playback time"),
        FMath::IsNearlyEqual(Replay->GetPlaybackTimeSeconds(), Midpoint, KINDA_SMALL_NUMBER));
    TestTrue(TEXT("Finite rate changes are accepted"), Replay->SetPlaybackRate(5.0f));
    TestEqual(TEXT("Rate changes remain bounded"), Replay->GetPlaybackRate(),
        ADiscReplayActor::MaximumPlaybackRate);
    TestEqual(TEXT("Playback-rate cycling wraps from the upper bound"),
        Replay->CyclePlaybackRate(), 0.50f);
    TestTrue(TEXT("Paused replay resumes"), Replay->ResumeReplay());
    TestTrue(TEXT("Resumed replay reports playing"), Replay->IsReplayPlaying());
    TestFalse(TEXT("Invalid seek fails closed"),
        Replay->SeekReplay(std::numeric_limits<float>::quiet_NaN()));
    Replay->CancelReplay();
    TestFalse(TEXT("Cancelled replay no longer plays"), Replay->IsReplayPlaying());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12CameraViewCoverageTest,
    "DiscGolfTour.Session12.CameraReplay.ViewModeCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12CameraViewCoverageTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace DiscGolfCameraViewContract;
    TestEqual(TEXT("Aim is player-owned"), RequiredOwner(EDiscGolfCameraViewMode::Aim),
        EDiscGolfCameraViewOwner::PlayerSetup);
    TestEqual(TEXT("Tee is player-owned"), RequiredOwner(EDiscGolfCameraViewMode::Tee),
        EDiscGolfCameraViewOwner::PlayerSetup);
    TestEqual(TEXT("Follow-disc is live-shot-owned"), RequiredOwner(EDiscGolfCameraViewMode::FollowDisc),
        EDiscGolfCameraViewOwner::LiveShot);
    TestEqual(TEXT("Landing is live-shot-owned"), RequiredOwner(EDiscGolfCameraViewMode::Landing),
        EDiscGolfCameraViewOwner::LiveShot);
    TestEqual(TEXT("Basket is live-shot-owned"), RequiredOwner(EDiscGolfCameraViewMode::Basket),
        EDiscGolfCameraViewOwner::LiveShot);
    TestEqual(TEXT("Shot replay is replay-owned"), RequiredOwner(EDiscGolfCameraViewMode::ShotReplay),
        EDiscGolfCameraViewOwner::Replay);
    TestEqual(TEXT("Creator is creator-owned"), RequiredOwner(EDiscGolfCameraViewMode::CharacterCreator),
        EDiscGolfCameraViewOwner::CharacterCreator);
    TestEqual(TEXT("Free camera is free-camera-owned"), RequiredOwner(EDiscGolfCameraViewMode::FreeCamera),
        EDiscGolfCameraViewOwner::FreeCamera);
    TestEqual(TEXT("Course flyover has an explicit owner"), RequiredOwner(EDiscGolfCameraViewMode::CourseFlyover),
        EDiscGolfCameraViewOwner::CourseFlyover);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12CameraViewOwnershipTest,
    "DiscGolfTour.Session12.CameraReplay.ViewOwnershipAndStaleRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12CameraViewOwnershipTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace DiscGolfCameraViewContract;
    FDiscGolfCameraViewState State;
    FDiscGolfCameraViewToken FlyoverToken;
    FString Error;
    TestTrue(TEXT("Flyover acquires an idle presentation view"),
        TryAcquire(State, EDiscGolfCameraViewOwner::CourseFlyover,
            EDiscGolfCameraViewMode::CourseFlyover, FlyoverToken, Error));

    const FDiscGolfCameraViewState BeforeRejectedReplay = State;
    FDiscGolfCameraViewToken ReplayToken;
    TestFalse(TEXT("Replay cannot overlap the active flyover owner"),
        TryAcquire(State, EDiscGolfCameraViewOwner::Replay,
            EDiscGolfCameraViewMode::ShotReplay, ReplayToken, Error));
    TestEqual(TEXT("Rejected concurrency preserves owner"), State.Owner, BeforeRejectedReplay.Owner);
    TestEqual(TEXT("Rejected concurrency preserves generation"), State.Generation,
        BeforeRejectedReplay.Generation);

    FDiscGolfCameraViewToken NewFlyoverToken;
    TestTrue(TEXT("The current owner may refresh its mode generation"),
        TryAcquire(State, EDiscGolfCameraViewOwner::CourseFlyover,
            EDiscGolfCameraViewMode::CourseFlyover, NewFlyoverToken, Error));
    TestFalse(TEXT("The prior generation cannot release the current view"),
        TryRelease(State, FlyoverToken, Error));
    TestTrue(TEXT("The current generation releases successfully"),
        TryRelease(State, NewFlyoverToken, Error));
    TestFalse(TEXT("Released view is inactive"), State.bActive);

    TestTrue(TEXT("Replay can acquire after the prior owner releases"),
        TryAcquire(State, EDiscGolfCameraViewOwner::Replay,
            EDiscGolfCameraViewMode::ShotReplay, ReplayToken, Error));
    TestTrue(TEXT("Replay token is current"), IsCurrent(State, ReplayToken));
    const FDiscGolfCameraViewToken CurrentReplayToken = ReplayToken;
    TestFalse(TEXT("Mismatched owner cannot request a mode"),
        TryAcquire(State, EDiscGolfCameraViewOwner::PlayerSetup,
            EDiscGolfCameraViewMode::Basket, ReplayToken, Error));
    TestEqual(TEXT("Rejected acquisition preserves the current token owner"),
        ReplayToken.Owner, CurrentReplayToken.Owner);
    TestEqual(TEXT("Rejected acquisition preserves the current token mode"),
        ReplayToken.Mode, CurrentReplayToken.Mode);
    TestEqual(TEXT("Rejected acquisition preserves the current token generation"),
        ReplayToken.Generation, CurrentReplayToken.Generation);
    TestTrue(TEXT("Rejected acquisition cannot orphan the active view"),
        IsCurrent(State, ReplayToken));
    return true;
}

#endif
