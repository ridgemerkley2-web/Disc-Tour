#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "DiscBagComponent.h"
#include "DiscGolfAIPlannerAdapter.h"

#include <limits>

namespace
{
FDGAIShotContext MakeContext(const bool bPutting = false)
{
    FDGAIShotContext Context;
    Context.LieLocationCm = FVector::ZeroVector;
    Context.TargetLocationCm = FVector(10000.0, 0.0, 0.0);
    Context.WindVelocityMps = FVector(1.0, -0.5, 0.0);
    Context.bPutting = bPutting;
    Context.StrokesTaken = 0;
    Context.Par = 3;
    return Context;
}

FDiscGolfAIMeasuredShotOutcome MakeOutcome(
    const UDiscGolfAIGolferProfile& Profile,
    const int32 DiscIndex,
    const FVector& FinalLocationCm)
{
    const FDGDiscInstance& Disc = Profile.BagTemplate[DiscIndex];
    FDiscGolfAIMeasuredShotOutcome Outcome;
    Outcome.StartWorldLocationCm = FVector::ZeroVector;
    Outcome.FinalWorldLocationCm = FinalLocationCm;
    Outcome.Command.DiscInstanceId = Disc.InstanceId;
    Outcome.Command.MoldId = Disc.DiscDefinitionId;
    Outcome.Command.Plastic = EDiscPlastic::Tour;
    Outcome.Command.ThrowStyle = EThrowStyle::Backhand;
    Outcome.Command.ShotContext = EDiscShotContext::Drive;
    Outcome.Command.Direction = FVector::ForwardVector;
    Outcome.Command.Power01 = 0.78f;
    Outcome.Command.HyzerDeg = 2.0f;
    Outcome.Command.NoseAngleDeg = 0.5f;
    Outcome.Command.LaunchAngleDeg = 8.0f;
    Outcome.Command.TimingError = 0.0f;
    Outcome.FinalTelemetry.State = EDiscFlightState::Settled;
    Outcome.FinalTelemetry.GroundState = EDiscGroundState::Settled;
    Outcome.FinalTelemetry.GroundSurface = EGroundSurfaceType::Fairway;
    Outcome.FinalTelemetry.CourseSurface = ECourseSurfaceType::Fairway;
    Outcome.FinalTelemetry.FlightTimeSeconds = 6.0f;
    Outcome.FinalTelemetry.CarryMeters =
        FVector::Dist(Outcome.StartWorldLocationCm, FinalLocationCm) / 100.0f;
    Outcome.FinalTelemetry.Release.ThrowStyle = Outcome.Command.ThrowStyle;
    Outcome.FinalTelemetry.Release.ShotContext = Outcome.Command.ShotContext;
    Outcome.FinalTelemetry.Release.Direction = Outcome.Command.Direction;
    Outcome.FinalTelemetry.Release.Quality01 = 1.0f;
    Outcome.FinalTelemetry.Release.SpeedMultiplier = 1.0f;
    Outcome.FinalTelemetry.Release.SpinMultiplier = 1.0f;
    Outcome.FinalTelemetry.Release.LiePowerMultiplier = 1.0f;
    Outcome.FinalTelemetry.Release.LieTimingErrorMultiplier = 1.0f;
    return Outcome;
}

UDiscGolfAIGolferProfile* BuildProfile(FAutomationTestBase& Test)
{
    UDiscGolfAIGolferProfile* Profile = nullptr;
    FString Error;
    Test.TestTrue(
        TEXT("Generic proof profile builds"),
        DiscGolfAIPlannerAdapter::BuildGenericProofProfile(
            GetTransientPackage(), Profile, Error));
    Test.TestTrue(TEXT("Generic proof profile has no build error"), Error.IsEmpty());
    Test.TestNotNull(TEXT("Generic proof profile exists"), Profile);
    return Profile;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession14AIGenericProfileTest,
    "DiscGolfTour.Session14.AI.GenericProfileAndDefaultBag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14AIGenericProfileTest::RunTest(const FString& Parameters)
{
    UDiscGolfAIGolferProfile* Profile = BuildProfile(*this);
    if (!Profile) return false;

    FString Error;
    TestTrue(TEXT("Profile validates"),
        DiscGolfAIPlannerAdapter::ValidateProfile(*Profile, Error));
    TestTrue(TEXT("Profile uses generic identity"),
        Profile->GolferId.ToString().StartsWith(TEXT("dg_generic_")));
    const FDGDiscBagLoadout DefaultLoadout = UDiscBagComponent::BuildDefaultLoadout();
    TestEqual(TEXT("Profile contains the complete default bag"),
        Profile->BagTemplate.Num(), DefaultLoadout.Discs.Num());
    TestTrue(TEXT("Power is bounded"),
        FMath::IsFinite(Profile->Skill.Power) && Profile->Skill.Power >= 0.0f && Profile->Skill.Power <= 1.0f);
    TestTrue(TEXT("Accuracy is bounded"),
        FMath::IsFinite(Profile->Skill.Accuracy) && Profile->Skill.Accuracy >= 0.0f && Profile->Skill.Accuracy <= 1.0f);
    TestTrue(TEXT("Putting is bounded"),
        FMath::IsFinite(Profile->Skill.Putting) && Profile->Skill.Putting >= 0.0f && Profile->Skill.Putting <= 1.0f);
    TestTrue(TEXT("Consistency is bounded"),
        FMath::IsFinite(Profile->Skill.Consistency) && Profile->Skill.Consistency >= 0.0f && Profile->Skill.Consistency <= 1.0f);
    TestTrue(TEXT("Aggression is bounded"),
        FMath::IsFinite(Profile->Skill.Aggression) && Profile->Skill.Aggression >= 0.0f && Profile->Skill.Aggression <= 1.0f);
    TestTrue(TEXT("Course management is bounded"),
        FMath::IsFinite(Profile->Skill.CourseManagement) && Profile->Skill.CourseManagement >= 0.0f && Profile->Skill.CourseManagement <= 1.0f);

    Profile->Skill.Accuracy = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite skill is rejected"),
        DiscGolfAIPlannerAdapter::ValidateProfile(*Profile, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession14AIMeasuredCandidateTest,
    "DiscGolfTour.Session14.AI.MeasuredOutcomeCandidate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14AIMeasuredCandidateTest::RunTest(const FString& Parameters)
{
    UDiscGolfAIGolferProfile* Profile = BuildProfile(*this);
    if (!Profile) return false;

    const FDGAIShotContext Context = MakeContext();
    const FDiscGolfAIMeasuredShotOutcome Outcome =
        MakeOutcome(*Profile, 1, FVector(6200.0, 300.0, 0.0));
    FDGAIShotCandidate Candidate;
    FString Error;
    TestTrue(TEXT("Measured outcome builds a candidate"),
        DiscGolfAIPlannerAdapter::BuildCandidateFromMeasuredOutcome(
            *Profile, Context, Outcome, Candidate, Error));
    TestTrue(TEXT("Candidate retains measured landing"),
        Candidate.PredictedLandingLocationCm.Equals(Outcome.FinalWorldLocationCm));
    TestEqual(TEXT("Candidate retains stable instance identity"),
        Candidate.Disc.InstanceId, Outcome.Command.DiscInstanceId);
    TestTrue(TEXT("Measured progress is target-distance reduction"),
        FMath::IsNearlyEqual(Candidate.ExpectedProgressM, 61.8816f, 0.01f));
    TestTrue(TEXT("Landing error is measured from the target"),
        FMath::IsNearlyEqual(Candidate.LandingErrorM, 38.1181f, 0.01f));
    TestEqual(TEXT("No fixture contact produces zero obstacle risk"),
        Candidate.ObstacleHitProbability, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession14AIInvalidAtomicTest,
    "DiscGolfTour.Session14.AI.InvalidInputIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14AIInvalidAtomicTest::RunTest(const FString& Parameters)
{
    UDiscGolfAIGolferProfile* Profile = BuildProfile(*this);
    if (!Profile) return false;

    const FDGAIShotContext Context = MakeContext();
    FDiscGolfAIMeasuredShotOutcome Invalid =
        MakeOutcome(*Profile, 0, FVector(6000.0, 0.0, 0.0));
    Invalid.FinalWorldLocationCm.X = std::numeric_limits<double>::quiet_NaN();

    FDGAIShotCandidate CandidateSentinel;
    CandidateSentinel.Disc = Profile->BagTemplate.Last();
    CandidateSentinel.ExpectedProgressM = 321.0f;
    const FGuid CandidateSentinelId = CandidateSentinel.Disc.InstanceId;
    FString Error;
    TestFalse(TEXT("Non-finite measured outcome is rejected"),
        DiscGolfAIPlannerAdapter::BuildCandidateFromMeasuredOutcome(
            *Profile, Context, Invalid, CandidateSentinel, Error));
    TestEqual(TEXT("Rejected candidate leaves identity untouched"),
        CandidateSentinel.Disc.InstanceId, CandidateSentinelId);
    TestEqual(TEXT("Rejected candidate leaves values untouched"),
        CandidateSentinel.ExpectedProgressM, 321.0f);

    FThrowCommand CommandSentinel;
    CommandSentinel.DiscInstanceId = Profile->BagTemplate.Last().InstanceId;
    CommandSentinel.MoldId = TEXT("SentinelMold");
    FDGAIShotCandidate SelectionSentinel = CandidateSentinel;
    const TArray<FDiscGolfAIMeasuredShotOutcome> Outcomes = { Invalid };
    TestFalse(TEXT("Invalid measured set is rejected"),
        DiscGolfAIPlannerAdapter::SelectMeasuredShot(
            *Profile, Context, Outcomes, CommandSentinel, SelectionSentinel, Error));
    TestEqual(TEXT("Rejected selection leaves command identity untouched"),
        CommandSentinel.MoldId, FName(TEXT("SentinelMold")));
    TestEqual(TEXT("Rejected selection leaves candidate identity untouched"),
        SelectionSentinel.Disc.InstanceId, CandidateSentinelId);

    FDGAIShotCandidate InvalidCandidate = CandidateSentinel;
    InvalidCandidate.Power01 = 2.0f;
    TestFalse(TEXT("Out-of-range candidate power is rejected"),
        DiscGolfAIPlannerAdapter::ValidateCandidate(
            *Profile, InvalidCandidate, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession14AIDeterministicSelectionTest,
    "DiscGolfTour.Session14.AI.DeterministicSelectionAndTies",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14AIDeterministicSelectionTest::RunTest(const FString& Parameters)
{
    UDiscGolfAIGolferProfile* Profile = BuildProfile(*this);
    if (!Profile) return false;

    const FDGAIShotContext Context = MakeContext();
    FDiscGolfAIMeasuredShotOutcome First =
        MakeOutcome(*Profile, 0, FVector(6500.0, 0.0, 0.0));
    FDiscGolfAIMeasuredShotOutcome Second =
        MakeOutcome(*Profile, 1, FVector(6500.0, 0.0, 0.0));
    TArray<FDiscGolfAIMeasuredShotOutcome> Forward = { First, Second };
    TArray<FDiscGolfAIMeasuredShotOutcome> Reverse = { Second, First };

    FThrowCommand ForwardCommand;
    FThrowCommand ReverseCommand;
    FDGAIShotCandidate ForwardCandidate;
    FDGAIShotCandidate ReverseCandidate;
    FString Error;
    TestTrue(TEXT("Forward-order tie selects"),
        DiscGolfAIPlannerAdapter::SelectMeasuredShot(
            *Profile, Context, Forward, ForwardCommand, ForwardCandidate, Error));
    TestTrue(TEXT("Reverse-order tie selects"),
        DiscGolfAIPlannerAdapter::SelectMeasuredShot(
            *Profile, Context, Reverse, ReverseCommand, ReverseCandidate, Error));
    TestEqual(TEXT("Tie choice is independent of caller order"),
        ForwardCommand.DiscInstanceId, ReverseCommand.DiscInstanceId);
    TestEqual(TEXT("Stable identity ordering selects the first GUID"),
        ForwardCommand.DiscInstanceId, First.Command.DiscInstanceId);

    FDiscGolfAIMeasuredShotOutcome Better =
        MakeOutcome(*Profile, 2, FVector(9000.0, 0.0, 0.0));
    FDiscGolfAIMeasuredShotOutcome Worse =
        MakeOutcome(*Profile, 3, FVector(3000.0, 0.0, 0.0));
    const TArray<FDiscGolfAIMeasuredShotOutcome> Unequal = { Worse, Better };
    TestTrue(TEXT("Plugin planner selects an unequal set"),
        DiscGolfAIPlannerAdapter::SelectMeasuredShot(
            *Profile, Context, Unequal, ForwardCommand, ForwardCandidate, Error));
    TestEqual(TEXT("Plugin utility selects the measured better result"),
        ForwardCommand.DiscInstanceId, Better.Command.DiscInstanceId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession14AICommandIdentityTest,
    "DiscGolfTour.Session14.AI.SelectedCommandIdentityAndEnums",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14AICommandIdentityTest::RunTest(const FString& Parameters)
{
    UDiscGolfAIGolferProfile* Profile = BuildProfile(*this);
    if (!Profile) return false;

    const FDGAIShotContext Context = MakeContext();
    FDiscGolfAIMeasuredShotOutcome Outcome =
        MakeOutcome(*Profile, 4, FVector(8200.0, -250.0, 0.0));
    Outcome.Command.ThrowStyle = EThrowStyle::Forehand;
    Outcome.Command.Direction = FVector(0.98, -0.2, 0.0).GetSafeNormal();
    Outcome.Command.Power01 = 0.67f;
    Outcome.Command.HyzerDeg = -6.0f;
    Outcome.Command.NoseAngleDeg = -1.5f;
    Outcome.Command.LaunchAngleDeg = 11.0f;
    Outcome.Command.TimingError = -0.12f;
    Outcome.FinalTelemetry.Release.ThrowStyle = Outcome.Command.ThrowStyle;
    Outcome.FinalTelemetry.Release.Direction = Outcome.Command.Direction;

    const TArray<FDiscGolfAIMeasuredShotOutcome> Outcomes = { Outcome };
    FThrowCommand SelectedCommand;
    FDGAIShotCandidate SelectedCandidate;
    FString Error;
    TestTrue(TEXT("Measured command selects"),
        DiscGolfAIPlannerAdapter::SelectMeasuredShot(
            *Profile, Context, Outcomes, SelectedCommand, SelectedCandidate, Error));
    TestEqual(TEXT("Stable instance ID survives round trip"),
        SelectedCommand.DiscInstanceId, Outcome.Command.DiscInstanceId);
    TestEqual(TEXT("Mold ID survives round trip"),
        SelectedCommand.MoldId, Outcome.Command.MoldId);
    TestEqual(TEXT("Plastic enum survives round trip"),
        SelectedCommand.Plastic, Outcome.Command.Plastic);
    TestEqual(TEXT("Throw-style enum survives round trip"),
        SelectedCommand.ThrowStyle, Outcome.Command.ThrowStyle);
    TestEqual(TEXT("Shot-context enum survives round trip"),
        SelectedCommand.ShotContext, Outcome.Command.ShotContext);
    TestTrue(TEXT("Direction survives round trip"),
        SelectedCommand.Direction.Equals(Outcome.Command.Direction));
    TestEqual(TEXT("Power survives round trip"),
        SelectedCommand.Power01, Outcome.Command.Power01);
    TestEqual(TEXT("Hyzer survives round trip"),
        SelectedCommand.HyzerDeg, Outcome.Command.HyzerDeg);
    TestEqual(TEXT("Nose survives round trip"),
        SelectedCommand.NoseAngleDeg, Outcome.Command.NoseAngleDeg);
    TestEqual(TEXT("Launch angle survives round trip"),
        SelectedCommand.LaunchAngleDeg, Outcome.Command.LaunchAngleDeg);
    TestEqual(TEXT("Timing error survives round trip"),
        SelectedCommand.TimingError, Outcome.Command.TimingError);
    return true;
}

#endif
