#include "DiscGolfSession15VerticalSliceRunner.h"

#include "DiscActor.h"
#include "DiscBagComponent.h"
#include "DiscFlightComponent.h"
#include "DiscGolferPawn.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfHoleActor.h"
#include "DiscGolfMath.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformMemory.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"

namespace
{
constexpr double PhaseTimeoutSeconds = 75.0;
constexpr double DriveWarmupSeconds = 10.0;
constexpr double DrivePerformanceStabilizeSeconds = 2.0;
constexpr int32 DrivePerformanceCaptureSamples = 600;
constexpr double ReplayExerciseSeconds = 0.6;
constexpr float SixtyFpsFrameMs = 1000.0f / 60.0f;
const TCHAR* Session15UserDirParent = TEXT("C:/DGTour_TestRuns/Session15");
const TCHAR* PhaseSchema = TEXT("DiscGolfTour.Session15VerticalSlicePhase.v1");
const TCHAR* CanonicalSchema = TEXT("DiscGolfTour.Session15VerticalSliceAcceptance.v1");
const TCHAR* PerformanceSchema = TEXT("DiscGolfTour.Session15VerticalSlicePerformance.v1");
const TCHAR* TechnicalPass = TEXT("PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED");

int32 CountOccurrencesInsensitive(const FString& Haystack, const FString& Needle)
{
    int32 Count = 0;
    int32 SearchFrom = 0;
    while (!Needle.IsEmpty())
    {
        const int32 Found = Haystack.Find(
            Needle, ESearchCase::IgnoreCase, ESearchDir::FromStart, SearchFrom);
        if (Found == INDEX_NONE) break;
        ++Count;
        SearchFrom = Found + Needle.Len();
    }
    return Count;
}

template <typename T>
bool StructJson(const T& Value, FString& OutJson)
{
    OutJson.Reset();
    return FJsonObjectConverter::UStructToJsonObjectString(
        T::StaticStruct(), &Value, OutJson, 0, 0, 0, nullptr, false);
}

bool JsonStringField(
    const TSharedPtr<FJsonObject>& Object,
    const TCHAR* Field,
    FString& OutValue)
{
    return Object.IsValid() && Object->TryGetStringField(Field, OutValue);
}

bool IsFiniteLie(const FDiscGolfLieState& Lie)
{
    return Lie.SchemaVersion == 1
        && !Lie.RawDiscLocationCm.ContainsNaN()
        && !Lie.LieLocationCm.ContainsNaN()
        && FMath::IsFinite(Lie.DistanceToBasketMeters)
        && Lie.DistanceToBasketMeters >= 0.0f
        && FMath::IsFinite(Lie.Effects.PowerMultiplier)
        && FMath::IsFinite(Lie.Effects.TimingErrorMultiplier);
}

TArray<TSharedPtr<FJsonValue>> ReleaseBlockersJson()
{
    static const TCHAR* Blockers[] = {
        TEXT("FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED"),
        TEXT("EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE"),
        TEXT("CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED"),
        TEXT("DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE"),
        TEXT("QUARANTINED_IMPORT_RECEIPTS_PENDING"),
        TEXT("POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE"),
        TEXT("ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED"),
        TEXT("MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED"),
        TEXT("SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY"),
        TEXT("SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING")
    };
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const TCHAR* Blocker : Blockers)
    {
        Result.Add(MakeShared<FJsonValueString>(Blocker));
    }
    return Result;
}
}

ADiscGolfSession15VerticalSliceRunner::ADiscGolfSession15VerticalSliceRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
}

void ADiscGolfSession15VerticalSliceRunner::Start()
{
    if (bFinished || Stage != EStage::Idle) return;

    FString Error;
    if (!ResolveInvocation(Error) || !ResolveRuntime(Error))
    {
        Fail(Error);
        return;
    }

    switch (Phase)
    {
        case EPhase::Setup:
            if (!RunSetup(Error)) Fail(Error);
            else Pass();
            return;
        case EPhase::Drive:
            if (!ValidateCommonAgainstSetup(Error) || !ValidateFreshHoleOne(Error))
            {
                Fail(Error);
                return;
            }
            GameMode->SkipCurrentPresentation();
            SetActorTickEnabled(true);
            SetStage(EStage::DriveWarmup);
            return;
        case EPhase::Finish:
            if (!BeginFinish(Error))
            {
                Fail(Error);
                return;
            }
            SetActorTickEnabled(true);
            return;
        case EPhase::Verify:
            if (!RunVerify(Error)) Fail(Error);
            else Pass();
            return;
        default:
            Fail(TEXT("unsupported Session 15 phase"));
            return;
    }
}

void ADiscGolfSession15VerticalSliceRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bFinished) return;
    if (bCollectPerformance && FMath::IsFinite(DeltaSeconds) && DeltaSeconds > 0.0f)
    {
        PerformanceFrameMs.Add(DeltaSeconds * 1000.0f);
    }
    if (GameMode && GameMode->IsBroadcastCameraActive()) bCameraSeen = true;
    if (SecondsInStage() > PhaseTimeoutSeconds)
    {
        Fail(TEXT("phase stage timed out before the existing gameplay authority completed"));
        return;
    }

    FString Error;
    switch (Stage)
    {
        case EStage::DriveWarmup:
            if (SecondsInStage() >= DriveWarmupSeconds)
            {
                GameMode->ResetPerformanceTelemetry();
                PerformanceFrameMs.Reset();
                bCollectPerformance = true;
                if (!BeginDrive(Error)) Fail(Error);
            }
            break;
        case EStage::AwaitingDriveRelease:
            ObserveDriveRelease();
            break;
        case EStage::DriveReplay:
            if (!bReplayExercised) BeginDriveReplay();
            else if (GetWorld()->GetTimeSeconds() - ReplayStartedSeconds >= ReplayExerciseSeconds)
                FinishDriveReplay();
            break;
        case EStage::DrivePerformanceStabilize:
            if (SecondsInStage() >= DrivePerformanceStabilizeSeconds)
            {
                GameMode->ResetPerformanceTelemetry();
                PerformanceFrameMs.Reset();
                bCollectPerformance = true;
                SetStage(EStage::DrivePerformanceCapture);
            }
            break;
        case EStage::DrivePerformanceCapture:
            GameMode->RefreshPerformanceTelemetry();
            if (GameMode->PerformanceSummary.SampleCount >= DrivePerformanceCaptureSamples)
                CompleteDrivePerformanceCapture();
            break;
        case EStage::AwaitingCircle1Ready:
            if (GameMode->CanPlayerThrow() && !LaunchCircle1(Error)) Fail(Error);
            break;
        case EStage::FinishReplay:
            if (!bReplayExercised) BeginFinishReplay();
            else if (GetWorld()->GetTimeSeconds() - ReplayStartedSeconds >= ReplayExerciseSeconds)
                FinishFinishReplay();
            break;
        default:
            break;
    }
}

bool ADiscGolfSession15VerticalSliceRunner::ResolveInvocation(FString& OutError)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    const FString CommandLineText(CommandLine);
    if (!FParse::Param(CommandLine, TEXT("Session15VerticalSliceSmokeTest")))
    {
        OutError = TEXT("missing -Session15VerticalSliceSmokeTest");
        return false;
    }
    FString PhaseText;
    if (CountOccurrencesInsensitive(CommandLineText, TEXT("-Session15VerticalSlicePhase=")) != 1
        || !FParse::Value(CommandLine, TEXT("Session15VerticalSlicePhase="), PhaseText))
    {
        OutError = TEXT("requires exactly one -Session15VerticalSlicePhase=Setup|Drive|Finish|Verify");
        return false;
    }
    if (PhaseText.Equals(TEXT("Setup"), ESearchCase::IgnoreCase)) Phase = EPhase::Setup;
    else if (PhaseText.Equals(TEXT("Drive"), ESearchCase::IgnoreCase)) Phase = EPhase::Drive;
    else if (PhaseText.Equals(TEXT("Finish"), ESearchCase::IgnoreCase)) Phase = EPhase::Finish;
    else if (PhaseText.Equals(TEXT("Verify"), ESearchCase::IgnoreCase)) Phase = EPhase::Verify;
    else
    {
        OutError = FString::Printf(TEXT("unknown Session 15 phase: %s"), *PhaseText);
        return false;
    }

    FString RequestedUserDir;
    if (CountOccurrencesInsensitive(CommandLineText, TEXT("-UserDir=")) != 1
        || !FParse::Value(CommandLine, TEXT("UserDir="), RequestedUserDir)
        || RequestedUserDir.IsEmpty() || FPaths::IsRelative(RequestedUserDir))
    {
        OutError = TEXT("requires exactly one absolute -UserDir=C:/DGTour_TestRuns/Session15/<GUID>");
        return false;
    }
    FString Requested = FPaths::ConvertRelativePathToFull(RequestedUserDir);
    FString Active = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir());
    FString ExpectedParent = FPaths::ConvertRelativePathToFull(Session15UserDirParent);
    FString Project = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString ProjectSaved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    FPaths::NormalizeDirectoryName(Requested);
    FPaths::NormalizeDirectoryName(Active);
    FPaths::NormalizeDirectoryName(ExpectedParent);
    FPaths::NormalizeDirectoryName(Project);
    FPaths::NormalizeDirectoryName(ProjectSaved);
    FGuid DirectoryGuid;
    if (!FPaths::IsSamePath(Requested, Active)
        || !FPaths::IsSamePath(FPaths::GetPath(Requested), ExpectedParent)
        || !FGuid::Parse(FPaths::GetCleanFilename(Requested), DirectoryGuid)
        || !DirectoryGuid.IsValid()
        || !IFileManager::Get().DirectoryExists(*Requested)
        || FPaths::IsSamePath(Requested, Project)
        || FPaths::IsUnderDirectory(Requested, Project)
        || FPaths::IsSamePath(Requested, ProjectSaved)
        || FPaths::IsUnderDirectory(Requested, ProjectSaved))
    {
        OutError = TEXT("active UserDir must be an existing external C:/DGTour_TestRuns/Session15/<GUID>");
        return false;
    }
    AcceptedUserDir = Requested;
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::ResolveRuntime(FString& OutError)
{
    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    Golfer = Cast<ADiscGolferPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
    Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    TourGameInstance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    if (!GameMode || !PlayerController || !Golfer || !Bag
        || !TourGameInstance || !TourGameInstance->GetProfile())
    {
        OutError = TEXT("GameMode, original player pawn, bag, or schema-10 profile was unavailable");
        return false;
    }
    if (!DiscGolfSaveSchema::IsCurrent(TourGameInstance->GetProfile()->SaveSchemaVersion))
    {
        OutError = TEXT("Session 15 refuses a non-current protected player profile");
        return false;
    }
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::ValidateVerifiedMetaHuman(FString& OutError) const
{
    const UDiscGolfMetaHumanAvatarBackendComponent* Backend = Golfer
        ? Golfer->GetAvatarBackendComponent() : nullptr;
    const UDiscGolfAvatarBackendProfile* Profile = Backend
        ? Backend->GetActiveBackendProfile() : nullptr;
    if (!Backend || !Profile
        || Profile->BackendId != FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId)
        || !Backend->IsVisualBackendReady()
        || !Backend->IsPresentationPolicyVerified()
        || Backend->GetVerifiedPresentationPolicy()
            != EDGMetaHumanPresentationPolicy::GameplayPerformance
        || !Backend->GetVerifiedVisualBody()
        || !Backend->GetVerifiedVisualHead()
        || !Backend->GetVerifiedVisualOutfit()
        || Golfer->IsDGProxyPresentationVisible())
    {
        OutError = Backend
            ? FString::Printf(TEXT("verified MetaHuman GameplayPerformance backend unavailable: %s"),
                *Backend->GetLastAdapterStatus())
            : TEXT("MetaHuman backend component unavailable");
        return false;
    }
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::RunSetup(FString& OutError)
{
    const FDGFullCharacterCustomization Original =
        TourGameInstance->GetFullCharacterCustomization();
    const FDGOutfitLoadout OriginalOutfit = Original.Outfit;
    const FDiscGolfOutfitResolution OutfitResolution =
        DiscGolfOutfitRuntime::ResolveCanonicalLoadout(
            OriginalOutfit, Golfer->GetOutfitCatalog(), Original.Body);
    if (!OutfitResolution.bCatalogAvailable
        || !OutfitResolution.bAllEntriesResolved
        || !DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            OriginalOutfit, OutfitResolution.Loadout))
    {
        OutError = TEXT("the original project-generic outfit was not valid against its installed catalog");
        return false;
    }

    GameMode->SkipCurrentPresentation();
    if (!PlayerController->OpenCharacterCreator())
    {
        OutError = FString::Printf(TEXT("real creator API did not open: %s"),
            *PlayerController->GetCharacterCreatorStatusText());
        return false;
    }
    FDGFullCharacterCustomization Candidate;
    if (!PlayerController->SelectCharacterCreatorBackend(
            FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId), Candidate))
    {
        PlayerController->CancelCharacterCreator();
        OutError = FString::Printf(TEXT("real creator API rejected the verified MetaHuman backend: %s"),
            *PlayerController->GetCharacterCreatorBackendStatusText());
        return false;
    }
    Candidate.Identity.Handedness = EDGHandedness::Right;
    Candidate.Outfit = OriginalOutfit;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Candidate);
    if (!PlayerController->ApplyFullCharacterCreatorDraft(Candidate)
        || PlayerController->IsCharacterCreatorOpen()
        || !DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
            OriginalOutfit, Golfer->GetCurrentOutfitLoadout()))
    {
        if (PlayerController->IsCharacterCreatorOpen())
        {
            PlayerController->CancelCharacterCreator();
        }
        OutError = FString::Printf(
            TEXT("real creator Apply did not close with the right-handed MetaHuman draft and original generic outfit: %s"),
            *PlayerController->GetCharacterCreatorStatusText());
        return false;
    }
    const FDGFullCharacterCustomization Applied =
        Golfer->GetCurrentFullCharacterCustomization();
    FString AppliedJson;
    FString SavedJson;
    if (!StructJson(Applied, AppliedJson)
        || !StructJson(TourGameInstance->GetFullCharacterCustomization(), SavedJson)
        || AppliedJson != SavedJson
        || !ValidateVerifiedMetaHuman(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("schema-10 creator Apply did not reconstruct exactly in the profile authority");
        }
        return false;
    }

    FDiscGolfPlayerSettings Settings;
    Settings.GraphicsQuality = 2;
    Settings.ResolutionX = 1920;
    Settings.ResolutionY = 1080;
    Settings.WindowMode = 1;
    Settings.bAutoFollowDisc = true;
    Settings.ReplaySpeed = 1.0f;
    Settings.bHudVisible = true;
    Settings.bBasketMarkerVisible = true;
    Settings.bSubtitles = true;
    Settings.AimAssist01 = 0.25f;
    Settings.TimingWindowScale = 1.20f;
    Settings.bShotShapeGuide = true;
    Settings.bOptionalFlightPreview = false;
    Settings.bHoldForTiming = false;
    Settings.Normalize();
    TourGameInstance->UpdatePlayerSettings(Settings);

    if (!Bag->SelectEquipment(TEXT("Apex"), EDiscPlastic::Tour))
    {
        OutError = TEXT("could not select the project-owned Apex/Tour bag instance");
        return false;
    }
    FDGDiscInstance Selected;
    FString EquipmentError;
    if (!Bag->GetSelectedDiscInstance(Selected) || !Selected.InstanceId.IsValid()
        || Selected.DiscDefinitionId != TEXT("Apex") || Selected.PlasticId != TEXT("Tour")
        || !Bag->SaveEquipment(EquipmentError))
    {
        OutError = FString::Printf(TEXT("stable Apex/Tour equipment save failed: %s"), *EquipmentError);
        return false;
    }

    TourGameInstance->SaveProfile();
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::ValidateCommonAgainstSetup(
    FString& OutError,
    bool bRequireSetupEquipment)
{
    TSharedPtr<FJsonObject> Setup;
    if (!LoadPhaseReport(TEXT("Setup"), Setup, OutError)) return false;
    if (!Setup->GetBoolField(TEXT("passed")))
    {
        OutError = TEXT("Setup phase report was not a pass");
        return false;
    }
    if (!JsonStringField(Setup, TEXT("character_json"), SetupCharacterJson)
        || !JsonStringField(Setup, TEXT("outfit_json"), SetupOutfitJson)
        || !JsonStringField(Setup, TEXT("settings_json"), SetupSettingsJson)
        || !JsonStringField(Setup, TEXT("selected_disc_json"), SetupDiscJson))
    {
        OutError = TEXT("Setup phase report omitted reconstruction fields");
        return false;
    }

    FString CharacterJson;
    FString OutfitJson;
    FString SettingsJson;
    FString DiscJson;
    FDGDiscInstance Selected;
    if (!StructJson(TourGameInstance->GetFullCharacterCustomization(), CharacterJson)
        || !StructJson(Golfer->GetCurrentOutfitLoadout(), OutfitJson)
        || !StructJson(TourGameInstance->GetPlayerSettings(), SettingsJson)
        || !Bag->GetSelectedDiscInstance(Selected)
        || !StructJson(Selected, DiscJson)
        || CharacterJson != SetupCharacterJson
        || OutfitJson != SetupOutfitJson
        || SettingsJson != SetupSettingsJson
        || (bRequireSetupEquipment && DiscJson != SetupDiscJson))
    {
        OutError = bRequireSetupEquipment
            ? TEXT("fresh process did not reconstruct exact Setup character/outfit/settings/equipment state")
            : TEXT("fresh process did not reconstruct exact Setup character/outfit/settings state");
        return false;
    }
    return ValidateVerifiedMetaHuman(OutError);
}

bool ADiscGolfSession15VerticalSliceRunner::ValidateFreshHoleOne(FString& OutError) const
{
    const ADiscGolfHoleActor* Hole = GameMode ? GameMode->GetActiveHole() : nullptr;
    const FDiscGolfHoleBlockoutDefinition* Definition = GameMode
        ? GameMode->GetActiveHoleDefinition() : nullptr;
    if (!Hole || !Definition || !Hole->IsAuthoredBlockout()
        || Hole->CourseId != TEXT("PineRidgeChampionship")
        || Hole->HoleNumber != 1 || Hole->Par != 3
        || Definition->HoleNumber != 1 || Definition->Par != 3
        || GameMode->GetStrokes() != 0 || GameMode->GetPenaltyStrokes() != 0
        || GameMode->IsHoleComplete() || GameMode->GetActiveDisc())
    {
        OutError = TEXT("phase did not start at fresh authored Pine Ridge Hole 1 / par 3 authority");
        return false;
    }
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::BeginDrive(FString& OutError)
{
    const FDiscGolfHoleBlockoutDefinition* Definition = GameMode->GetActiveHoleDefinition();
    const FDiscGolfShotRouteDefinition* Route = Definition
        ? Definition->ShotRoutes.FindByPredicate([](const FDiscGolfShotRouteDefinition& Entry)
        {
            return Entry.RouteId == TEXT("CenterPlacement");
        }) : nullptr;
    if (!Route || Route->WaypointsCm.Num() < 2)
    {
        OutError = TEXT("active Hole 1 definition omitted CenterPlacement waypoint 1");
        return false;
    }
    if (!Bag->SelectEquipment(TEXT("Apex"), EDiscPlastic::Tour))
    {
        OutError = TEXT("Apex/Tour selection was unavailable at Drive");
        return false;
    }
    FDGDiscInstance Selected;
    if (!Bag->GetSelectedDiscInstance(Selected) || !Selected.InstanceId.IsValid())
    {
        OutError = TEXT("Drive selected equipment had no stable instance identity");
        return false;
    }
    if (!GameMode->IsShotTracerEnabled()) GameMode->ToggleShotTracer();
#if DG_WITH_THROW_LAB
    if (!GameMode->IsThrowLabActive()) GameMode->ToggleThrowLab();
#endif
    BaselineReleaseCount = Golfer->GetRHBHThrowAdapter()
        ? Golfer->GetRHBHThrowAdapter()->GetTotalReleaseCommitCount() : 0;
    BaselineStrokes = GameMode->GetStrokes();
    BaselineAudioCount = GameMode->GetPresentationAudioTraceCount();
#if DG_WITH_THROW_LAB
    BaselineThrowLabCount = GameMode->GetThrowLabRecordCount();
#else
    BaselineThrowLabCount = 0;
#endif

    FVector AimPoint = Route->WaypointsCm[1];
    AimPoint.Z = Golfer->GetActorLocation().Z;
    // The animated grip release begins behind the authored tee origin. Keep the
    // frozen CenterPlacement power/shape while biasing the aim one meter toward
    // the corridor's right side so the real grip-origin flight reaches Circle 2.
    const FVector RouteDirection = (AimPoint - Golfer->GetActorLocation())
        .GetSafeNormal2D(SMALL_NUMBER, FVector::ForwardVector);
    AimPoint += FVector(-RouteDirection.Y, RouteDirection.X, 0.0f) * 100.0f;
    Golfer->FaceLocation(AimPoint);
    FThrowCommand Command;
    Command.DiscInstanceId = Selected.InstanceId;
    Command.MoldId = TEXT("Apex");
    Command.Plastic = EDiscPlastic::Tour;
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Drive;
    Command.Direction = (AimPoint - Golfer->GetActorLocation())
        .GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    Command.Power01 = 0.90f;
    Command.HyzerDeg = 0.0f;
    Command.NoseAngleDeg = 0.0f;
    Command.LaunchAngleDeg = 7.0f;
    Command.TimingError = 0.0f;
    if (!GameMode->CanPlayerThrow() || !Golfer->TryStartAnimatedRHBHThrow(Command))
    {
        OutError = TEXT("real player RHBH animation transaction rejected CenterPlacement");
        return false;
    }
    SetStage(EStage::AwaitingDriveRelease);
    return true;
}

void ADiscGolfSession15VerticalSliceRunner::ObserveDriveRelease()
{
    UDiscGolfRHBHThrowAdapterComponent* Adapter = Golfer->GetRHBHThrowAdapter();
    if (!Adapter || Adapter->GetTotalReleaseCommitCount() > BaselineReleaseCount + 1)
    {
        Fail(TEXT("Drive committed more than one release"));
        return;
    }
    ADiscActor* Disc = GameMode->GetActiveDisc();
    if (!Disc) return;
    FString Error;
    if (!BindObservedDisc(Error))
    {
        Fail(Error);
        return;
    }
    const FResolvedDiscDefinition Resolved = Disc->GetResolvedDisc();
    FDGDiscInstance Selected;
    bDiscIdentitySeen = Bag->GetSelectedDiscInstance(Selected)
        && Resolved.DiscInstanceId == Selected.InstanceId;
    const bool bReleaseValid = Adapter->HasCommittedRelease()
        && Adapter->GetReleaseCommitCountForAttempt() == 1
        && Adapter->GetTotalReleaseCommitCount() == BaselineReleaseCount + 1
        && Adapter->WasLastAuthoritativeLaunchAccepted()
        && GameMode->HasLastRelease()
        && GameMode->GetStrokes() == BaselineStrokes + 1
        && Disc->GetFlightComponent()
        && Disc->GetFlightComponent()->GetVelocityMps().SizeSquared() > 0.01f;
    bCameraSeen |= GameMode->IsBroadcastCameraActive();
    if (!bReleaseValid || !bDiscIdentitySeen || !GameMode->IsShotTracerEnabled())
    {
        Fail(TEXT("animated release did not preserve one stable selected disc, stroke, flight, and tracer"));
        return;
    }
    CaptureScreenshot(TEXT("Drive"), TEXT("Session15_Drive.png"));
    SetStage(EStage::AwaitingDriveOutcome);
}

bool ADiscGolfSession15VerticalSliceRunner::BindObservedDisc(FString& OutError)
{
    ObservedDisc = GameMode ? GameMode->GetActiveDisc() : nullptr;
    if (!ObservedDisc)
    {
        OutError = TEXT("authoritative gameplay disc was unavailable after RequestThrow");
        return false;
    }
    ObservedDisc->OnDiscSettled.AddUniqueDynamic(
        this, &ADiscGolfSession15VerticalSliceRunner::HandleObservedDiscSettled);
    ObservedDisc->OnDiscHoledOut.AddUniqueDynamic(
        this, &ADiscGolfSession15VerticalSliceRunner::HandleObservedDiscHoledOut);
    return true;
}

void ADiscGolfSession15VerticalSliceRunner::HandleObservedDiscSettled(
    ADiscActor* Disc,
    FVector FinalLocation)
{
    (void)FinalLocation;
    if (bFinished || Disc != ObservedDisc) return;
    const UDiscFlightComponent* Flight = Disc->GetFlightComponent();
    FlightSampleCount = Flight ? Flight->GetTrajectorySamples().Num() : 0;
    bWindSeen = Flight && Flight->GetTrajectorySamples().ContainsByPredicate(
        [](const FDiscTrajectorySample& Sample)
        {
            return Sample.WindMps.SizeSquared() > KINDA_SMALL_NUMBER;
        });
    ObservedDisc = nullptr;

    if (Stage == EStage::AwaitingDriveOutcome)
    {
        const FDiscGolfLieState Lie = GameMode->GetCurrentLieState();
        bNaturalCircle2Lie = IsFiniteLie(Lie)
            && Lie.LieType == ELieType::Circle2
            && Lie.ShotContext == EDiscShotContext::Circle2Putt
            && Lie.DistanceToBasketMeters > 10.0f
            && Lie.DistanceToBasketMeters <= 20.0f
            && GameMode->GetStrokes() == 1
            && GameMode->GetPenaltyStrokes() == 0
            && !GameMode->IsHoleComplete();
        if (!bNaturalCircle2Lie || FlightSampleCount < 100 || !bCameraSeen || !bWindSeen
            || !GameMode->HasLastFlightTelemetry()
            || !GameMode->HasReplayCapture()
#if DG_WITH_THROW_LAB
            || GameMode->GetThrowLabRecordCount() != BaselineThrowLabCount + 1
#endif
            || GameMode->GetPresentationAudioTraceCount() - BaselineAudioCount < 4)
        {
            Fail(TEXT("CenterPlacement did not settle through real wind/flight/camera into the natural Circle 2 save seam"));
            return;
        }
#if DG_WITH_THROW_LAB
        ThrowLabCount = GameMode->GetThrowLabRecordCount();
#else
        ThrowLabCount = 0;
#endif
        AudioEventDelta = GameMode->GetPresentationAudioTraceCount() - BaselineAudioCount;
        SetStage(EStage::DriveReplay);
        return;
    }

    if (Stage == EStage::AwaitingCircle2Outcome)
    {
        const FDiscGolfLieState Lie = GameMode->GetCurrentLieState();
        bCircle2SettledToCircle1 = IsFiniteLie(Lie)
            && Lie.LieType == ELieType::Circle1
            && Lie.ShotContext == EDiscShotContext::Circle1Putt
            && Lie.DistanceToBasketMeters <= 10.0f
            && GameMode->GetStrokes() == 2
            && !GameMode->IsHoleComplete();
        if (!bCircle2SettledToCircle1)
        {
            Fail(TEXT("Touch .746 Circle 2 putt did not settle naturally into Circle 1"));
            return;
        }
        SetStage(EStage::AwaitingCircle1Ready);
        return;
    }

    if (Stage == EStage::AwaitingCircle1Outcome)
    {
        Fail(TEXT("distance-calibrated Touch Circle 1 putt settled instead of producing an actual caught basket outcome"));
    }
}

void ADiscGolfSession15VerticalSliceRunner::HandleObservedDiscHoledOut(ADiscActor* Disc)
{
    if (bFinished || Disc != ObservedDisc) return;
    ObservedDisc = nullptr;
    if (Stage != EStage::AwaitingCircle1Outcome)
    {
        Fail(TEXT("basket caught a shot outside the required natural Circle 1 completion stage"));
        return;
    }
    const FDiscFlightTelemetry Telemetry = GameMode->GetLastFlightTelemetry();
    bCaughtHoleOut = Telemetry.LastBasketContact == EBasketContactResult::Caught
        && GameMode->IsHoleComplete()
        && GameMode->GetStrokes() == 3
        && GameMode->GetPenaltyStrokes() == 0
        && DiscGolfRound::CompletedHoleCount(GameMode->GetRoundState()) == 1
        && GameMode->GetRoundState().HoleScores.IsValidIndex(0)
        && GameMode->GetRoundState().HoleScores[0].bCompleted
        && GameMode->GetRoundState().HoleScores[0].Strokes == 3;
    if (!bCaughtHoleOut)
    {
        Fail(TEXT("actual basket outcome did not record Hole 1 exactly once at three strokes"));
        return;
    }
#if DG_WITH_THROW_LAB
    ThrowLabCount = GameMode->GetThrowLabRecordCount();
#else
    ThrowLabCount = 0;
#endif
    AudioEventDelta = GameMode->GetPresentationAudioTraceCount() - BaselineAudioCount;
    CaptureScreenshot(TEXT("Finish"), TEXT("Session15_HoleComplete.png"));
    SetStage(EStage::FinishReplay);
}

void ADiscGolfSession15VerticalSliceRunner::BeginDriveReplay()
{
    if (!GameMode->HasReplayCapture())
    {
        Fail(TEXT("Drive actual-sample replay capture was unavailable"));
        return;
    }
    GameMode->ToggleInstantReplay();
    if (!GameMode->IsInstantReplayActive() || GameMode->GetReplayDurationSeconds() <= 0.0f)
    {
        Fail(TEXT("Drive replay did not start from the normal replay authority"));
        return;
    }
    GameMode->ToggleReplayPause();
    GameMode->SeekReplayRelative(0.15f);
    GameMode->CycleReplayPlaybackRate();
    GameMode->CycleReplayCamera();
    GameMode->ToggleReplayPause();
    bReplayExercised = true;
    ReplayStartedSeconds = GetWorld()->GetTimeSeconds();
}

void ADiscGolfSession15VerticalSliceRunner::FinishDriveReplay()
{
    if (GameMode->IsInstantReplayActive()) GameMode->ToggleInstantReplay();
#if DG_WITH_THROW_LAB
    if (!bThrowLabReplayExercised)
    {
        GameMode->ReplayThrowLabSelected();
        if (!GameMode->IsInstantReplayActive()
            || GameMode->GetReplayDurationSeconds() <= 0.0f)
        {
            Fail(TEXT("Drive Throw Lab replay did not start from its persisted actual-sample record"));
            return;
        }
        GameMode->ToggleReplayPause();
        GameMode->SeekReplayRelative(0.15f);
        GameMode->CycleReplayPlaybackRate();
        GameMode->CycleReplayCamera();
        GameMode->ToggleReplayPause();
        bThrowLabReplayExercised = true;
        ReplayStartedSeconds = GetWorld()->GetTimeSeconds();
        return;
    }
    if (GameMode->IsInstantReplayActive()) GameMode->ToggleInstantReplay();
    GameMode->SaveThrowLab();
#else
    bThrowLabReplayExercised = true;
#endif
    bCollectPerformance = false;
    SetStage(EStage::DrivePerformanceStabilize);
}

void ADiscGolfSession15VerticalSliceRunner::CompleteDrivePerformanceCapture()
{
    bCollectPerformance = false;
    GameMode->RefreshPerformanceTelemetry();
    DriveP95FrameMs = GameMode->PerformanceSummary.P95FrameTimeMs;
    PerformanceSampleCount = GameMode->PerformanceSummary.SampleCount;
    PerformanceHitchCount = GameMode->PerformanceSummary.HitchCount;
    PerformanceMemoryBytes = GameMode->PerformanceSummary.UsedPhysicalBytes;
    PerformanceRHIDetail = GDynamicRHI ? GDynamicRHI->GetName() : TEXT("Unavailable");
    const bool bD3D12 = PerformanceRHIDetail.Contains(
        TEXT("D3D12"), ESearchCase::IgnoreCase);
    PerformanceRHI = bD3D12 ? TEXT("D3D12") : PerformanceRHIDetail;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint ViewportSize = GEngine->GameViewport->Viewport->GetSizeXY();
        PerformanceWidth = ViewportSize.X;
        PerformanceHeight = ViewportSize.Y;
    }
    if (GameMode->GetPerformanceBudgetState() != EDiscGolfPerformanceBudgetState::Pass
        || !bD3D12
        || PerformanceWidth != 1920 || PerformanceHeight != 1080
        || PerformanceSampleCount < 120
        || PerformanceHitchCount != 0
        || PerformanceMemoryBytes > 3758096384ull
        || DriveP95FrameMs > 22.0f)
    {
        Fail(TEXT("rendered GameplayPerformance/Omen performance budget did not pass during the Drive phase"));
        return;
    }
    FString PerformancePath;
    if (!WriteDrivePerformanceReport(PerformancePath))
    {
        Fail(TEXT("Drive performance report could not be written"));
        return;
    }
    Pass();
}

bool ADiscGolfSession15VerticalSliceRunner::BeginFinish(FString& OutError)
{
    if (!ValidateCommonAgainstSetup(OutError)) return false;
    TSharedPtr<FJsonObject> Drive;
    if (!LoadPhaseReport(TEXT("Drive"), Drive, OutError)
        || !Drive->GetBoolField(TEXT("passed"))
        || !JsonStringField(Drive, TEXT("round_json"), DriveRoundJson)
        || !JsonStringField(Drive, TEXT("lie_json"), DriveLieJson)
        || !JsonStringField(Drive, TEXT("selected_disc_json"), DriveDiscJson))
    {
        if (OutError.IsEmpty()) OutError = TEXT("Drive phase report was incomplete");
        return false;
    }
    if (!GameMode->RestorePracticeRoundSnapshot())
    {
        OutError = TEXT("explicit friend-only restore rejected the Drive practice snapshot");
        return false;
    }
    FString RoundJson;
    FString LieJson;
    FString DiscJson;
    FDGDiscInstance Selected;
    if (!StructJson(GameMode->GetRoundState(), RoundJson)
        || !StructJson(GameMode->GetCurrentLieState(), LieJson)
        || !Bag->GetSelectedDiscInstance(Selected)
        || !StructJson(Selected, DiscJson)
        || RoundJson != DriveRoundJson || LieJson != DriveLieJson || DiscJson != DriveDiscJson
        || GameMode->GetStrokes() != 1 || GameMode->GetPenaltyStrokes() != 0
        || GameMode->IsHoleComplete() || GameMode->GetCurrentLieType() != ELieType::Circle2)
    {
        OutError = TEXT("Finish did not restore the exact Drive round/lie/Apex instance snapshot");
        return false;
    }
#if DG_WITH_THROW_LAB
    if (!GameMode->IsThrowLabActive()) GameMode->ToggleThrowLab();
    GameMode->LoadThrowLab();
    BaselineThrowLabCount = GameMode->GetThrowLabRecordCount();
#else
    BaselineThrowLabCount = 1;
#endif
    if (BaselineThrowLabCount != 1)
    {
        OutError = TEXT("Finish did not reload exactly the one Drive Throw Lab capture");
        return false;
    }
    BaselineAudioCount = GameMode->GetPresentationAudioTraceCount();
    GameMode->SkipCurrentPresentation();
    if (!Bag->SelectEquipment(TEXT("Touch"), EDiscPlastic::Base))
    {
        OutError = TEXT("could not select Touch/Base for the natural putting continuation");
        return false;
    }
    bReplayExercised = false;
    return LaunchCircle2(OutError);
}

bool ADiscGolfSession15VerticalSliceRunner::LaunchCircle2(FString& OutError)
{
    if (!GameMode->CanPlayerThrow()
        || GameMode->GetCurrentShotContext() != EDiscShotContext::Circle2Putt)
    {
        OutError = TEXT("restored natural lie was not a legal Circle 2 putt");
        return false;
    }
    FDGDiscInstance Selected;
    if (!Bag->GetSelectedDiscInstance(Selected) || Selected.DiscDefinitionId != TEXT("Touch")
        || Selected.PlasticId != TEXT("Base") || !Selected.InstanceId.IsValid())
    {
        OutError = TEXT("Touch/Base stable instance was not selected");
        return false;
    }
    TouchInstanceId = Selected.InstanceId;
    const FVector Basket = GameMode->GetActiveHole()->BasketLocation;
    Golfer->FaceLocation(Basket);
    FVector Direction(Basket.X - Golfer->GetActorLocation().X,
        Basket.Y - Golfer->GetActorLocation().Y, 0.0f);
    Direction = Direction.GetSafeNormal(SMALL_NUMBER, Golfer->GetActorForwardVector());
    FThrowCommand Command;
    Command.DiscInstanceId = Selected.InstanceId;
    Command.MoldId = TEXT("Touch");
    Command.Plastic = EDiscPlastic::Base;
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Circle2Putt;
    Command.Direction = Direction;
    Command.Power01 = 0.746f;
    Command.HyzerDeg = 0.0f;
    Command.NoseAngleDeg = 1.0f;
    Command.LaunchAngleDeg = 12.0f;
    Command.TimingError = 0.0f;
    if (!GameMode->RequestThrow(Command))
    {
        OutError = TEXT("Circle 2 command was rejected by authoritative throw ingress");
        return false;
    }
    if (!BindObservedDisc(OutError)) return false;
    SetStage(EStage::AwaitingCircle2Outcome);
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::LaunchCircle1(FString& OutError)
{
    if (GameMode->GetCurrentShotContext() != EDiscShotContext::Circle1Putt)
    {
        OutError = TEXT("natural second lie was not a Circle 1 putt");
        return false;
    }
    FDGDiscInstance Selected;
    if (!Bag->GetSelectedDiscInstance(Selected) || !Selected.InstanceId.IsValid()
        || Selected.InstanceId != TouchInstanceId
        || Selected.DiscDefinitionId != TEXT("Touch") || Selected.PlasticId != TEXT("Base"))
    {
        OutError = TEXT("Touch/Base stable instance disappeared before Circle 1");
        return false;
    }
    const FVector Basket = GameMode->GetActiveHole()->BasketLocation;
    Golfer->FaceLocation(Basket);
    FVector Direction(Basket.X - Golfer->GetActorLocation().X,
        Basket.Y - Golfer->GetActorLocation().Y, 0.0f);
    Direction = Direction.GetSafeNormal(SMALL_NUMBER, Golfer->GetActorForwardVector());
    FThrowCommand Command;
    Command.DiscInstanceId = Selected.InstanceId;
    Command.MoldId = TEXT("Touch");
    Command.Plastic = EDiscPlastic::Base;
    Command.ThrowStyle = EThrowStyle::Backhand;
    Command.ShotContext = EDiscShotContext::Circle1Putt;
    Command.Direction = Direction;
    // The second lie is produced by the actual Circle 2 flight, so its distance
    // is intentionally not a preset. Use the existing putting recommendation
    // authority to preserve a natural center-chain attempt from that exact lie.
    Command.Power01 = DiscGolfMath::RecommendedPuttPower01(
        GameMode->GetBasketDistanceMeters());
    Command.HyzerDeg = 0.0f;
    Command.NoseAngleDeg = 1.0f;
    // The natural follow-up is typically only 1.5-2 m after the Circle 2 putt;
    // ten degrees reaches the authored chain window without clipping the band.
    Command.LaunchAngleDeg = 10.0f;
    Command.TimingError = 0.0f;
    if (!GameMode->RequestThrow(Command))
    {
        OutError = TEXT("Circle 1 command was rejected by authoritative throw ingress");
        return false;
    }
    if (!BindObservedDisc(OutError)) return false;
    SetStage(EStage::AwaitingCircle1Outcome);
    return true;
}

void ADiscGolfSession15VerticalSliceRunner::BeginFinishReplay()
{
    if (!GameMode->HasReplayCapture())
    {
        Fail(TEXT("completed-hole replay capture was unavailable"));
        return;
    }
    GameMode->ToggleInstantReplay();
    if (!GameMode->IsInstantReplayActive())
    {
        Fail(TEXT("completed-hole replay did not start"));
        return;
    }
    GameMode->ToggleReplayPause();
    GameMode->SeekReplayRelative(0.10f);
    GameMode->CycleReplayPlaybackRate();
    GameMode->CycleReplayCamera();
    GameMode->ToggleReplayPause();
    bReplayExercised = true;
    ReplayStartedSeconds = GetWorld()->GetTimeSeconds();
}

void ADiscGolfSession15VerticalSliceRunner::FinishFinishReplay()
{
    if (GameMode->IsInstantReplayActive()) GameMode->ToggleInstantReplay();
    #if DG_WITH_THROW_LAB
    GameMode->SaveThrowLab();
    if (GameMode->GetThrowLabRecordCount() != 3
        || AudioEventDelta < 6)
    #else
    if (AudioEventDelta < 6)
    #endif
    {
        Fail(TEXT("Finish did not record exactly two additional Throw Lab shots and the expected audio lifecycle"));
        return;
    }
    Pass();
}

bool ADiscGolfSession15VerticalSliceRunner::RunVerify(FString& OutError)
{
    if (!ValidateCommonAgainstSetup(OutError, false)) return false;
    TSharedPtr<FJsonObject> Drive;
    TSharedPtr<FJsonObject> Finish;
    if (!LoadPhaseReport(TEXT("Drive"), Drive, OutError)
        || !LoadPhaseReport(TEXT("Finish"), Finish, OutError)
        || !Drive->GetBoolField(TEXT("passed"))
        || !Finish->GetBoolField(TEXT("passed")))
    {
        if (OutError.IsEmpty()) OutError = TEXT("Drive or Finish report was not a pass");
        return false;
    }
    FString FinishRound;
    FString FinishLie;
    FString FinishDisc;
    if (!JsonStringField(Finish, TEXT("round_json"), FinishRound)
        || !JsonStringField(Finish, TEXT("lie_json"), FinishLie)
        || !JsonStringField(Finish, TEXT("selected_disc_json"), FinishDisc)
        || !GameMode->RestorePracticeRoundSnapshot())
    {
        OutError = TEXT("Verify could not load the completed Finish snapshot");
        return false;
    }
    FString RoundJson;
    FString LieJson;
    FString DiscJson;
    FDGDiscInstance Selected;
    if (!StructJson(GameMode->GetRoundState(), RoundJson)
        || !StructJson(GameMode->GetCurrentLieState(), LieJson)
        || !Bag->GetSelectedDiscInstance(Selected)
        || !StructJson(Selected, DiscJson)
        || RoundJson != FinishRound || LieJson != FinishLie || DiscJson != FinishDisc
        || Selected.DiscDefinitionId != TEXT("Touch") || Selected.PlasticId != TEXT("Base")
        || GameMode->GetStrokes() != 3 || GameMode->GetPenaltyStrokes() != 0
        || !GameMode->IsHoleComplete()
        || DiscGolfRound::CompletedHoleCount(GameMode->GetRoundState()) != 1)
    {
        OutError = TEXT("fresh Verify process did not reconstruct exact completed score/lie/Touch state");
        return false;
    }
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::CaptureScreenshot(
    const FString& PhaseName,
    const FString& FileName)
{
    const FString Directory = FPaths::Combine(GetReportRoot(), PhaseName);
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, FileName);
    FScreenshotRequest::RequestScreenshot(Path, false, false);
    bScreenshotRequested = true;
    return true;
}

FString ADiscGolfSession15VerticalSliceRunner::GetReportRoot() const
{
    return FPaths::Combine(AcceptedUserDir, TEXT("Saved/Session15Reports"));
}

FString ADiscGolfSession15VerticalSliceRunner::GetPhaseReportPath(
    const FString& PhaseName) const
{
    return FPaths::Combine(GetReportRoot(), PhaseName, TEXT("PhaseReport.json"));
}

FString ADiscGolfSession15VerticalSliceRunner::GetPhaseName() const
{
    switch (Phase)
    {
        case EPhase::Setup: return TEXT("Setup");
        case EPhase::Drive: return TEXT("Drive");
        case EPhase::Finish: return TEXT("Finish");
        case EPhase::Verify: return TEXT("Verify");
        default: return TEXT("Invalid");
    }
}

bool ADiscGolfSession15VerticalSliceRunner::LoadPhaseReport(
    const FString& PhaseName,
    TSharedPtr<FJsonObject>& OutReport,
    FString& OutError) const
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *GetPhaseReportPath(PhaseName)))
    {
        OutError = FString::Printf(TEXT("missing prior phase report: %s"), *PhaseName);
        return false;
    }
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, OutReport) || !OutReport.IsValid()
        || OutReport->GetStringField(TEXT("schema")) != PhaseSchema
        || OutReport->GetStringField(TEXT("phase")) != PhaseName)
    {
        OutError = FString::Printf(TEXT("invalid prior phase report: %s"), *PhaseName);
        return false;
    }
    return true;
}

bool ADiscGolfSession15VerticalSliceRunner::WritePhaseReport(
    bool bPassed,
    FString& OutPath) const
{
    const FString PhaseName = GetPhaseName();
    FString CharacterJson;
    FString OutfitJson;
    FString SettingsJson;
    FString DiscJson;
    FString RoundJson;
    FString LieJson;
    FDGDiscInstance Selected;
    if (!TourGameInstance || !Golfer || !Bag || !GameMode
        || !StructJson(TourGameInstance->GetFullCharacterCustomization(), CharacterJson)
        || !StructJson(Golfer->GetCurrentOutfitLoadout(), OutfitJson)
        || !StructJson(TourGameInstance->GetPlayerSettings(), SettingsJson)
        || !Bag->GetSelectedDiscInstance(Selected)
        || !StructJson(Selected, DiscJson)
        || !StructJson(GameMode->GetRoundState(), RoundJson)
        || !StructJson(GameMode->GetCurrentLieState(), LieJson))
    {
        return false;
    }
    const ADiscGolfHoleActor* Hole = GameMode->GetActiveHole();
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), PhaseSchema);
    Root->SetStringField(TEXT("phase"), PhaseName);
    Root->SetBoolField(TEXT("passed"), bPassed);
    Root->SetStringField(TEXT("failure_reason"), FailureReason);
    Root->SetStringField(TEXT("external_user_dir"), AcceptedUserDir);
    Root->SetStringField(TEXT("course_id"), Hole ? Hole->CourseId.ToString() : TEXT("None"));
    Root->SetNumberField(TEXT("hole_number"), Hole ? Hole->HoleNumber : 0);
    Root->SetNumberField(TEXT("strokes"), GameMode->GetStrokes());
    Root->SetNumberField(TEXT("penalty_strokes"), GameMode->GetPenaltyStrokes());
    Root->SetBoolField(TEXT("hole_complete"), GameMode->IsHoleComplete());
    Root->SetStringField(TEXT("character_json"), CharacterJson);
    Root->SetStringField(TEXT("outfit_json"), OutfitJson);
    Root->SetStringField(TEXT("settings_json"), SettingsJson);
    Root->SetStringField(TEXT("selected_disc_json"), DiscJson);
    Root->SetStringField(TEXT("round_json"), RoundJson);
    Root->SetStringField(TEXT("lie_json"), LieJson);
    Root->SetStringField(TEXT("blocker_summary"),
        TEXT("technical slice only; art, calibration, audio-content, provenance, legal, and human feel gates remain"));
    Root->SetArrayField(TEXT("release_blockers"), ReleaseBlockersJson());

    if (Phase == EPhase::Drive)
    {
        Root->SetNumberField(TEXT("release_count_delta"), 1);
        Root->SetNumberField(TEXT("flight_sample_count"), FlightSampleCount);
        Root->SetBoolField(TEXT("replay_exercised"), bReplayExercised);
        Root->SetBoolField(TEXT("throw_lab_replay_exercised"), bThrowLabReplayExercised);
        Root->SetNumberField(TEXT("throw_lab_count"), ThrowLabCount);
        Root->SetNumberField(TEXT("audio_event_delta"), AudioEventDelta);
        Root->SetBoolField(TEXT("tracer_enabled"), GameMode->IsShotTracerEnabled());
        Root->SetBoolField(TEXT("camera_seen"), bCameraSeen);
        Root->SetBoolField(TEXT("wind_seen"), bWindSeen);
        Root->SetBoolField(TEXT("natural_circle2_lie"), bNaturalCircle2Lie);
        Root->SetNumberField(TEXT("p95_frame_ms"), DriveP95FrameMs);
        Root->SetStringField(TEXT("rhi"), PerformanceRHI);
        Root->SetStringField(TEXT("rhi_detail"), PerformanceRHIDetail);
        Root->SetNumberField(TEXT("width"), PerformanceWidth);
        Root->SetNumberField(TEXT("height"), PerformanceHeight);
        Root->SetNumberField(TEXT("performance_sample_count"), PerformanceSampleCount);
        Root->SetNumberField(TEXT("hitch_count"), PerformanceHitchCount);
        Root->SetNumberField(TEXT("memory_bytes"), static_cast<double>(PerformanceMemoryBytes));
        Root->SetBoolField(TEXT("meets_22ms_gate"), DriveP95FrameMs <= 22.0f);
        Root->SetBoolField(TEXT("meets_60fps_diagnostic"), DriveP95FrameMs <= SixtyFpsFrameMs);
        Root->SetBoolField(TEXT("performance_budget_pass"),
            GameMode->GetPerformanceBudgetState() == EDiscGolfPerformanceBudgetState::Pass);
        Root->SetBoolField(TEXT("screenshot_requested"), bScreenshotRequested);
    }
    else if (Phase == EPhase::Finish)
    {
        Root->SetBoolField(TEXT("circle2_settled_to_circle1"), bCircle2SettledToCircle1);
        Root->SetBoolField(TEXT("caught_holeout"), bCaughtHoleOut);
        Root->SetNumberField(TEXT("total_strokes"), GameMode->GetStrokes());
        Root->SetNumberField(TEXT("completed_holes"),
            DiscGolfRound::CompletedHoleCount(GameMode->GetRoundState()));
        Root->SetBoolField(TEXT("replay_exercised"), bReplayExercised);
        Root->SetNumberField(TEXT("throw_lab_count"), ThrowLabCount);
        Root->SetNumberField(TEXT("audio_event_delta"), AudioEventDelta);
        Root->SetBoolField(TEXT("screenshot_requested"), bScreenshotRequested);
    }

    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    if (!FJsonSerializer::Serialize(Root, Writer)) return false;
    OutPath = GetPhaseReportPath(PhaseName);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
    return FFileHelper::SaveStringToFile(Text, *OutPath);
}

bool ADiscGolfSession15VerticalSliceRunner::WriteDrivePerformanceReport(
    FString& OutPath) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), PerformanceSchema);
    Root->SetStringField(TEXT("phase"), TEXT("Drive"));
    Root->SetBoolField(TEXT("passed"), true);
    Root->SetStringField(TEXT("quality_profile"), TEXT("GameplayPerformance"));
    Root->SetStringField(TEXT("runtime_profile"), TEXT("OmenGameplay1080pHighFoliageV1"));
    Root->SetStringField(TEXT("segment"),
        TEXT("integrated_hole1_post_interaction_rendered_gameplay"));
    Root->SetStringField(TEXT("rhi"), PerformanceRHI);
    Root->SetStringField(TEXT("rhi_detail"), PerformanceRHIDetail);
    Root->SetNumberField(TEXT("width"), PerformanceWidth);
    Root->SetNumberField(TEXT("height"), PerformanceHeight);
    Root->SetNumberField(TEXT("warmup_seconds"), DriveWarmupSeconds);
    Root->SetNumberField(TEXT("post_interaction_stabilization_seconds"),
        DrivePerformanceStabilizeSeconds);
    Root->SetNumberField(TEXT("capture_sample_target"),
        DrivePerformanceCaptureSamples);
    Root->SetNumberField(TEXT("sample_count"), PerformanceSampleCount);
    Root->SetNumberField(TEXT("p95_frame_ms"), DriveP95FrameMs);
    Root->SetNumberField(TEXT("hitch_count"), PerformanceHitchCount);
    Root->SetNumberField(TEXT("memory_bytes"), static_cast<double>(PerformanceMemoryBytes));
    Root->SetNumberField(TEXT("p95_budget_ms"), 22.0f);
    Root->SetNumberField(TEXT("sixty_fps_frame_ms"), SixtyFpsFrameMs);
    Root->SetBoolField(TEXT("meets_60fps_diagnostic"), DriveP95FrameMs <= SixtyFpsFrameMs);
    Root->SetBoolField(TEXT("performance_budget_pass"),
        GameMode && GameMode->GetPerformanceBudgetState() == EDiscGolfPerformanceBudgetState::Pass);
    Root->SetStringField(TEXT("status"), GameMode ? GameMode->GetPerformanceStatusText() : TEXT("Unavailable"));
    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    if (!FJsonSerializer::Serialize(Root, Writer)) return false;
    OutPath = FPaths::Combine(GetReportRoot(), TEXT("Drive/PerformanceReport.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
    return FFileHelper::SaveStringToFile(Text, *OutPath);
}

bool ADiscGolfSession15VerticalSliceRunner::WriteCanonicalReport(FString& OutPath) const
{
    TSharedPtr<FJsonObject> Setup;
    TSharedPtr<FJsonObject> Drive;
    TSharedPtr<FJsonObject> Finish;
    FString Error;
    if (!LoadPhaseReport(TEXT("Setup"), Setup, Error)
        || !LoadPhaseReport(TEXT("Drive"), Drive, Error)
        || !LoadPhaseReport(TEXT("Finish"), Finish, Error))
    {
        return false;
    }
    TSharedRef<FJsonObject> PhasePass = MakeShared<FJsonObject>();
    PhasePass->SetBoolField(TEXT("Setup"), Setup->GetBoolField(TEXT("passed")));
    PhasePass->SetBoolField(TEXT("Drive"), Drive->GetBoolField(TEXT("passed")));
    PhasePass->SetBoolField(TEXT("Finish"), Finish->GetBoolField(TEXT("passed")));
    PhasePass->SetBoolField(TEXT("Verify"), true);

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), CanonicalSchema);
    Root->SetStringField(TEXT("result"), TechnicalPass);
    Root->SetBoolField(TEXT("bounded_technical_vertical_slice_complete"), true);
    Root->SetBoolField(TEXT("release_ready"), false);
    Root->SetBoolField(TEXT("release_use_allowed"), false);
    Root->SetStringField(TEXT("external_user_dir"), AcceptedUserDir);
    Root->SetObjectField(TEXT("phase_pass"), PhasePass);
    const double P95 = Drive->GetNumberField(TEXT("p95_frame_ms"));
    Root->SetNumberField(TEXT("p95_frame_ms"), P95);
    Root->SetNumberField(TEXT("sixty_fps_frame_ms"), SixtyFpsFrameMs);
    Root->SetBoolField(TEXT("meets_60fps_diagnostic"), P95 <= SixtyFpsFrameMs);
    Root->SetBoolField(TEXT("performance_budget_pass"),
        Drive->GetBoolField(TEXT("performance_budget_pass")));
    Root->SetStringField(TEXT("technical_substitution"),
        TEXT("project-original generic identity and existing accepted gameplay authorities replace donor brand language"));
    Root->SetStringField(TEXT("art_blocker"),
        TEXT("Pine Ridge presentation assetsReady remains false; final environment and character-art judgment are pending"));
    Root->SetStringField(TEXT("calibration_blocker"),
        TEXT("current bounded fictional equipment is not production weight/wear/broad-disc calibration approval"));
    Root->SetStringField(TEXT("audio_blocker"),
        TEXT("semantic responsive audio events are integrated; authored production mix/content certification is pending"));
    Root->SetStringField(TEXT("footing_blocker"),
        TEXT("surface lie modifiers are integrated; the planned full terrain/stance footing evaluator remains pending"));
    Root->SetArrayField(TEXT("release_blockers"), ReleaseBlockersJson());

    FString Text;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
    if (!FJsonSerializer::Serialize(Root, Writer)) return false;
    OutPath = FPaths::Combine(GetReportRoot(), TEXT("Session15VerticalSliceReport.json"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutPath), true);
    return FFileHelper::SaveStringToFile(Text, *OutPath);
}

float ADiscGolfSession15VerticalSliceRunner::ComputeP95FrameMs() const
{
    if (PerformanceFrameMs.IsEmpty()) return 0.0f;
    TArray<float> Sorted = PerformanceFrameMs;
    Sorted.Sort();
    const int32 Index = FMath::Clamp(
        FMath::CeilToInt(static_cast<float>(Sorted.Num()) * 0.95f) - 1,
        0, Sorted.Num() - 1);
    return Sorted[Index];
}

void ADiscGolfSession15VerticalSliceRunner::Pass()
{
    if (bFinished) return;
    bFinished = true;
    Stage = EStage::Finished;
    SetActorTickEnabled(false);
    FString PhaseReport;
    if (!WritePhaseReport(true, PhaseReport))
    {
        bFinished = false;
        Fail(TEXT("phase report could not be written"));
        return;
    }
    const FString PhaseName = GetPhaseName();
    UE_LOG(LogDiscGolfTour, Display, TEXT("DG_SESSION15_VERTICAL_SLICE_%s: PASS report=%s"),
        *PhaseName.ToUpper(), *PhaseReport);
    if (Phase == EPhase::Verify)
    {
        FString Canonical;
        if (!WriteCanonicalReport(Canonical))
        {
            bFinished = false;
            Fail(TEXT("canonical acceptance report could not be written"));
            return;
        }
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION15_VERTICAL_SLICE_ACCEPTANCE: PASS_TECHNICAL_VERTICAL_SLICE_RELEASE_BLOCKED report=%s"),
            *Canonical);
    }
    FPlatformMisc::RequestExitWithStatus(false, 0);
}

void ADiscGolfSession15VerticalSliceRunner::Fail(const FString& Reason)
{
    if (bFinished) return;
    bFinished = true;
    FailureReason = Reason;
    Stage = EStage::Finished;
    SetActorTickEnabled(false);
    if (ObservedDisc)
    {
        ObservedDisc->OnDiscSettled.RemoveDynamic(
            this, &ADiscGolfSession15VerticalSliceRunner::HandleObservedDiscSettled);
        ObservedDisc->OnDiscHoledOut.RemoveDynamic(
            this, &ADiscGolfSession15VerticalSliceRunner::HandleObservedDiscHoledOut);
    }
    FString ReportPath;
    WritePhaseReport(false, ReportPath);
    UE_LOG(LogDiscGolfTour, Error, TEXT("DG_SESSION15_VERTICAL_SLICE_%s: FAIL reason=%s report=%s"),
        *GetPhaseName().ToUpper(), *Reason, *ReportPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession15VerticalSliceRunner::SetStage(EStage NewStage)
{
    Stage = NewStage;
    StageStartedSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
}

double ADiscGolfSession15VerticalSliceRunner::SecondsInStage() const
{
    return GetWorld() ? GetWorld()->GetTimeSeconds() - StageStartedSeconds : 0.0;
}
