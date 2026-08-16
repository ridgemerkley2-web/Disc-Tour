#include "DiscGolfSession5MocapSmokeRunner.h"

#include "DiscGolferPawn.h"
#include "DiscGolfSession5MocapValidationPaths.h"
#include "DiscGolfTour.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ADiscGolfSession5MocapSmokeRunner::Start()
{
    FParse::Value(FCommandLine::Get(), TEXT("Session4Profile="), RequestedProfile);
    if (RequestedProfile.IsEmpty())
    {
        RequestedProfile = TEXT("Baseline");
    }

    ADiscGolferPawn* Player = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    const FString ExpectedPath = DiscGolfSession5MocapValidation::PipelineTestMontage;
    if (!DiscGolfSession5MocapValidation::IsPipelineRuntimeValidationRequested()
        || !Player
        || !Player->IsSession5PipelineValidationMontageActive()
        || Player->GetActiveRHBHThrowMontagePath() != ExpectedPath)
    {
        Fail(FString::Printf(
            TEXT("pipeline-test montage preflight failed (expected=%s actual=%s)"),
            *ExpectedPath,
            Player ? *Player->GetActiveRHBHThrowMontagePath() : TEXT("NO_PLAYER")));
        return;
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 5 MOCAP PIPELINE SMOKE START: profile=%s montage=%s; delegating the complete accepted one-throw authority contract."),
        *RequestedProfile,
        *Player->GetActiveRHBHThrowMontagePath());
    Super::Start();
}

void ADiscGolfSession5MocapSmokeRunner::Fail(const FString& Reason)
{
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("SESSION 5 MOCAP PIPELINE SMOKE FAIL: profile=%s reason=%s"),
        *RequestedProfile,
        *Reason);
    Super::Fail(Reason);
}

void ADiscGolfSession5MocapSmokeRunner::Pass()
{
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 5 MOCAP PIPELINE SMOKE PASS: profile=%s pipeline_montage=1 cached_FThrowCommand=1 cancel_before_release_zero_discs=1 release_once=1 authoritative_disc=1 existing_flight=1 follow_through=1 recovery=1 next_action=1 default_route_untouched=1."),
        *RequestedProfile);
    Super::Pass();
}
