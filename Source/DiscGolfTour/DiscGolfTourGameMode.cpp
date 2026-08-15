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
#include "DiscGolfSaveGame.h"
#include "DiscBagComponent.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfPresentationMath.h"
#include "DiscGolferPawn.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscTrajectorySubsystem.h"
#include "DiscReplayActor.h"
#include "DiscBroadcastCameraDirector.h"
#include "DiscGolfFlyoverRouteActor.h"
#include "DiscGolfLevelDesignReviewActor.h"
#include "DiscGolfFoliagePresentationActor.h"
#include "DiscGolfTerrainPresentationActor.h"
#include "DiscGolfWaterPresentationActor.h"
#include "DiscGolfCourseSurfaceActor.h"
#include "DiscGolfFixturePresentationActor.h"
#include "DiscGolfFixtureQaRunner.h"
#include "DiscGolfEnvironmentController.h"
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

ADiscGolfTourGameMode::ADiscGolfTourGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    DefaultPawnClass = ADiscGolferPawn::StaticClass();
    PlayerControllerClass = ADiscGolfTourPlayerController::StaticClass();
    HUDClass = ADiscGolfHUD::StaticClass();
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
    PerformanceTracker.Reset();
    PerformanceSummary = FDiscGolfPerformanceSummary();
    PerformanceBudgetState = EDiscGolfPerformanceBudgetState::WarmingUp;
    PerformanceRefreshAccumulator = 0.0f;
    PerformanceStatusText = FString::Printf(TEXT("PERF WARMING 0/%d"),
        PerformanceTracker.GetBudget().MinimumSampleCount);
}

void ADiscGolfTourGameMode::ApplyOmenPerformanceCaptureProfile()
{
    Scalability::FQualityLevels Quality = Scalability::GetQualityLevels();
    Quality.ResolutionQuality = 100.0f;
    Quality.ViewDistanceQuality = 2;
    Quality.AntiAliasingQuality = 2;
    Quality.ShadowQuality = 2;
    Quality.GlobalIlluminationQuality = 2;
    Quality.ReflectionQuality = 2;
    Quality.PostProcessQuality = 2;
    Quality.TextureQuality = 2;
    Quality.EffectsQuality = 2;
    // Keep the authored High presentation population while the rest of the
    // gameplay preset uses the documented moderate Omen settings.
    Quality.FoliageQuality = 3;
    Quality.ShadingQuality = 2;
    Scalability::SetQualityLevels(Quality, true);
    PerformanceCaptureProfile = TEXT("OmenGameplay1080pHighFoliageV1");
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Performance profile applied: %s (resolution=100, quality=2, foliage=3)."),
        *PerformanceCaptureProfile);
}

bool ADiscGolfTourGameMode::CapturePerformanceSnapshot()
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
    const bool bSaved = CapturePerformanceSnapshot();
    const bool bPassed = bSaved && PerformanceBudgetState == EDiscGolfPerformanceBudgetState::Pass;
    UE_LOG(LogDiscGolfTour, Display, TEXT("PERFORMANCE CAPTURE %s: %s"),
        bPassed ? TEXT("PASS") : TEXT("FAIL"), *PerformanceStatusText);
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::StartPerformanceCaptureSamples()
{
    if (!bPerformanceCaptureActive) return;
    ResetPerformanceTelemetry();
    FTimerHandle PerformanceCaptureTimer;
    GetWorldTimerManager().SetTimer(PerformanceCaptureTimer, FTimerDelegate::CreateUObject(
        this, &ADiscGolfTourGameMode::FinishPerformanceCapture),
        PerformanceCaptureDurationSeconds, false);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Performance capture sampling started for %.1f seconds after 10.0 seconds warm-up."),
        PerformanceCaptureDurationSeconds);
}

void ADiscGolfTourGameMode::BeginPlay()
{
    Super::BeginPlay();

    bDeveloperHudVisible = FParse::Param(FCommandLine::Get(), TEXT("DeveloperHUD"));
    bShotTracerEnabled = bDeveloperHudVisible
        || FParse::Param(FCommandLine::Get(), TEXT("ShotTracer"));
    float RequestedPerformanceCaptureSeconds = 0.0f;
    const bool bPerformanceCaptureRequested = FParse::Value(
        FCommandLine::Get(), TEXT("PerformanceCaptureSeconds="), RequestedPerformanceCaptureSeconds);
    if (bPerformanceCaptureRequested)
    {
        PerformanceCaptureDurationSeconds = FMath::Clamp(
            RequestedPerformanceCaptureSeconds, 5.0f, 300.0f);
        ApplyOmenPerformanceCaptureProfile();
    }
    WindDirector = GetWorld()->SpawnActor<AWindDirector>();
    DevBootstrap = GetWorld()->SpawnActor<ADevCourseBootstrap>();
    const bool bRouteTelemetrySmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("NeedleGateRouteTelemetrySmokeTest"));
    const bool bRouteTelemetryRequested = bRouteTelemetrySmokeRequested || FParse::Param(
        FCommandLine::Get(), TEXT("NeedleGateRouteTelemetry"));
    const bool bGalleryLakeWaterSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("GalleryLakeWaterSmokeTest"));
    const bool bDenseForestSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("DenseForestSmokeTest"));
    const bool bGroundGrassSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("GroundGrassSmokeTest"));
    const bool bHole1FlightRouteSmokeRequested = FParse::Param(
        FCommandLine::Get(), TEXT("Hole1FlightRouteSmokeTest"));
    FString InitialCourse = (bRouteTelemetryRequested || bGalleryLakeWaterSmokeRequested
        || bDenseForestSmokeRequested || bGroundGrassSmokeRequested
        || bHole1FlightRouteSmokeRequested)
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
    if (FParse::Param(FCommandLine::Get(), TEXT("LevelDesignReview")))
    {
        ToggleLevelDesignReview();
    }
    if (FParse::Param(FCommandLine::Get(), TEXT("FixturePresentationGallery")))
    {
        SpawnFixturePresentationGallery();
    }

    FString IgnoredVisualQAName;
    const bool bAutomatedLaunch = FApp::IsUnattended()
        || FParse::Param(FCommandLine::Get(), TEXT("ThreeHoleRoundSmokeTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("CourseSmokeTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("RegressionSuiteSmokeTest"))
        || FParse::Param(FCommandLine::Get(), TEXT("PineRidgePlaySmokeTest"))
        || bHole1FlightRouteSmokeRequested
        || FParse::Param(FCommandLine::Get(), TEXT("FixtureCollisionSmokeTest"))
        || bGalleryLakeWaterSmokeRequested
        || bDenseForestSmokeRequested
        || bGroundGrassSmokeRequested
        || bRouteTelemetryRequested
        || FParse::Value(FCommandLine::Get(), TEXT("VisualQAScreenshot="), IgnoredVisualQAName)
        || bPerformanceCaptureRequested;
    if (!bCourseOverridden && !bHoleOverridden && !bAutomatedLaunch)
    {
        RestorePracticeRoundSnapshot();
    }

    if (UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem())
    {
        TrajectoryStatusText = FString::Printf(TEXT("AUTO EXPORT READY - %d regression presets"),
            Trajectories->GetRegressionPresets().Num());
    }
    if (bRouteTelemetryRequested && !bRouteTelemetrySmokeRequested)
    {
        StartNeedleGateRouteTelemetry(false);
    }

    if (bPerformanceCaptureRequested)
    {
        bPerformanceCaptureActive = true;
        if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr)
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
        return;
    }

    FString VisualQAName;
    if (FParse::Value(FCommandLine::Get(), TEXT("VisualQAScreenshot="), VisualQAName))
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
    else if (FParse::Param(FCommandLine::Get(), TEXT("RegressionSuiteSmokeTest")))
    {
        bRegressionSuiteSmokeTestActive = true;
        FTimerHandle SuiteTimer;
        GetWorldTimerManager().SetTimer(SuiteTimer, FTimerDelegate::CreateUObject(
            this, &ADiscGolfTourGameMode::RunPhysicsRegressionSuite), 0.5f, false);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("PineRidgePlaySmokeTest")))
    {
        bPineRidgePlaySmokeTestActive = true;
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
        && IsLevelDesignReviewVisible()
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
            TEXT("NEEDLE GATE ROUTE TELEMETRY SMOKE FAIL: active=%d hole=%d routes=%d zones=%d attempts=%d target=%d profile=%s review=%d report=%lld."),
            bRouteTelemetryActive ? 1 : 0, ActiveHole ? ActiveHole->HoleNumber : -1,
            RouteTelemetrySession.Routes.Num(), RouteTelemetrySession.LandingZones.Num(),
            RouteTelemetrySession.Attempts.Num(), RouteTelemetrySession.TargetAttemptsPerRoute,
            *RouteTelemetrySession.CollisionProfileId.ToString(), IsLevelDesignReviewVisible() ? 1 : 0,
            ReportSize);
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
    bLevelDesignReviewRequested = true;
    if (DevBootstrap)
    {
        if (ADiscGolfLevelDesignReviewActor* Review = DevBootstrap->GetLevelDesignReview())
        {
            Review->SetReviewVisible(true);
            Review->SetFocusedRoute(RouteTelemetrySession.ActiveRouteId);
        }
    }
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
    if (DevBootstrap)
    {
        if (ADiscGolfLevelDesignReviewActor* Review = DevBootstrap->GetLevelDesignReview())
        {
            Review->SetFocusedRoute(Route->RouteId);
        }
    }
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
    if (DevBootstrap)
    {
        if (ADiscGolfLevelDesignReviewActor* Review = DevBootstrap->GetLevelDesignReview())
        {
            Review->SetFocusedRoute(NAME_None);
        }
    }
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
            if (DevBootstrap)
            {
                if (ADiscGolfLevelDesignReviewActor* Review = DevBootstrap->GetLevelDesignReview())
                {
                    Review->SetFocusedRoute(Next->RouteId);
                }
            }
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

bool ADiscGolfTourGameMode::CanPlayerThrow() const
{
    return DiscGolfGameplayGate::CanLaunchThrow(
        ActiveHole != nullptr, ActiveDisc != nullptr, ReplayActor != nullptr,
        IsCourseFlyoverActive(), bHoleComplete, bScorecardVisible, bHoleIntroActive)
        && !bRegressionActive && !bLieTransitionActive;
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
    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap->GetFlyoverRoute()) Route->StopPreview();

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
        NewHole = DevBootstrap->BuildPersistentPineRidgeCourse(ActiveHoleDefinitions, Error);
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
        if (ADiscGolfEnvironmentController* Environment =
            DevBootstrap->GetEnvironmentController())
        {
            Environment->SynchronizeWindDirector(WindDirector);
        }
        WindDirector->RefreshCourseZones();
    }
    EnsurePlayerPawn();
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
    const int32 PreviousHoleNumber = ActiveHole ? ActiveHole->HoleNumber : 0;
    StopInstantReplay(false);
    StopBroadcastCamera(false);
    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap->GetFlyoverRoute()) Route->StopPreview();
    if (ActiveDisc)
    {
        ActiveDisc->Destroy();
        ActiveDisc = nullptr;
    }
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
    if (bLevelDesignReviewRequested && DevBootstrap)
    {
        if (ADiscGolfLevelDesignReviewActor* Review = DevBootstrap->GetLevelDesignReview())
        {
            Review->SetReviewVisible(true);
            if (bRouteTelemetryActive && ActiveHole->HoleNumber == 2)
            {
                Review->SetFocusedRoute(RouteTelemetrySession.ActiveRouteId);
            }
        }
    }
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
}

void ADiscGolfTourGameMode::RestartRound()
{
    if (ActiveHoleDefinitions.IsEmpty() || ActiveCourseManifest.CourseId.IsNone()) return;
    FString Error;
    if (!DiscGolfRound::Initialize(RoundState, ActiveCourseManifest.CourseId,
        ActiveCourseManifest.LayoutId, ActiveCourseManifest.DisplayName, ActiveHoleDefinitions, Error)
        || !LoadRoundHoleByIndex(0, Error))
    {
        CourseStatusText = FString::Printf(TEXT("ROUND RESTART FAILED - %s"), *Error);
        return;
    }
    bScorecardVisible = false;
}

void ADiscGolfTourGameMode::ToggleScorecard()
{
    if (!HasAuthoredRound())
    {
        CourseStatusText = TEXT("SCORECARD IS AVAILABLE IN THE PINE RIDGE ROUND");
        return;
    }
    bScorecardVisible = !bScorecardVisible;
    if (bScorecardVisible)
    {
        if (ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0)))
        {
            Golfer->CancelThrowPresentation();
        }
    }
}

void ADiscGolfTourGameMode::ToggleDeveloperHud()
{
    bDeveloperHudVisible = !bDeveloperHudVisible;
    UE_LOG(LogDiscGolfTour, Display, TEXT("Developer HUD %s."),
        bDeveloperHudVisible ? TEXT("enabled") : TEXT("hidden"));
}

void ADiscGolfTourGameMode::SkipCurrentPresentation()
{
    if (bHoleIntroActive)
    {
        FinishHoleIntroduction();
        return;
    }

    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr;
        IsValid(Route) && Route->IsPreviewing())
    {
        Route->StopPreview();
        Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
        ReturnCameraToPlayer();
        CourseStatusText = ActiveHole
            ? FString::Printf(TEXT("%s | FLYOVER SKIPPED"), *ActiveHole->GetCourseSummary())
            : TEXT("FLYOVER SKIPPED");
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveFlyover(false),
            ActiveHole ? ActiveHole->BasketLocation : FVector::ZeroVector);
    }
}

void ADiscGolfTourGameMode::ToggleLevelDesignReview()
{
    ADiscGolfLevelDesignReviewActor* Review = DevBootstrap
        ? DevBootstrap->GetLevelDesignReview() : nullptr;
    if (!Review)
    {
        CourseStatusText = TEXT("LEVEL DESIGN REVIEW IS AVAILABLE ON AUTHORED HOLES");
        return;
    }
    bLevelDesignReviewRequested = !Review->IsReviewVisible();
    Review->SetReviewVisible(bLevelDesignReviewRequested);
    CourseStatusText = FString::Printf(TEXT("LEVEL DESIGN REVIEW %s // %d STRATEGY ROUTES"),
        Review->IsReviewVisible() ? TEXT("ON") : TEXT("OFF"), Review->GetRouteCount());
    UE_LOG(LogDiscGolfTour, Display, TEXT("Level-design review %s with %d authored routes."),
        Review->IsReviewVisible() ? TEXT("enabled") : TEXT("hidden"), Review->GetRouteCount());
}

bool ADiscGolfTourGameMode::IsLevelDesignReviewVisible() const
{
    const ADiscGolfLevelDesignReviewActor* Review = DevBootstrap
        ? DevBootstrap->GetLevelDesignReview() : nullptr;
    return IsValid(Review) && Review->IsReviewVisible();
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
    if (ActiveDisc || bRegressionActive || ReplayActor)
    {
        CourseStatusText = TEXT("FLYOVER BLOCKED - WAIT FOR CURRENT PRESENTATION");
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
    FinishHoleIntroduction();
    StopBroadcastCamera(false);
    Route->OnFlyoverFinished.RemoveDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
    Route->OnFlyoverFinished.AddDynamic(this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
    if (Route->StartPreview(PC))
    {
        CourseStatusText = FString::Printf(TEXT("COURSE FLYOVER | %d POINTS | %.0f M ROUTE"),
            Route->GetPointCount(), Route->GetRouteLengthCm() / 100.0f);
        RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveFlyover(true),
            ActiveHole ? ActiveHole->TeeLocation : FVector::ZeroVector);
    }
    else
    {
        CourseStatusText = TEXT("FLYOVER FAILED TO START");
    }
}

void ADiscGolfTourGameMode::HandleFlyoverFinished(ADiscGolfFlyoverRouteActor* FinishedRoute)
{
    if (FinishedRoute) FinishedRoute->OnFlyoverFinished.RemoveDynamic(
        this, &ADiscGolfTourGameMode::HandleFlyoverFinished);
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
    const ADiscGolfLevelDesignReviewActor* Review = DevBootstrap ? DevBootstrap->GetLevelDesignReview() : nullptr;
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
        && Review && Review->GetRouteCount() == 3 && Review->IsCollisionInvariant()
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
            TEXT("PINE RIDGE COURSE SMOKE FAIL: %s | world fixtures=%d grass=%d rock=%d sign=%d contracts=%s | visuals=%d/%d brush=%d invariant=%s | route=%d review=%d invariant=%s previewing=%s"),
            *CourseStatusText, FixtureCount, GrassCount, RockCount, SignCount,
            bFixtureContractsValid ? TEXT("valid") : TEXT("invalid"),
            FixtureVisualCount, 3, BrushVisualInstanceCount,
            bFixtureVisualContractsValid ? TEXT("true") : TEXT("false"),
            Route ? Route->GetPointCount() : -1,
            Review ? Review->GetRouteCount() : -1,
            Review && Review->IsCollisionInvariant() ? TEXT("true") : TEXT("false"),
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
        && Ground->IsCollisionInvariant();
    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("GROUND COVER SMOKE PASS: one persistent %d-triangle course ground, macro span=%d, 1152 feathered fairway, 512 feathered connector plus 36 wear patches/288 triangles, 576 shoreline triangles with %.0fcm erosion variation and 42 rock/96 reed-cluster/384 textured reed-stem/14 deadfall accents at %.0fcm minimum route clearance, %d slope-aligned grass clusters/%d HISM cards/%d logical triangles across 3 wind-reactive species, %d litter clusters/%d triangles across 2 slope/height-aware HISM variants, cull=%d-%dcm max_slope=%.2fdeg, 3 holes, collision invariant."),
            Ground->GetTriangleCount(), Ground->GetGroundMacroVariationSpan(),
            Ground->GetMaxShorelineErosionOffsetCm(),
            Ground->GetMinShorelineAccentRouteClearanceCm(),
            Ground->GetGrassBladeClusterCount(),
            Ground->GetGrassCardInstanceCount(), Ground->GetGrassTriangleCount(),
            Ground->GetLitterClusterCount(), Ground->GetLitterTriangleCount(),
            Ground->GetGroundCoverCullStartDistanceCm(),
            Ground->GetGroundCoverCullEndDistanceCm(),
            Ground->GetMaxGroundCoverSlopeDegrees());
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("GROUND COVER SMOKE FAIL: actors=%d ground=%s ready=%s holes=%d terrain=%d macro=%d erosion=%.0fcm fairway=%d connectors=%d shoreline=%d grass=%d clusters/%d cards/%d triangles species=%d litter=%d/%d variants=%d cull=%d-%d slope=%.2f invariant=%s."),
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
            Ground && Ground->IsCollisionInvariant() ? TEXT("true") : TEXT("false"));
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
    RequestThrow(Command);
}

void ADiscGolfTourGameMode::FinishPineRidgePlaySmokeTest(bool bHoledOut)
{
    if (!bPineRidgePlaySmokeTestActive) return;
    bPineRidgePlaySmokeTestActive = false;
    const FDiscTrajectorySummary Summary = GetLastTrajectorySummary();
    const bool bPassed = ActiveHole && ActiveHole->IsAuthoredBlockout()
        && Strokes == 1 && bHasLastFlightTelemetry
        && Summary.SampleCount >= 100
        && Summary.DurationSeconds > 0.25f && Summary.DurationSeconds <= 30.1f
        && (Summary.GroundContactCount >= 1 || bHoledOut)
        && Summary.FinalWorldLocationCm.Z > -200.0f
        && bPineRidgeSmokeSawAuthoredCamera
        && bPineRidgeSmokeEnteredWindZone
        && WindDirector && WindDirector->GetCourseZoneCount() == 2;
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
            TEXT("PINE RIDGE PLAY SMOKE FAIL: samples=%d duration=%.2f contacts=%d final_z=%.1f camera=%d wind_entered=%d wind_zones=%d."),
            Summary.SampleCount, Summary.DurationSeconds, Summary.GroundContactCount,
            Summary.FinalWorldLocationCm.Z, bPineRidgeSmokeSawAuthoredCamera ? 1 : 0,
            bPineRidgeSmokeEnteredWindZone ? 1 : 0,
            WindDirector ? WindDirector->GetCourseZoneCount() : -1);
    }
    FPlatformMisc::RequestExit(false);
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
    RequestThrow(Command);
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
        WindDirector->BaseWindMps = FVector::ZeroVector;
        WindDirector->GustAmplitudeMps = 0.0f;
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
    RequestThrow(PuttPreset->MakeCommand(Golfer->GetActorForwardVector()));
    if (!ActiveDisc)
    {
        FinishThreeHoleRoundSmokeTest(false, TEXT("smoke putt could not launch"));
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
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}

void ADiscGolfTourGameMode::StartHole()
{
    if (!ActiveHole) return;

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

void ADiscGolfTourGameMode::RequestThrow(const FThrowCommand& Command)
{
    if (!CanPlayerThrow()) return;
    LaunchThrow(Command);
}

void ADiscGolfTourGameMode::LaunchThrow(const FThrowCommand& Command)
{
    // This private path is shared by accepted player throws and trusted regression
    // presets. Fundamental lifecycle guards still prevent replacing an active disc.
    if (!DiscGolfGameplayGate::CanLaunchThrow(
        ActiveHole != nullptr, ActiveDisc != nullptr, ReplayActor != nullptr,
        IsCourseFlyoverActive(), bHoleComplete, bScorecardVisible, bHoleIntroActive))
    {
        return;
    }

    UDiscCatalogSubsystem* Catalog = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDiscCatalogSubsystem>() : nullptr;
    if (!Catalog) return;

    FResolvedDiscDefinition ResolvedDisc;
    if (!Catalog->ResolveDisc(Command.MoldId, Command.Plastic, ResolvedDisc)) return;

    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer) return;

    const FVector SpawnLocation = Golfer->GetActorLocation() + Golfer->GetActorForwardVector() * 70.0f + FVector(0, 0, 35.0f);
    ActiveDisc = GetWorld()->SpawnActor<ADiscActor>(SpawnLocation, Golfer->GetActorRotation());
    if (!ActiveDisc) return;

    ActiveDisc->InitializeDisc(ResolvedDisc, WindDirector);
    ActiveDisc->OnDiscSettled.AddDynamic(this, &ADiscGolfTourGameMode::HandleDiscSettled);
    ActiveDisc->OnDiscHoledOut.AddDynamic(this, &ADiscGolfTourGameMode::HandleDiscHoledOut);

    ThrowStartLieLocation = CurrentLieLocation;
    bRouteTelemetryShotPending = bRouteTelemetryActive && ActiveHole && ActiveHole->HoleNumber == 2
        && Strokes == 0 && CurrentLieType == ELieType::Tee
        && !RouteTelemetrySession.ActiveRouteId.IsNone()
        && DiscGolfRouteTelemetry::CountAttempts(
            RouteTelemetrySession, RouteTelemetrySession.ActiveRouteId)
            < RouteTelemetrySession.TargetAttemptsPerRoute;
    FThrowCommand AuthoredCommand = DiscGolfCourseRules::ApplyLieEffects(Command, CurrentLieState.Effects);
    AuthoredCommand.ShotContext = GetCurrentShotContext();
    LastRelease = DiscGolfMath::ResolveThrowRelease(AuthoredCommand);
    LastRelease.LiePowerMultiplier = CurrentLieState.Effects.PowerMultiplier;
    LastRelease.LieTimingErrorMultiplier = CurrentLieState.Effects.TimingErrorMultiplier;
    bHasLastRelease = true;
    LastReleaseWorldSeconds = GetWorld()->GetTimeSeconds();
    bHasLastFlightTelemetry = false;
    LastPresentationGroundTransitionCount = 0;
    ++PresentationShotSequence;
    ++Strokes;
    LastThrowShotNumber = Strokes;
    ActiveDisc->Throw(LastRelease);
    Golfer->NotifyAuthoritativeRelease(LastRelease);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveThrowRelease(
        LastRelease.Grade, LastRelease.Timing, LastRelease.Quality01, LastRelease.ReleaseSpeedMps),
        SpawnLocation);
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveAirborneFlight(
        LastRelease.ReleaseSpeedMps, LastRelease.SpinRpm, 0.0f), SpawnLocation);
    StartBroadcastCameraForShot(ActiveDisc, SpawnLocation, AuthoredCommand.ShotContext);
}

void ADiscGolfTourGameMode::HandleDiscSettled(ADiscActor* Disc, FVector FinalLocation)
{
    if (Disc != ActiveDisc) return;

    UpdatePresentationAudioEvents();
    LastFlightTelemetry = Disc->GetFlightComponent()->GetTelemetry();
    bHasLastFlightTelemetry = true;
    RecordPresentationAudioEvent(DiscGolfPresentationAudio::ResolveBasketOutcome(
        LastFlightTelemetry.LastBasketContact, LastFlightTelemetry.SpeedMps),
        Disc->GetActorLocation(), LastFlightTelemetry.FlightTimeSeconds, false,
        LastFlightTelemetry.CourseSurface, LastFlightTelemetry.GroundState,
        LastFlightTelemetry.LastBasketContact);
    ResolveSettledLie(Disc, FinalLocation);
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
    RecordHole1FlightRouteResult(false);
    FinishPineRidgePlaySmokeTest(false);
    FinishRegressionRun();
}

void ADiscGolfTourGameMode::FinishLieTransition()
{
    ReturnCameraToPlayer();
    bLieTransitionActive = false;
}

void ADiscGolfTourGameMode::HandleDiscHoledOut(ADiscActor* Disc)
{
    if (Disc != ActiveDisc) return;

    UpdatePresentationAudioEvents();
    LastFlightTelemetry = Disc->GetFlightComponent()->GetTelemetry();
    bHasLastFlightTelemetry = true;
    bHoleComplete = true;
    if (HasAuthoredRound())
    {
        FString RoundError;
        if (!DiscGolfRound::RecordCurrentHole(RoundState, Strokes, PenaltyStrokes, RoundError))
        {
            UE_LOG(LogDiscGolfTour, Error, TEXT("Round score could not be recorded: %s"), *RoundError);
        }
        else
        {
            UE_LOG(LogDiscGolfTour, Display, TEXT("Round score recorded: hole %d, %d strokes, round %s."),
                ActiveHole ? ActiveHole->HoleNumber : 0, Strokes,
                *DiscGolfRound::ScoreLabel(DiscGolfRound::ScoreToPar(RoundState)));
        }
        UpdateCourseStatus();
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

    // A tree trunk can block the vertical visibility trace before the terrain.
    // Ignore unrelated blockers one actor at a time until typed/tagged/material
    // course collision is reached.
    for (int32 Attempt = 0; Attempt < 16; ++Attempt)
    {
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params)) return false;
        if (DiscGolfCourseRules::HasSurfaceIdentity(Hit.GetActor(), Hit.PhysMaterial.Get()))
        {
            OutSurface = DiscGolfCourseRules::ResolveSurface(Hit.GetActor(), Hit.PhysMaterial.Get());
            OutGroundLocationCm = Hit.ImpactPoint;
            return true;
        }
        if (!Hit.GetActor()) return false;
        Params.AddIgnoredActor(Hit.GetActor());
    }
    return false;
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
        TraceCourseSurfaceAtLocation(Samples[PriorIndex].WorldLocationCm, Surface, GroundPoint);
        if (DiscGolfCourseRules::IsPenaltySurface(Surface))
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
                TraceCourseSurfaceAtLocation(Midpoint, MidSurface, MidGround);
                if (DiscGolfCourseRules::IsPenaltySurface(MidSurface)) OutsidePoint = MidGround;
                else InsidePoint = MidGround;
            }

            FVector Relief = DiscGolfCourseRules::ReliefPointInsideBoundary(InsidePoint, OutsidePoint);
            ECourseSurfaceType ReliefSurface = ECourseSurfaceType::Fairway;
            FVector ReliefGround = Relief;
            if (TraceCourseSurfaceAtLocation(Relief, ReliefSurface, ReliefGround)
                && !DiscGolfCourseRules::IsPenaltySurface(ReliefSurface))
            {
                Relief = ReliefGround;
            }
            return Relief;
        }
        if (PriorIndex == 0) break;
    }
    return FallbackLocationCm;
}

void ADiscGolfTourGameMode::ResolveSettledLie(ADiscActor* Disc, const FVector& FinalLocation)
{
    if (!Disc || !ActiveHole) return;

    ECourseSurfaceType SurfaceAtRest = LastFlightTelemetry.CourseSurface;
    FVector GroundLocation = FinalLocation;
    TraceCourseSurfaceAtLocation(FinalLocation, SurfaceAtRest, GroundLocation);
    const FVector LastInBounds = SurfaceAtRest == ECourseSurfaceType::OutOfBounds
        ? FindLastInBoundsRelief(Disc->GetFlightComponent()->GetTrajectorySamples(), ThrowStartLieLocation)
        : FinalLocation;

    CurrentLieState = DiscGolfCourseRules::ResolveLie(
        SurfaceAtRest, FinalLocation, LastInBounds, ActiveHole->BasketLocation);
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
    if (bRegressionActive) return;
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

    Strokes = Profile->PracticeStrokes;
    PenaltyStrokes = Profile->PracticePenaltyStrokes;
    bHoleComplete = Profile->bPracticeHoleComplete;
    CurrentLieState = Profile->PracticeLieState;
    CurrentLieLocation = CurrentLieState.LieLocationCm;
    ThrowStartLieLocation = CurrentLieLocation;
    CurrentLieType = CurrentLieState.LieType;
    LieRulesStatusText = FString::Printf(TEXT("RESTORED - %s - %s"),
        *DiscGolfCourseRules::SurfaceName(CurrentLieState.PlayingSurface),
        *DiscGolfCourseRules::LieEffectsText(CurrentLieState.Effects));
    MovePlayerToLie(CurrentLieLocation);
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
            PC->SetViewTargetWithBlend(Pawn, 0.45f);
        }
    }
}

void ADiscGolfTourGameMode::ResetHole()
{
    if (RoundState.bRoundComplete && ActiveDisc == nullptr && !bRegressionActive)
    {
        RestartRound();
        return;
    }

    // A player reset during a regression throw must also release the regression
    // state. StartRegressionPreset calls ResetHole before spawning its disc, so
    // the ActiveDisc guard keeps normal suite-to-suite transitions intact.
    const bool bCancelledActiveRegression = bRegressionActive && ActiveDisc != nullptr;
    if (bCancelledActiveRegression)
    {
        bRegressionActive = false;
        bRegressionSuiteActive = false;
        ActiveRegressionPresetId = NAME_None;
        ActiveRegressionRenderFps = 0;
        RegressionQueue.Reset();
        RegressionQueuePosition = 0;
        RegressionSuiteSummaries.Reset();
        RestoreRegressionEnvironment();
    }

    RouteTelemetryAttemptAwaitingScoreIndex = INDEX_NONE;
    bRouteTelemetryShotPending = false;
    StopInstantReplay(false);
    StopBroadcastCamera(false);
    if (ADiscGolfFlyoverRouteActor* Route = DevBootstrap ? DevBootstrap->GetFlyoverRoute() : nullptr)
    {
        Route->StopPreview();
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
    if (!bObFixture && !bHazardFixture)
    {
        TrajectoryStatusText = TEXT("Rules fixture must be OB or Hazard");
        return;
    }

    if (ActiveHole->CourseId != TEXT("RegressionCourse") && !LoadCourse(TEXT("Regression")))
    {
        TrajectoryStatusText = TEXT("Rules fixture could not load the regression course");
        return;
    }

    ResetHole();
    ADiscGolferPawn* Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    if (!Golfer) return;

    const FVector Direction = bObFixture ? FVector(0.0f, -1.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
    CurrentLieLocation = bObFixture
        ? FVector(6200.0f, -2450.0f, 0.0f)
        : FVector(8300.0f, 2900.0f, 0.0f);
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
        : TEXT("RUNNING HAZARD RULES FIXTURE");
    RequestThrow(Command);
}

void ADiscGolfTourGameMode::StartBroadcastCameraForShot(
    ADiscActor* Disc,
    const FVector& ReleaseLocationCm,
    EDiscShotContext ShotContext)
{
    StopBroadcastCamera(false);
    if (!GetWorld() || !Disc || !ActiveHole) return;

    BroadcastCameraDirector = GetWorld()->SpawnActor<ADiscBroadcastCameraDirector>(
        ReleaseLocationCm, FRotator::ZeroRotator);
    if (!BroadcastCameraDirector || !BroadcastCameraDirector->InitializeForShot(
        Disc, ReleaseLocationCm, ActiveHole->BasketLocation, ShotContext))
    {
        if (BroadcastCameraDirector) BroadcastCameraDirector->Destroy();
        BroadcastCameraDirector = nullptr;
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            PC->SetViewTargetWithBlend(Disc, 0.20f);
        }
        return;
    }

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetViewTargetWithBlend(BroadcastCameraDirector, 0.16f);
    }
}

void ADiscGolfTourGameMode::StopBroadcastCamera(bool bPreserveForCameraBlend)
{
    if (!BroadcastCameraDirector) return;

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
        TrajectoryStatusText = FString::Printf(TEXT("EXPORT FAILED - %s"), *Error);
        UE_LOG(LogDiscGolfTour, Error, TEXT("Trajectory export failed: %s"), *Error);
        return;
    }

    if (bRegressionActive)
    {
        RegressionSuiteSummaries.Add(Summary);
        TrajectoryStatusText = FString::Printf(TEXT("%s - %s | carry %.2f m | apex %.2f m"),
            Summary.bRegressionPassed ? TEXT("REGRESSION PASS") : TEXT("REGRESSION FAIL"),
            *Summary.PresetId.ToString(), Summary.FinalCarryMeters, Summary.ApexMeters);
    }
    else
    {
        TrajectoryStatusText = FString::Printf(TEXT("EXPORTED %d samples | air %.1f m | final %.1f m"),
            Summary.SampleCount, Summary.AirCarryMeters, Summary.FinalCarryMeters);
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
    if (!Flight || Flight->GetTrajectorySamples().Num() < 2) return;
    DiscGolfPlayerExperience::BuildBoundedReplaySamples(
        Flight->GetTrajectorySamples(), LastReplaySamples);
    LastReplayDisc = Disc->GetResolvedDisc();
    LastReplayRelease = LastRelease;
    const float Duration = LastReplaySamples.Last().TimeSeconds - LastReplaySamples[0].TimeSeconds;
    ReplayStatusText = FString::Printf(TEXT("Replay ready - %.1f s - %d solver samples"),
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
    if (ReplayActor) ReplayActor->CycleCameraMode();
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
    if (ActiveDisc || bRegressionActive)
    {
        ReplayStatusText = TEXT("Replay unavailable while a shot or suite is running");
        return;
    }
    if (LastReplaySamples.Num() < 2 || !GetWorld())
    {
        ReplayStatusText = TEXT("Complete a shot before starting replay");
        return;
    }

    ReplayActor = GetWorld()->SpawnActor<ADiscReplayActor>(
        LastReplaySamples[0].WorldLocationCm,
        FRotator::ZeroRotator);
    if (!ReplayActor || !ReplayActor->InitializeReplay(LastReplaySamples, 0.75f))
    {
        if (ReplayActor) ReplayActor->Destroy();
        ReplayActor = nullptr;
        ReplayStatusText = TEXT("Replay could not start from the recorded samples");
        ReturnCameraToPlayer();
        return;
    }

    ReplayActor->OnReplayFinished.AddDynamic(this, &ADiscGolfTourGameMode::HandleReplayFinished);
    ReplayStatusText = FString::Printf(TEXT("Instant replay - %.2fx - %.1f s recorded"),
        ReplayActor->GetPlaybackRate(), ReplayActor->GetDurationSeconds());
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->SetViewTargetWithBlend(ReplayActor, 0.25f);
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
    if (bReturnCamera) ReturnCameraToPlayer();
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
    return ReplayActor ? ReplayActor->GetPlaybackRate() : 0.75f;
}

FString ADiscGolfTourGameMode::GetPresentationStatusText() const
{
    if (ReplayActor)
    {
        return FString::Printf(TEXT("INSTANT REPLAY  %3.0f%%  |  %.2fx  |  V / Right Shoulder cancel"),
            ReplayActor->GetProgress01() * 100.0f, ReplayActor->GetPlaybackRate());
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
    const bool bReplaying = ReplayActor != nullptr;
    for (int32 PointIndex = 1; PointIndex < Indices.Num(); ++PointIndex)
    {
        const FDiscTrajectorySample& A = (*Samples)[Indices[PointIndex - 1]];
        const FDiscTrajectorySample& B = (*Samples)[Indices[PointIndex]];
        const bool bGround = A.GroundState != EDiscGroundState::Airborne
            || B.GroundState != EDiscGroundState::Airborne;
        const FColor Color = bGround
            ? FColor(255, 145, 32)
            : bReplaying
                ? FColor(255, 220, 70)
                : FColor(55, 225, 255);
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
    if (bRegressionEnvironmentSaved || !WindDirector) return;
    SavedBaseWindMps = WindDirector->BaseWindMps;
    SavedGustAmplitudeMps = WindDirector->GustAmplitudeMps;
    if (const IConsoleVariable* MaxFpsVariable = IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))
    {
        SavedMaxFps = MaxFpsVariable->GetFloat();
    }
    bRegressionEnvironmentSaved = true;
}

void ADiscGolfTourGameMode::RestoreRegressionEnvironment()
{
    if (bRegressionEnvironmentSaved && WindDirector)
    {
        WindDirector->BaseWindMps = SavedBaseWindMps;
        WindDirector->GustAmplitudeMps = SavedGustAmplitudeMps;
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
        WindDirector->BaseWindMps = Preset.WindMps;
        WindDirector->GustAmplitudeMps = 0.0f;
    }
    if (GEngine && GetWorld())
    {
        GEngine->Exec(GetWorld(), *FString::Printf(TEXT("t.MaxFPS %d"), Preset.RenderFps));
    }

    ResetHole();
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

    TrajectoryStatusText = FString::Printf(TEXT("RUNNING %s"), *Preset.DisplayName.ToString());
    LaunchThrow(Preset.MakeCommand(Golfer->GetActorForwardVector()));
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
    bRegressionActive = true;
    bRegressionSuiteActive = false;
    RegressionPresentationAudioTraceBaseline = PresentationAudioEventTrace.Num();
    RegressionSuiteSummaries.Reset();
    StartRegressionPreset(SelectedRegressionPresetIndex);
}

void ADiscGolfTourGameMode::RunPhysicsRegressionSuite()
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
    UDiscTrajectorySubsystem* Trajectories = GetTrajectorySubsystem();
    if (!Trajectories || Trajectories->GetRegressionPresets().IsEmpty())
    {
        TrajectoryStatusText = TEXT("No regression presets are available");
        return;
    }

    SaveRegressionEnvironment();
    bRegressionActive = true;
    bRegressionSuiteActive = true;
    RegressionPresentationAudioTraceBaseline = PresentationAudioEventTrace.Num();
    RegressionQueue.Reset();
    for (int32 Index = 0; Index < Trajectories->GetRegressionPresets().Num(); ++Index)
    {
        RegressionQueue.Add(Index);
    }
    RegressionQueuePosition = 0;
    RegressionSuiteSummaries.Reset();
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
    if (Trajectories && Trajectories->WriteRegressionSuiteReport(
        RegressionSuiteSummaries, LastRegressionReportPath, bPassed, Error))
    {
        TrajectoryStatusText = FString::Printf(TEXT("SUITE %s - %d scenarios - report saved"),
            bPassed ? TEXT("PASS") : TEXT("FAIL"), RegressionSuiteSummaries.Num());
        UE_LOG(LogDiscGolfTour, Display, TEXT("Physics regression suite %s: %s"),
            bPassed ? TEXT("PASSED") : TEXT("FAILED"), *LastRegressionReportPath);
    }
    else
    {
        TrajectoryStatusText = FString::Printf(TEXT("SUITE REPORT FAILED - %s"), *Error);
    }

    const bool bPresentationTraceUnchanged =
        PresentationAudioEventTrace.Num() == RegressionPresentationAudioTraceBaseline;
    if (!bPresentationTraceUnchanged)
    {
        bPassed = false;
        TrajectoryStatusText = TEXT("SUITE FAIL - regression emitted presentation audio events");
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Physics regression presentation isolation failed: trace grew from %d to %d."),
            RegressionPresentationAudioTraceBaseline, PresentationAudioEventTrace.Num());
    }

    bRegressionActive = false;
    bRegressionSuiteActive = false;
    ActiveRegressionPresetId = NAME_None;
    ActiveRegressionRenderFps = 0;
    RegressionQueue.Reset();
    RegressionQueuePosition = 0;
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
        FPlatformMisc::RequestExit(false);
    }
}
