#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfRHBHThrowTransaction.h"

namespace
{
    FThrowCommand MakeSession3Command()
    {
        FThrowCommand Command;
        Command.DiscInstanceId = FGuid(
            0xD6153001, 0x53455353, 0x494F4E33, 0x00000001);
        Command.MoldId = TEXT("Session3AuthorityFixture");
        Command.Plastic = EDiscPlastic::Crystal;
        Command.ThrowStyle = EThrowStyle::Backhand;
        Command.Handedness = EDGHandedness::Right;
        Command.ShotContext = EDiscShotContext::Drive;
        Command.Direction = FVector(0.8125f, -0.4375f, 0.125f);
        Command.Power01 = 0.73125f;
        Command.HyzerDeg = -11.375f;
        Command.NoseAngleDeg = 2.625f;
        Command.LaunchAngleDeg = 8.875f;
        Command.TimingError = -0.21875f;
        return Command;
    }

    void TestCommandIsExact(
        FAutomationTestBase& Test,
        const FThrowCommand& Actual,
        const FThrowCommand& Expected)
    {
        Test.TestEqual(TEXT("Disc instance id passes through exactly"),
            Actual.DiscInstanceId, Expected.DiscInstanceId);
        Test.TestEqual(TEXT("Mold id passes through exactly"), Actual.MoldId, Expected.MoldId);
        Test.TestEqual(TEXT("Plastic passes through exactly"), Actual.Plastic, Expected.Plastic);
        Test.TestEqual(TEXT("Throw style passes through exactly"), Actual.ThrowStyle, Expected.ThrowStyle);
        Test.TestEqual(TEXT("Handedness passes through exactly"), Actual.Handedness, Expected.Handedness);
        Test.TestEqual(TEXT("Shot context passes through exactly"), Actual.ShotContext, Expected.ShotContext);
        Test.TestTrue(TEXT("Direction passes through without normalization or remapping"), Actual.Direction == Expected.Direction);
        Test.TestEqual(TEXT("Power passes through exactly"), Actual.Power01, Expected.Power01);
        Test.TestEqual(TEXT("Hyzer passes through exactly"), Actual.HyzerDeg, Expected.HyzerDeg);
        Test.TestEqual(TEXT("Nose angle passes through exactly"), Actual.NoseAngleDeg, Expected.NoseAngleDeg);
        Test.TestEqual(TEXT("Launch angle passes through exactly"), Actual.LaunchAngleDeg, Expected.LaunchAngleDeg);
        Test.TestEqual(TEXT("Timing error passes through exactly"), Actual.TimingError, Expected.TimingError);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession3SingleAuthorityReleaseTest,
    "DiscGolfTour.Character.Session3.ThrowTransaction.SingleAuthorityRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession3SingleAuthorityReleaseTest::RunTest(const FString& Parameters)
{
    FDiscGolfRHBHThrowTransaction Transaction;
    const FThrowCommand Command = MakeSession3Command();
    int32 AuthoritativeLaunchRequests = 0;

    TestTrue(TEXT("RHBH transaction begins"), Transaction.Begin(Command));
    TestTrue(TEXT("Animation transaction is awaiting its exact release"), Transaction.IsAwaitingRelease());
    TestTrue(TEXT("Release commits"), Transaction.TryCommitRelease(
        [&AuthoritativeLaunchRequests](const FThrowCommand&)
        {
            ++AuthoritativeLaunchRequests;
        }));

    TestEqual(TEXT("Exactly one authoritative launch is requested"), AuthoritativeLaunchRequests, 1);
    TestEqual(TEXT("Exactly one release is committed for the attempt"), Transaction.GetReleaseCommitCountForAttempt(), 1);
    TestTrue(TEXT("The transaction records that the disc was released"), Transaction.HasCommittedRelease());
    TestTrue(TEXT("Transaction advances to Released before recovery"),
        Transaction.GetState() == EDiscGolfRHBHThrowTransactionState::Released);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession3DuplicateAndReentrantReleaseTest,
    "DiscGolfTour.Character.Session3.ThrowTransaction.DuplicateReleaseGuard",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession3DuplicateAndReentrantReleaseTest::RunTest(const FString& Parameters)
{
    FDiscGolfRHBHThrowTransaction Transaction;
    int32 AuthoritativeLaunchRequests = 0;
    int32 ReentrantLaunchRequests = 0;
    TestTrue(TEXT("Transaction begins"), Transaction.Begin(MakeSession3Command()));

    TestTrue(TEXT("First release commits"), Transaction.TryCommitRelease(
        [&Transaction, &AuthoritativeLaunchRequests, &ReentrantLaunchRequests](const FThrowCommand&)
        {
            ++AuthoritativeLaunchRequests;
            Transaction.TryCommitRelease(
                [&ReentrantLaunchRequests](const FThrowCommand&)
                {
                    ++ReentrantLaunchRequests;
                });
        }));

    TestFalse(TEXT("Later duplicate release is rejected"), Transaction.TryCommitRelease(
        [&AuthoritativeLaunchRequests](const FThrowCommand&)
        {
            ++AuthoritativeLaunchRequests;
        }));
    TestEqual(TEXT("Only the first callback reaches launch authority"), AuthoritativeLaunchRequests, 1);
    TestEqual(TEXT("Re-entrant callback cannot reach launch authority"), ReentrantLaunchRequests, 0);
    TestEqual(TEXT("Lifetime release count records one commit"), Transaction.GetTotalReleaseCommitCount(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession3CancelBeforeReleaseTest,
    "DiscGolfTour.Character.Session3.ThrowTransaction.CancelBeforeRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession3CancelBeforeReleaseTest::RunTest(const FString& Parameters)
{
    FDiscGolfRHBHThrowTransaction Transaction;
    int32 AuthoritativeLaunchRequests = 0;
    TestTrue(TEXT("Transaction begins"), Transaction.Begin(MakeSession3Command()));
    TestTrue(TEXT("Pre-release cancellation succeeds"), Transaction.CancelBeforeRelease());

    TestFalse(TEXT("A stale release after cancellation is rejected"), Transaction.TryCommitRelease(
        [&AuthoritativeLaunchRequests](const FThrowCommand&)
        {
            ++AuthoritativeLaunchRequests;
        }));
    TestEqual(TEXT("Cancellation launches zero gameplay discs"), AuthoritativeLaunchRequests, 0);
    TestEqual(TEXT("Cancellation commits zero releases"), Transaction.GetTotalReleaseCommitCount(), 0);
    TestFalse(TEXT("Cancellation clears the animation transaction lock"), Transaction.IsActive());
    TestTrue(TEXT("Cancellation records its recovery reason"),
        Transaction.GetRecoveryReason() == EDiscGolfRHBHThrowRecoveryReason::CancelledBeforeRelease);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession3RecoveryTest,
    "DiscGolfTour.Character.Session3.ThrowTransaction.FinishAndWatchdogRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession3RecoveryTest::RunTest(const FString& Parameters)
{
    FDiscGolfRHBHThrowTransaction Normal;
    TestTrue(TEXT("Normal attempt begins"), Normal.Begin(MakeSession3Command()));
    TestTrue(TEXT("Normal attempt releases"), Normal.TryCommitRelease([](const FThrowCommand&) {}));
    TestTrue(TEXT("Throw Finished recovers the normal attempt"), Normal.Finish());
    TestFalse(TEXT("Normal finish clears the transaction lock"), Normal.IsActive());
    TestTrue(TEXT("Normal finish reason is retained"),
        Normal.GetRecoveryReason() == EDiscGolfRHBHThrowRecoveryReason::ThrowFinished);
    TestFalse(TEXT("Duplicate Throw Finished is idempotently rejected"), Normal.Finish());

    FDiscGolfRHBHThrowTransaction MissingReleaseNotify;
    TestTrue(TEXT("Missing-release fixture begins"), MissingReleaseNotify.Begin(MakeSession3Command()));
    TestTrue(TEXT("Watchdog recovers a held disc without launching"), MissingReleaseNotify.RecoverFromWatchdog());
    TestEqual(TEXT("Pre-release watchdog commits zero releases"), MissingReleaseNotify.GetTotalReleaseCommitCount(), 0);
    TestFalse(TEXT("Pre-release watchdog clears the transaction lock"), MissingReleaseNotify.IsActive());
    TestTrue(TEXT("Pre-release watchdog reason is retained"), MissingReleaseNotify.GetRecoveryReason()
        == EDiscGolfRHBHThrowRecoveryReason::WatchdogBeforeRelease);

    FDiscGolfRHBHThrowTransaction EarlyFinishNotify;
    TestTrue(TEXT("Early-finish fixture begins"), EarlyFinishNotify.Begin(MakeSession3Command()));
    TestTrue(TEXT("Throw Finished before release safely recovers"), EarlyFinishNotify.Finish());
    TestEqual(TEXT("Early Throw Finished commits zero releases"), EarlyFinishNotify.GetTotalReleaseCommitCount(), 0);
    TestFalse(TEXT("Early Throw Finished clears the transaction lock"), EarlyFinishNotify.IsActive());
    TestTrue(TEXT("Early Throw Finished reason is retained"), EarlyFinishNotify.GetRecoveryReason()
        == EDiscGolfRHBHThrowRecoveryReason::ThrowFinishedBeforeRelease);

    FDiscGolfRHBHThrowTransaction InterruptedBeforeRelease;
    TestTrue(TEXT("Pre-release interruption fixture begins"), InterruptedBeforeRelease.Begin(MakeSession3Command()));
    TestTrue(TEXT("Pre-release interruption safely recovers"), InterruptedBeforeRelease.RecoverFromInterruption());
    TestEqual(TEXT("Pre-release interruption commits zero releases"),
        InterruptedBeforeRelease.GetTotalReleaseCommitCount(), 0);
    TestTrue(TEXT("Pre-release interruption keeps its distinct reason"),
        InterruptedBeforeRelease.GetRecoveryReason()
            == EDiscGolfRHBHThrowRecoveryReason::InterruptedBeforeRelease);

    FDiscGolfRHBHThrowTransaction MissingFinishNotify;
    TestTrue(TEXT("Missing-finish fixture begins"), MissingFinishNotify.Begin(MakeSession3Command()));
    TestTrue(TEXT("Missing-finish fixture releases"), MissingFinishNotify.TryCommitRelease([](const FThrowCommand&) {}));
    TestTrue(TEXT("Watchdog recovers after an already committed release"), MissingFinishNotify.RecoverFromWatchdog());
    TestEqual(TEXT("Post-release watchdog never requests a replacement"), MissingFinishNotify.GetTotalReleaseCommitCount(), 1);
    TestFalse(TEXT("Post-release watchdog clears the transaction lock"), MissingFinishNotify.IsActive());
    TestTrue(TEXT("Post-release watchdog reason is retained"), MissingFinishNotify.GetRecoveryReason()
        == EDiscGolfRHBHThrowRecoveryReason::WatchdogAfterRelease);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession3CommandPassThroughTest,
    "DiscGolfTour.Character.Session3.ThrowTransaction.CachedCommandPassThrough",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession3CommandPassThroughTest::RunTest(const FString& Parameters)
{
    FDiscGolfRHBHThrowTransaction Transaction;
    const FThrowCommand Expected = MakeSession3Command();
    FThrowCommand Received;
    bool bLaunchPathCalled = false;

    TestTrue(TEXT("Transaction snapshots the authoritative command"), Transaction.Begin(Expected));
    FThrowCommand Replacement = Expected;
    Replacement.Power01 = 0.99f;
    Replacement.HyzerDeg = 24.0f;
    TestFalse(TEXT("An active attempt cannot overwrite its cached command"), Transaction.Begin(Replacement));
    TestTrue(TEXT("Release calls the launch adapter once"), Transaction.TryCommitRelease(
        [&Received, &bLaunchPathCalled](const FThrowCommand& CachedCommand)
        {
            bLaunchPathCalled = true;
            Received = CachedCommand;
        }));

    TestTrue(TEXT("Existing authoritative launch path receives a request"), bLaunchPathCalled);
    TestCommandIsExact(*this, Received, Expected);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession3StaleAttemptTest,
    "DiscGolfTour.Character.Session3.ThrowTransaction.StaleAttemptRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession3StaleAttemptTest::RunTest(const FString& Parameters)
{
    FDiscGolfRHBHThrowTransaction Transaction;
    int32 AuthoritativeLaunchRequests = 0;
    TestTrue(TEXT("First attempt begins"), Transaction.Begin(MakeSession3Command()));
    const uint64 FirstAttempt = Transaction.GetAttemptSerial();
    TestTrue(TEXT("First attempt cancels"), Transaction.CancelBeforeRelease(FirstAttempt));

    TestTrue(TEXT("Second attempt begins after recovery"), Transaction.Begin(MakeSession3Command()));
    const uint64 SecondAttempt = Transaction.GetAttemptSerial();
    TestTrue(TEXT("Attempt serial advances"), SecondAttempt > FirstAttempt);
    TestFalse(TEXT("Release captured by the prior attempt is rejected"), Transaction.TryCommitRelease(
        FirstAttempt,
        [&AuthoritativeLaunchRequests](const FThrowCommand&)
        {
            ++AuthoritativeLaunchRequests;
        }));
    TestTrue(TEXT("Current attempt release is accepted"), Transaction.TryCommitRelease(
        SecondAttempt,
        [&AuthoritativeLaunchRequests](const FThrowCommand&)
        {
            ++AuthoritativeLaunchRequests;
        }));
    TestEqual(TEXT("Only the current attempt reaches launch authority"), AuthoritativeLaunchRequests, 1);
    TestFalse(TEXT("Stale finish callback cannot recover the current attempt"), Transaction.Finish(FirstAttempt));
    TestTrue(TEXT("Current finish callback recovers the current attempt"), Transaction.Finish(SecondAttempt));

    FDiscGolfRHBHThrowTransaction InterruptedAfterRelease;
    TestTrue(TEXT("Interrupted fixture begins"), InterruptedAfterRelease.Begin(MakeSession3Command()));
    const uint64 InterruptedAttempt = InterruptedAfterRelease.GetAttemptSerial();
    TestTrue(TEXT("Interrupted fixture commits one release"), InterruptedAfterRelease.TryCommitRelease(
        InterruptedAttempt, [](const FThrowCommand&) {}));
    TestTrue(TEXT("Current interruption recovers after release"),
        InterruptedAfterRelease.RecoverFromInterruption(InterruptedAttempt));
    TestTrue(TEXT("Released interruption keeps its distinct reason"),
        InterruptedAfterRelease.GetRecoveryReason()
            == EDiscGolfRHBHThrowRecoveryReason::InterruptedAfterRelease);
    TestEqual(TEXT("Released interruption never creates a replacement"),
        InterruptedAfterRelease.GetTotalReleaseCommitCount(), 1);
    return true;
}

#endif
