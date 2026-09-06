#include "Misc/AutomationTest.h"

#include "../DiscBagComponent.h"
#include "DiscGolfPlayabilityLibrary.h"
#include "DiscGolfPlayabilityMonitorComponent.h"
#include "../DiscGolfSession16CorePlayabilityContract.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
const FDateTime StartedUtc(2026, 8, 24, 12, 0, 0);
const FDateTime FinishedUtc(2026, 8, 24, 12, 1, 0);

TArray<FDiscGolfSession16ObservedCheck> PassingObservations()
{
    TArray<FDiscGolfSession16ObservedCheck> Observations;
    for (const FDiscGolfSession16RequiredCheck& Required
         : DiscGolfSession16CorePlayabilityContract::RequiredChecks())
    {
        FDiscGolfSession16ObservedCheck Observation;
        Observation.CheckId = Required.CheckId;
        Observation.Status = EDGPlayabilityStatus::Passed;
        Observation.Evidence = FString::Printf(
            TEXT("Observed existing authority for %s"),
            *Required.CheckId.ToString());
        Observation.DurationSeconds = 0.25f;
        Observations.Add(MoveTemp(Observation));
    }
    return Observations;
}

bool BuildPassingReport(FDGPlayabilityGateReport& OutReport, FString& OutError)
{
    return DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
        PassingObservations(),
        StartedUtc,
        FinishedUtc,
        OutReport,
        OutError);
}

const FDGPlayabilityCheckResult* FindResult(
    const FDGPlayabilityGateReport& Report,
    FName CheckId)
{
    return Report.Results.FindByPredicate(
        [CheckId](const FDGPlayabilityCheckResult& Result)
        {
            return Result.CheckId == CheckId;
        });
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession16ExactCatalogTest,
    "DiscGolfTour.Session16.Catalog.ExactRequiredChecks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession16ExactCatalogTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TArray<FDiscGolfSession16RequiredCheck>& Checks =
        DiscGolfSession16CorePlayabilityContract::RequiredChecks();
    TestEqual(TEXT("exact prepared check count"), Checks.Num(), 44);
    const TArray<FName> ExpectedIds = {
        TEXT("boot_game"), TEXT("main_menu_available"),
        TEXT("player_profile_available"), TEXT("course_load"),
        TEXT("spawn_at_tee"), TEXT("bag_available"), TEXT("select_disc"),
        TEXT("enter_aim"), TEXT("start_throw"), TEXT("release_notify_fires"),
        TEXT("disc_launches"), TEXT("physics_advances"),
        TEXT("disc_settles_or_holes_out"), TEXT("lie_updates"),
        TEXT("next_throw_available"), TEXT("ob_penalty_if_triggered"),
        TEXT("water_penalty_if_triggered"), TEXT("basket_detects_completion"),
        TEXT("score_updates"), TEXT("hole_completes"), TEXT("pause_resume"),
        TEXT("camera_recovers"), TEXT("input_context_recovers"),
        TEXT("replay_exits_cleanly"), TEXT("no_duplicate_player"),
        TEXT("no_duplicate_disc"), TEXT("no_soft_lock"),
        TEXT("advance_to_next_hole"), TEXT("spawn_next_hole"),
        TEXT("scorecard_persists_between_holes"),
        TEXT("multiple_holes_complete"), TEXT("round_completes"),
        TEXT("results_screen_available"), TEXT("return_to_menu_or_continue"),
        TEXT("save_player"), TEXT("save_bag"), TEXT("save_settings"),
        TEXT("save_round_or_career"), TEXT("load_save"),
        TEXT("loaded_character_matches"), TEXT("loaded_bag_matches"),
        TEXT("loaded_settings_match"), TEXT("missing_cosmetic_falls_back"),
        TEXT("missing_optional_asset_does_not_block_play"),
    };
    for (int32 Index = 0; Index < ExpectedIds.Num(); ++Index)
    {
        TestTrue(TEXT("prepared id and order match build kit"),
            Checks[Index].CheckId == ExpectedIds[Index]);
    }

    const int32 Smoke = Checks.FilterByPredicate([](const auto& Check)
    {
        return Check.GateLevel == EDGPlayabilityGateLevel::Smoke;
    }).Num();
    const int32 CoreLoop = Checks.FilterByPredicate([](const auto& Check)
    {
        return Check.GateLevel == EDGPlayabilityGateLevel::CoreLoop;
    }).Num();
    const int32 Round = Checks.FilterByPredicate([](const auto& Check)
    {
        return Check.GateLevel == EDGPlayabilityGateLevel::Round;
    }).Num();
    const int32 Persistence = Checks.FilterByPredicate([](const auto& Check)
    {
        return Check.GateLevel == EDGPlayabilityGateLevel::Persistence;
    }).Num();
    TestEqual(TEXT("Smoke count"), Smoke, 11);
    TestEqual(TEXT("CoreLoop count"), CoreLoop, 16);
    TestEqual(TEXT("Round count"), Round, 7);
    TestEqual(TEXT("Persistence count"), Persistence, 10);

    TSet<FName> CheckIds;
    for (const FDiscGolfSession16RequiredCheck& Check : Checks)
    {
        TestFalse(TEXT("check id is populated"), Check.CheckId.IsNone());
        TestTrue(TEXT("check ids are unique"), !CheckIds.Contains(Check.CheckId));
        CheckIds.Add(Check.CheckId);
        TestTrue(
            TEXT("mapped failure is not None"),
            Check.FailureCode != EDGPlayabilityFailureCode::None);
    }
    TestTrue(TEXT("first prepared id"), Checks[0].CheckId == TEXT("boot_game"));
    TestTrue(
        TEXT("last prepared id"),
        Checks.Last().CheckId == TEXT("missing_optional_asset_does_not_block_play"));

    const TArray<EDGPlayabilityFailureCode>& FailureCodes =
        DiscGolfSession16CorePlayabilityContract::FailureCodes();
    TestEqual(TEXT("complete failure-code count"), FailureCodes.Num(), 44);
    TSet<uint8> UniqueFailureCodes;
    for (EDGPlayabilityFailureCode FailureCode : FailureCodes)
    {
        TestTrue(TEXT("failure code excludes None"), FailureCode != EDGPlayabilityFailureCode::None);
        UniqueFailureCodes.Add(static_cast<uint8>(FailureCode));
    }
    TestEqual(TEXT("failure codes are unique"), UniqueFailureCodes.Num(), 44);
    return true;
}

#define DG_IMPLEMENT_GATE_PASS_TEST(ClassName, TestPath, Gate) \
IMPLEMENT_SIMPLE_AUTOMATION_TEST( \
    ClassName, TestPath, \
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter) \
bool ClassName::RunTest(const FString& Parameters) \
{ \
    (void)Parameters; \
    FDGPlayabilityGateReport Report; \
    FString Error; \
    TestTrue(TEXT("complete observations build"), BuildPassingReport(Report, Error)); \
    TestTrue(TEXT("report validates"), \
        DiscGolfSession16CorePlayabilityContract::ValidateReport(Report, Error)); \
    TestTrue(TEXT("requested gate passes"), \
        UDiscGolfPlayabilityLibrary::GatePassed(Report, Gate)); \
    TestEqual(TEXT("all checks passed"), Report.PassedChecks, 44); \
    TestEqual(TEXT("no failed checks"), Report.FailedChecks, 0); \
    TestEqual(TEXT("report passed"), Report.OverallStatus, EDGPlayabilityStatus::Passed); \
    return true; \
}

DG_IMPLEMENT_GATE_PASS_TEST(
    FDiscGolfSession16SmokePassTest,
    "DiscGolfTour.Session16.Gates.SmokePass",
    EDGPlayabilityGateLevel::Smoke)

DG_IMPLEMENT_GATE_PASS_TEST(
    FDiscGolfSession16CoreLoopPassTest,
    "DiscGolfTour.Session16.Gates.CoreLoopPass",
    EDGPlayabilityGateLevel::CoreLoop)

DG_IMPLEMENT_GATE_PASS_TEST(
    FDiscGolfSession16RoundPassTest,
    "DiscGolfTour.Session16.Gates.RoundPass",
    EDGPlayabilityGateLevel::Round)

DG_IMPLEMENT_GATE_PASS_TEST(
    FDiscGolfSession16PersistencePassTest,
    "DiscGolfTour.Session16.Gates.PersistencePass",
    EDGPlayabilityGateLevel::Persistence)

#undef DG_IMPLEMENT_GATE_PASS_TEST

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession16RejectIncompleteEvidenceTest,
    "DiscGolfTour.Session16.Gates.RejectIncompleteEvidence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession16RejectIncompleteEvidenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDGPlayabilityGateReport Empty = UDiscGolfPlayabilityLibrary::BuildReport(
        {}, StartedUtc, FinishedUtc);
    TestEqual(TEXT("empty report is NotRun"), Empty.OverallStatus, EDGPlayabilityStatus::NotRun);
    TestFalse(
        TEXT("empty report cannot pass"),
        UDiscGolfPlayabilityLibrary::GatePassed(Empty, EDGPlayabilityGateLevel::Smoke));

    for (EDGPlayabilityStatus IncompleteStatus
         : {EDGPlayabilityStatus::NotRun, EDGPlayabilityStatus::Running})
    {
        TArray<FDiscGolfSession16ObservedCheck> Observations = PassingObservations();
        Observations[0].Status = IncompleteStatus;
        FDGPlayabilityGateReport Report;
        FString Error;
        TestTrue(TEXT("well-formed incomplete report builds"),
            DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
                Observations, StartedUtc, FinishedUtc, Report, Error));
        TestFalse(TEXT("incomplete report cannot pass Smoke"),
            UDiscGolfPlayabilityLibrary::GatePassed(
                Report, EDGPlayabilityGateLevel::Smoke));
    }

    TArray<FDiscGolfSession16ObservedCheck> Missing = PassingObservations();
    Missing.Pop();
    FDGPlayabilityGateReport Sentinel;
    Sentinel.RunId = FGuid(1, 2, 3, 4);
    FString Error;
    TestFalse(TEXT("missing observation rejected"),
        DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
            Missing, StartedUtc, FinishedUtc, Sentinel, Error));
    TestTrue(TEXT("failed build leaves destination unchanged"),
        Sentinel.RunId == FGuid(1, 2, 3, 4));

    TArray<FDiscGolfSession16ObservedCheck> EmptyEvidence = PassingObservations();
    EmptyEvidence[3].Evidence.Reset();
    TestFalse(TEXT("empty per-check evidence rejected"),
        DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
            EmptyEvidence, StartedUtc, FinishedUtc, Sentinel, Error));

    TArray<FDiscGolfSession16ObservedCheck> Duplicate = PassingObservations();
    Duplicate.Last().CheckId = Duplicate[0].CheckId;
    TestFalse(TEXT("duplicate observation rejected"),
        DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
            Duplicate, StartedUtc, FinishedUtc, Sentinel, Error));

    TArray<FDiscGolfSession16ObservedCheck> Unknown = PassingObservations();
    Unknown.Last().CheckId = TEXT("unregistered_gate_claim");
    TestFalse(TEXT("unknown observation rejected"),
        DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
            Unknown, StartedUtc, FinishedUtc, Sentinel, Error));

    FDGPlayabilityGateReport PassingReport;
    TestTrue(TEXT("passing fixture builds"), BuildPassingReport(PassingReport, Error));
    TestFalse(TEXT("invalid through-level enum rejected"),
        UDiscGolfPlayabilityLibrary::GatePassed(
            PassingReport,
            static_cast<EDGPlayabilityGateLevel>(255)));

    FDGPlayabilityGateReport InvalidStatusReport = PassingReport;
    InvalidStatusReport.Results[0].Status =
        static_cast<EDGPlayabilityStatus>(255);
    --InvalidStatusReport.PassedChecks;
    TestFalse(TEXT("invalid result-status enum rejected even with coherent counters"),
        UDiscGolfPlayabilityLibrary::GatePassed(
            InvalidStatusReport,
            EDGPlayabilityGateLevel::Persistence));
    const FDGPlayabilityGateReport RebuiltInvalidStatus =
        UDiscGolfPlayabilityLibrary::BuildReport(
            InvalidStatusReport.Results,
            StartedUtc,
            FinishedUtc);
    TestEqual(TEXT("invalid result-status enum makes a rebuilt report fail"),
        RebuiltInvalidStatus.OverallStatus,
        EDGPlayabilityStatus::Failed);

    PassingReport.PassedChecks = 0;
    TestFalse(TEXT("contradictory report counters rejected by gate"),
        UDiscGolfPlayabilityLibrary::GatePassed(
            PassingReport,
            EDGPlayabilityGateLevel::Persistence));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession16FailureReportSemanticsTest,
    "DiscGolfTour.Session16.Contract.FailureAndReportSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession16FailureReportSemanticsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDiscGolfSession16ObservedCheck> Observations = PassingObservations();
    FDiscGolfSession16ObservedCheck* Score = Observations.FindByPredicate(
        [](const FDiscGolfSession16ObservedCheck& Observation)
        {
            return Observation.CheckId == TEXT("score_updates");
        });
    TestNotNull(TEXT("score observation exists"), Score);
    if (!Score)
    {
        return false;
    }
    Score->Status = EDGPlayabilityStatus::Failed;
    Score->Evidence = TEXT("Authoritative round score did not change after the caught throw");

    FDGPlayabilityGateReport Report;
    FString Error;
    TestTrue(TEXT("failed observations still produce a truthful report"),
        DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
            Observations, StartedUtc, FinishedUtc, Report, Error));
    const FDGPlayabilityCheckResult* ScoreResult = FindResult(Report, TEXT("score_updates"));
    TestNotNull(TEXT("score result exists"), ScoreResult);
    if (!ScoreResult)
    {
        return false;
    }
    TestEqual(TEXT("contract assigns specific failure"),
        ScoreResult->FailureCode, EDGPlayabilityFailureCode::ScoreUpdateFailed);
    TestEqual(TEXT("one failure counted"), Report.FailedChecks, 1);
    TestEqual(TEXT("one blocking failure counted"), Report.BlockingFailures, 1);
    TestEqual(TEXT("overall failure"), Report.OverallStatus, EDGPlayabilityStatus::Failed);
    TestFalse(TEXT("CoreLoop is blocked"),
        UDiscGolfPlayabilityLibrary::GatePassed(
            Report, EDGPlayabilityGateLevel::CoreLoop));
    TestTrue(TEXT("truthful failure report validates"),
        DiscGolfSession16CorePlayabilityContract::ValidateReport(Report, Error));

    const int32 ScoreIndex = Report.Results.IndexOfByPredicate(
        [](const FDGPlayabilityCheckResult& Result)
        {
            return Result.CheckId == TEXT("score_updates");
        });
    Report.Results[ScoreIndex].FailureCode = EDGPlayabilityFailureCode::UnexpectedError;
    TestFalse(TEXT("spoofed generic failure metadata rejected"),
        DiscGolfSession16CorePlayabilityContract::ValidateReport(Report, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession16WatchdogLifecycleTest,
    "DiscGolfTour.Session16.Watchdog.ExplicitLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession16WatchdogLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfPlayabilityMonitorComponent* Monitor =
        NewObject<UDiscGolfPlayabilityMonitorComponent>();
    TestNotNull(TEXT("monitor constructed"), Monitor);
    if (!Monitor)
    {
        return false;
    }

    Monitor->AdvanceMonitoring(100.0f);
    TestFalse(TEXT("monitor is opt-in"), Monitor->IsMonitoring());
    TestEqual(TEXT("inactive time does not accrue"), Monitor->SecondsSinceProgress, 0.0f);

    Monitor->StartMonitoring(TEXT("DiscInFlight"), 1.0f);
    TestTrue(TEXT("start activates monitor"), Monitor->IsMonitoring());
    Monitor->AdvanceMonitoring(0.75f);
    Monitor->SuspendMonitoring(TEXT("Paused"));
    TestFalse(TEXT("suspended state is not monitored"), Monitor->IsMonitoring());
    Monitor->AdvanceMonitoring(100.0f);
    TestEqual(TEXT("suspended state does not accrue"), Monitor->SecondsSinceProgress, 0.75f);
    TestFalse(TEXT("suspension cannot report"), Monitor->HasReportedSoftLock());

    Monitor->ResumeMonitoring(TEXT("DiscInFlight"));
    TestTrue(TEXT("resume reactivates monitor"), Monitor->IsMonitoring());
    TestEqual(TEXT("resume gets a fresh context window"), Monitor->SecondsSinceProgress, 0.0f);
    Monitor->AdvanceMonitoring(1.01f);
    TestTrue(TEXT("timeout reports soft lock"), Monitor->HasReportedSoftLock());
    const float TimeAtReport = Monitor->SecondsSinceProgress;
    Monitor->AdvanceMonitoring(100.0f);
    TestEqual(TEXT("watchdog is one-shot until progress"),
        Monitor->SecondsSinceProgress, TimeAtReport);

    Monitor->MarkProgress(TEXT("DiscResolved"));
    TestFalse(TEXT("progress rearms watchdog"), Monitor->HasReportedSoftLock());
    Monitor->AdvanceMonitoring(1.01f);
    TestTrue(TEXT("rearmed watchdog can report a later lock"), Monitor->HasReportedSoftLock());
    Monitor->StopMonitoring(TEXT("LieEstablished"));
    TestFalse(TEXT("stop disables monitoring"), Monitor->IsMonitoring());
    Monitor->ResetMonitor();
    TestFalse(TEXT("reset clears one-shot"), Monitor->HasReportedSoftLock());
    TestEqual(TEXT("reset state"), Monitor->CurrentStateName, FString(TEXT("Unknown")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession16OptionalAssetFallbackTest,
    "DiscGolfTour.Session16.Persistence.OptionalAssetFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession16OptionalAssetFallbackTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FString MissingPackage =
        TEXT("/Game/Optional/Premium/DA_DG_IntentionallyAbsentPremiumVisual");
    TestFalse(TEXT("optional premium fixture is genuinely absent"),
        FPackageName::DoesPackageExist(MissingPackage));

    const FDGDiscBagLoadout FallbackBag = UDiscBagComponent::BuildDefaultLoadout();
    TestTrue(TEXT("generic fallback bag remains playable"), FallbackBag.Discs.Num() > 0);

    TArray<FDiscGolfSession16ObservedCheck> Observations = PassingObservations();
    FDiscGolfSession16ObservedCheck* Optional = Observations.FindByPredicate(
        [](const FDiscGolfSession16ObservedCheck& Observation)
        {
            return Observation.CheckId
                == TEXT("missing_optional_asset_does_not_block_play");
        });
    TestNotNull(TEXT("optional-asset observation exists"), Optional);
    if (!Optional)
    {
        return false;
    }
    Optional->Evidence = FString::Printf(
        TEXT("Optional package %s absent; generic bag retained %d playable discs"),
        *MissingPackage,
        FallbackBag.Discs.Num());

    FDGPlayabilityGateReport Report;
    FString Error;
    TestTrue(TEXT("absence evidence builds"),
        DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
            Observations, StartedUtc, FinishedUtc, Report, Error));
    TestTrue(TEXT("optional absence does not block Persistence"),
        UDiscGolfPlayabilityLibrary::GatePassed(
            Report, EDGPlayabilityGateLevel::Persistence));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
