#include "DiscGolfSession14AISmokeRunner.h"

#include "DiscActor.h"
#include "DiscBagComponent.h"
#include "DiscCatalogSubsystem.h"
#include "DiscFlightComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolfAIPlannerAdapter.h"
#include "DiscGolfCareerSubsystem.h"
#include "DiscGolfCompetitionRuntime.h"
#include "DiscGolfHoleActor.h"
#include "DiscGolfMath.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "WindDirector.h"

namespace
{
constexpr float PreviewTimeoutSeconds = 45.0f;
constexpr float FinalTimeoutSeconds = 45.0f;
constexpr int32 MinimumMeasuredCandidateCount = 3;
const TCHAR* Session14ExternalUserDirParent = TEXT("C:/DGTour_TestRuns/Session14");

int32 CountOccurrencesInsensitive(const FString& Haystack, const FString& Needle)
{
    int32 Count = 0;
    int32 SearchFrom = 0;
    while (Needle.Len() > 0)
    {
        const int32 Found = Haystack.Find(
            Needle, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
        if (Found == INDEX_NONE)
        {
            break;
        }
        ++Count;
        SearchFrom = Found + Needle.Len();
    }
    return Count;
}

bool IsFiniteTelemetry(const FDiscFlightTelemetry& Telemetry)
{
    return FMath::IsFinite(Telemetry.FlightTimeSeconds)
        && FMath::IsFinite(Telemetry.CarryMeters)
        && FMath::IsFinite(Telemetry.GroundDistanceMeters)
        && FMath::IsFinite(Telemetry.SpeedMps)
        && !Telemetry.VelocityMps.ContainsNaN();
}

bool PlannerCandidatesMatch(
    const FDGAIShotCandidate& A,
    const FDGAIShotCandidate& B)
{
    return A.Disc.InstanceId == B.Disc.InstanceId
        && A.Disc.DiscDefinitionId == B.Disc.DiscDefinitionId
        && A.Disc.PlasticId == B.Disc.PlasticId
        && A.ThrowType == B.ThrowType
        && A.Power01 == B.Power01
        && A.HyzerDegrees == B.HyzerDegrees
        && A.NoseDegrees == B.NoseDegrees
        && A.PredictedLandingLocationCm == B.PredictedLandingLocationCm
        && A.ExpectedProgressM == B.ExpectedProgressM
        && A.SuccessProbability == B.SuccessProbability
        && A.OutOfBoundsProbability == B.OutOfBoundsProbability
        && A.ObstacleHitProbability == B.ObstacleHitProbability
        && A.LandingErrorM == B.LandingErrorM;
}

TSharedRef<FJsonObject> VectorJson(const FVector& Value)
{
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetNumberField(TEXT("x"), Value.X);
    Json->SetNumberField(TEXT("y"), Value.Y);
    Json->SetNumberField(TEXT("z"), Value.Z);
    return Json;
}

bool CareerProgressMatches(
    const FDGCareerProgress& A,
    const FDGCareerProgress& B)
{
    if (A.SeasonNumber != B.SeasonNumber
        || A.CareerCurrency != B.CareerCurrency
        || A.PlayerRating != B.PlayerRating
        || A.WorldRank != B.WorldRank
        || A.CompletedEventIds != B.CompletedEventIds
        || A.RoundHistory.Num() != B.RoundHistory.Num()
        || A.Sponsorships.Num() != B.Sponsorships.Num())
    {
        return false;
    }
    for (int32 RoundIndex = 0; RoundIndex < A.RoundHistory.Num(); ++RoundIndex)
    {
        const FDGRoundScorecard& ARound = A.RoundHistory[RoundIndex];
        const FDGRoundScorecard& BRound = B.RoundHistory[RoundIndex];
        if (ARound.EventId != BRound.EventId
            || ARound.CourseId != BRound.CourseId
            || ARound.RoundNumber != BRound.RoundNumber
            || ARound.Holes.Num() != BRound.Holes.Num())
        {
            return false;
        }
        for (int32 HoleIndex = 0; HoleIndex < ARound.Holes.Num(); ++HoleIndex)
        {
            const FDGScorecardHole& AHole = ARound.Holes[HoleIndex];
            const FDGScorecardHole& BHole = BRound.Holes[HoleIndex];
            if (AHole.HoleNumber != BHole.HoleNumber
                || AHole.Par != BHole.Par
                || AHole.Strokes != BHole.Strokes
                || AHole.PenaltyStrokes != BHole.PenaltyStrokes
                || AHole.bComplete != BHole.bComplete)
            {
                return false;
            }
        }
    }
    for (int32 Index = 0; Index < A.Sponsorships.Num(); ++Index)
    {
        const FDGSponsorshipProgress& ASponsorship = A.Sponsorships[Index];
        const FDGSponsorshipProgress& BSponsorship = B.Sponsorships[Index];
        if (ASponsorship.BrandId != BSponsorship.BrandId
            || ASponsorship.Tier != BSponsorship.Tier
            || ASponsorship.Reputation != BSponsorship.Reputation
            || ASponsorship.bActive != BSponsorship.bActive)
        {
            return false;
        }
    }
    return true;
}
}

ADiscGolfSession14AISmokeRunner::ADiscGolfSession14AISmokeRunner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADiscGolfSession14AISmokeRunner::Start()
{
    if (bFinished || PreviewDisc || FinalDisc)
    {
        return;
    }

    FString Error;
    if (!ResolveFixture(Error))
    {
        Fail(Error);
        return;
    }

    GameMode->SkipCurrentPresentation();
    if (!GameMode->CanPlayerThrow())
    {
        Fail(TEXT("Pine Ridge did not expose a legal player throw after presentation was skipped"));
        return;
    }

    BaselineStrokes = GameMode->GetStrokes();
    BaselinePenaltyStrokes = GameMode->GetPenaltyStrokes();
    BaselinePresentationEventCount = GameMode->GetPresentationAudioTraceCount();
    BaselineCompletedHoles = DiscGolfRound::CompletedHoleCount(GameMode->GetRoundState());
    BaselineRoundScoreToPar = GameMode->GetRoundScoreToPar();
    if (BaselineStrokes != 0 || BaselinePenaltyStrokes != 0
        || GameMode->GetActiveDisc() || GameMode->IsHoleComplete())
    {
        Fail(TEXT("AI proof did not start from a fresh Pine Ridge tee state"));
        return;
    }

    BuildCandidateCommands();
    if (CandidateCommands.Num() < MinimumMeasuredCandidateCount)
    {
        Fail(TEXT("AI proof constructed fewer than three candidate commands"));
        return;
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 14 AI GOLFER SMOKE START: course=%s hole=%d candidates=%d instance=%s."),
        *GameMode->GetActiveHole()->CourseId.ToString(), GameMode->GetActiveHole()->HoleNumber,
        CandidateCommands.Num(), *SelectedDiscInstance.InstanceId.ToString());
    LaunchNextPreview();
}

bool ADiscGolfSession14AISmokeRunner::ResolveFixture(FString& OutError)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    const FString CommandLineText(CommandLine);
    FString RequestedUserDir;
    if (CountOccurrencesInsensitive(CommandLineText, TEXT("-UserDir=")) != 1
        || !FParse::Value(CommandLine, TEXT("UserDir="), RequestedUserDir)
        || RequestedUserDir.IsEmpty()
        || FPaths::IsRelative(RequestedUserDir))
    {
        OutError = TEXT(
            "Session 14 requires exactly one absolute -UserDir=C:/DGTour_TestRuns/Session14/<GUID>");
        return false;
    }

    FString NormalizedRequestedUserDir =
        FPaths::ConvertRelativePathToFull(RequestedUserDir);
    FPaths::NormalizeDirectoryName(NormalizedRequestedUserDir);
    FString ActiveUserDir =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir());
    FPaths::NormalizeDirectoryName(ActiveUserDir);
    FString ExpectedParent =
        FPaths::ConvertRelativePathToFull(Session14ExternalUserDirParent);
    FPaths::NormalizeDirectoryName(ExpectedParent);
    FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectDir);
    FString ProjectSavedDir =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FPaths::NormalizeDirectoryName(ProjectSavedDir);
    FGuid UserDirGuid;
    if (!FPaths::IsSamePath(NormalizedRequestedUserDir, ActiveUserDir)
        || !FPaths::IsSamePath(FPaths::GetPath(NormalizedRequestedUserDir), ExpectedParent)
        || FPaths::IsSamePath(NormalizedRequestedUserDir, ProjectDir)
        || FPaths::IsUnderDirectory(NormalizedRequestedUserDir, ProjectDir)
        || FPaths::IsSamePath(NormalizedRequestedUserDir, ProjectSavedDir)
        || FPaths::IsUnderDirectory(NormalizedRequestedUserDir, ProjectSavedDir)
        || !FGuid::Parse(FPaths::GetCleanFilename(NormalizedRequestedUserDir), UserDirGuid)
        || !UserDirGuid.IsValid()
        || !IFileManager::Get().DirectoryExists(*NormalizedRequestedUserDir))
    {
        OutError = TEXT(
            "active -UserDir must be an existing C:/DGTour_TestRuns/Session14/<GUID> directory outside the project");
        return false;
    }
    AcceptedExternalUserDir = NormalizedRequestedUserDir;

    GameMode = GetWorld() ? Cast<ADiscGolfTourGameMode>(GetWorld()->GetAuthGameMode()) : nullptr;
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    UDiscCatalogSubsystem* Catalog = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDiscCatalogSubsystem>() : nullptr;
    ADiscGolfHoleActor* Hole = GameMode ? GameMode->GetActiveHole() : nullptr;
    if (!GameMode || !Golfer || !Bag || !Catalog || !Hole)
    {
        OutError = TEXT("game mode, Pine Ridge hole, golfer, bag, catalog, or planner was unavailable");
        return false;
    }
    if (!Hole->IsAuthoredBlockout() || Hole->CourseId != TEXT("PineRidgeChampionship"))
    {
        OutError = TEXT("Session 14 AI proof requires the loaded authored Pine Ridge course");
        return false;
    }
    if (!Bag->GetSelectedDiscInstance(SelectedDiscInstance)
        || !SelectedDiscInstance.InstanceId.IsValid())
    {
        OutError = TEXT("player bag did not expose a stable selected disc instance");
        return false;
    }

    FResolvedDiscDefinition CatalogDisc;
    if (!Catalog->ResolveDisc(Bag->GetSelectedMoldId(), Bag->GetSelectedPlastic(), CatalogDisc))
    {
        OutError = TEXT("selected player disc did not resolve through the active catalog");
        return false;
    }
    if (!UDiscBagComponent::ResolveDiscInstance(
        SelectedDiscInstance, CatalogDisc, ResolvedDisc, OutError))
    {
        return false;
    }
    if (ResolvedDisc.DiscInstanceId != SelectedDiscInstance.InstanceId)
    {
        OutError = TEXT("catalog/equipment resolution lost the stable disc instance identity");
        return false;
    }

    UDiscGolfAIGolferProfile* BuiltProfile = nullptr;
    if (!DiscGolfAIPlannerAdapter::BuildGenericProofProfile(
            this, BuiltProfile, OutError))
    {
        return false;
    }
    AIProfile = BuiltProfile;
    // The proof must plan with the current player's validated generic bag, not a parallel
    // inventory. This remains a transient profile copy and cannot mutate the player loadout.
    AIProfile->BagTemplate = Bag->GetEquipmentLoadout().Discs;
    if (!DiscGolfAIPlannerAdapter::ValidateProfile(*AIProfile, OutError))
    {
        return false;
    }

    PreviewStartLocationCm = Golfer->GetActorLocation()
        + Golfer->GetActorForwardVector() * 70.0f + FVector(0.0f, 0.0f, 35.0f);
    BasketLocationCm = Hole->BasketLocation;
    if (PreviewStartLocationCm.ContainsNaN() || BasketLocationCm.ContainsNaN()
        || FVector::DistSquared2D(PreviewStartLocationCm, BasketLocationCm)
            < FMath::Square(1000.0f))
    {
        OutError = TEXT("AI proof tee/basket geometry was non-finite or implausibly short");
        return false;
    }

    ShotContext.LieLocationCm = PreviewStartLocationCm;
    ShotContext.TargetLocationCm = BasketLocationCm;
    ShotContext.WindVelocityMps = GameMode->GetWindDirector()
        ? GameMode->GetWindDirector()->GetWindMpsAt(PreviewStartLocationCm)
        : FVector::ZeroVector;
    ShotContext.bPutting = false;
    ShotContext.StrokesTaken = GameMode->GetStrokes();
    ShotContext.Par = Hole->Par;
    if (!DiscGolfAIPlannerAdapter::ValidateContext(ShotContext, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

void ADiscGolfSession14AISmokeRunner::BuildCandidateCommands()
{
    CandidateCommands.Reset();
    const FVector BaseDirection = FVector(
        BasketLocationCm.X - PreviewStartLocationCm.X,
        BasketLocationCm.Y - PreviewStartLocationCm.Y,
        0.0f).GetSafeNormal(SMALL_NUMBER, Golfer->GetActorForwardVector());

    const auto AddCandidate = [this, &BaseDirection](
        EThrowStyle ThrowStyle,
        float AimOffsetDeg,
        float Power01,
        float HyzerDeg,
        float NoseDeg,
        float LaunchDeg)
    {
        FThrowCommand Command;
        Command.DiscInstanceId = SelectedDiscInstance.InstanceId;
        Command.MoldId = ResolvedDisc.MoldId;
        Command.Plastic = ResolvedDisc.Plastic;
        Command.ThrowStyle = ThrowStyle;
        Command.ShotContext = EDiscShotContext::Drive;
        Command.Direction = FQuat(
            FVector::UpVector, FMath::DegreesToRadians(AimOffsetDeg)).RotateVector(BaseDirection);
        Command.Power01 = Power01;
        Command.HyzerDeg = HyzerDeg;
        Command.NoseAngleDeg = NoseDeg;
        Command.LaunchAngleDeg = LaunchDeg;
        Command.TimingError = 0.0f;
        CandidateCommands.Add(Command);
    };

    AddCandidate(EThrowStyle::Backhand, 0.0f, 0.78f, 2.0f, 0.0f, 8.0f);
    AddCandidate(EThrowStyle::Backhand, -4.0f, 0.82f, 13.0f, 0.5f, 9.0f);
    AddCandidate(EThrowStyle::Backhand, 5.0f, 0.80f, -9.0f, -0.5f, 8.0f);
    AddCandidate(EThrowStyle::Forehand, 3.0f, 0.79f, -5.0f, 0.0f, 8.5f);
}

void ADiscGolfSession14AISmokeRunner::LaunchNextPreview()
{
    if (bFinished)
    {
        return;
    }
    FString IsolationError;
    if (!ValidatePreviewIsolation(IsolationError))
    {
        Fail(IsolationError);
        return;
    }
    if (!CandidateCommands.IsValidIndex(PreviewIndex))
    {
        SelectAndLaunchAuthoritative();
        return;
    }

    const FThrowCommand& Command = CandidateCommands[PreviewIndex];
    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);
    if (Release.ReleaseSpeedMps <= 0.0f || Release.SpinRpm <= 0.0f)
    {
        Fail(FString::Printf(TEXT("candidate %d produced an invalid release"), PreviewIndex));
        return;
    }

    PreviewDisc = GetWorld()->SpawnActor<ADiscActor>(
        PreviewStartLocationCm, Command.Direction.Rotation());
    if (!PreviewDisc)
    {
        Fail(FString::Printf(TEXT("candidate %d preview disc could not spawn"), PreviewIndex));
        return;
    }
    if (!PreviewDisc->InitializeDisc(ResolvedDisc, GameMode->GetWindDirector()))
    {
        Fail(FString::Printf(
            TEXT("candidate %d preview disc initialization was rejected"), PreviewIndex));
        return;
    }
    PreviewDisc->OnDiscSettled.AddUniqueDynamic(
        this, &ADiscGolfSession14AISmokeRunner::HandlePreviewSettled);
    PreviewDisc->OnDiscHoledOut.AddUniqueDynamic(
        this, &ADiscGolfSession14AISmokeRunner::HandlePreviewHoledOut);
    if (!PreviewDisc->Throw(Release)
        || !PreviewDisc->GetFlightComponent()
        || !PreviewDisc->GetFlightComponent()->IsFlying())
    {
        Fail(FString::Printf(TEXT("candidate %d did not enter the existing flight solver"), PreviewIndex));
        return;
    }

    GetWorldTimerManager().SetTimer(
        PreviewTimeoutTimer, this,
        &ADiscGolfSession14AISmokeRunner::HandlePreviewTimeout,
        PreviewTimeoutSeconds, false);
}

void ADiscGolfSession14AISmokeRunner::HandlePreviewSettled(
    ADiscActor* Disc,
    FVector FinalLocation)
{
    CompletePreview(Disc, FinalLocation, false);
}

void ADiscGolfSession14AISmokeRunner::HandlePreviewHoledOut(ADiscActor* Disc)
{
    CompletePreview(Disc, Disc ? Disc->GetActorLocation() : FVector::ZeroVector, true);
}

void ADiscGolfSession14AISmokeRunner::CompletePreview(
    ADiscActor* Disc,
    const FVector& FinalLocation,
    bool bHoledOut)
{
    if (bFinished || Disc != PreviewDisc || !Disc || !Disc->GetFlightComponent())
    {
        if (!bFinished)
        {
            Fail(TEXT("preview completion came from an unexpected disc"));
        }
        return;
    }
    GetWorldTimerManager().ClearTimer(PreviewTimeoutTimer);

    const UDiscFlightComponent* Flight = Disc->GetFlightComponent();
    const FDiscFlightTelemetry Telemetry = Flight->GetTelemetry();
    const int32 SampleCount = Flight->GetTrajectorySamples().Num();
    if (!IsFiniteTelemetry(Telemetry) || FinalLocation.ContainsNaN()
        || Telemetry.FlightTimeSeconds <= 0.0f || SampleCount < 2
        || Telemetry.Release.ReleaseSpeedMps <= 0.0f
        || Telemetry.Release.SpinRpm <= 0.0f)
    {
        Fail(FString::Printf(TEXT("candidate %d returned invalid measured flight evidence"), PreviewIndex));
        return;
    }

    const FThrowCommand& Command = CandidateCommands[PreviewIndex];
    FDiscGolfSession14MeasuredCandidate Measurement;
    Measurement.CandidateId = FName(*FString::Printf(TEXT("Candidate_%02d"), PreviewIndex + 1));
    Measurement.Command = Command;
    Measurement.Telemetry = Telemetry;
    Measurement.StartLocationCm = PreviewStartLocationCm;
    Measurement.FinalLocationCm = FinalLocation;
    Measurement.SampleCount = SampleCount;
    Measurement.bHoledOut = bHoledOut;

    FDiscGolfAIMeasuredShotOutcome Outcome;
    Outcome.Command = Command;
    Outcome.StartWorldLocationCm = PreviewStartLocationCm;
    Outcome.FinalWorldLocationCm = FinalLocation;
    Outcome.FinalTelemetry = Telemetry;
    FString CandidateError;
    if (!AIProfile || !DiscGolfAIPlannerAdapter::BuildCandidateFromMeasuredOutcome(
            *AIProfile, ShotContext, Outcome,
            Measurement.PlannerCandidate, CandidateError))
    {
        Fail(FString::Printf(
            TEXT("candidate %d failed the measured-outcome adapter: %s"),
            PreviewIndex, *CandidateError));
        return;
    }

    const int32 StoredMeasurementIndex = MeasuredCandidates.Add(MoveTemp(Measurement));
    const FDiscGolfSession14MeasuredCandidate& StoredMeasurement =
        MeasuredCandidates[StoredMeasurementIndex];
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 14 AI PREVIEW MEASURED: index=%d samples=%d carry=%.3f progress=%.3f error=%.3f fixtures=%d holed=%d."),
        PreviewIndex, SampleCount, Telemetry.CarryMeters,
        StoredMeasurement.PlannerCandidate.ExpectedProgressM,
        StoredMeasurement.PlannerCandidate.LandingErrorM,
        Telemetry.FixtureContactCount, bHoledOut ? 1 : 0);

    CleanupPreview();
    ++PreviewIndex;
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
        this, &ADiscGolfSession14AISmokeRunner::LaunchNextPreview));
}

bool ADiscGolfSession14AISmokeRunner::ValidatePreviewIsolation(FString& OutError) const
{
    if (!GameMode)
    {
        OutError = TEXT("GameMode authority disappeared during preview evaluation");
        return false;
    }
    if (GameMode->GetActiveDisc())
    {
        OutError = TEXT("a preview entered or replaced GameMode active-disc authority");
        return false;
    }
    if (GameMode->GetStrokes() != BaselineStrokes
        || GameMode->GetPenaltyStrokes() != BaselinePenaltyStrokes)
    {
        OutError = TEXT("a preview mutated player stroke or penalty state");
        return false;
    }
    if (GameMode->GetPresentationAudioTraceCount() != BaselinePresentationEventCount)
    {
        OutError = TEXT("a preview emitted authoritative presentation events");
        return false;
    }
    const FDiscGolfRoundState Round = GameMode->GetRoundState();
    if (DiscGolfRound::CompletedHoleCount(Round) != BaselineCompletedHoles
        || GameMode->GetRoundScoreToPar() != BaselineRoundScoreToPar
        || GameMode->IsHoleComplete())
    {
        OutError = TEXT("a preview mutated round or hole completion state");
        return false;
    }
    OutError.Reset();
    return true;
}

void ADiscGolfSession14AISmokeRunner::SelectAndLaunchAuthoritative()
{
    FString Error;
    if (!ValidatePreviewIsolation(Error))
    {
        Fail(Error);
        return;
    }
    if (MeasuredCandidates.Num() != CandidateCommands.Num()
        || MeasuredCandidates.Num() < MinimumMeasuredCandidateCount)
    {
        Fail(TEXT("measured candidate set was incomplete"));
        return;
    }

    TArray<FDiscGolfAIMeasuredShotOutcome> Outcomes;
    Outcomes.Reserve(MeasuredCandidates.Num());
    for (const FDiscGolfSession14MeasuredCandidate& Measurement : MeasuredCandidates)
    {
        FDiscGolfAIMeasuredShotOutcome& Outcome = Outcomes.AddDefaulted_GetRef();
        Outcome.Command = Measurement.Command;
        Outcome.StartWorldLocationCm = Measurement.StartLocationCm;
        Outcome.FinalWorldLocationCm = Measurement.FinalLocationCm;
        Outcome.FinalTelemetry = Measurement.Telemetry;
    }
    if (!AIProfile || !DiscGolfAIPlannerAdapter::SelectMeasuredShot(
            *AIProfile, ShotContext, Outcomes,
            SelectedCommand, SelectedPlannerCandidate, Error))
    {
        Fail(FString::Printf(
            TEXT("measured-shot adapter/planner rejected the complete candidate set: %s"),
            *Error));
        return;
    }

    SelectedMeasuredIndex = INDEX_NONE;
    for (int32 Index = 0; Index < MeasuredCandidates.Num(); ++Index)
    {
        if (PlannerCandidatesMatch(
            MeasuredCandidates[Index].PlannerCandidate, SelectedPlannerCandidate))
        {
            SelectedMeasuredIndex = Index;
            break;
        }
    }
    if (!MeasuredCandidates.IsValidIndex(SelectedMeasuredIndex))
    {
        Fail(TEXT("planner selection identity was not a member of the measured set"));
        return;
    }

    if (SelectedCommand.DiscInstanceId != SelectedDiscInstance.InstanceId
        || SelectedCommand.MoldId != ResolvedDisc.MoldId
        || SelectedCommand.Plastic != ResolvedDisc.Plastic
        || SelectedPlannerCandidate.Disc.InstanceId != SelectedDiscInstance.InstanceId)
    {
        Fail(TEXT("selected measured candidate lost the player-owned equipment identity"));
        return;
    }

    ++AuthoritativeRequestCount;
    if (!GameMode->RequestThrow(SelectedCommand))
    {
        Fail(TEXT("selected command was rejected by the authoritative player ingress"));
        return;
    }
    FinalDisc = GameMode->GetActiveDisc();
    AuthoritativeStrokeDeltaAtLaunch = GameMode->GetStrokes() - BaselineStrokes;
    if (AuthoritativeRequestCount != 1 || !FinalDisc || !FinalDisc->GetFlightComponent()
        || !FinalDisc->GetFlightComponent()->IsFlying()
        || !GameMode->HasLastRelease() || AuthoritativeStrokeDeltaAtLaunch != 1
        || FinalDisc->GetResolvedDisc().DiscInstanceId != SelectedDiscInstance.InstanceId)
    {
        Fail(TEXT("selected command did not enter the one existing authoritative throw path exactly once"));
        return;
    }

    FinalDisc->OnDiscSettled.AddUniqueDynamic(
        this, &ADiscGolfSession14AISmokeRunner::HandleFinalSettled);
    FinalDisc->OnDiscHoledOut.AddUniqueDynamic(
        this, &ADiscGolfSession14AISmokeRunner::HandleFinalHoledOut);
    GetWorldTimerManager().SetTimer(
        FinalTimeoutTimer, this,
        &ADiscGolfSession14AISmokeRunner::HandleFinalTimeout,
        FinalTimeoutSeconds, false);
}

void ADiscGolfSession14AISmokeRunner::HandleFinalSettled(
    ADiscActor* Disc,
    FVector FinalLocation)
{
    if (bFinished || Disc != FinalDisc || !Disc || !Disc->GetFlightComponent())
    {
        if (!bFinished) Fail(TEXT("authoritative settle came from an unexpected disc"));
        return;
    }
    GetWorldTimerManager().ClearTimer(FinalTimeoutTimer);
    FinalTelemetry = Disc->GetFlightComponent()->GetTelemetry();
    FinalSampleCount = Disc->GetFlightComponent()->GetTrajectorySamples().Num();
    FinalAuthoritativeLocationCm = FinalLocation;
    bFinalHoledOut = false;

    if (AuthoritativeRequestCount != 1
        || GameMode->GetStrokes() - BaselineStrokes != 1
        || !GameMode->HasLastRelease() || !GameMode->HasLastFlightTelemetry()
        || !IsFiniteTelemetry(FinalTelemetry) || FinalTelemetry.FlightTimeSeconds <= 0.0f
        || FinalSampleCount < 2 || FinalLocation.ContainsNaN()
        || Disc->GetResolvedDisc().DiscInstanceId != SelectedDiscInstance.InstanceId)
    {
        Fail(TEXT("authoritative selected flight did not finish with valid one-stroke telemetry"));
        return;
    }
    Pass();
}

void ADiscGolfSession14AISmokeRunner::HandleFinalHoledOut(ADiscActor* Disc)
{
    if (bFinished || Disc != FinalDisc || !Disc || !Disc->GetFlightComponent())
    {
        if (!bFinished) Fail(TEXT("authoritative hole-out came from an unexpected disc"));
        return;
    }
    GetWorldTimerManager().ClearTimer(FinalTimeoutTimer);
    FinalTelemetry = Disc->GetFlightComponent()->GetTelemetry();
    FinalSampleCount = Disc->GetFlightComponent()->GetTrajectorySamples().Num();
    FinalAuthoritativeLocationCm = Disc->GetActorLocation();
    bFinalHoledOut = true;
    if (AuthoritativeRequestCount != 1
        || GameMode->GetStrokes() - BaselineStrokes != 1
        || !GameMode->HasLastRelease() || !GameMode->HasLastFlightTelemetry()
        || !IsFiniteTelemetry(FinalTelemetry) || FinalTelemetry.FlightTimeSeconds <= 0.0f
        || FinalSampleCount < 2
        || Disc->GetResolvedDisc().DiscInstanceId != SelectedDiscInstance.InstanceId)
    {
        Fail(TEXT("authoritative selected hole-out did not finish with valid one-stroke telemetry"));
        return;
    }
    Pass();
}

void ADiscGolfSession14AISmokeRunner::HandlePreviewTimeout()
{
    Fail(FString::Printf(TEXT("candidate %d exceeded the bounded %.0f second preview timeout"),
        PreviewIndex, PreviewTimeoutSeconds));
}

void ADiscGolfSession14AISmokeRunner::HandleFinalTimeout()
{
    Fail(FString::Printf(TEXT("authoritative selected flight exceeded the bounded %.0f second timeout"),
        FinalTimeoutSeconds));
}

void ADiscGolfSession14AISmokeRunner::CleanupPreview()
{
    GetWorldTimerManager().ClearTimer(PreviewTimeoutTimer);
    if (PreviewDisc)
    {
        PreviewDisc->OnDiscSettled.RemoveDynamic(
            this, &ADiscGolfSession14AISmokeRunner::HandlePreviewSettled);
        PreviewDisc->OnDiscHoledOut.RemoveDynamic(
            this, &ADiscGolfSession14AISmokeRunner::HandlePreviewHoledOut);
        PreviewDisc->Destroy();
        PreviewDisc = nullptr;
    }
}

bool ADiscGolfSession14AISmokeRunner::WriteReport(
    bool bPassed,
    FString& OutPath) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_session14_ai_golfer_smoke"));
    Root->SetNumberField(TEXT("schema_version"), 1);
    Root->SetStringField(TEXT("result"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(TEXT("failure_reason"), bPassed ? TEXT("") : FailureReason);
    Root->SetStringField(TEXT("authority"),
        TEXT("DiscGolfMath::ResolveThrowRelease -> ADiscActor/UDiscFlightComponent previews -> ADiscGolfTourGameMode::RequestThrow selected execution"));
    Root->SetBoolField(TEXT("runner_tick_enabled"), PrimaryActorTick.bCanEverTick);
    Root->SetBoolField(TEXT("preview_game_mode_delegates_bound"), false);
    Root->SetStringField(TEXT("course_id"), GameMode && GameMode->GetActiveHole()
        ? GameMode->GetActiveHole()->CourseId.ToString() : TEXT("None"));
    Root->SetNumberField(TEXT("hole_number"), GameMode && GameMode->GetActiveHole()
        ? GameMode->GetActiveHole()->HoleNumber : 0);
    Root->SetStringField(TEXT("disc_instance_id"), SelectedDiscInstance.InstanceId.ToString());
    Root->SetStringField(TEXT("mold_id"), ResolvedDisc.MoldId.ToString());
    Root->SetNumberField(TEXT("candidate_count"), MeasuredCandidates.Num());

    TArray<TSharedPtr<FJsonValue>> CandidateValues;
    for (int32 Index = 0; Index < MeasuredCandidates.Num(); ++Index)
    {
        const FDiscGolfSession14MeasuredCandidate& Measurement = MeasuredCandidates[Index];
        TSharedRef<FJsonObject> Candidate = MakeShared<FJsonObject>();
        Candidate->SetStringField(TEXT("candidate_id"), Measurement.CandidateId.ToString());
        Candidate->SetNumberField(TEXT("candidate_index"), Index);
        Candidate->SetStringField(TEXT("throw_style"),
            Measurement.Command.ThrowStyle == EThrowStyle::Forehand
                ? TEXT("Forehand") : TEXT("Backhand"));
        Candidate->SetNumberField(TEXT("power_01"), Measurement.Command.Power01);
        Candidate->SetNumberField(TEXT("hyzer_deg"), Measurement.Command.HyzerDeg);
        Candidate->SetNumberField(TEXT("nose_deg"), Measurement.Command.NoseAngleDeg);
        Candidate->SetNumberField(TEXT("launch_deg"), Measurement.Command.LaunchAngleDeg);
        Candidate->SetObjectField(TEXT("final_location_cm"), VectorJson(Measurement.FinalLocationCm));
        Candidate->SetNumberField(TEXT("sample_count"), Measurement.SampleCount);
        Candidate->SetNumberField(TEXT("flight_time_s"), Measurement.Telemetry.FlightTimeSeconds);
        Candidate->SetNumberField(TEXT("carry_m"), Measurement.Telemetry.CarryMeters);
        Candidate->SetNumberField(TEXT("progress_m"), Measurement.PlannerCandidate.ExpectedProgressM);
        Candidate->SetNumberField(TEXT("landing_error_m"), Measurement.PlannerCandidate.LandingErrorM);
        Candidate->SetNumberField(TEXT("fixture_contacts"), Measurement.Telemetry.FixtureContactCount);
        Candidate->SetNumberField(TEXT("course_surface"),
            static_cast<int32>(Measurement.Telemetry.CourseSurface));
        Candidate->SetBoolField(TEXT("holed_out"), Measurement.bHoledOut);
        Candidate->SetBoolField(TEXT("selected"), Index == SelectedMeasuredIndex);
        CandidateValues.Add(MakeShared<FJsonValueObject>(Candidate));
    }
    Root->SetArrayField(TEXT("measured_candidates"), CandidateValues);

    Root->SetNumberField(TEXT("selected_candidate_index"), SelectedMeasuredIndex);
    Root->SetStringField(TEXT("selected_candidate_id"),
        MeasuredCandidates.IsValidIndex(SelectedMeasuredIndex)
            ? MeasuredCandidates[SelectedMeasuredIndex].CandidateId.ToString() : TEXT("None"));
    Root->SetNumberField(TEXT("selected_utility"), SelectedPlannerCandidate.UtilityScore);
    Root->SetNumberField(TEXT("authoritative_request_count"), AuthoritativeRequestCount);
    Root->SetNumberField(TEXT("authoritative_stroke_delta_at_launch"), AuthoritativeStrokeDeltaAtLaunch);
    Root->SetNumberField(TEXT("final_stroke_delta"),
        GameMode ? GameMode->GetStrokes() - BaselineStrokes : -1);
    Root->SetBoolField(TEXT("final_holed_out"), bFinalHoledOut);
    Root->SetObjectField(TEXT("final_location_cm"), VectorJson(FinalAuthoritativeLocationCm));
    Root->SetNumberField(TEXT("final_sample_count"), FinalSampleCount);
    Root->SetNumberField(TEXT("final_flight_time_s"), FinalTelemetry.FlightTimeSeconds);
    Root->SetNumberField(TEXT("final_carry_m"), FinalTelemetry.CarryMeters);
    Root->SetNumberField(TEXT("final_fixture_contacts"), FinalTelemetry.FixtureContactCount);
    Root->SetBoolField(TEXT("career_committed"), bCareerCommitted);
    Root->SetBoolField(TEXT("career_saved_to_isolated_slot"), bCareerSaved);
    Root->SetBoolField(TEXT("career_loaded_from_isolated_slot"), bCareerLoaded);
    Root->SetBoolField(TEXT("career_isolated_slot_deleted"), bCareerSlotDeleted);
    Root->SetStringField(TEXT("event_id"), CareerEventId.ToString());
    Root->SetStringField(TEXT("presenting_brand_id"), CareerPresentingBrandId.ToString());
    Root->SetNumberField(TEXT("career_round_history_count"), CareerRoundHistoryCount);
    Root->SetBoolField(TEXT("career_round_trip_equal"), bCareerRoundTripEqual);
    Root->SetStringField(TEXT("career_round_source"),
        TEXT("local_synthetic_completed_round_not_gameplay_round"));
    Root->SetStringField(TEXT("career_isolated_slot_prefix"),
        TEXT("DGT_Career_Session14Smoke_"));
    Root->SetStringField(TEXT("external_user_dir"), AcceptedExternalUserDir);

    FString Json;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        return false;
    }

    const FString Directory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("Session14Reports"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true)
        && !IFileManager::Get().DirectoryExists(*Directory))
    {
        return false;
    }
    OutPath = FPaths::Combine(Directory, TEXT("Session14AIGolferSmoke.json"));
    return FFileHelper::SaveStringToFile(
        Json, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool ADiscGolfSession14AISmokeRunner::RunDisposableCareerProof(FString& OutError)
{
    bCareerCommitted = false;
    bCareerSaved = false;
    bCareerLoaded = false;
    bCareerRoundTripEqual = false;
    bCareerSlotDeleted = false;
    CareerRoundHistoryCount = 0;
    CareerEventId = NAME_None;
    CareerPresentingBrandId = NAME_None;

    UGameInstance* GameInstance = GetGameInstance();
    if (!GameInstance || !GameMode)
    {
        OutError = TEXT("game instance or GameMode was unavailable to the isolated career proof");
        return false;
    }

    // This scorecard is intentionally local synthetic proof data. The authoritative AI throw
    // above did not complete a round, and the live GameMode round is never passed to career code.
    const FDiscGolfCompetitionRuntimeDefinition Event =
        DiscGolfCompetitionRuntime::BuildSourceFallbackEvent();
    FString ValidationError;
    if (!DiscGolfCompetitionRuntime::ValidateEvent(Event, ValidationError)
        || Event.Rounds.Num() != 1
        || Event.HolePars.Num() != DiscGolfCompetitionRuntime::Session14HoleCount)
    {
        OutError = FString::Printf(
            TEXT("source fallback event was invalid: %s"), *ValidationError);
        return false;
    }
    CareerEventId = Event.EventId;
    CareerPresentingBrandId = Event.PresentingBrandId;

    FDiscGolfRoundState SyntheticCompletedRound;
    SyntheticCompletedRound.CourseId = Event.Rounds[0].CourseId;
    SyntheticCompletedRound.LayoutId = Event.Rounds[0].TeeSetId;
    SyntheticCompletedRound.CourseName = Event.DisplayName;
    SyntheticCompletedRound.CurrentHoleIndex = Event.HolePars.Num() - 1;
    SyntheticCompletedRound.bRoundComplete = true;
    for (int32 Index = 0; Index < Event.HolePars.Num(); ++Index)
    {
        FDiscGolfRoundHoleScore& Hole =
            SyntheticCompletedRound.HoleScores.AddDefaulted_GetRef();
        Hole.HoleNumber = Index + 1;
        Hole.HoleName = FText::FromString(
            FString::Printf(TEXT("Synthetic Proof Hole %d"), Index + 1));
        Hole.Par = Event.HolePars[Index];
        Hole.Strokes = Event.HolePars[Index];
        Hole.PenaltyStrokes = 0;
        Hole.bCompleted = true;
    }

    const int32 BeforeStrokes = GameMode->GetStrokes();
    const int32 BeforePenaltyStrokes = GameMode->GetPenaltyStrokes();
    const int32 BeforeCompletedHoles =
        DiscGolfRound::CompletedHoleCount(GameMode->GetRoundState());
    const int32 BeforeRoundScoreToPar = GameMode->GetRoundScoreToPar();
    const bool bBeforeHoleComplete = GameMode->IsHoleComplete();

    const FString SlotName = FString::Printf(
        TEXT("DGT_Career_Session14Smoke_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    constexpr int32 UserIndex = 0;
    UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

    const auto DeleteDisposableSlot = [&]()
    {
        if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
        {
            UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
        }
        bCareerSlotDeleted =
            !UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
    };

    // Separate transient subsystem objects prove serialization without touching the live
    // GameInstance subsystem's in-memory progress or its production/default save domain.
    UDiscGolfCareerSubsystem* SourceCareer =
        NewObject<UDiscGolfCareerSubsystem>(GameInstance);
    UDiscGolfCareerSubsystem* LoadedCareer =
        NewObject<UDiscGolfCareerSubsystem>(GameInstance);
    if (!SourceCareer || !LoadedCareer)
    {
        OutError = TEXT("could not allocate disposable career subsystem instances");
        DeleteDisposableSlot();
        return false;
    }
    SourceCareer->ResetToDefaults();
    LoadedCareer->ResetToDefaults();

    FString CareerError;
    if (!SourceCareer->CommitCompletedRound(Event, SyntheticCompletedRound, CareerError))
    {
        OutError = FString::Printf(TEXT("career commit failed: %s"), *CareerError);
        DeleteDisposableSlot();
        return false;
    }
    bCareerCommitted = true;
    if (!SourceCareer->SaveCareerToSlot(SlotName, UserIndex, CareerError))
    {
        OutError = FString::Printf(TEXT("isolated career save failed: %s"), *CareerError);
        DeleteDisposableSlot();
        return false;
    }
    bCareerSaved = true;
    if (!LoadedCareer->LoadCareerFromSlot(SlotName, UserIndex, CareerError))
    {
        OutError = FString::Printf(TEXT("isolated career load failed: %s"), *CareerError);
        DeleteDisposableSlot();
        return false;
    }

    const FDGCareerProgress& LoadedProgress = LoadedCareer->GetProgress();
    CareerRoundHistoryCount = LoadedProgress.RoundHistory.Num();
    bCareerRoundTripEqual = CareerProgressMatches(
        SourceCareer->GetProgress(), LoadedProgress);
    if (LoadedProgress.CompletedEventIds.Num() != 1
        || LoadedProgress.CompletedEventIds[0] != Event.EventId
        || LoadedProgress.RoundHistory.Num() != 1
        || !bCareerRoundTripEqual
        || !DiscGolfCompetitionRuntime::ValidateCompletedScorecard(
            Event, LoadedProgress.RoundHistory[0], CareerError))
    {
        OutError = FString::Printf(
            TEXT("loaded isolated career did not contain the exact synthetic event result: %s"),
            *CareerError);
        DeleteDisposableSlot();
        return false;
    }
    bCareerLoaded = true;
    DeleteDisposableSlot();
    if (!bCareerSlotDeleted)
    {
        OutError = TEXT("disposable Session 14 career slot could not be deleted");
        return false;
    }

    if (GameMode->GetStrokes() != BeforeStrokes
        || GameMode->GetPenaltyStrokes() != BeforePenaltyStrokes
        || DiscGolfRound::CompletedHoleCount(GameMode->GetRoundState()) != BeforeCompletedHoles
        || GameMode->GetRoundScoreToPar() != BeforeRoundScoreToPar
        || GameMode->IsHoleComplete() != bBeforeHoleComplete)
    {
        OutError = TEXT("disposable career proof mutated live GameMode round authority");
        return false;
    }

    OutError.Reset();
    return true;
}

void ADiscGolfSession14AISmokeRunner::Pass()
{
    if (bFinished)
    {
        return;
    }
    FString CareerError;
    if (!RunDisposableCareerProof(CareerError))
    {
        Fail(FString::Printf(TEXT("disposable career persistence proof failed: %s"), *CareerError));
        return;
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 14 CAREER TOURNAMENT SMOKE PASS: event=PineRidgeChampionship brand=dg_generic round_history=1 roundtrip=1 slot_deleted=1 synthetic_round=1"));
    bFinished = true;
    GetWorldTimerManager().ClearTimer(PreviewTimeoutTimer);
    GetWorldTimerManager().ClearTimer(FinalTimeoutTimer);
    FString ReportPath;
    const bool bReportWritten = WriteReport(true, ReportPath);
    if (!bReportWritten)
    {
        bFinished = false;
        Fail(TEXT("deterministic Session 14 JSON report could not be written"));
        return;
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("SESSION 14 AI GOLFER SMOKE PASS: previews=%d selected=%d requests=1 stroke_delta=1 final_samples=%d final_time=%.3f report=%s"),
        MeasuredCandidates.Num(), SelectedMeasuredIndex, FinalSampleCount,
        FinalTelemetry.FlightTimeSeconds, *ReportPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ADiscGolfSession14AISmokeRunner::Fail(const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    FailureReason = Reason;
    GetWorldTimerManager().ClearTimer(PreviewTimeoutTimer);
    GetWorldTimerManager().ClearTimer(FinalTimeoutTimer);
    CleanupPreview();
    if (FinalDisc)
    {
        FinalDisc->OnDiscSettled.RemoveDynamic(
            this, &ADiscGolfSession14AISmokeRunner::HandleFinalSettled);
        FinalDisc->OnDiscHoledOut.RemoveDynamic(
            this, &ADiscGolfSession14AISmokeRunner::HandleFinalHoledOut);
    }
    FString ReportPath;
    const bool bReportWritten = WriteReport(false, ReportPath);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("SESSION 14 AI GOLFER SMOKE FAIL: %s | previews=%d selected=%d requests=%d stroke_delta=%d report_written=%d report=%s"),
        *Reason, MeasuredCandidates.Num(), SelectedMeasuredIndex,
        AuthoritativeRequestCount,
        GameMode ? GameMode->GetStrokes() - BaselineStrokes : -1,
        bReportWritten ? 1 : 0, *ReportPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}
