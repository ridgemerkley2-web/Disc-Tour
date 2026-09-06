#include "DiscGolfTourGameMode.h"
#include "DiscGolfTour.h"
#include "DevCourseBootstrap.h"
#include "BasketActor.h"
#include "DiscActor.h"
#include "DiscCatalogSubsystem.h"
#include "DiscFlightComponent.h"
#include "DiscGolfHoleActor.h"
#include "DiscGolfHUD.h"
#include "DiscGolfMath.h"
#include "DiscGolfGameplayGate.h"
#include "DiscGolfCourseRules.h"
#include "DiscGolfCoursePresentationDefinition.h"
#include "DiscGolfQualityAdapter.h"
#include "DiscGolfSaveGame.h"
#include "DiscBagComponent.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfPresentationMath.h"
#include "DiscGolfPresentationAudioRouterComponent.h"
#include "DiscGolfPlayabilityMonitorComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscTrajectorySubsystem.h"
#include "DiscReplayActor.h"
#include "DiscBroadcastCameraDirector.h"
#include "DiscGolfFlyoverRouteActor.h"
#include "DiscGolfFoliagePresentationActor.h"
#include "DiscGolfTerrainPresentationActor.h"
#include "DiscGolfWaterPresentationActor.h"
#include "DiscGolfCourseSurfaceActor.h"
#include "DiscGolfFixturePresentationActor.h"
#include "DiscGolfFixtureQaRunner.h"
#include "DiscGolfWorldFixtureActor.h"
#include "ThrowControllerComponent.h"
#include "WindDirector.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "HAL/FileManager.h"
#include "DynamicRHI.h"
#include "RHIGlobals.h"
#include "HighResScreenshot.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"
#include "Scalability.h"
#include "UnrealClient.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool IsCanonicalLowerUuidV4(const FString& Value)
{
    FGuid Parsed;
    return Value.Len() == 36
        && FGuid::ParseExact(Value, EGuidFormats::DigitsWithHyphensLower, Parsed)
        && Parsed.IsValid()
        && Parsed.ToString(EGuidFormats::DigitsWithHyphensLower) == Value
        && Value[14] == TEXT('4')
        && (Value[19] == TEXT('8') || Value[19] == TEXT('9')
            || Value[19] == TEXT('a') || Value[19] == TEXT('b'));
}

FString NormalizedAbsoluteDirectory(const FString& Directory)
{
    FString Result = FPaths::ConvertRelativePathToFull(Directory);
    FPaths::NormalizeDirectoryName(Result);
    return Result;
}

bool IsSameAsOrUnderDirectory(const FString& Candidate, const FString& Parent)
{
    const FString NormalizedCandidate = NormalizedAbsoluteDirectory(Candidate);
    FString NormalizedParent = NormalizedAbsoluteDirectory(Parent);
    if (NormalizedCandidate.Equals(NormalizedParent, ESearchCase::IgnoreCase))
    {
        return true;
    }
    NormalizedParent += TEXT("/");
    return NormalizedCandidate.StartsWith(NormalizedParent, ESearchCase::IgnoreCase);
}

bool InvalidateLatestPhysicsRegressionReport()
{
#if DG_WITH_DEVELOPMENT_CONTENT
    IFileManager& FileManager = IFileManager::Get();
    const FString LatestPath = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("PhysicsRegressionReports/LatestPhysicsRegression.json"));
    if (FileManager.FileExists(*LatestPath))
    {
        FileManager.Delete(*LatestPath, false, true, true);
    }
    return !FileManager.FileExists(*LatestPath);
#else
    // Regression evidence is excluded from the release SKU. Keep the call sites
    // harmless without compiling the diagnostic path marker into Shipping.
    return true;
#endif
}

bool TryBindAutomatedPlayerCommand(
    ADiscGolferPawn* Golfer,
    FThrowCommand& Command,
    FString& OutError)
{
    OutError.Reset();
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    const UDiscGolfCharacterProfile* Profile = Golfer
        ? Golfer->GetRuntimeCharacterProfile() : nullptr;
    if (!Bag || !Profile)
    {
        OutError = TEXT("player equipment or character profile is unavailable");
        return false;
    }
    if (!Bag->SelectEquipment(Command.MoldId, Command.Plastic))
    {
        OutError = TEXT("requested automated equipment could not be selected");
        return false;
    }
    FDGDiscInstance Selected;
    if (!Bag->GetSelectedDiscInstance(Selected)
        || !Selected.InstanceId.IsValid()
        || Selected.DiscDefinitionId != Command.MoldId)
    {
        OutError = TEXT("selected automated equipment has no stable matching instance");
        return false;
    }
    Command.DiscInstanceId = Selected.InstanceId;
    Command.Handedness = Profile->Handedness;
    return true;
}
}

ADiscGolfTourGameMode::ADiscGolfTourGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ADiscGolferPawn::StaticClass();
    PlayerControllerClass = ADiscGolfTourPlayerController::StaticClass();
    HUDClass = ADiscGolfHUD::StaticClass();
    PresentationAudioRouter = CreateDefaultSubobject<UDiscGolfPresentationAudioRouterComponent>(
        TEXT("PresentationAudioRouter"));
    PlayabilityMonitor = CreateDefaultSubobject<UDiscGolfPlayabilityMonitorComponent>(
        TEXT("PlayabilityMonitor"));
}

void ADiscGolfTourGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    PerformanceTracker.AddFrame(DeltaSeconds);
    PerformanceRefreshAccumulator += DeltaSeconds;
    if (PerformanceRefreshAccumulator >= 0.5f)
    {
        PerformanceRefreshAccumulator = FMath::Fmod(PerformanceRefreshAccumulator, 0.5f);
        RefreshPerformanceTelemetry();
    }
    BasketVisibilityRefreshAccumulator += DeltaSeconds;
    if (BasketVisibilityRefreshAccumulator >= 0.2f)
    {
        BasketVisibilityRefreshAccumulator = FMath::Fmod(BasketVisibilityRefreshAccumulator, 0.2f);
        RefreshBasketVisibility();
    }
    DrawShotTracer();
    UpdatePresentationAudioEvents();
    if (bPineRidgePlaySmokeTestActive && BroadcastCameraDirector
        && BroadcastCameraDirector->GetStatusText().Contains(TEXT("COURSE ANCHOR")))
    {
        bPineRidgeSmokeSawAuthoredCamera = true;
    }
    if (bPineRidgePlaySmokeTestActive && ActiveDisc && WindDirector
        && !WindDirector->GetActiveZoneIdAt(ActiveDisc->GetActorLocation()).IsNone())
    {
        bPineRidgeSmokeEnteredWindZone = true;
    }
}

void ADiscGolfTourGameMode::RefreshPerformanceTelemetry()
{
    const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
    PerformanceSummary = PerformanceTracker.Summarize(MemoryStats.UsedPhysical);
    PerformanceBudgetState = DiscGolfPerformance::Evaluate(
        PerformanceSummary, PerformanceTracker.GetBudget());
    PerformanceStatusText = DiscGolfPerformance::StatusText(
        PerformanceSummary, PerformanceTracker.GetBudget());
}

void ADiscGolfTourGameMode::ResetPerformanceTelemetry()
{
    if (!IsPublicPerformanceCaptureMutationAllowed(DG_WITH_DEVELOPMENT_CONTENT != 0))
    {
        PerformanceStatusText = TEXT("Shipping public performance telemetry reset rejected.");
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Shipping public performance telemetry reset rejected."));
        return;
    }
    ResetPerformanceTelemetryInternal();
}

void ADiscGolfTourGameMode::ResetPerformanceTelemetryInternal()
{
    PerformanceTracker.Reset();
    PerformanceSummary = FDiscGolfPerformanceSummary();
    PerformanceBudgetState = EDiscGolfPerformanceBudgetState::WarmingUp;
    PerformanceRefreshAccumulator = 0.0f;
    PerformanceStatusText = FString::Printf(TEXT("PERF WARMING 0/%d"),
        PerformanceTracker.GetBudget().MinimumSampleCount);
}

void ADiscGolfTourGameMode::ApplyOmenPerformanceCaptureProfile()
{
    const FDiscGolfResolvedQualityProfile QualityProfile =
        DiscGolfQualityAdapter::MakeOmenCaptureProfile(
            Scalability::GetQualityLevels());
    Scalability::SetQualityLevels(QualityProfile.EngineQuality, true);
    PerformanceCaptureProfile = QualityProfile.ProfileId.ToString();
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Performance profile applied: %s (resolution=100, quality=2, foliage=3)."),
        *PerformanceCaptureProfile);
}

FDiscGolfReleasePerformanceCaptureLaunchValidation
ADiscGolfTourGameMode::ValidateReleasePerformanceCaptureLaunch(
    const FString& CommandLine,
    bool bIsUnattended,
    const FString& ResolvedUserDir,
    const FString& ProjectDirectory,
    const FString& RootDirectory,
    bool bCaptureDirectoryAlreadyExists,
    bool bSaveGameDirectoryAlreadyExists)
{
    FDiscGolfReleasePerformanceCaptureLaunchValidation Result;
    TArray<FString> Arguments;
    const TCHAR* Cursor = *CommandLine;
    FString Argument;
    while (FParse::Token(Cursor, Argument, false))
    {
        Arguments.Add(Argument);
    }

    int32 CourseCount = 0;
    int32 HoleCount = 0;
    int32 DurationCount = 0;
    int32 ResXCount = 0;
    int32 ResYCount = 0;
    int32 UserDirCount = 0;
    int32 ForceResCount = 0;
    int32 RenderOffscreenCount = 0;
    int32 Dx12Count = 0;
    int32 UnattendedCount = 0;
    int32 NoLoadCount = 0;
    int32 NoProfileWritesCount = 0;
    bool bUnknownArgument = false;
    FString CourseValue;
    FString HoleValue;
    FString DurationValue;
    FString ResXValue;
    FString ResYValue;
    FString UserDirValue;

    const auto CaptureValue = [](const FString& Token, const TCHAR* Prefix, int32& Count,
                                  FString& OutValue) -> bool
    {
        const FString PrefixText(Prefix);
        if (!Token.StartsWith(PrefixText, ESearchCase::CaseSensitive))
        {
            return false;
        }
        ++Count;
        OutValue = Token.Mid(PrefixText.Len());
        return true;
    };

    for (const FString& Token : Arguments)
    {
        if (Token.StartsWith(TEXT("-PerformanceCapture"), ESearchCase::IgnoreCase))
        {
            Result.bAttempted = true;
        }
        if (CaptureValue(Token, TEXT("-Course="), CourseCount, CourseValue)
            || CaptureValue(Token, TEXT("-Hole="), HoleCount, HoleValue)
            || CaptureValue(Token, TEXT("-PerformanceCaptureSeconds="), DurationCount, DurationValue)
            || CaptureValue(Token, TEXT("-ResX="), ResXCount, ResXValue)
            || CaptureValue(Token, TEXT("-ResY="), ResYCount, ResYValue)
            || CaptureValue(Token, TEXT("-UserDir="), UserDirCount, UserDirValue))
        {
            continue;
        }
        if (Token.Equals(TEXT("-ForceRes"), ESearchCase::CaseSensitive))
        {
            ++ForceResCount;
        }
        else if (Token.Equals(TEXT("-RenderOffscreen"), ESearchCase::CaseSensitive))
        {
            ++RenderOffscreenCount;
        }
        else if (Token.Equals(TEXT("-dx12"), ESearchCase::CaseSensitive))
        {
            ++Dx12Count;
        }
        else if (Token.Equals(TEXT("-unattended"), ESearchCase::CaseSensitive))
        {
            ++UnattendedCount;
        }
        else if (Token.Equals(TEXT("-NoLoadExistingSave"), ESearchCase::CaseSensitive))
        {
            ++NoLoadCount;
        }
        else if (Token.Equals(TEXT("-DGNoProfileWrites"), ESearchCase::CaseSensitive))
        {
            ++NoProfileWritesCount;
        }
        else
        {
            bUnknownArgument = true;
        }
    }

    if (!Result.bAttempted)
    {
        return Result;
    }
    if (!bIsUnattended || UnattendedCount != 1)
    {
        Result.Error = TEXT("release performance capture requires unattended mode and the -unattended flag");
        return Result;
    }
    const bool bExactCaptureArguments = CourseCount == 1
        && CourseValue.Equals(TEXT("PineRidge"), ESearchCase::CaseSensitive)
        && HoleCount == 1 && (HoleValue == TEXT("1") || HoleValue == TEXT("2") || HoleValue == TEXT("3"))
        && DurationCount == 1 && DurationValue == TEXT("30")
        && ResXCount == 1 && ResXValue == TEXT("1920")
        && ResYCount == 1 && ResYValue == TEXT("1080")
        && ForceResCount == 1 && RenderOffscreenCount == 1 && Dx12Count == 1
        && NoLoadCount == 1 && NoProfileWritesCount == 1
        && Arguments.Num() == 12 && !bUnknownArgument;
    if (!bExactCaptureArguments)
    {
        Result.Error = TEXT("release performance capture requires exact Pine Ridge hole, duration, resolution, offscreen, and D3D12 flags");
        return Result;
    }
    if (UserDirCount != 1 || UserDirValue.IsEmpty() || FPaths::IsRelative(UserDirValue)
        || UserDirValue.Contains(TEXT("../")) || UserDirValue.Contains(TEXT("..\\")))
    {
        Result.Error = TEXT("release performance capture requires a fresh external UUID UserDir");
        return Result;
    }

    const FString NormalizedSuppliedUserDir = NormalizedAbsoluteDirectory(UserDirValue);
    const FString NormalizedResolvedUserDir = NormalizedAbsoluteDirectory(ResolvedUserDir);
    const FString UserDirToken = FPaths::GetCleanFilename(NormalizedResolvedUserDir);
    if (!NormalizedSuppliedUserDir.Equals(NormalizedResolvedUserDir, ESearchCase::IgnoreCase)
        || !IsCanonicalLowerUuidV4(UserDirToken)
        || ProjectDirectory.IsEmpty() || RootDirectory.IsEmpty()
        || IsSameAsOrUnderDirectory(NormalizedResolvedUserDir, ProjectDirectory)
        || IsSameAsOrUnderDirectory(NormalizedResolvedUserDir, RootDirectory)
        || bCaptureDirectoryAlreadyExists || bSaveGameDirectoryAlreadyExists)
    {
        Result.Error = TEXT("release performance capture requires a fresh external UUID UserDir");
        return Result;
    }

    Result.bAccepted = true;
    Result.HoleNumber = FCString::Atoi(*HoleValue);
    Result.DurationSeconds = 30.0f;
    Result.UserDirToken = UserDirToken;
    return Result;
}

bool ADiscGolfTourGameMode::CapturePerformanceSnapshot()
{
    if (!IsPublicPerformanceCaptureMutationAllowed(DG_WITH_DEVELOPMENT_CONTENT != 0))
    {
        PerformanceStatusText = TEXT("Shipping public performance snapshot rejected.");
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Shipping public performance snapshot rejected."));
        return false;
    }
    return CapturePerformanceSnapshotInternal();
}

bool ADiscGolfTourGameMode::CapturePerformanceSnapshotInternal()
{
    RefreshPerformanceTelemetry();
    const FDiscGolfPerformanceBudget& Budget = PerformanceTracker.GetBudget();

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_performance_capture"));
    Root->SetNumberField(TEXT("version"), 2);
    Root->SetStringField(TEXT("captured_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("result"), DiscGolfPerformance::StateName(PerformanceBudgetState));
    Root->SetStringField(TEXT("course_id"), ActiveHole ? ActiveHole->CourseId.ToString() : TEXT("None"));
    Root->SetNumberField(TEXT("hole_number"), ActiveHole ? ActiveHole->HoleNumber : 0);

    const FString RhiName = GDynamicRHI ? FString(GDynamicRHI->GetName()) : TEXT("Unavailable");
    const bool bRendered = GDynamicRHI && !RhiName.Contains(TEXT("Null"), ESearchCase::IgnoreCase);
    FIntPoint ViewportSize = FIntPoint::ZeroValue;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
    }
    Root->SetStringField(TEXT("capture_profile"), PerformanceCaptureProfile);
    Root->SetStringField(TEXT("camera_route"), TEXT("authored_flyover_continuous"));
    Root->SetNumberField(TEXT("warmup_seconds"), 10.0f);
    Root->SetNumberField(TEXT("requested_duration_seconds"), PerformanceCaptureDurationSeconds);
    Root->SetBoolField(TEXT("rendered"), bRendered);
    Root->SetStringField(TEXT("rhi"), RhiName);
    Root->SetStringField(TEXT("gpu_brand"), GRHIAdapterName);
    Root->SetNumberField(TEXT("resolution_x"), ViewportSize.X);
    Root->SetNumberField(TEXT("resolution_y"), ViewportSize.Y);
#if WITH_EDITOR
    Root->SetStringField(TEXT("runtime_mode"), TEXT("editor_game"));
#else
    Root->SetStringField(TEXT("runtime_mode"), TEXT("packaged"));
#endif

    const Scalability::FQualityLevels Quality = Scalability::GetQualityLevels();
    TSharedRef<FJsonObject> QualityJson = MakeShared<FJsonObject>();
    QualityJson->SetNumberField(TEXT("resolution_quality"), Quality.ResolutionQuality);
    QualityJson->SetNumberField(TEXT("view_distance"), Quality.ViewDistanceQuality);
    QualityJson->SetNumberField(TEXT("anti_aliasing"), Quality.AntiAliasingQuality);
    QualityJson->SetNumberField(TEXT("shadow"), Quality.ShadowQuality);
    QualityJson->SetNumberField(TEXT("global_illumination"), Quality.GlobalIlluminationQuality);
    QualityJson->SetNumberField(TEXT("reflection"), Quality.ReflectionQuality);
    QualityJson->SetNumberField(TEXT("post_process"), Quality.PostProcessQuality);
    QualityJson->SetNumberField(TEXT("texture"), Quality.TextureQuality);
    QualityJson->SetNumberField(TEXT("effects"), Quality.EffectsQuality);
    QualityJson->SetNumberField(TEXT("foliage"), Quality.FoliageQuality);
    QualityJson->SetNumberField(TEXT("shading"), Quality.ShadingQuality);
    Root->SetObjectField(TEXT("quality"), QualityJson);

    bPerformanceCaptureRuntimeContractValid = true;
    if (bReleasePerformanceCapture)
    {
        const bool bQualityValid = FMath::IsNearlyEqual(Quality.ResolutionQuality, 100.0f, 0.01f)
            && Quality.ViewDistanceQuality == 2 && Quality.AntiAliasingQuality == 2
            && Quality.ShadowQuality == 2 && Quality.GlobalIlluminationQuality == 2
            && Quality.ReflectionQuality == 2 && Quality.PostProcessQuality == 2
            && Quality.TextureQuality == 2 && Quality.EffectsQuality == 2
            && Quality.FoliageQuality == 3 && Quality.ShadingQuality == 2;
        bPerformanceCaptureRuntimeContractValid = bRendered
            && RhiName.Equals(TEXT("D3D12"), ESearchCase::IgnoreCase)
            && !GRHIAdapterName.IsEmpty()
            && ViewportSize == FIntPoint(1920, 1080)
            && PerformanceCaptureProfile == TEXT("OmenGameplay1080pHighFoliageV1")
            && FMath::IsNearlyEqual(PerformanceCaptureDurationSeconds, 30.0f)
            && ActiveHole && ActiveHole->CourseId == TEXT("PineRidgeChampionship")
            && ActiveHole->HoleNumber == PerformanceCaptureRequestedHoleNumber
            && IsCourseFlyoverActive() && bQualityValid;
        if (!bPerformanceCaptureRuntimeContractValid)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: actual RHI, GPU, viewport, course, flyover, duration, or quality state differs from the guarded launch contract."));
        }
    }

    TSharedRef<FJsonObject> BudgetJson = MakeShared<FJsonObject>();
    BudgetJson->SetNumberField(TEXT("target_frame_ms"), Budget.TargetFrameTimeMs);
    BudgetJson->SetNumberField(TEXT("warning_p95_frame_ms"), Budget.WarningP95FrameTimeMs);
    BudgetJson->SetNumberField(TEXT("fail_p95_frame_ms"), Budget.FailP95FrameTimeMs);
    BudgetJson->SetNumberField(TEXT("hitch_frame_ms"), Budget.HitchFrameTimeMs);
    BudgetJson->SetNumberField(TEXT("max_hitch_rate_percent"), Budget.MaxHitchRatePercent);
    BudgetJson->SetNumberField(TEXT("warning_used_physical_bytes"), static_cast<double>(Budget.WarningUsedPhysicalBytes));
    BudgetJson->SetNumberField(TEXT("max_used_physical_bytes"), static_cast<double>(Budget.MaxUsedPhysicalBytes));
    BudgetJson->SetNumberField(TEXT("minimum_sample_count"), Budget.MinimumSampleCount);
    BudgetJson->SetNumberField(TEXT("window_sample_count"), Budget.WindowSampleCount);
    Root->SetObjectField(TEXT("budget"), BudgetJson);

    TSharedRef<FJsonObject> SummaryJson = MakeShared<FJsonObject>();
    SummaryJson->SetNumberField(TEXT("sample_count"), PerformanceSummary.SampleCount);
    SummaryJson->SetNumberField(TEXT("average_frame_ms"), PerformanceSummary.AverageFrameTimeMs);
    SummaryJson->SetNumberField(TEXT("p95_frame_ms"), PerformanceSummary.P95FrameTimeMs);
    SummaryJson->SetNumberField(TEXT("max_frame_ms"), PerformanceSummary.MaxFrameTimeMs);
    SummaryJson->SetNumberField(TEXT("average_fps"), PerformanceSummary.AverageFps);
    SummaryJson->SetNumberField(TEXT("hitch_count"), PerformanceSummary.HitchCount);
    SummaryJson->SetNumberField(TEXT("hitch_rate_percent"), PerformanceSummary.HitchRatePercent);
    SummaryJson->SetNumberField(TEXT("used_physical_bytes"), static_cast<double>(PerformanceSummary.UsedPhysicalBytes));
    Root->SetObjectField(TEXT("summary"), SummaryJson);

    FString JsonText;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
    if (!FJsonSerializer::Serialize(Root, Writer)) return false;

    const FString CaptureDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PerformanceCaptures"));
    IFileManager::Get().MakeDirectory(*CaptureDirectory, true);
    const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
    const FString TimestampedPath = FPaths::Combine(CaptureDirectory,
        FString::Printf(TEXT("Performance_%s.json"), *Stamp));
    const FString LatestPath = FPaths::Combine(CaptureDirectory, TEXT("LatestPerformance.json"));
    const bool bSaved = FFileHelper::SaveStringToFile(JsonText, *TimestampedPath)
        && FFileHelper::SaveStringToFile(JsonText, *LatestPath);
    if (bSaved)
    {
        UE_LOG(LogDiscGolfTour, Display, TEXT("Performance capture saved: %s | %s"),
            *DiscGolfPerformance::StateName(PerformanceBudgetState), *TimestampedPath);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error, TEXT("Performance capture could not be written to %s."), *CaptureDirectory);
    }
    return bSaved;
}

void ADiscGolfTourGameMode::FinishPerformanceCapture()
{
    bPerformanceCaptureActive = false;
    const bool bSaved = CapturePerformanceSnapshotInternal();
    const bool bPassed = bSaved
        && PerformanceBudgetState == EDiscGolfPerformanceBudgetState::Pass
        && (!bReleasePerformanceCapture || bPerformanceCaptureRuntimeContractValid);
    UE_LOG(LogDiscGolfTour, Display, TEXT("PERFORMANCE CAPTURE %s: %s"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), *PerformanceStatusText);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::StartPerformanceCaptureSamples()
{
    if (!bPerformanceCaptureActive) return;
    if (bReleasePerformanceCapture
        && (!IsCourseFlyoverActive() || !ActiveHole
            || ActiveHole->HoleNumber != PerformanceCaptureRequestedHoleNumber))
    {
        bPerformanceCaptureActive = false;
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: the authored flyover did not remain active through warm-up."));
        FPlatformMisc::RequestExitWithStatus(false, 2);
        return;
    }
    ResetPerformanceTelemetryInternal();
    FTimerHandle PerformanceCaptureTimer;
    GetWorldTimerManager().SetTimer(PerformanceCaptureTimer, FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::FinishPerformanceCapture),
        PerformanceCaptureDurationSeconds, false);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Performance capture sampling started for %.1f seconds after 10.0 seconds warm-up."),
        PerformanceCaptureDurationSeconds);
}

bool ADiscGolfTourGameMode::BeginPerformanceCaptureSequence()
{
    ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr;
    if (bReleasePerformanceCapture && !Route)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: the authored flyover route is unavailable."));
        FPlatformMisc::RequestExitWithStatus(false, 2);
        return false;
    }

    bPerformanceCaptureActive = true;
    if (Route)
    {
        Route->PreviewDurationSeconds = PerformanceCaptureDurationSeconds + 12.0f;
    }
    FTimerHandle PerformanceFlyoverTimer;
    GetWorldTimerManager().SetTimer(PerformanceFlyoverTimer, FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::PreviewCourseFlyover), 0.5f, false);
    FTimerHandle PerformanceWarmupTimer;
    GetWorldTimerManager().SetTimer(PerformanceWarmupTimer, FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::StartPerformanceCaptureSamples), 10.0f, false);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Performance capture warming authored flyover and asset residency for 10.0 seconds."));
    return true;
}

void ADiscGolfTourGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->OnPlayabilityFailure.AddDynamic(
            this, &ADiscGolfTourGameMode::HandlePlayabilityFailure);
        PlayabilityMonitor->ResetMonitor();
    }

    bool bPerformanceCaptureRequested = false;
    bool bDeveloperToolAutomated = false;
    bool bDeveloperToolPineRidge = false;
    bool bDeveloperToolPerformanceProfile = false;
    bool bDeveloperToolExclusiveCapture = false;
    bool bRouteTelemetrySmokeRequested = false;
    bool bRouteTelemetryRequested = false;
    bool bGalleryLakeWaterSmokeRequested = false;
    bool bDenseForestSmokeRequested = false;
    bool bGroundGrassSmokeRequested = false;
    bool bHole1FlightRouteSmokeRequested = false;
    bool bSession12PresentationSmokeRequested = false;
    bool bSession16MainMenuSmokeRequested = false;
    bool bSession17RoundFlowSmokeRequested = false;
    bool bSession16RulesSmokeRequested = false;
    bool bRegressionSuiteSmokeRequested = false;
    bool bAutomatedLaunch = false;
#if DG_WITH_RELEASE_PERFORMANCE_CAPTURE
    const FString ReleaseCaptureCommandLine(FCommandLine::Get());
    const bool bReleaseCaptureAttempted = ReleaseCaptureCommandLine.Contains(
        TEXT("-PerformanceCapture"), ESearchCase::IgnoreCase);
    if (bReleaseCaptureAttempted)
    {
        const FString CaptureDirectory = FPaths::Combine(
            FPaths::ProjectSavedDir(), TEXT("PerformanceCaptures"));
        const FString SaveGameDirectory = FPaths::Combine(
            FPaths::ProjectSavedDir(), TEXT("SaveGames"));
        const FDiscGolfReleasePerformanceCaptureLaunchValidation ReleaseCapture =
            ValidateReleasePerformanceCaptureLaunch(
                ReleaseCaptureCommandLine,
                FApp::IsUnattended(),
                FPaths::ProjectUserDir(),
                FPaths::ProjectDir(),
                FPaths::RootDir(),
                IFileManager::Get().DirectoryExists(*CaptureDirectory),
                IFileManager::Get().DirectoryExists(*SaveGameDirectory));
        if (!ReleaseCapture.bAccepted)
        {
            PerformanceStatusText = FString::Printf(
                TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: %s"), *ReleaseCapture.Error);
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: %s"), *ReleaseCapture.Error);
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return;
        }
        if (!IFileManager::Get().MakeDirectory(*CaptureDirectory, true))
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: fresh capture namespace could not be created."));
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return;
        }
        TUniquePtr<FArchive> GuardWriter;
        FString GuardError;
        const FString GuardPath = FPaths::Combine(
            CaptureDirectory, TEXT("ReleasePerformanceCapture.guard"));
        if (!DiscGolfRuntimeCheckpointJournal::TryCreateExclusiveJournalWriter(
                GuardPath, GuardWriter, GuardError)
            || !GuardWriter || !GuardWriter->Close())
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("RELEASE PERFORMANCE CAPTURE REJECTED: exclusive capture namespace claim failed."));
            FPlatformMisc::RequestExitWithStatus(false, 2);
            return;
        }
        bReleasePerformanceCapture = true;
        bPerformanceCaptureRequested = true;
        bPerformanceCaptureRuntimeContractValid = false;
        PerformanceCaptureDurationSeconds = ReleaseCapture.DurationSeconds;
        PerformanceCaptureRequestedHoleNumber = ReleaseCapture.HoleNumber;
        ApplyOmenPerformanceCaptureProfile();
        bAutomatedLaunch = true;
    }
#endif
#if DG_WITH_DEVELOPMENT_CONTENT
    bDeveloperHudVisible = FParse::Param(FCommandLine::Get(), TEXT("DeveloperHUD"));
    bShotTracerEnabled = bDeveloperHudVisible
        || FParse::Param(FCommandLine::Get(), TEXT("ShotTracer"));
    float RequestedPerformanceCaptureSeconds = 0.0f;
    bPerformanceCaptureRequested = FParse::Value(
        FCommandLine::Get(), TEXT("PerformanceCaptureSeconds="), RequestedPerformanceCaptureSeconds);
    if (bPerformanceCaptureRequested)
    {
        PerformanceCaptureDurationSeconds = FMath::Clamp(
            RequestedPerformanceCaptureSeconds, 5.0f, 300.0f);
        ApplyOmenPerformanceCaptureProfile();
    }
    bDeveloperToolAutomated = FParse::Param(
        FCommandLine::Get(), TEXT("DGDeveloperToolAutomation"));
    bDeveloperToolPineRidge = FParse::Param(
        FCommandLine::Get(), TEXT("DGDeveloperToolPineRidge"));
    bDeveloperToolPerformanceProfile = FParse::Param(
        FCommandLine::Get(), TEXT("DGDeveloperToolPerformanceProfile"));
    bDeveloperToolExclusiveCapture = FParse::Param(
        FCommandLine::Get(), TEXT("DGDeveloperToolExclusiveCapture"));
    if (bDeveloperToolPerformanceProfile)
    {
        ApplyOmenPerformanceCaptureProfile();
    }
    bRouteTelemetrySmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("NeedleGateRouteTelemetrySmokeTest"));
    bRouteTelemetryRequested = bRouteTelemetrySmokeRequested || FParse::Param(
        FCommandLine::Get(), TEXT("NeedleGateRouteTelemetry"));
    bGalleryLakeWaterSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("GalleryLakeWaterSmokeTest"));
    bDenseForestSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("DenseForestSmokeTest"));
    bGroundGrassSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("GroundGrassSmokeTest"));
    bHole1FlightRouteSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("Hole1FlightRouteSmokeTest"));
    bSession12PresentationSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("Session12PresentationSmokeTest"));
    bSession16MainMenuSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("Session16MainMenuSmokeTest"));
    bSession17RoundFlowSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("Session17RoundFlowSmokeTest"));
    bSession16RulesSmokeRequested = FParse::Value(
        FCommandLine::Get(), TEXT("Session16RulesSmokeTest="), Session16RulesSmokeScenario);
    bRegressionSuiteSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("RegressionSuiteSmokeTest"));
    FString IgnoredVisualQAName;
    bAutomatedLaunch = FApp::IsUnattended()
        || FParse::Param(FCommandLine::Get(), TEXT("ThreeHoleRoundSmokeTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("CourseSmokeTest"))
        || bRegressionSuiteSmokeRequested
        || FParse::Param(FCommandLine::Get(), TEXT("PineRidgePlaySmokeTest"))
        || bDeveloperToolAutomated
        || bSession12PresentationSmokeRequested
        || bHole1FlightRouteSmokeRequested
        || bSession16MainMenuSmokeRequested
        || bSession17RoundFlowSmokeRequested
        || bSession16RulesSmokeRequested
        || FParse::Param(FCommandLine::Get(), TEXT("FixtureCollisionSmokeTest"))
        || bGalleryLakeWaterSmokeRequested
        || bDenseForestSmokeRequested
        || bGroundGrassSmokeRequested
        || bRouteTelemetryRequested
        || FParse::Value(FCommandLine::Get(), TEXT("VisualQAScreenshot="), IgnoredVisualQAName)
        || bPerformanceCaptureRequested;
#endif

    // This opt-in must own its exclusive file and round UUID before LoadCourse
    // reaches the initial StartHole. Normal play has none of these variables
    // and pays no journal I/O cost.
    InitializeRuntimeCheckpointJournal();

    // A regression-smoke process owns Latest from the moment its launch flag is
    // parsed. Publish the fail-closed marker before world/course bootstrap or
    // any competing automation branch can return, so a prior PASS can never be
    // mistaken for evidence from this process.
    if (bRegressionSuiteSmokeRequested)
    {
        bRegressionSuiteSmokeTestActive = true;
        if (UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem())
        {
            FString MarkerError;
            LastRegressionReportPath.Reset();
            if (!Trajectories->BeginRegressionSuiteReport(
                    LastRegressionReportPath, MarkerError))
            {
                const FString Failure = FString::Printf(
                    TEXT("Regression report startup marker failed: %s"), *MarkerError);
                FailRegressionSuiteSmoke(Failure);
                return;
            }
        }
        else
        {
            FailRegressionSuiteSmoke(
                TEXT("Trajectory subsystem is unavailable at smoke startup"));
            return;
        }
    }

    WindDirector = GetWorld()->SpawnActor<AWindDirector>();
    DevBootstrap = GetWorld()->SpawnActor<ADevCourseBootstrap>();
    FString InitialCourse = (bRouteTelemetryRequested || bGalleryLakeWaterSmokeRequested
        || bDenseForestSmokeRequested || bGroundGrassSmokeRequested
        || bHole1FlightRouteSmokeRequested || bDeveloperToolPineRidge
        || bSession12PresentationSmokeRequested
        || bSession16MainMenuSmokeRequested
        || bSession17RoundFlowSmokeRequested
        || !bAutomatedLaunch)
        ? TEXT("PineRidge") : TEXT("Regression");
    const bool bCourseOverridden = FParse::Value(FCommandLine::Get(), TEXT("Course="), InitialCourse);
    if (!LoadCourse(InitialCourse))
    {
        LoadCourse(TEXT("Regression"));
    }
    int32 InitialHoleNumber = bRouteTelemetryRequested ? 2
        : bGalleryLakeWaterSmokeRequested ? 3 : 1;
    const bool bHoleOverridden = FParse::Value(FCommandLine::Get(), TEXT("Hole="), InitialHoleNumber);
    if ((bHoleOverridden || bRouteTelemetryRequested || bGalleryLakeWaterSmokeRequested)
        && InitialHoleNumber > 1 && HasAuthoredRound())
    {
        LoadRoundHole(InitialHoleNumber);
    }
#if DG_WITH_DEVELOPMENT_CONTENT
    if (FParse::Param(FCommandLine::Get(), TEXT("FixturePresentationGallery")))
    {
        SpawnFixturePresentationGallery();
    }
#endif

    if (!bCourseOverridden && !bHoleOverridden && !bAutomatedLaunch)
    {
        RestorePracticeRoundSnapshot();
    }
    if (bSession16MainMenuSmokeRequested || bSession17RoundFlowSmokeRequested
        || !bAutomatedLaunch)
    {
        ShowMainMenu();
    }

    if (UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem())
    {
#if DG_WITH_DEVELOPMENT_CONTENT
        TrajectoryStatusText = FString::Printf(TEXT("AUTO EXPORT READY - %d regression presets"),
            Trajectories->GetRegressionPresets().Num());
#else
        TrajectoryStatusText = TEXT("IN-MEMORY TRAJECTORY SUMMARY READY");
#endif
    }
#if DG_WITH_RELEASE_PERFORMANCE_CAPTURE
    if (bPerformanceCaptureRequested)
    {
        BeginPerformanceCaptureSequence();
        return;
    }
#endif
#if DG_WITH_DEVELOPMENT_CONTENT
    if (bRouteTelemetryRequested && !bRouteTelemetrySmokeRequested)
    {
        StartNeedleGateRouteTelemetry(false);
    }

    if (bPerformanceCaptureRequested && !bDeveloperToolExclusiveCapture)
    {
        BeginPerformanceCaptureSequence();
        return;
    }

    FString VisualQAName;
    if (!bDeveloperToolExclusiveCapture
        && FParse::Value(
            FCommandLine::Get(), TEXT("VisualQAScreenshot="), VisualQAName))
    {
        VisualQAName.ReplaceInline(TEXT("/"), TEXT("_"));
        VisualQAName.ReplaceInline(TEXT("\\"), TEXT("_"));
        VisualQAName.ReplaceInline(TEXT(".."), TEXT("_"));
        if (VisualQAName.IsEmpty()) VisualQAName = TEXT("DiscGolfVisualQA");
        bScorecardVisible = FParse::Param(FCommandLine::Get(), TEXT("ShowScorecard"));
        if (FParse::Param(FCommandLine::Get(), TEXT("ShorelineDressingVisualQA")))
        {
            FTimerHandle ShoreCameraTimer;
            GetWorldTimerManager().SetTimer(ShoreCameraTimer, FTimerDelegate::CreateUObject(
                this, &ADiscGolfTourGameMode::PositionShorelineDressingVisualQACamera), 0.5f, false);
        }
        else if (FParse::Param(FCommandLine::Get(), TEXT("TrailWearVisualQA")))
        {
            FTimerHandle TrailCameraTimer;
            GetWorldTimerManager().SetTimer(TrailCameraTimer, FTimerDelegate::CreateUObject(
                this, &ADiscGolfTourGameMode::PositionTrailWearVisualQACamera), 0.5f, false);
        }
        else if (FParse::Param(FCommandLine::Get(), TEXT("GalleryLakeWaterVisualQA")))
        {
            FTimerHandle WaterCameraTimer;
            GetWorldTimerManager().SetTimer(WaterCameraTimer, FTimerDelegate::CreateUObject(
                this, &ADiscGolfTourGameMode::PositionGalleryLakeWaterVisualQACamera), 0.5f, false);
        }
        else if (FParse::Param(FCommandLine::Get(), TEXT("GroundCoverVisualQA")))
        {
            FTimerHandle GroundCoverCameraTimer;
            GetWorldTimerManager().SetTimer(GroundCoverCameraTimer, FTimerDelegate::CreateUObject(
                this, &ADiscGolfTourGameMode::PositionGroundCoverVisualQACamera), 0.5f, false);
        }
        VisualQAScreenshotPath = FPaths::Combine(FPaths::ScreenShotDir(), VisualQAName + TEXT(".png"));
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::CaptureVisualQAScreenshot), 1.0f, false);
        FTimerHandle FinishTimer;
        GetWorldTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishVisualQAScreenshot), 2.5f, false);
        return;
    }

    if (bGroundGrassSmokeRequested)
    {
        FTimerHandle GroundGrassSmokeTimer;
        GetWorldTimerManager().SetTimer(GroundGrassSmokeTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishGroundGrassSmokeTest), 0.75f, false);
    }
    else if (bDenseForestSmokeRequested)
    {
        FTimerHandle ForestSmokeTimer;
        GetWorldTimerManager().SetTimer(ForestSmokeTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishDenseForestSmokeTest), 0.75f, false);
    }
    else if (bGalleryLakeWaterSmokeRequested)
    {
        FTimerHandle WaterSmokeTimer;
        GetWorldTimerManager().SetTimer(WaterSmokeTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishGalleryLakeWaterSmokeTest), 0.75f, false);
    }
    else if (bSession17RoundFlowSmokeRequested)
    {
        FTimerHandle RoundFlowSmokeTimer;
        GetWorldTimerManager().SetTimer(RoundFlowSmokeTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::RunSession17RoundFlowSmokeTest), 0.5f, false);
    }
    else if (bSession16MainMenuSmokeRequested)
    {
        FTimerHandle MainMenuSmokeTimer;
        GetWorldTimerManager().SetTimer(MainMenuSmokeTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::RunSession16MainMenuSmokeTest), 0.5f, false);
    }
    else if (bSession16RulesSmokeRequested)
    {
        FTimerHandle Session16RulesTimer;
        GetWorldTimerManager().SetTimer(Session16RulesTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::StartSession16RulesSmokeTest), 0.5f, false);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("ThreeHoleRoundSmokeTest")))
    {
        bThreeHoleRoundSmokeTestActive = true;
        FTimerHandle RoundTimer;
        GetWorldTimerManager().SetTimer(RoundTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::StartThreeHoleRoundSmokeTest), 0.5f, false);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("CourseSmokeTest")))
    {
        bCourseSmokeTestActive = true;
        FTimerHandle PreviewTimer;
        GetWorldTimerManager().SetTimer(PreviewTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::PreviewCourseFlyover), 0.5f, false);
        FTimerHandle FinishTimer;
        GetWorldTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishCourseSmokeTest), 10.5f, false);
    }
    else if (bRegressionSuiteSmokeRequested)
    {
        FTimerHandle SuiteTimer;
        GetWorldTimerManager().SetTimer(SuiteTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::RunPhysicsRegressionSuite), 0.5f, false);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("PineRidgePlaySmokeTest"))
        || bSession12PresentationSmokeRequested)
    {
        bPineRidgePlaySmokeTestActive = true;
        bSession12PresentationSmokeTestActive = bSession12PresentationSmokeRequested;
        FTimerHandle PlayTimer;
        GetWorldTimerManager().SetTimer(PlayTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::StartPineRidgePlaySmokeTest), 0.5f, false);
    }
    else if (bHole1FlightRouteSmokeRequested)
    {
        bHole1FlightRouteSmokeTestActive = true;
        FTimerHandle RouteTimer;
        GetWorldTimerManager().SetTimer(RouteTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::StartHole1FlightRouteSmokeTest), 0.5f, false);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("FixtureCollisionSmokeTest")))
    {
        FixtureQaRunner = GetWorld()->SpawnActor<ADiscGolfFixtureQaRunner>();
        if (!FixtureQaRunner)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("FIXTURE COLLISION SMOKE FAIL: QA runner could not spawn."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
        FTimerHandle FixtureQaTimer;
        GetWorldTimerManager().SetTimer(FixtureQaTimer, FTimerDelegate::CreateUObject(
            FixtureQaRunner, &ADiscGolfFixtureQaRunner::Start), 0.5f, false);
    }
    else if (bRouteTelemetrySmokeRequested)
    {
        const bool bStarted = StartNeedleGateRouteTelemetry(bRouteTelemetrySmokeRequested);
        if (bRouteTelemetrySmokeRequested)
        {
            if (!bStarted)
            {
                UE_LOG(LogDiscGolfTour, Error,
                    TEXT("NEEDLE GATE ROUTE TELEMETRY SMOKE FAIL: session could not start."));
                FPlatformMisc::RequestExitWithStatus(false, 1);
                return;
            }
            FTimerHandle RouteTelemetrySmokeTimer;
            GetWorldTimerManager().SetTimer(RouteTelemetrySmokeTimer, FTimerDelegate::CreateUObject(
                this, &ADiscGolfTourGameMode::FinishRouteTelemetrySmokeTest), 0.25f, false);
        }
    }
#endif
}

void ADiscGolfTourGameMode::FinishRouteTelemetrySmokeTest()
{
    const int64 ReportSize = IFileManager::Get().FileSize(*LastRouteTelemetryReportPath);
    const bool bPassed = bRouteTelemetryActive
        && ActiveHole && ActiveHole->HoleNumber == 2
        && RouteTelemetrySession.Routes.Num() == 3
        && RouteTelemetrySession.LandingZones.Num() == 2
        && RouteTelemetrySession.Attempts.IsEmpty()
        && RouteTelemetrySession.TargetAttemptsPerRoute == 20
        && RouteTelemetrySession.CollisionProfileId == TEXT("PineRidgeCompetitiveV2_Fixtures")
        && ReportSize > 0;
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("NEEDLE GATE ROUTE TELEMETRY SMOKE PASS: 3 routes, 20 attempts each, collision profile %s, review overlay active, schema-v1 report %lld bytes."),
            *RouteTelemetrySession.CollisionProfileId.ToString(), ReportSize);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("NEEDLE GATE ROUTE TELEMETRY SMOKE FAIL: active=%d hole=%d routes=%d zones=%d attempts=%d target=%d profile=%s report=%lld."),
            bRouteTelemetryActive ? 1 : 0, ActiveHole ? ActiveHole->HoleNumber : -1,
            RouteTelemetrySession.Routes.Num(), RouteTelemetrySession.LandingZones.Num(),
            RouteTelemetrySession.Attempts.Num(), RouteTelemetrySession.TargetAttemptsPerRoute,
            *RouteTelemetrySession.CollisionProfileId.ToString(), ReportSize);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

const FDiscGolfHoleBlockoutDefinition* ADiscGolfTourGameMode::GetActiveHoleDefinition() const
{
    if (!ActiveHole) return nullptr;
    return ActiveHoleDefinitions.FindByPredicate([this](const FDiscGolfHoleBlockoutDefinition& Definition)
    {
        return Definition.HoleNumber == ActiveHole->HoleNumber;
    });
}

bool ADiscGolfTourGameMode::StartNeedleGateRouteTelemetry(bool bResetExisting)
{
    if (ActiveDisc || bRegressionActive || ReplayActor || IsCourseFlyoverActive())
    {
        RouteTelemetryLastResultText = TEXT("Finish the active shot or playback before starting telemetry");
        return false;
    }
    if (!HasAuthoredRound() && !LoadCourse(TEXT("PineRidge"))) return false;
    if (!ActiveHole || ActiveHole->HoleNumber != 2)
    {
        if (!LoadRoundHole(2)) return false;
    }
    const FDiscGolfHoleBlockoutDefinition* Definition = GetActiveHoleDefinition();
    if (!Definition || Definition->HoleNumber != 2 || Definition->ShotRoutes.Num() != 3)
    {
        RouteTelemetryLastResultText = TEXT("Needle Gate route definitions are unavailable");
        return false;
    }

    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RouteTelemetryReports"));
    const FString LatestPath = FPaths::Combine(Directory, TEXT("LatestRouteTelemetry.json"));
    FDiscGolfRouteTelemetrySession Session;
    FString LoadError;
    const bool bResumed = !bResetExisting && IFileManager::Get().FileSize(*LatestPath) > 0
        && DiscGolfRouteTelemetry::LoadSession(LatestPath, Session, LoadError)
        && DiscGolfRouteTelemetry::IsCompatible(
            Session, Definition->CourseId, Definition->LayoutId,
            ActiveCoursePresentation.CollisionProfileId, Definition->HoleNumber);
    if (!bResumed)
    {
        Session = FDiscGolfRouteTelemetrySession();
        Session.SessionId = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
        Session.StartedUtc = FDateTime::UtcNow().ToIso8601();
        Session.CourseId = Definition->CourseId;
        Session.LayoutId = Definition->LayoutId;
        Session.CollisionProfileId = ActiveCoursePresentation.CollisionProfileId;
        Session.HoleNumber = Definition->HoleNumber;
        Session.TargetAttemptsPerRoute = 20;
    }
    Session.Routes = Definition->ShotRoutes;
    Session.LandingZones = Definition->LandingZones;
    const bool bActiveRouteValid = Session.Routes.ContainsByPredicate([&Session](const FDiscGolfShotRouteDefinition& Route)
    {
        return Route.RouteId == Session.ActiveRouteId;
    });
    if (!bActiveRouteValid)
    {
        const FDiscGolfShotRouteDefinition* FirstIncomplete = Session.Routes.FindByPredicate(
            [&Session](const FDiscGolfShotRouteDefinition& Route)
            {
                return DiscGolfRouteTelemetry::CountAttempts(Session, Route.RouteId) < Session.TargetAttemptsPerRoute;
            });
        Session.ActiveRouteId = FirstIncomplete ? FirstIncomplete->RouteId : Session.Routes[0].RouteId;
    }
    RouteTelemetrySession = MoveTemp(Session);
    bRouteTelemetryActive = true;
    bRouteTelemetryShotPending = false;
    PendingRouteTradeoffUnderstood = -1;
    LastRouteTelemetryAttemptIndex = RouteTelemetrySession.Attempts.IsEmpty()
        ? INDEX_NONE : RouteTelemetrySession.Attempts.Num() - 1;
    RouteTelemetryAttemptAwaitingScoreIndex = INDEX_NONE;
    RouteTelemetryLastResultText = bResumed
        ? FString::Printf(TEXT("Resumed %d recorded attempts"), RouteTelemetrySession.Attempts.Num())
        : TEXT("New session ready; rate tradeoff before the tee shot");
    RefreshRouteTelemetryStatus();
    SaveRouteTelemetry();
    UE_LOG(LogDiscGolfTour, Display, TEXT("NEEDLE GATE ROUTE TELEMETRY %s: %s"),
        bResumed ? TEXT("RESUMED") : TEXT("STARTED"), *RouteTelemetryProgressText);
    return true;
}

bool ADiscGolfTourGameMode::SelectRouteTelemetry(const FString& RouteId)
{
    if (!bRouteTelemetryActive || ActiveDisc) return false;
    const FName Requested(*RouteId);
    const FDiscGolfShotRouteDefinition* Route = RouteTelemetrySession.Routes.FindByPredicate(
        [Requested](const FDiscGolfShotRouteDefinition& Candidate) { return Candidate.RouteId == Requested; });
    if (!Route)
    {
        RouteTelemetryLastResultText = FString::Printf(TEXT("Unknown route: %s"), *RouteId);
        return false;
    }
    RouteTelemetrySession.ActiveRouteId = Route->RouteId;
    PendingRouteTradeoffUnderstood = -1;
    RouteTelemetryLastResultText = TEXT("Route selected; set tradeoff rating, then throw from the tee");
    RefreshRouteTelemetryStatus();
    return SaveRouteTelemetry();
}

bool ADiscGolfTourGameMode::SetRouteTelemetryTradeoffUnderstood(int32 Understood)
{
    if (!bRouteTelemetryActive || ActiveDisc || (Understood != 0 && Understood != 1)) return false;
    PendingRouteTradeoffUnderstood = Understood;
    RouteTelemetryLastResultText = Understood > 0
        ? TEXT("Tradeoff marked understood for the next tee shot")
        : TEXT("Tradeoff marked unclear for the next tee shot");
    return true;
}

bool ADiscGolfTourGameMode::SetRouteTelemetryNextShotClear(int32 Clear)
{
    if (!bRouteTelemetryActive || !RouteTelemetrySession.Attempts.IsValidIndex(LastRouteTelemetryAttemptIndex)
        || (Clear != 0 && Clear != 1)) return false;
    RouteTelemetrySession.Attempts[LastRouteTelemetryAttemptIndex].NextShotClear = Clear;
    RouteTelemetryLastResultText += Clear > 0 ? TEXT(" // next shot clear") : TEXT(" // next shot obstructed");
    return SaveRouteTelemetry();
}

bool ADiscGolfTourGameMode::SaveRouteTelemetry()
{
    if (!bRouteTelemetryActive) return false;
    FString Error;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("RouteTelemetryReports"));
    if (!DiscGolfRouteTelemetry::SaveSession(
        RouteTelemetrySession, Directory, LastRouteTelemetryReportPath, Error))
    {
        RouteTelemetryLastResultText = FString::Printf(TEXT("Route report failed: %s"), *Error);
        UE_LOG(LogDiscGolfTour, Error, TEXT("Route telemetry report failed: %s"), *Error);
        return false;
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("Route telemetry report saved: %s"), *LastRouteTelemetryReportPath);
    return true;
}

void ADiscGolfTourGameMode::StopRouteTelemetry()
{
    if (bRouteTelemetryActive) SaveRouteTelemetry();
    bRouteTelemetryActive = false;
    bRouteTelemetryShotPending = false;
    PendingRouteTradeoffUnderstood = -1;
    RouteTelemetryAttemptAwaitingScoreIndex = INDEX_NONE;
    RouteTelemetryRouteLabel = TEXT("ROUTE TELEMETRY OFF");
    RouteTelemetryProgressText.Reset();
    RouteTelemetryIntentText.Reset();
}

void ADiscGolfTourGameMode::RefreshRouteTelemetryStatus()
{
    if (!bRouteTelemetryActive) return;
    const FDiscGolfShotRouteDefinition* ActiveRoute = RouteTelemetrySession.Routes.FindByPredicate(
        [this](const FDiscGolfShotRouteDefinition& Route)
        {
            return Route.RouteId == RouteTelemetrySession.ActiveRouteId;
        });
    if (!ActiveRoute) return;
    const int32 RouteCount = DiscGolfRouteTelemetry::CountAttempts(
        RouteTelemetrySession, ActiveRoute->RouteId);
    RouteTelemetryRouteLabel = FString::Printf(TEXT("%s // %s"),
        *DiscGolfRouteTelemetry::RouteTypeName(ActiveRoute->RouteType).ToUpper(),
        *ActiveRoute->Label.ToString().ToUpper());
    RouteTelemetryProgressText = FString::Printf(TEXT("ROUTE %d/%d // TOTAL %d/%d%s"),
        RouteCount, RouteTelemetrySession.TargetAttemptsPerRoute,
        RouteTelemetrySession.Attempts.Num(),
        RouteTelemetrySession.TargetAttemptsPerRoute * RouteTelemetrySession.Routes.Num(),
        DiscGolfRouteTelemetry::IsComplete(RouteTelemetrySession) ? TEXT(" // COMPLETE") : TEXT(""));
    RouteTelemetryIntentText = ActiveRoute->ShotIntent.ToString();
}

bool ADiscGolfTourGameMode::HasRouteTelemetryBasketVisibility(
    const FVector& FromLocationCm,
    const ADiscActor* Disc) const
{
    if (!GetWorld() || !ActiveHole) return false;
    const FVector Start = FromLocationCm + FVector(0.0f, 0.0f, 125.0f);
    const FVector End = ActiveHole->BasketLocation + FVector(0.0f, 0.0f, 125.0f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGolfRouteTelemetryVisibility), false);
    if (Disc) Params.AddIgnoredActor(Disc);
    if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0)) Params.AddIgnoredActor(Pawn);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return true;
    return FVector::DistSquared(Hit.ImpactPoint, End) <= FMath::Square(250.0f);
}

void ADiscGolfTourGameMode::CaptureRouteTelemetryAttempt(ADiscActor* Disc, bool bHoledOut)
{
    if (!bRouteTelemetryActive || !bRouteTelemetryShotPending || !Disc || !ActiveHole) return;
    bRouteTelemetryShotPending = false;
    const FDiscGolfShotRouteDefinition* Route = RouteTelemetrySession.Routes.FindByPredicate(
        [this](const FDiscGolfShotRouteDefinition& Candidate)
        {
            return Candidate.RouteId == RouteTelemetrySession.ActiveRouteId;
        });
    const FDiscGolfLandingZoneDefinition* Zone = Route ? RouteTelemetrySession.LandingZones.FindByPredicate(
        [Route](const FDiscGolfLandingZoneDefinition& Candidate)
        {
            return Candidate.ZoneId == Route->LandingZoneId;
        }) : nullptr;
    const UDiscFlightComponent* Flight = Disc->GetFlightComponent();
    if (!Route || !Zone || !Flight) return;

    const TArray<FDiscTrajectorySample>& Samples = Flight->GetTrajectorySamples();
    const FVector RawFinal = CurrentLieState.RawDiscLocationCm.IsNearlyZero()
        ? Disc->GetActorLocation() : CurrentLieState.RawDiscLocationCm;
    const FDiscGolfRouteTelemetryEvaluation Evaluation = DiscGolfRouteTelemetry::EvaluateRouteShot(
        *Route, *Zone, Samples, RawFinal);
    FDiscGolfRouteTelemetryAttempt Attempt;
    Attempt.AttemptNumber = RouteTelemetrySession.Attempts.Num() + 1;
    Attempt.RouteId = Route->RouteId;
    Attempt.RecordedUtc = FDateTime::UtcNow().ToIso8601();
    Attempt.ReleaseLocationCm = Samples.IsEmpty() ? ThrowStartLieLocation : Samples[0].WorldLocationCm;
    Attempt.RawFinalLocationCm = RawFinal;
    Attempt.LieLocationCm = CurrentLieState.LieLocationCm;
    Attempt.bLandingZoneHit = Evaluation.bLandingZoneHit;
    Attempt.MissSide = Evaluation.MissSide;
    Attempt.CorridorSampleCount = Evaluation.CorridorSampleCount;
    Attempt.CorridorSamplesInside = Evaluation.CorridorSamplesInside;
    Attempt.CorridorAdherencePercent = Evaluation.CorridorAdherencePercent;
    Attempt.MaximumCorridorDeviationMeters = Evaluation.MaximumCorridorDeviationMeters;
    Attempt.PenaltyType = CurrentLieState.PenaltyType;
    Attempt.PenaltyStrokes = CurrentLieState.PenaltyStrokes;
    Attempt.RemainingDistanceMeters = FVector::Dist2D(CurrentLieState.LieLocationCm, ActiveHole->BasketLocation) / 100.0f;
    Attempt.bBasketVisible = HasRouteTelemetryBasketVisibility(CurrentLieState.LieLocationCm, Disc);
    Attempt.TradeoffUnderstood = PendingRouteTradeoffUnderstood;
    Attempt.bHoledOutOnRouteShot = bHoledOut;
    Attempt.FinalHoleScore = bHoledOut ? Strokes : -1;
    Attempt.FixtureContactCount = LastFlightTelemetry.FixtureContactCount;
    Attempt.LastFixtureType = LastFlightTelemetry.LastFixtureType;
    Attempt.FlightTimeSeconds = LastFlightTelemetry.FlightTimeSeconds;
    Attempt.FinalCarryMeters = LastFlightTelemetry.CarryMeters;
    Attempt.MoldId = Disc->GetResolvedDisc().MoldId;
    Attempt.Plastic = Disc->GetResolvedDisc().Plastic;
    Attempt.ThrowStyle = LastRelease.ThrowStyle;
    Attempt.ReleaseSpeedMps = LastRelease.ReleaseSpeedMps;
    Attempt.ReleaseSpinRpm = LastRelease.SpinRpm;
    Attempt.ReleaseHyzerDeg = LastRelease.EffectiveHyzerDeg;
    Attempt.ReleaseNoseDeg = LastRelease.EffectiveNoseAngleDeg;
    Attempt.ReleaseLaunchDeg = LastRelease.EffectiveLaunchAngleDeg;
    Attempt.ReleaseQuality01 = LastRelease.Quality01;
    LastRouteTelemetryAttemptIndex = RouteTelemetrySession.Attempts.Add(MoveTemp(Attempt));
    RouteTelemetryAttemptAwaitingScoreIndex = bHoledOut ? INDEX_NONE : LastRouteTelemetryAttemptIndex;
    PendingRouteTradeoffUnderstood = -1;

    const FDiscGolfRouteTelemetryAttempt& Recorded = RouteTelemetrySession.Attempts[LastRouteTelemetryAttemptIndex];
    RouteTelemetryLastResultText = FString::Printf(TEXT("%s // %s // corridor %.0f%% // %.1f m left%s"),
        Recorded.bLandingZoneHit ? TEXT("LANDING HIT") : *Recorded.MissSide.ToUpper(),
        Recorded.PenaltyStrokes > 0 ? TEXT("PENALTY") : TEXT("CLEAN"),
        Recorded.CorridorAdherencePercent, Recorded.RemainingDistanceMeters,
        Recorded.bBasketVisible ? TEXT(" // basket visible") : TEXT(" // basket blocked"));

    if (DiscGolfRouteTelemetry::CountAttempts(RouteTelemetrySession, Route->RouteId)
        >= RouteTelemetrySession.TargetAttemptsPerRoute)
    {
        const FDiscGolfShotRouteDefinition* Next = RouteTelemetrySession.Routes.FindByPredicate(
            [this](const FDiscGolfShotRouteDefinition& Candidate)
            {
                return DiscGolfRouteTelemetry::CountAttempts(RouteTelemetrySession, Candidate.RouteId)
                    < RouteTelemetrySession.TargetAttemptsPerRoute;
            });
        if (Next)
        {
            RouteTelemetrySession.ActiveRouteId = Next->RouteId;
        }
    }
    RefreshRouteTelemetryStatus();
    SaveRouteTelemetry();
    UE_LOG(LogDiscGolfTour, Display, TEXT("ROUTE TELEMETRY ATTEMPT %d: %s"),
        Recorded.AttemptNumber, *RouteTelemetryLastResultText);
}

void ADiscGolfTourGameMode::FinalizeRouteTelemetryHoleScore()
{
    if (!bRouteTelemetryActive
        || !RouteTelemetrySession.Attempts.IsValidIndex(RouteTelemetryAttemptAwaitingScoreIndex)) return;
    RouteTelemetrySession.Attempts[RouteTelemetryAttemptAwaitingScoreIndex].FinalHoleScore = Strokes;
    RouteTelemetryLastResultText += FString::Printf(TEXT(" // final score %d"), Strokes);
    RouteTelemetryAttemptAwaitingScoreIndex = INDEX_NONE;
    SaveRouteTelemetry();
}

void ADiscGolfTourGameMode::SpawnFixturePresentationGallery()
{
    if (!GetWorld()) return;
    auto SpawnVisual = [this](
        const TCHAR* Id,
        EDiscGolfFixtureType Type,
        EDiscGolfPrimitiveShape Shape,
        const FVector& Location,
        const FVector& Scale,
        float Yaw = 0.0f)
    {
        FDiscGolfCollisionFixtureDefinition Definition;
        Definition.FixtureId = FName(Id);
        Definition.FixtureType = Type;
        Definition.Shape = Shape;
        Definition.LocationCm = Location;
        Definition.Scale = Scale;
        Definition.Rotation = FRotator(0.0f, Yaw, 0.0f);
        ADiscGolfFixturePresentationActor* Visual =
            GetWorld()->SpawnActor<ADiscGolfFixturePresentationActor>();
        if (!Visual || !Visual->Configure(Definition, 1.0f, 1.0f))
        {
            if (Visual) Visual->Destroy();
            return false;
        }
        return Visual->IsCollisionInvariant();
    };

    int32 ValidCount = 0;
    ValidCount += SpawnVisual(TEXT("GalleryBoulder"), EDiscGolfFixtureType::Rock,
        EDiscGolfPrimitiveShape::Sphere, FVector(900.0f, -330.0f, 60.0f),
        FVector(1.25f, 1.0f, 0.90f)) ? 1 : 0;
    ValidCount += SpawnVisual(TEXT("GallerySign"), EDiscGolfFixtureType::Sign,
        EDiscGolfPrimitiveShape::Box, FVector(900.0f, 330.0f, 135.0f),
        FVector(0.12f, 2.2f, 1.4f), -8.0f) ? 1 : 0;
    ValidCount += SpawnVisual(TEXT("GalleryBrush"), EDiscGolfFixtureType::DenseGrass,
        EDiscGolfPrimitiveShape::Box, FVector(1550.0f, 0.0f, 100.0f),
        FVector(5.0f, 2.8f, 2.0f), 4.0f) ? 1 : 0;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("FIXTURE PRESENTATION GALLERY: %d/3 collision-free visual families ready."), ValidCount);
}

void ADiscGolfTourGameMode::CaptureVisualQAScreenshot()
{
    if (VisualQAScreenshotPath.IsEmpty()) return;
    FScreenshotRequest::RequestScreenshot(VisualQAScreenshotPath, false, false);
    UE_LOG(LogDiscGolfTour, Display, TEXT("Visual QA screenshot requested: %s"), *VisualQAScreenshotPath);
}

void ADiscGolfTourGameMode::PositionGalleryLakeWaterVisualQACamera()
{
    const FDiscGolfHoleBlockoutDefinition* Definition = GetActiveHoleDefinition();
    const ADiscGolfWaterPresentationActor* Water = DevBootstrap
        ? DevBootstrap->GetWaterPresentation() : nullptr;
    if (!Definition || Definition->HoleNumber != 3 || !Water || !Water->IsReady()) return;

    const FDiscGolfCameraAnchorDefinition* FairwayAnchor = Definition->CameraAnchors.FindByPredicate(
        [](const FDiscGolfCameraAnchorDefinition& Anchor)
        {
            return Anchor.Mode == EDiscGolfCameraAnchorMode::Fairway;
        });
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!FairwayAnchor || !PlayerController || !GetWorld()) return;

    const FVector Target = Water->GetActorLocation();
    // Material QA needs the entire ellipse in frame; the authored fairway anchor remains untouched
    // and supplies the course-facing FOV, while this derived position clears the blockout shelves.
    const FVector CameraLocation = Target + FVector(-7600.0f, -5600.0f, 4300.0f);
    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
        CameraLocation, (Target - CameraLocation).Rotation());
    if (!Camera) return;
    Camera->GetCameraComponent()->SetFieldOfView(FMath::Max(50.0f, FairwayAnchor->FieldOfViewDeg));
    PlayerController->bAutoManageActiveCameraTarget = false;
    PlayerController->SetViewTarget(Camera);
    if (AHUD* Hud = PlayerController->GetHUD()) Hud->bShowHUD = false;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Gallery Lake visual QA camera derived from authored fairway anchor %s."),
        *FairwayAnchor->AnchorId.ToString());
}

void ADiscGolfTourGameMode::PositionGroundCoverVisualQACamera()
{
    const FDiscGolfHoleBlockoutDefinition* Definition = GetActiveHoleDefinition();
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!Definition || !PlayerController || !GetWorld()) return;

    FVector Forward = Definition->BasketLocationCm - Definition->TeeLocationCm;
    Forward.Z = 0.0f;
    Forward = Forward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    const FVector Side(-Forward.Y, Forward.X, 0.0f);
    FVector Target = Definition->TeeLocationCm + Forward * 3000.0f + Side * 1800.0f;
    Target.Z = FMath::Lerp(Definition->TeeLocationCm.Z,
        Definition->BasketLocationCm.Z, 0.24f) + 28.0f;
    // Keep the lens on the maintained corridor and aim it toward the forest edge.
    // The previous offset placed the proof camera inside a fir crown, which hid the
    // very ground-cover layers this deterministic view is intended to validate.
    const FVector CameraLocation = Target - Forward * 600.0f - Side * 1550.0f
        + FVector(0.0f, 0.0f, 450.0f);
    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
        CameraLocation, (Target - CameraLocation).Rotation());
    if (!Camera) return;
    Camera->GetCameraComponent()->SetFieldOfView(52.0f);
    PlayerController->bAutoManageActiveCameraTarget = false;
    PlayerController->SetViewTarget(Camera);
    if (AHUD* Hud = PlayerController->GetHUD()) Hud->bShowHUD = false;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Ground-cover visual QA camera positioned beside Hole %d route edge."),
        Definition->HoleNumber);
}

void ADiscGolfTourGameMode::PositionShorelineDressingVisualQACamera()
{
    const FDiscGolfHoleBlockoutDefinition* Definition = GetActiveHoleDefinition();
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!Definition || Definition->HoleNumber != 3 || !PlayerController || !GetWorld()) return;
    const FDiscGolfBlockoutSurfaceDefinition* Lake = Definition->Surfaces.FindByPredicate(
        [](const FDiscGolfBlockoutSurfaceDefinition& Surface)
        {
            return Surface.SurfaceId.ToString().Contains(TEXT("LakeWater"));
        });
    if (!Lake) return;

    const float RadiusX = 50.0f * FMath::Abs(Lake->Scale.X);
    const float RadiusY = 50.0f * FMath::Abs(Lake->Scale.Y);
    const FVector LocalRadial(-0.94f, -0.34f, 0.0f);
    const FVector Radial = Lake->Rotation.RotateVector(LocalRadial).GetSafeNormal();
    const FVector Tangent(-Radial.Y, Radial.X, 0.0f);
    FVector Target = Lake->LocationCm + Lake->Rotation.RotateVector(
        FVector(LocalRadial.X * RadiusX * 1.14f, LocalRadial.Y * RadiusY * 1.14f, 0.0f));
    Target.Z = Lake->LocationCm.Z + 62.0f;
    const FVector CameraLocation = Target + Radial * 1050.0f - Tangent * 480.0f
        + FVector(0.0f, 0.0f, 430.0f);
    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
        CameraLocation, (Target - CameraLocation).Rotation());
    if (!Camera) return;
    Camera->GetCameraComponent()->SetFieldOfView(56.0f);
    PlayerController->bAutoManageActiveCameraTarget = false;
    PlayerController->SetViewTarget(Camera);
    if (AHUD* Hud = PlayerController->GetHUD()) Hud->bShowHUD = false;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Gallery Lake shoreline-dressing visual QA camera positioned at protected outer bank."));
}

void ADiscGolfTourGameMode::PositionTrailWearVisualQACamera()
{
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (!PlayerController || !GetWorld() || ActiveHoleDefinitions.Num() < 2) return;
    const FVector Start = ActiveHoleDefinitions[0].BasketLocationCm;
    const FVector End = ActiveHoleDefinitions[1].TeeLocationCm;
    FVector Direction = End - Start;
    Direction.Z = 0.0f;
    Direction = Direction.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
    const FVector Side(-Direction.Y, Direction.X, 0.0f);
    const FVector Bend = Side * 900.0f;
    FVector Target = FMath::Lerp(Start, End, 0.34f) + Bend;
    Target.Z += 24.0f;
    const FVector CameraLocation = Target - Direction * 1250.0f - Side * 900.0f
        + FVector(0.0f, 0.0f, 760.0f);
    ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(
        CameraLocation, (Target - CameraLocation).Rotation());
    if (!Camera) return;
    Camera->GetCameraComponent()->SetFieldOfView(58.0f);
    PlayerController->bAutoManageActiveCameraTarget = false;
    PlayerController->SetViewTarget(Camera);
    if (AHUD* Hud = PlayerController->GetHUD()) Hud->bShowHUD = false;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Connector trail-wear visual QA camera positioned between Holes 1 and 2."));
}

void ADiscGolfTourGameMode::FinishVisualQAScreenshot()
{
    const int64 ScreenshotSize = IFileManager::Get().FileSize(*VisualQAScreenshotPath);
    if (ScreenshotSize > 0)
    {
        UE_LOG(LogDiscGolfTour, Display, TEXT("VISUAL QA SCREENSHOT PASS: %s (%lld bytes)."),
            *VisualQAScreenshotPath, ScreenshotSize);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error, TEXT("VISUAL QA SCREENSHOT FAIL: %s"), *VisualQAScreenshotPath);
    }
    FPlatformMisc::RequestExit(false);
}

void ADiscGolfTourGameMode::EnsurePlayerPawn()
{
    if (!GetWorld()) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || PC->GetPawn()) return;

    const FVector SpawnLocation = ActiveHole ? ActiveHole->TeeLocation + FVector(0, 0, 88.0f) : FVector(0, 0, 88.0f);
    const FVector FaceTarget = ActiveHole ? ActiveHole->BasketLocation : SpawnLocation + FVector(1000, 0, 0);
    const FRotator SpawnRotation(0.0f, (FaceTarget - SpawnLocation).Rotation().Yaw, 0.0f);

    if (ADiscGolferPawn* Golfer = GetWorld()->SpawnActor<ADiscGolferPawn>(SpawnLocation, SpawnRotation))
    {
        PC->Possess(Golfer);
    }
}

void ADiscGolfTourGameMode::ShowMainMenu()
{
    if (!GetWorld() || !ActiveHole)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Native main menu could not open before course bootstrap completed."));
        return;
    }

    bMainMenuVisible = true;
    bScorecardVisible = false;
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        Golfer->CancelAnimatedThrow();
        Golfer->CancelThrowPresentation();
    }
    if (ADiscGolfTourPlayerController* PlayerController =
            Cast<ADiscGolfTourPlayerController>(
                UGameplayStatics::GetPlayerController(this, 0)))
    {
        PlayerController->ApplyMainMenuInputMode(true);
    }
}

bool ADiscGolfTourGameMode::StartOrContinueFromMainMenu()
{
    if (!bMainMenuVisible || ActiveDisc || bRegressionActive || ReplayActor
        || IsCourseFlyoverActive() || bLieTransitionActive)
    {
        return false;
    }

    bScorecardVisible = false;
    if (!HasAuthoredRound())
    {
        if (!LoadCourse(TEXT("PineRidge")))
        {
            ShowMainMenu();
            return false;
        }
    }
    else if (bHoleComplete)
    {
        if (RoundState.bRoundComplete)
        {
            RestartRound();
        }
        else
        {
            AdvanceToNextHole();
        }
    }

    EnsurePlayerPawn();
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    FDGDiscInstance SelectedDisc;
    if (!HasAuthoredRound() || !ActiveHole || !Golfer || !Bag
        || !Bag->GetSelectedDiscInstance(SelectedDisc))
    {
        ShowMainMenu();
        return false;
    }

    FinishHoleIntroduction();
    bMainMenuVisible = false;
    ReturnCameraToPlayer();
    if (ADiscGolfTourPlayerController* PlayerController =
            Cast<ADiscGolfTourPlayerController>(
                UGameplayStatics::GetPlayerController(this, 0)))
    {
        PlayerController->ApplyMainMenuInputMode(false);
    }

    if (!CanPlayerThrow())
    {
        ShowMainMenu();
        return false;
    }
    return true;
}

bool ADiscGolfTourGameMode::ReturnToMainMenu()
{
    if (bMainMenuVisible || ActiveDisc || bRegressionActive || ReplayActor
        || IsCourseFlyoverActive() || bHoleIntroActive || bLieTransitionActive)
    {
        return false;
    }

    bScorecardVisible = false;
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        Golfer->CancelThrowPresentation();
    }
    ReturnCameraToPlayer();
    ShowMainMenu();
    return bMainMenuVisible;
}

void ADiscGolfTourGameMode::RunSession16MainMenuSmokeTest()
{
    const auto Fail = [](const FString& Reason)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION16_MAIN_MENU: FAIL reason=%s"), *Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };

    if (!bMainMenuVisible)
    {
        Fail(TEXT("native front end was not visible after bootstrap"));
        return;
    }
    if (CanPlayerThrow())
    {
        Fail(TEXT("throw gate remained open while native front end was visible"));
        return;
    }
    ADiscGolfTourPlayerController* PlayerController =
        Cast<ADiscGolfTourPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0));
    if (!PlayerController || !PlayerController->bShowMouseCursor
        || PlayerController->GetActiveInputRoute() != EDiscGolfInputRoute::UI)
    {
        Fail(TEXT("native front end did not own local UI input before Continue"));
        return;
    }
    if (!StartOrContinueFromMainMenu())
    {
        Fail(TEXT("native Start or Continue action did not enter gameplay"));
        return;
    }

    int32 GolferCount = 0;
    ADiscGolferPawn* Golfer = nullptr;
    for (TActorIterator<ADiscGolferPawn> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            ++GolferCount;
            Golfer = *It;
        }
    }
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    FDGDiscInstance SelectedDisc;
    const bool bCourseReady = ActiveCourseManifest.CourseId == TEXT("PineRidgeChampionship")
        && HasAuthoredRound() && ActiveHole != nullptr;
    const bool bTeeReady = bCourseReady && CurrentLieType == ELieType::Tee
        && CurrentLieState.LieType == ELieType::Tee
        && CurrentLieState.LieLocationCm.Equals(ActiveHole->TeeLocation, 1.0f);
    const bool bBagReady = Bag && Bag->GetSelectedDiscInstance(SelectedDisc)
        && !Bag->GetSelectedMoldId().IsNone();
    const bool bThrowReady = !bMainMenuVisible && CanPlayerThrow();
    const bool bGameplayInputReady = !PlayerController->bShowMouseCursor
        && PlayerController->GetActiveInputRoute() == EDiscGolfInputRoute::Gameplay;
    if (GolferCount != 1 || !bCourseReady || !bTeeReady || !bBagReady
        || !bThrowReady || !bGameplayInputReady)
    {
        Fail(FString::Printf(
            TEXT("player=%d course=%s tee=%s bag=%s throw_ready=%s input=%s"),
            GolferCount,
            bCourseReady ? TEXT("ready") : TEXT("invalid"),
            bTeeReady ? TEXT("ready") : TEXT("invalid"),
            bBagReady ? TEXT("ready") : TEXT("invalid"),
            bThrowReady ? TEXT("yes") : TEXT("no"),
            bGameplayInputReady ? TEXT("gameplay") : TEXT("invalid")));
        return;
    }

    UE_LOG(LogDiscGolfTour, Display, TEXT("DG_SESSION16_MAIN_MENU: PASS"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ADiscGolfTourGameMode::RunSession17RoundFlowSmokeTest()
{
    const auto Fail = [](const FString& Reason)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION17_ROUND_FLOW_FRONT_END: FAIL reason=%s"), *Reason);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    };

    ADiscGolfTourPlayerController* PlayerController =
        Cast<ADiscGolfTourPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0));
    if (!PlayerController || !bMainMenuVisible || CanPlayerThrow())
    {
        Fail(TEXT("front-end authority was unavailable or its throw gate was open"));
        return;
    }

    PlayerController->RefreshRoundFlowPresentation();
    const bool bFrontEndInteractive = PlayerController->HasInteractiveRoundFlowWidget()
        && PlayerController->bShowMouseCursor
        && PlayerController->GetActiveInputRoute() == EDiscGolfInputRoute::UI;
    if (!bFrontEndInteractive)
    {
        Fail(TEXT("front end did not expose a focusable native widget with UI input ownership"));
        return;
    }

    FString SettingsRecoveryError;
    if (!PlayerController->RunRoundFlowSettingsRecoveryProbe(SettingsRecoveryError))
    {
        Fail(FString::Printf(TEXT("settings-origin recovery failed: %s"),
            *SettingsRecoveryError));
        return;
    }

    if (!PlayerController->HandleRoundFlowAction(
            EDGRoundFlowAction::StartOrContinue))
    {
        Fail(TEXT("validated Start or Continue action did not enter gameplay"));
        return;
    }

    int32 GolferCount = 0;
    ADiscGolferPawn* Golfer = nullptr;
    for (TActorIterator<ADiscGolferPawn> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            ++GolferCount;
            Golfer = *It;
        }
    }
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    FDGDiscInstance SelectedDisc;
    const bool bCourseReady = ActiveCourseManifest.CourseId
            == TEXT("PineRidgeChampionship")
        && HasAuthoredRound() && ActiveHole != nullptr;
    const bool bTeeReady = bCourseReady && CurrentLieType == ELieType::Tee
        && CurrentLieState.LieType == ELieType::Tee
        && CurrentLieState.LieLocationCm.Equals(ActiveHole->TeeLocation, 1.0f);
    const bool bBagReady = Bag && Bag->GetSelectedDiscInstance(SelectedDisc)
        && !Bag->GetSelectedMoldId().IsNone();
    const bool bGameplayRecovered = !bMainMenuVisible && CanPlayerThrow()
        && !PlayerController->HasInteractiveRoundFlowWidget()
        && !PlayerController->bShowMouseCursor
        && PlayerController->GetActiveInputRoute() == EDiscGolfInputRoute::Gameplay;
    const bool bDuplicateActionRejected = !PlayerController->HandleRoundFlowAction(
        EDGRoundFlowAction::StartOrContinue);

    int32 GolferCountAfterDuplicate = 0;
    for (TActorIterator<ADiscGolferPawn> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            ++GolferCountAfterDuplicate;
        }
    }
    if (GolferCount != 1 || GolferCountAfterDuplicate != 1 || !bCourseReady
        || !bTeeReady || !bBagReady || !bGameplayRecovered
        || !bDuplicateActionRejected)
    {
        Fail(FString::Printf(
            TEXT("player=%d duplicate_player=%d course=%s tee=%s bag=%s gameplay=%s duplicate_action=%s"),
            GolferCount, GolferCountAfterDuplicate,
            bCourseReady ? TEXT("ready") : TEXT("invalid"),
            bTeeReady ? TEXT("ready") : TEXT("invalid"),
            bBagReady ? TEXT("ready") : TEXT("invalid"),
            bGameplayRecovered ? TEXT("recovered") : TEXT("invalid"),
            bDuplicateActionRejected ? TEXT("rejected") : TEXT("accepted")));
        return;
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION17_ROUND_FLOW_FRONT_END: PASS widget=interactive focus=valid settings=restored continue=gameplay duplicate=rejected"));
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ADiscGolfTourGameMode::StartSession16RulesSmokeTest()
{
    const bool bOutOfBounds = Session16RulesSmokeScenario.Equals(
        TEXT("OB"), ESearchCase::IgnoreCase);
    const bool bWater = Session16RulesSmokeScenario.Equals(
        TEXT("Water"), ESearchCase::IgnoreCase);
    if (!bOutOfBounds && !bWater)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION16_RULES: FAIL reason=scenario must be OB or Water"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }

    bSession16RulesSmokeTestActive = true;
    RunCourseRulesFixture(bOutOfBounds ? TEXT("OB") : TEXT("Water"));
    if (!ActiveDisc)
    {
        bSession16RulesSmokeTestActive = false;
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION16_RULES_%s: FAIL reason=authoritative fixture throw did not launch"),
            bOutOfBounds ? TEXT("OB") : TEXT("WATER"));
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
}

void ADiscGolfTourGameMode::FinishSession16RulesSmokeTest()
{
    if (!bSession16RulesSmokeTestActive)
    {
        return;
    }
    bSession16RulesSmokeTestActive = false;

    const bool bOutOfBounds = Session16RulesSmokeScenario.Equals(
        TEXT("OB"), ESearchCase::IgnoreCase);
    const EDiscGolfPenaltyType ExpectedPenalty = bOutOfBounds
        ? EDiscGolfPenaltyType::OutOfBounds
        : EDiscGolfPenaltyType::Hazard;
    const bool bExpectedSurfaceAndRelief = bOutOfBounds
        ? CurrentLieState.SurfaceAtRest == ECourseSurfaceType::OutOfBounds
            && CurrentLieState.PlayingSurface == ECourseSurfaceType::Fairway
            && CurrentLieState.ReliefRule == EDiscGolfReliefRule::LastInBounds
            && !CurrentLieState.RawDiscLocationCm.Equals(
                CurrentLieState.LieLocationCm, 1.0f)
        : ActiveCourseManifest.CourseId == TEXT("PineRidgeChampionship")
            && ActiveHole && ActiveHole->HoleNumber == 3
            && CurrentLieState.SurfaceAtRest == ECourseSurfaceType::Hazard
            && CurrentLieState.PlayingSurface == ECourseSurfaceType::Hazard
            && CurrentLieState.ReliefRule == EDiscGolfReliefRule::PlayFromResult
            && CurrentLieState.RawDiscLocationCm.Equals(
                CurrentLieState.LieLocationCm, 1.0f)
            && ResolvePresentationSurfaceMaterialAtLocation(
                CurrentLieState.RawDiscLocationCm) == TEXT("Water");
    const bool bPassed = CurrentLieState.PenaltyType == ExpectedPenalty
        && bExpectedSurfaceAndRelief
        && CurrentLieState.PenaltyStrokes == 1
        && PenaltyStrokes == 1
        && Strokes == 2
        && !CurrentLieState.LieLocationCm.ContainsNaN()
        && FMath::IsFinite(CurrentLieState.DistanceToBasketMeters)
        && ActiveDisc == nullptr
        && CanPlayerThrow();
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION16_RULES_%s: PASS penalty=%s surface=%s relief=%d strokes=%d penalty_strokes=%d next_throw=1"),
            bOutOfBounds ? TEXT("OB") : TEXT("WATER"),
            *DiscGolfCourseRules::PenaltyName(CurrentLieState.PenaltyType),
            *DiscGolfCourseRules::SurfaceName(CurrentLieState.SurfaceAtRest),
            static_cast<int32>(CurrentLieState.ReliefRule), Strokes, PenaltyStrokes);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION16_RULES_%s: FAIL penalty=%s surface=%s relief=%d provenance=%s lie_penalty=%d strokes=%d penalty_strokes=%d next_throw=%d"),
            bOutOfBounds ? TEXT("OB") : TEXT("WATER"),
            *DiscGolfCourseRules::PenaltyName(CurrentLieState.PenaltyType),
            *DiscGolfCourseRules::SurfaceName(CurrentLieState.SurfaceAtRest),
            static_cast<int32>(CurrentLieState.ReliefRule),
            *ResolvePresentationSurfaceMaterialAtLocation(
                CurrentLieState.RawDiscLocationCm).ToString(),
            CurrentLieState.PenaltyStrokes, Strokes, PenaltyStrokes,
            CanPlayerThrow() ? 1 : 0);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

bool ADiscGolfTourGameMode::CanPlayerThrow() const
{
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    const ADiscGolfTourPlayerController* PlayerController =
        Cast<ADiscGolfTourPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0));
    const bool bBaseLaunchAllowed = DiscGolfGameplayGate::CanLaunchThrow(
        ActiveHole != nullptr, ActiveDisc != nullptr, ReplayActor != nullptr,
        IsCourseFlyoverActive(), bHoleComplete, bScorecardVisible, bHoleIntroActive);
    return DiscGolfGameplayGate::CanCommitPlayerRelease(
            bBaseLaunchAllowed,
            bMainMenuVisible,
            bRegressionActive,
            bLieTransitionActive,
            PlayerController != nullptr,
            PlayerController && PlayerController->IsPresentationDismissReleasePending(),
            PlayerController && PlayerController->IsFreshThrowDownRequiredAfterPresentation(),
            PlayerController && PlayerController->IsControlsMenuOpen(),
            PlayerController && PlayerController->IsCharacterCreatorOpen(),
            GetWorld() && GetWorld()->IsPaused())
        && (!Golfer || !Golfer->IsAnimatedThrowActive());
}

bool ADiscGolfTourGameMode::IsPlayerThrowProvenanceValid(
    const FThrowCommand& Command,
    EDGHandedness ActiveProfileHandedness,
    EDiscShotContext AuthoritativeShotContext,
    const FDGDiscInstance& SelectedInstance)
{
    FName ExpectedPlasticId = NAME_None;
    switch (Command.Plastic)
    {
        case EDiscPlastic::Base: ExpectedPlasticId = TEXT("Base"); break;
        case EDiscPlastic::Tour: ExpectedPlasticId = TEXT("Tour"); break;
        case EDiscPlastic::Crystal: ExpectedPlasticId = TEXT("Crystal"); break;
        default: return false;
    }
    return Command.DiscInstanceId.IsValid()
        && SelectedInstance.InstanceId == Command.DiscInstanceId
        && SelectedInstance.DiscDefinitionId == Command.MoldId
        && SelectedInstance.PlasticId == ExpectedPlasticId
        && Command.Handedness == ActiveProfileHandedness
        && Command.ShotContext == AuthoritativeShotContext;
}

bool ADiscGolfTourGameMode::TryPrepareReleaseForOriginOverride(
    const FVector* ReleaseLocationOverrideCm,
    const FVector& AimLineOriginCm,
    float AimReferenceDistanceCm,
    FThrowRelease& InOutRelease,
    FString& OutError)
{
    if (!ReleaseLocationOverrideCm)
    {
        OutError.Reset();
        return true;
    }

    const auto Reject = [&OutError](const TCHAR* Error)
    {
        OutError = Error;
        return false;
    };
    const auto IsFiniteVector = [](const FVector& Value)
    {
        return FMath::IsFinite(Value.X)
            && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z);
    };
    if (!IsFiniteVector(AimLineOriginCm)
        || !IsFiniteVector(*ReleaseLocationOverrideCm))
    {
        return Reject(TEXT("Animated release origins must be finite"));
    }

    constexpr double MaximumOriginDistanceCm =
        static_cast<double>(MaximumAnimatedGripOriginDistanceCm);
    const double OriginDistanceSquared = FVector::DistSquared(
        AimLineOriginCm, *ReleaseLocationOverrideCm);
    if (!FMath::IsFinite(OriginDistanceSquared)
        || OriginDistanceSquared
            > MaximumOriginDistanceCm * MaximumOriginDistanceCm)
    {
        return Reject(TEXT("Animated grip origin is outside the authoritative lie bound"));
    }
    if (!FMath::IsFinite(AimReferenceDistanceCm)
        || AimReferenceDistanceCm <= SMALL_NUMBER)
    {
        return Reject(TEXT("Animated aim range must be finite and positive"));
    }

    FVector AimPreservingDirection;
    if (!DiscGolfMath::TryRebaseReleaseDirectionToPreserveAimPoint(
        AimLineOriginCm,
        *ReleaseLocationOverrideCm,
        AimReferenceDistanceCm,
        InOutRelease.Direction,
        InOutRelease.AimOffsetDeg,
        AimPreservingDirection,
        OutError))
    {
        return false;
    }

    InOutRelease.Direction = AimPreservingDirection;
    OutError.Reset();
    return true;
}

bool ADiscGolfTourGameMode::CanOpenCharacterCreator() const
{
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    return !bMainMenuVisible
        && ActiveDisc == nullptr
        && ReplayActor == nullptr
        && !IsCourseFlyoverActive()
        && !bRegressionActive
        && !bLieTransitionActive
        && !bHoleIntroActive
        && !bScorecardVisible
        && (!Golfer || !Golfer->IsAnimatedThrowActive());
}

bool ADiscGolfTourGameMode::LoadCourse(const FString& CourseName)
{
    if (!DevBootstrap || !GetWorld())
    {
        CourseStatusText = TEXT("COURSE LOAD FAILED - BOOTSTRAP UNAVAILABLE");
        return false;
    }
    if (ActiveDisc || bRegressionActive)
    {
        CourseStatusText = TEXT("COURSE CHANGE BLOCKED - WAIT FOR CURRENT THROW");
        return false;
    }
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        Golfer && Golfer->IsAnimatedThrowActive())
    {
        CourseStatusText = TEXT("COURSE CHANGE BLOCKED - FINISH OR CANCEL CHARACTER THROW");
        return false;
    }

    FName RequestedId = NAME_None;
    if (CourseName.Equals(TEXT("PineRidge"), ESearchCase::IgnoreCase)
        || CourseName.Equals(TEXT("PineRidgeChampionship"), ESearchCase::IgnoreCase)
        || CourseName.Equals(TEXT("Pine Ridge"), ESearchCase::IgnoreCase))
    {
        RequestedId = TEXT("PineRidge");
    }
    else if (CourseName.Equals(TEXT("Regression"), ESearchCase::IgnoreCase)
        || CourseName.Equals(TEXT("RegressionCourse"), ESearchCase::IgnoreCase)
        || CourseName.Equals(TEXT("Practice"), ESearchCase::IgnoreCase))
    {
        RequestedId = TEXT("Regression");
    }
    else
    {
        CourseStatusText = FString::Printf(TEXT("COURSE LOAD FAILED - UNKNOWN '%s'"), *CourseName);
        return false;
    }

    StopInstantReplay(false);
    StopBroadcastCamera(false);
    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap->GetFlyoverRoute())
    {
        Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
        Route->StopPreview();
    }
    ActiveFlyoverRoute = nullptr;
    if (FlyoverCameraViewToken.IsValid())
    {
        FString ReleaseError;
        DiscGolfCameraViewContract::TryRelease(CameraViewState, FlyoverCameraViewToken, ReleaseError);
        FlyoverCameraViewToken = FDiscGolfCameraViewToken();
    }
    ReturnCameraToPlayer();

    ActiveCourseManifest = FDiscGolfCourseManifestDefinition();
    ActiveHoleDefinitions.Reset();
    ActiveCoursePresentation = FDiscGolfCoursePresentationDefinition();
    CoursePresentationStatusText = TEXT("ART CONTRACT N/A");
    RoundState = FDiscGolfRoundState();
    bScorecardVisible = false;

    FString Error;
    ADiscGolfHoleActor* NewHole = nullptr;
    if (RequestedId == TEXT("PineRidge"))
    {
        if (!InitializePineRidgeRound(Error))
        {
            CourseStatusText = FString::Printf(TEXT("COURSE LOAD FAILED - %s"), *Error);
            return false;
        }
        FString PresentationSource;
        FString PresentationError;
        if (DiscGolfCoursePresentation::LoadPineRidge(
            ActiveCoursePresentation, PresentationSource, PresentationError))
        {
            CoursePresentationStatusText = FString::Printf(TEXT("ART %s // %s"),
                *PresentationSource,
                ActiveCoursePresentation.bAssetsReady ? TEXT("ASSETS READY") : TEXT("ASSETS PENDING"));
            if (!PresentationError.IsEmpty())
            {
                UE_LOG(LogDiscGolfTour, Warning, TEXT("Course presentation fallback: %s"), *PresentationError);
            }
        }
        else
        {
            CoursePresentationStatusText = TEXT("ART CONTRACT INVALID // BLOCKOUT ACTIVE");
            UE_LOG(LogDiscGolfTour, Warning, TEXT("Course presentation contract invalid: %s"), *PresentationError);
        }
        const FDiscGolfResolvedQualityProfile QualityProfile =
            DiscGolfQualityAdapter::ResolveCurrent(Scalability::GetQualityLevels());
        NewHole = DevBootstrap->BuildPersistentPineRidgeCourse(
            ActiveHoleDefinitions,
            QualityProfile,
            Error);
    }
    else
    {
        NewHole = DevBootstrap->BuildCourse(RequestedId, Error);
    }
    if (!NewHole)
    {
        CourseStatusText = FString::Printf(TEXT("COURSE LOAD FAILED - %s"), *Error);
        return false;
    }

    ActiveHole = NewHole;
    if (WindDirector)
    {
        WindDirector->RefreshCourseZones();
    }
    EnsurePlayerPawn();
    BeginRuntimeCheckpointRound();
    StartHole();
    UpdateCourseStatus();
    UE_LOG(LogDiscGolfTour, Display, TEXT("Course loaded: %s"), *CourseStatusText);
    return true;
}

bool ADiscGolfTourGameMode::InitializePineRidgeRound(FString& OutError)
{
    FString ManifestSource;
    if (!DiscGolfCourseDefinition::LoadPineRidgeCourseManifest(
        ActiveCourseManifest, ManifestSource, OutError))
    {
        return false;
    }
    ActiveHoleDefinitions.Reset(ActiveCourseManifest.Holes.Num());
    for (const FDiscGolfCourseManifestHoleEntry& Entry : ActiveCourseManifest.Holes)
    {
        FDiscGolfHoleBlockoutDefinition LocalDefinition;
        FString Source;
        if (!DiscGolfCourseDefinition::LoadPineRidgeHole(
            Entry.HoleNumber, LocalDefinition, Source, OutError))
        {
            return false;
        }
        FDiscGolfHoleBlockoutDefinition WorldDefinition;
        if (!DiscGolfCourseDefinition::PlaceHoleInCourse(
            LocalDefinition, Entry, WorldDefinition, OutError))
        {
            return false;
        }
        ActiveHoleDefinitions.Add(MoveTemp(WorldDefinition));
    }
    if (!DiscGolfRound::Initialize(RoundState, ActiveCourseManifest.CourseId,
        ActiveCourseManifest.LayoutId, ActiveCourseManifest.DisplayName, ActiveHoleDefinitions, OutError))
    {
        return false;
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("Course manifest loaded: %s | %s | %d holes."),
        *ActiveCourseManifest.DisplayName.ToString(), *ManifestSource, ActiveCourseManifest.Holes.Num());
    return true;
}

bool ADiscGolfTourGameMode::LoadRoundHoleByIndex(int32 HoleIndex, FString& OutError)
{
    if (!ActiveHoleDefinitions.IsValidIndex(HoleIndex) || !DevBootstrap)
    {
        OutError = TEXT("round hole index is invalid");
        return false;
    }
    if (ActiveDisc || bRegressionActive)
    {
        OutError = TEXT("an active disc or regression owns the round state");
        return false;
    }
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        Golfer && Golfer->IsAnimatedThrowActive())
    {
        OutError = TEXT("finish or cancel the active character throw before changing holes");
        return false;
    }
    const int32 PreviousHoleNumber = ActiveHole ? ActiveHole->HoleNumber : 0;
    StopInstantReplay(false);
    StopBroadcastCamera(false);
    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap->GetFlyoverRoute())
    {
        Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
        Route->StopPreview();
    }
    ActiveFlyoverRoute = nullptr;
    if (FlyoverCameraViewToken.IsValid())
    {
        FString ReleaseError;
        DiscGolfCameraViewContract::TryRelease(CameraViewState, FlyoverCameraViewToken, ReleaseError);
        FlyoverCameraViewToken = FDiscGolfCameraViewToken();
    }
    ReturnCameraToPlayer();
    if (!DevBootstrap->ActivatePineRidgeHole(
        ActiveHoleDefinitions[HoleIndex].HoleNumber, OutError)) return false;
    ADiscGolfHoleActor* NewHole = DevBootstrap->GetHole();
    if (!NewHole) { OutError = TEXT("persistent hole activation returned no hole actor"); return false; }
    RoundState.CurrentHoleIndex = HoleIndex;
    ActiveHole = NewHole;
    if (WindDirector) WindDirector->RefreshCourseZones();
    EnsurePlayerPawn();
    if (PreviousHoleNumber > 0 && PreviousHoleNumber != ActiveHole->HoleNumber)
    {
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveHoleTransition(
            PreviousHoleNumber, ActiveHole->HoleNumber, ActiveHoleDefinitions.Num()));
    }
    StartHole();
    UpdateCourseStatus();
    UE_LOG(LogDiscGolfTour, Display, TEXT("Seamless round transition activated hole %d/%d: %s."),
        HoleIndex + 1, ActiveHoleDefinitions.Num(), *ActiveHole->HoleName.ToString());
    OutError.Reset();
    return true;
}

bool ADiscGolfTourGameMode::LoadRoundHole(int32 HoleNumber)
{
    if (ActiveDisc || bRegressionActive || ReplayActor || IsCourseFlyoverActive()
        || bLieTransitionActive)
    {
        CourseStatusText = TEXT("HOLE LOAD BLOCKED - FINISH ACTIVE PLAYBACK OR SHOT");
        return false;
    }
    if (ActiveHoleDefinitions.IsEmpty())
    {
        CourseStatusText = TEXT("HOLE SELECTION REQUIRES THE PINE RIDGE ROUND");
        return false;
    }
    const int32 HoleIndex = ActiveHoleDefinitions.IndexOfByPredicate(
        [HoleNumber](const FDiscGolfHoleBlockoutDefinition& Definition)
        {
            return Definition.HoleNumber == HoleNumber;
        });
    FString Error;
    if (HoleIndex == INDEX_NONE || !LoadRoundHoleByIndex(HoleIndex, Error))
    {
        CourseStatusText = FString::Printf(TEXT("HOLE LOAD FAILED - %s"),
            HoleIndex == INDEX_NONE ? TEXT("NOT IN MANIFEST") : *Error);
        return false;
    }
    bScorecardVisible = false;
    return true;
}

void ADiscGolfTourGameMode::UpdateCourseStatus()
{
    if (!ActiveHole) return;
    const int32 FlyoverPointCount = ActiveHole->FlyoverRoute
        ? ActiveHole->FlyoverRoute->GetPointCount() : 0;
    const FString RoundText = HasAuthoredRound()
        ? FString::Printf(TEXT(" | ROUND H%d/%d %s"), ActiveHole->HoleNumber,
            RoundState.HoleScores.Num(), *DiscGolfRound::ScoreLabel(DiscGolfRound::ScoreToPar(RoundState)))
        : FString();
    CourseStatusText = FString::Printf(TEXT("%s | %d spectator | %d flyover points%s | %s"),
        *ActiveHole->GetCourseSummary(), ActiveHole->SpectatorBoundaryCount, FlyoverPointCount,
        *RoundText, *CoursePresentationStatusText);
}

void ADiscGolfTourGameMode::AdvanceToNextHole()
{
    if (!HasAuthoredRound())
    {
        CourseStatusText = TEXT("NEXT HOLE IS AVAILABLE IN THE PINE RIDGE ROUND");
        return;
    }
    if (ActiveDisc || bRegressionActive || ReplayActor || IsCourseFlyoverActive())
    {
        CourseStatusText = TEXT("NEXT HOLE BLOCKED - FINISH ACTIVE PLAYBACK OR SHOT");
        return;
    }
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        Golfer && Golfer->IsAnimatedThrowActive())
    {
        CourseStatusText = TEXT("NEXT HOLE BLOCKED - FINISH OR CANCEL CHARACTER THROW");
        return;
    }
    if (!bHoleComplete)
    {
        CourseStatusText = TEXT("NEXT HOLE BLOCKED - COMPLETE THE CURRENT HOLE");
        return;
    }
    if (RoundState.bRoundComplete)
    {
        RestartRound();
        return;
    }
    FString Error;
    const int32 PreviousHoleIndex = RoundState.CurrentHoleIndex;
    if (!DiscGolfRound::Advance(RoundState, Error)
        || !LoadRoundHoleByIndex(RoundState.CurrentHoleIndex, Error))
    {
        RoundState.CurrentHoleIndex = PreviousHoleIndex;
        CourseStatusText = FString::Printf(TEXT("NEXT HOLE BLOCKED - %s"), *Error);
        return;
    }
    bScorecardVisible = false;
    if (ADiscGolfTourPlayerController* PlayerController =
            Cast<ADiscGolfTourPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        PlayerController->RefreshRoundFlowPresentation();
    }
}

void ADiscGolfTourGameMode::RestartRound()
{
    if (ActiveHoleDefinitions.IsEmpty() || ActiveCourseManifest.CourseId.IsNone()) return;
    if (ActiveDisc || bRegressionActive || ReplayActor || IsCourseFlyoverActive()
        || bLieTransitionActive)
    {
        CourseStatusText = TEXT("ROUND RESTART BLOCKED - FINISH ACTIVE PLAYBACK OR SHOT");
        return;
    }
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        Golfer && Golfer->IsAnimatedThrowActive())
    {
        CourseStatusText = TEXT("ROUND RESTART BLOCKED - FINISH OR CANCEL CHARACTER THROW");
        return;
    }
    if (RuntimeCheckpointJournalWriter && bRuntimeCheckpointRoundStarted)
    {
        AbortRuntimeCheckpointJournal(TEXT("the captured round was restarted"));
    }
    FString Error;
    if (!DiscGolfRound::Initialize(RoundState, ActiveCourseManifest.CourseId,
        ActiveCourseManifest.LayoutId, ActiveCourseManifest.DisplayName, ActiveHoleDefinitions, Error)
        || !LoadRoundHoleByIndex(0, Error))
    {
        CourseStatusText = FString::Printf(TEXT("ROUND RESTART FAILED - %s"), *Error);
        return;
    }
    bScorecardVisible = false;
    if (ADiscGolfTourPlayerController* PlayerController =
            Cast<ADiscGolfTourPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        PlayerController->RefreshRoundFlowPresentation();
    }
}

void ADiscGolfTourGameMode::ToggleScorecard()
{
    if (!HasAuthoredRound())
    {
        CourseStatusText = TEXT("SCORECARD IS AVAILABLE IN THE PINE RIDGE ROUND");
        return;
    }
    if (!bScorecardVisible)
    {
        const ADiscGolfTourPlayerController* TourPC = Cast<ADiscGolfTourPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0));
        if (bMainMenuVisible || ActiveDisc || bRegressionActive || ReplayActor || IsCourseFlyoverActive()
            || bHoleIntroActive || bLieTransitionActive
            || (TourPC && (TourPC->IsCharacterCreatorOpen() || TourPC->IsControlsMenuOpen())))
        {
            CourseStatusText = TEXT("SCORECARD BLOCKED - CLOSE THE ACTIVE PRESENTATION");
            return;
        }
    }
    bScorecardVisible = !bScorecardVisible;
    if (bScorecardVisible)
    {
        if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            Golfer->CancelAnimatedThrow();
            Golfer->CancelThrowPresentation();
        }
        if (RoundState.bRoundComplete)
        {
            RecordRuntimeCheckpointFinalScorecard();
        }
    }
    if (ADiscGolfTourPlayerController* PlayerController =
            Cast<ADiscGolfTourPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
    {
        PlayerController->RefreshRoundFlowPresentation();
    }
}

void ADiscGolfTourGameMode::ToggleDeveloperHud()
{
    bDeveloperHudVisible = !bDeveloperHudVisible;
    UE_LOG(LogDiscGolfTour, Display, TEXT("Developer HUD %s."),
        bDeveloperHudVisible ? TEXT("enabled") : TEXT("hidden"));
}

#if 0 // Retired from the game-mode surface; Throw Lab remains a direct development/test subsystem.
void ADiscGolfTourGameMode::ToggleThrowLab()
{
#if !DG_WITH_THROW_LAB
    bThrowLabActive = false;
    ThrowLabStatusText = TEXT("THROW LAB NOT INCLUDED IN THIS SHIPPING SKU");
#else
    bThrowLabActive = !bThrowLabActive;
    if (bThrowLabActive)
    {
        bDeveloperHudVisible = true;
        const UDiscThrowLabSubsystem* Lab = GetThrowLabSubsystem();
        ThrowLabStatusText = Lab
            ? FString::Printf(TEXT("THROW LAB ARMED | %d RECORDS"), Lab->GetRecordCount())
            : TEXT("THROW LAB UNAVAILABLE");
    }
    else
    {
        ThrowLabStatusText = TEXT("THROW LAB OFF");
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("%s"), *ThrowLabStatusText);
#endif
}

int32 ADiscGolfTourGameMode::GetThrowLabRecordCount() const
{
    const UDiscThrowLabSubsystem* Lab = GetThrowLabSubsystem();
    return Lab ? Lab->GetRecordCount() : 0;
}

FString ADiscGolfTourGameMode::GetThrowLabStatusText() const
{
    if (!bThrowLabActive)
    {
        return ThrowLabStatusText;
    }
    const UDiscThrowLabSubsystem* Lab = GetThrowLabSubsystem();
    return Lab
        ? FString::Printf(TEXT("%s | %s"), *ThrowLabStatusText, *Lab->GetStatusText())
        : TEXT("THROW LAB UNAVAILABLE");
}

FString ADiscGolfTourGameMode::GetThrowLabComparisonText() const
{
    if (!bThrowLabActive)
    {
        return TEXT("Comparison inactive");
    }
    const UDiscThrowLabSubsystem* Lab = GetThrowLabSubsystem();
    return Lab ? Lab->GetComparisonText() : TEXT("Comparison unavailable");
}

void ADiscGolfTourGameMode::SelectThrowLabRecord(int32 Offset)
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    FString Error;
    if (!Lab || !Lab->SelectRecordByOffset(Offset, Error))
    {
        ThrowLabStatusText = Lab ? Error : TEXT("Throw Lab is not active");
    }
}

void ADiscGolfTourGameMode::ToggleThrowLabSelectedPinned()
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    bool bPinned = false;
    FString Error;
    if (!Lab || !Lab->ToggleSelectedPinned(bPinned, Error))
    {
        ThrowLabStatusText = Lab ? Error : TEXT("Throw Lab is not active");
        return;
    }
    ThrowLabStatusText = bPinned ? TEXT("Selected record pinned") : TEXT("Selected record unpinned");
}

void ADiscGolfTourGameMode::DeleteThrowLabSelected()
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    FString Error;
    if (!Lab || !Lab->DeleteSelected(Error))
    {
        ThrowLabStatusText = Lab ? Error : TEXT("Throw Lab is not active");
        return;
    }
    ThrowLabStatusText = TEXT("Selected Throw Lab record deleted");
}

void ADiscGolfTourGameMode::CompareThrowLabLatestPair()
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    FString Error;
    const int32 Count = Lab ? Lab->GetRecordCount() : 0;
    if (!Lab || Count < 2 || !Lab->SetComparisonByIndices(Count - 2, Count - 1, Error))
    {
        ThrowLabStatusText = !Lab ? TEXT("Throw Lab is not active")
            : Count < 2 ? TEXT("Complete two throws before comparing") : Error;
        return;
    }
    ThrowLabStatusText = TEXT("Comparing the latest two completed throws");
}

void ADiscGolfTourGameMode::ReplayThrowLabSelected()
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    const FDiscThrowLabRecord* Record = Lab ? Lab->GetSelectedRecord() : nullptr;
    if (!Record || Record->ReplaySamples.Num() < 2)
    {
        ThrowLabStatusText = TEXT("Selected Throw Lab record has no replayable trajectory");
        return;
    }
    const ADiscGolfTourPlayerController* TourPC = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    if (ActiveDisc || bRegressionActive || ReplayActor || bLieTransitionActive
        || bScorecardVisible || bHoleIntroActive || IsCourseFlyoverActive()
        || BroadcastCameraDirector
        || (TourPC && (TourPC->IsCharacterCreatorOpen() || TourPC->IsControlsMenuOpen())))
    {
        ThrowLabStatusText = TEXT("Throw Lab replay unavailable while another gameplay view is active");
        return;
    }

    LastReplaySamples = Record->ReplaySamples;
    LastReplayDisc = Record->Disc;
    LastReplayRelease = Record->Release;
    ReplayStatusText = FString::Printf(TEXT("Throw Lab replay ready - %s - %d actual samples"),
        *Record->RecordId, LastReplaySamples.Num());
    ToggleInstantReplay();
}

void ADiscGolfTourGameMode::SaveThrowLab()
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    FString Error;
    if (!Lab || !Lab->SaveLab(Error))
    {
        ThrowLabStatusText = Lab ? Error : TEXT("Throw Lab is not active");
        return;
    }
    ThrowLabStatusText = TEXT("Throw Lab records saved to the separate development slot");
}

void ADiscGolfTourGameMode::LoadThrowLab()
{
    UDiscThrowLabSubsystem* Lab = bThrowLabActive ? GetThrowLabSubsystem() : nullptr;
    FString Error;
    if (!Lab || !Lab->LoadLab(Error))
    {
        ThrowLabStatusText = Lab ? Error : TEXT("Throw Lab is not active");
        return;
    }
    ThrowLabStatusText = FString::Printf(TEXT("Loaded %d Throw Lab records"), Lab->GetRecordCount());
}
#endif

void ADiscGolfTourGameMode::ToggleSelectedDiscFavorite()
{
    if (!CanPlayerThrow())
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Disc favorite can only change before a legal throw"));
        return;
    }
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    if (!Bag || !Bag->ToggleSelectedFavorite())
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Selected disc favorite could not be changed"));
        return;
    }
    FString SaveError;
    const FString EquipmentStatus = Bag->SaveEquipment(SaveError)
        ? Bag->GetSelectedEquipmentStatusText()
        : FString::Printf(TEXT("Favorite changed in memory; save failed - %s"), *SaveError);
    UE_LOG(LogDiscGolfTour, Display, TEXT("%s"), *EquipmentStatus);
}

void ADiscGolfTourGameMode::SkipCurrentPresentation()
{
    if (bHoleIntroActive)
    {
        // Space and gamepad South also drive the Enhanced Input throw action.
        // Keep the intro gate closed until the current input evaluation is over,
        // so the dismissal press cannot become the first half of a throw.
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishHoleIntroduction));
        return;
    }

    if (ADiscGolfFlyoverRouteActor* Route = ActiveFlyoverRoute;
        IsValid(Route) && Route->IsPreviewing())
    {
        Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
        Route->StopPreview();
        FString ReleaseError;
        const bool bReleased = DiscGolfCameraViewContract::TryRelease(
            CameraViewState, FlyoverCameraViewToken, ReleaseError);
        FlyoverCameraViewToken = FDiscGolfCameraViewToken();
        ActiveFlyoverRoute = nullptr;
        if (bReleased) ReturnCameraToPlayer();
        CourseStatusText = ActiveHole
            ? FString::Printf(TEXT("%s | FLYOVER SKIPPED"), *ActiveHole->GetCourseSummary())
            : TEXT("FLYOVER SKIPPED");
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveFlyover(false),
            ActiveHole ? ActiveHole->BasketLocation : FVector::ZeroVector);
    }
}

bool ADiscGolfTourGameMode::IsCourseFlyoverActive() const
{
    const ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr;
    return IsValid(Route) && Route->IsPreviewing();
}

void ADiscGolfTourGameMode::ToggleCourse()
{
    const bool bOnPineRidge = ActiveHole && ActiveHole->CourseId == TEXT("PineRidgeChampionship");
    LoadCourse(bOnPineRidge ? TEXT("Regression") : TEXT("PineRidge"));
}

void ADiscGolfTourGameMode::PreviewCourseFlyover()
{
    const ADiscGolfTourPlayerController* TourPC = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    if (bMainMenuVisible || ActiveDisc || bRegressionActive || ReplayActor || bLieTransitionActive || bScorecardVisible
        || (TourPC && (TourPC->IsCharacterCreatorOpen() || TourPC->IsControlsMenuOpen())))
    {
        CourseStatusText = TEXT("FLYOVER BLOCKED - WAIT FOR CURRENT PRESENTATION");
        return;
    }
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        Golfer && Golfer->IsAnimatedThrowActive())
    {
        CourseStatusText = TEXT("FLYOVER BLOCKED - FINISH OR CANCEL CHARACTER THROW");
        return;
    }
    ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr;
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!Route || !PC)
    {
        CourseStatusText = TEXT("FLYOVER UNAVAILABLE ON THIS COURSE");
        return;
    }
    if (Route->IsPreviewing())
    {
        SkipCurrentPresentation();
        return;
    }
    FString AcquireError;
    if (!DiscGolfCameraViewContract::TryAcquire(
        CameraViewState,
        EDiscGolfCameraViewOwner::CourseFlyover,
        EDiscGolfCameraViewMode::CourseFlyover,
        FlyoverCameraViewToken,
        AcquireError))
    {
        CourseStatusText = FString::Printf(TEXT("FLYOVER BLOCKED - %s"), *AcquireError);
        return;
    }
    FinishHoleIntroduction();
    StopBroadcastCamera(false);
    Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
    Route->OnFlyoverFinished.AddDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
    if (Route->StartPreview(PC))
    {
        ActiveFlyoverRoute = Route;
        CourseStatusText = FString::Printf(TEXT("COURSE FLYOVER | %d POINTS | %.0f M ROUTE"),
            Route->GetPointCount(), Route->GetRouteLengthCm() / 100.0f);
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveFlyover(true),
            ActiveHole ? ActiveHole->TeeLocation : FVector::ZeroVector);
    }
    else
    {
        ActiveFlyoverRoute = nullptr;
        FString ReleaseError;
        DiscGolfCameraViewContract::TryRelease(CameraViewState, FlyoverCameraViewToken, ReleaseError);
        FlyoverCameraViewToken = FDiscGolfCameraViewToken();
        CourseStatusText = TEXT("FLYOVER FAILED TO START");
    }
}

void ADiscGolfTourGameMode::HandleFlyoverFinished(ADiscGolfFlyoverRouteActor* FinishedRoute)
{
    if (FinishedRoute) FinishedRoute->OnFlyoverFinished.RemoveDynamic(
        this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
    if (FinishedRoute != ActiveFlyoverRoute
        || !DiscGolfCameraViewContract::IsCurrent(CameraViewState, FlyoverCameraViewToken))
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Ignored stale flyover completion from a prior route or camera generation."));
        return;
    }
    ActiveFlyoverRoute = nullptr;
    FString ReleaseError;
    const bool bReleased = DiscGolfCameraViewContract::TryRelease(
        CameraViewState, FlyoverCameraViewToken, ReleaseError);
    FlyoverCameraViewToken = FDiscGolfCameraViewToken();
    if (!bReleased)
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Ignored stale flyover camera completion: %s"), *ReleaseError);
        return;
    }
    ReturnCameraToPlayer();
    CourseStatusText = ActiveHole
        ? FString::Printf(TEXT("%s | FLYOVER COMPLETE"), *ActiveHole->GetCourseSummary())
        : TEXT("FLYOVER COMPLETE");
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveFlyover(false),
        ActiveHole ? ActiveHole->BasketLocation : FVector::ZeroVector);
}

void ADiscGolfTourGameMode::FinishCourseSmokeTest()
{
    if (!bCourseSmokeTestActive) return;
    bCourseSmokeTestActive = false;
    const ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr;
    const ADiscGolfTerrainPresentationActor* Ground = DevBootstrap
        ? DevBootstrap->GetCourseTerrainPresentation() : nullptr;
    int32 FixtureCount = 0;
    int32 GrassCount = 0;
    int32 RockCount = 0;
    int32 SignCount = 0;
    bool bFixtureContractsValid = true;
    for (TActorIterator<ADiscGolfWorldFixtureActor> It(GetWorld()); It; ++It)
    {
        const ADiscGolfWorldFixtureActor* Fixture = *It;
        ++FixtureCount;
        const bool bContractValid = Fixture->HasValidCollisionContract();
        bFixtureContractsValid &= bContractValid;
        if (!bContractValid)
        {
            const UStaticMeshComponent* Mesh = Fixture->GetStaticMeshComponent();
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("FIXTURE CONTRACT INVALID: id=%s type=%d collision=%d worldDynamic=%d overlaps=%s"),
                *Fixture->FixtureId.ToString(), static_cast<int32>(Fixture->FixtureType),
                Mesh ? static_cast<int32>(Mesh->GetCollisionEnabled()) : -1,
                Mesh ? static_cast<int32>(Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic)) : -1,
                Mesh && Mesh->GetGenerateOverlapEvents() ? TEXT("true") : TEXT("false"));
        }
        GrassCount += Fixture->FixtureType == EDiscGolfFixtureType::DenseGrass ? 1 : 0;
        RockCount += Fixture->FixtureType == EDiscGolfFixtureType::Rock ? 1 : 0;
        SignCount += Fixture->FixtureType == EDiscGolfFixtureType::Sign ? 1 : 0;
    }
    int32 FixtureVisualCount = 0;
    int32 FixtureVisualGrassCount = 0;
    int32 FixtureVisualRockCount = 0;
    int32 FixtureVisualSignCount = 0;
    int32 BrushVisualInstanceCount = 0;
    bool bFixtureVisualContractsValid = true;
    for (TActorIterator<ADiscGolfFixturePresentationActor> It(GetWorld()); It; ++It)
    {
        const ADiscGolfFixturePresentationActor* Visual = *It;
        ++FixtureVisualCount;
        bFixtureVisualContractsValid &= Visual->IsCollisionInvariant();
        FixtureVisualGrassCount += Visual->GetFixtureType() == EDiscGolfFixtureType::DenseGrass ? 1 : 0;
        FixtureVisualRockCount += Visual->GetFixtureType() == EDiscGolfFixtureType::Rock ? 1 : 0;
        FixtureVisualSignCount += Visual->GetFixtureType() == EDiscGolfFixtureType::Sign ? 1 : 0;
        BrushVisualInstanceCount += Visual->GetBrushInstanceCount();
    }
    const bool bFixtureVisualSetComplete = FixtureVisualCount == 0
        || (FixtureVisualCount == 9
            && FixtureVisualGrassCount == 3 && FixtureVisualRockCount == 3
            && FixtureVisualSignCount == 3 && BrushVisualInstanceCount >= 9
            && bFixtureVisualContractsValid);
    const bool bPassed = ActiveHole && ActiveHole->IsAuthoredBlockout()
        && ActiveHole->SurfaceCount == 11
        && ActiveHole->WorldFixtureCount == 15
        && DevBootstrap && DevBootstrap->GetPersistentHoleCount() == 3
        && Ground && Ground->IsReady() && Ground->GetCourseHoleCount() == 3
        && Ground->GetTriangleCount() > 10000 && Ground->GetConnectorTriangleCount() == 512
        && Ground->GetFairwayTriangleCount() == 1152
        && Ground->GetShorelineTriangleCount() == 576
        && Ground->GetTrailWearPatchCount() == 36
        && Ground->GetTrailWearTriangleCount() == 288
        && Ground->GetGrassBladeClusterCount() >= 4000
        && Ground->GetGrassTriangleCount() == Ground->GetGrassBladeClusterCount() * 4
        && Ground->GetGrassCardInstanceCount() == Ground->GetGrassBladeClusterCount() * 2
        && Ground->GetGrassSpeciesVariantCount() == 3
        && Ground->GetLitterClusterCount() >= 1000
        && Ground->GetLitterTriangleCount() == Ground->GetLitterClusterCount() * 2
        && Ground->GetLitterVariantCount() == 2
        && Ground->GetGroundCoverCullStartDistanceCm() == 11250
        && Ground->GetGroundCoverCullEndDistanceCm() == 30000
        && Ground->GetMaxGroundCoverSlopeDegrees() > 0.0f
        && Ground->GetMaxGroundCoverSlopeDegrees() <= 38.0f
        && Ground->GetGroundMacroVariationSpan() >= 96
        && Ground->GetMaxShorelineErosionOffsetCm() >= 100.0f
        && Ground->GetShoreRockInstanceCount() == 42
        && Ground->GetShoreReedClusterCount() == 96
        && Ground->GetShoreReedStemInstanceCount() == 384
        && Ground->GetShoreDeadfallInstanceCount() == 14
        && Ground->GetShoreAccentVariantCount() == 3
        && Ground->GetMinShorelineAccentRouteClearanceCm() >= 1050.0f
        && Ground->IsCollisionInvariant()
        && FixtureCount == 53 && GrassCount == 3 && RockCount == 3 && SignCount == 3
        && bFixtureContractsValid
        && bFixtureVisualSetComplete
        && ActiveHole->LandingZoneCount == 2
        && ActiveHole->ShotRouteCount == 3
        && ActiveHole->CameraAnchorCount == 3
        && ActiveHole->SpectatorBoundaryCount == 3
        && ActiveHole->WindZoneCount == 2
        && WindDirector && WindDirector->GetCourseZoneCount() == 6
        && Route && Route->GetPointCount() == 6
        && !Route->IsPreviewing()
        && CourseStatusText.Contains(TEXT("FLYOVER COMPLETE"));
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("PINE RIDGE COURSE SMOKE PASS: 3 persistent holes coexist with one %d-triangle ground mesh, feathered fairways/trails plus 36 wear patches, 576-triangle shoreline with 42 rock/96 reed/14 deadfall accents, %d grass clusters across 3 wind-reactive HISM species and %d litter clusters across 2 HISM variants, 300m cover cull, 53 collision fixtures, %d fixture visuals, %d brush instances, 6 wind zones, and active Hole 1 route/camera/flyover contracts."),
            Ground->GetTriangleCount(), Ground->GetGrassBladeClusterCount(), Ground->GetLitterClusterCount(),
            FixtureVisualCount, BrushVisualInstanceCount);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("PINE RIDGE COURSE SMOKE FAIL: %s | world fixtures=%d grass=%d rock=%d sign=%d contracts=%s | visuals=%d/%d brush=%d invariant=%s | route=%d previewing=%s"),
            *CourseStatusText, FixtureCount, GrassCount, RockCount, SignCount,
            bFixtureContractsValid ? TEXT("valid") : TEXT("invalid"),
            FixtureVisualCount, 3, BrushVisualInstanceCount,
            bFixtureVisualContractsValid ? TEXT("true") : TEXT("false"),
            Route ? Route->GetPointCount() : -1,
            Route && Route->IsPreviewing() ? TEXT("true") : TEXT("false"));
    }
    FPlatformMisc::RequestExit(false);
}

void ADiscGolfTourGameMode::FinishGroundGrassSmokeTest()
{
    const ADiscGolfTerrainPresentationActor* Ground = DevBootstrap
        ? DevBootstrap->GetCourseTerrainPresentation() : nullptr;
    int32 GroundActorCount = 0;
    for (TActorIterator<ADiscGolfTerrainPresentationActor> It(GetWorld()); It; ++It)
    {
        ++GroundActorCount;
    }
    const bool bHole3Ready = HasAuthoredRound()
        && ((ActiveHole && ActiveHole->HoleNumber == 3) || LoadRoundHole(3));
    const FVector HistoricalBlockerProbe(45026.8467f, 6052.3681f, -655.0f);
    bool bTerrainCollisionHit = false;
    FHitResult TerrainCollisionHit;
    if (GetWorld() && Ground)
    {
        FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGolfTerrainCollisionSmoke), false);
        Params.bReturnFaceIndex = true;
        if (ActiveDisc) Params.AddIgnoredActor(ActiveDisc);
        const FVector Start(HistoricalBlockerProbe.X, HistoricalBlockerProbe.Y, 5000.0f);
        const FVector End(HistoricalBlockerProbe.X, HistoricalBlockerProbe.Y, -1000.0f);
        for (int32 Attempt = 0; Attempt < 16; ++Attempt)
        {
            FHitResult Hit;
            if (!GetWorld()->LineTraceSingleByChannel(
                Hit, Start, End, ECC_Visibility, Params))
            {
                break;
            }
            AActor* HitActor = Hit.GetActor();
            if (HitActor && HitActor->ActorHasTag(TEXT("Presentation.CourseTerrain")))
            {
                TerrainCollisionHit = Hit;
                bTerrainCollisionHit = true;
                break;
            }
            if (!HitActor) break;
            Params.AddIgnoredActor(HitActor);
        }
    }
    ECourseSurfaceType BlockerSurface = ECourseSurfaceType::Fairway;
    FVector BlockerGround = HistoricalBlockerProbe;
    const bool bCombinedProbe = bHole3Ready && TraceCourseSurfaceAtLocation(
        HistoricalBlockerProbe, BlockerSurface, BlockerGround);
    const bool bHistoricalBlockerFixed = bTerrainCollisionHit && bCombinedProbe
        && Ground && Ground->IsBaseTerrainCollisionFace(TerrainCollisionHit.FaceIndex)
        && BlockerSurface == ECourseSurfaceType::DeepRough
        && FMath::IsNearlyEqual(TerrainCollisionHit.ImpactPoint.Z, -219.0f, 12.0f)
        && FMath::IsNearlyEqual(BlockerGround.Z, TerrainCollisionHit.ImpactPoint.Z, 2.0f)
        && BlockerGround.Z > HistoricalBlockerProbe.Z + 400.0f;

    const bool bPassed = ActiveHole && DevBootstrap
        && DevBootstrap->GetPersistentHoleCount() == 3
        && GroundActorCount == 1 && Ground && Ground->IsReady()
        && Ground->GetCourseHoleCount() == 3
        && Ground->GetTriangleCount() > 10000
        && Ground->GetConnectorTriangleCount() == 512
        && Ground->GetFairwayTriangleCount() == 1152
        && Ground->GetShorelineTriangleCount() == 576
        && Ground->GetTrailWearPatchCount() == 36
        && Ground->GetTrailWearTriangleCount() == 288
        && Ground->GetGrassBladeClusterCount() >= 4000
        && Ground->GetGrassTriangleCount() == Ground->GetGrassBladeClusterCount() * 4
        && Ground->GetGrassCardInstanceCount() == Ground->GetGrassBladeClusterCount() * 2
        && Ground->GetGrassSpeciesVariantCount() == 3
        && Ground->GetLitterClusterCount() >= 1000
        && Ground->GetLitterTriangleCount() == Ground->GetLitterClusterCount() * 2
        && Ground->GetLitterVariantCount() == 2
        && Ground->GetGroundCoverCullStartDistanceCm() == 11250
        && Ground->GetGroundCoverCullEndDistanceCm() == 30000
        && Ground->GetMaxGroundCoverSlopeDegrees() > 0.0f
        && Ground->GetMaxGroundCoverSlopeDegrees() <= 38.0f
        && Ground->GetGroundMacroVariationSpan() >= 96
        && Ground->GetMaxShorelineErosionOffsetCm() >= 100.0f
        && Ground->GetShoreRockInstanceCount() == 42
        && Ground->GetShoreReedClusterCount() == 96
        && Ground->GetShoreReedStemInstanceCount() == 384
        && Ground->GetShoreDeadfallInstanceCount() == 14
        && Ground->GetShoreAccentVariantCount() == 3
        && Ground->GetMinShorelineAccentRouteClearanceCm() >= 1050.0f
        && Ground->IsCollisionInvariant()
        && bHistoricalBlockerFixed;
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("GROUND COVER SMOKE PASS: one persistent %d-triangle course ground, macro span=%d, 1152 feathered fairway, 512 feathered connector plus 36 wear patches/288 triangles, 576 shoreline triangles with %.0fcm erosion variation and 42 rock/96 reed-cluster/384 textured reed-stem/14 deadfall accents at %.0fcm minimum route clearance, %d slope-aligned grass clusters/%d HISM cards/%d logical triangles across 3 wind-reactive species, %d litter clusters/%d triangles across 2 slope/height-aware HISM variants, cull=%d-%dcm max_slope=%.2fdeg, 3 holes, deterministic base collision; historical Hole 3 blocker now resolves DeepRough at visible Z=%.1fcm."),
            Ground->GetTriangleCount(), Ground->GetGroundMacroVariationSpan(),
            Ground->GetMaxShorelineErosionOffsetCm(),
            Ground->GetMinShorelineAccentRouteClearanceCm(),
            Ground->GetGrassBladeClusterCount(),
            Ground->GetGrassCardInstanceCount(), Ground->GetGrassTriangleCount(),
            Ground->GetLitterClusterCount(), Ground->GetLitterTriangleCount(),
            Ground->GetGroundCoverCullStartDistanceCm(),
            Ground->GetGroundCoverCullEndDistanceCm(),
            Ground->GetMaxGroundCoverSlopeDegrees(), BlockerGround.Z);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("GROUND COVER SMOKE FAIL: actors=%d ground=%s ready=%s holes=%d terrain=%d macro=%d erosion=%.0fcm fairway=%d connectors=%d shoreline=%d grass=%d clusters/%d cards/%d triangles species=%d litter=%d/%d variants=%d cull=%d-%d slope=%.2f invariant=%s blocker_hit=%s combined=%s surface=%d collision_z=%.1f resolved_z=%.1f fixed=%s."),
            GroundActorCount, Ground ? TEXT("yes") : TEXT("no"),
            Ground && Ground->IsReady() ? TEXT("true") : TEXT("false"),
            Ground ? Ground->GetCourseHoleCount() : 0,
            Ground ? Ground->GetTriangleCount() : 0,
            Ground ? Ground->GetGroundMacroVariationSpan() : 0,
            Ground ? Ground->GetMaxShorelineErosionOffsetCm() : 0.0f,
            Ground ? Ground->GetFairwayTriangleCount() : 0,
            Ground ? Ground->GetConnectorTriangleCount() : 0,
            Ground ? Ground->GetShorelineTriangleCount() : 0,
            Ground ? Ground->GetGrassBladeClusterCount() : 0,
            Ground ? Ground->GetGrassCardInstanceCount() : 0,
            Ground ? Ground->GetGrassTriangleCount() : 0,
            Ground ? Ground->GetGrassSpeciesVariantCount() : 0,
            Ground ? Ground->GetLitterClusterCount() : 0,
            Ground ? Ground->GetLitterTriangleCount() : 0,
            Ground ? Ground->GetLitterVariantCount() : 0,
            Ground ? Ground->GetGroundCoverCullStartDistanceCm() : 0,
            Ground ? Ground->GetGroundCoverCullEndDistanceCm() : 0,
            Ground ? Ground->GetMaxGroundCoverSlopeDegrees() : 0.0f,
            Ground && Ground->IsCollisionInvariant() ? TEXT("true") : TEXT("false"),
            bTerrainCollisionHit ? TEXT("true") : TEXT("false"),
            bCombinedProbe ? TEXT("true") : TEXT("false"),
            static_cast<int32>(BlockerSurface),
            bTerrainCollisionHit ? TerrainCollisionHit.ImpactPoint.Z : 0.0f,
            BlockerGround.Z,
            bHistoricalBlockerFixed ? TEXT("true") : TEXT("false"));
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::FinishGalleryLakeWaterSmokeTest()
{
    const ADiscGolfWaterPresentationActor* Water = DevBootstrap
        ? DevBootstrap->GetWaterPresentation() : nullptr;
    int32 WaterAuthorityCount = 0;
    bool bAuthorityHidden = false;
    bool bAuthorityCollisionActive = false;
    bool bAuthorityIsHazard = false;
    for (TActorIterator<ADiscGolfCourseSurfaceActor> It(GetWorld()); It; ++It)
    {
        const ADiscGolfCourseSurfaceActor* Surface = *It;
        if (!Surface->ActorHasTag(TEXT("Presentation.Surface.Water"))) continue;
        ++WaterAuthorityCount;
        const UStaticMeshComponent* Mesh = Surface->GetStaticMeshComponent();
        bAuthorityHidden |= Mesh && Mesh->bHiddenInGame;
        bAuthorityCollisionActive |= Mesh
            && Mesh->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
        bAuthorityIsHazard |= Surface->GetCourseSurfaceType() == ECourseSurfaceType::Hazard;
    }

    const bool bPassed = ActiveHole && ActiveHole->HoleNumber == 3
        && ActiveHole->SurfaceCount == 12
        && Water && Water->IsReady() && Water->GetTriangleCount() == 2944
        && Water->IsCollisionInvariant()
        && WaterAuthorityCount == 1 && bAuthorityHidden
        && bAuthorityCollisionActive && bAuthorityIsHazard;
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("GALLERY LAKE WATER SMOKE PASS: 2944 animated presentation triangles, hidden hazard authority retained collision and penalty identity."));
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("GALLERY LAKE WATER SMOKE FAIL: hole=%d surfaces=%d water=%s ready=%s triangles=%d invariant=%s authority=%d hidden=%s collision=%s hazard=%s."),
            ActiveHole ? ActiveHole->HoleNumber : 0,
            ActiveHole ? ActiveHole->SurfaceCount : 0,
            Water ? TEXT("yes") : TEXT("no"),
            Water && Water->IsReady() ? TEXT("true") : TEXT("false"),
            Water ? Water->GetTriangleCount() : 0,
            Water && Water->IsCollisionInvariant() ? TEXT("true") : TEXT("false"),
            WaterAuthorityCount,
            bAuthorityHidden ? TEXT("true") : TEXT("false"),
            bAuthorityCollisionActive ? TEXT("true") : TEXT("false"),
            bAuthorityIsHazard ? TEXT("true") : TEXT("false"));
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::FinishDenseForestSmokeTest()
{
    FDiscGolfCoursePresentationDefinition Presentation;
    FString PresentationSource;
    FString PresentationError;
    const bool bPresentationLoaded = DiscGolfCoursePresentation::LoadPineRidge(
        Presentation, PresentationSource, PresentationError);
    bool bPassed = bPresentationLoaded && DevBootstrap
        && DevBootstrap->GetPersistentHoleCount() == ActiveHoleDefinitions.Num()
        && ActiveHoleDefinitions.Num() == 3;
    int32 TotalDecorative = 0;
    int32 TotalAuthored = 0;
    for (const FDiscGolfHoleBlockoutDefinition& Definition : ActiveHoleDefinitions)
    {
        const FDiscGolfHoleVisualPlan* Plan = Presentation.Holes.FindByPredicate(
            [&Definition](const FDiscGolfHoleVisualPlan& Candidate)
            {
                return Candidate.HoleNumber == Definition.HoleNumber;
            });
        const ADiscGolfFoliagePresentationActor* Foliage = DevBootstrap
            ? DevBootstrap->GetFoliagePresentationForHole(Definition.HoleNumber) : nullptr;
        int32 TreeProxyCount = 0;
        bool bTreeContractsValid = true;
        const FString TreePrefix = FString::Printf(TEXT("H%02d_Tree"), Definition.HoleNumber);
        for (TActorIterator<ADiscGolfWorldFixtureActor> It(GetWorld()); It; ++It)
        {
            if (It->FixtureType != EDiscGolfFixtureType::Tree
                || !It->FixtureId.ToString().StartsWith(TreePrefix)) continue;
            ++TreeProxyCount;
            bTreeContractsValid &= It->HasValidCollisionContract();
        }
        const int32 DecorativeCount = Foliage ? Foliage->GetDecorativeInstanceCount() : 0;
        const int32 DecorativeTarget = Foliage ? Foliage->GetDecorativeInstanceTarget() : 0;
        const int32 AuthoredTreeCount = Definition.Trees.Num();
        const bool bHolePassed = Plan && Foliage && Foliage->IsReady()
            && Foliage->IsCollisionInvariant()
            && Foliage->GetForestReferenceId() == Plan->ForestReferenceId
            && DecorativeTarget >= 100
            && DecorativeCount >= FMath::CeilToInt(DecorativeTarget * 0.85f)
            && Foliage->GetVisualInstanceCount() == DecorativeCount + AuthoredTreeCount
            && TreeProxyCount == AuthoredTreeCount && bTreeContractsValid;
        bPassed &= bHolePassed;
        TotalDecorative += DecorativeCount;
        TotalAuthored += AuthoredTreeCount;
        if (bHolePassed)
        {
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("DENSE FOREST HOLE %d %s: %d/%d decorative, %d/%d authoritative trunks, invariant=true."),
                Definition.HoleNumber, *Plan->ForestReferenceId.ToString(), DecorativeCount,
                DecorativeTarget, TreeProxyCount, AuthoredTreeCount);
        }
        else
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("DENSE FOREST HOLE %d FAIL: plan=%s foliage=%s decorative=%d/%d trunks=%d/%d invariant=%s."),
                Definition.HoleNumber, Plan ? TEXT("yes") : TEXT("no"),
                Foliage ? TEXT("yes") : TEXT("no"), DecorativeCount, DecorativeTarget,
                TreeProxyCount, AuthoredTreeCount,
                Foliage && Foliage->IsCollisionInvariant() ? TEXT("true") : TEXT("false"));
        }
    }
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DENSE FOREST SMOKE PASS: %d persistent holes, %d decorative forest instances, %d unchanged collision trunks."),
            DevBootstrap->GetPersistentHoleCount(), TotalDecorative, TotalAuthored);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DENSE FOREST SMOKE FAIL: %d persistent holes, %d decorative forest instances, %d unchanged collision trunks."),
            DevBootstrap ? DevBootstrap->GetPersistentHoleCount() : 0,
            TotalDecorative, TotalAuthored);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::StartPineRidgePlaySmokeTest()
{
    if (!bPineRidgePlaySmokeTestActive) return;
    if (!ActiveHole || !ActiveHole->IsAuthoredBlockout())
    {
        if (!LoadCourse(TEXT("PineRidge")))
        {
            UE_LOG(LogDiscGolfTour, Error, TEXT("PINE RIDGE PLAY SMOKE FAIL: course could not load."));
            FPlatformMisc::RequestExit(false);
            return;
        }
    }

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer)
    {
        UE_LOG(LogDiscGolfTour, Error, TEXT("PINE RIDGE PLAY SMOKE FAIL: golfer unavailable."));
        FPlatformMisc::RequestExit(false);
        return;
    }
    Golfer->FaceLocation(ActiveHole->BasketLocation);
    FThrowCommand Command;
    Command.MoldId = TEXT("Apex");
    Command.Plastic = EDiscPlastic::Tour;
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Drive;
    Command.Direction = (ActiveHole->BasketLocation - Golfer->GetActorLocation())
        .GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    Command.Power01 = 0.82f;
    Command.HyzerDeg = 3.0f;
    Command.NoseAngleDeg = 1.0f;
    Command.LaunchAngleDeg = 7.0f;
    Command.TimingError = 0.0f;
    FString CommandError;
    if (!TryBindAutomatedPlayerCommand(Golfer, Command, CommandError)
        || !RequestThrow(Command))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("PINE RIDGE PLAY SMOKE FAIL: throw ingress rejected (%s)."),
            CommandError.IsEmpty() ? TEXT("authoritative launch failed") : *CommandError);
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }
}

void ADiscGolfTourGameMode::FinishPineRidgePlaySmokeTest(bool bHoledOut)
{
    if (!bPineRidgePlaySmokeTestActive) return;
    bPineRidgePlaySmokeTestActive = false;
    const FDiscTrajectorySummary Summary = GetLastTrajectorySummary();
    int32 ExpectedWindZoneCount = 0;
    for (const FDiscGolfHoleBlockoutDefinition& Definition : ActiveHoleDefinitions)
    {
        ExpectedWindZoneCount += Definition.WindZones.Num();
    }
    const bool bPassed = ActiveHole && ActiveHole->IsAuthoredBlockout()
        && Strokes == 1 && bHasLastFlightTelemetry
        && Summary.SampleCount >= 100
        && Summary.DurationSeconds > 0.25f && Summary.DurationSeconds <= 30.1f
        && (Summary.GroundContactCount >= 1 || bHoledOut)
        && Summary.FinalWorldLocationCm.Z > -200.0f
        && bPineRidgeSmokeSawAuthoredCamera
        && bPineRidgeSmokeEnteredWindZone
        && ExpectedWindZoneCount > 0
        && WindDirector && WindDirector->GetCourseZoneCount() == ExpectedWindZoneCount;
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("PINE RIDGE PLAY SMOKE PASS: %d samples, %.1f m final carry, %d contacts, surface %s, authored camera and local wind active."),
            Summary.SampleCount, Summary.FinalCarryMeters, Summary.GroundContactCount,
            *DiscGolfCourseRules::SurfaceName(Summary.SurfaceAtRest));
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("PINE RIDGE PLAY SMOKE FAIL: samples=%d duration=%.2f contacts=%d final_z=%.1f camera=%d wind_entered=%d wind_zones=%d expected=%d."),
            Summary.SampleCount, Summary.DurationSeconds, Summary.GroundContactCount,
            Summary.FinalWorldLocationCm.Z, bPineRidgeSmokeSawAuthoredCamera ? 1 : 0,
            bPineRidgeSmokeEnteredWindZone ? 1 : 0,
            WindDirector ? WindDirector->GetCourseZoneCount() : -1, ExpectedWindZoneCount);
    }
    if (!bSession12PresentationSmokeTestActive)
    {
        FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
        return;
    }

    bSession12BaseSmokePassed = bPassed;
    if (!bPassed)
    {
        bSession12PresentationSmokeTestActive = false;
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("SESSION 12 PRESENTATION SMOKE FAIL: base Pine Ridge throw failed."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }

    ToggleInstantReplay();
    if (!ReplayActor)
    {
        bSession12PresentationSmokeTestActive = false;
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("SESSION 12 PRESENTATION SMOKE FAIL: strict actual-sample replay did not start."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }
    const float InitialReplayRate = ReplayActor->GetPlaybackRate();
    const EDiscReplayCameraMode InitialReplayCamera = ReplayActor->GetCameraMode();
    ToggleReplayPause();
    const bool bPauseWorked = ReplayActor->IsReplayPaused();
    const float SeekTarget = FMath::Min(0.25f, ReplayActor->GetDurationSeconds() * 0.25f);
    Session12ReplaySeekTargetSeconds = SeekTarget;
    SeekReplayRelative(SeekTarget);
    const bool bSeekWorked = FMath::IsNearlyEqual(
        ReplayActor->GetPlaybackTimeSeconds(), SeekTarget, KINDA_SMALL_NUMBER);
    CycleReplayPlaybackRate();
    const bool bRateWorked = !FMath::IsNearlyEqual(
        ReplayActor->GetPlaybackRate(), InitialReplayRate, KINDA_SMALL_NUMBER);
    CycleReplayCamera();
    const bool bCameraWorked = ReplayActor->GetCameraMode() != InitialReplayCamera;
    ToggleReplayPause();
    bSession12ReplayControlsPassed = bPauseWorked && bSeekWorked && bRateWorked
        && bCameraWorked && ReplayActor->IsReplayPlaying();

    FTimerHandle PresentationSmokeTimer;
    GetWorldTimerManager().SetTimer(PresentationSmokeTimer, FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::FinishSession12PresentationSmokeTest), 0.75f, false);
}

void ADiscGolfTourGameMode::FinishSession12PresentationSmokeTest()
{
    if (!bSession12PresentationSmokeTestActive) return;
    bSession12PresentationSmokeTestActive = false;

    FString ReplayValidationError;
    const bool bReplayCaptureValid = ADiscReplayActor::ValidateReplayInput(
        LastReplaySamples, ReplayValidationError);
    const bool bReplayAdvanced = ReplayActor
        && ReplayActor->GetPlaybackTimeSeconds()
            > Session12ReplaySeekTargetSeconds + 0.01f;
    const bool bReplayOwned = ReplayActor
        && DiscGolfCameraViewContract::IsCurrent(CameraViewState, ReplayCameraViewToken);
    const bool bAudioRouted = PresentationAudioRouter
        && PresentationAudioRouter->GetRouteTraceCount() > 0;
    const bool bPassed = bSession12BaseSmokePassed && bReplayCaptureValid
        && LastReplaySamples.Num() >= 2
        && LastReplaySamples.Num() <= ADiscReplayActor::MaxReplayInputSamples
        && bReplayAdvanced && bReplayOwned && bAudioRouted
        && bSession12ReplayControlsPassed
        && bLastReplayCaptureActualSampleProvenanceValid
        && bLastReplayCaptureNominalRateValid;

    const int32 ReplaySampleCount = LastReplaySamples.Num();
    const float ReplayProgress = ReplayActor ? ReplayActor->GetProgress01() : 0.0f;
    const int32 AudioRouteCount = PresentationAudioRouter
        ? PresentationAudioRouter->GetRouteTraceCount() : 0;
    StopInstantReplay(true);

    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("SESSION 12 PRESENTATION SMOKE PASS: %d bounded actual samples, replay progress %.3f, exclusive view ownership, pause/seek/rate/camera controls, and %d sanitized audio routes."),
            ReplaySampleCount, ReplayProgress, AudioRouteCount);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("SESSION 12 PRESENTATION SMOKE FAIL: samples=%d replay_valid=%d replay_progress=%.3f replay_owned=%d controls=%d provenance=%d rate_bound=%d audio_routes=%d error=%s."),
            ReplaySampleCount, bReplayCaptureValid ? 1 : 0, ReplayProgress,
            bReplayOwned ? 1 : 0, bSession12ReplayControlsPassed ? 1 : 0,
            bLastReplayCaptureActualSampleProvenanceValid ? 1 : 0,
            bLastReplayCaptureNominalRateValid ? 1 : 0,
            AudioRouteCount, *ReplayValidationError);
    }
#if DG_WITH_DEVELOPMENT_CONTENT
    if (bPassed && FParse::Param(
        FCommandLine::Get(), TEXT("Session16CorePlayabilityGate")))
    {
        bSession16TemporaryStateBasePassed = true;
        FTimerHandle Session16TemporaryStateTimer;
        GetWorldTimerManager().SetTimer(
            Session16TemporaryStateTimer,
            FTimerDelegate::CreateUObject(
                this, &ADiscGolfTourGameMode::FinishSession16TemporaryStateSmokeTest),
            0.75f,
            false);
        return;
    }
#endif
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::FinishSession16TemporaryStateSmokeTest()
{
    const bool bBasePassed = bSession16TemporaryStateBasePassed;
    ADiscGolfTourPlayerController* PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
    const bool bReplayRecovered = ReplayActor == nullptr && !CameraViewState.bActive;
    const bool bCameraRecovered = PlayerController && PlayerPawn
        && PlayerController->GetViewTarget() == PlayerPawn;
    FString InputRecoveryError;
    const bool bPauseAndInputRecovered = PlayerController
        && PlayerController->RunPlayabilityPauseResumeProbe(InputRecoveryError);
    const bool bPassed = bBasePassed
        && bReplayRecovered
        && bCameraRecovered
        && bPauseAndInputRecovered
        && PlayabilityFailureCount == 0;
    bSession16TemporaryStateBasePassed = false;

    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION16_TEMPORARY_STATE_RECOVERY: PASS pause_resume=1 replay_exit=1 camera_owner=player input_route=gameplay"));
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION16_TEMPORARY_STATE_RECOVERY: FAIL base=%d replay=%d camera=%d input=%d playability_failures=%d reason=%s"),
            bBasePassed ? 1 : 0,
            bReplayRecovered ? 1 : 0,
            bCameraRecovered ? 1 : 0,
            bPauseAndInputRecovered ? 1 : 0,
            PlayabilityFailureCount,
            *InputRecoveryError);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::StartHole1FlightRouteSmokeTest()
{
    if (!bHole1FlightRouteSmokeTestActive) return;
    if (!ActiveHole || ActiveHole->HoleNumber != 1 || !ActiveHole->IsAuthoredBlockout())
    {
        if (!LoadCourse(TEXT("PineRidge")))
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("HOLE 1 FLIGHT ROUTES FAIL: persistent course could not load."));
            FPlatformMisc::RequestExitWithStatus(false, 1);
            return;
        }
    }
    Hole1FlightRouteIndex = 0;
    Hole1FlightRouteResults.Reset();
    StartNextHole1FlightRouteThrow();
}

void ADiscGolfTourGameMode::StartNextHole1FlightRouteThrow()
{
    if (!bHole1FlightRouteSmokeTestActive) return;
    struct FScenario
    {
        const TCHAR* Name;
        FName RouteId;
        EThrowStyle Style;
        float Power;
        float Hyzer;
        float Nose;
        float Launch;
    };
    static const FScenario Scenarios[] =
    {
        { TEXT("RHBH Flat"), TEXT("CenterPlacement"), EThrowStyle::Backhand, 0.90f, 0.0f, 0.0f, 7.0f },
        { TEXT("RHBH Hyzer"), TEXT("SkipShelfAttack"), EThrowStyle::Backhand, 0.92f, 8.0f, 0.0f, 7.5f },
        { TEXT("RHBH Turnover/Anhyzer"), TEXT("RightBailout"), EThrowStyle::Backhand, 0.86f, -1.5f, 0.0f, 7.0f },
        { TEXT("RHFH Equivalent"), TEXT("RightBailout"), EThrowStyle::Forehand, 0.82f, 3.0f, 1.0f, 7.0f }
    };
    if (Hole1FlightRouteIndex >= UE_ARRAY_COUNT(Scenarios))
    {
        FinishHole1FlightRouteSmokeTest();
        return;
    }

    ResetHole();
    const FDiscGolfHoleBlockoutDefinition* Definition = GetActiveHoleDefinition();
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    const FScenario& Scenario = Scenarios[Hole1FlightRouteIndex];
    const FDiscGolfShotRouteDefinition* Route = Definition
        ? Definition->ShotRoutes.FindByPredicate([&Scenario](const auto& Candidate)
            { return Candidate.RouteId == Scenario.RouteId; })
        : nullptr;
    if (!Definition || !Golfer || !Route || Route->WaypointsCm.Num() < 2)
    {
        FDiscGolfHole1FlightRouteResult Failed;
        Failed.Scenario = Scenario.Name;
        Failed.RouteId = Scenario.RouteId.ToString();
        Failed.ThrowStyle = Scenario.Style;
        Failed.Reason = TEXT("authored golfer/route data unavailable");
        Hole1FlightRouteResults.Add(Failed);
        ++Hole1FlightRouteIndex;
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::StartNextHole1FlightRouteThrow));
        return;
    }

    FVector AimPoint = Route->WaypointsCm[1];
    if (Scenario.Style == EThrowStyle::Forehand)
    {
        // Aim a stable forehand into the center window and let its mirrored flight enter the
        // right route. Pointing it at the outer waypoint double-counts the lateral shot shape.
        if (const FDiscGolfShotRouteDefinition* CenterRoute =
            Definition->ShotRoutes.FindByPredicate([](const auto& Candidate)
                { return Candidate.RouteType == EDiscGolfShotRouteType::Primary; }))
        {
            if (CenterRoute->WaypointsCm.Num() >= 2) AimPoint = CenterRoute->WaypointsCm[1];
        }
    }
    AimPoint.Z = Golfer->GetActorLocation().Z;
    Golfer->FaceLocation(AimPoint);
    FThrowCommand Command;
    Command.MoldId = TEXT("Apex");
    Command.Plastic = EDiscPlastic::Tour;
    Command.ThrowStyle = Scenario.Style;
    Command.ShotContext = EDiscShotContext::Drive;
    Command.Direction = (AimPoint - Golfer->GetActorLocation())
        .GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    Command.Power01 = Scenario.Power;
    Command.HyzerDeg = Scenario.Hyzer;
    Command.NoseAngleDeg = Scenario.Nose;
    Command.LaunchAngleDeg = Scenario.Launch;
    Command.TimingError = 0.0f;
    UE_LOG(LogDiscGolfTour, Display, TEXT("HOLE 1 FLIGHT ROUTE %d/4: %s via %s"),
        Hole1FlightRouteIndex + 1, Scenario.Name, *Scenario.RouteId.ToString());
    FString CommandError;
    if (!TryBindAutomatedPlayerCommand(Golfer, Command, CommandError)
        || !RequestThrow(Command))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("HOLE 1 FLIGHT ROUTE FAIL: throw ingress rejected (%s)."),
            CommandError.IsEmpty() ? TEXT("authoritative launch failed") : *CommandError);
        FinishHole1FlightRouteSmokeTest();
    }
}

void ADiscGolfTourGameMode::RecordHole1FlightRouteResult(bool bHoledOut)
{
    if (!bHole1FlightRouteSmokeTestActive) return;
    static const TCHAR* ScenarioNames[] =
    {
        TEXT("RHBH Flat"), TEXT("RHBH Hyzer"),
        TEXT("RHBH Turnover/Anhyzer"), TEXT("RHFH Equivalent")
    };
    static const TCHAR* RouteIds[] =
    {
        TEXT("CenterPlacement"), TEXT("SkipShelfAttack"),
        TEXT("RightBailout"), TEXT("RightBailout")
    };
    static const EThrowStyle Styles[] =
    {
        EThrowStyle::Backhand, EThrowStyle::Backhand,
        EThrowStyle::Backhand, EThrowStyle::Forehand
    };
    if (!ActiveHole || Hole1FlightRouteIndex < 0 || Hole1FlightRouteIndex >= 4) return;

    const FDiscTrajectorySummary Summary = GetLastTrajectorySummary();
    FDiscGolfHole1FlightRouteResult Result;
    Result.Scenario = ScenarioNames[Hole1FlightRouteIndex];
    Result.RouteId = RouteIds[Hole1FlightRouteIndex];
    Result.ThrowStyle = Styles[Hole1FlightRouteIndex];
    Result.FinalCarryMeters = Summary.FinalCarryMeters;
    Result.LateralMeters = Summary.LateralMeters;
    Result.RemainingToBasketMeters = FVector::Dist2D(
        Summary.FinalWorldLocationCm, ActiveHole->BasketLocation) / 100.0f;
    Result.SampleCount = Summary.SampleCount;
    Result.FixtureContactCount = LastFlightTelemetry.FixtureContactCount;
    Result.bPassed = Summary.SampleCount >= 100
        && Summary.DurationSeconds > 0.25f && Summary.DurationSeconds <= 30.1f
        && (Summary.GroundContactCount >= 1 || bHoledOut)
        && Summary.FinalCarryMeters >= 78.0f && Summary.FinalCarryMeters <= 130.0f
        && Result.RemainingToBasketMeters <= 45.0f
        && Summary.PenaltyType == EDiscGolfPenaltyType::None
        && Result.FixtureContactCount <= 1;
    if (!Result.bPassed)
    {
        Result.Reason = FString::Printf(TEXT(
            "samples=%d carry=%.1fm remaining=%.1fm contacts=%d penalty=%d"),
            Result.SampleCount, Result.FinalCarryMeters, Result.RemainingToBasketMeters,
            Result.FixtureContactCount, static_cast<int32>(Summary.PenaltyType));
    }
    else
    {
        Result.Reason = TEXT("reachable legal route with no invisible-wall behavior");
    }
    Hole1FlightRouteResults.Add(Result);
    ++Hole1FlightRouteIndex;
    GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::StartNextHole1FlightRouteThrow));
}

void ADiscGolfTourGameMode::FinishHole1FlightRouteSmokeTest()
{
#if DG_RELEASE_V05_SCOPE
    // This command-driven report is development evidence. It must not create an
    // diagnostic artifact even if an internal caller reaches the seam.
    bHole1FlightRouteSmokeTestActive = false;
    return;
#else
    if (!bHole1FlightRouteSmokeTestActive) return;
    bHole1FlightRouteSmokeTestActive = false;
    const bool bPassed = Hole1FlightRouteResults.Num() == 4
        && !Hole1FlightRouteResults.ContainsByPredicate([](const auto& Result)
            { return !Result.bPassed; });
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("pine_ridge_hole1_flight_route_acceptance"));
    Root->SetNumberField(TEXT("schemaVersion"), 1);
    Root->SetBoolField(TEXT("passed"), bPassed);
    Root->SetNumberField(TEXT("routeCount"), Hole1FlightRouteResults.Num());
    TArray<TSharedPtr<FJsonValue>> Results;
    for (const FDiscGolfHole1FlightRouteResult& Result : Hole1FlightRouteResults)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("scenario"), Result.Scenario);
        Object->SetStringField(TEXT("routeId"), Result.RouteId);
        Object->SetStringField(TEXT("throwStyle"),
            Result.ThrowStyle == EThrowStyle::Forehand ? TEXT("Forehand") : TEXT("Backhand"));
        Object->SetNumberField(TEXT("finalCarryMeters"), Result.FinalCarryMeters);
        Object->SetNumberField(TEXT("lateralMeters"), Result.LateralMeters);
        Object->SetNumberField(TEXT("remainingToBasketMeters"), Result.RemainingToBasketMeters);
        Object->SetNumberField(TEXT("sampleCount"), Result.SampleCount);
        Object->SetNumberField(TEXT("fixtureContactCount"), Result.FixtureContactCount);
        Object->SetBoolField(TEXT("passed"), Result.bPassed);
        Object->SetStringField(TEXT("reason"), Result.Reason);
        Results.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("results"), Results);
    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("EnvironmentReports"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, TEXT("PineRidgeHole1FlightRoutes.json"));
    const bool bWrote = FFileHelper::SaveStringToFile(Json, *Path);
    int32 PassedCount = 0;
    for (const FDiscGolfHole1FlightRouteResult& Result : Hole1FlightRouteResults)
    {
        if (Result.bPassed) ++PassedCount;
    }
    if (bPassed && bWrote)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("HOLE 1 FLIGHT ROUTES PASS: %d/4 passed; report %s"), PassedCount, *Path);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("HOLE 1 FLIGHT ROUTES FAIL: %d/4 passed; report %s"), PassedCount, *Path);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed && bWrote ? 0 : 1);
#endif
}

void ADiscGolfTourGameMode::StartThreeHoleRoundSmokeTest()
{
    if (!bThreeHoleRoundSmokeTestActive) return;
    if (!HasAuthoredRound() || !ActiveHole || !ActiveHole->IsAuthoredBlockout())
    {
        if (!LoadCourse(TEXT("PineRidge")))
        {
            FinishThreeHoleRoundSmokeTest(false, TEXT("course or manifest could not load"));
            return;
        }
    }
    else
    {
        RestartRound();
    }
    if (!HasAuthoredRound() || RoundState.HoleScores.Num() != 3)
    {
        FinishThreeHoleRoundSmokeTest(false, TEXT("manifest did not initialize three holes"));
        return;
    }
    if (WindDirector)
    {
        FString WindError;
        if (!WindDirector->TryConfigurePhysicsWind(
            FVector::ZeroVector, 0.0f, WindDirector->GustFrequencyHz, WindError))
        {
            FinishThreeHoleRoundSmokeTest(false,
                FString::Printf(TEXT("calm wind setup failed: %s"), *WindError));
            return;
        }
    }
    StartRoundSmokePutt();
}

void ADiscGolfTourGameMode::StartRoundSmokePutt()
{
    if (!bThreeHoleRoundSmokeTestActive || !ActiveHole)
    {
        FinishThreeHoleRoundSmokeTest(false, TEXT("active hole was unavailable"));
        return;
    }
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    const FPhysicsRegressionPreset* PuttPreset = Trajectories
        ? Trajectories->GetRegressionPresets().FindByPredicate([](const FPhysicsRegressionPreset& Preset)
        {
            return Preset.PresetId == TEXT("TouchCircle1Center");
        })
        : nullptr;
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!PuttPreset || !Golfer)
    {
        FinishThreeHoleRoundSmokeTest(false, TEXT("center-chain putt preset or golfer was unavailable"));
        return;
    }

    const FVector HoleDirection = FVector(
        ActiveHole->BasketLocation.X - ActiveHole->TeeLocation.X,
        ActiveHole->BasketLocation.Y - ActiveHole->TeeLocation.Y,
        0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    CurrentLieLocation = ActiveHole->BasketLocation - HoleDirection * 700.0f;
    CurrentLieLocation.Z = ActiveHole->BasketLocation.Z;
    MovePlayerToLie(CurrentLieLocation);
    UpdateLieType();
    Golfer->FaceLocation(ActiveHole->BasketLocation);
    TrajectoryStatusText = FString::Printf(TEXT("THREE-HOLE SMOKE - HOLE %d CENTER-CHAIN PUTT"), ActiveHole->HoleNumber);
    FThrowCommand Command = PuttPreset->MakeCommand(Golfer->GetActorForwardVector());
    FString CommandError;
    if (!TryBindAutomatedPlayerCommand(Golfer, Command, CommandError)
        || !RequestThrow(Command)
        || !ActiveDisc)
    {
        FinishThreeHoleRoundSmokeTest(false, FString::Printf(
            TEXT("smoke putt could not launch: %s"),
            CommandError.IsEmpty() ? TEXT("authoritative launch failed") : *CommandError));
    }
}

void ADiscGolfTourGameMode::AdvanceThreeHoleRoundSmokeTest()
{
    if (!bThreeHoleRoundSmokeTestActive) return;
    if (RoundState.bRoundComplete)
    {
        const bool bPassed = ActiveHole && ActiveHole->HoleNumber == 3
            && ActiveHole->DefinitionSource == TEXT("PERSISTENT AUTHORED COURSE")
            && DevBootstrap && DevBootstrap->GetPersistentHoleCount() == 3
            && DiscGolfRound::CompletedHoleCount(RoundState) == 3
            && DiscGolfRound::TotalStrokes(RoundState) == 3
            && DiscGolfRound::CompletedPar(RoundState) == 11
            && DiscGolfRound::ScoreToPar(RoundState) == -8;
        FinishThreeHoleRoundSmokeTest(bPassed, bPassed
            ? TEXT("all manifest holes completed and scored")
            : TEXT("final round totals or authored-hole state did not match"));
        return;
    }
    AdvanceToNextHole();
    if (!ActiveHole || ActiveHole->HoleNumber != RoundState.CurrentHoleIndex + 1)
    {
        FinishThreeHoleRoundSmokeTest(false, TEXT("hole transition did not load the expected manifest entry"));
        return;
    }
    FTimerHandle PuttTimer;
    GetWorldTimerManager().SetTimer(PuttTimer, FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::StartRoundSmokePutt), 0.25f, false);
}

void ADiscGolfTourGameMode::FinishThreeHoleRoundSmokeTest(bool bPassed, const FString& Reason)
{
    if (!bThreeHoleRoundSmokeTestActive) return;
    FString FinalReason = Reason;
    if (bPassed)
    {
        int32 HoleStartCount = 0;
        int32 HoleCompletionCount = 0;
        int32 HoleTransitionCount = 0;
        int32 OrderedTransitionCount = 0;
        int32 OrderedBasketCompletionCount = 0;
        bool bTransitionAwaitingHoleStart = false;
        bool bBasketAwaitingHoleCompletion = false;
        bool bCausalOrderingValid = true;
        bool bSawRelease = false;
        bool bSawAirborne = false;
        bool bSawBasket = false;
        bool bSawRoundCompletion = false;
        bool bTraceValid = PresentationAudioEventTrace.Num() <= DiscGolfPresentationAudio::MaximumTraceEvents;
        EDiscGolfPresentationAudioCategory PreviousCategory =
            EDiscGolfPresentationAudioCategory::Invalid;
        for (const FDiscGolfPresentationAudioEvent& Event : PresentationAudioEventTrace)
        {
            bTraceValid = bTraceValid && Event.IsValid() && Event.bHasContext;
            switch (Event.Category)
            {
                case EDiscGolfPresentationAudioCategory::ThrowRelease: bSawRelease = true; break;
                case EDiscGolfPresentationAudioCategory::AirborneFlight: bSawAirborne = true; break;
                case EDiscGolfPresentationAudioCategory::BasketOutcome:
                    bSawBasket = true;
                    bCausalOrderingValid = bCausalOrderingValid && !bBasketAwaitingHoleCompletion;
                    bBasketAwaitingHoleCompletion = true;
                    break;
                case EDiscGolfPresentationAudioCategory::RoundCompletion:
                    bSawRoundCompletion = true;
                    bCausalOrderingValid = bCausalOrderingValid
                        && PreviousCategory == EDiscGolfPresentationAudioCategory::HoleCompletion;
                    break;
                case EDiscGolfPresentationAudioCategory::HoleCompletion:
                    ++HoleCompletionCount;
                    bCausalOrderingValid = bCausalOrderingValid && bBasketAwaitingHoleCompletion;
                    if (bBasketAwaitingHoleCompletion) ++OrderedBasketCompletionCount;
                    bBasketAwaitingHoleCompletion = false;
                    break;
                case EDiscGolfPresentationAudioCategory::HoleTransition:
                    ++HoleTransitionCount;
                    bTransitionAwaitingHoleStart = true;
                    break;
                case EDiscGolfPresentationAudioCategory::HoleStart:
                    ++HoleStartCount;
                    if (bTransitionAwaitingHoleStart)
                    {
                        ++OrderedTransitionCount;
                        bTransitionAwaitingHoleStart = false;
                    }
                    break;
                default: break;
            }
            PreviousCategory = Event.Category;
        }
        bPassed = bTraceValid && bSawRelease && bSawAirborne && bSawBasket
            && bSawRoundCompletion && HoleStartCount >= 3 && HoleCompletionCount == 3
            && HoleTransitionCount == 2 && OrderedTransitionCount == 2
            && OrderedBasketCompletionCount == 3 && !bBasketAwaitingHoleCompletion
            && bCausalOrderingValid;
        if (!bPassed)
        {
            FinalReason = FString::Printf(
                TEXT("presentation event lifecycle invalid (trace=%d valid=%s start=%d complete=%d transition=%d orderedTransition=%d orderedBasketComplete=%d causal=%s release=%s airborne=%s basket=%s round=%s)"),
                PresentationAudioEventTrace.Num(), bTraceValid ? TEXT("yes") : TEXT("no"),
                HoleStartCount, HoleCompletionCount, HoleTransitionCount, OrderedTransitionCount,
                OrderedBasketCompletionCount, bCausalOrderingValid ? TEXT("yes") : TEXT("no"),
                bSawRelease ? TEXT("yes") : TEXT("no"), bSawAirborne ? TEXT("yes") : TEXT("no"),
                bSawBasket ? TEXT("yes") : TEXT("no"), bSawRoundCompletion ? TEXT("yes") : TEXT("no"));
        }
    }
#if DG_WITH_DEVELOPMENT_CONTENT
    if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("Session17RoundFlowGate")))
    {
        if (!bScorecardVisible)
        {
            ToggleScorecard();
        }
        ADiscGolfTourPlayerController* PlayerController =
            Cast<ADiscGolfTourPlayerController>(
                UGameplayStatics::GetPlayerController(this, 0));
        if (PlayerController)
        {
            PlayerController->RefreshRoundFlowPresentation();
        }
        const bool bResultsReachable = PlayerController
            && bScorecardVisible
            && RoundState.bRoundComplete
            && DiscGolfRound::CompletedHoleCount(RoundState) == 3
            && PlayerController->HasInteractiveRoundFlowWidget()
            && PlayerController->bShowMouseCursor
            && PlayerController->GetActiveInputRoute() == EDiscGolfInputRoute::UI;
        FString SettingsRecoveryError;
        const bool bSettingsRecovered = bResultsReachable
            && PlayerController->RunRoundFlowSettingsRecoveryProbe(
                SettingsRecoveryError);
        const bool bContinueActionAccepted = bSettingsRecovered
            && PlayerController->HandleRoundFlowAction(
                EDGRoundFlowAction::AdvanceOrRestart);
        const bool bContinueRecovered = ActiveHole
            && ActiveHole->HoleNumber == 1
            && !RoundState.bRoundComplete
            && DiscGolfRound::CompletedHoleCount(RoundState) == 0
            && !bScorecardVisible
            && CanPlayerThrow()
            && !PlayerController->HasInteractiveRoundFlowWidget()
            && !PlayerController->bShowMouseCursor
            && PlayerController->GetActiveInputRoute() == EDiscGolfInputRoute::Gameplay;
        const bool bDuplicateActionRejected = bContinueRecovered
            && !PlayerController->HandleRoundFlowAction(
                EDGRoundFlowAction::AdvanceOrRestart);
        bPassed = bResultsReachable && bSettingsRecovered
            && bContinueActionAccepted && bContinueRecovered
            && bDuplicateActionRejected;
        if (bPassed)
        {
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("DG_SESSION17_ROUND_FLOW_RESULTS: PASS widget=interactive focus=valid settings=restored continue=restart_round hole=1 playable=1 duplicate=rejected"));
        }
        else
        {
            FinalReason = FString::Printf(
                TEXT("Session 17 round-flow results recovery failed (results=%s settings=%s action=%s continue=%s duplicate=%s error=%s)"),
                bResultsReachable ? TEXT("yes") : TEXT("no"),
                bSettingsRecovered ? TEXT("yes") : TEXT("no"),
                bContinueActionAccepted ? TEXT("accepted") : TEXT("rejected"),
                bContinueRecovered ? TEXT("yes") : TEXT("no"),
                bDuplicateActionRejected ? TEXT("rejected") : TEXT("accepted"),
                SettingsRecoveryError.IsEmpty() ? TEXT("none") : *SettingsRecoveryError);
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("DG_SESSION17_ROUND_FLOW_RESULTS: FAIL reason=%s"),
                *FinalReason);
        }
    }
    if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("Session16CorePlayabilityGate")))
    {
        // Session 16 observes the real terminal round state and then exercises
        // the existing completed-round Continue action. This does not add a
        // second results or progression authority: the native scorecard and
        // AdvanceToNextHole -> RestartRound path remain the implementation.
        if (!bScorecardVisible)
        {
            ToggleScorecard();
        }
        const bool bResultsReachable = bScorecardVisible
            && RoundState.bRoundComplete
            && DiscGolfRound::CompletedHoleCount(RoundState) == 3;
        ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        const bool bContinueActionAccepted = Golfer
            && Golfer->TryAdvanceToNextHoleFromPlayerInput();
        const bool bContinueRecovered = ActiveHole
            && ActiveHole->HoleNumber == 1
            && !RoundState.bRoundComplete
            && DiscGolfRound::CompletedHoleCount(RoundState) == 0
            && !bScorecardVisible
            && CanPlayerThrow();
        bPassed = bResultsReachable && bContinueActionAccepted && bContinueRecovered;
        if (bPassed)
        {
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("DG_SESSION16_RESULTS_CONTINUE: PASS results=scorecard round_complete=1 continue=restart_round hole=1 playable=1"));
        }
        else
        {
            FinalReason = FString::Printf(
                TEXT("Session 16 results/continue recovery failed (results=%s action=%s continue=%s)"),
                bResultsReachable ? TEXT("yes") : TEXT("no"),
                bContinueActionAccepted ? TEXT("accepted") : TEXT("rejected"),
                bContinueRecovered ? TEXT("yes") : TEXT("no"));
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("DG_SESSION16_RESULTS_CONTINUE: FAIL reason=%s"), *FinalReason);
        }
    }
    bThreeHoleRoundSmokeTestActive = false;
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("THREE HOLE ROUND SMOKE PASS: 3/3 holes, 3 strokes, 11 par, -8 round; manifest transitions, scoring, scorecard state, and save snapshot active."));
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error, TEXT("THREE HOLE ROUND SMOKE FAIL: %s"), *FinalReason);
    }
    if (bPassed && FParse::Param(FCommandLine::Get(), TEXT("CaptureRoundComplete")))
    {
        bScorecardVisible = true;
        VisualQAScreenshotPath = FPaths::Combine(FPaths::ScreenShotDir(), TEXT("RoundCompleteVisualQA_v05.png"));
        FTimerHandle CaptureTimer;
        GetWorldTimerManager().SetTimer(CaptureTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::CaptureVisualQAScreenshot), 0.25f, false);
        FTimerHandle FinishTimer;
        GetWorldTimerManager().SetTimer(FinishTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishVisualQAScreenshot), 1.5f, false);
        return;
    }
#endif
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::InitializeRuntimeCheckpointJournal()
{
    using namespace DiscGolfRuntimeCheckpointJournal;

    FCaptureBinding Binding;
    Binding.CandidateId = FPlatformMisc::GetEnvironmentVariable(
        TEXT("DGT_S19_CAPTURE_CANDIDATE_ID"));
    Binding.UserDirToken = FPlatformMisc::GetEnvironmentVariable(
        TEXT("DGT_S19_CAPTURE_USERDIR_TOKEN"));
    Binding.ExecutableSha256 = FPlatformMisc::GetEnvironmentVariable(
        TEXT("DGT_S19_CAPTURE_EXE_SHA256"));
    Binding.ArchiveManifestSha256 = FPlatformMisc::GetEnvironmentVariable(
        TEXT("DGT_S19_CAPTURE_ARCHIVE_MANIFEST_SHA256"));
    Binding.CaptureNonce = FPlatformMisc::GetEnvironmentVariable(
        TEXT("DGT_S19_CAPTURE_NONCE"));

    const bool bAnyBindingValue = !Binding.CandidateId.IsEmpty()
        || !Binding.UserDirToken.IsEmpty()
        || !Binding.ExecutableSha256.IsEmpty()
        || !Binding.ArchiveManifestSha256.IsEmpty()
        || !Binding.CaptureNonce.IsEmpty();
    if (!bAnyBindingValue)
    {
        return;
    }
    bRuntimeCheckpointJournalRequested = true;

    const FString LowerCommandLine = FString(FCommandLine::Get()).ToLower();
    if (FApp::IsUnattended()
        || LowerCommandLine.Contains(TEXT("smoketest"))
        || LowerCommandLine.Contains(TEXT("automation"))
        || LowerCommandLine.Contains(TEXT("-testexit")))
    {
        AbortRuntimeCheckpointJournal(
            TEXT("unattended, smoke, and automation launches cannot emit gameplay evidence"));
        return;
    }

    FString BindingError;
    const FString RedirectedUserDir = FPaths::ProjectUserDir();
    if (!TryValidateCaptureBinding(Binding, RedirectedUserDir, BindingError))
    {
        AbortRuntimeCheckpointJournal(BindingError);
        return;
    }

    RuntimeCheckpointRoundId = NewCanonicalUuidV4();
    RuntimeCheckpointStartedSeconds = FPlatformTime::Seconds();
    RuntimeCheckpointLastMonotonicMs = 0;
    RuntimeCheckpointNextSequence = 1;
    RuntimeCheckpointFirstLieMask = 0;
    RuntimeCheckpointCaptureBinding = Binding;

    const FString JournalPath = FPaths::Combine(
        RedirectedUserDir, UserDirRelativePath);
    const FString JournalDirectory = FPaths::GetPath(JournalPath);
    IFileManager& FileManager = IFileManager::Get();
    if (!FileManager.MakeDirectory(*JournalDirectory, true))
    {
        AbortRuntimeCheckpointJournal(
            TEXT("journal directory could not be created"));
        return;
    }
    FString CreateError;
    if (!TryCreateExclusiveJournalWriter(
            JournalPath, RuntimeCheckpointJournalWriter, CreateError))
    {
        AbortRuntimeCheckpointJournal(CreateError);
        return;
    }

    FString HeaderLine;
    FString HeaderError;
    if (!TrySerializeHeaderLine(
            Binding,
            RuntimeCheckpointRoundId,
            FDateTime::UtcNow().ToIso8601(),
            HeaderLine,
            HeaderError))
    {
        AbortRuntimeCheckpointJournal(HeaderError);
        return;
    }
    FTCHARToUTF8 EncodedHeader(*HeaderLine);
    RuntimeCheckpointJournalWriter->Serialize(
        const_cast<ANSICHAR*>(EncodedHeader.Get()), EncodedHeader.Length());
    RuntimeCheckpointJournalWriter->Flush();
    if (RuntimeCheckpointJournalWriter->IsError())
    {
        AbortRuntimeCheckpointJournal(TEXT("journal header write failed"));
        return;
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Runtime checkpoint journal armed for candidate %s and round %s."),
        *Binding.CandidateId, *RuntimeCheckpointRoundId);
}

void ADiscGolfTourGameMode::BeginRuntimeCheckpointRound()
{
    if (!RuntimeCheckpointJournalWriter || bRuntimeCheckpointJournalTerminal
        || bRuntimeCheckpointRoundStarted)
    {
        return;
    }
    if (!ActiveHole || ActiveHole->CourseId != TEXT("PineRidgeChampionship")
        || RoundState.CurrentHoleIndex != 0 || RoundState.HoleScores.Num() != 3
        || RoundState.bRoundComplete
        || RoundState.HoleScores[0].HoleNumber != 1 || RoundState.HoleScores[0].Par != 3
        || RoundState.HoleScores[1].HoleNumber != 2 || RoundState.HoleScores[1].Par != 4
        || RoundState.HoleScores[2].HoleNumber != 3 || RoundState.HoleScores[2].Par != 4
        || DiscGolfRound::CompletedHoleCount(RoundState) != 0
        || DiscGolfRound::TotalStrokes(RoundState) != 0
        || DiscGolfRound::TotalPenaltyStrokes(RoundState) != 0)
    {
        AbortRuntimeCheckpointJournal(
            TEXT("initial authored three-hole round state is invalid"));
        return;
    }
    bRuntimeCheckpointRoundStarted = WriteRuntimeCheckpointEvent(
        TEXT("ROUND_STARTED"), TOptional<int32>(), false);
}

void ADiscGolfTourGameMode::GetRuntimeCheckpointCumulativeScore(
    int32& OutStrokes,
    int32& OutPenalties) const
{
    OutStrokes = DiscGolfRound::TotalStrokes(RoundState);
    OutPenalties = DiscGolfRound::TotalPenaltyStrokes(RoundState);
    if (RoundState.HoleScores.IsValidIndex(RoundState.CurrentHoleIndex)
        && !RoundState.HoleScores[RoundState.CurrentHoleIndex].bCompleted)
    {
        OutStrokes += Strokes;
        OutPenalties += PenaltyStrokes;
    }
}

bool ADiscGolfTourGameMode::WriteRuntimeCheckpointEvent(
    const FString& EventName,
    const TOptional<int32>& HoleNumber,
    bool bIncludeFinalScore)
{
    using namespace DiscGolfRuntimeCheckpointJournal;

    static const TCHAR* ExpectedNames[] = {
        TEXT("ROUND_STARTED"),
        TEXT("HOLE_1_TEE"), TEXT("HOLE_1_LIE"), TEXT("HOLE_1_COMPLETED"),
        TEXT("HOLE_2_TEE"), TEXT("HOLE_2_LIE"), TEXT("HOLE_2_COMPLETED"),
        TEXT("HOLE_3_TEE"), TEXT("HOLE_3_LIE"), TEXT("HOLE_3_COMPLETED"),
        TEXT("ROUND_COMPLETED"), TEXT("FINAL_SCORECARD"),
    };
    static const int32 ExpectedHoles[] = {
        0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 0, 0,
    };
    static const int32 ExpectedCompletedHoles[] = {
        0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3,
    };

    if (!RuntimeCheckpointJournalWriter || bRuntimeCheckpointJournalTerminal)
    {
        return false;
    }
    const int32 EventIndex = RuntimeCheckpointNextSequence - 1;
    const int32 ActualCompletedHoles = DiscGolfRound::CompletedHoleCount(RoundState);
    const bool bExpectedHoleSet = EventIndex >= 0
        && EventIndex < UE_ARRAY_COUNT(ExpectedHoles)
        && ExpectedHoles[EventIndex] != 0;
    if (EventIndex < 0 || EventIndex >= UE_ARRAY_COUNT(ExpectedNames)
        || EventName != ExpectedNames[EventIndex]
        || HoleNumber.IsSet() != bExpectedHoleSet
        || (bExpectedHoleSet && HoleNumber.GetValue() != ExpectedHoles[EventIndex])
        || ActualCompletedHoles != ExpectedCompletedHoles[EventIndex]
        || bIncludeFinalScore != (EventName == TEXT("FINAL_SCORECARD")))
    {
        AbortRuntimeCheckpointJournal(
            TEXT("runtime gameplay checkpoint sequence diverged"));
        return false;
    }

    int32 TotalStrokes = 0;
    int32 TotalPenalties = 0;
    GetRuntimeCheckpointCumulativeScore(TotalStrokes, TotalPenalties);
    const int64 ElapsedMs = FMath::Max<int64>(
        RuntimeCheckpointLastMonotonicMs + 1,
        FMath::Max<int64>(0, static_cast<int64>(
            (FPlatformTime::Seconds() - RuntimeCheckpointStartedSeconds) * 1000.0)));

    FCheckpointEvent Event;
    Event.Sequence = RuntimeCheckpointNextSequence;
    Event.MonotonicMs = ElapsedMs;
    Event.Event = EventName;
    Event.RoundId = RuntimeCheckpointRoundId;
    Event.HoleNumber = HoleNumber;
    Event.CompletedHoles = ActualCompletedHoles;
    Event.TotalStrokes = TotalStrokes;
    Event.TotalPenalties = TotalPenalties;
    if (bIncludeFinalScore)
    {
        for (const FDiscGolfRoundHoleScore& Score : RoundState.HoleScores)
        {
            Event.FinalScoreRows.Add({
                Score.HoleNumber,
                Score.Par,
                Score.Strokes,
                Score.PenaltyStrokes,
            });
        }
    }

    FString Line;
    FString SerializationError;
    if (!TrySerializeEventLine(Event, Line, SerializationError))
    {
        AbortRuntimeCheckpointJournal(SerializationError);
        return false;
    }
    FTCHARToUTF8 EncodedLine(*Line);
    RuntimeCheckpointJournalWriter->Serialize(
        const_cast<ANSICHAR*>(EncodedLine.Get()), EncodedLine.Length());
    RuntimeCheckpointJournalWriter->Flush();
    if (RuntimeCheckpointJournalWriter->IsError())
    {
        AbortRuntimeCheckpointJournal(TEXT("runtime checkpoint append failed"));
        return false;
    }

    RuntimeCheckpointLastMonotonicMs = ElapsedMs;
    ++RuntimeCheckpointNextSequence;
    if (bIncludeFinalScore)
    {
        if (!RuntimeCheckpointJournalWriter->Close()
            || RuntimeCheckpointJournalWriter->IsError())
        {
            AbortRuntimeCheckpointJournal(
                TEXT("runtime checkpoint final close failed"));
            return false;
        }
        bRuntimeCheckpointJournalTerminal = true;
        RuntimeCheckpointJournalWriter.Reset();
    }
    return true;
}

void ADiscGolfTourGameMode::RecordRuntimeCheckpointTee()
{
    if (!RuntimeCheckpointJournalWriter || !bRuntimeCheckpointRoundStarted
        || bRuntimeCheckpointJournalTerminal || !ActiveHole)
    {
        return;
    }
    WriteRuntimeCheckpointEvent(
        FString::Printf(TEXT("HOLE_%d_TEE"), ActiveHole->HoleNumber),
        TOptional<int32>(ActiveHole->HoleNumber),
        false);
}

void ADiscGolfTourGameMode::RecordRuntimeCheckpointFirstLie()
{
    if (!RuntimeCheckpointJournalWriter || !bRuntimeCheckpointRoundStarted
        || bRuntimeCheckpointJournalTerminal || !ActiveHole)
    {
        return;
    }
    const int32 HoleNumber = ActiveHole->HoleNumber;
    if (HoleNumber < 1 || HoleNumber > 3)
    {
        AbortRuntimeCheckpointJournal(TEXT("lie checkpoint hole is out of range"));
        return;
    }
    const int32 HoleBit = 1 << (HoleNumber - 1);
    if ((RuntimeCheckpointFirstLieMask & HoleBit) != 0)
    {
        return;
    }
    if (WriteRuntimeCheckpointEvent(
            FString::Printf(TEXT("HOLE_%d_LIE"), HoleNumber),
            TOptional<int32>(HoleNumber),
            false))
    {
        RuntimeCheckpointFirstLieMask |= HoleBit;
    }
}

void ADiscGolfTourGameMode::RecordRuntimeCheckpointHoleCompleted()
{
    if (!RuntimeCheckpointJournalWriter || !bRuntimeCheckpointRoundStarted
        || bRuntimeCheckpointJournalTerminal || !ActiveHole)
    {
        return;
    }
    const int32 HoleNumber = ActiveHole->HoleNumber;
    if (!WriteRuntimeCheckpointEvent(
            FString::Printf(TEXT("HOLE_%d_COMPLETED"), HoleNumber),
            TOptional<int32>(HoleNumber),
            false))
    {
        return;
    }
    if (RoundState.bRoundComplete)
    {
        WriteRuntimeCheckpointEvent(
            TEXT("ROUND_COMPLETED"), TOptional<int32>(), false);
    }
}

void ADiscGolfTourGameMode::RecordRuntimeCheckpointFinalScorecard()
{
    if (!RuntimeCheckpointJournalWriter || !bRuntimeCheckpointRoundStarted
        || bRuntimeCheckpointJournalTerminal || !RoundState.bRoundComplete)
    {
        return;
    }
    WriteRuntimeCheckpointEvent(
        TEXT("FINAL_SCORECARD"), TOptional<int32>(), true);
}

void ADiscGolfTourGameMode::AbortRuntimeCheckpointJournal(const FString& Reason)
{
    if (RuntimeCheckpointJournalWriter)
    {
        RuntimeCheckpointJournalWriter->Flush();
        RuntimeCheckpointJournalWriter->Close();
        RuntimeCheckpointJournalWriter.Reset();
    }
    bRuntimeCheckpointJournalTerminal = true;
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("Runtime checkpoint journal failed closed: %s"), *Reason);
}

void ADiscGolfTourGameMode::StartHole()
{
    if (!ActiveHole) return;

    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->StopMonitoring(TEXT("AtTee"));
    }

    EnsurePlayerPawn();
    Strokes = 0;
    PenaltyStrokes = 0;
    LastThrowShotNumber = 0;
    bHoleComplete = false;
    bHasLastRelease = false;
    LastReleaseWorldSeconds = -1.0f;
    bHasLastFlightTelemetry = false;
    bLieTransitionActive = false;
    LastShotPresentationResult = FDiscGolfShotPresentationResult();
    LastShotResultWorldSeconds = -1.0f;
    CurrentLieType = ELieType::Tee;
    CurrentLieLocation = ActiveHole->TeeLocation;
    ThrowStartLieLocation = CurrentLieLocation;
    CurrentLieState = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::TeePad,
        CurrentLieLocation,
        CurrentLieLocation,
        ActiveHole->BasketLocation,
        true);
    LieRulesStatusText = TEXT("TEE PAD - CLEAN STANCE");

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer)
    {
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ADiscGolfTourGameMode::StartHole));
        return;
    }

    Golfer->SetActorLocation(CurrentLieLocation + FVector(0, 0, 88.0f));
    Golfer->FaceLocation(ActiveHole->BasketLocation);
    ApplyShotContextToGolfer();
    ReturnCameraToPlayer();
    const bool bExplicitIntro = FParse::Param(FCommandLine::Get(), TEXT("ShowHoleIntro"));
    const bool bSkipIntro = FParse::Param(FCommandLine::Get(), TEXT("SkipHoleIntro"));
    bHoleIntroActive = ActiveHole->IsAuthoredBlockout() && !bSkipIntro
        && (bExplicitIntro || !FApp::IsUnattended());
    if (bHoleIntroActive)
    {
        HoleIntroStartWorldSeconds = GetWorld()->GetTimeSeconds();
        FTimerHandle IntroTimer;
        GetWorldTimerManager().SetTimer(IntroTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishHoleIntroduction), HoleIntroDurationSeconds, false);
    }
    const int32 TotalHoleCount = HasAuthoredRound() ? RoundState.HoleScores.Num() : 1;
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveHoleStart(
        ActiveHole->HoleNumber, TotalHoleCount, ActiveHole->Par), ActiveHole->TeeLocation);
    RecordRuntimeCheckpointTee();
}

void ADiscGolfTourGameMode::FinishHoleIntroduction()
{
    bHoleIntroActive = false;
    HoleIntroStartWorldSeconds = -1.0f;
}

bool ADiscGolfTourGameMode::IsHoleIntroVisible() const
{
    return bHoleIntroActive && ActiveHole != nullptr;
}

float ADiscGolfTourGameMode::GetHoleIntroProgress01() const
{
    if (!IsHoleIntroVisible() || !GetWorld() || HoleIntroStartWorldSeconds < 0.0f) return 1.0f;
    return FMath::Clamp((GetWorld()->GetTimeSeconds() - HoleIntroStartWorldSeconds)
        / FMath::Max(HoleIntroDurationSeconds, 0.1f), 0.0f, 1.0f);
}

float ADiscGolfTourGameMode::GetLastShotResultAgeSeconds() const
{
    if (!LastShotPresentationResult.bValid || !GetWorld() || LastShotResultWorldSeconds < 0.0f)
    {
        return TNumericLimits<float>::Max();
    }
    return FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - LastShotResultWorldSeconds);
}

FString ADiscGolfTourGameMode::GetHoleCompletionLabel() const
{
    return ActiveHole && bHoleComplete
        ? DiscGolfPlayerExperience::HoleScoreName(Strokes, ActiveHole->Par)
        : FString();
}

FDiscGolfPlayerSettings ADiscGolfTourGameMode::GetPlayerSettings() const
{
    const UDiscGolfTourGameInstance* Instance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    return Instance ? Instance->GetPlayerSettings() : FDiscGolfPlayerSettings();
}

bool ADiscGolfTourGameMode::IsBasketDirectlyVisible() const
{
    return bBasketDirectlyVisibleCached;
}

void ADiscGolfTourGameMode::RefreshBasketVisibility()
{
    if (!ActiveHole || !GetWorld())
    {
        bBasketDirectlyVisibleCached = false;
        return;
    }
    const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC)
    {
        bBasketDirectlyVisibleCached = false;
        return;
    }
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGolfBasketVisibility), false);
    if (const APawn* Pawn = PC->GetPawn()) Params.AddIgnoredActor(Pawn);
    if (ActiveDisc) Params.AddIgnoredActor(ActiveDisc);
    FHitResult Hit;
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit, ViewLocation, ActiveHole->BasketLocation + FVector(0.0f, 0.0f, 110.0f),
        ECC_Visibility, Params);
    bBasketDirectlyVisibleCached = !bBlocked || Hit.GetActor() == ActiveHole->Basket.Get();
}

EDiscShotContext ADiscGolfTourGameMode::GetCurrentShotContext() const
{
    return CurrentLieState.ShotContext;
}

float ADiscGolfTourGameMode::GetBasketDistanceMeters() const
{
    if (!ActiveHole) return 0.0f;
    return FVector::Dist2D(CurrentLieLocation, ActiveHole->BasketLocation) / 100.0f;
}

float ADiscGolfTourGameMode::GetLastReleaseAgeSeconds() const
{
    if (!bHasLastRelease || LastReleaseWorldSeconds < 0.0f || !GetWorld())
    {
        return TNumericLimits<float>::Max();
    }
    return FMath::Max(0.0f, GetWorld()->GetTimeSeconds() - LastReleaseWorldSeconds);
}

bool ADiscGolfTourGameMode::TryGetAuthoritativeHudWindSample(
    const FVector& PreThrowSampleLocationCm,
    FVector& OutSampleLocationCm,
    FVector& OutWindMps,
    FString& OutError) const
{
    OutSampleLocationCm = PreThrowSampleLocationCm;
    OutWindMps = FVector::ZeroVector;

    if (const ADiscActor* Disc = ActiveDisc)
    {
        const UDiscFlightComponent* Flight = Disc->GetFlightComponent();
        if (Flight && Flight->IsFlying())
        {
            const TArray<FDiscTrajectorySample>& Samples = Flight->GetTrajectorySamples();
            if (Samples.IsEmpty())
            {
                OutError = TEXT("Active flight has no authoritative wind sample");
                return false;
            }
            const FDiscTrajectorySample& Sample = Samples.Last();
            const bool bWindFinite = FMath::IsFinite(Sample.WindMps.X)
                && FMath::IsFinite(Sample.WindMps.Y)
                && FMath::IsFinite(Sample.WindMps.Z);
            if (!bWindFinite)
            {
                OutError = TEXT("Active flight's authoritative wind sample is non-finite");
                return false;
            }
            OutSampleLocationCm = Sample.WorldLocationCm;
            OutWindMps = Sample.WindMps;
            OutError.Reset();
            return true;
        }
    }

    if (PresentationShotSequence < 0 || PresentationShotSequence >= MAX_int32)
    {
        OutError = TEXT("Next accepted-shot wind sequence is exhausted");
        return false;
    }
    float PhaseOriginSeconds = 0.0f;
    if (!TrySampleWindForAcceptedShot(
        WindDirector, PresentationShotSequence + 1,
        PreThrowSampleLocationCm, PhaseOriginSeconds, OutWindMps, OutError))
    {
        OutWindMps = FVector::ZeroVector;
        return false;
    }
    OutError.Reset();
    return true;
}

bool ADiscGolfTourGameMode::TrySampleWindForAcceptedShot(
    const AWindDirector* InWindDirector,
    int32 AcceptedShotSequence,
    const FVector& SampleLocationCm,
    float& OutPhaseOriginSeconds,
    FVector& OutWindMps,
    FString& OutError)
{
    OutPhaseOriginSeconds = 0.0f;
    OutWindMps = FVector::ZeroVector;
    return AWindDirector::TryBuildDeterministicShotPhaseOrigin(
            AcceptedShotSequence, OutPhaseOriginSeconds, OutError)
        && UDiscFlightComponent::TrySampleWindForFixedStep(
            InWindDirector, SampleLocationCm, OutPhaseOriginSeconds,
            OutWindMps, OutError);
}

float ADiscGolfTourGameMode::GetAimErrorAtBasketCm() const
{
    if (!ActiveHole || !GetWorld()) return 0.0f;
    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer) return 0.0f;
    const FVector ToBasket = ActiveHole->BasketLocation - Golfer->GetActorLocation();
    const float ErrorDeg = DiscGolfMath::SignedAimErrorDeg(Golfer->GetActorForwardVector(), ToBasket);
    return FMath::Tan(FMath::DegreesToRadians(ErrorDeg)) * GetBasketDistanceMeters() * 100.0f;
}

void ADiscGolfTourGameMode::ApplyShotContextToGolfer()
{
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer || !Golfer->GetThrowController()) return;
    Golfer->GetThrowController()->SetShotContext(GetCurrentShotContext(), GetBasketDistanceMeters());
    Golfer->SetPresentationShotContext(GetCurrentShotContext(), GetBasketDistanceMeters());
}

bool ADiscGolfTourGameMode::RequestThrow(const FThrowCommand& Command)
{
    if (!CanPlayerThrow()) return false;
    return LaunchThrow(Command);
}

bool ADiscGolfTourGameMode::RequestThrowFromGrip(
    const FThrowCommand& Command,
    const FTransform& GripWorldTransform)
{
    // This C++-only seam may bypass CanPlayerThrow solely because the adapter's
    // committed release keeps that normal gate closed until the montage recovers.
    // Every other lifecycle authority remains mandatory.
    const ADiscGolfTourPlayerController* PlayerController =
        Cast<ADiscGolfTourPlayerController>(
            UGameplayStatics::GetPlayerController(this, 0));
    const bool bBaseLaunchAllowed = DiscGolfGameplayGate::CanLaunchThrow(
        ActiveHole != nullptr, ActiveDisc != nullptr, ReplayActor != nullptr,
        IsCourseFlyoverActive(), bHoleComplete, bScorecardVisible, bHoleIntroActive);
    if (!DiscGolfGameplayGate::CanCommitPlayerRelease(
            bBaseLaunchAllowed,
            bMainMenuVisible,
            bRegressionActive,
            bLieTransitionActive,
            PlayerController != nullptr,
            PlayerController && PlayerController->IsPresentationDismissReleasePending(),
            PlayerController && PlayerController->IsFreshThrowDownRequiredAfterPresentation(),
            PlayerController && PlayerController->IsControlsMenuOpen(),
            PlayerController && PlayerController->IsCharacterCreatorOpen(),
            GetWorld() && GetWorld()->IsPaused()))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Character release rejected because the committed player lifecycle gate is closed."));
        return false;
    }

    if (GripWorldTransform.ContainsNaN())
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Character release rejected because disc_grip_r produced a non-finite transform."));
        return false;
    }

    const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    const UDiscGolfRHBHThrowAdapterComponent* Adapter = Golfer
        ? Golfer->GetRHBHThrowAdapter() : nullptr;
    const FVector GripLocation = GripWorldTransform.GetLocation();
    if (!Golfer || !Adapter
        || !Adapter->IsThrowActive()
        || !Adapter->HasCommittedRelease()
        || Adapter->GetReleaseCommitCountForAttempt() != 1)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Character release rejected because no committed Session 3 player transaction owns it."));
        return false;
    }

    const FThrowCommand CachedCommand = Adapter->GetLastAuthoritativeCommand();
    const bool bMatchesCommittedCommand = CachedCommand.DiscInstanceId == Command.DiscInstanceId
        && CachedCommand.MoldId == Command.MoldId
        && CachedCommand.Plastic == Command.Plastic
        && CachedCommand.ThrowStyle == Command.ThrowStyle
        && CachedCommand.Handedness == Command.Handedness
        && CachedCommand.ShotContext == Command.ShotContext
        && CachedCommand.Direction == Command.Direction
        && CachedCommand.Power01 == Command.Power01
        && CachedCommand.HyzerDeg == Command.HyzerDeg
        && CachedCommand.NoseAngleDeg == Command.NoseAngleDeg
        && CachedCommand.LaunchAngleDeg == Command.LaunchAngleDeg
        && CachedCommand.TimingError == Command.TimingError;
    if (!bMatchesCommittedCommand)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Character release rejected because its command does not match the committed transaction."));
        return false;
    }

    if (FVector::DistSquared(GripLocation, Golfer->GetActorLocation())
        > FMath::Square(MaximumAnimatedGripOriginDistanceCm))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Character release rejected because disc_grip_r was implausibly far from the player."));
        return false;
    }

    // The exact adapter transaction is now Released, so CanPlayerThrow would
    // intentionally reject it. LaunchThrow retains the shared disc lifecycle gate.
    return LaunchThrow(Command, &GripLocation);
}

bool ADiscGolfTourGameMode::LaunchThrow(
    const FThrowCommand& Command,
    const FVector* ReleaseLocationOverrideCm,
    bool bTrustedRegressionCatalogCommand)
{
    if (!DiscGolfMath::IsThrowCommandValid(Command))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because the command violates the authoritative input contract."));
        return false;
    }

    // This private path is shared by accepted player throws and trusted regression
    // presets. Fundamental lifecycle guards still prevent replacing an active disc.
    if (!DiscGolfGameplayGate::CanLaunchThrow(
        ActiveHole != nullptr, ActiveDisc != nullptr, ReplayActor != nullptr,
        IsCourseFlyoverActive(), bHoleComplete, bScorecardVisible, bHoleIntroActive))
    {
        return false;
    }

    UDiscCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDiscCatalogSubsystem>() : nullptr;
    if (!Catalog) return false;

    FResolvedDiscDefinition ResolvedDisc;
    if (!Catalog->ResolveDisc(Command.MoldId, Command.Plastic, ResolvedDisc)) return false;

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer) return false;

    if (bTrustedRegressionCatalogCommand && !bRegressionActive)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because trusted catalog ingress is restricted to active regression."));
        return false;
    }
    // Regression presets remain catalog-only so accepted reference envelopes do
    // not inherit mutable player equipment. A real player command instead carries
    // the exact stable instance captured before the release transaction began.
    if (!bTrustedRegressionCatalogCommand)
    {
        UDiscBagComponent* Bag = Golfer->GetDiscBag();
        const UDiscGolfCharacterProfile* Profile = Golfer->GetRuntimeCharacterProfile();
        if (!Bag || !Profile) return false;

        FDGDiscInstance Instance;
        if (!Bag->GetSelectedDiscInstance(Instance)
            || !IsPlayerThrowProvenanceValid(
                Command, Profile->Handedness, GetCurrentShotContext(), Instance))
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Throw rejected because equipment, handedness, or shot-context provenance changed before release."));
            return false;
        }

        FString EquipmentError;
        FResolvedDiscDefinition InstanceResolved;
        if (!UDiscBagComponent::ResolveDiscInstance(
            Instance, ResolvedDisc, InstanceResolved, EquipmentError))
        {
            UE_LOG(LogDiscGolfTour, Error, TEXT("Throw equipment resolution failed: %s"), *EquipmentError);
            return false;
        }
        ResolvedDisc = MoveTemp(InstanceResolved);
    }

    if (!GetWorld()) return false;
    const FVector SpawnLocation = ReleaseLocationOverrideCm
        ? *ReleaseLocationOverrideCm
        : Golfer->GetActorLocation() + Golfer->GetActorForwardVector() * 70.0f + FVector(0, 0, 35.0f);
    const FTransform SpawnTransform(
        Golfer->GetActorQuat(), SpawnLocation, FVector::OneVector);
    if (!UDiscFlightComponent::IsSpawnTransformValidForLaunch(SpawnTransform))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because the authoritative spawn transform is invalid."));
        return false;
    }

    if (PresentationShotSequence < 0 || PresentationShotSequence >= MAX_int32)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because the accepted-shot wind sequence is exhausted."));
        return false;
    }
    const int32 AcceptedShotSequence = PresentationShotSequence + 1;
    float WindPhaseOriginSeconds = 0.0f;
    FString WindError;
    FVector InitialWindMps = FVector::ZeroVector;
    if (!TrySampleWindForAcceptedShot(
        WindDirector, AcceptedShotSequence, SpawnLocation,
        WindPhaseOriginSeconds, InitialWindMps, WindError))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected before disc spawn because the physics wind contract failed: %s"),
            *WindError);
        return false;
    }

    const FVector CandidateThrowStartLieLocation = CurrentLieLocation;
    const bool bCandidateRouteTelemetryShotPending =
        bRouteTelemetryActive && ActiveHole && ActiveHole->HoleNumber == 2
        && Strokes == 0 && CurrentLieType == ELieType::Tee
        && !RouteTelemetrySession.ActiveRouteId.IsNone()
        && DiscGolfRouteTelemetry::CountAttempts(
            RouteTelemetrySession, RouteTelemetrySession.ActiveRouteId)
            < RouteTelemetrySession.TargetAttemptsPerRoute;
    FThrowCommand AuthoredCommand = DiscGolfCourseRules::ApplyLieEffects(
        Command, CurrentLieState.Effects);
    AuthoredCommand.ShotContext = GetCurrentShotContext();
    FThrowRelease CandidateRelease = DiscGolfMath::ResolveThrowRelease(AuthoredCommand);
    CandidateRelease.LiePowerMultiplier = CurrentLieState.Effects.PowerMultiplier;
    CandidateRelease.LieTimingErrorMultiplier = CurrentLieState.Effects.TimingErrorMultiplier;
    CandidateRelease.WindPhaseOriginSeconds = WindPhaseOriginSeconds;
    FString AimRebaseError;
    // CurrentLieLocation is fixed before the montage begins and is the stable
    // pre-animation planar aim origin. MovePlayerToLie's +88 cm pawn Z placement
    // is irrelevant because aim distance and release rebasing are XY-only.
    const FVector AimLineOriginCm = CandidateThrowStartLieLocation;
    const float AimReferenceDistanceCm = FVector::Dist2D(
        AimLineOriginCm, ActiveHole->BasketLocation);
    if (!TryPrepareReleaseForOriginOverride(
        ReleaseLocationOverrideCm,
        AimLineOriginCm,
        AimReferenceDistanceCm,
        CandidateRelease,
        AimRebaseError))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected before disc spawn because the grip-origin aim rebase failed: %s"),
            *AimRebaseError);
        return false;
    }
    FString ReleaseError;
    if (!DiscGolfMath::IsThrowReleaseValid(CandidateRelease, &ReleaseError))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected before disc spawn because the resolved release is invalid: %s"),
            *ReleaseError);
        return false;
    }

    ActiveDisc = GetWorld()->SpawnActor<ADiscActor>(SpawnLocation, Golfer->GetActorRotation());
    if (!ActiveDisc) return false;

    if (!ActiveDisc->InitializeDisc(ResolvedDisc, WindDirector))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because disc initialization failed after preflight."));
        ActiveDisc->Destroy();
        ActiveDisc = nullptr;
        return false;
    }
    if (!ActiveDisc->ConfigureLaunchingActorCollisionExclusion(Golfer))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because the launching golfer collision exclusion could not be configured."));
        ActiveDisc->Destroy();
        ActiveDisc = nullptr;
        return false;
    }
    ActiveDisc->OnDiscSettled.AddDynamic(this, &ADiscGolfTourGameMode::HandleDiscSettled);
    ActiveDisc->OnDiscHoledOut.AddDynamic(this, &ADiscGolfTourGameMode::HandleDiscHoledOut);

    if (!ActiveDisc->Throw(CandidateRelease)
        || !ActiveDisc->GetFlightComponent()->IsFlying())
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Throw rejected because the disc did not enter authoritative flight."));
        ActiveDisc->Destroy();
        ActiveDisc = nullptr;
        return false;
    }

    // Commit score, telemetry, presentation, and accepted-shot provenance only
    // after the component has accepted the complete launch transaction.
    ThrowStartLieLocation = CandidateThrowStartLieLocation;
    bRouteTelemetryShotPending = bCandidateRouteTelemetryShotPending;
    LastRelease = MoveTemp(CandidateRelease);
    bHasLastRelease = true;
    LastReleaseWorldSeconds = GetWorld()->GetTimeSeconds();
    bHasLastFlightTelemetry = false;
    LastPresentationGroundTransitionCount = 0;
    PresentationShotSequence = AcceptedShotSequence;
    ++Strokes;
    LastThrowShotNumber = Strokes;
    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->StartMonitoring(
            TEXT("DiscInFlight"),
            DiscGolfMath::EffectiveFlightTimeoutSeconds(
                LastRelease.ShotContext, ResolvedDisc.Speed,
                ActiveDisc->GetFlightComponent()->MaxFlightSeconds) + 1.0f);
    }
    Golfer->NotifyAuthoritativeRelease(LastRelease);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveThrowRelease(
        LastRelease.Grade, LastRelease.Timing, LastRelease.Quality01, LastRelease.ReleaseSpeedMps),
        SpawnLocation);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveAirborneFlight(
        LastRelease.ReleaseSpeedMps, LastRelease.SpinRpm, 0.0f), SpawnLocation);
    StartBroadcastCameraForShot(ActiveDisc, SpawnLocation, AuthoredCommand.ShotContext);

    // A short tap-in can spawn already overlapping the chain volume. Resolve
    // that contact now, at solver time zero and only after score/release state
    // is committed. Deferring to the next render tick would allow 30/60/120 FPS
    // launches to advance different numbers of fixed steps before evaluation.
    ADiscActor* const LaunchedDisc = ActiveDisc;
    if (ActiveHole && ActiveHole->Basket)
    {
        ActiveHole->Basket->EvaluateOverlappingDiscContact(LaunchedDisc);
    }
    return true;
}

void ADiscGolfTourGameMode::HandleDiscSettled(ADiscActor* Disc, FVector FinalLocation)
{
    if (Disc != ActiveDisc) return;

    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->MarkProgress(TEXT("DiscResolved"));
    }

    UpdatePresentationAudioEvents();
    LastFlightTelemetry = Disc->GetFlightComponent()->GetTelemetry();
    bHasLastFlightTelemetry = true;
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveBasketOutcome(
        LastFlightTelemetry.LastBasketContact, LastFlightTelemetry.SpeedMps),
        Disc->GetActorLocation(), LastFlightTelemetry.FlightTimeSeconds, false,
        LastFlightTelemetry.CourseSurface, LastFlightTelemetry.GroundState,
        LastFlightTelemetry.LastBasketContact);
    ResolveSettledLie(Disc, FinalLocation);
    RecordRuntimeCheckpointFirstLie();
    CaptureRouteTelemetryAttempt(Disc, false);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolvePenalty(
        CurrentLieState.PenaltyType, CurrentLieState.PenaltyStrokes),
        CurrentLieLocation, LastFlightTelemetry.FlightTimeSeconds, false,
        CurrentLieState.SurfaceAtRest, LastFlightTelemetry.GroundState,
        LastFlightTelemetry.LastBasketContact, CurrentLieState.PenaltyType);
    LastShotPresentationResult.bValid = true;
    LastShotPresentationResult.Landing = DiscGolfPlayerExperience::ClassifyLanding(CurrentLieState, false);
    LastShotPresentationResult.CarryMeters = LastFlightTelemetry.CarryMeters;
    LastShotPresentationResult.TotalMeters = FVector::Dist2D(ThrowStartLieLocation, FinalLocation) / 100.0f;
    LastShotPresentationResult.RemainingMeters = CurrentLieState.DistanceToBasketMeters;
    LastShotPresentationResult.Penalty = CurrentLieState.PenaltyType;
    LastShotPresentationResult.bHoledOut = false;
    LastShotResultWorldSeconds = GetWorld()->GetTimeSeconds();
    CompleteDiscCapture(Disc, false);
    MovePlayerToLie(CurrentLieLocation);
    StopBroadcastCamera(true);
    bLieTransitionActive = !FApp::IsUnattended();
    if (bLieTransitionActive)
    {
        FTimerHandle LieTransitionTimer;
        GetWorldTimerManager().SetTimer(LieTransitionTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::FinishLieTransition), 0.38f, false);
    }
    else
    {
        ReturnCameraToPlayer();
    }

    Disc->SetLifeSpan(1.5f);
    ActiveDisc = nullptr;
    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->StopMonitoring(TEXT("LieEstablished"));
    }
    FinishSession16RulesSmokeTest();
    RecordHole1FlightRouteResult(false);
    FinishPineRidgePlaySmokeTest(false);
    FinishRegressionRun();
}

void ADiscGolfTourGameMode::FinishLieTransition()
{
    bLieTransitionActive = false;
    if (!CameraViewState.bActive)
    {
        ReturnCameraToPlayer();
    }
}

void ADiscGolfTourGameMode::HandleDiscHoledOut(ADiscActor* Disc)
{
    if (Disc != ActiveDisc) return;

    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->MarkProgress(TEXT("DiscResolved"));
    }

    UpdatePresentationAudioEvents();
    LastFlightTelemetry = Disc->GetFlightComponent()->GetTelemetry();
    bHasLastFlightTelemetry = true;
    bHoleComplete = true;
    bool bRoundScoreRecorded = false;
    if (HasAuthoredRound())
    {
        FString RoundError;
        if (!DiscGolfRound::RecordCurrentHole(RoundState, Strokes, PenaltyStrokes, RoundError))
        {
            UE_LOG(LogDiscGolfTour, Error, TEXT("Round score could not be recorded: %s"), *RoundError);
        }
        else
        {
            bRoundScoreRecorded = true;
            UE_LOG(LogDiscGolfTour, Display, TEXT("Round score recorded: hole %d, %d strokes, round %s."),
                ActiveHole ? ActiveHole->HoleNumber : 0, Strokes,
                *DiscGolfRound::ScoreLabel(DiscGolfRound::ScoreToPar(RoundState)));
        }
        UpdateCourseStatus();
    }
    if (RuntimeCheckpointJournalWriter)
    {
        if (bRoundScoreRecorded)
        {
            RecordRuntimeCheckpointHoleCompleted();
        }
        else
        {
            AbortRuntimeCheckpointJournal(TEXT("the authored hole score was not committed"));
        }
    }
    CurrentLieState = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::Fairway,
        Disc->GetActorLocation(),
        Disc->GetActorLocation(),
        ActiveHole ? ActiveHole->BasketLocation : Disc->GetActorLocation());
    CurrentLieType = ELieType::Circle1;
    CurrentLieState.PenaltyType = EDiscGolfPenaltyType::None;
    CurrentLieState.PenaltyStrokes = 0;
    CurrentLieState.LieType = ELieType::Circle1;
    CurrentLieState.ShotContext = EDiscShotContext::Circle1Putt;
    LieRulesStatusText = TEXT("CENTER CHAINS - NO PENALTY");
    LastShotPresentationResult.bValid = true;
    LastShotPresentationResult.Landing = EDiscGolfLandingClassification::Green;
    LastShotPresentationResult.CarryMeters = LastFlightTelemetry.CarryMeters;
    LastShotPresentationResult.TotalMeters = FVector::Dist2D(ThrowStartLieLocation, Disc->GetActorLocation()) / 100.0f;
    LastShotPresentationResult.RemainingMeters = 0.0f;
    LastShotPresentationResult.Penalty = EDiscGolfPenaltyType::None;
    LastShotPresentationResult.bHoledOut = true;
    LastShotResultWorldSeconds = GetWorld()->GetTimeSeconds();
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveBasketOutcome(
        LastFlightTelemetry.LastBasketContact, LastFlightTelemetry.SpeedMps),
        Disc->GetActorLocation(), LastFlightTelemetry.FlightTimeSeconds, false,
        LastFlightTelemetry.CourseSurface, LastFlightTelemetry.GroundState,
        LastFlightTelemetry.LastBasketContact);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveHoleCompletion(
        ActiveHole ? ActiveHole->HoleNumber : 0, Strokes, ActiveHole ? ActiveHole->Par : 0),
        Disc->GetActorLocation(), LastFlightTelemetry.FlightTimeSeconds);
    if (RoundState.bRoundComplete)
    {
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveRoundCompletion(
            DiscGolfRound::CompletedHoleCount(RoundState), RoundState.HoleScores.Num(),
            DiscGolfRound::ScoreToPar(RoundState)),
            Disc->GetActorLocation(), LastFlightTelemetry.FlightTimeSeconds);
    }
    CaptureRouteTelemetryAttempt(Disc, true);
    FinalizeRouteTelemetryHoleScore();
    CompleteDiscCapture(Disc, true);
    SavePracticeRoundSnapshot();
    StopBroadcastCamera(true);
    ReturnCameraToPlayer();
    Disc->SetLifeSpan(2.0f);
    ActiveDisc = nullptr;
    if (PlayabilityMonitor)
    {
        PlayabilityMonitor->StopMonitoring(
            RoundState.bRoundComplete ? TEXT("RoundComplete") : TEXT("HoleComplete"));
    }
    RecordHole1FlightRouteResult(true);
    FinishPineRidgePlaySmokeTest(true);
    FinishRegressionRun();
    if (bThreeHoleRoundSmokeTestActive)
    {
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::AdvanceThreeHoleRoundSmokeTest));
    }
}

void ADiscGolfTourGameMode::MovePlayerToLie(const FVector& DiscLocation)
{
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer || !ActiveHole) return;

    Golfer->SetActorLocation(DiscLocation + FVector(0, 0, 88.0f));
    Golfer->FaceLocation(ActiveHole->BasketLocation);
}

void ADiscGolfTourGameMode::UpdateLieType()
{
    if (!ActiveHole) return;
    ECourseSurfaceType Surface = ECourseSurfaceType::Fairway;
    FVector GroundLocation = CurrentLieLocation;
    TraceCourseSurfaceAtLocation(CurrentLieLocation, Surface, GroundLocation);
    CurrentLieState = DiscGolfCourseRules::ResolveLie(
        Surface, CurrentLieLocation, CurrentLieLocation, ActiveHole->BasketLocation);
    CurrentLieType = CurrentLieState.LieType;
    LieRulesStatusText = FString::Printf(TEXT("%s - %s"),
        *DiscGolfCourseRules::SurfaceName(CurrentLieState.PlayingSurface),
        *DiscGolfCourseRules::LieEffectsText(CurrentLieState.Effects));
    ApplyShotContextToGolfer();
}

bool ADiscGolfTourGameMode::TraceCourseSurfaceAtLocation(
    const FVector& LocationCm,
    ECourseSurfaceType& OutSurface,
    FVector& OutGroundLocationCm) const
{
    OutSurface = ECourseSurfaceType::Fairway;
    OutGroundLocationCm = LocationCm;
    UWorld* World = GetWorld();
    if (!World) return false;

    const FVector TraceStart(LocationCm.X, LocationCm.Y, FMath::Max(LocationCm.Z + 500.0f, 5000.0f));
    const FVector TraceEnd(LocationCm.X, LocationCm.Y, -1000.0f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGolfCourseSurface), false);
    Params.bReturnPhysicalMaterial = true;
    if (ActiveDisc) Params.AddIgnoredActor(ActiveDisc);
    const FName ActiveHoleSurfaceTag = ActiveHole && HasAuthoredRound()
        ? FName(*FString::Printf(TEXT("Course.Hole.%d"), ActiveHole->HoleNumber))
        : NAME_None;
    bool bHasPresentationGround = false;
    FVector PresentationGround = LocationCm;
    bool bHasTypedGround = false;
    ECourseSurfaceType TypedSurface = ECourseSurfaceType::Fairway;
    FVector TypedGround = LocationCm;
    bool bTypedUsesContinuousGround = false;

    const auto FinishTypedProbe = [&]()
    {
        if (!bHasTypedGround) return false;
        OutSurface = TypedSurface;
        if (!bHasPresentationGround)
        {
            OutGroundLocationCm = TypedGround;
        }
        else if (bTypedUsesContinuousGround)
        {
            OutGroundLocationCm = PresentationGround;
        }
        else
        {
            // Pads, rock, hazards, OB, and water remain authored vertical
            // authorities whenever they sit above the continuous ground.
            OutGroundLocationCm = TypedGround.Z >= PresentationGround.Z
                ? TypedGround : PresentationGround;
        }
        return true;
    };

    // A tree trunk can block the vertical visibility trace before the terrain.
    // Ignore unrelated blockers one actor at a time until typed/tagged/material
    // course collision is reached.
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
        {
            return FinishTypedProbe();
        }
        AActor* HitActor = Hit.GetActor();
        if (HitActor && HitActor->ActorHasTag(TEXT("Presentation.CourseTerrain")))
        {
            bHasPresentationGround = true;
            PresentationGround = Hit.ImpactPoint;
            if (bHasTypedGround) return FinishTypedProbe();
            Params.AddIgnoredActor(HitActor);
            continue;
        }
        if (!ActiveHoleSurfaceTag.IsNone()
            && HitActor
            && !HitActor->ActorHasTag(ActiveHoleSurfaceTag))
        {
            Params.AddIgnoredActor(HitActor);
            continue;
        }
        if (DiscGolfCourseRules::HasSurfaceIdentity(HitActor, Hit.PhysMaterial.Get()))
        {
            TypedSurface = DiscGolfCourseRules::ResolveSurface(HitActor, Hit.PhysMaterial.Get());
            TypedGround = Hit.ImpactPoint;
            bHasTypedGround = true;
            bTypedUsesContinuousGround = TypedSurface == ECourseSurfaceType::Fairway
                || TypedSurface == ECourseSurfaceType::LightRough
                || TypedSurface == ECourseSurfaceType::DeepRough
                || TypedSurface == ECourseSurfaceType::Dirt;
            if (!bTypedUsesContinuousGround || bHasPresentationGround) return FinishTypedProbe();
            Params.AddIgnoredActor(HitActor);
            continue;
        }
        if (!HitActor) return false;
        Params.AddIgnoredActor(HitActor);
    }
    return FinishTypedProbe();
}

FName ADiscGolfTourGameMode::ResolvePresentationSurfaceMaterialAtLocation(const FVector& LocationCm) const
{
    UWorld* World = GetWorld();
    if (!World) return NAME_None;
    const FVector TraceStart(LocationCm.X, LocationCm.Y, FMath::Max(LocationCm.Z + 500.0f, 5000.0f));
    const FVector TraceEnd(LocationCm.X, LocationCm.Y, -1000.0f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DiscGolfPresentationSurface), false);
    if (ActiveDisc) Params.AddIgnoredActor(ActiveDisc);
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)) return NAME_None;
        const AActor* HitActor = Hit.GetActor();
        if (DiscGolfCourseRules::HasSurfaceIdentity(HitActor, Hit.PhysMaterial.Get()))
        {
            return HitActor && HitActor->ActorHasTag(TEXT("Presentation.Surface.Water"))
                ? FName(TEXT("Water")) : NAME_None;
        }
        if (!HitActor) return NAME_None;
        Params.AddIgnoredActor(HitActor);
    }
    return NAME_None;
}

FVector ADiscGolfTourGameMode::FindLastInBoundsRelief(
    const TArray<FDiscTrajectorySample>& Samples,
    const FVector& FallbackLocationCm) const
{
    if (Samples.IsEmpty()) return FallbackLocationCm;

    FVector OutsidePoint = Samples.Last().WorldLocationCm;
    constexpr int32 SampleStride = 12;
    for (int32 Index = Samples.Num() - 1; Index > 0; Index = FMath::Max(0, Index - SampleStride))
    {
        const int32 PriorIndex = FMath::Max(0, Index - SampleStride);
        ECourseSurfaceType Surface = ECourseSurfaceType::Fairway;
        FVector GroundPoint = Samples[PriorIndex].WorldLocationCm;
        const bool bSurfaceTraceHit = TraceCourseSurfaceAtLocation(
            Samples[PriorIndex].WorldLocationCm, Surface, GroundPoint);
        if (!DiscGolfCourseRules::IsInBoundsSurfaceProbe(
            bSurfaceTraceHit, Surface,
            Samples[PriorIndex].WorldLocationCm, GroundPoint))
        {
            OutsidePoint = GroundPoint;
        }
        else
        {
            FVector InsidePoint = GroundPoint;
            for (int32 Refine = 0; Refine < 10; ++Refine)
            {
                const FVector Midpoint = (InsidePoint + OutsidePoint) * 0.5f;
                ECourseSurfaceType MidSurface = ECourseSurfaceType::Fairway;
                FVector MidGround = Midpoint;
                const bool bMidSurfaceTraceHit = TraceCourseSurfaceAtLocation(
                    Midpoint, MidSurface, MidGround);
                if (!DiscGolfCourseRules::IsInBoundsSurfaceProbe(
                    bMidSurfaceTraceHit, MidSurface, Midpoint, MidGround)) OutsidePoint = MidGround;
                else InsidePoint = MidGround;
            }

            FVector Relief = DiscGolfCourseRules::ReliefPointInsideBoundary(InsidePoint, OutsidePoint);
            ECourseSurfaceType ReliefSurface = ECourseSurfaceType::Fairway;
            FVector ReliefGround = Relief;
            const bool bReliefSurfaceTraceHit = TraceCourseSurfaceAtLocation(
                Relief, ReliefSurface, ReliefGround);
            if (DiscGolfCourseRules::IsInBoundsSurfaceProbe(
                bReliefSurfaceTraceHit, ReliefSurface, Relief, ReliefGround))
            {
                return ReliefGround;
            }
            // The one-metre relief can itself cross a narrow or irregular
            // boundary. Never return an untraced point as a playable lie.
            return InsidePoint;
        }
        if (PriorIndex == 0) break;
    }
    ECourseSurfaceType FallbackSurface = ECourseSurfaceType::Fairway;
    FVector FallbackGround = FallbackLocationCm;
    const bool bFallbackSurfaceTraceHit = TraceCourseSurfaceAtLocation(
        FallbackLocationCm, FallbackSurface, FallbackGround);
    return DiscGolfCourseRules::IsInBoundsSurfaceProbe(
        bFallbackSurfaceTraceHit, FallbackSurface,
        FallbackLocationCm, FallbackGround)
        ? FallbackGround : FallbackLocationCm;
}

void ADiscGolfTourGameMode::ResolveSettledLie(ADiscActor* Disc, const FVector& FinalLocation)
{
    if (!Disc || !ActiveHole) return;

    ECourseSurfaceType SurfaceAtRest = LastFlightTelemetry.CourseSurface;
    FVector GroundLocation = FinalLocation;
    const bool bSurfaceTraceHit = TraceCourseSurfaceAtLocation(
        FinalLocation, SurfaceAtRest, GroundLocation);
    const bool bSupportedSettledSurface =
        DiscGolfCourseRules::IsSupportedSettledSurfaceProbe(
            bSurfaceTraceHit, FinalLocation, GroundLocation);
    if (!bSupportedSettledSurface)
    {
        // A max-flight timeout or world-edge fallthrough has no authoritative
        // playing surface. Recover from the last proven in-bounds sample.
        SurfaceAtRest = ECourseSurfaceType::OutOfBounds;
    }
    const FVector ResolvedRawLocation = SurfaceAtRest == ECourseSurfaceType::OutOfBounds
        ? FinalLocation : GroundLocation;
    const FVector LastInBounds = SurfaceAtRest == ECourseSurfaceType::OutOfBounds
        ? FindLastInBoundsRelief(Disc->GetFlightComponent()->GetTrajectorySamples(), ThrowStartLieLocation)
        : ResolvedRawLocation;

    CurrentLieState = DiscGolfCourseRules::ResolveLie(
        SurfaceAtRest, ResolvedRawLocation, LastInBounds, ActiveHole->BasketLocation);
    CurrentLieLocation = CurrentLieState.LieLocationCm;
    CurrentLieType = CurrentLieState.LieType;
    Strokes += CurrentLieState.PenaltyStrokes;
    PenaltyStrokes += CurrentLieState.PenaltyStrokes;

    if (CurrentLieState.PenaltyType == EDiscGolfPenaltyType::OutOfBounds)
    {
        LieRulesStatusText = TEXT("OUT OF BOUNDS +1 - LAST IN BOUNDS RELIEF");
    }
    else if (CurrentLieState.PenaltyType == EDiscGolfPenaltyType::Hazard)
    {
        LieRulesStatusText = FString::Printf(TEXT("HAZARD +1 - PLAY FROM RESULT | %s"),
            *DiscGolfCourseRules::LieEffectsText(CurrentLieState.Effects));
    }
    else
    {
        LieRulesStatusText = FString::Printf(TEXT("%s - %s"),
            *DiscGolfCourseRules::SurfaceName(CurrentLieState.PlayingSurface),
            *DiscGolfCourseRules::LieEffectsText(CurrentLieState.Effects));
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Lie resolved: rest=%s play=%s lie=%d penalty=%s +%d at [%.1f, %.1f, %.1f]."),
        *DiscGolfCourseRules::SurfaceName(CurrentLieState.SurfaceAtRest),
        *DiscGolfCourseRules::SurfaceName(CurrentLieState.PlayingSurface),
        static_cast<int32>(CurrentLieState.LieType),
        *DiscGolfCourseRules::PenaltyName(CurrentLieState.PenaltyType),
        CurrentLieState.PenaltyStrokes,
        CurrentLieLocation.X, CurrentLieLocation.Y, CurrentLieLocation.Z);

    ApplyShotContextToGolfer();
    SavePracticeRoundSnapshot();
}

void ADiscGolfTourGameMode::SavePracticeRoundSnapshot()
{
    const bool bDeveloperToolNoSave = FParse::Param(
        FCommandLine::Get(), TEXT("DGDeveloperToolNoSave"));
    if (bRegressionActive || bDeveloperToolNoSave) return;
    UDiscGolfTourGameInstance* GameInstance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    UDiscGolfSaveGame* Profile = GameInstance ? GameInstance->GetProfile() : nullptr;
    if (!GameInstance || !Profile) return;

    Profile->bHasPracticeRoundSnapshot = true;
    Profile->PracticeStrokes = Strokes;
    Profile->PracticePenaltyStrokes = PenaltyStrokes;
    Profile->bPracticeHoleComplete = bHoleComplete;
    Profile->PracticeLieState = CurrentLieState;
    Profile->PracticeCourseId = ActiveHole ? ActiveHole->CourseId : NAME_None;
    Profile->PracticeLayoutId = ActiveHole ? ActiveHole->LayoutId : NAME_None;
    Profile->PracticeHoleNumber = ActiveHole ? ActiveHole->HoleNumber : 1;
    Profile->PracticeRoundState = RoundState;
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        if (const UDiscBagComponent* Bag = Golfer->GetDiscBag())
        {
            Profile->PracticeMoldId = Bag->GetSelectedMoldId();
            Profile->PracticePlastic = Bag->GetSelectedPlastic();
        }
    }
    GameInstance->SaveProfile();
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        if (const UDiscBagComponent* Bag = Golfer->GetDiscBag())
        {
            FString EquipmentSaveError;
            if (!Bag->SaveEquipment(EquipmentSaveError))
            {
                UE_LOG(LogDiscGolfTour, Warning,
                    TEXT("Practice profile saved but isolated equipment payload failed: %s"),
                    *EquipmentSaveError);
            }
        }
    }
}

bool ADiscGolfTourGameMode::RestorePracticeRoundSnapshot()
{
    UDiscGolfTourGameInstance* GameInstance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    const UDiscGolfSaveGame* Profile = GameInstance ? GameInstance->GetProfile() : nullptr;
    if (!Profile || !Profile->bHasPracticeRoundSnapshot
        || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        return false;
    }

    const bool bPineRidge = Profile->PracticeCourseId == TEXT("PineRidgeChampionship");
    const bool bRegression = Profile->PracticeCourseId == TEXT("RegressionCourse");
    if ((!bPineRidge && !bRegression)
        || Profile->PracticeHoleNumber < 1
        || Profile->PracticeStrokes < 0
        || Profile->PracticePenaltyStrokes < 0
        || Profile->PracticePenaltyStrokes > Profile->PracticeStrokes
        || Profile->PracticeLieState.SchemaVersion != 1
        || Profile->PracticeLieState.LieLocationCm.ContainsNaN()
        || Profile->PracticeLieState.RawDiscLocationCm.ContainsNaN())
    {
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because its core fields are invalid."));
        return false;
    }

    if (!LoadCourse(bPineRidge ? TEXT("PineRidge") : TEXT("Regression")))
    {
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because its course could not be loaded."));
        return false;
    }

    if (bPineRidge)
    {
        const FDiscGolfRoundState& SavedRound = Profile->PracticeRoundState;
        if (SavedRound.CourseId != RoundState.CourseId
            || SavedRound.LayoutId != RoundState.LayoutId
            || SavedRound.HoleScores.Num() != RoundState.HoleScores.Num()
            || !SavedRound.HoleScores.IsValidIndex(Profile->PracticeHoleNumber - 1))
        {
            UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because its round identity is stale."));
            return false;
        }
        for (int32 Index = 0; Index < RoundState.HoleScores.Num(); ++Index)
        {
            const FDiscGolfRoundHoleScore& SavedScore = SavedRound.HoleScores[Index];
            const FDiscGolfRoundHoleScore& ExpectedScore = RoundState.HoleScores[Index];
            if (SavedScore.HoleNumber != ExpectedScore.HoleNumber || SavedScore.Par != ExpectedScore.Par
                || SavedScore.Strokes < 0 || SavedScore.PenaltyStrokes < 0
                || SavedScore.PenaltyStrokes > SavedScore.Strokes
                || (SavedScore.bCompleted && SavedScore.Strokes == 0))
            {
                UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because a saved hole score is invalid."));
                return false;
            }
        }
        const FDiscGolfRoundHoleScore& ActiveSavedScore =
            SavedRound.HoleScores[Profile->PracticeHoleNumber - 1];
        if (SavedRound.CurrentHoleIndex != Profile->PracticeHoleNumber - 1
            || ActiveSavedScore.bCompleted != Profile->bPracticeHoleComplete
            || (Profile->bPracticeHoleComplete
                && (ActiveSavedScore.Strokes != Profile->PracticeStrokes
                    || ActiveSavedScore.PenaltyStrokes != Profile->PracticePenaltyStrokes))
            || SavedRound.bRoundComplete !=
                (DiscGolfRound::CompletedHoleCount(SavedRound) == SavedRound.HoleScores.Num()))
        {
            UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because its active-hole state is inconsistent."));
            return false;
        }
        if (!LoadRoundHole(Profile->PracticeHoleNumber)) return false;
        RoundState = SavedRound;
        RoundState.CurrentHoleIndex = Profile->PracticeHoleNumber - 1;
    }
    else if (Profile->PracticeHoleNumber != 1 || !Profile->PracticeRoundState.HoleScores.IsEmpty())
    {
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because its regression-course state is invalid."));
        return false;
    }

    if (!ActiveHole || Profile->PracticeLayoutId != ActiveHole->LayoutId
        || FVector::Dist2D(Profile->PracticeLieState.LieLocationCm, ActiveHole->BasketLocation) > 500000.0f)
    {
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Practice snapshot ignored because its lie does not match the loaded hole."));
        return false;
    }

    ECourseSurfaceType RestoredLieSurface = ECourseSurfaceType::Fairway;
    FVector RestoredLieGround = Profile->PracticeLieState.LieLocationCm;
    FDiscGolfLieState RestoredLieState = Profile->PracticeLieState;
    bool bRestoredLieWasCanonicalized = false;
    if (!Profile->bPracticeHoleComplete)
    {
        const bool bRestoredLieSurfaceTraceHit = TraceCourseSurfaceAtLocation(
            Profile->PracticeLieState.LieLocationCm,
            RestoredLieSurface,
            RestoredLieGround);
        const bool bSupportedRestoredLie =
            DiscGolfCourseRules::TryProjectRestoredLieToSupportedGround(
                Profile->PracticeLieState,
                bRestoredLieSurfaceTraceHit,
                RestoredLieGround,
                RestoredLieState);
        if (!bSupportedRestoredLie
            || RestoredLieSurface == ECourseSurfaceType::OutOfBounds)
        {
            UE_LOG(LogDiscGolfTour, Warning,
                TEXT("Practice snapshot ignored because its saved lie has no supported in-course surface."));
            return false;
        }
        bRestoredLieWasCanonicalized =
            !RestoredLieState.LieLocationCm.Equals(
                Profile->PracticeLieState.LieLocationCm, 0.01f);
    }

    Strokes = Profile->PracticeStrokes;
    PenaltyStrokes = Profile->PracticePenaltyStrokes;
    bHoleComplete = Profile->bPracticeHoleComplete;
    CurrentLieState = MoveTemp(RestoredLieState);
    CurrentLieLocation = CurrentLieState.LieLocationCm;
    ThrowStartLieLocation = CurrentLieLocation;
    CurrentLieType = CurrentLieState.LieType;
    LieRulesStatusText = FString::Printf(TEXT("RESTORED - %s - %s"),
        *DiscGolfCourseRules::SurfaceName(CurrentLieState.PlayingSurface),
        *DiscGolfCourseRules::LieEffectsText(CurrentLieState.Effects));
    if (!bHoleComplete)
    {
        MovePlayerToLie(CurrentLieLocation);
    }
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        if (UDiscBagComponent* Bag = Golfer->GetDiscBag();
            Bag && !Bag->SelectEquipment(Profile->PracticeMoldId, Profile->PracticePlastic))
        {
            Bag->SelectEquipment(TEXT("Apex"), EDiscPlastic::Tour);
            UE_LOG(LogDiscGolfTour, Warning,
                TEXT("Practice snapshot equipment was unavailable; restored the Apex Tour fallback."));
        }
    }
    ApplyShotContextToGolfer();
    UpdateCourseStatus();
    CourseStatusText += TEXT(" | PRACTICE RESTORED");
    if (bRestoredLieWasCanonicalized)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("Practice snapshot lie was projected onto current supported course ground."));
        SavePracticeRoundSnapshot();
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("Practice snapshot restored: %s hole %d, %d strokes, %d penalties."),
        *Profile->PracticeCourseId.ToString(), Profile->PracticeHoleNumber, Strokes, PenaltyStrokes);
    return true;
}

void ADiscGolfTourGameMode::ReturnCameraToPlayer()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (APawn* Pawn = PC->GetPawn())
        {
            const float BlendSeconds = GetPlayerSettings().bReducedMotion ? 0.0f : 0.45f;
            PC->SetViewTargetWithBlend(Pawn, BlendSeconds);
        }
    }
}

void ADiscGolfTourGameMode::ResetHole()
{
    if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0)))
    {
        Golfer->CancelAnimatedThrow();
    }

    if (RoundState.bRoundComplete && ActiveDisc == nullptr && !bRegressionActive)
    {
        RestartRound();
        return;
    }

    // Every external reset cancels regression, including the one-tick transition
    // between suite presets. StartRegressionPreset owns the only internal reset
    // exemption through bRegressionInternalReset.
    const bool bCancelledActiveRegression = ShouldCancelRegressionForReset(
        bRegressionActive, bRegressionInternalReset);
    const bool bCancelledRegressionSuite =
        bCancelledActiveRegression && bRegressionSuiteActive;
    if (bCancelledActiveRegression)
    {
        bRegressionActive = false;
        bRegressionSuiteActive = false;
        ActiveRegressionPresetId = NAME_None;
        ActiveRegressionRenderFps = 0;
        RegressionQueue.Reset();
        RegressionQueuePosition = 0;
        RegressionPresentationAudioSequenceBaseline = 0;
        RestoreRegressionEnvironment();
    }

    RouteTelemetryAttemptAwaitingScoreIndex = INDEX_NONE;
    bRouteTelemetryShotPending = false;
    StopInstantReplay(false);
    StopBroadcastCamera(false);
    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr)
    {
        Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
        Route->StopPreview();
    }
    ActiveFlyoverRoute = nullptr;
    if (FlyoverCameraViewToken.IsValid())
    {
        FString ReleaseError;
        DiscGolfCameraViewContract::TryRelease(CameraViewState, FlyoverCameraViewToken, ReleaseError);
        FlyoverCameraViewToken = FDiscGolfCameraViewToken();
    }
    if (ActiveDisc)
    {
        ActiveDisc->Destroy();
        ActiveDisc = nullptr;
    }
    LastReplaySamples.Reset();
    LastPresentationGroundTransitionCount = 0;
    ReplayStatusText = TEXT("Replay waiting for a completed shot");
    if (RoundState.HoleScores.IsValidIndex(RoundState.CurrentHoleIndex)
        && RoundState.HoleScores[RoundState.CurrentHoleIndex].bCompleted)
    {
        FDiscGolfRoundHoleScore& Score = RoundState.HoleScores[RoundState.CurrentHoleIndex];
        Score.Strokes = 0;
        Score.PenaltyStrokes = 0;
        Score.bCompleted = false;
        RoundState.bRoundComplete = false;
    }
    bScorecardVisible = false;
    StartHole();
    UpdateCourseStatus();
    if (bCancelledActiveRegression)
    {
        TrajectoryStatusText = TEXT("Regression cancelled - ready");
        if (bCancelledRegressionSuite)
        {
            FailRegressionSuiteSmoke(TEXT("Active regression was cancelled"));
        }
        RegressionSuiteSummaries.Reset();
    }
}

void ADiscGolfTourGameMode::RecordPresentationAudioEvent(
    const FDiscGolfPresentationAudioEvent& Event,
    const FVector& WorldLocationCm,
    float EventTimeSeconds,
    bool bReplayPresentation,
    ECourseSurfaceType CourseSurface,
    EDiscGroundState GroundState,
    EBasketContactResult BasketResult,
    EDiscGolfPenaltyType Penalty)
{
    if (!DiscGolfPresentationAudio::ShouldEmit(Event, bRegressionActive)) return;

    FDiscGolfPresentationAudioContext Context;
    Context.ShotSequence = PresentationShotSequence;
    Context.HoleNumber = ActiveHole ? ActiveHole->HoleNumber : 0;
    Context.WorldLocationCm = WorldLocationCm;
    Context.EventTimeSeconds = FMath::Max(FMath::IsFinite(EventTimeSeconds) ? EventTimeSeconds : 0.0f, 0.0f);
    Context.bReplayPresentation = bReplayPresentation;
    Context.CourseSurface = CourseSurface;
    Context.GroundState = GroundState;
    Context.BasketResult = BasketResult;
    Context.Penalty = Penalty;
    FDiscGolfPresentationAudioEvent ContextualEvent =
        DiscGolfPresentationAudio::WithContext(Event, Context);
    if (!ContextualEvent.IsValid()) return;

    if (!DiscGolfPresentationAudio::AppendBoundedTrace(
        PresentationAudioEventTrace, ContextualEvent))
    {
        return;
    }
    LastPresentationAudioEvent = ContextualEvent;
    ++PresentationAudioEventSequence;

    if (PresentationAudioRouter)
    {
        const FDiscGolfPlayerSettings Settings = GetPlayerSettings();
        const float OutputGain01 = FMath::Clamp(
            Settings.MasterVolume * Settings.EffectsVolume, 0.0f, 1.0f);
        PresentationAudioRouter->ConsumeSemanticEvent(
            ContextualEvent, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, OutputGain01);
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Presentation audio event #%llu: %s | %s | intensity %.2f | pitch %.2f"),
        static_cast<unsigned long long>(PresentationAudioEventSequence),
        *ContextualEvent.EventId.ToString(), *ContextualEvent.Label,
        ContextualEvent.Intensity01, ContextualEvent.PitchMultiplier);
}

void ADiscGolfTourGameMode::UpdatePresentationAudioEvents()
{
    if (!ActiveDisc || bRegressionActive || !ActiveDisc->GetFlightComponent()) return;
    const TArray<FDiscGroundTransition>& Transitions =
        ActiveDisc->GetFlightComponent()->GetGroundTransitions();
    LastPresentationGroundTransitionCount = FMath::Clamp(
        LastPresentationGroundTransitionCount, 0, Transitions.Num());
    for (int32 Index = LastPresentationGroundTransitionCount; Index < Transitions.Num(); ++Index)
    {
        const FDiscGroundTransition& Transition = Transitions[Index];
        if (Transition.FromState == EDiscGroundState::Airborne
            || Transition.ToState == EDiscGroundState::Impact)
        {
            RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveCourseSurfaceContact(
                Transition.Surface, Transition.CourseSurface,
                Transition.ImpactSpeedMps, Transition.IncidenceAngleDeg,
                ResolvePresentationSurfaceMaterialAtLocation(Transition.WorldLocationCm)),
                Transition.WorldLocationCm, Transition.TimeSeconds, false,
                Transition.CourseSurface, Transition.ToState);
        }
        if (Transition.ToState != EDiscGroundState::Impact)
        {
            RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveGroundState(
                Transition.ToState, Transition.Surface, Transition.ImpactSpeedMps),
                Transition.WorldLocationCm, Transition.TimeSeconds, false,
                Transition.CourseSurface, Transition.ToState);
        }
    }
    LastPresentationGroundTransitionCount = Transitions.Num();
}

FString ADiscGolfTourGameMode::GetPresentationAudioStatusText() const
{
    if (!LastPresentationAudioEvent.IsValid())
    {
        return TEXT("AUDIO EVENTS READY - SILENT ASSET FALLBACK");
    }
    return FString::Printf(TEXT("AUDIO EVENT %llu | %s | %s"),
        static_cast<unsigned long long>(PresentationAudioEventSequence),
        *LastPresentationAudioEvent.EventId.ToString(), *LastPresentationAudioEvent.Label);
}

void ADiscGolfTourGameMode::RunCourseRulesFixture(const FString& FixtureName)
{
    if (ActiveDisc || bRegressionActive || !ActiveHole)
    {
        TrajectoryStatusText = TEXT("Wait for the current throw or regression to finish");
        return;
    }

    const bool bObFixture = FixtureName.Equals(TEXT("OB"), ESearchCase::IgnoreCase)
        || FixtureName.Equals(TEXT("OutOfBounds"), ESearchCase::IgnoreCase);
    const bool bHazardFixture = FixtureName.Equals(TEXT("Hazard"), ESearchCase::IgnoreCase);
    const bool bWaterFixture = FixtureName.Equals(TEXT("Water"), ESearchCase::IgnoreCase);
    if (!bObFixture && !bHazardFixture && !bWaterFixture)
    {
        TrajectoryStatusText = TEXT("Rules fixture must be OB, Hazard, or Water");
        return;
    }

    if (bWaterFixture)
    {
        if ((!HasAuthoredRound()
                || ActiveCourseManifest.CourseId != TEXT("PineRidgeChampionship"))
            && !LoadCourse(TEXT("PineRidge")))
        {
            TrajectoryStatusText = TEXT("Water fixture could not load Pine Ridge");
            return;
        }
        if ((!ActiveHole || ActiveHole->HoleNumber != 3) && !LoadRoundHole(3))
        {
            TrajectoryStatusText = TEXT("Water fixture could not activate Pine Ridge Hole 3");
            return;
        }
    }
    else if (ActiveHole->CourseId != TEXT("RegressionCourse")
        && !LoadCourse(TEXT("Regression")))
    {
        TrajectoryStatusText = TEXT("Rules fixture could not load the regression course");
        return;
    }

    ResetHole();
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer) return;

    const FVector Direction = bObFixture
        ? FVector(0.0f, -1.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
    if (bWaterFixture)
    {
        const FDiscGolfHoleBlockoutDefinition* HoleDefinition =
            ActiveHoleDefinitions.IsValidIndex(RoundState.CurrentHoleIndex)
                ? &ActiveHoleDefinitions[RoundState.CurrentHoleIndex] : nullptr;
        const FDiscGolfBlockoutSurfaceDefinition* WaterSurface = HoleDefinition
            ? HoleDefinition->Surfaces.FindByPredicate(
                [](const FDiscGolfBlockoutSurfaceDefinition& Surface)
                {
                    return Surface.SurfaceType == ECourseSurfaceType::Hazard
                        && Surface.SurfaceId.ToString().Contains(TEXT("LakeWater"));
                })
            : nullptr;
        if (!WaterSurface)
        {
            TrajectoryStatusText = TEXT("Water fixture could not resolve authored GalleryLakeWater authority");
            return;
        }
        CurrentLieLocation = WaterSurface->LocationCm;
    }
    else
    {
        CurrentLieLocation = bObFixture
            ? FVector(6200.0f, -2450.0f, 0.0f)
            : FVector(8300.0f, 2900.0f, 0.0f);
    }
    CurrentLieState = DiscGolfCourseRules::ResolveLie(
        ECourseSurfaceType::Fairway,
        CurrentLieLocation,
        CurrentLieLocation,
        ActiveHole->BasketLocation);
    CurrentLieType = CurrentLieState.LieType;
    MovePlayerToLie(CurrentLieLocation);
    Golfer->FaceLocation(CurrentLieLocation + Direction * 1000.0f);
    ApplyShotContextToGolfer();

    FThrowCommand Command;
    Command.MoldId = TEXT("Touch");
    Command.Plastic = EDiscPlastic::Base;
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Drive;
    Command.Direction = Direction;
    Command.Power01 = 0.0f;
    Command.HyzerDeg = 0.0f;
    Command.NoseAngleDeg = -1.0f;
    Command.LaunchAngleDeg = -5.0f;
    Command.TimingError = 0.0f;
    TrajectoryStatusText = bObFixture
        ? TEXT("RUNNING OUT-OF-BOUNDS RULES FIXTURE")
        : bWaterFixture
            ? TEXT("RUNNING AUTHORED PINE RIDGE WATER RULES FIXTURE")
            : TEXT("RUNNING HAZARD RULES FIXTURE");
    FString CommandError;
    if (!TryBindAutomatedPlayerCommand(Golfer, Command, CommandError)
        || !RequestThrow(Command))
    {
        TrajectoryStatusText = FString::Printf(
            TEXT("RULES FIXTURE THROW REJECTED - %s"),
            CommandError.IsEmpty() ? TEXT("authoritative launch failed") : *CommandError);
    }
}

void ADiscGolfTourGameMode::StartBroadcastCameraForShot(
    ADiscActor* Disc,
    const FVector& ReleaseLocationCm,
    EDiscShotContext ShotContext)
{
    StopBroadcastCamera(false);
    if (!GetWorld() || !Disc || !ActiveHole) return;
    if (!GetPlayerSettings().bAutoFollowDisc) return;

    FString AcquireError;
    if (!DiscGolfCameraViewContract::TryAcquire(
        CameraViewState,
        EDiscGolfCameraViewOwner::LiveShot,
        EDiscGolfCameraViewMode::FollowDisc,
        BroadcastCameraViewToken,
        AcquireError))
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Broadcast camera view blocked: %s"), *AcquireError);
        return;
    }

    BroadcastCameraDirector = GetWorld()->SpawnActor<ADiscBroadcastCameraDirector>(
        ReleaseLocationCm, FRotator::ZeroRotator);
    if (!BroadcastCameraDirector || !BroadcastCameraDirector->InitializeForShot(
        Disc, ReleaseLocationCm, ActiveHole->BasketLocation, ShotContext))
    {
        if (BroadcastCameraDirector) BroadcastCameraDirector->Destroy();
        BroadcastCameraDirector = nullptr;
        FString ReleaseError;
        DiscGolfCameraViewContract::TryRelease(
            CameraViewState, BroadcastCameraViewToken, ReleaseError);
        BroadcastCameraViewToken = FDiscGolfCameraViewToken();
        ReturnCameraToPlayer();
        return;
    }

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        const float BlendSeconds = GetPlayerSettings().bReducedMotion ? 0.0f : 0.16f;
        PC->SetViewTargetWithBlend(BroadcastCameraDirector, BlendSeconds);
    }
}

void ADiscGolfTourGameMode::StopBroadcastCamera(bool bPreserveForCameraBlend)
{
    if (!BroadcastCameraDirector)
    {
        if (BroadcastCameraViewToken.IsValid())
        {
            FString ReleaseError;
            DiscGolfCameraViewContract::TryRelease(
                CameraViewState, BroadcastCameraViewToken, ReleaseError);
        }
        BroadcastCameraViewToken = FDiscGolfCameraViewToken();
        return;
    }

    ADiscBroadcastCameraDirector* PreviousDirector = BroadcastCameraDirector;
    BroadcastCameraDirector = nullptr;
    PreviousDirector->StopTracking();
    if (bPreserveForCameraBlend)
    {
        PreviousDirector->SetLifeSpan(0.60f);
    }
    else
    {
        PreviousDirector->Destroy();
    }
    FString ReleaseError;
    if (!DiscGolfCameraViewContract::TryRelease(
        CameraViewState, BroadcastCameraViewToken, ReleaseError))
    {
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Broadcast camera release ignored: %s"), *ReleaseError);
    }
    BroadcastCameraViewToken = FDiscGolfCameraViewToken();
}

UDiscTrajectorySubsystem* ADiscGolfTourGameMode::GetTrajectorySubsystem() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UDiscTrajectorySubsystem>() : nullptr;
}

bool ADiscGolfTourGameMode::HasLastTrajectorySummary() const
{
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    return Trajectories && Trajectories->HasLastCapture();
}

FDiscTrajectorySummary ADiscGolfTourGameMode::GetLastTrajectorySummary() const
{
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    return Trajectories && Trajectories->HasLastCapture()
        ? Trajectories->GetLastSummary()
        : FDiscTrajectorySummary();
}

bool ADiscGolfTourGameMode::DeferNextTrajectoryExport(FString& OutError)
{
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories)
    {
        OutError = TEXT("Trajectory subsystem is unavailable");
        return false;
    }
    if (!Trajectories->DeferNextCaptureExport(OutError))
    {
        return false;
    }

    TrajectoryStatusText = TEXT("TRAJECTORY EXPORT DEFERRED - waiting for completed flight");
    return true;
}

bool ADiscGolfTourGameMode::FlushDeferredTrajectoryExport(
    FDiscTrajectorySummary& OutSummary,
    FString& OutError)
{
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories)
    {
        OutSummary = FDiscTrajectorySummary();
        OutError = TEXT("Trajectory subsystem is unavailable");
        return false;
    }
    if (!Trajectories->FlushDeferredCaptureExport(OutSummary, OutError))
    {
        TrajectoryStatusText = FString::Printf(TEXT("DEFERRED EXPORT FAILED - %s"), *OutError);
        return false;
    }

    TrajectoryStatusText = FString::Printf(
        TEXT("EXPORTED DEFERRED %d samples | air %.1f m | final %.1f m"),
        OutSummary.SampleCount,
        OutSummary.AirCarryMeters,
        OutSummary.FinalCarryMeters);
    return true;
}

bool ADiscGolfTourGameMode::DiscardDeferredTrajectoryExport()
{
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories || !Trajectories->DiscardDeferredCaptureExport())
    {
        return false;
    }

    TrajectoryStatusText = TEXT("Trajectory auto-export ready");
    return true;
}

bool ADiscGolfTourGameMode::IsTrajectoryExportDeferralArmed() const
{
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    return Trajectories && Trajectories->IsCaptureExportDeferralArmed();
}

bool ADiscGolfTourGameMode::HasPendingDeferredTrajectoryExport() const
{
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    return Trajectories && Trajectories->HasPendingDeferredCaptureExport();
}

bool ADiscGolfTourGameMode::GetPendingDeferredTrajectorySummary(
    FDiscTrajectorySummary& OutSummary) const
{
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories)
    {
        OutSummary = FDiscTrajectorySummary();
        return false;
    }
    return Trajectories->GetPendingDeferredCaptureSummary(OutSummary);
}

FString ADiscGolfTourGameMode::GetSelectedRegressionPresetName() const
{
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories) return TEXT("NONE");
    const TArray<FPhysicsRegressionPreset>& Presets = Trajectories->GetRegressionPresets();
    return Presets.IsValidIndex(SelectedRegressionPresetIndex)
        ? Presets[SelectedRegressionPresetIndex].DisplayName.ToString()
        : TEXT("NONE");
}

void ADiscGolfTourGameMode::CompleteDiscCapture(ADiscActor* Disc, bool bHoledOut)
{
    if (Disc) RecordThrowHistory(Disc, bHoledOut);
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Disc || !Trajectories) return;
    const bool bSelectedDiagnosticOnly = bRegressionActive && !bRegressionSuiteActive
        && !Trajectories->AreRegressionPresetsAuthoritative();

    const UDiscFlightComponent* Flight = Disc->GetFlightComponent();
    if (!Flight) return;
    StoreReplayCapture(Disc);

    FDiscTrajectorySummary Summary;
    FString Error;
    const FName PresetId = bRegressionActive ? ActiveRegressionPresetId : NAME_None;
    const int32 RenderFps = bRegressionActive ? ActiveRegressionRenderFps : 0;
    if (!Trajectories->CompleteCapture(
        Disc->GetResolvedDisc(), LastRelease, Flight->GetTrajectorySamples(), Flight->GetGroundTransitions(),
        LastFlightTelemetry, CurrentLieState, bHoledOut, PresetId, RenderFps, Summary, Error))
    {
        TrajectoryStatusText = FString::Printf(TEXT("%sEXPORT FAILED - %s"),
            bSelectedDiagnosticOnly ? TEXT("DIAGNOSTIC-ONLY | ") : TEXT(""), *Error);
        UE_LOG(LogDiscGolfTour, Error, TEXT("Trajectory export failed: %s"), *Error);
        return;
    }

    const bool bExportDeferred = Trajectories->HasPendingDeferredCaptureExport();
#if 0 // Throw Lab capture is available only to direct development/test subsystem callers.
    if (!bRegressionActive && bThrowLabActive)
    {
        UDiscThrowLabSubsystem* ThrowLab = GetThrowLabSubsystem();
        FString ThrowLabError;
        if (!ThrowLab || !ThrowLab->RecordCompletedThrow(
            Disc->GetResolvedDisc(), LastRelease, Flight->GetTrajectorySamples(),
            Flight->GetGroundTransitions(), LastFlightTelemetry, Summary, ThrowLabError))
        {
            ThrowLabStatusText = ThrowLab
                ? FString::Printf(TEXT("THROW LAB RECORD REJECTED - %s"), *ThrowLabError)
                : TEXT("THROW LAB RECORD REJECTED - subsystem unavailable");
            UE_LOG(LogDiscGolfTour, Error, TEXT("%s"), *ThrowLabStatusText);
        }
        else
        {
            ThrowLabStatusText = FString::Printf(TEXT("THROW LAB CAPTURED | %s | %d BOUNDED SAMPLES"),
                *Summary.CaptureId, ThrowLab->GetSelectedRecord()
                    ? ThrowLab->GetSelectedRecord()->ReplaySamples.Num() : 0);
            UE_LOG(LogDiscGolfTour, Display, TEXT("%s"), *ThrowLabStatusText);
        }
    }
#endif
    if (bRegressionActive)
    {
        RegressionSuiteSummaries.Add(Summary);
        TrajectoryStatusText = FString::Printf(TEXT("%s%s - %s | carry %.2f m | apex %.2f m%s"),
            bSelectedDiagnosticOnly ? TEXT("DIAGNOSTIC-ONLY | ") : TEXT(""),
            Summary.bRegressionPassed ? TEXT("REGRESSION PASS") : TEXT("REGRESSION FAIL"),
            *Summary.PresetId.ToString(), Summary.FinalCarryMeters, Summary.ApexMeters,
            bExportDeferred ? TEXT(" | export deferred") : TEXT(""));
    }
    else if (bExportDeferred)
    {
        TrajectoryStatusText = FString::Printf(
            TEXT("CAPTURED %d samples | export deferred"),
            Summary.SampleCount);
    }
    else
    {
#if DG_RELEASE_V05_SCOPE
        TrajectoryStatusText = FString::Printf(
            TEXT("CAPTURED %d samples | air %.1f m | final %.1f m"),
            Summary.SampleCount, Summary.AirCarryMeters, Summary.FinalCarryMeters);
#else
        TrajectoryStatusText = FString::Printf(TEXT("EXPORTED %d samples | air %.1f m | final %.1f m"),
            Summary.SampleCount, Summary.AirCarryMeters, Summary.FinalCarryMeters);
#endif
    }
}

void ADiscGolfTourGameMode::RecordThrowHistory(ADiscActor* Disc, bool bHoledOut)
{
    const UDiscFlightComponent* Flight = Disc ? Disc->GetFlightComponent() : nullptr;
    UGameInstance* Instance = GetGameInstance();
    UDiscGolfThrowHistorySubsystem* History = Instance
        ? Instance->GetSubsystem<UDiscGolfThrowHistorySubsystem>() : nullptr;
    if (!Disc || !Flight || !History || !ActiveHole) return;

    const FResolvedDiscDefinition Resolved = Disc->GetResolvedDisc();
    const TArray<FDiscTrajectorySample>& Samples = Flight->GetTrajectorySamples();
    FDiscGolfThrowHistoryEntry Entry;
    Entry.HoleNumber = ActiveHole->HoleNumber;
    Entry.ShotNumber = FMath::Max(1, LastThrowShotNumber);
    Entry.DiscId = Resolved.MoldId;
    Entry.DiscName = Resolved.DisplayName;
    Entry.Plastic = Resolved.Plastic;
    Entry.DiscWeightGrams = Resolved.Aero.MassKg * 1000.0f;
    Entry.ReleaseSpeedMps = LastRelease.ReleaseSpeedMps;
    Entry.HyzerAngleDeg = LastRelease.EffectiveHyzerDeg;
    Entry.NoseAngleDeg = LastRelease.EffectiveNoseAngleDeg;
    Entry.LaunchDirection = LastRelease.Direction;
    Entry.WindMps = Samples.IsEmpty() ? FVector::ZeroVector : Samples[0].WindMps;
    Entry.CarryDistanceMeters = LastFlightTelemetry.CarryMeters;
    Entry.TotalDistanceMeters = Samples.IsEmpty() ? LastFlightTelemetry.CarryMeters
        : FVector::Dist2D(Samples[0].WorldLocationCm, Samples.Last().WorldLocationCm) / 100.0f;
    Entry.RemainingDistanceMeters = bHoledOut ? 0.0f : CurrentLieState.DistanceToBasketMeters;
    Entry.Landing = DiscGolfPlayerExperience::ClassifyLanding(CurrentLieState, bHoledOut);
    Entry.Penalty = CurrentLieState.PenaltyType;
    Entry.PenaltyStrokes = CurrentLieState.PenaltyStrokes;
    Entry.LieLocationCm = bHoledOut ? Disc->GetActorLocation() : CurrentLieLocation;
    Entry.bHoledOut = bHoledOut;
    History->RecordThrow(Entry);
}

void ADiscGolfTourGameMode::StoreReplayCapture(ADiscActor* Disc)
{
    const UDiscFlightComponent* Flight = Disc ? Disc->GetFlightComponent() : nullptr;
    bLastReplayCaptureActualSampleProvenanceValid = false;
    bLastReplayCaptureNominalRateValid = false;
    if (!Flight || Flight->GetTrajectorySamples().Num() < 2) return;
    FString SelectionError;
    const FDiscActualReplaySelectionPolicy SelectionPolicy;
    if (!DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
        Flight->GetTrajectorySamples(), Flight->GetGroundTransitions(),
        SelectionPolicy, LastReplaySamples, SelectionError))
    {
        LastReplaySamples.Reset();
        ReplayStatusText = FString::Printf(
            TEXT("Replay capture rejected - %s"), *SelectionError);
        UE_LOG(LogDiscGolfTour, Warning,
            TEXT("Strict actual-sample replay capture rejected: %s"), *SelectionError);
        return;
    }
    LastReplayDisc = Disc->GetResolvedDisc();
    LastReplayRelease = LastRelease;
    const float Duration = LastReplaySamples.Last().TimeSeconds - LastReplaySamples[0].TimeSeconds;
    bLastReplayCaptureActualSampleProvenanceValid = LastReplaySamples.Num() >= 2;
    for (const FDiscTrajectorySample& Selected : LastReplaySamples)
    {
        bLastReplayCaptureActualSampleProvenanceValid &=
            Flight->GetTrajectorySamples().ContainsByPredicate(
                [&Selected](const FDiscTrajectorySample& Actual)
                {
                    return Actual.TimeSeconds == Selected.TimeSeconds
                        && Actual.WorldLocationCm == Selected.WorldLocationCm
                        && Actual.VelocityMps == Selected.VelocityMps
                        && Actual.DiscNormalWorld == Selected.DiscNormalWorld
                        && Actual.WindMps == Selected.WindMps
                        && Actual.SpinRpm == Selected.SpinRpm
                        && Actual.AngleOfAttackDeg == Selected.AngleOfAttackDeg
                        && Actual.GroundState == Selected.GroundState
                        && Actual.GroundSurface == Selected.GroundSurface
                        && Actual.CourseSurface == Selected.CourseSurface
                        && Actual.GroundContactCount == Selected.GroundContactCount;
                });
    }
    bLastReplayCaptureNominalRateValid = Duration > SMALL_NUMBER
        && static_cast<float>(LastReplaySamples.Num() - 1) / Duration
            <= SelectionPolicy.MaxSampleRateHz + KINDA_SMALL_NUMBER;
    ReplayStatusText = FString::Printf(TEXT("Replay ready - %.1f s - %d bounded actual samples"),
        Duration, LastReplaySamples.Num());
}

void ADiscGolfTourGameMode::ToggleShotTracer()
{
    if (ReplayActor)
    {
        CycleReplayCamera();
        return;
    }
    bShotTracerEnabled = !bShotTracerEnabled;
    ReplayStatusText = FString::Printf(TEXT("Shot tracer %s%s"),
        bShotTracerEnabled ? TEXT("ON") : TEXT("OFF"),
        HasReplayCapture() ? TEXT(" - replay ready") : TEXT(""));
}

void ADiscGolfTourGameMode::CycleReplayCamera()
{
    if (ReplayActor)
    {
        ReplayActor->CycleCameraMode();
        ReplayStatusText = FString::Printf(TEXT("Instant replay - %s - %.2fx"),
            *GetReplayCameraLabel(), ReplayActor->GetPlaybackRate());
    }
}

void ADiscGolfTourGameMode::ToggleReplayPause()
{
    if (!ReplayActor) return;
    if (ReplayActor->IsReplayPaused()) ReplayActor->ResumeReplay();
    else ReplayActor->PauseReplay();
    ReplayStatusText = FString::Printf(TEXT("Instant replay - %s - %.2fx"),
        ReplayActor->IsReplayPaused() ? TEXT("PAUSED") : TEXT("PLAYING"),
        ReplayActor->GetPlaybackRate());
}

void ADiscGolfTourGameMode::SeekReplayRelative(float DeltaSeconds)
{
    if (!ReplayActor || !FMath::IsFinite(DeltaSeconds)) return;
    ReplayActor->SeekReplay(ReplayActor->GetPlaybackTimeSeconds() + DeltaSeconds);
    ReplayStatusText = FString::Printf(TEXT("Instant replay - %.1f / %.1f s - %.2fx"),
        ReplayActor->GetPlaybackTimeSeconds(), ReplayActor->GetDurationSeconds(),
        ReplayActor->GetPlaybackRate());
}

void ADiscGolfTourGameMode::CycleReplayPlaybackRate()
{
    if (!ReplayActor) return;
    const float NewRate = ReplayActor->CyclePlaybackRate();
    ReplayStatusText = FString::Printf(TEXT("Instant replay - %.2fx - %s"),
        NewRate, ReplayActor->IsReplayPaused() ? TEXT("PAUSED") : TEXT("PLAYING"));
}

FString ADiscGolfTourGameMode::GetReplayCameraLabel() const
{
    return ReplayActor && ReplayActor->GetCameraMode() == EDiscReplayCameraMode::Tee
        ? TEXT("TEE CAMERA") : TEXT("TRACKING CAMERA");
}

void ADiscGolfTourGameMode::ToggleInstantReplay()
{
    if (ReplayActor)
    {
        const float ReplayTime = ReplayActor->GetProgress01() * ReplayActor->GetDurationSeconds();
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveReplay(false),
            ReplayActor->GetActorLocation(), ReplayTime, true);
        StopInstantReplay(true);
        ReplayStatusText = TEXT("Replay cancelled");
        return;
    }
    const ADiscGolfTourPlayerController* TourPC = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    if (bMainMenuVisible || ActiveDisc || bRegressionActive || bLieTransitionActive || bScorecardVisible
        || bHoleIntroActive || IsCourseFlyoverActive() || BroadcastCameraDirector
        || (TourPC && (TourPC->IsCharacterCreatorOpen() || TourPC->IsControlsMenuOpen())))
    {
        ReplayStatusText = TEXT("Replay unavailable while another presentation is active");
        return;
    }
    if (const ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(
            UGameplayStatics::GetPlayerPawn(this, 0));
        Golfer && Golfer->IsAnimatedThrowActive())
    {
        ReplayStatusText = TEXT("Replay unavailable while a character throw is active");
        return;
    }
    if (LastReplaySamples.Num() < 2 || !GetWorld())
    {
        ReplayStatusText = TEXT("Complete a shot before starting replay");
        return;
    }

    FString AcquireError;
    if (!DiscGolfCameraViewContract::TryAcquire(
        CameraViewState,
        EDiscGolfCameraViewOwner::Replay,
        EDiscGolfCameraViewMode::ShotReplay,
        ReplayCameraViewToken,
        AcquireError))
    {
        ReplayStatusText = FString::Printf(TEXT("Replay camera blocked - %s"), *AcquireError);
        return;
    }

    ReplayActor = GetWorld()->SpawnActor<ADiscReplayActor>(
        LastReplaySamples[0].WorldLocationCm,
        FRotator::ZeroRotator);
    const FDiscGolfPlayerSettings Settings = GetPlayerSettings();
    if (!ReplayActor || !ReplayActor->InitializeReplay(LastReplaySamples, Settings.ReplaySpeed))
    {
        if (ReplayActor) ReplayActor->Destroy();
        ReplayActor = nullptr;
        ReplayStatusText = TEXT("Replay could not start from the recorded samples");
        FString ReleaseError;
        DiscGolfCameraViewContract::TryRelease(
            CameraViewState, ReplayCameraViewToken, ReleaseError);
        ReplayCameraViewToken = FDiscGolfCameraViewToken();
        ReturnCameraToPlayer();
        return;
    }
    ReplayActor->SetReducedMotion(Settings.bReducedMotion);

    ReplayActor->OnReplayFinished.AddDynamic(this, &ADiscGolfTourGameMode::HandleReplayFinished);
    ReplayStatusText = FString::Printf(TEXT("Instant replay - %.2fx - %.1f s recorded"),
        ReplayActor->GetPlaybackRate(), ReplayActor->GetDurationSeconds());
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetViewTargetWithBlend(ReplayActor, Settings.bReducedMotion ? 0.0f : 0.25f);
    }
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveReplay(true),
        LastReplaySamples[0].WorldLocationCm, 0.0f, true);
}

void ADiscGolfTourGameMode::StopInstantReplay(bool bReturnCamera)
{
    if (ReplayActor)
    {
        ReplayActor->OnReplayFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleReplayFinished);
        ReplayActor->Destroy();
        ReplayActor = nullptr;
    }
    FString ReleaseError;
    const bool bReleased = DiscGolfCameraViewContract::TryRelease(
        CameraViewState, ReplayCameraViewToken, ReleaseError);
    ReplayCameraViewToken = FDiscGolfCameraViewToken();
    if (bReturnCamera && bReleased) ReturnCameraToPlayer();
}

void ADiscGolfTourGameMode::HandleReplayFinished(ADiscReplayActor* FinishedReplay)
{
    if (FinishedReplay != ReplayActor) return;
    const float Duration = ReplayActor->GetDurationSeconds();
    StopInstantReplay(true);
    ReplayStatusText = FString::Printf(TEXT("Replay complete - %.1f s - press V / Right Shoulder to watch again"), Duration);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveReplay(false),
        LastReplaySamples.IsEmpty() ? FVector::ZeroVector : LastReplaySamples.Last().WorldLocationCm,
        Duration, true);
}

void ADiscGolfTourGameMode::HandlePlayabilityFailure(FDGPlayabilityCheckResult Result)
{
    LastPlayabilityFailure = MoveTemp(Result);
    ++PlayabilityFailureCount;
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("PLAYABILITY FAILURE: check=%s gate=%d code=%d blocking=%s state=%s message=%s"),
        *LastPlayabilityFailure.CheckId.ToString(),
        static_cast<int32>(LastPlayabilityFailure.GateLevel),
        static_cast<int32>(LastPlayabilityFailure.FailureCode),
        LastPlayabilityFailure.bBlocking ? TEXT("yes") : TEXT("no"),
        PlayabilityMonitor ? *PlayabilityMonitor->CurrentStateName : TEXT("Unavailable"),
        *LastPlayabilityFailure.Message);
    if (LastPlayabilityFailure.FailureCode == EDGPlayabilityFailureCode::Timeout
        && LastPlayabilityFailure.CheckId == TEXT("DiscInFlight")
        && ActiveDisc
        && ActiveDisc->GetFlightComponent()->IsFlying())
    {
        // The monitor is an independent escape hatch if the solver stops
        // making progress. The normal settled delegate owns lie relief,
        // capture cleanup, camera return, and save persistence.
        ActiveDisc->GetFlightComponent()->StopFlight(true);
    }
}

float ADiscGolfTourGameMode::GetReplayProgress01() const
{
    return ReplayActor ? ReplayActor->GetProgress01() : 0.0f;
}

float ADiscGolfTourGameMode::GetReplayDurationSeconds() const
{
    if (ReplayActor) return ReplayActor->GetDurationSeconds();
    return LastReplaySamples.Num() >= 2
        ? LastReplaySamples.Last().TimeSeconds - LastReplaySamples[0].TimeSeconds
        : 0.0f;
}

float ADiscGolfTourGameMode::GetReplayPlaybackRate() const
{
    return ReplayActor ? ReplayActor->GetPlaybackRate() : GetPlayerSettings().ReplaySpeed;
}

FString ADiscGolfTourGameMode::GetPresentationStatusText() const
{
    if (ReplayActor)
    {
        return FString::Printf(TEXT("INSTANT REPLAY  %3.0f%%  |  %.2fx  |  %s  |  V / Right Shoulder cancel"),
            ReplayActor->GetProgress01() * 100.0f, ReplayActor->GetPlaybackRate(),
            ReplayActor->IsReplayPaused() ? TEXT("PAUSED") : TEXT("PLAYING"));
    }
    return FString::Printf(TEXT("TRACER %s  |  %s  |  [T toggle | V replay]"),
        bShotTracerEnabled ? TEXT("ON") : TEXT("OFF"), *ReplayStatusText);
}

FString ADiscGolfTourGameMode::GetBroadcastCameraStatusText() const
{
    if (BroadcastCameraDirector)
    {
        return BroadcastCameraDirector->GetStatusText();
    }
    if (ReplayActor)
    {
        return TEXT("CAMERA REPLAY CHASE  |  RECORDED PATH");
    }
    return TEXT("CAMERA AUTO READY  |  LAUNCH > FAIRWAY TRACK > BASKET / LANDING");
}

void ADiscGolfTourGameMode::DrawShotTracer() const
{
    if (!bShotTracerEnabled || !GetWorld()) return;
    const TArray<FDiscTrajectorySample>* Samples = nullptr;
    if (ActiveDisc && ActiveDisc->GetFlightComponent())
    {
        Samples = &ActiveDisc->GetFlightComponent()->GetTrajectorySamples();
    }
    else if (LastReplaySamples.Num() >= 2)
    {
        Samples = &LastReplaySamples;
    }
    if (!Samples || Samples->Num() < 2) return;

    TArray<int32> Indices;
    DiscGolfPresentationMath::BuildTracerSampleIndices(*Samples, Indices);
    const FColor PreferredTracerColor = DiscGolfPlayerExperience::ResolveTracerColor(
        GetPlayerSettings().TracerColorPreset).ToFColor(true);
    for (int32 PointIndex = 1; PointIndex < Indices.Num(); ++PointIndex)
    {
        const FDiscTrajectorySample& A = (*Samples)[Indices[PointIndex - 1]];
        const FDiscTrajectorySample& B = (*Samples)[Indices[PointIndex]];
        const bool bGround = A.GroundState != EDiscGroundState::Airborne
            || B.GroundState != EDiscGroundState::Airborne;
        const FColor Color = bGround
            ? FColor(255, 145, 32)
            : PreferredTracerColor;
        DrawDebugLine(GetWorld(), A.WorldLocationCm, B.WorldLocationCm, Color, false, 0.0f, 0, bGround ? 5.0f : 4.0f);
    }

    if (!ActiveDisc)
    {
        const FDiscTrajectorySample& Final = Samples->Last();
        DrawDebugSphere(GetWorld(), Final.WorldLocationCm, 13.0f, 10,
            Final.GroundContactCount > 0 ? FColor(255, 145, 32) : FColor(90, 255, 120),
            false, 0.0f, 0, 2.0f);
    }
}

void ADiscGolfTourGameMode::CyclePhysicsRegressionPreset()
{
    if (bRegressionActive)
    {
        TrajectoryStatusText = TEXT("Regression is already running");
        return;
    }
    const UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    const int32 Count = Trajectories ? Trajectories->GetRegressionPresets().Num() : 0;
    if (Count <= 0)
    {
        TrajectoryStatusText = TEXT("No regression presets are available");
        return;
    }
    SelectedRegressionPresetIndex = (SelectedRegressionPresetIndex + 1) % Count;
    TrajectoryStatusText = FString::Printf(TEXT("SELECTED %s"), *GetSelectedRegressionPresetName());
}

void ADiscGolfTourGameMode::SaveRegressionEnvironment()
{
    if (bRegressionEnvironmentSaved) return;
    if (WindDirector)
    {
        FString WindError;
        if (!WindDirector->ValidatePhysicsWindConfiguration(WindError))
        {
            TrajectoryStatusText = FString::Printf(
                TEXT("Regression rejected invalid source wind: %s"), *WindError);
            UE_LOG(LogDiscGolfTour, Error, TEXT("%s"), *TrajectoryStatusText);
            return;
        }
        SavedBaseWindMps = WindDirector->BaseWindMps;
        SavedGustAmplitudeMps = WindDirector->GustAmplitudeMps;
        SavedGustFrequencyHz = WindDirector->GustFrequencyHz;
    }
    if (const IConsoleVariable* MaxFpsVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
    {
        SavedMaxFps = MaxFpsVariable->GetFloat();
    }
    bRegressionEnvironmentSaved = true;
}

void ADiscGolfTourGameMode::RestoreRegressionEnvironment()
{
    if (!bRegressionEnvironmentSaved) return;
    if (WindDirector)
    {
        FString WindError;
        if (!WindDirector->TryConfigurePhysicsWind(
            SavedBaseWindMps, SavedGustAmplitudeMps,
            SavedGustFrequencyHz, WindError))
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Regression could not restore its validated source wind: %s"),
                *WindError);
        }
    }
    bRegressionEnvironmentSaved = false;
    if (GEngine && GetWorld())
    {
        GEngine->Exec(GetWorld(), *FString::Printf(TEXT("t.MaxFPS %.3f"), SavedMaxFps));
    }
}

void ADiscGolfTourGameMode::StartRegressionPreset(int32 PresetIndex)
{
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories || !Trajectories->GetRegressionPresets().IsValidIndex(PresetIndex))
    {
        TrajectoryStatusText = TEXT("Regression preset index is invalid");
        FinishRegressionRun();
        return;
    }

    const FPhysicsRegressionPreset& Preset = Trajectories->GetRegressionPresets()[PresetIndex];
    ActiveRegressionPresetId = Preset.PresetId;
    ActiveRegressionRenderFps = Preset.RenderFps;
    if (WindDirector)
    {
        FString WindError;
        if (!WindDirector->TryConfigurePhysicsWind(
            Preset.WindMps, 0.0f, WindDirector->GustFrequencyHz, WindError))
        {
            const FString Failure = FString::Printf(
                TEXT("Regression preset %s has invalid wind: %s"),
                *Preset.PresetId.ToString(), *WindError);
            RegressionSuiteSummaries.Add(
                UDiscTrajectorySubsystem::MakeRegressionLaunchFailureSummary(
                    Preset, EDGHandedness::Right, Failure));
            TrajectoryStatusText = Failure;
            UE_LOG(LogDiscGolfTour, Error, TEXT("%s"), *Failure);
            FinishRegressionRun();
            return;
        }
    }
    if (GEngine && GetWorld())
    {
        GEngine->Exec(GetWorld(), *FString::Printf(TEXT("t.MaxFPS %d"), Preset.RenderFps));
    }

    bRegressionInternalReset = true;
    ResetHole();
    bRegressionInternalReset = false;
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer)
    {
        TrajectoryStatusText = TEXT("Regression cannot find the golfer");
        FinishRegressionRun();
        return;
    }

    if (Preset.StartDistanceMeters > 0.0f && ActiveHole)
    {
        const FVector HoleDirection = FVector(
            ActiveHole->BasketLocation.X - ActiveHole->TeeLocation.X,
            ActiveHole->BasketLocation.Y - ActiveHole->TeeLocation.Y,
            0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
        CurrentLieLocation = ActiveHole->BasketLocation - HoleDirection * (Preset.StartDistanceMeters * 100.0f);
        CurrentLieLocation.Z = ActiveHole->BasketLocation.Z;
        MovePlayerToLie(CurrentLieLocation);
        UpdateLieType();
        Golfer->AddActorWorldRotation(FRotator(0.0f, Preset.AimOffsetDeg, 0.0f));
    }

    const bool bSelectedDiagnosticOnly = !bRegressionSuiteActive
        && !Trajectories->AreRegressionPresetsAuthoritative();
    TrajectoryStatusText = FString::Printf(TEXT("%sRUNNING %s"),
        bSelectedDiagnosticOnly ? TEXT("DIAGNOSTIC-ONLY | ") : TEXT(""),
        *Preset.DisplayName.ToString());
    const FThrowCommand Command = Preset.MakeCommand(Golfer->GetActorForwardVector());
    if (!LaunchThrow(Command, nullptr, true))
    {
        const FString Failure = FString::Printf(
            TEXT("Authoritative throw boundary rejected regression preset %s"),
            *Preset.PresetId.ToString());
        RegressionSuiteSummaries.Add(
            UDiscTrajectorySubsystem::MakeRegressionLaunchFailureSummary(
                Preset, Command.Handedness, Failure));
        TrajectoryStatusText = FString::Printf(TEXT("%sREGRESSION FAIL - %s"),
            bSelectedDiagnosticOnly ? TEXT("DIAGNOSTIC-ONLY | ") : TEXT(""), *Failure);
        UE_LOG(LogDiscGolfTour, Error, TEXT("%s"), *TrajectoryStatusText);
        FinishRegressionRun();
    }
}

void ADiscGolfTourGameMode::RunSelectedPhysicsRegression()
{
    if (bRegressionActive || ActiveDisc)
    {
        TrajectoryStatusText = TEXT("Wait for the current throw or regression to finish");
        return;
    }
    if (ActiveHole && ActiveHole->CourseId != TEXT("RegressionCourse") && !LoadCourse(TEXT("Regression")))
    {
        TrajectoryStatusText = TEXT("Regression course could not be loaded");
        return;
    }
    SaveRegressionEnvironment();
    if (!bRegressionEnvironmentSaved)
    {
        if (TrajectoryStatusText.IsEmpty())
        {
            TrajectoryStatusText = TEXT("Regression could not validate its source environment");
        }
        return;
    }
    bRegressionActive = true;
    bRegressionSuiteActive = false;
    RegressionPresentationAudioSequenceBaseline = PresentationAudioEventSequence;
    RegressionSuiteSummaries.Reset();
    StartRegressionPreset(SelectedRegressionPresetIndex);
}

void ADiscGolfTourGameMode::RunPhysicsRegressionSuite()
{
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    const auto BeginCurrentReport = [this, Trajectories](FString& OutFailure)
    {
        LastRegressionReportPath.Reset();
        if (!Trajectories)
        {
            OutFailure = TEXT("Trajectory subsystem is unavailable");
            return false;
        }
        FString ReportError;
        if (!Trajectories->BeginRegressionSuiteReport(
                LastRegressionReportPath, ReportError))
        {
            OutFailure = FString::Printf(
                TEXT("Regression report preflight failed: %s"), *ReportError);
            return false;
        }
        OutFailure.Reset();
        return true;
    };

    if (bRegressionActive || ActiveDisc)
    {
        const FString Failure = TEXT("Wait for the current throw or regression to finish");
        TrajectoryStatusText = Failure;
        if (bRegressionSuiteSmokeTestActive)
        {
            FailRegressionSuiteSmoke(Failure);
        }
        return;
    }

    RegressionSuiteSummaries.Reset();
    if (!bRegressionSuiteSmokeTestActive)
    {
        FString MarkerFailure;
        if (!BeginCurrentReport(MarkerFailure))
        {
            TrajectoryStatusText = MarkerFailure;
            FailRegressionSuiteSmoke(MarkerFailure);
            return;
        }
    }

    if (ActiveHole && ActiveHole->CourseId != TEXT("RegressionCourse") && !LoadCourse(TEXT("Regression")))
    {
        const FString Failure = TEXT("Regression course could not be loaded");
        TrajectoryStatusText = Failure;
        FailRegressionSuiteSmoke(Failure);
        return;
    }
    if (!Trajectories || Trajectories->GetRegressionPresets().IsEmpty())
    {
        const FString Failure = TEXT("No regression presets are available");
        TrajectoryStatusText = Failure;
        FailRegressionSuiteSmoke(Failure);
        return;
    }

    SaveRegressionEnvironment();
    if (!bRegressionEnvironmentSaved)
    {
        const FString Failure = TrajectoryStatusText.IsEmpty()
            ? TEXT("Regression could not validate its source environment")
            : TrajectoryStatusText;
        FailRegressionSuiteSmoke(Failure);
        return;
    }
    bRegressionActive = true;
    bRegressionSuiteActive = true;
    RegressionPresentationAudioSequenceBaseline = PresentationAudioEventSequence;
    RegressionQueue.Reset();
    for (int32 Index = 0; Index < Trajectories->GetRegressionPresets().Num(); ++Index)
    {
        RegressionQueue.Add(Index);
    }
    RegressionQueuePosition = 0;
    StartRegressionPreset(RegressionQueue[0]);
}

void ADiscGolfTourGameMode::FinishRegressionRun()
{
    if (!bRegressionActive) return;
    if (bRegressionSuiteActive)
    {
        ++RegressionQueuePosition;
        GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &ADiscGolfTourGameMode::AdvanceRegressionSuite));
        return;
    }

    bRegressionActive = false;
    ActiveRegressionPresetId = NAME_None;
    ActiveRegressionRenderFps = 0;
    RestoreRegressionEnvironment();
}

void ADiscGolfTourGameMode::FailRegressionSuiteSmoke(const FString& Failure)
{
    if (UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem())
    {
        FString FailureReportPath;
        FString FailureReportError;
        if (Trajectories->WriteRegressionSuiteFailureReport(
                RegressionSuiteSummaries,
                Failure,
                FailureReportPath,
                FailureReportError))
        {
            LastRegressionReportPath = MoveTemp(FailureReportPath);
        }
        else
        {
            LastRegressionReportPath.Reset();
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Regression failure report could not be published: %s"),
                *FailureReportError);
            if (!InvalidateLatestPhysicsRegressionReport())
            {
                UE_LOG(LogDiscGolfTour, Error,
                    TEXT("Prior Latest physics regression report could not be invalidated."));
            }
        }
    }
    else
    {
        LastRegressionReportPath.Reset();
        if (!InvalidateLatestPhysicsRegressionReport())
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("Prior Latest physics regression report could not be invalidated."));
        }
    }

    if (!bRegressionSuiteSmokeTestActive) return;

    bRegressionSuiteSmokeTestActive = false;
    bRegressionActive = false;
    bRegressionSuiteActive = false;
    ActiveRegressionPresetId = NAME_None;
    ActiveRegressionRenderFps = 0;
    RegressionQueue.Reset();
    RegressionQueuePosition = 0;
    RegressionPresentationAudioSequenceBaseline = 0;
    RegressionSuiteSummaries.Reset();
    RestoreRegressionEnvironment();

    TrajectoryStatusText = FString::Printf(
        TEXT("PHYSICS REGRESSION SUITE SMOKE FAIL - %s"), *Failure);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("PHYSICS REGRESSION SUITE SMOKE FAIL: %s"), *Failure);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfTourGameMode::AdvanceRegressionSuite()
{
    if (!bRegressionSuiteActive) return;
    if (RegressionQueue.IsValidIndex(RegressionQueuePosition))
    {
        StartRegressionPreset(RegressionQueue[RegressionQueuePosition]);
        return;
    }

    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    FString Error;
    bool bPassed = false;
    const bool bPresentationTraceUnchanged = IsPresentationSequenceUnchanged(
        RegressionPresentationAudioSequenceBaseline,
        PresentationAudioEventSequence);
    if (!bPresentationTraceUnchanged)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Physics regression presentation isolation failed: event sequence changed from %llu to %llu."),
            static_cast<unsigned long long>(RegressionPresentationAudioSequenceBaseline),
            static_cast<unsigned long long>(PresentationAudioEventSequence));
    }

    const bool bReportWritten = Trajectories && Trajectories->WriteRegressionSuiteReport(
        RegressionSuiteSummaries, bPresentationTraceUnchanged,
        LastRegressionReportPath, bPassed, Error);
    if (bReportWritten)
    {
        TrajectoryStatusText = FString::Printf(TEXT("SUITE %s - %d scenarios - report saved"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"), RegressionSuiteSummaries.Num());
        UE_LOG(LogDiscGolfTour, Display, TEXT("Physics regression suite %s: %s"),
            bPassed ? TEXT("PASSED") : TEXT("FAILED"), *LastRegressionReportPath);
    }
    else
    {
        bPassed = false;
        if (!Trajectories)
        {
            Error = TEXT("Trajectory subsystem is unavailable");
            if (!InvalidateLatestPhysicsRegressionReport())
            {
                Error += TEXT("; prior Latest report could not be invalidated");
            }
            LastRegressionReportPath.Reset();
        }
        else
        {
            FString FailureReportPath;
            FString FailureReportError;
            const FString PublicationFailure = FString::Printf(
                TEXT("Final regression report publication failed: %s"), *Error);
            if (Trajectories->WriteRegressionSuiteFailureReport(
                    RegressionSuiteSummaries,
                    PublicationFailure,
                    FailureReportPath,
                    FailureReportError))
            {
                LastRegressionReportPath = MoveTemp(FailureReportPath);
            }
            else
            {
                LastRegressionReportPath.Reset();
                Error += FString::Printf(
                    TEXT("; terminal failure report also failed: %s"),
                    *FailureReportError);
                if (!InvalidateLatestPhysicsRegressionReport())
                {
                    Error += TEXT("; prior Latest report could not be invalidated");
                }
            }
        }
        TrajectoryStatusText = FString::Printf(TEXT("SUITE REPORT FAILED - %s"), *Error);
    }

    bRegressionActive = false;
    bRegressionSuiteActive = false;
    ActiveRegressionPresetId = NAME_None;
    ActiveRegressionRenderFps = 0;
    RegressionQueue.Reset();
    RegressionQueuePosition = 0;
    RegressionPresentationAudioSequenceBaseline = 0;
    RestoreRegressionEnvironment();
    if (bRegressionSuiteSmokeTestActive)
    {
        bRegressionSuiteSmokeTestActive = false;
        if (bPassed)
        {
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("PHYSICS REGRESSION SUITE SMOKE PASS: %d scenarios."), RegressionSuiteSummaries.Num());
        }
        else
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("PHYSICS REGRESSION SUITE SMOKE FAIL: %s"), *TrajectoryStatusText);
        }
        if (bPassed)
        {
            FPlatformMisc::RequestExit(false);
        }
        else
        {
            FPlatformMisc::RequestExitWithStatus(false, 1);
        }
    }
}
