#include "DiscGolfSession16CorePlayabilityContract.h"

#include "DiscGolfPlayabilityLibrary.h"

namespace
{
using FRequired = FDiscGolfSession16RequiredCheck;

const TArray<FRequired> RequiredCatalog = {
    // Smoke (11)
    {TEXT("boot_game"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::BootFailed, 30.0f},
    {TEXT("main_menu_available"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::MainMenuUnavailable},
    {TEXT("player_profile_available"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::PlayerProfileUnavailable},
    {TEXT("course_load"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::CourseLoadFailed, 30.0f},
    {TEXT("spawn_at_tee"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::HoleSpawnFailed},
    {TEXT("bag_available"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::BagUnavailable},
    {TEXT("select_disc"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::DiscSelectionFailed},
    {TEXT("enter_aim"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::AimStateUnavailable},
    {TEXT("start_throw"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::ThrowCouldNotStart},
    {TEXT("release_notify_fires"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::ReleaseNotifyMissing},
    {TEXT("disc_launches"), EDGPlayabilityGateLevel::Smoke, EDGPlayabilityFailureCode::DiscLaunchFailed, 5.0f},

    // CoreLoop (16)
    {TEXT("physics_advances"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::PhysicsDidNotAdvance},
    {TEXT("disc_settles_or_holes_out"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::DiscNeverSettled, 30.0f},
    {TEXT("lie_updates"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::LieNotUpdated},
    {TEXT("next_throw_available"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::PlayerCouldNotContinue},
    {TEXT("ob_penalty_if_triggered"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::OutOfBoundsRuleFailed},
    {TEXT("water_penalty_if_triggered"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::WaterRuleFailed},
    {TEXT("basket_detects_completion"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::BasketDetectionFailed},
    {TEXT("score_updates"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::ScoreUpdateFailed},
    {TEXT("hole_completes"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::HoleCompletionFailed, 180.0f},
    {TEXT("pause_resume"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::ResumeFailed},
    {TEXT("camera_recovers"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::CameraStateStuck},
    {TEXT("input_context_recovers"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::InputContextStuck},
    {TEXT("replay_exits_cleanly"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::ReplayStateStuck},
    {TEXT("no_duplicate_player"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::DuplicatePlayerDetected},
    {TEXT("no_duplicate_disc"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::DuplicateDiscDetected},
    {TEXT("no_soft_lock"), EDGPlayabilityGateLevel::CoreLoop, EDGPlayabilityFailureCode::SoftLockDetected},

    // Round (7)
    {TEXT("advance_to_next_hole"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::NextHoleFailed},
    {TEXT("spawn_next_hole"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::HoleSpawnFailed},
    {TEXT("scorecard_persists_between_holes"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::ScoreUpdateFailed},
    {TEXT("multiple_holes_complete"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::HoleCompletionFailed},
    {TEXT("round_completes"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::RoundCompletionFailed, 1800.0f},
    {TEXT("results_screen_available"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::ResultsScreenFailed},
    {TEXT("return_to_menu_or_continue"), EDGPlayabilityGateLevel::Round, EDGPlayabilityFailureCode::PlayerCouldNotContinue},

    // Persistence (10)
    {TEXT("save_player"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveFailed},
    {TEXT("save_bag"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveFailed},
    {TEXT("save_settings"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveFailed},
    {TEXT("save_round_or_career"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveFailed},
    {TEXT("load_save"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::LoadFailed},
    {TEXT("loaded_character_matches"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveMismatch},
    {TEXT("loaded_bag_matches"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveMismatch},
    {TEXT("loaded_settings_match"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::SaveMismatch},
    {TEXT("missing_cosmetic_falls_back"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::InvalidCosmeticRecoveryFailed},
    {TEXT("missing_optional_asset_does_not_block_play"), EDGPlayabilityGateLevel::Persistence, EDGPlayabilityFailureCode::MissingAssetRecoveryFailed},
};

const TArray<EDGPlayabilityFailureCode> FailureCatalog = {
    EDGPlayabilityFailureCode::BootFailed,
    EDGPlayabilityFailureCode::MainMenuUnavailable,
    EDGPlayabilityFailureCode::PlayerProfileUnavailable,
    EDGPlayabilityFailureCode::CourseLoadFailed,
    EDGPlayabilityFailureCode::HoleSpawnFailed,
    EDGPlayabilityFailureCode::TeeStateInvalid,
    EDGPlayabilityFailureCode::BagUnavailable,
    EDGPlayabilityFailureCode::NoSelectableDisc,
    EDGPlayabilityFailureCode::DiscSelectionFailed,
    EDGPlayabilityFailureCode::AimStateUnavailable,
    EDGPlayabilityFailureCode::ThrowCouldNotStart,
    EDGPlayabilityFailureCode::ReleaseNotifyMissing,
    EDGPlayabilityFailureCode::DiscLaunchFailed,
    EDGPlayabilityFailureCode::PhysicsDidNotAdvance,
    EDGPlayabilityFailureCode::DiscNeverSettled,
    EDGPlayabilityFailureCode::DiscStateInvalid,
    EDGPlayabilityFailureCode::LieNotUpdated,
    EDGPlayabilityFailureCode::LieInvalid,
    EDGPlayabilityFailureCode::PlayerCouldNotContinue,
    EDGPlayabilityFailureCode::PenaltyRuleFailed,
    EDGPlayabilityFailureCode::OutOfBoundsRuleFailed,
    EDGPlayabilityFailureCode::WaterRuleFailed,
    EDGPlayabilityFailureCode::MandoRuleFailed,
    EDGPlayabilityFailureCode::BasketDetectionFailed,
    EDGPlayabilityFailureCode::HoleCompletionFailed,
    EDGPlayabilityFailureCode::ScoreUpdateFailed,
    EDGPlayabilityFailureCode::NextHoleFailed,
    EDGPlayabilityFailureCode::PauseFailed,
    EDGPlayabilityFailureCode::ResumeFailed,
    EDGPlayabilityFailureCode::CameraStateStuck,
    EDGPlayabilityFailureCode::InputContextStuck,
    EDGPlayabilityFailureCode::ReplayStateStuck,
    EDGPlayabilityFailureCode::DuplicatePlayerDetected,
    EDGPlayabilityFailureCode::DuplicateDiscDetected,
    EDGPlayabilityFailureCode::RoundCompletionFailed,
    EDGPlayabilityFailureCode::ResultsScreenFailed,
    EDGPlayabilityFailureCode::SaveFailed,
    EDGPlayabilityFailureCode::LoadFailed,
    EDGPlayabilityFailureCode::SaveMismatch,
    EDGPlayabilityFailureCode::MissingAssetRecoveryFailed,
    EDGPlayabilityFailureCode::InvalidCosmeticRecoveryFailed,
    EDGPlayabilityFailureCode::SoftLockDetected,
    EDGPlayabilityFailureCode::Timeout,
    EDGPlayabilityFailureCode::UnexpectedError,
};

bool IsKnownStatus(EDGPlayabilityStatus Status)
{
    return Status >= EDGPlayabilityStatus::NotRun
        && Status <= EDGPlayabilityStatus::Blocked;
}

bool IsFailureStatus(EDGPlayabilityStatus Status)
{
    return Status == EDGPlayabilityStatus::Failed
        || Status == EDGPlayabilityStatus::Blocked;
}
}

const TArray<FDiscGolfSession16RequiredCheck>&
DiscGolfSession16CorePlayabilityContract::RequiredChecks()
{
    return RequiredCatalog;
}

const TArray<EDGPlayabilityFailureCode>&
DiscGolfSession16CorePlayabilityContract::FailureCodes()
{
    return FailureCatalog;
}

const FDiscGolfSession16RequiredCheck*
DiscGolfSession16CorePlayabilityContract::FindRequiredCheck(FName CheckId)
{
    return RequiredCatalog.FindByPredicate(
        [CheckId](const FRequired& Check)
        {
            return Check.CheckId == CheckId;
        });
}

bool DiscGolfSession16CorePlayabilityContract::BuildReportFromObservations(
    const TArray<FDiscGolfSession16ObservedCheck>& Observations,
    FDateTime StartedUtc,
    FDateTime FinishedUtc,
    FDGPlayabilityGateReport& OutReport,
    FString& OutError)
{
    if (Observations.Num() != RequiredCheckCount)
    {
        OutError = FString::Printf(
            TEXT("Session 16 requires exactly %d observations; received %d"),
            RequiredCheckCount,
            Observations.Num());
        return false;
    }
    if (StartedUtc.GetTicks() <= 0 || FinishedUtc < StartedUtc)
    {
        OutError = TEXT("Session 16 report timestamps are invalid");
        return false;
    }

    TMap<FName, const FDiscGolfSession16ObservedCheck*> ById;
    for (const FDiscGolfSession16ObservedCheck& Observation : Observations)
    {
        if (Observation.CheckId.IsNone()
            || !IsKnownStatus(Observation.Status)
            || Observation.Evidence.TrimStartAndEnd().IsEmpty()
            || !FMath::IsFinite(Observation.DurationSeconds)
            || Observation.DurationSeconds < 0.0f)
        {
            OutError = FString::Printf(
                TEXT("Session 16 observation '%s' is malformed or lacks evidence"),
                *Observation.CheckId.ToString());
            return false;
        }
        if (!FindRequiredCheck(Observation.CheckId))
        {
            OutError = FString::Printf(
                TEXT("Session 16 observation uses unknown check id '%s'"),
                *Observation.CheckId.ToString());
            return false;
        }
        if (ById.Contains(Observation.CheckId))
        {
            OutError = FString::Printf(
                TEXT("Session 16 observation duplicates check id '%s'"),
                *Observation.CheckId.ToString());
            return false;
        }
        ById.Add(Observation.CheckId, &Observation);
    }

    TArray<FDGPlayabilityCheckResult> Results;
    Results.Reserve(RequiredCatalog.Num());
    for (const FRequired& Required : RequiredCatalog)
    {
        const FDiscGolfSession16ObservedCheck* const* Found = ById.Find(Required.CheckId);
        if (!Found)
        {
            OutError = FString::Printf(
                TEXT("Session 16 observation is missing check id '%s'"),
                *Required.CheckId.ToString());
            return false;
        }

        const FDiscGolfSession16ObservedCheck& Observation = **Found;
        FDGPlayabilityCheckResult Result;
        Result.CheckId = Required.CheckId;
        Result.GateLevel = Required.GateLevel;
        Result.Status = Observation.Status;
        Result.FailureCode = IsFailureStatus(Observation.Status)
            ? Required.FailureCode
            : EDGPlayabilityFailureCode::None;
        Result.Message = Observation.Evidence;
        Result.DurationSeconds = Observation.DurationSeconds;
        Result.bBlocking = true;
        Results.Add(MoveTemp(Result));
    }

    FDGPlayabilityGateReport Candidate = UDiscGolfPlayabilityLibrary::BuildReport(
        Results,
        StartedUtc,
        FinishedUtc);
    if (!ValidateReport(Candidate, OutError))
    {
        return false;
    }
    OutReport = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool DiscGolfSession16CorePlayabilityContract::ValidateReport(
    const FDGPlayabilityGateReport& Report,
    FString& OutError)
{
    if (!Report.RunId.IsValid()
        || Report.StartedUtc.GetTicks() <= 0
        || Report.FinishedUtc < Report.StartedUtc
        || Report.Results.Num() != RequiredCheckCount)
    {
        OutError = TEXT("Session 16 report identity, time range, or result count is invalid");
        return false;
    }

    int32 PassedChecks = 0;
    int32 FailedChecks = 0;
    int32 BlockingFailures = 0;
    TSet<FName> Seen;
    for (int32 Index = 0; Index < RequiredCatalog.Num(); ++Index)
    {
        const FRequired& Required = RequiredCatalog[Index];
        const FDGPlayabilityCheckResult& Result = Report.Results[Index];
        const EDGPlayabilityFailureCode ExpectedFailure = IsFailureStatus(Result.Status)
            ? Required.FailureCode
            : EDGPlayabilityFailureCode::None;
        if (Result.CheckId != Required.CheckId
            || Result.GateLevel != Required.GateLevel
            || Result.FailureCode != ExpectedFailure
            || !Result.bBlocking
            || Seen.Contains(Result.CheckId)
            || !IsKnownStatus(Result.Status)
            || Result.Message.TrimStartAndEnd().IsEmpty()
            || !FMath::IsFinite(Result.DurationSeconds)
            || Result.DurationSeconds < 0.0f)
        {
            OutError = FString::Printf(
                TEXT("Session 16 report result %d violates required metadata for '%s'"),
                Index,
                *Required.CheckId.ToString());
            return false;
        }
        Seen.Add(Result.CheckId);
        if (Result.Status == EDGPlayabilityStatus::Passed)
        {
            ++PassedChecks;
        }
        if (IsFailureStatus(Result.Status))
        {
            ++FailedChecks;
            ++BlockingFailures;
        }
    }

    EDGPlayabilityStatus ExpectedOverall = EDGPlayabilityStatus::Passed;
    if (BlockingFailures > 0)
    {
        ExpectedOverall = EDGPlayabilityStatus::Failed;
    }
    else if (Report.Results.ContainsByPredicate(
        [](const FDGPlayabilityCheckResult& Result)
        {
            return Result.Status == EDGPlayabilityStatus::Running;
        }))
    {
        ExpectedOverall = EDGPlayabilityStatus::Running;
    }
    else if (Report.Results.ContainsByPredicate(
        [](const FDGPlayabilityCheckResult& Result)
        {
            return Result.Status == EDGPlayabilityStatus::NotRun;
        }))
    {
        ExpectedOverall = EDGPlayabilityStatus::NotRun;
    }

    if (Report.PassedChecks != PassedChecks
        || Report.FailedChecks != FailedChecks
        || Report.BlockingFailures != BlockingFailures
        || Report.OverallStatus != ExpectedOverall)
    {
        OutError = TEXT("Session 16 report counters or overall status are inconsistent");
        return false;
    }

    OutError.Reset();
    return true;
}
