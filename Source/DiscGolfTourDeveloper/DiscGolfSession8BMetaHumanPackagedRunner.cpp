#include "DiscGolfSession8BMetaHumanPackagedRunner.h"

#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourGameInstance.h"
#include "DiscGolfTourPlayerController.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfSaveGame.h"
#include "DiscGolfFullCharacterRuntime.h"
#include "DiscGolfOutfitRuntime.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.h"
#include "DiscGolfMetaHumanOutfitRequiredBonesComponent.h"
#include "DiscGolfMetaHumanRetargetAnimInstance.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscBagComponent.h"
#include "ThrowControllerComponent.h"
#include "DiscActor.h"
#include "DiscFlightComponent.h"
#include "DiscTrajectoryTypes.h"

#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfThrowComponent.h"

#include "Animation/MeshDeformerInstance.h"
#include "Animation/MorphTarget.h"
#include "BoneContainer.h"
#include "Components/InputComponent.h"
#include "Components/ExternalMorphSet.h"
#include "Components/LODSyncComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GroomComponent.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "LODSyncInterface.h"
#include "Materials/MaterialInterface.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "DynamicRHI.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "Retargeter/IKRetargeter.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "Scalability.h"
#include "SkeletalRenderPublic.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealClient.h"

namespace DiscGolfSession8BMetaHumanPackaged
{
constexpr int32 ExpectedWidth = 1920;
constexpr int32 ExpectedHeight = 1080;
constexpr int64 MinimumPngBytes = 32 * 1024;
constexpr double ScreenshotSettleSeconds = 0.25;
constexpr double ScreenshotTimeoutSeconds = 30.0;
constexpr double RenderReadinessTimeoutSeconds = 5.0;
constexpr const TCHAR* HairRenderPendingPrefix =
    TEXT("PENDING_HAIR_RENDER:");
constexpr const TCHAR* VisualRenderPendingPrefix =
    TEXT("PENDING_VISUAL_RENDER:");
constexpr const TCHAR* OutfitSkinPendingPrefix =
    TEXT("PENDING_OUTFIT_SKIN:");
constexpr double StepTimeoutSeconds = 45.0;
constexpr double TimingCaptureSeconds = 0.35;
constexpr double PerformanceWarmupSeconds = 10.0;
constexpr double PerformanceSegmentSeconds = 15.0;
constexpr uint64 AcceptedUsedPhysicalBytes = 3758096384ull;
constexpr uint64 HardFailureUsedPhysicalBytes = 4294967296ull;
constexpr float AcceptedP95FrameMs = 22.0f;
constexpr float HardFailureP95FrameMs = 33.34f;
constexpr float HitchFrameMs = 50.0f;
constexpr float HardFailureHitchRatePercent = 1.0f;
constexpr const TCHAR* SaveSlot = TEXT("DiscGolfTour_Profile_0");
constexpr const TCHAR* ExpectedExternalUserRoot =
    TEXT("C:/DGTour_TestRuns/Session8B_Packaged");
constexpr const TCHAR* Schema =
    TEXT("DiscGolfTour.Session8BMetaHumanPackagedAcceptance.v3.4");
constexpr const TCHAR* ExpectedHairGroomAssetPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/")
    TEXT("MHC_DG_Golfer_Default/Grooms/Hair_S_Clean.Hair_S_Clean");
constexpr const TCHAR* ExpectedOutfitMeshAssetPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/")
    TEXT("MHC_DG_Golfer_Default/Clothing/")
    TEXT("MHC_DG_Golfer_Default_Outfits.MHC_DG_Golfer_Default_Outfits");
constexpr const TCHAR* ExpectedClothingPostProcessClassPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Common/Animation/")
    TEXT("ABP_Clothing_PostProcess.ABP_Clothing_PostProcess_C");
constexpr const TCHAR* ExpectedBodyPostProcessClassPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Common/Body/")
    TEXT("ABP_Body_PostProcess.ABP_Body_PostProcess_C");
constexpr const TCHAR* ExpectedShirtMaterialSlot =
    TEXT("M_DG_bodyShapeD_Shirt");
constexpr const TCHAR* ExpectedShortMaterialSlot =
    TEXT("M_DG_bodyShapeD_Short");
constexpr const TCHAR* ExpectedShirtMaterialPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/")
    TEXT("MHC_DG_Golfer_Default/Clothing/")
    TEXT("MI_WI_DefaultGarment_M_DG_bodyShapeD_Shirt.")
    TEXT("MI_WI_DefaultGarment_M_DG_bodyShapeD_Shirt");
constexpr const TCHAR* ExpectedShortMaterialPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/")
    TEXT("MHC_DG_Golfer_Default/Clothing/")
    TEXT("MI_WI_DefaultGarment_M_DG_bodyShapeD_Short.")
    TEXT("MI_WI_DefaultGarment_M_DG_bodyShapeD_Short");
constexpr float ShirtMinimumKneeClearanceCm = 5.0f;
constexpr float ShortMinimumKneeClearanceCm = -5.0f;
constexpr float GarmentMinimumFootClearanceCm = 20.0f;
constexpr float ShirtMaximumHeightCm = 85.0f;
constexpr float ShortMaximumHeightCm = 60.0f;
constexpr float BelowKneeToleranceCm = 5.0f;
constexpr float BelowFootToleranceCm = 1.0f;
constexpr const TCHAR* Marker =
    TEXT("DG_SESSION8B_METAHUMAN_PACKAGED_ACCEPTANCE");

const TCHAR* const PhaseNames[] = {
    TEXT("apply_metahuman"),
    TEXT("reload_cancel_failure_switch_dg"),
    TEXT("reload_dg_restore_metahuman_visual_throw"),
    TEXT("metahuman_performance")
};

const TCHAR* const ConflictingRunnerFlags[] = {
    TEXT("ThreeHoleRoundSmokeTest"),
    TEXT("CourseSmokeTest"),
    TEXT("RegressionSuiteSmokeTest"),
    TEXT("PineRidgePlaySmokeTest"),
    TEXT("NeedleGateRouteTelemetrySmokeTest"),
    TEXT("NeedleGateRouteTelemetry"),
    TEXT("GalleryLakeWaterSmokeTest"),
    TEXT("DenseForestSmokeTest"),
    TEXT("GroundGrassSmokeTest"),
    TEXT("Hole1FlightRouteSmokeTest"),
    TEXT("FixtureCollisionSmokeTest"),
    TEXT("Session3OneThrowSmokeTest"),
    TEXT("Session3VisualCapture"),
    TEXT("Session4VisualCapture"),
    TEXT("Session5MocapPipelineSmokeTest"),
    TEXT("Session5MocapVisualCapture"),
    TEXT("Session6OutfitThrowSmokeTest"),
    TEXT("Session6OutfitVisualCapture"),
    TEXT("Session7FullCharacterThrowSmokeTest"),
    TEXT("Session7FullCharacterVisualCapture"),
    TEXT("Session8CookClosureSmokeTest")
};

int32 CountOccurrences(const FString& Text, const FString& Needle)
{
    int32 Count = 0;
    int32 SearchFrom = 0;
    while (SearchFrom < Text.Len())
    {
        const int32 Found = Text.Find(
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

int32 ReadBigEndianInt32(const uint8* Bytes)
{
    return (static_cast<int32>(Bytes[0]) << 24)
        | (static_cast<int32>(Bytes[1]) << 16)
        | (static_cast<int32>(Bytes[2]) << 8)
        | static_cast<int32>(Bytes[3]);
}

FString SafeSingleLine(FString Value)
{
    Value.ReplaceInline(TEXT("\r"), TEXT(" "));
    Value.ReplaceInline(TEXT("\n"), TEXT(" "));
    return Value;
}

FString ThrowPhaseName(EDGThrowPhase Phase)
{
    switch (Phase)
    {
        case EDGThrowPhase::Aim: return TEXT("Aim");
        case EDGThrowPhase::RunUp: return TEXT("RunUp");
        case EDGThrowPhase::ReachBack: return TEXT("ReachBack");
        case EDGThrowPhase::Plant: return TEXT("Plant");
        case EDGThrowPhase::Acceleration: return TEXT("Acceleration");
        case EDGThrowPhase::Release: return TEXT("Release");
        case EDGThrowPhase::FollowThrough: return TEXT("FollowThrough");
        case EDGThrowPhase::Recovery: return TEXT("Recovery");
        case EDGThrowPhase::Idle:
        default: return TEXT("Idle");
    }
}

const TCHAR* PresentationPolicyName(
    EDGMetaHumanPresentationPolicy Policy)
{
    switch (Policy)
    {
        case EDGMetaHumanPresentationPolicy::CharacterCreator:
            return TEXT("CharacterCreator");
        case EDGMetaHumanPresentationPolicy::GameplayPerformance:
            return TEXT("GameplayPerformance");
        case EDGMetaHumanPresentationPolicy::Unconfigured:
        default:
            return TEXT("Unconfigured");
    }
}

const TCHAR* VisibilityTickOptionName(
    EVisibilityBasedAnimTickOption Option)
{
    switch (Option)
    {
        case EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones:
            return TEXT("AlwaysTickPoseAndRefreshBones");
        case EVisibilityBasedAnimTickOption::AlwaysTickPose:
            return TEXT("AlwaysTickPose");
        case EVisibilityBasedAnimTickOption::
                OnlyTickMontagesAndRefreshBonesWhenPlayingMontages:
            return TEXT(
                "OnlyTickMontagesAndRefreshBonesWhenPlayingMontages");
        case EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered:
            return TEXT("OnlyTickMontagesWhenNotRendered");
        case EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered:
            return TEXT("OnlyTickPoseWhenRendered");
        default: return TEXT("Unknown");
    }
}

const TCHAR* SyncOptionName(ESyncOption Option)
{
    switch (Option)
    {
        case ESyncOption::Drive: return TEXT("Drive");
        case ESyncOption::Passive: return TEXT("Passive");
        case ESyncOption::Disabled:
        default: return TEXT("Disabled");
    }
}

bool HasSourceTickPrerequisite(
    const USkeletalMeshComponent* Mesh,
    const USkeletalMeshComponent* AnimationSource)
{
    if (!IsValid(Mesh) || !IsValid(AnimationSource))
    {
        return false;
    }
    for (const FTickPrerequisite& Prerequisite
         : Mesh->PrimaryComponentTick.GetPrerequisites())
    {
        if (Prerequisite.PrerequisiteObject.Get() == AnimationSource
            && Prerequisite.Get()
                == &AnimationSource->PrimaryComponentTick)
        {
            return true;
        }
    }
    return false;
}

bool IsVisiblePresentationMesh(const USkeletalMeshComponent* Mesh)
{
    return IsValid(Mesh)
        && !Mesh->IsA<UDiscGolfMetaHumanOutfitRequiredBonesComponent>()
        && Mesh->GetSkeletalMeshAsset()
        && Mesh->IsVisible()
        && !Mesh->bHiddenInGame
        && Mesh->bRenderInMainPass
        && !Mesh->bVisibleInSceneCaptureOnly;
}

FDiscGolfPerformanceSummary SummarizeAllFrameIntervals(
    const TArray<float>& FrameTimesMs,
    uint64 UsedPhysicalBytes,
    const FDiscGolfPerformanceBudget& Budget)
{
    FDiscGolfPerformanceSummary Summary;
    Summary.SampleCount = FrameTimesMs.Num();
    Summary.UsedPhysicalBytes = UsedPhysicalBytes;
    Summary.bHasMinimumSamples =
        Summary.SampleCount >= Budget.MinimumSampleCount;
    if (FrameTimesMs.IsEmpty())
    {
        return Summary;
    }

    TArray<float> SortedFrameTimes = FrameTimesMs;
    double TotalFrameTimeMs = 0.0;
    for (const float FrameTimeMs : FrameTimesMs)
    {
        TotalFrameTimeMs += FrameTimeMs;
        Summary.MaxFrameTimeMs = FMath::Max(
            Summary.MaxFrameTimeMs, FrameTimeMs);
        if (FrameTimeMs >= Budget.HitchFrameTimeMs)
        {
            ++Summary.HitchCount;
        }
    }
    Summary.AverageFrameTimeMs = static_cast<float>(
        TotalFrameTimeMs / static_cast<double>(Summary.SampleCount));
    Summary.AverageFps = Summary.AverageFrameTimeMs > SMALL_NUMBER
        ? 1000.0f / Summary.AverageFrameTimeMs : 0.0f;
    Summary.HitchRatePercent =
        100.0f * static_cast<float>(Summary.HitchCount)
        / static_cast<float>(Summary.SampleCount);
    SortedFrameTimes.Sort();
    const int32 P95Index = FMath::Clamp(
        FMath::CeilToInt(Summary.SampleCount * 0.95f) - 1,
        0,
        Summary.SampleCount - 1);
    Summary.P95FrameTimeMs = SortedFrameTimes[P95Index];
    return Summary;
}

bool BindSession8BSelectedThrowProvenance(
    ADiscGolferPawn* Pawn,
    ADiscGolfTourGameMode* GameMode,
    FThrowCommand& InOutCommand)
{
    UDiscBagComponent* Bag = Pawn ? Pawn->GetDiscBag() : nullptr;
    const UDiscGolfCharacterProfile* Profile = Pawn
        ? Pawn->GetRuntimeCharacterProfile() : nullptr;
    FDGDiscInstance SelectedInstance;
    if (!Bag || !Profile || !GameMode
        || !Bag->GetSelectedDiscInstance(SelectedInstance)
        || !SelectedInstance.InstanceId.IsValid()
        || SelectedInstance.DiscDefinitionId.IsNone()
        || Bag->GetSelectedMoldId() != SelectedInstance.DiscDefinitionId)
    {
        return false;
    }

    InOutCommand.DiscInstanceId = SelectedInstance.InstanceId;
    InOutCommand.MoldId = SelectedInstance.DiscDefinitionId;
    InOutCommand.Plastic = Bag->GetSelectedPlastic();
    InOutCommand.Handedness = Profile->Handedness;
    return ADiscGolfTourGameMode::IsPlayerThrowProvenanceValid(
        InOutCommand,
        Profile->Handedness,
        GameMode->GetCurrentShotContext(),
        SelectedInstance);
}

}

using namespace DiscGolfSession8BMetaHumanPackaged;

ADiscGolfSession8BMetaHumanPackagedRunner::
    ADiscGolfSession8BMetaHumanPackagedRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ADiscGolfSession8BMetaHumanPackagedRunner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    ResumeAfterThrowCapture();
    if (GameMode)
    {
        GameMode->DiscardDeferredTrajectoryExport();
    }
    if (ThrowAdapter)
    {
        ThrowAdapter->OnReleaseCommitted.RemoveDynamic(
            this,
            &ADiscGolfSession8BMetaHumanPackagedRunner::HandleReleaseCommitted);
        ThrowAdapter->OnThrowRecovered.RemoveDynamic(
            this,
            &ADiscGolfSession8BMetaHumanPackagedRunner::HandleThrowRecovered);
    }
    Super::EndPlay(EndPlayReason);
}

void ADiscGolfSession8BMetaHumanPackagedRunner::Start()
{
    if (bStarted || bFinished)
    {
        return;
    }
    bStarted = true;

    FString Error;
    if (!ValidateInvocation(Error))
    {
        Fail(TEXT("INVOCATION_CONTRACT"), Error);
        return;
    }
    if (!AcquireRuntime(Error))
    {
        Fail(TEXT("RUNTIME_FOUNDATION"), Error);
        return;
    }
    if (!BeginPhase(Error))
    {
        Fail(TEXT("PHASE_PRECONDITION"), Error);
        return;
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("%s: START phase=%s run_id=%s isolated_profile_save_only=1"),
        Marker, *PhaseName, *RunId);
}

void ADiscGolfSession8BMetaHumanPackagedRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    (void)DeltaSeconds;
    if (!bStarted || bFinished)
    {
        return;
    }
    if (bCapturePending)
    {
        PollCapture();
        return;
    }
    RunStep();
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::ValidateInvocation(
    FString& OutError)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    const FString CommandLineText(CommandLine);
#if WITH_EDITOR
    OutError = TEXT("the acceptance runner is forbidden in Editor builds");
    return false;
#else
    if (!FPlatformProperties::RequiresCookedData())
    {
        OutError = TEXT("the acceptance runner requires cooked packaged data");
        return false;
    }
    if (!FApp::IsUnattended()
        || !FParse::Param(CommandLine,
            TEXT("Session8BMetaHumanPackagedAcceptance"))
        || !FParse::Param(CommandLine,
            TEXT("Session8BPracticeSnapshotSaveSuppressed")))
    {
        OutError = TEXT(
            "unattended packaged acceptance and practice-snapshot suppression flags are both required");
        return false;
    }
    if (CountOccurrences(
            CommandLineText, TEXT("-Session8BMetaHumanPackagedAcceptance")) != 1
        || CountOccurrences(
            CommandLineText, TEXT("-Session8BPracticeSnapshotSaveSuppressed")) != 1
        || CountOccurrences(
            CommandLineText, TEXT("-Session8BMetaHumanPhase=")) != 1
        || CountOccurrences(
            CommandLineText, TEXT("-Session8BRunId=")) != 1
        || CountOccurrences(CommandLineText, TEXT("-UserDir=")) != 1)
    {
        OutError = TEXT("acceptance, suppression, phase, run-id and UserDir arguments must each occur exactly once");
        return false;
    }
    if (CommandLineText.Contains(TEXT("-PerformanceCaptureSeconds="),
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("the legacy authored-flyover performance route cannot be combined with Session 8B");
        return false;
    }
    if (CommandLineText.Contains(TEXT("-VisualQAScreenshot="),
            ESearchCase::IgnoreCase))
    {
        OutError = TEXT("the generic visual-QA route cannot be combined with Session 8B");
        return false;
    }
    for (const TCHAR* ConflictingFlag : ConflictingRunnerFlags)
    {
        if (FParse::Param(CommandLine, ConflictingFlag))
        {
            OutError = FString::Printf(
                TEXT("conflicting validation runner flag was present: -%s"),
                ConflictingFlag);
            return false;
        }
    }

    if (!FParse::Value(
            CommandLine, TEXT("Session8BMetaHumanPhase="), PhaseName))
    {
        OutError = TEXT("a Session8BMetaHumanPhase value is required");
        return false;
    }
    if (PhaseName == PhaseNames[0])
    {
        Phase = EPhase::ApplyMetaHuman;
    }
    else if (PhaseName == PhaseNames[1])
    {
        Phase = EPhase::ReloadCancelFailureSwitchDG;
    }
    else if (PhaseName == PhaseNames[2])
    {
        Phase = EPhase::ReloadDGRestoreMetaHumanVisualThrow;
    }
    else if (PhaseName == PhaseNames[3])
    {
        Phase = EPhase::MetaHumanPerformance;
    }
    else
    {
        OutError = FString::Printf(
            TEXT("unknown Session 8B phase: %s"), *PhaseName);
        return false;
    }

    FString RequestedUserDir;
    if (!FParse::Value(CommandLine, TEXT("Session8BRunId="), RunId)
        || !FParse::Value(CommandLine, TEXT("UserDir="), RequestedUserDir)
        || RunId.IsEmpty() || RequestedUserDir.IsEmpty()
        || FPaths::IsRelative(RequestedUserDir))
    {
        OutError = TEXT("an absolute external UserDir and matching GUID run id are required");
        return false;
    }
    FGuid ParsedRunId;
    if (!FGuid::Parse(RunId, ParsedRunId) || !ParsedRunId.IsValid()
        || RunId != ParsedRunId.ToString(EGuidFormats::DigitsWithHyphensLower))
    {
        OutError = TEXT("Session8BRunId must be a canonical lower-case hyphenated GUID");
        return false;
    }

    UserDirectory = FPaths::ConvertRelativePathToFull(RequestedUserDir);
    FPaths::NormalizeDirectoryName(UserDirectory);
    FString ActiveUserDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir());
    FPaths::NormalizeDirectoryName(ActiveUserDirectory);
    FString ProjectDirectory =
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectDirectory);
    const FString ParentDirectory = FPaths::GetPath(UserDirectory);
    if (!FPaths::IsSamePath(UserDirectory, ActiveUserDirectory)
        || FPaths::GetCleanFilename(UserDirectory) != RunId
        || !FPaths::IsSamePath(
            ParentDirectory, ExpectedExternalUserRoot)
        || FPaths::IsSamePath(UserDirectory, ProjectDirectory)
        || FPaths::IsUnderDirectory(UserDirectory, ProjectDirectory))
    {
        OutError = TEXT(
            "UserDir must be the active exact C:/DGTour_TestRuns/Session8B_Packaged/<matching-guid> directory");
        return false;
    }
    FString OwnedMarkerText;
    const FString OwnedMarkerPath = FPaths::Combine(
        UserDirectory,
        TEXT(".dg_session8b_packaged_acceptance_owned"));
    if (!FFileHelper::LoadFileToString(
            OwnedMarkerText, *OwnedMarkerPath)
        || OwnedMarkerText.TrimStartAndEnd() != RunId)
    {
        OutError = TEXT(
            "the exact launcher-owned UserDir marker was absent or did not match the run id");
        return false;
    }

    int32 ResX = 0;
    int32 ResY = 0;
    FString Course;
    int32 Hole = 0;
    if (!FParse::Param(CommandLine, TEXT("RenderOffscreen"))
        || !FParse::Param(CommandLine, TEXT("d3d12"))
        || FParse::Param(CommandLine, TEXT("nullrhi"))
        || !FParse::Param(CommandLine, TEXT("nop4"))
        || !FParse::Param(CommandLine, TEXT("nosplash"))
        || !FParse::Param(CommandLine, TEXT("UTF8Output"))
        || !FParse::Param(CommandLine, TEXT("stdout"))
        || !FParse::Param(CommandLine, TEXT("FullStdOutLogOutput"))
        || !FParse::Param(CommandLine, TEXT("ForceRes"))
        || !FParse::Param(CommandLine, TEXT("SkipHoleIntro"))
        || !FParse::Value(CommandLine, TEXT("ResX="), ResX)
        || !FParse::Value(CommandLine, TEXT("ResY="), ResY)
        || !FParse::Value(CommandLine, TEXT("Course="), Course)
        || !FParse::Value(CommandLine, TEXT("Hole="), Hole)
        || ResX != ExpectedWidth || ResY != ExpectedHeight
        || Course != TEXT("PineRidge") || Hole != 1)
    {
        OutError = TEXT(
            "rendered D3D12 1920x1080 PineRidge hole-1 offscreen invocation is required");
        return false;
    }

    SavePath = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("SaveGames"),
        FString(SaveSlot) + TEXT(".sav"));
    SavePath = FPaths::ConvertRelativePathToFull(SavePath);
    FPaths::NormalizeFilename(SavePath);
    FString ExpectedSavePath = FPaths::Combine(
        UserDirectory, TEXT("Saved/SaveGames"),
        FString(SaveSlot) + TEXT(".sav"));
    ExpectedSavePath = FPaths::ConvertRelativePathToFull(ExpectedSavePath);
    FPaths::NormalizeFilename(ExpectedSavePath);
    if (!FPaths::IsSamePath(SavePath, ExpectedSavePath))
    {
        OutError = TEXT("the active default profile path did not resolve inside the isolated UserDir");
        return false;
    }
    const bool bSaveExists = IFileManager::Get().FileExists(*SavePath);
    if (Phase != EPhase::ApplyMetaHuman && !bSaveExists)
    {
        OutError = TEXT("phases 2-4 require the prior phase profile in the same UserDir");
        return false;
    }

    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("CharacterFramework/Session8BMetaHumanPackagedAcceptance"),
        PhaseName);
    OutputDirectory = FPaths::ConvertRelativePathToFull(OutputDirectory);
    FPaths::NormalizeDirectoryName(OutputDirectory);
    if (!FPaths::IsUnderDirectory(OutputDirectory, UserDirectory))
    {
        OutError = TEXT("phase evidence output escaped the isolated UserDir");
        return false;
    }
    ReportPath = FPaths::Combine(OutputDirectory, TEXT("PhaseReport.json"));
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    return true;
#endif
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::AcquireRuntime(
    FString& OutError)
{
    GameMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    Golfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    GameInstance = Cast<UDiscGolfTourGameInstance>(GetGameInstance());
    AvatarBackend = Golfer ? Golfer->GetAvatarBackendComponent() : nullptr;
    ThrowAdapter = Golfer ? Golfer->GetRHBHThrowAdapter() : nullptr;
    FrameworkThrow = Golfer
        ? Golfer->FindComponentByClass<UDiscGolfThrowComponent>() : nullptr;
    CanonicalMetaHumanProfile = LoadObject<UDiscGolfAvatarBackendProfile>(
        nullptr,
        DiscGolfAvatarBackendRuntime::MetaHumanDefaultProfileObjectPath);
    if (!GameMode || !PlayerController || !Golfer || !GameInstance
        || !GameInstance->GetProfile() || !AvatarBackend || !ThrowAdapter
        || !FrameworkThrow || !Golfer->GetSkeletalGolferMesh()
        || !Golfer->GetHeldDiscVisual() || !Golfer->GetThrowController()
        || !Golfer->GetDiscBag() || !CanonicalMetaHumanProfile)
    {
        OutError = TEXT(
            "game mode, player, save profile, avatar adapter, throw adapter, framework throw or canonical MetaHuman profile was unavailable");
        return false;
    }
    if (!DiscGolfSaveSchema::IsCurrent(
            GameInstance->GetProfile()->SaveSchemaVersion))
    {
        OutError = TEXT("the in-memory player profile was not schema 10");
        return false;
    }

    FString ProfileReason;
    if (!DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            CanonicalMetaHumanProfile, ProfileReason)
        || CanonicalMetaHumanProfile->PreferredQualityProfileId
            != FName(TEXT("GameplayPerformance"))
        || CanonicalMetaHumanProfile->bAllowRuntimeFaceSculpting)
    {
        OutError = FString::Printf(
            TEXT("the canonical fixed assembled profile is not the accepted GameplayPerformance contract: %s"),
            *ProfileReason);
        return false;
    }

    const FString RhiName = GDynamicRHI
        ? FString(GDynamicRHI->GetName()) : TEXT("Unavailable");
    const FIntPoint ViewportSize = GEngine && GEngine->GameViewport
            && GEngine->GameViewport->Viewport
        ? GEngine->GameViewport->Viewport->GetSizeXY()
        : FIntPoint::ZeroValue;
    if (!FApp::CanEverRender()
        || !GDynamicRHI || RhiName.Contains(TEXT("Null"), ESearchCase::IgnoreCase)
        || !RhiName.Contains(TEXT("D3D12"), ESearchCase::IgnoreCase)
        || GRHIAdapterName.IsEmpty()
        || GRHIAdapterName.Contains(
            TEXT("Microsoft Basic Render"), ESearchCase::IgnoreCase)
        || GRHIAdapterName.Contains(TEXT("Software"), ESearchCase::IgnoreCase)
        || ViewportSize != FIntPoint(ExpectedWidth, ExpectedHeight))
    {
        OutError = FString::Printf(
            TEXT("rendered D3D12 viewport/GPU contract failed (rhi=%s gpu=%s viewport=%dx%d)"),
            *RhiName, *GRHIAdapterName, ViewportSize.X, ViewportSize.Y);
        return false;
    }
    const Scalability::FQualityLevels Quality =
        Scalability::GetQualityLevels();
    if (!FMath::IsNearlyEqual(Quality.ResolutionQuality, 100.0f)
        || Quality.ViewDistanceQuality != 2
        || Quality.AntiAliasingQuality != 2
        || Quality.ShadowQuality != 2
        || Quality.GlobalIlluminationQuality != 2
        || Quality.ReflectionQuality != 2
        || Quality.PostProcessQuality != 2
        || Quality.TextureQuality != 2
        || Quality.EffectsQuality != 2
        || Quality.FoliageQuality != 3
        || Quality.ShadingQuality != 2)
    {
        OutError = TEXT(
            "OmenGameplay1080pHighFoliageV1 scalability values were not active for the rendered acceptance phase");
        return false;
    }

    int32 GolferCount = 0;
    for (TActorIterator<ADiscGolferPawn> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It))
        {
            ++GolferCount;
        }
    }
    if (GolferCount != 1)
    {
        OutError = FString::Printf(
            TEXT("the packaged lane requires exactly one golfer pawn (found %d)"),
            GolferCount);
        return false;
    }

    GameMode->SkipCurrentPresentation();
    InitialCustomization = GameInstance->GetFullCharacterCustomization();
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(
        InitialCustomization);
    LoadSaveBytes(InitialSaveBytes);
    BaselineStrokes = GameMode->GetStrokes();
    BaselineWorldDiscs = CountWorldDiscs();
    BaselineReleaseCommits = ThrowAdapter->GetTotalReleaseCommitCount();
    ThrowAdapter->OnReleaseCommitted.AddUniqueDynamic(
        this,
        &ADiscGolfSession8BMetaHumanPackagedRunner::HandleReleaseCommitted);
    ThrowAdapter->OnThrowRecovered.AddUniqueDynamic(
        this,
        &ADiscGolfSession8BMetaHumanPackagedRunner::HandleThrowRecovered);
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::BeginPhase(
    FString& OutError)
{
    switch (Phase)
    {
        case EPhase::ApplyMetaHuman:
        {
            if (!VerifyDGPresentation(OutError))
            {
                return false;
            }
            ProxySentinelCustomization = InitialCustomization;
            ProxySentinelCustomization.Identity.DisplayName =
                TEXT("Session8B Proxy Sentinel");
            ProxySentinelCustomization.BodyBuild.Muscularity = 0.61f;
            ProxySentinelCustomization.BodyBuild.BodyFat = 0.23f;
            ProxySentinelCustomization.BodyBuild.Chest = 0.37f;
            ProxySentinelCustomization.BodyBuild.Waist = -0.24f;
            ProxySentinelCustomization.BodyBuild.Arms = 0.42f;
            ProxySentinelCustomization.Appearance.SkinTone =
                FLinearColor(0.43f, 0.25f, 0.16f, 1.0f);
            ProxySentinelCustomization.Appearance.EyeColor =
                FLinearColor(0.08f, 0.23f, 0.31f, 1.0f);
            if (!DiscGolfFullCharacterRuntime::ApplyFacePreset(
                    TEXT("face_square"), ProxySentinelCustomization.Face))
            {
                OutError = TEXT(
                    "the frozen face_square proxy sentinel was unavailable");
                return false;
            }
            ProxySentinelCustomization.Hair.HairStyleId =
                TEXT("hair_medium");
            ProxySentinelCustomization.Hair.FacialHairId =
                TEXT("facialhair_beard");
            ProxySentinelCustomization.Hair.EyebrowId =
                TEXT("brow_alt");
            ProxySentinelCustomization.Appearance.ScarId =
                TEXT("scar_proxy");
            ProxySentinelCustomization.Appearance.TattooIds = {
                TEXT("tattoo_proxy")};
            ProxySentinelCustomization.Outfit.Equipped.RemoveAll(
                [](const FDGEquippedOutfitEntry& Entry)
                {
                    return Entry.Slot == EDGOutfitSlot::Headwear;
                });
            FString OutfitError;
            if (!DiscGolfOutfitRuntime::SetSlotSelection(
                    ProxySentinelCustomization.Outfit,
                    EDGOutfitSlot::Headwear,
                    TEXT("proxy_s6_headwear_cap_01"),
                    TEXT("Default"),
                    Golfer->GetOutfitCatalog(),
                    ProxySentinelCustomization.Body,
                    OutfitError))
            {
                OutError = FString::Printf(
                    TEXT("the frozen proxy cap sentinel was unavailable: %s"),
                    *OutfitError);
                return false;
            }
            DiscGolfFullCharacterRuntime::NormalizeForPersistence(
                ProxySentinelCustomization);
            FString Status;
            if (!Golfer->ApplyFullCharacterCustomizationTransactionally(
                    ProxySentinelCustomization, false, Status)
                || !VerifyDGPresentation(OutError)
                || !OpenCreator(OutError))
            {
                if (OutError.IsEmpty())
                {
                    OutError = FString::Printf(
                        TEXT("DG sentinel setup failed: %s"), *Status);
                }
                return false;
            }
            TArray<uint8> SaveAfterSentinelSetup;
            const bool bSaveExistsAfterSentinel =
                LoadSaveBytes(SaveAfterSentinelSetup);
            if (bSaveExistsAfterSentinel != !InitialSaveBytes.IsEmpty()
                || SaveAfterSentinelSetup != InitialSaveBytes)
            {
                OutError = TEXT("transient DG sentinel setup changed the startup profile before Apply");
                return false;
            }
            SetStep(EStep::Phase1CaptureDG);
            return true;
        }

        case EPhase::ReloadCancelFailureSwitchDG:
            if (!VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &InitialCustomization, OutError)
                || !VerifyDiskProfile(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &InitialCustomization, OutError)
                || !VerifyMetaHumanPresentation(OutError)
                || !RunGameplayCandidateIsolationProbe(OutError)
                || !OpenCreator(OutError))
            {
                return false;
            }
            CheckpointSaveBytes = InitialSaveBytes;
            SetStep(EStep::Phase2PreviewDG);
            return true;

        case EPhase::ReloadDGRestoreMetaHumanVisualThrow:
            if (!VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId),
                    &InitialCustomization, OutError)
                || !VerifyDiskProfile(
                    FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId),
                    &InitialCustomization, OutError)
                || !VerifyDGPresentation(OutError))
            {
                return false;
            }
            CheckpointSaveBytes = InitialSaveBytes;
            SetStep(EStep::Phase3CaptureFreshDG);
            return true;

        case EPhase::MetaHumanPerformance:
            if (!VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &InitialCustomization, OutError)
                || !VerifyDiskProfile(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &InitialCustomization, OutError)
                || !VerifyMetaHumanPresentation(OutError))
            {
                return false;
            }
            {
                FPresentationPolicyEvidence Before;
                FPresentationPolicyEvidence After;
                if (!PopulatePresentationPolicyEvidence(
                        EDGMetaHumanPresentationPolicy::GameplayPerformance,
                        Before,
                        OutError)
                    || !OpenCreator(OutError)
                    || !PopulatePresentationPolicyEvidence(
                        EDGMetaHumanPresentationPolicy::CharacterCreator,
                        After,
                        OutError))
                {
                    return false;
                }
                FPresentationPolicyCounters EntryDelta;
                EntryDelta.TransitionSuccess = 1;
                if (!AppendPresentationPolicyProbe(
                        TEXT("phase4_creator_entry"),
                        TEXT("COMMITTED_ACTIVE_TRANSITION"),
                        TEXT("OpenCharacterCreator_BeginCharacterCreatorPreview"),
                        true,
                        true,
                        EntryDelta,
                        false,
                        Before,
                        After,
                        PresentationPolicyTransitionProbes,
                        OutError)
                    || !RunRedundantPresentationPolicyProbe(
                        TEXT("phase4_creator_redundant_no_op"),
                        EDGMetaHumanPresentationPolicy::CharacterCreator,
                        OutError))
                {
                    return false;
                }
            }
            PlayerController->ZoomCharacterCreatorPreview(-100.0f);
            PlayerController->ZoomCharacterCreatorPreview(-80.0f);
            PerformanceWarmupStartedSeconds = FPlatformTime::Seconds();
            CheckpointSaveBytes = InitialSaveBytes;
            SetStep(EStep::Phase4WarmupCreator);
            return true;

        case EPhase::Invalid:
        default:
            OutError = TEXT("the requested phase was not resolved");
            return false;
    }
}

void ADiscGolfSession8BMetaHumanPackagedRunner::SetStep(EStep NewStep)
{
    Step = NewStep;
    StepStartedSeconds = FPlatformTime::Seconds();
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::VerifyCurrentCustomization(
    FName BackendId,
    const FDGFullCharacterCustomization* Expected,
    FString& OutError) const
{
    if (!GameInstance || !Golfer)
    {
        OutError = TEXT("the current customization authorities disappeared");
        return false;
    }
    FDGFullCharacterCustomization InMemory =
        GameInstance->GetFullCharacterCustomization();
    FDGFullCharacterCustomization OnPawn =
        Golfer->GetCurrentFullCharacterCustomization();
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(InMemory);
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(OnPawn);
    if (InMemory.AvatarBackendId != BackendId
        || OnPawn.AvatarBackendId != BackendId
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            InMemory, OnPawn))
    {
        OutError = TEXT(
            "game-instance and pawn customization/backend authorities diverged");
        return false;
    }
    if (Expected)
    {
        FDGFullCharacterCustomization NormalizedExpected = *Expected;
        DiscGolfFullCharacterRuntime::NormalizeForPersistence(
            NormalizedExpected);
        if (!DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
                InMemory, NormalizedExpected))
        {
            OutError = TEXT("current customization did not equal the expected complete payload");
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::VerifyDiskProfile(
    FName BackendId,
    const FDGFullCharacterCustomization* Expected,
    FString& OutError) const
{
    USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(SaveSlot, 0);
    UDiscGolfSaveGame* Profile = Cast<UDiscGolfSaveGame>(Loaded);
    if (!Profile || !DiscGolfSaveSchema::IsCurrent(Profile->SaveSchemaVersion))
    {
        OutError = TEXT("isolated disk profile was absent or not schema 10");
        return false;
    }
    FDGFullCharacterCustomization Actual = Profile->CharacterCustomization;
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(Actual);
    if (Actual.AvatarBackendId != BackendId)
    {
        OutError = FString::Printf(
            TEXT("isolated disk profile backend was %s instead of %s"),
            *Actual.AvatarBackendId.ToString(), *BackendId.ToString());
        return false;
    }
    if (Expected)
    {
        FDGFullCharacterCustomization NormalizedExpected = *Expected;
        DiscGolfFullCharacterRuntime::NormalizeForPersistence(
            NormalizedExpected);
        if (!DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
                Actual, NormalizedExpected))
        {
            OutError = TEXT("isolated disk profile did not preserve the complete expected payload");
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::VerifyDGPresentation(
    FString& OutError) const
{
    UClass* const MetaHumanVisualClass = CanonicalMetaHumanProfile
        ? CanonicalMetaHumanProfile->VisualActorClass.LoadSynchronous()
        : nullptr;
    if (!Golfer || !AvatarBackend
        || !MetaHumanVisualClass
        || !Golfer->IsDGProxyPresentationVisible()
        || AvatarBackend->IsVisualBackendReady()
        || AvatarBackend->GetActiveVisualActor()
        || AvatarBackend->GetActiveBackendProfile()
        || CountActiveVisualActors(MetaHumanVisualClass) != 0)
    {
        OutError = TEXT(
            "DGMaster proxy was not the sole visible presentation or a stale backend actor remained");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::VerifyMetaHumanPresentation(
    FString& OutError) const
{
    if (!Golfer || !AvatarBackend || !CanonicalMetaHumanProfile)
    {
        OutError = TEXT("MetaHuman presentation authorities disappeared");
        return false;
    }
    AActor* VisualActor = AvatarBackend->GetActiveVisualActor();
    USkeletalMeshComponent* VisualBody = AvatarBackend->GetVerifiedVisualBody();
    USkeletalMeshComponent* VisualHead = AvatarBackend->GetVerifiedVisualHead();
    USkeletalMeshComponent* AnimationSource =
        AvatarBackend->GetActiveAnimationSourceMesh();
    UDiscGolfMetaHumanRetargetAnimInstance* RetargetInstance =
        VisualBody
        ? Cast<UDiscGolfMetaHumanRetargetAnimInstance>(
            VisualBody->GetAnimInstance())
        : nullptr;
    UIKRetargeter* CanonicalRetargeter = Cast<UIKRetargeter>(
        CanonicalMetaHumanProfile->RetargetAsset.LoadSynchronous());
    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    const USceneComponent* Root = IsValid(VisualActor)
        ? VisualActor->GetRootComponent() : nullptr;
    if (Golfer->IsDGProxyPresentationVisible()
        || !AvatarBackend->IsVisualBackendReady()
        || !IsValid(VisualActor) || VisualActor->IsHidden()
        || CountActiveVisualActors(VisualActor->GetClass()) != 1
        || VisualActor->GetActorEnableCollision()
        || VisualActor->GetOwner() != Golfer
        || AvatarBackend->GetActiveBackendProfile()
            != CanonicalMetaHumanProfile
        || CanonicalMetaHumanProfile->PreferredQualityProfileId
            != FName(TEXT("GameplayPerformance"))
        || Current.AvatarBackendId
            != FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId)
        || Current.Hair.FacialHairId
            != FName(TEXT("facialhair_beard"))
        || AnimationSource != Golfer->GetSkeletalGolferMesh()
        || !IsValid(CanonicalRetargeter)
        || !IsValid(RetargetInstance)
        || RetargetInstance->GetClass()
            != UDiscGolfMetaHumanRetargetAnimInstance::StaticClass()
        || !RetargetInstance->IsConfiguredFor(
            CanonicalRetargeter, AnimationSource)
        || !IsValid(VisualBody) || !IsValid(VisualHead)
        || VisualBody == VisualHead
        || !VisualBody->GetSkeletalMeshAsset()
        || !VisualHead->GetSkeletalMeshAsset()
        || !Root || !Root->IsAttachedTo(AnimationSource)
        || VisualActor->IsA<APawn>()
        || VisualActor->IsA<AController>()
        || VisualActor->FindComponentByClass<UMovementComponent>()
        || VisualActor->FindComponentByClass<UInputComponent>())
    {
        OutError = TEXT(
            "requested MetaHuman was not a verified visible presentation-only actor over the preserved DG animation source");
        return false;
    }
    const FDGAvatarBackendState State =
        AvatarBackend->GetAvatarBackendState();
    if (!State.bVisualReady
        || State.BackendId != FName(
            DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId))
    {
        OutError = TEXT("active avatar-backend state did not prove the canonical MetaHuman ID");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    PopulatePresentationPolicyEvidence(
        EDGMetaHumanPresentationPolicy ExpectedPolicy,
        FPresentationPolicyEvidence& OutEvidence,
        FString& OutError) const
{
    OutEvidence = FPresentationPolicyEvidence();
    if (!AvatarBackend || !CanonicalMetaHumanProfile || !Golfer)
    {
        OutError = TEXT(
            "presentation-policy evidence lost its backend, profile, or golfer authority");
        return false;
    }

    AActor* const VisualActor = AvatarBackend->GetActiveVisualActor();
    USkeletalMeshComponent* const AnimationSource =
        AvatarBackend->GetActiveAnimationSourceMesh();
    if (!IsValid(VisualActor) || !IsValid(AnimationSource)
        || AvatarBackend->GetActiveBackendProfile()
            != CanonicalMetaHumanProfile)
    {
        OutError = TEXT(
            "presentation-policy evidence lacked the canonical active visual/profile/source tuple");
        return false;
    }

    OutEvidence.QualityProfileId =
        CanonicalMetaHumanProfile->PreferredQualityProfileId.ToString();
    OutEvidence.RequestedPolicy = PresentationPolicyName(
        AvatarBackend->GetRequestedPresentationPolicy());
    OutEvidence.VerifiedPolicy = PresentationPolicyName(
        AvatarBackend->GetVerifiedPresentationPolicy());
    OutEvidence.bPolicyVerified =
        AvatarBackend->IsPresentationPolicyVerified();
    OutEvidence.VerifiedForcedLOD =
        AvatarBackend->GetVerifiedPresentationForcedLOD();
    OutEvidence.PolicyStatus =
        AvatarBackend->GetPresentationPolicyStatus();
    OutEvidence.ActiveVisualActor = VisualActor->GetPathName();
    OutEvidence.AnimationSourceComponent = AnimationSource->GetPathName();
    OutEvidence.Counters.TransitionSuccess =
        AvatarBackend->GetPresentationPolicyTransitionSuccessCount();
    OutEvidence.Counters.TransitionFailure =
        AvatarBackend->GetPresentationPolicyTransitionFailureCount();
    OutEvidence.Counters.RollbackSuccess =
        AvatarBackend->GetPresentationPolicyRollbackSuccessCount();
    OutEvidence.Counters.RollbackFailure =
        AvatarBackend->GetPresentationPolicyRollbackFailureCount();

    const FString ExpectedPolicyName = PresentationPolicyName(ExpectedPolicy);
    const bool bCreator = ExpectedPolicy
        == EDGMetaHumanPresentationPolicy::CharacterCreator;
    const bool bGameplay = ExpectedPolicy
        == EDGMetaHumanPresentationPolicy::GameplayPerformance;
    const int32 ExpectedForcedLOD = bCreator ? 0 : 2;
    if ((!bCreator && !bGameplay)
        || OutEvidence.QualityProfileId != TEXT("GameplayPerformance")
        || OutEvidence.RequestedPolicy != ExpectedPolicyName
        || OutEvidence.VerifiedPolicy != ExpectedPolicyName
        || !OutEvidence.bPolicyVerified
        || OutEvidence.VerifiedForcedLOD != ExpectedForcedLOD
        || OutEvidence.PolicyStatus.IsEmpty()
        || OutEvidence.Counters.TransitionSuccess < 0
        || OutEvidence.Counters.TransitionFailure != 0
        || OutEvidence.Counters.RollbackSuccess != 0
        || OutEvidence.Counters.RollbackFailure != 0)
    {
        OutError = FString::Printf(
            TEXT("%s policy public state was not exact: profile=%s requested=%s verified=%s is_verified=%d forced_lod=%d status=%s counters=%d/%d/%d/%d"),
            *ExpectedPolicyName,
            *OutEvidence.QualityProfileId,
            *OutEvidence.RequestedPolicy,
            *OutEvidence.VerifiedPolicy,
            OutEvidence.bPolicyVerified ? 1 : 0,
            OutEvidence.VerifiedForcedLOD,
            *OutEvidence.PolicyStatus,
            OutEvidence.Counters.TransitionSuccess,
            OutEvidence.Counters.TransitionFailure,
            OutEvidence.Counters.RollbackSuccess,
            OutEvidence.Counters.RollbackFailure);
        return false;
    }

    TInlineComponentArray<ULODSyncComponent*> LODSyncComponents;
    VisualActor->GetComponents(LODSyncComponents);
    if (LODSyncComponents.Num() != 1
        || !IsValid(LODSyncComponents[0]))
    {
        OutError = FString::Printf(
            TEXT("presentation-policy evidence found %d LODSync components instead of exactly one"),
            LODSyncComponents.Num());
        return false;
    }
    ULODSyncComponent* const LODSync = LODSyncComponents[0];
    FLODSyncPolicyEvidence& LODSyncEvidence = OutEvidence.LODSync;
    LODSyncEvidence.ComponentPath = LODSync->GetPathName();
    LODSyncEvidence.NumLODs = LODSync->NumLODs;
    LODSyncEvidence.MinLOD = LODSync->MinLOD;
    LODSyncEvidence.ForcedLOD = LODSync->ForcedLOD;
    LODSyncEvidence.bRegistered = LODSync->IsRegistered();
    LODSyncEvidence.bTickEnabled = LODSync->IsComponentTickEnabled();
    LODSyncEvidence.bTickEvenWhenPaused =
        LODSync->PrimaryComponentTick.bTickEvenWhenPaused;
    LODSyncEvidence.bContractValid =
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(
                LODSync, LODSyncEvidence.ContractStatus);

    TInlineComponentArray<UActorComponent*> AllComponents;
    VisualActor->GetComponents(AllComponents);
    auto HasNamedComponent = [&AllComponents](FName Name)
    {
        return AllComponents.ContainsByPredicate(
            [Name](const UActorComponent* Component)
            {
                return IsValid(Component) && Component->GetFName() == Name;
            });
    };
    for (const FComponentSync& Entry : LODSync->ComponentsToSync)
    {
        FLODSyncComponentEntryEvidence Evidence;
        Evidence.Name = Entry.Name.ToString();
        Evidence.SyncOption = SyncOptionName(Entry.SyncOption);
        Evidence.bComponentPresent = HasNamedComponent(Entry.Name);
        LODSyncEvidence.ComponentsToSync.Add(MoveTemp(Evidence));
        if (!LODSyncEvidence.ComponentsToSync.Last().bComponentPresent)
        {
            LODSyncEvidence.MissingDeclaredComponents.Add(
                Entry.Name.ToString());
        }
    }
    static const FName OrderedMappingNames[] = {
        FName(TEXT("Hair")),
        FName(TEXT("Beard")),
        FName(TEXT("Mustache")),
        FName(TEXT("Eyebrows")),
        FName(TEXT("SkeletalMesh")),
        FName(TEXT("SkeletalMesh1")),
        FName(TEXT("SkeletalMesh2")),
    };
    for (const FName MappingName : OrderedMappingNames)
    {
        const FLODMappingData* const Mapping =
            LODSync->CustomLODMapping.Find(MappingName);
        FLODSyncMappingEvidence Evidence;
        Evidence.Name = MappingName.ToString();
        if (Mapping)
        {
            Evidence.Mapping = Mapping->Mapping;
        }
        LODSyncEvidence.CustomLODMapping.Add(MoveTemp(Evidence));
    }
    static const TCHAR* ExpectedSyncNames[] = {
        TEXT("Body"), TEXT("Face"), TEXT("SkeletalMesh"),
        TEXT("SkeletalMesh1"), TEXT("SkeletalMesh2"), TEXT("Hair"),
        TEXT("Eyebrows"), TEXT("Mustache"), TEXT("Beard")};
    static const TCHAR* ExpectedSyncOptions[] = {
        TEXT("Drive"), TEXT("Drive"), TEXT("Passive"),
        TEXT("Passive"), TEXT("Passive"), TEXT("Passive"),
        TEXT("Passive"), TEXT("Passive"), TEXT("Passive")};
    static const TArray<int32> HairMapping = {3, 5, 7};
    static const TArray<int32> SkeletalMapping = {1, 2, 3};
    bool bStaticSchemaExact = LODSyncEvidence.bContractValid
        && LODSyncEvidence.NumLODs == 3
        && LODSyncEvidence.MinLOD == 0
        && LODSyncEvidence.ForcedLOD == ExpectedForcedLOD
        && LODSyncEvidence.bRegistered
        && LODSyncEvidence.bTickEnabled
        && LODSyncEvidence.bTickEvenWhenPaused == bCreator
        && LODSyncEvidence.ComponentsToSync.Num()
            == UE_ARRAY_COUNT(ExpectedSyncNames)
        && LODSyncEvidence.CustomLODMapping.Num()
            == UE_ARRAY_COUNT(OrderedMappingNames)
        && LODSyncEvidence.MissingDeclaredComponents.Num() == 2
        && LODSyncEvidence.MissingDeclaredComponents[0]
            == TEXT("SkeletalMesh1")
        && LODSyncEvidence.MissingDeclaredComponents[1]
            == TEXT("SkeletalMesh2");
    for (int32 Index = 0;
         bStaticSchemaExact && Index < UE_ARRAY_COUNT(ExpectedSyncNames);
         ++Index)
    {
        const FLODSyncComponentEntryEvidence& Entry =
            LODSyncEvidence.ComponentsToSync[Index];
        const bool bExpectedPresent = Index != 3 && Index != 4;
        bStaticSchemaExact = Entry.Name == ExpectedSyncNames[Index]
            && Entry.SyncOption == ExpectedSyncOptions[Index]
            && Entry.bComponentPresent == bExpectedPresent;
    }
    for (int32 Index = 0;
         bStaticSchemaExact && Index < LODSyncEvidence.CustomLODMapping.Num();
         ++Index)
    {
        const bool bHairFamily = Index < 4;
        bStaticSchemaExact =
            LODSyncEvidence.CustomLODMapping[Index].Name
                == OrderedMappingNames[Index].ToString()
            && LODSyncEvidence.CustomLODMapping[Index].Mapping
                == (bHairFamily ? HairMapping : SkeletalMapping);
    }
    if (!bStaticSchemaExact)
    {
        OutError = FString::Printf(
            TEXT("%s LODSync schema/effective tier was not the exact 3-tier nine-entry seven-map contract: %s"),
            *ExpectedPolicyName,
            *LODSyncEvidence.ContractStatus);
        return false;
    }

    TInlineComponentArray<USkeletalMeshComponent*> AllSkeletalMeshes;
    VisualActor->GetComponents(AllSkeletalMeshes);
    AllSkeletalMeshes.RemoveAll(
        [](const USkeletalMeshComponent* Mesh)
        {
            return !IsValid(Mesh)
                || Mesh->IsA<
                    UDiscGolfMetaHumanOutfitRequiredBonesComponent>();
        });
    static const FName ExpectedSkeletalNames[] = {
        FName(TEXT("Body")),
        FName(TEXT("Face")),
        FName(TEXT("SkeletalMesh")),
    };
    if (AllSkeletalMeshes.Num() != UE_ARRAY_COUNT(ExpectedSkeletalNames))
    {
        OutError = FString::Printf(
            TEXT("%s policy found %d non-helper skeletal components instead of the exact Body/Face/Outfit set"),
            *ExpectedPolicyName,
            AllSkeletalMeshes.Num());
        return false;
    }
    for (int32 ExpectedIndex = 0;
         ExpectedIndex < UE_ARRAY_COUNT(ExpectedSkeletalNames);
         ++ExpectedIndex)
    {
        const FName ExpectedName = ExpectedSkeletalNames[ExpectedIndex];
        TArray<USkeletalMeshComponent*> Matches = AllSkeletalMeshes.FilterByPredicate(
            [ExpectedName](const USkeletalMeshComponent* Mesh)
            {
                return Mesh->GetFName() == ExpectedName;
            });
        if (Matches.Num() != 1)
        {
            OutError = FString::Printf(
                TEXT("%s policy did not expose exactly one skeletal component named %s"),
                *ExpectedPolicyName,
                *ExpectedName.ToString());
            return false;
        }
        USkeletalMeshComponent* const Mesh = Matches[0];
        const ILODSyncInterface* const LODInterface =
            static_cast<const ILODSyncInterface*>(Mesh);
        FSkeletalPolicyEvidence Evidence;
        Evidence.Name = ExpectedName.ToString();
        Evidence.ComponentPath = Mesh->GetPathName();
        Evidence.SkeletalMeshPath = Mesh->GetSkeletalMeshAsset()
            ? Mesh->GetSkeletalMeshAsset()->GetPathName() : FString();
        Evidence.VisibilityTickOption = VisibilityTickOptionName(
            Mesh->VisibilityBasedAnimTickOption);
        Evidence.LODCount = Mesh->GetSkeletalMeshAsset()
            ? Mesh->GetSkeletalMeshAsset()->GetLODNum() : 0;
        Evidence.ExpectedMappedLOD = ExpectedIndex < 2
            ? ExpectedForcedLOD : (bCreator ? 1 : 3);
        Evidence.ForcedLODLegacyOneBased = Mesh->GetForcedLOD();
        Evidence.ForceRenderedLOD = LODInterface
            ? LODInterface->GetForceRenderedLOD() : INDEX_NONE;
        Evidence.ForceStreamedLOD = LODInterface
            ? LODInterface->GetForceStreamedLOD() : INDEX_NONE;
        const FSkeletalMeshObject* const MeshObject = Mesh->GetMeshObject();
        Evidence.ActualRenderedLOD = MeshObject
            ? MeshObject->GetLOD() : INDEX_NONE;
        Evidence.PredictedLOD = Mesh->GetPredictedLODLevel();
        Evidence.DesiredSyncLOD = LODInterface
            ? LODInterface->GetDesiredSyncLOD() : INDEX_NONE;
        Evidence.BestAvailableLOD = LODInterface
            ? LODInterface->GetBestAvailableLOD() : INDEX_NONE;
        Evidence.bRegistered = Mesh->IsRegistered();
        Evidence.bVisiblePresentation = IsVisiblePresentationMesh(Mesh);
        Evidence.bTickEnabled = Mesh->IsComponentTickEnabled();
        Evidence.bTickEvenWhenPaused =
            Mesh->PrimaryComponentTick.bTickEvenWhenPaused;
        Evidence.bUpdateRateOptimizations =
            Mesh->bEnableUpdateRateOptimizations;
        Evidence.bSourceTickPrerequisite =
            HasSourceTickPrerequisite(Mesh, AnimationSource);
        const FString ExpectedTickOption = bCreator
            ? TEXT("AlwaysTickPoseAndRefreshBones")
            : TEXT("OnlyTickPoseWhenRendered");
        if (!LODInterface || Evidence.SkeletalMeshPath.IsEmpty()
            || Evidence.LODCount <= Evidence.ExpectedMappedLOD
            || Evidence.ForcedLODLegacyOneBased
                != Evidence.ExpectedMappedLOD + 1
            || Evidence.ForceRenderedLOD != Evidence.ExpectedMappedLOD
            || Evidence.ForceStreamedLOD != Evidence.ExpectedMappedLOD
            || Evidence.BestAvailableLOD < 0
            || Evidence.BestAvailableLOD > Evidence.ExpectedMappedLOD
            || !Evidence.bRegistered || !Evidence.bVisiblePresentation
            || !Evidence.bTickEnabled
            || Evidence.bTickEvenWhenPaused != bCreator
            || Evidence.bUpdateRateOptimizations
            || !Evidence.bSourceTickPrerequisite
            || Evidence.VisibilityTickOption != ExpectedTickOption)
        {
            OutError = FString::Printf(
                TEXT("%s skeletal policy was not exact for %s: expected_lod=%d forced_legacy=%d rendered=%d streamed=%d best=%d tick=%d paused=%d uro=%d option=%s prerequisite=%d"),
                *ExpectedPolicyName,
                *Evidence.Name,
                Evidence.ExpectedMappedLOD,
                Evidence.ForcedLODLegacyOneBased,
                Evidence.ForceRenderedLOD,
                Evidence.ForceStreamedLOD,
                Evidence.BestAvailableLOD,
                Evidence.bTickEnabled ? 1 : 0,
                Evidence.bTickEvenWhenPaused ? 1 : 0,
                Evidence.bUpdateRateOptimizations ? 1 : 0,
                *Evidence.VisibilityTickOption,
                Evidence.bSourceTickPrerequisite ? 1 : 0);
            return false;
        }
        OutEvidence.SkeletalComponents.Add(MoveTemp(Evidence));
    }

    TInlineComponentArray<UGroomComponent*> AllGrooms;
    VisualActor->GetComponents(AllGrooms);
    AllGrooms.RemoveAll(
        [](const UGroomComponent* Groom)
        {
            return !IsValid(Groom);
        });
    static const FName ExpectedGroomNames[] = {
        FName(TEXT("Hair")),
        FName(TEXT("Eyebrows")),
        FName(TEXT("Fuzz")),
        FName(TEXT("Eyelashes")),
        FName(TEXT("Mustache")),
        FName(TEXT("Beard")),
    };
    if (AllGrooms.Num() != UE_ARRAY_COUNT(ExpectedGroomNames))
    {
        OutError = FString::Printf(
            TEXT("%s policy found %d Groom components instead of the exact six-component presentation set"),
            *ExpectedPolicyName,
            AllGrooms.Num());
        return false;
    }
    for (const FName ExpectedName : ExpectedGroomNames)
    {
        TArray<UGroomComponent*> Matches = AllGrooms.FilterByPredicate(
            [ExpectedName](const UGroomComponent* Groom)
            {
                return Groom->GetFName() == ExpectedName;
            });
        if (Matches.Num() != 1)
        {
            OutError = FString::Printf(
                TEXT("%s policy did not expose exactly one Groom named %s"),
                *ExpectedPolicyName,
                *ExpectedName.ToString());
            return false;
        }
        UGroomComponent* const Groom = Matches[0];
        const ILODSyncInterface* const LODInterface =
            static_cast<const ILODSyncInterface*>(Groom);
        FGroomPolicyEvidence Evidence;
        Evidence.Name = ExpectedName.ToString();
        Evidence.ComponentPath = Groom->GetPathName();
        Evidence.GroomAssetPath = Groom->GroomAsset
            ? Groom->GroomAsset->GetPathName() : FString();
        Evidence.bMappedByLODSync =
            ExpectedName == FName(TEXT("Hair"))
            || ExpectedName == FName(TEXT("Eyebrows"))
            || ExpectedName == FName(TEXT("Mustache"))
            || ExpectedName == FName(TEXT("Beard"));
        Evidence.ExpectedMappedLOD = Evidence.bMappedByLODSync
            ? (bCreator ? 3 : 7) : INDEX_NONE;
        Evidence.ForcedLOD = Groom->GetForcedLOD();
        Evidence.ForceRenderedLOD = LODInterface
            ? LODInterface->GetForceRenderedLOD() : INDEX_NONE;
        Evidence.ForceStreamedLOD = LODInterface
            ? LODInterface->GetForceStreamedLOD() : INDEX_NONE;
        Evidence.DesiredSyncLOD = Groom->GetDesiredSyncLOD();
        Evidence.BestAvailableLOD = Groom->GetBestAvailableLOD();
        Evidence.bRegistered = Groom->IsRegistered();
        Evidence.bTickEnabled = Groom->IsComponentTickEnabled();
        Evidence.bTickEvenWhenPaused =
            Groom->PrimaryComponentTick.bTickEvenWhenPaused;
        if (!LODInterface || !Evidence.bRegistered
            || !Evidence.bTickEnabled
            || Evidence.bTickEvenWhenPaused != bCreator
            || (Evidence.bMappedByLODSync
                && (Evidence.ForcedLOD != Evidence.ExpectedMappedLOD
                    || Evidence.ForceRenderedLOD
                        != Evidence.ExpectedMappedLOD
                    || Evidence.ForceStreamedLOD != INDEX_NONE)))
        {
            OutError = FString::Printf(
                TEXT("%s Groom policy was not exact for %s: mapped=%d expected=%d forced=%d rendered=%d streamed=%d tick=%d paused=%d"),
                *ExpectedPolicyName,
                *Evidence.Name,
                Evidence.bMappedByLODSync ? 1 : 0,
                Evidence.ExpectedMappedLOD,
                Evidence.ForcedLOD,
                Evidence.ForceRenderedLOD,
                Evidence.ForceStreamedLOD,
                Evidence.bTickEnabled ? 1 : 0,
                Evidence.bTickEvenWhenPaused ? 1 : 0);
            return false;
        }
        OutEvidence.GroomComponents.Add(MoveTemp(Evidence));
    }

    TInlineComponentArray<
        UDiscGolfMetaHumanOutfitRequiredBonesComponent*> Helpers;
    VisualActor->GetComponents(Helpers);
    if (Helpers.Num() != 1 || !IsValid(Helpers[0]))
    {
        OutError = FString::Printf(
            TEXT("%s policy found %d Outfit required-bones helpers instead of exactly one"),
            *ExpectedPolicyName,
            Helpers.Num());
        return false;
    }
    UDiscGolfMetaHumanOutfitRequiredBonesComponent* const Helper = Helpers[0];
    USkeletalMeshComponent** const BodyMatch =
        AllSkeletalMeshes.FindByPredicate(
        [](const USkeletalMeshComponent* Mesh)
        {
            return Mesh->GetFName() == FName(TEXT("Body"));
        });
    USkeletalMeshComponent** const OutfitMatch =
        AllSkeletalMeshes.FindByPredicate(
        [](const USkeletalMeshComponent* Mesh)
        {
            return Mesh->GetFName() == FName(TEXT("SkeletalMesh"));
        });
    USkeletalMeshComponent* const Body = BodyMatch ? *BodyMatch : nullptr;
    USkeletalMeshComponent* const Outfit = OutfitMatch ? *OutfitMatch : nullptr;
    FOutfitRequiredBonesHelperEvidence& HelperEvidence =
        OutEvidence.OutfitRequiredBonesHelper;
    HelperEvidence.ComponentPath = Helper->GetPathName();
    HelperEvidence.LeaderPoseComponentPath =
        Helper->LeaderPoseComponent.IsValid()
        ? Helper->LeaderPoseComponent->GetPathName() : FString();
    HelperEvidence.ConfiguredActualOutfitLOD =
        Helper->GetConfiguredActualOutfitLOD();
    HelperEvidence.ConfiguredPredictedOutfitLOD =
        Helper->GetConfiguredPredictedOutfitLOD();
    HelperEvidence.ConfiguredOutfitBoneCount =
        Helper->GetConfiguredOutfitBoneCount();
    HelperEvidence.ConfiguredReadyOutfitLODCount =
        Helper->GetConfiguredReadyOutfitLODCount();
    HelperEvidence.RequiredLeaderBoneCount =
        Helper->GetRequiredLeaderBoneCount();
    HelperEvidence.MappedOutfitUsedLeaderBoneCount =
        Helper->GetMappedOutfitUsedLeaderBoneCount();
    HelperEvidence.bRegistered = Helper->IsRegistered();
    HelperEvidence.bConfiguredForBodyAndOutfit =
        Helper->IsConfiguredFor(Outfit, Body);
    HelperEvidence.bAssetless = Helper->GetSkinnedAsset() == nullptr;
    HelperEvidence.bVisible = Helper->IsVisible();
    HelperEvidence.bHiddenInGame = Helper->bHiddenInGame;
    HelperEvidence.bShouldRender = Helper->ShouldRender();
    HelperEvidence.bRenderInMainPass = Helper->bRenderInMainPass;
    HelperEvidence.bVisibleInSceneCaptureOnly =
        Helper->bVisibleInSceneCaptureOnly;
    HelperEvidence.bCanEverTick = Helper->PrimaryComponentTick.bCanEverTick;
    HelperEvidence.bTickEnabled = Helper->IsComponentTickEnabled();
    HelperEvidence.bCollisionDisabled = Helper->GetCollisionEnabled()
        == ECollisionEnabled::NoCollision;
    HelperEvidence.bGenerateOverlapEvents = Helper->GetGenerateOverlapEvents();
    HelperEvidence.bRegisteredAsBodyFollower = IsValid(Body)
        && Body->GetFollowerPoseComponents().ContainsByPredicate(
            [Helper](const TWeakObjectPtr<USkinnedMeshComponent>& Follower)
            {
                return Follower.Get() == Helper;
            });
    if (!IsValid(Body) || !IsValid(Outfit)
        || !HelperEvidence.bRegistered
        || !HelperEvidence.bConfiguredForBodyAndOutfit
        || !HelperEvidence.bAssetless || HelperEvidence.bVisible
        || !HelperEvidence.bHiddenInGame || HelperEvidence.bShouldRender
        || HelperEvidence.bRenderInMainPass
        || HelperEvidence.bVisibleInSceneCaptureOnly
        || HelperEvidence.bCanEverTick || HelperEvidence.bTickEnabled
        || !HelperEvidence.bCollisionDisabled
        || HelperEvidence.bGenerateOverlapEvents
        || !HelperEvidence.bRegisteredAsBodyFollower
        || HelperEvidence.LeaderPoseComponentPath != Body->GetPathName()
        || HelperEvidence.ConfiguredActualOutfitLOD < INDEX_NONE
        || HelperEvidence.ConfiguredPredictedOutfitLOD < 0
        || HelperEvidence.ConfiguredOutfitBoneCount <= 0
        || HelperEvidence.ConfiguredReadyOutfitLODCount <= 0
        || HelperEvidence.RequiredLeaderBoneCount <= 0
        || HelperEvidence.MappedOutfitUsedLeaderBoneCount <= 0)
    {
        OutError = FString::Printf(
            TEXT("%s Outfit required-bones helper identity/config/render/tick contract was not exact: helper=%s leader=%s registered=%d configured=%d assetless=%d visible=%d hidden=%d should_render=%d main_pass=%d scene_only=%d can_tick=%d tick=%d follower=%d"),
            *ExpectedPolicyName,
            *HelperEvidence.ComponentPath,
            *HelperEvidence.LeaderPoseComponentPath,
            HelperEvidence.bRegistered ? 1 : 0,
            HelperEvidence.bConfiguredForBodyAndOutfit ? 1 : 0,
            HelperEvidence.bAssetless ? 1 : 0,
            HelperEvidence.bVisible ? 1 : 0,
            HelperEvidence.bHiddenInGame ? 1 : 0,
            HelperEvidence.bShouldRender ? 1 : 0,
            HelperEvidence.bRenderInMainPass ? 1 : 0,
            HelperEvidence.bVisibleInSceneCaptureOnly ? 1 : 0,
            HelperEvidence.bCanEverTick ? 1 : 0,
            HelperEvidence.bTickEnabled ? 1 : 0,
            HelperEvidence.bRegisteredAsBodyFollower ? 1 : 0);
        return false;
    }

    FString& Stable = OutEvidence.StableStateFingerprint;
    Stable = FString::Printf(
        TEXT("%s|%s|%s|%d|%s|%s|%d|%s|%d|%d|%d|%d|%d"),
        *OutEvidence.QualityProfileId,
        *OutEvidence.RequestedPolicy,
        *OutEvidence.VerifiedPolicy,
        OutEvidence.bPolicyVerified ? 1 : 0,
        *OutEvidence.ActiveVisualActor,
        *OutEvidence.AnimationSourceComponent,
        OutEvidence.VerifiedForcedLOD,
        *LODSyncEvidence.ComponentPath,
        LODSyncEvidence.NumLODs,
        LODSyncEvidence.MinLOD,
        LODSyncEvidence.ForcedLOD,
        LODSyncEvidence.bTickEnabled ? 1 : 0,
        LODSyncEvidence.bTickEvenWhenPaused ? 1 : 0);
    Stable += TEXT("|PSTATUS:");
    Stable += OutEvidence.PolicyStatus;
    for (const FLODSyncComponentEntryEvidence& Entry
         : LODSyncEvidence.ComponentsToSync)
    {
        Stable += FString::Printf(TEXT("|S:%s:%s:%d"),
            *Entry.Name, *Entry.SyncOption,
            Entry.bComponentPresent ? 1 : 0);
    }
    for (const FLODSyncMappingEvidence& Mapping
         : LODSyncEvidence.CustomLODMapping)
    {
        Stable += FString::Printf(TEXT("|M:%s"), *Mapping.Name);
        for (const int32 LOD : Mapping.Mapping)
        {
            Stable += FString::Printf(TEXT(":%d"), LOD);
        }
    }
    for (const FSkeletalPolicyEvidence& Mesh
         : OutEvidence.SkeletalComponents)
    {
        Stable += FString::Printf(
            TEXT("|K:%s:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%s:%d:%d"),
            *Mesh.Name,
            *Mesh.ComponentPath,
            *Mesh.SkeletalMeshPath,
            Mesh.ExpectedMappedLOD,
            Mesh.ForcedLODLegacyOneBased,
            Mesh.ForceRenderedLOD,
            Mesh.ForceStreamedLOD,
            Mesh.bRegistered ? 1 : 0,
            Mesh.bVisiblePresentation ? 1 : 0,
            Mesh.bTickEnabled ? 1 : 0,
            Mesh.bTickEvenWhenPaused ? 1 : 0,
            *Mesh.VisibilityTickOption,
            Mesh.bSourceTickPrerequisite ? 1 : 0,
            Mesh.bUpdateRateOptimizations ? 1 : 0);
    }
    for (const FGroomPolicyEvidence& Groom
         : OutEvidence.GroomComponents)
    {
        Stable += FString::Printf(
            TEXT("|G:%s:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d"),
            *Groom.Name,
            *Groom.ComponentPath,
            *Groom.GroomAssetPath,
            Groom.bMappedByLODSync ? 1 : 0,
            Groom.ExpectedMappedLOD,
            Groom.ForcedLOD,
            Groom.ForceRenderedLOD,
            Groom.ForceStreamedLOD,
            Groom.bRegistered ? 1 : 0,
            Groom.bTickEnabled ? 1 : 0,
            Groom.bTickEvenWhenPaused ? 1 : 0);
    }
    Stable += FString::Printf(
        TEXT("|H:%s:%s:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d:%d"),
        *HelperEvidence.ComponentPath,
        *HelperEvidence.LeaderPoseComponentPath,
        HelperEvidence.ConfiguredActualOutfitLOD,
        HelperEvidence.ConfiguredPredictedOutfitLOD,
        HelperEvidence.ConfiguredOutfitBoneCount,
        HelperEvidence.ConfiguredReadyOutfitLODCount,
        HelperEvidence.RequiredLeaderBoneCount,
        HelperEvidence.MappedOutfitUsedLeaderBoneCount,
        HelperEvidence.bRegistered ? 1 : 0,
        HelperEvidence.bConfiguredForBodyAndOutfit ? 1 : 0,
        HelperEvidence.bAssetless ? 1 : 0,
        HelperEvidence.bVisible ? 1 : 0,
        HelperEvidence.bHiddenInGame ? 1 : 0,
        HelperEvidence.bShouldRender ? 1 : 0,
        HelperEvidence.bRenderInMainPass ? 1 : 0,
        HelperEvidence.bVisibleInSceneCaptureOnly ? 1 : 0,
        HelperEvidence.bCanEverTick ? 1 : 0,
        HelperEvidence.bTickEnabled ? 1 : 0,
        HelperEvidence.bCollisionDisabled ? 1 : 0,
        HelperEvidence.bGenerateOverlapEvents ? 1 : 0,
        HelperEvidence.bRegisteredAsBodyFollower ? 1 : 0);
    OutEvidence.ExactStateFingerprint = Stable + FString::Printf(
        TEXT("|C:%d:%d:%d:%d"),
        OutEvidence.Counters.TransitionSuccess,
        OutEvidence.Counters.TransitionFailure,
        OutEvidence.Counters.RollbackSuccess,
        OutEvidence.Counters.RollbackFailure);
    OutEvidence.bCollected = true;
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    AppendPresentationPolicyProbe(
        const TCHAR* Name,
        const TCHAR* Classification,
        const TCHAR* Invocation,
        bool bCallResult,
        bool bExpectedCallResult,
        const FPresentationPolicyCounters& ExpectedDelta,
        bool bExpectStableStatePreserved,
        const FPresentationPolicyEvidence& Before,
        const FPresentationPolicyEvidence& After,
        TArray<FPresentationPolicyProbeRecord>& OutRecords,
        FString& OutError) const
{
    FPresentationPolicyProbeRecord Record;
    Record.Name = Name;
    Record.Classification = Classification;
    Record.Invocation = Invocation;
    Record.bCallResult = bCallResult;
    Record.bExpectedCallResult = bExpectedCallResult;
    Record.Before = Before;
    Record.After = After;
    Record.bActorIdentityPreserved =
        Before.ActiveVisualActor == After.ActiveVisualActor;
    Record.bHelperIdentityPreserved =
        Before.OutfitRequiredBonesHelper.ComponentPath
            == After.OutfitRequiredBonesHelper.ComponentPath;
    bool bComponentIdentityPreserved =
        Before.AnimationSourceComponent == After.AnimationSourceComponent
        && Before.LODSync.ComponentPath == After.LODSync.ComponentPath
        && Before.SkeletalComponents.Num()
            == After.SkeletalComponents.Num()
        && Before.GroomComponents.Num() == After.GroomComponents.Num();
    for (int32 Index = 0;
         bComponentIdentityPreserved
            && Index < Before.SkeletalComponents.Num();
         ++Index)
    {
        bComponentIdentityPreserved =
            Before.SkeletalComponents[Index].Name
                == After.SkeletalComponents[Index].Name
            && Before.SkeletalComponents[Index].ComponentPath
                == After.SkeletalComponents[Index].ComponentPath
            && Before.SkeletalComponents[Index].SkeletalMeshPath
                == After.SkeletalComponents[Index].SkeletalMeshPath;
    }
    for (int32 Index = 0;
         bComponentIdentityPreserved
            && Index < Before.GroomComponents.Num();
         ++Index)
    {
        bComponentIdentityPreserved =
            Before.GroomComponents[Index].Name
                == After.GroomComponents[Index].Name
            && Before.GroomComponents[Index].ComponentPath
                == After.GroomComponents[Index].ComponentPath
            && Before.GroomComponents[Index].GroomAssetPath
                == After.GroomComponents[Index].GroomAssetPath;
    }
    Record.bStablePolicyStatePreserved =
        Before.ExactStateFingerprint == After.ExactStateFingerprint;
    Record.CounterDelta.TransitionSuccess =
        After.Counters.TransitionSuccess - Before.Counters.TransitionSuccess;
    Record.CounterDelta.TransitionFailure =
        After.Counters.TransitionFailure - Before.Counters.TransitionFailure;
    Record.CounterDelta.RollbackSuccess =
        After.Counters.RollbackSuccess - Before.Counters.RollbackSuccess;
    Record.CounterDelta.RollbackFailure =
        After.Counters.RollbackFailure - Before.Counters.RollbackFailure;
    OutRecords.Add(Record);

    if (!Before.bCollected || !After.bCollected
        || bCallResult != bExpectedCallResult
        || !Record.bActorIdentityPreserved
        || !Record.bHelperIdentityPreserved
        || !bComponentIdentityPreserved
        || Record.bStablePolicyStatePreserved
            != bExpectStableStatePreserved
        || Record.CounterDelta.TransitionSuccess
            != ExpectedDelta.TransitionSuccess
        || Record.CounterDelta.TransitionFailure
            != ExpectedDelta.TransitionFailure
        || Record.CounterDelta.RollbackSuccess
            != ExpectedDelta.RollbackSuccess
        || Record.CounterDelta.RollbackFailure
            != ExpectedDelta.RollbackFailure)
    {
        OutError = FString::Printf(
            TEXT("presentation-policy probe %s failed: result=%d expected_result=%d actor_same=%d helper_same=%d stable_same=%d expected_stable_same=%d delta=%d/%d/%d/%d expected_delta=%d/%d/%d/%d"),
            Name,
            bCallResult ? 1 : 0,
            bExpectedCallResult ? 1 : 0,
            Record.bActorIdentityPreserved ? 1 : 0,
            Record.bHelperIdentityPreserved ? 1 : 0,
            Record.bStablePolicyStatePreserved ? 1 : 0,
            bExpectStableStatePreserved ? 1 : 0,
            Record.CounterDelta.TransitionSuccess,
            Record.CounterDelta.TransitionFailure,
            Record.CounterDelta.RollbackSuccess,
            Record.CounterDelta.RollbackFailure,
            ExpectedDelta.TransitionSuccess,
            ExpectedDelta.TransitionFailure,
            ExpectedDelta.RollbackSuccess,
            ExpectedDelta.RollbackFailure);
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    RunRedundantPresentationPolicyProbe(
        const TCHAR* Name,
        EDGMetaHumanPresentationPolicy Policy,
        FString& OutError)
{
    FPresentationPolicyEvidence Before;
    FPresentationPolicyEvidence After;
    if (!PopulatePresentationPolicyEvidence(Policy, Before, OutError))
    {
        return false;
    }
    const bool bResult = AvatarBackend->SetPresentationPolicy(Policy);
    if (!PopulatePresentationPolicyEvidence(Policy, After, OutError))
    {
        return false;
    }
    const FPresentationPolicyCounters ZeroDelta;
    return AppendPresentationPolicyProbe(
        Name,
        TEXT("VERIFIED_SAME_MODE_NO_OP"),
        TEXT("SetPresentationPolicy"),
        bResult,
        true,
        ZeroDelta,
        true,
        Before,
        After,
        PresentationPolicyTransitionProbes,
        OutError);
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    RunGameplayCandidateIsolationProbe(FString& OutError)
{
    if (!AvatarBackend || !Golfer || !CanonicalMetaHumanProfile
        || (PlayerController && PlayerController->IsCharacterCreatorOpen()))
    {
        OutError = TEXT(
            "pre-creator candidate isolation requires a closed creator and canonical active MetaHuman");
        return false;
    }
    if (!VerifyMetaHumanPresentation(OutError))
    {
        return false;
    }

    FPresentationPolicyEvidence PolicyBefore;
    if (!PopulatePresentationPolicyEvidence(
            EDGMetaHumanPresentationPolicy::GameplayPerformance,
            PolicyBefore,
            OutError))
    {
        return false;
    }
    const FDGFullCharacterCustomization CustomizationBefore =
        Golfer->GetCurrentFullCharacterCustomization();
    AActor* const ActiveActorBefore = AvatarBackend->GetActiveVisualActor();
    USkeletalMeshComponent* const ActiveBodyBefore =
        AvatarBackend->GetVerifiedVisualBody();
    USkeletalMeshComponent* const ActiveHeadBefore =
        AvatarBackend->GetVerifiedVisualHead();
    UClass* const VisualClass = IsValid(ActiveActorBefore)
        ? ActiveActorBefore->GetClass() : nullptr;
    const int32 VisualCountBefore = CountActiveVisualActors(VisualClass);
    TArray<uint8> SaveBefore;
    if (!LoadSaveBytes(SaveBefore))
    {
        OutError = TEXT(
            "pre-creator candidate isolation could not read the isolated save checkpoint");
        return false;
    }

    TransientFailureProfile = DuplicateObject<UDiscGolfAvatarBackendProfile>(
        CanonicalMetaHumanProfile, this);
    if (!TransientFailureProfile)
    {
        OutError = TEXT(
            "pre-creator candidate isolation could not allocate its invalid profile");
        return false;
    }
    TransientFailureProfile->VisualHeadComponentTag =
        TransientFailureProfile->VisualBodyComponentTag;
    AvatarBackend->BackendProfile = TransientFailureProfile;
    const bool bUnexpectedCandidateSuccess =
        AvatarBackend->BuildVisualBackend(
            Golfer->GetSkeletalGolferMesh(), CustomizationBefore);
    BackendFailureAdapterStatus = AvatarBackend->GetLastAdapterStatus();
    AvatarBackend->BackendProfile = CanonicalMetaHumanProfile;

    FPresentationPolicyEvidence PolicyAfter;
    if (!PopulatePresentationPolicyEvidence(
            EDGMetaHumanPresentationPolicy::GameplayPerformance,
            PolicyAfter,
            OutError))
    {
        return false;
    }
    const FPresentationPolicyCounters ZeroDelta;
    if (!AppendPresentationPolicyProbe(
            TEXT("phase2_gameplay_invalid_candidate"),
            TEXT("PREVALIDATION_CANDIDATE_REJECTION"),
            TEXT("BuildVisualBackend_duplicate_body_head_tag"),
            bUnexpectedCandidateSuccess,
            false,
            ZeroDelta,
            true,
            PolicyBefore,
            PolicyAfter,
            PresentationPolicyCandidateIsolationProbes,
            OutError))
    {
        return false;
    }

    TArray<uint8> SaveAfter;
    if (BackendFailureAdapterStatus
            != TEXT("Distinct visual body/head component tags are required.")
        || !LoadSaveBytes(SaveAfter) || SaveAfter != SaveBefore
        || AvatarBackend->GetActiveVisualActor() != ActiveActorBefore
        || AvatarBackend->GetVerifiedVisualBody() != ActiveBodyBefore
        || AvatarBackend->GetVerifiedVisualHead() != ActiveHeadBefore
        || CountActiveVisualActors(VisualClass) != VisualCountBefore
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            CustomizationBefore,
            Golfer->GetCurrentFullCharacterCustomization())
        || !VerifyMetaHumanPresentation(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT(
                "prevalidation candidate rejection changed gameplay tier2 actor/helper/policy/LOD/tick/URO/counters/customization/save state");
        }
        return false;
    }
    bGameplayCandidateIsolationProbePassed = true;
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    FreezeStableThrowPresentation(FString& OutError)
{
    if (!VerifyMetaHumanPresentation(OutError))
    {
        return false;
    }
    StableThrowVisualActor = AvatarBackend->GetActiveVisualActor();
    StableThrowVisualBody = AvatarBackend->GetVerifiedVisualBody();
    StableThrowVisualHead = AvatarBackend->GetVerifiedVisualHead();
    if (!IsValid(StableThrowVisualActor)
        || !IsValid(StableThrowVisualBody)
        || !IsValid(StableThrowVisualHead))
    {
        OutError = TEXT(
            "the verified MetaHuman actor/body/head tuple could not be frozen before the throw");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    VerifyStableThrowPresentation(FString& OutError) const
{
    if (!VerifyMetaHumanPresentation(OutError))
    {
        return false;
    }
    if (!IsValid(StableThrowVisualActor)
        || !IsValid(StableThrowVisualBody)
        || !IsValid(StableThrowVisualHead)
        || AvatarBackend->GetActiveVisualActor()
            != StableThrowVisualActor
        || AvatarBackend->GetVerifiedVisualBody()
            != StableThrowVisualBody
        || AvatarBackend->GetVerifiedVisualHead()
            != StableThrowVisualHead)
    {
        OutError = TEXT(
            "the MetaHuman actor/body/head presentation tuple changed during the authoritative RHBH lifecycle");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::LoadSaveBytes(
    TArray<uint8>& OutBytes) const
{
    OutBytes.Reset();
    return IFileManager::Get().FileExists(*SavePath)
        && FFileHelper::LoadFileToArray(OutBytes, *SavePath);
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    VerifyOnlyIsolatedProfileSave(FString& OutError) const
{
    TArray<FString> SaveFiles;
    IFileManager::Get().FindFilesRecursive(
        SaveFiles,
        *UserDirectory,
        TEXT("*.sav"),
        true,
        false,
        false);
    if (SaveFiles.Num() != 1
        || !FPaths::IsSamePath(SaveFiles[0], SavePath))
    {
        OutError = FString::Printf(
            TEXT("external UserDir contained %d save files instead of exactly the isolated default profile"),
            SaveFiles.Num());
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::OpenCreator(
    FString& OutError)
{
    if (!GameMode || !PlayerController || !Golfer
        || !GameMode->CanOpenCharacterCreator()
        || !PlayerController->OpenCharacterCreator()
        || !PlayerController->IsCharacterCreatorOpen()
        || !UGameplayStatics::IsGamePaused(this))
    {
        OutError = TEXT("the real character creator could not open and pause a safe gameplay state");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::SelectBackend(
    FName BackendId,
    FString& OutError)
{
    FDGFullCharacterCustomization Draft;
    if (!PlayerController || !PlayerController->SelectCharacterCreatorBackend(
            BackendId, Draft))
    {
        OutError = PlayerController
            ? PlayerController->GetCharacterCreatorStatusText()
            : TEXT("character creator disappeared");
        return false;
    }
    if (Draft.AvatarBackendId != BackendId
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Draft,
            PlayerController->GetCharacterCreatorDraftCustomization()))
    {
        OutError = TEXT("backend selection did not become the real complete creator draft");
        return false;
    }
    return BackendId == FName(
            DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId)
        ? VerifyMetaHumanPresentation(OutError)
        : VerifyDGPresentation(OutError);
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::ApplyCreatorDraft(
    FString& OutError)
{
    if (!PlayerController || !PlayerController->IsCharacterCreatorOpen())
    {
        OutError = TEXT("creator was not open for Apply");
        return false;
    }
    const FDGFullCharacterCustomization Draft =
        PlayerController->GetCharacterCreatorDraftCustomization();
    if (!PlayerController->ApplyFullCharacterCreatorDraft(Draft)
        || PlayerController->IsCharacterCreatorOpen()
        || UGameplayStatics::IsGamePaused(this))
    {
        OutError = TEXT("real complete-character Apply did not save and close the creator");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    RunFailureAtomicityProbes(FString& OutError)
{
    if (!PlayerController || !PlayerController->IsCharacterCreatorOpen()
        || !AvatarBackend || !Golfer)
    {
        OutError = TEXT("failure probes require the open creator and verified MetaHuman adapter");
        return false;
    }
    if (!VerifyMetaHumanPresentation(OutError))
    {
        return false;
    }

    const FDGFullCharacterCustomization Before =
        Golfer->GetCurrentFullCharacterCustomization();
    const FDGFullCharacterCustomization DraftBefore =
        PlayerController->GetCharacterCreatorDraftCustomization();
    AActor* const ActiveActorBefore =
        AvatarBackend->GetActiveVisualActor();
    USkeletalMeshComponent* const ActiveBodyBefore =
        AvatarBackend->GetVerifiedVisualBody();
    USkeletalMeshComponent* const ActiveHeadBefore =
        AvatarBackend->GetVerifiedVisualHead();
    UClass* const VisualClass = ActiveActorBefore
        ? ActiveActorBefore->GetClass() : nullptr;
    const int32 VisualCountBefore = CountActiveVisualActors(VisualClass);
    TArray<uint8> SaveBefore;
    if (!LoadSaveBytes(SaveBefore))
    {
        OutError = TEXT("failure probes could not read the isolated profile checkpoint");
        return false;
    }

    FDGFullCharacterCustomization RejectedDraft;
    const bool bUnexpectedCosmeticSuccess =
        PlayerController->SelectCharacterCreatorCosmetic(
            EDGCosmeticKind::Hair,
            TEXT("session8b_unavailable_cosmetic"),
            RejectedDraft);
    TArray<uint8> SaveAfterCosmetic;
    if (bUnexpectedCosmeticSuccess
        || !LoadSaveBytes(SaveAfterCosmetic)
        || SaveAfterCosmetic != SaveBefore
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Before, Golfer->GetCurrentFullCharacterCustomization())
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            DraftBefore,
            PlayerController->GetCharacterCreatorDraftCustomization())
        || AvatarBackend->GetActiveVisualActor() != ActiveActorBefore
        || !VerifyMetaHumanPresentation(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT(
                "unavailable cosmetic was not rejected before visual/save mutation");
        }
        return false;
    }
    bCosmeticFailureProbePassed = true;

    TransientFailureProfile = DuplicateObject<UDiscGolfAvatarBackendProfile>(
        CanonicalMetaHumanProfile, this);
    if (!TransientFailureProfile)
    {
        OutError = TEXT("could not create the transient backend failure candidate");
        return false;
    }
    TransientFailureProfile->VisualHeadComponentTag =
        TransientFailureProfile->VisualBodyComponentTag;
    AvatarBackend->BackendProfile = TransientFailureProfile;
    const bool bUnexpectedCandidateSuccess =
        AvatarBackend->BuildVisualBackend(
            Golfer->GetSkeletalGolferMesh(), Before);
    BackendFailureAdapterStatus = AvatarBackend->GetLastAdapterStatus();
    AvatarBackend->BackendProfile = CanonicalMetaHumanProfile;

    TArray<uint8> SaveAfterCandidate;
    if (bUnexpectedCandidateSuccess
        || BackendFailureAdapterStatus
            != TEXT("Distinct visual body/head component tags are required.")
        || !LoadSaveBytes(SaveAfterCandidate)
        || SaveAfterCandidate != SaveBefore
        || AvatarBackend->GetActiveVisualActor() != ActiveActorBefore
        || AvatarBackend->GetVerifiedVisualBody() != ActiveBodyBefore
        || AvatarBackend->GetVerifiedVisualHead() != ActiveHeadBefore
        || CountActiveVisualActors(VisualClass) != VisualCountBefore
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            Before, Golfer->GetCurrentFullCharacterCustomization())
        || !DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
            DraftBefore,
            PlayerController->GetCharacterCreatorDraftCustomization())
        || !VerifyMetaHumanPresentation(OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT(
                "after-spawn invalid backend candidate did not reach the expected adapter rejection and retain the active verified visual atomically");
        }
        return false;
    }
    bBackendFailureProbePassed = true;
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::BeginRealRHBHTiming(
    FString& OutError)
{
    UThrowControllerComponent* Controller = Golfer
        ? Golfer->GetThrowController() : nullptr;
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    if (!GameMode || !Controller || !Bag || !ThrowAdapter
        || UGameplayStatics::IsGamePaused(this)
        || !GameMode->CanPlayerThrow() || ThrowAdapter->IsThrowActive()
        || Bag->GetSelectedMoldId().IsNone())
    {
        OutError = TEXT("a legal unpaused gameplay state was unavailable for the first real throw press");
        return false;
    }
    ReleasedGameplayDisc = nullptr;
    ImmutableReleaseGripWorldLocation = FVector::ZeroVector;
    bHasImmutableReleaseGripWorldLocation = false;
    ReleaseCallbackFrameCounter = 0;
    CaptureFreezeFrameCounter = 0;
    PresentationEventSerial = 0;
    ReleaseCallbackEventOrder = 0;
    CaptureFreezeEventOrder = 0;
    bReleasePresentationCapturePending = false;
    FrozenHandCorrectionEvidence =
        FDiscGolfMetaHumanHandCorrectionEvidence();
    FrozenHandCorrectionTargetBoneRevision = 0;
    bHasFrozenHandCorrectionEvidence = false;
    Controller->CancelTiming();
    Controller->SetShotContext(
        EDiscShotContext::Drive, GameMode->GetBasketDistanceMeters());
    if (Controller->GetThrowStyle() != EThrowStyle::Backhand)
    {
        Controller->ToggleThrowStyle();
    }
    FThrowCommand UnexpectedCommand;
    if (Controller->HandleThrowPress(
            Bag->GetSelectedMoldId(),
            Bag->GetSelectedPlastic(),
            Golfer->GetActorForwardVector(),
            UnexpectedCommand)
        || !Controller->IsTimingActive())
    {
        OutError = TEXT("the first real throw press did not enter the accepted timing state");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::CommitRealRHBHThrow(
    FString& OutError)
{
    UThrowControllerComponent* Controller = Golfer
        ? Golfer->GetThrowController() : nullptr;
    UDiscBagComponent* Bag = Golfer ? Golfer->GetDiscBag() : nullptr;
    if (!Controller || !Bag
        || !Controller->HandleThrowPress(
            Bag->GetSelectedMoldId(),
            Bag->GetSelectedPlastic(),
            Golfer->GetActorForwardVector(),
            LiveThrowCommand))
    {
        OutError = TEXT(
            "the second real throw press did not produce an authoritative RHBH command");
        return false;
    }
    if (!BindSession8BSelectedThrowProvenance(
            Golfer, GameMode, LiveThrowCommand)
        || LiveThrowCommand.ThrowStyle != EThrowStyle::Backhand
        || LiveThrowCommand.ShotContext != EDiscShotContext::Drive
        || LiveThrowCommand.Direction.IsNearlyZero()
        || !Golfer->TryStartAnimatedRHBHThrow(LiveThrowCommand)
        || !ThrowAdapter->IsThrowActive()
        || !ThrowAdapter->IsAwaitingRelease())
    {
        OutError = TEXT(
            "the second real throw press did not create one authoritative animated RHBH transaction");
        return false;
    }
    LiveAttemptSerial = ThrowAdapter->GetAttemptSerial();
    if (LiveAttemptSerial <= 0
        || ThrowAdapter->GetReleaseCommitCountForAttempt() != 0
        || ThrowAdapter->GetTotalReleaseCommitCount()
            != BaselineReleaseCommits
        || GameMode->GetStrokes() != BaselineStrokes
        || CountWorldDiscs() != BaselineWorldDiscs)
    {
        OutError = TEXT("throw authority mutated before the framework release notify");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::PauseForThrowCapture(
    FString& OutError)
{
    const bool bDeferredReleaseFreeze =
        Phase == EPhase::ReloadDGRestoreMetaHumanVisualThrow
        && Step == EStep::Phase3AwaitRelease
        && bReleasePresentationCapturePending
        && bHasThrowCaptureFlightTickSnapshot
        && IsValid(ThrowCaptureFlightComponent);
    if (bPausedForThrowCapture || UGameplayStatics::IsGamePaused(this)
        || PausedSkeletalMeshStates.Num() != 0
        || (!bDeferredReleaseFreeze
            && (bHasThrowCaptureFlightTickSnapshot
                || IsValid(ThrowCaptureFlightComponent))))
    {
        OutError = TEXT("throw evidence frame could not freeze the rendered world exactly once");
        return false;
    }
    if (bDeferredReleaseFreeze
        && (ReleaseCallbackFrameCounter == 0
            || ReleaseCallbackFrameCounter != GFrameCounter))
    {
        OutError = TEXT(
            "release presentation freeze did not run in the accepted callback frame");
        return false;
    }

    const bool bRequiresFrozenHandCorrectionEvidence =
        Phase == EPhase::ReloadDGRestoreMetaHumanVisualThrow
        && (Step == EStep::Phase3CaptureGrip
            || (Step == EStep::Phase3AwaitRelease
                && bDeferredReleaseFreeze));
    if (bRequiresFrozenHandCorrectionEvidence)
    {
        if (IsValid(StableThrowVisualBody))
        {
            StableThrowVisualBody->HandleExistingParallelEvaluationTask(
                true,
                true);
        }
        const UDiscGolfMetaHumanRetargetAnimInstance* const RetargetInstance =
            IsValid(StableThrowVisualBody)
            ? Cast<UDiscGolfMetaHumanRetargetAnimInstance>(
                StableThrowVisualBody->GetAnimInstance())
            : nullptr;
        FDiscGolfMetaHumanHandCorrectionEvidence Correction;
        const uint32 TargetRevision = IsValid(StableThrowVisualBody)
            ? StableThrowVisualBody->GetBoneTransformRevisionNumber() : 0;
        if (!IsValid(RetargetInstance)
            || !RetargetInstance->GetPresentationHandCorrectionEvidence(
                Correction)
            || !Correction.bSnapshotValid
            || !Correction.bReachable
            || !Correction.bApplied
            || StableThrowVisualBody->IsRunningParallelEvaluation()
            || Correction.SourceSampleFrameCounter == 0
            || Correction.SourceSampleFrameCounter != GFrameCounter
            || Correction.TargetCorrectionFrameCounter != GFrameCounter
            || TargetRevision == Correction.TargetBoneRevisionBeforeEvaluate)
        {
            OutError = FString::Printf(
                TEXT("throw evidence frame could not freeze one same-frame deterministic hand correction: source_frame=%llu target_frame=%llu freeze_frame=%llu snapshot=%d reachable=%d applied=%d target_before=%u target_freeze=%u"),
                Correction.SourceSampleFrameCounter,
                Correction.TargetCorrectionFrameCounter,
                GFrameCounter,
                Correction.bSnapshotValid ? 1 : 0,
                Correction.bReachable ? 1 : 0,
                Correction.bApplied ? 1 : 0,
                Correction.TargetBoneRevisionBeforeEvaluate,
                TargetRevision);
            return false;
        }
        FrozenHandCorrectionEvidence = Correction;
        FrozenHandCorrectionTargetBoneRevision = TargetRevision;
        bHasFrozenHandCorrectionEvidence = true;
    }

    auto AddMeshToPause = [this](USkeletalMeshComponent* Mesh)
    {
        if (!IsValid(Mesh)
            || PausedSkeletalMeshStates.ContainsByPredicate(
                [Mesh](const FPausedSkeletalMeshState& State)
                {
                    return State.Mesh.Get() == Mesh;
                }))
        {
            return;
        }
        FPausedSkeletalMeshState& State =
            PausedSkeletalMeshStates.AddDefaulted_GetRef();
        State.Mesh = Mesh;
        State.bPreviousPauseAnims = Mesh->bPauseAnims;
        Mesh->bPauseAnims = true;
    };

    AddMeshToPause(Golfer ? Golfer->GetSkeletalGolferMesh() : nullptr);
    if (IsValid(StableThrowVisualActor))
    {
        TInlineComponentArray<USkeletalMeshComponent*> VisualMeshes;
        StableThrowVisualActor->GetComponents(VisualMeshes);
        for (USkeletalMeshComponent* Mesh : VisualMeshes)
        {
            AddMeshToPause(Mesh);
        }
    }
    if (PausedSkeletalMeshStates.Num() == 0)
    {
        ResumeAfterThrowCapture();
        OutError = TEXT(
            "throw evidence frame could not identify a presentation pose to freeze");
        return false;
    }

    UDiscFlightComponent* const Flight = IsValid(ReleasedGameplayDisc)
        ? ReleasedGameplayDisc->GetFlightComponent() : nullptr;
    const bool bExactPreReleaseGripFrame =
        Phase == EPhase::ReloadDGRestoreMetaHumanVisualThrow
        && Step == EStep::Phase3CaptureGrip
        && FrameworkThrow
        && FrameworkThrow->CurrentPhase == EDGThrowPhase::ReachBack
        && GameMode
        && !GameMode->GetActiveDisc();
    if (!IsValid(Flight) && !bExactPreReleaseGripFrame)
    {
        ResumeAfterThrowCapture();
        OutError = TEXT(
            "throw evidence frame could not identify the authoritative active flight tick");
        return false;
    }
    if (IsValid(Flight))
    {
        if (!Flight->IsFlying()
            || (bDeferredReleaseFreeze
                && (Flight != ThrowCaptureFlightComponent
                    || !bHasThrowCaptureFlightTickSnapshot
                    || !bThrowCaptureFlightTickWasEnabled
                    || Flight->IsComponentTickEnabled())))
        {
            ResumeAfterThrowCapture();
            OutError = TEXT(
                "throw evidence frame did not retain its exact suspended authoritative flight");
            return false;
        }
        if (!bDeferredReleaseFreeze)
        {
            ThrowCaptureFlightComponent = Flight;
            bThrowCaptureFlightTickWasEnabled =
                Flight->IsComponentTickEnabled();
            bHasThrowCaptureFlightTickSnapshot = true;
            if (!bThrowCaptureFlightTickWasEnabled)
            {
                ResumeAfterThrowCapture();
                OutError = TEXT(
                    "authoritative flight tick was not enabled before the evidence freeze");
                return false;
            }
            Flight->SetComponentTickEnabled(false);
            if (Flight->IsComponentTickEnabled())
            {
                ResumeAfterThrowCapture();
                OutError = TEXT(
                    "throw evidence frame could not suspend the authoritative flight tick");
                return false;
            }
        }
    }
    bPausedForThrowCapture = true;
    if (!UGameplayStatics::SetGamePaused(this, true)
        || !UGameplayStatics::IsGamePaused(this))
    {
        ResumeAfterThrowCapture();
        OutError = TEXT("throw evidence frame could not freeze the rendered world and presentation pose");
        return false;
    }
    CaptureFreezeFrameCounter = GFrameCounter;
    CaptureFreezeEventOrder = ++PresentationEventSerial;
    return true;
}

void ADiscGolfSession8BMetaHumanPackagedRunner::ResumeAfterThrowCapture()
{
    if (bHasThrowCaptureFlightTickSnapshot
        && IsValid(ThrowCaptureFlightComponent))
    {
        ThrowCaptureFlightComponent->SetComponentTickEnabled(
            bThrowCaptureFlightTickWasEnabled);
    }
    ThrowCaptureFlightComponent = nullptr;
    bHasThrowCaptureFlightTickSnapshot = false;
    bThrowCaptureFlightTickWasEnabled = false;
    bReleasePresentationCapturePending = false;
    FrozenHandCorrectionEvidence =
        FDiscGolfMetaHumanHandCorrectionEvidence();
    FrozenHandCorrectionTargetBoneRevision = 0;
    bHasFrozenHandCorrectionEvidence = false;
    for (const FPausedSkeletalMeshState& State : PausedSkeletalMeshStates)
    {
        if (USkeletalMeshComponent* Mesh = State.Mesh.Get())
        {
            Mesh->bPauseAnims = State.bPreviousPauseAnims;
        }
    }
    PausedSkeletalMeshStates.Reset();
    if (bPausedForThrowCapture)
    {
        UGameplayStatics::SetGamePaused(this, false);
    }
    bPausedForThrowCapture = false;
}

void ADiscGolfSession8BMetaHumanPackagedRunner::HandleReleaseCommitted(
    int64 AttemptSerial,
    bool bAccepted)
{
    const bool bPhase3 =
        Phase == EPhase::ReloadDGRestoreMetaHumanVisualThrow;
    const bool bPhase4 = Phase == EPhase::MetaHumanPerformance;
    if (bFinished || (!bPhase3 && !bPhase4))
    {
        return;
    }
    ++ReleaseCallbackCount;
    ReleasedGameplayDisc = GameMode ? GameMode->GetActiveDisc() : nullptr;
    FString Error;
    if (ReleaseCallbackCount != 1 || AttemptSerial != LiveAttemptSerial
        || (bPhase3 && Step != EStep::Phase3AwaitRelease)
        || (bPhase4 && Step != EStep::Phase4SampleGameplay)
        || !bAccepted || !ThrowAdapter->HasCommittedRelease()
        || ThrowAdapter->GetReleaseCommitCountForAttempt() != 1
        || ThrowAdapter->GetTotalReleaseCommitCount()
            != BaselineReleaseCommits + 1
        || !ReleasedGameplayDisc
        || !ReleasedGameplayDisc->GetFlightComponent()
        || GameMode->GetStrokes() != BaselineStrokes + 1
        || CountWorldDiscs() != BaselineWorldDiscs + 1
        || (Golfer->GetHeldDiscVisual()
            && Golfer->GetHeldDiscVisual()->IsVisible()))
    {
        Fail(TEXT("RELEASE_AUTHORITY"), Error.IsEmpty()
            ? TEXT("release callback did not expose exactly one accepted gameplay launch")
            : Error);
        return;
    }
    USkeletalMeshComponent* const AnimationSource =
        Golfer ? Golfer->GetSkeletalGolferMesh() : nullptr;
    const FName ReleaseGripBone = FrameworkThrow
        ? FrameworkThrow->GetActiveDiscGripBone() : NAME_None;
    const FVector ReleasedDiscLocation =
        ReleasedGameplayDisc->GetActorLocation();
    if (!IsValid(AnimationSource) || ReleaseGripBone.IsNone()
        || !AnimationSource->DoesSocketExist(ReleaseGripBone))
    {
        Fail(TEXT("RELEASE_AUTHORITY"),
            TEXT("accepted release did not expose its authoritative DG grip socket"));
        return;
    }
    ImmutableReleaseGripWorldLocation =
        AnimationSource->GetSocketLocation(ReleaseGripBone);
    bHasImmutableReleaseGripWorldLocation =
        !ImmutableReleaseGripWorldLocation.ContainsNaN()
        && !ReleasedDiscLocation.ContainsNaN()
        && FVector::Distance(
            ReleasedDiscLocation,
            ImmutableReleaseGripWorldLocation) <= 25.0f;
    if (!bHasImmutableReleaseGripWorldLocation)
    {
        Fail(TEXT("RELEASE_AUTHORITY"),
            TEXT("accepted gameplay disc was not spawned at the synchronously sampled DG grip"));
        return;
    }
    if (bPhase3)
    {
        UDiscFlightComponent* const Flight =
            ReleasedGameplayDisc->GetFlightComponent();
        if (!IsValid(Flight) || !Flight->IsFlying()
            || !Flight->IsComponentTickEnabled()
            || bHasThrowCaptureFlightTickSnapshot
            || IsValid(ThrowCaptureFlightComponent)
            || bReleasePresentationCapturePending)
        {
            Fail(TEXT("RELEASE_AUTHORITY"),
                TEXT("release callback did not expose one enabled authoritative flight tick to snapshot"));
            return;
        }
        ThrowCaptureFlightComponent = Flight;
        bThrowCaptureFlightTickWasEnabled = true;
        bHasThrowCaptureFlightTickSnapshot = true;
        Flight->SetComponentTickEnabled(false);
        if (Flight->IsComponentTickEnabled())
        {
            Fail(TEXT("RELEASE_AUTHORITY"),
                TEXT("release callback could not immediately suspend authoritative flight"));
            return;
        }
        ReleaseCallbackFrameCounter = GFrameCounter;
        ReleaseCallbackEventOrder = ++PresentationEventSerial;
        bReleasePresentationCapturePending = true;
    }
}

void ADiscGolfSession8BMetaHumanPackagedRunner::HandleThrowRecovered(
    int64 AttemptSerial,
    bool bDiscWasReleased)
{
    const bool bEligiblePhase =
        Phase == EPhase::ReloadDGRestoreMetaHumanVisualThrow
        || Phase == EPhase::MetaHumanPerformance;
    if (bFinished || !bEligiblePhase)
    {
        return;
    }
    if (AttemptSerial != LiveAttemptSerial || !bDiscWasReleased)
    {
        Fail(TEXT("THROW_RECOVERY"),
            TEXT("live RHBH recovery did not match the sole released attempt"));
        return;
    }
    ++RecoveryCallbackCount;
    if (RecoveryCallbackCount > 1)
    {
        Fail(TEXT("THROW_RECOVERY"),
            TEXT("live RHBH recovery callback fired more than once"));
    }
}

void ADiscGolfSession8BMetaHumanPackagedRunner::RunStep()
{
    const double Now = FPlatformTime::Seconds();
    FString Error;
    if (Now - StepStartedSeconds > StepTimeoutSeconds
        && Step != EStep::Phase4SampleCreator
        && Step != EStep::Phase4SampleGameplay)
    {
        Fail(TEXT("STEP_TIMEOUT"),
            FString::Printf(TEXT("phase step %d timed out"),
                static_cast<int32>(Step)));
        return;
    }

    switch (Step)
    {
        case EStep::Phase1CaptureDG:
            if (!RequestCapture(
                    TEXT("01_DG_Creator_Sentinel_Baseline.png"),
                    TEXT("dg_creator_proxy_sentinel_baseline"),
                    EStep::Phase1SelectMetaHuman,
                    Error))
            {
                Fail(TEXT("CAPTURE_REQUEST"), Error);
            }
            return;

        case EStep::Phase1SelectMetaHuman:
        {
            if (!SelectBackend(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    Error))
            {
                Fail(TEXT("DG_TO_METAHUMAN"), Error);
                return;
            }
            FDGFullCharacterCustomization Expected =
                ProxySentinelCustomization;
            Expected.AvatarBackendId =
                DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId;
            DiscGolfFullCharacterRuntime::NormalizeForPersistence(Expected);
            const FDGFullCharacterCustomization Actual =
                Golfer->GetCurrentFullCharacterCustomization();
            bProxyValuesPreserved =
                DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
                    Expected, Actual)
                && Actual.Hair.FacialHairId
                    == FName(TEXT("facialhair_beard"));
            if (!bProxyValuesPreserved)
            {
                Fail(TEXT("PROXY_VALUE_PRESERVATION"),
                    TEXT("MetaHuman selection rewrote preserved DG proxy values"));
                return;
            }
            if (!Golfer->SetCharacterCreatorPreviewFraming(
                    680.0f, -18.0f, 0.0f, 0.0f))
            {
                Fail(TEXT("CREATOR_FRAMING"),
                    TEXT("could not establish the full-body MetaHuman creator framing"));
                return;
            }
            SetStep(EStep::Phase1CaptureMetaHumanFullBody);
            return;
        }

        case EStep::Phase1CaptureMetaHumanFullBody:
            if (!RequestCapture(
                    TEXT("02_MH_Creator_Curated_FullBody.png"),
                    TEXT("metahuman_creator_curated_fixed_preset_full_body"),
                    EStep::Phase1CaptureMetaHumanCloseup,
                    Error))
            {
                Fail(TEXT("CAPTURE_REQUEST"), Error);
            }
            return;

        case EStep::Phase1CaptureMetaHumanCloseup:
            if (!Golfer->SetCharacterCreatorPreviewFraming(
                    380.0f, -4.0f, 0.0f, 70.0f))
            {
                Fail(TEXT("CREATOR_FRAMING"),
                    TEXT("could not establish the MetaHuman hair closeup framing"));
                return;
            }
            if (!RequestCapture(
                    TEXT("03_MH_Creator_CleanShaven_Hair_Closeup.png"),
                    TEXT("metahuman_creator_clean_shaven_hair_closeup"),
                    EStep::Phase1Apply,
                    Error))
            {
                Fail(TEXT("CAPTURE_REQUEST"), Error);
            }
            return;

        case EStep::Phase1Apply:
        {
            const FDGFullCharacterCustomization Expected =
                PlayerController->GetCharacterCreatorDraftCustomization();
            if (!ApplyCreatorDraft(Error)
                || !VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &Expected, Error)
                || !VerifyMetaHumanPresentation(Error)
                || !VerifyDiskProfile(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &Expected, Error)
                || !VerifyOnlyIsolatedProfileSave(Error))
            {
                Fail(TEXT("APPLY_PERSISTENCE"), Error);
                return;
            }
            TArray<uint8> Saved;
            bSaveWasMutatedByPhase = LoadSaveBytes(Saved)
                && !Saved.IsEmpty() && Saved != InitialSaveBytes;
            if (!bSaveWasMutatedByPhase)
            {
                Fail(TEXT("APPLY_PERSISTENCE"),
                    TEXT("phase 1 did not replace the startup state with exactly one isolated schema-10 MetaHuman profile"));
                return;
            }
            Pass();
            return;
        }

        case EStep::Phase2PreviewDG:
            if (!SelectBackend(
                    FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId),
                    Error)
                || !LoadSaveBytes(InitialSaveBytes)
                || InitialSaveBytes != CheckpointSaveBytes)
            {
                Fail(TEXT("METAHUMAN_TO_DG_PREVIEW"), Error.IsEmpty()
                    ? TEXT("DG preview mutated the saved MetaHuman profile") : Error);
                return;
            }
            SetStep(EStep::Phase2CaptureDGPreview);
            return;

        case EStep::Phase2CaptureDGPreview:
            if (!RequestCapture(
                    TEXT("04_DG_Creator_Preview_From_MH.png"),
                    TEXT("dg_creator_preview_from_saved_metahuman"),
                    EStep::Phase2Cancel,
                    Error))
            {
                Fail(TEXT("CAPTURE_REQUEST"), Error);
            }
            return;

        case EStep::Phase2Cancel:
            PlayerController->CancelCharacterCreator();
            if (PlayerController->IsCharacterCreatorOpen()
                || UGameplayStatics::IsGamePaused(this)
                || !VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &InitialCustomization, Error)
                || !VerifyMetaHumanPresentation(Error)
                || !LoadSaveBytes(InitialSaveBytes)
                || InitialSaveBytes != CheckpointSaveBytes)
            {
                Fail(TEXT("CANCEL_RESTORE"), Error.IsEmpty()
                    ? TEXT("Cancel did not restore the exact saved MetaHuman visual and bytes")
                    : Error);
                return;
            }
            SetStep(EStep::Phase2CaptureMetaHumanAfterCancel);
            return;

        case EStep::Phase2CaptureMetaHumanAfterCancel:
            if (!RequestCapture(
                    TEXT("05_MH_Gameplay_After_Cancel.png"),
                    TEXT("metahuman_gameplay_after_byte_exact_cancel"),
                    EStep::Phase2FailureProbes,
                    Error))
            {
                Fail(TEXT("CAPTURE_REQUEST"), Error);
            }
            return;

        case EStep::Phase2FailureProbes:
            if (!OpenCreator(Error)
                || !RunFailureAtomicityProbes(Error))
            {
                Fail(TEXT("FAILURE_ATOMICITY"), Error);
                return;
            }
            SetStep(EStep::Phase2ApplyDG);
            return;

        case EStep::Phase2ApplyDG:
        {
            if (!SelectBackend(
                    FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId),
                    Error))
            {
                Fail(TEXT("METAHUMAN_TO_DG_APPLY"), Error);
                return;
            }
            const FDGFullCharacterCustomization Expected =
                PlayerController->GetCharacterCreatorDraftCustomization();
            if (!ApplyCreatorDraft(Error)
                || !VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId),
                    &Expected, Error)
                || !VerifyDGPresentation(Error)
                || !VerifyDiskProfile(
                    FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId),
                    &Expected, Error)
                || !VerifyOnlyIsolatedProfileSave(Error))
            {
                Fail(TEXT("METAHUMAN_TO_DG_APPLY"), Error);
                return;
            }
            TArray<uint8> Saved;
            bSaveWasMutatedByPhase = LoadSaveBytes(Saved)
                && Saved != CheckpointSaveBytes;
            if (!bSaveWasMutatedByPhase
                || !bCosmeticFailureProbePassed
                || !bBackendFailureProbePassed
                || !bGameplayCandidateIsolationProbePassed
                || PresentationPolicyCandidateIsolationProbes.Num() != 1)
            {
                Fail(TEXT("FAILURE_ATOMICITY"),
                    TEXT("phase 2 proof flags or isolated DG Apply mutation were incomplete"));
                return;
            }
            Pass();
            return;
        }

        case EStep::Phase3CaptureFreshDG:
            if (!RequestCapture(
                    TEXT("06_DG_Gameplay_Fresh_Reload.png"),
                    TEXT("dg_gameplay_fresh_process_reload"),
                    EStep::Phase3ApplyMetaHuman,
                    Error))
            {
                Fail(TEXT("CAPTURE_REQUEST"), Error);
            }
            return;

        case EStep::Phase3ApplyMetaHuman:
        {
            if (!OpenCreator(Error)
                || !SelectBackend(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    Error))
            {
                Fail(TEXT("DG_TO_METAHUMAN_RESTORE"), Error);
                return;
            }
            const FDGFullCharacterCustomization Expected =
                PlayerController->GetCharacterCreatorDraftCustomization();
            if (!ApplyCreatorDraft(Error)
                || !VerifyCurrentCustomization(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &Expected, Error)
                || !FreezeStableThrowPresentation(Error)
                || !VerifyDiskProfile(
                    FName(DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId),
                    &Expected, Error)
                || !LoadSaveBytes(CheckpointSaveBytes)
                || CheckpointSaveBytes == InitialSaveBytes)
            {
                Fail(TEXT("DG_TO_METAHUMAN_RESTORE"), Error);
                return;
            }
            bSaveWasMutatedByPhase = true;
            BaselineStrokes = GameMode->GetStrokes();
            BaselineWorldDiscs = CountWorldDiscs();
            BaselineReleaseCommits = ThrowAdapter->GetTotalReleaseCommitCount();
            SetStep(EStep::Phase3BeginTiming);
            return;
        }

        case EStep::Phase3BeginTiming:
            if (!BeginRealRHBHTiming(Error))
            {
                Fail(TEXT("RHBH_FIRST_PRESS"), Error);
                return;
            }
            SetStep(EStep::Phase3CommitTiming);
            return;

        case EStep::Phase3CommitTiming:
            if (Now - StepStartedSeconds < TimingCaptureSeconds)
            {
                return;
            }
            if (!CommitRealRHBHThrow(Error))
            {
                Fail(TEXT("RHBH_SECOND_PRESS"), Error);
                return;
            }
            SetStep(EStep::Phase3CaptureGrip);
            return;

        case EStep::Phase3CaptureGrip:
            if (FrameworkThrow->CurrentPhase != EDGThrowPhase::ReachBack)
            {
                return;
            }
            if (!PauseForThrowCapture(Error)
                || !RequestCapture(
                    TEXT("07_MH_RHBH_Grip.png"),
                    TEXT("metahuman_rhbh_grip_before_release"),
                    EStep::Phase3AwaitRelease,
                    Error))
            {
                Fail(TEXT("GRIP_CAPTURE"), Error);
            }
            return;

        case EStep::Phase3AwaitRelease:
            if (ReleaseCallbackCount == 0)
            {
                return;
            }
            if (!bReleasePresentationCapturePending
                || !PauseForThrowCapture(Error)
                || !RequestCapture(
                    TEXT("08_MH_RHBH_Release.png"),
                    TEXT("metahuman_rhbh_release_authoritative_disc_at_grip"),
                    EStep::Phase3AwaitFollowThrough,
                    Error))
            {
                Fail(TEXT("RELEASE_CAPTURE"), Error.IsEmpty()
                    ? TEXT("release callback did not hand off its same-frame presentation freeze")
                    : Error);
                return;
            }
            bReleasePresentationCapturePending = false;
            return;

        case EStep::Phase3AwaitFollowThrough:
            if (!bDuplicateReleaseNoOpPassed)
            {
                const int32 CommitsBefore =
                    ThrowAdapter->GetTotalReleaseCommitCount();
                const int32 StrokesBefore = GameMode->GetStrokes();
                const int32 DiscsBefore = CountWorldDiscs();
                ADiscActor* const ActiveDiscBefore =
                    GameMode->GetActiveDisc();
                FrameworkThrow->NotifyDiscRelease(
                    Golfer->GetSkeletalGolferMesh());
                bDuplicateReleaseNoOpPassed =
                    ThrowAdapter->GetTotalReleaseCommitCount()
                        == CommitsBefore
                    && ThrowAdapter->GetReleaseCommitCountForAttempt() == 1
                    && GameMode->GetStrokes() == StrokesBefore
                    && CountWorldDiscs() == DiscsBefore
                    && GameMode->GetActiveDisc() == ActiveDiscBefore;
                if (!bDuplicateReleaseNoOpPassed)
                {
                    Fail(TEXT("DUPLICATE_RELEASE"),
                        TEXT("duplicate framework release mutated gameplay authority"));
                    return;
                }
            }
            if (FrameworkThrow->CurrentPhase != EDGThrowPhase::FollowThrough)
            {
                return;
            }
            if (!PauseForThrowCapture(Error)
                || !RequestCapture(
                    TEXT("09_MH_RHBH_FollowThrough.png"),
                    TEXT("metahuman_rhbh_follow_through"),
                    EStep::Phase3AwaitRecovery,
                    Error))
            {
                Fail(TEXT("FOLLOW_THROUGH_CAPTURE"), Error);
            }
            return;

        case EStep::Phase3AwaitRecovery:
        {
            if (RecoveryCallbackCount != 1
                || ThrowAdapter->IsThrowActive()
                || !GameMode->HasLastFlightTelemetry()
                || GameMode->GetActiveDisc()
                || !GameMode->CanPlayerThrow())
            {
                return;
            }
            TArray<uint8> CurrentSaveBytes;
            if (!VerifyStableThrowPresentation(Error)
                || !LoadSaveBytes(CurrentSaveBytes)
                || CurrentSaveBytes != CheckpointSaveBytes
                || !bDuplicateReleaseNoOpPassed)
            {
                Fail(TEXT("RHBH_RECOVERY"), Error.IsEmpty()
                    ? TEXT("flight/recovery changed the saved profile or authority proof")
                    : Error);
                return;
            }
            Pass();
            return;
        }

        case EStep::Phase4WarmupCreator:
            PlayerController->RotateCharacterCreatorPreview(0.35f);
            if (Now - PerformanceWarmupStartedSeconds
                < PerformanceWarmupSeconds)
            {
                return;
            }
            if (!BeginPerformanceSegment(TEXT("creator_closeup"), Error))
            {
                Fail(TEXT("CREATOR_PERFORMANCE"), Error);
                return;
            }
            SetStep(EStep::Phase4SampleCreator);
            return;

        case EStep::Phase4SampleCreator:
            PlayerController->RotateCharacterCreatorPreview(0.35f);
            TickPerformanceSegment();
            if (Now - PerformanceSegmentStartedSeconds
                < PerformanceSegmentSeconds)
            {
                return;
            }
            if (!FinishPerformanceSegment(TEXT("creator_closeup"), Error))
            {
                Fail(TEXT("CREATOR_PERFORMANCE"), Error);
                return;
            }
            {
                FPresentationPolicyEvidence Before;
                FPresentationPolicyEvidence After;
                if (!PopulatePresentationPolicyEvidence(
                        EDGMetaHumanPresentationPolicy::CharacterCreator,
                        Before,
                        Error))
                {
                    Fail(TEXT("CREATOR_PERFORMANCE"), Error);
                    return;
                }
                PlayerController->CancelCharacterCreator();
                const bool bCloseSucceeded =
                    !PlayerController->IsCharacterCreatorOpen()
                    && !UGameplayStatics::IsGamePaused(this);
                if (!PopulatePresentationPolicyEvidence(
                        EDGMetaHumanPresentationPolicy::GameplayPerformance,
                        After,
                        Error))
                {
                    Fail(TEXT("CREATOR_PERFORMANCE"), Error);
                    return;
                }
                FPresentationPolicyCounters ExitDelta;
                ExitDelta.TransitionSuccess = 1;
                if (!AppendPresentationPolicyProbe(
                        TEXT("phase4_gameplay_exit"),
                        TEXT("COMMITTED_ACTIVE_TRANSITION"),
                        TEXT("CancelCharacterCreator_EndCharacterCreatorPreview"),
                        bCloseSucceeded,
                        true,
                        ExitDelta,
                        false,
                        Before,
                        After,
                        PresentationPolicyTransitionProbes,
                        Error)
                    || !RunRedundantPresentationPolicyProbe(
                        TEXT("phase4_gameplay_redundant_no_op"),
                        EDGMetaHumanPresentationPolicy::GameplayPerformance,
                        Error))
                {
                    Fail(TEXT("CREATOR_PERFORMANCE"), Error);
                    return;
                }
                bPresentationPolicyTransitionProbesPassed =
                    PresentationPolicyTransitionProbes.Num() == 4;
            }
            if (PlayerController->IsCharacterCreatorOpen()
                || UGameplayStatics::IsGamePaused(this)
                || !VerifyMetaHumanPresentation(Error)
                || !LoadSaveBytes(InitialSaveBytes)
                || InitialSaveBytes != CheckpointSaveBytes)
            {
                Fail(TEXT("CREATOR_PERFORMANCE"), Error.IsEmpty()
                    ? TEXT("performance creator closeout changed the isolated save")
                    : Error);
                return;
            }
            if (!FreezeStableThrowPresentation(Error))
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error);
                return;
            }
            BaselineStrokes = GameMode->GetStrokes();
            BaselineWorldDiscs = CountWorldDiscs();
            BaselineReleaseCommits = ThrowAdapter->GetTotalReleaseCommitCount();
            SetStep(EStep::Phase4BeginGameplay);
            return;

        case EStep::Phase4BeginGameplay:
            if (!BeginRealRHBHTiming(Error))
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error);
                return;
            }
            SetStep(EStep::Phase4CommitTiming);
            return;

        case EStep::Phase4CommitTiming:
            if (Now - StepStartedSeconds < TimingCaptureSeconds)
            {
                return;
            }
            if (!GameMode->DeferNextTrajectoryExport(Error)
                || !GameMode->IsTrajectoryExportDeferralArmed()
                || !CommitRealRHBHThrow(Error))
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error);
                return;
            }
            if (!BeginPerformanceSegment(TEXT("rhbh_gameplay"), Error))
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error);
                return;
            }
            SetStep(EStep::Phase4SampleGameplay);
            return;

        case EStep::Phase4SampleGameplay:
            TickPerformanceSegment();
            if (Now - PerformanceSegmentStartedSeconds
                < PerformanceSegmentSeconds)
            {
                return;
            }
            DeferredPerformanceFailure.Reset();
            if (!FinishPerformanceSegment(TEXT("rhbh_gameplay"), Error))
            {
                DeferredPerformanceFailure = Error;
                Error.Reset();
            }
            if (!VerifyStableThrowPresentation(Error)
                || ThrowAdapter->GetTotalReleaseCommitCount()
                    != BaselineReleaseCommits + 1
                || !ThrowAdapter->WasLastAuthoritativeLaunchAccepted()
                || !bPerformanceSawAuthoritativeFlight
                || !bPresentationPolicyTransitionProbesPassed
                || PresentationPolicyTransitionProbes.Num() != 4
                || !LoadSaveBytes(InitialSaveBytes)
                || InitialSaveBytes != CheckpointSaveBytes)
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error.IsEmpty()
                    ? TEXT("rendered gameplay did not retain one accepted RHBH release and unchanged save")
                    : Error);
                return;
            }
            SetStep(EStep::Phase4AwaitRecovery);
            return;

        case EStep::Phase4AwaitRecovery:
            if (RecoveryCallbackCount != 1
                || ThrowAdapter->IsThrowActive()
                || !GameMode->HasLastFlightTelemetry()
                || GameMode->GetActiveDisc()
                || !GameMode->CanPlayerThrow())
            {
                return;
            }
            if (!VerifyStableThrowPresentation(Error)
                || ThrowAdapter->GetTotalReleaseCommitCount()
                    != BaselineReleaseCommits + 1
                || !ThrowAdapter->WasLastAuthoritativeLaunchAccepted()
                || !bPerformanceSawAuthoritativeFlight
                || !LoadSaveBytes(InitialSaveBytes)
                || InitialSaveBytes != CheckpointSaveBytes)
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error.IsEmpty()
                    ? TEXT("rendered RHBH lifecycle did not recover through the sole authoritative release/flight path with unchanged save")
                    : Error);
                return;
            }
            {
                FDiscTrajectorySummary PendingSummary;
                FDiscTrajectorySummary FlushedSummary;
                bTrajectoryExportDeferredAtSettlement =
                    GameMode->HasPendingDeferredTrajectoryExport()
                    && !GameMode->IsTrajectoryExportDeferralArmed();
                bTrajectorySummaryReadyBeforeFlush =
                    GameMode->GetPendingDeferredTrajectorySummary(
                        PendingSummary)
                    && !PendingSummary.CaptureId.IsEmpty();
                DeferredTrajectoryCaptureId = PendingSummary.CaptureId;
                bTrajectoryExportFlushedAfterSegment =
                    PerformanceRecords.Num() == 2;
                bTrajectoryExportFlushPassed =
                    bTrajectoryExportDeferredAtSettlement
                    && bTrajectorySummaryReadyBeforeFlush
                    && bTrajectoryExportFlushedAfterSegment
                    && GameMode->FlushDeferredTrajectoryExport(
                        FlushedSummary, Error)
                    && !GameMode->HasPendingDeferredTrajectoryExport()
                    && !GameMode->IsTrajectoryExportDeferralArmed()
                    && FlushedSummary.CaptureId
                        == PendingSummary.CaptureId;
                FlushedTrajectoryCaptureId = FlushedSummary.CaptureId;
                if (!bTrajectoryExportFlushPassed)
                {
                    Fail(TEXT("GAMEPLAY_PERFORMANCE"), Error.IsEmpty()
                        ? TEXT("trajectory export was not deferred until after the timed gameplay segment")
                        : Error);
                    return;
                }
            }
            if (!DeferredPerformanceFailure.IsEmpty())
            {
                Fail(TEXT("GAMEPLAY_PERFORMANCE"),
                    DeferredPerformanceFailure);
                return;
            }
            Pass();
            return;

        case EStep::Finished:
        case EStep::Idle:
        default:
            Fail(TEXT("STATE_MACHINE"),
                TEXT("runner entered an invalid or finished state before exit"));
            return;
    }
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::RequestCapture(
    const TCHAR* Filename,
    const TCHAR* Scene,
    EStep NextStep,
    FString& OutError)
{
    if (bCapturePending || !Filename || !Scene
        || FCString::Strlen(Filename) == 0
        || FCString::Strlen(Scene) == 0)
    {
        OutError = TEXT("a screenshot was already pending or its metadata was empty");
        return false;
    }
    const FIntPoint ViewportSize = GEngine && GEngine->GameViewport
            && GEngine->GameViewport->Viewport
        ? GEngine->GameViewport->Viewport->GetSizeXY()
        : FIntPoint::ZeroValue;
    if (ViewportSize != FIntPoint(ExpectedWidth, ExpectedHeight))
    {
        OutError = TEXT("screenshot request lost the exact 1920x1080 viewport");
        return false;
    }
    PendingCaptureFilename = Filename;
    PendingCaptureScene = Scene;
    PendingCapturePath = FPaths::Combine(OutputDirectory, Filename);
    IFileManager::Get().Delete(*PendingCapturePath, false, true, true);
    PendingCaptureStartedSeconds = FPlatformTime::Seconds();
    NextStepAfterCapture = NextStep;
    bCapturePending = true;
    bCaptureIssued = false;
    return true;
}

void ADiscGolfSession8BMetaHumanPackagedRunner::PollCapture()
{
    const double Now = FPlatformTime::Seconds();
    if (!bCaptureIssued)
    {
        if (GEngine)
        {
            GEngine->ClearOnScreenDebugMessages();
        }
        if (Now - PendingCaptureStartedSeconds < ScreenshotSettleSeconds)
        {
            return;
        }
        FCaptureRecord Preflight;
        Preflight.Filename = PendingCaptureFilename;
        Preflight.Scene = PendingCaptureScene;
        FString Error;
        if (!PopulateCaptureScene(Preflight, Error))
        {
            if ((Error.StartsWith(HairRenderPendingPrefix)
                    || Error.StartsWith(VisualRenderPendingPrefix)
                    || Error.StartsWith(OutfitSkinPendingPrefix))
                && Now - PendingCaptureStartedSeconds
                    < RenderReadinessTimeoutSeconds)
            {
                return;
            }
            Fail(TEXT("CAPTURE_SCENE"), Error);
            return;
        }
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath,
            true,
            false,
            false,
            FIntRect(),
            true);
        bCaptureIssued = true;
        PendingCaptureStartedSeconds = Now;
        return;
    }

    const int64 Size = IFileManager::Get().FileSize(*PendingCapturePath);
    if (Size >= MinimumPngBytes)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *PendingCapturePath)
            || Bytes.Num() < 24
            || Bytes[0] != 0x89 || Bytes[1] != 0x50
            || Bytes[2] != 0x4e || Bytes[3] != 0x47)
        {
            Fail(TEXT("CAPTURE_FILE"),
                TEXT("screenshot output was not a readable PNG"));
            return;
        }
        FCaptureRecord Capture;
        Capture.Filename = PendingCaptureFilename;
        Capture.Scene = PendingCaptureScene;
        Capture.Bytes = Size;
        Capture.Width = ReadBigEndianInt32(&Bytes[16]);
        Capture.Height = ReadBigEndianInt32(&Bytes[20]);
        FString Error;
        if (Capture.Width != ExpectedWidth
            || Capture.Height != ExpectedHeight
            || !PopulateCaptureScene(Capture, Error))
        {
            Fail(TEXT("CAPTURE_FILE"), Error.IsEmpty()
                ? FString::Printf(
                    TEXT("capture was %dx%d instead of 1920x1080"),
                    Capture.Width, Capture.Height)
                : Error);
            return;
        }
        Captures.Add(MoveTemp(Capture));
        const EStep CompletedNextStep = NextStepAfterCapture;
        bCapturePending = false;
        bCaptureIssued = false;
        PendingCapturePath.Reset();
        PendingCaptureFilename.Reset();
        PendingCaptureScene.Reset();
        ResumeAfterThrowCapture();
        SetStep(CompletedNextStep);
        return;
    }
    if (Now - PendingCaptureStartedSeconds > ScreenshotTimeoutSeconds)
    {
        Fail(TEXT("CAPTURE_TIMEOUT"),
            TEXT("rendered screenshot did not complete within 30 seconds"));
    }
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::PopulateCaptureScene(
    FCaptureRecord& OutCapture,
    FString& OutError) const
{
    if (!GameMode || !PlayerController || !Golfer || !AvatarBackend
        || !ThrowAdapter || !FrameworkThrow)
    {
        OutError = TEXT("capture scene lost a required runtime authority");
        return false;
    }
    const FDGFullCharacterCustomization Current =
        Golfer->GetCurrentFullCharacterCustomization();
    OutCapture.BackendId = Current.AvatarBackendId.ToString();
    AActor* const VisualActor = AvatarBackend->GetActiveVisualActor();
    OutCapture.ActiveVisualActor = IsValid(VisualActor)
        ? VisualActor->GetPathName() : FString();
    OutCapture.StrokeCount = GameMode->GetStrokes();
    OutCapture.WorldDiscCount = CountWorldDiscs();
    OutCapture.ReleaseCommitCount =
        ThrowAdapter->GetTotalReleaseCommitCount();
    OutCapture.bCreatorOpen = PlayerController->IsCharacterCreatorOpen();
    OutCapture.bDGProxyVisible =
        Golfer->IsDGProxyPresentationVisible();
    OutCapture.bMetaHumanReady =
        AvatarBackend->IsVisualBackendReady();
    UStaticMeshComponent* const HeldDisc =
        Golfer->GetHeldDiscVisual();
    OutCapture.bHeldDiscVisible = HeldDisc
        && HeldDisc->IsVisible() && !HeldDisc->bHiddenInGame;
    OutCapture.bGameplayDiscActive =
        GameMode->GetActiveDisc() != nullptr;
    OutCapture.ThrowPhase = ThrowPhaseName(FrameworkThrow->CurrentPhase);
    const FName GripBone = FrameworkThrow->GetActiveDiscGripBone();
    if (!GripBone.IsNone()
        && Golfer->GetSkeletalGolferMesh()->DoesSocketExist(GripBone))
    {
        OutCapture.GripWorldLocation =
            Golfer->GetSkeletalGolferMesh()->GetSocketLocation(GripBone);
    }
    if (OutCapture.Filename.StartsWith(TEXT("08_"))
        && bHasImmutableReleaseGripWorldLocation)
    {
        // The gameplay authority spawns the disc at the release transform
        // before broadcasting OnReleaseCommitted. Preserve that synchronous
        // point rather than re-querying an animation socket after settle time.
        OutCapture.GripWorldLocation = ImmutableReleaseGripWorldLocation;
    }
    if (GameMode->GetActiveDisc())
    {
        OutCapture.GameplayDiscWorldLocation =
            GameMode->GetActiveDisc()->GetActorLocation();
        OutCapture.DiscToGripDistanceCm = FVector::Distance(
            OutCapture.GameplayDiscWorldLocation,
            OutCapture.GripWorldLocation);
    }
    const bool bReleaseFrame =
        OutCapture.Filename.StartsWith(TEXT("08_"));
    if (bReleaseFrame)
    {
        UDiscFlightComponent* const ReleasedFlight =
            IsValid(ReleasedGameplayDisc)
            ? ReleasedGameplayDisc->GetFlightComponent() : nullptr;
        OutCapture.bReleaseEvidenceCollected = true;
        OutCapture.ExpectedStrokeCount = BaselineStrokes + 1;
        OutCapture.ExpectedWorldDiscCount = BaselineWorldDiscs + 1;
        OutCapture.ExpectedReleaseCommitCount = BaselineReleaseCommits + 1;
        OutCapture.bImmutableReleaseGripValid =
            bHasImmutableReleaseGripWorldLocation
            && !ImmutableReleaseGripWorldLocation.ContainsNaN();
        OutCapture.bGameplayDiscLocationFinite =
            !OutCapture.GameplayDiscWorldLocation.ContainsNaN();
        OutCapture.bDiscToGripDistanceFinite =
            FMath::IsFinite(OutCapture.DiscToGripDistanceCm);
        OutCapture.bReleasedDiscIdentityVerified =
            IsValid(ReleasedGameplayDisc)
            && GameMode->GetActiveDisc() == ReleasedGameplayDisc;
        OutCapture.bFlightComponentIdentityVerified =
            IsValid(ReleasedFlight)
            && ReleasedFlight == ThrowCaptureFlightComponent;
        OutCapture.bFlightIsFlying =
            IsValid(ReleasedFlight) && ReleasedFlight->IsFlying();
        OutCapture.bFlightTickSnapshotPresent =
            bHasThrowCaptureFlightTickSnapshot;
        OutCapture.bFlightTickWasEnabledBeforePause =
            bThrowCaptureFlightTickWasEnabled;
        OutCapture.bFlightTickSuspended =
            IsValid(ReleasedFlight)
            && !ReleasedFlight->IsComponentTickEnabled();
        OutCapture.bWorldPausedForCapture =
            bPausedForThrowCapture
            && UGameplayStatics::IsGamePaused(this);
    }

    const bool bExpectMetaHuman =
        OutCapture.Filename.StartsWith(TEXT("02_"))
        || OutCapture.Filename.StartsWith(TEXT("03_"))
        || OutCapture.Filename.StartsWith(TEXT("05_"))
        || OutCapture.Filename.StartsWith(TEXT("07_"))
        || OutCapture.Filename.StartsWith(TEXT("08_"))
        || OutCapture.Filename.StartsWith(TEXT("09_"));
    const bool bExpectStableThrowPresentation =
        OutCapture.Filename.StartsWith(TEXT("07_"))
        || OutCapture.Filename.StartsWith(TEXT("08_"))
        || OutCapture.Filename.StartsWith(TEXT("09_"));
    if (bExpectStableThrowPresentation)
    {
        if (!VerifyStableThrowPresentation(OutError))
        {
            return false;
        }
    }
    else if (bExpectMetaHuman)
    {
        if (!VerifyMetaHumanPresentation(OutError))
        {
            return false;
        }
    }
    else if (!VerifyDGPresentation(OutError))
    {
        return false;
    }
    if (bExpectMetaHuman
        && !PopulateMetaHumanRenderEvidence(OutCapture, OutError))
    {
        return false;
    }
    if (bExpectMetaHuman
        && !PopulatePresentationPolicyEvidence(
            OutCapture.Filename.StartsWith(TEXT("02_"))
                || OutCapture.Filename.StartsWith(TEXT("03_"))
                ? EDGMetaHumanPresentationPolicy::CharacterCreator
                : EDGMetaHumanPresentationPolicy::GameplayPerformance,
            OutCapture.PresentationPolicy,
            OutError))
    {
        return false;
    }

    const bool bExpectCreator =
        OutCapture.Filename.StartsWith(TEXT("01_"))
        || OutCapture.Filename.StartsWith(TEXT("02_"))
        || OutCapture.Filename.StartsWith(TEXT("03_"))
        || OutCapture.Filename.StartsWith(TEXT("04_"));
    if (OutCapture.bCreatorOpen != bExpectCreator)
    {
        OutError = TEXT("capture creator-open state did not match its frozen scene contract");
        return false;
    }

    if (OutCapture.Filename.StartsWith(TEXT("07_"))
        && (!OutCapture.bHeldDiscVisible
            || OutCapture.bGameplayDiscActive
            || FrameworkThrow->CurrentPhase != EDGThrowPhase::ReachBack))
    {
        OutError = TEXT("grip frame did not retain the held disc before release");
        return false;
    }
    if (bReleaseFrame
        && (OutCapture.bHeldDiscVisible
            || !OutCapture.bGameplayDiscActive
            || !OutCapture.bReleaseEvidenceCollected
            || !OutCapture.bImmutableReleaseGripValid
            || !OutCapture.bGameplayDiscLocationFinite
            || !OutCapture.bDiscToGripDistanceFinite
            || !OutCapture.bReleasedDiscIdentityVerified
            || OutCapture.ReleaseCommitCount
                != OutCapture.ExpectedReleaseCommitCount
            || OutCapture.StrokeCount != OutCapture.ExpectedStrokeCount
            || OutCapture.WorldDiscCount != OutCapture.ExpectedWorldDiscCount
            || OutCapture.DiscToGripDistanceCm < 0.0f
            || OutCapture.DiscToGripDistanceCm > 25.0f
            || !OutCapture.bFlightComponentIdentityVerified
            || !OutCapture.bFlightIsFlying
            || !OutCapture.bFlightTickSnapshotPresent
            || !OutCapture.bFlightTickWasEnabledBeforePause
            || !OutCapture.bFlightTickSuspended
            || !OutCapture.bWorldPausedForCapture))
    {
        OutError = FString::Printf(
            TEXT("release frame contract failed: held=%d active=%d evidence=%d ")
            TEXT("immutable=%d disc_finite=%d distance_finite=%d identity=%d ")
            TEXT("commits=%d/%d strokes=%d/%d world_discs=%d/%d distance_cm=%.3f ")
            TEXT("flight_identity=%d flying=%d tick_snapshot=%d tick_before=%d ")
            TEXT("tick_suspended=%d world_paused=%d"),
            OutCapture.bHeldDiscVisible ? 1 : 0,
            OutCapture.bGameplayDiscActive ? 1 : 0,
            OutCapture.bReleaseEvidenceCollected ? 1 : 0,
            OutCapture.bImmutableReleaseGripValid ? 1 : 0,
            OutCapture.bGameplayDiscLocationFinite ? 1 : 0,
            OutCapture.bDiscToGripDistanceFinite ? 1 : 0,
            OutCapture.bReleasedDiscIdentityVerified ? 1 : 0,
            OutCapture.ReleaseCommitCount,
            OutCapture.ExpectedReleaseCommitCount,
            OutCapture.StrokeCount,
            OutCapture.ExpectedStrokeCount,
            OutCapture.WorldDiscCount,
            OutCapture.ExpectedWorldDiscCount,
            OutCapture.DiscToGripDistanceCm,
            OutCapture.bFlightComponentIdentityVerified ? 1 : 0,
            OutCapture.bFlightIsFlying ? 1 : 0,
            OutCapture.bFlightTickSnapshotPresent ? 1 : 0,
            OutCapture.bFlightTickWasEnabledBeforePause ? 1 : 0,
            OutCapture.bFlightTickSuspended ? 1 : 0,
            OutCapture.bWorldPausedForCapture ? 1 : 0);
        return false;
    }
    if (OutCapture.Filename.StartsWith(TEXT("09_"))
        && (OutCapture.bHeldDiscVisible
            || FrameworkThrow->CurrentPhase
                != EDGThrowPhase::FollowThrough))
    {
        OutError = TEXT("follow-through frame was not in the real post-release phase");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    PopulateOutfitLiveSkinEvidence(
        USkeletalMeshComponent* Body,
        USkeletalMeshComponent* Outfit,
        FCaptureRecord::FOutfitLiveSkinEvidence& OutEvidence,
        FString& OutError) const
{
    OutEvidence = FCaptureRecord::FOutfitLiveSkinEvidence();
    auto FailStructural = [&OutEvidence, &OutError](
        const FString& Reason,
        bool bPending = false) -> bool
    {
        OutEvidence.StructuralReason = Reason;
        OutError = bPending
            ? FString(OutfitSkinPendingPrefix) + TEXT(" ") + Reason
            : Reason;
        if (!bPending)
        {
            UE_LOG(LogDiscGolfTour, Error,
                TEXT("%s: OUTFIT_LIVE_SKIN_STRUCTURAL_FAILURE reason=%s"),
                Marker,
                *Reason);
        }
        return false;
    };
    auto IsFiniteMatrix = [](const FMatrix44f& Matrix)
    {
        for (int32 Row = 0; Row < 4; ++Row)
        {
            for (int32 Column = 0; Column < 4; ++Column)
            {
                if (!FMath::IsFinite(Matrix.M[Row][Column]))
                {
                    return false;
                }
            }
        }
        return true;
    };

    if (!IsInGameThread() || !IsValid(Body) || !IsValid(Outfit))
    {
        return FailStructural(
            TEXT("Outfit live skin probe lost its game-thread Body/Outfit tuple"));
    }
    OutEvidence.PoseSyncMode = TEXT("LEADER_POSE_BODY");
    if (!Outfit->LeaderPoseComponent.IsValid()
        || Outfit->LeaderPoseComponent.Get() != Body
        || Outfit->GetAnimInstance()
        || Outfit->GetPostProcessInstance())
    {
        return FailStructural(
            TEXT("Outfit live skin probe requires the exact Body LeaderPose branch"));
    }
    if (Body->IsRunningParallelEvaluation()
        || Outfit->IsRunningParallelEvaluation())
    {
        return FailStructural(
            TEXT("Body or Outfit still had a parallel pose evaluation in flight"),
            true);
    }

    USkeletalMesh* const BodyAsset = Body->GetSkeletalMeshAsset();
    USkeletalMesh* const OutfitAsset = Outfit->GetSkeletalMeshAsset();
    if (!BodyAsset || !OutfitAsset
        || OutfitAsset->GetPathName() != ExpectedOutfitMeshAssetPath)
    {
        return FailStructural(
            TEXT("Outfit live skin probe did not retain its exact generated assets"));
    }

    const FSkeletalMeshObject* const MeshObject = Outfit->GetMeshObject();
    OutEvidence.bMeshObjectPresent = MeshObject != nullptr;
    OutEvidence.bMeshObjectDynamicDataValid = MeshObject
        && MeshObject->HaveValidDynamicData();
    if (!MeshObject || !MeshObject->HaveValidDynamicData())
    {
        return FailStructural(
            TEXT("Outfit render object did not yet expose valid dynamic data"),
            true);
    }
    if (MeshObject->IsNaniteMesh())
    {
        return FailStructural(
            TEXT("Outfit uses an unsupported Nanite deformation path"));
    }

    OutEvidence.ActualRenderedLOD = MeshObject->GetLOD();
    OutEvidence.PredictedLOD = Outfit->GetPredictedLODLevel();
    OutEvidence.ForcedLODLegacyOneBased = Outfit->GetForcedLOD();
    OutEvidence.ComputedMinLOD = Outfit->ComputeMinLOD();
    OutEvidence.AssetMinLOD = OutfitAsset->GetMinLodIdx();
    const ILODSyncInterface* const LODInterface =
        static_cast<const ILODSyncInterface*>(Outfit);
    OutEvidence.DesiredSyncLOD = LODInterface->GetDesiredSyncLOD();
    OutEvidence.BestAvailableLOD = LODInterface->GetBestAvailableLOD();
    OutEvidence.ForceRenderedLOD = LODInterface->GetForceRenderedLOD();
    OutEvidence.ForceStreamedLOD = LODInterface->GetForceStreamedLOD();

    FSkeletalMeshRenderData* const RenderData =
        OutfitAsset->GetResourceForRendering();
    if (!RenderData
        || &MeshObject->GetSkeletalMeshRenderData() != RenderData)
    {
        return FailStructural(
            TEXT("Outfit render object did not yet match the current asset render data"),
            true);
    }
    OutEvidence.CurrentFirstLOD = RenderData->CurrentFirstLODIdx;
    OutEvidence.PendingFirstLOD = RenderData->PendingFirstLODIdx;
    if (!RenderData->LODRenderData.IsValidIndex(
            OutEvidence.ActualRenderedLOD))
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual rendered LOD %d was outside %d render LODs"),
            OutEvidence.ActualRenderedLOD,
            RenderData->LODRenderData.Num()));
    }
    const int32 FirstReliableResidentLOD = FMath::Max(
        RenderData->CurrentFirstLODIdx,
        RenderData->PendingFirstLODIdx);
    if (OutEvidence.BestAvailableLOD != FirstReliableResidentLOD
        || OutEvidence.ActualRenderedLOD < FirstReliableResidentLOD)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual rendered LOD %d was not reliably resident: current_first=%d pending_first=%d best_available=%d"),
            OutEvidence.ActualRenderedLOD,
            RenderData->CurrentFirstLODIdx,
            RenderData->PendingFirstLODIdx,
            OutEvidence.BestAvailableLOD),
            true);
    }
    const FSkeletalMeshLODRenderData& LOD =
        RenderData->LODRenderData[OutEvidence.ActualRenderedLOD];
    if (!LOD.IsDataReady())
    {
        return FailStructural(
            TEXT("Outfit actual rendered LOD data was not ready"),
            true);
    }

    OutEvidence.VertexCount = static_cast<int32>(LOD.GetNumVertices());
    OutEvidence.SectionCount = LOD.RenderSections.Num();
    if (LOD.GetNumVertices() == 0
        || LOD.GetNumVertices() > static_cast<uint32>(MAX_int32)
        || LOD.RenderSections.Num() != 2)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual LOD did not contain the exact two-section vertex topology: vertices=%u sections=%d"),
            LOD.GetNumVertices(),
            LOD.RenderSections.Num()));
    }

    const FPositionVertexBuffer& PositionBuffer =
        LOD.StaticVertexBuffers.PositionVertexBuffer;
    OutEvidence.bPositionCPUAccessRequested =
        PositionBuffer.GetAllowCPUAccess();
    OutEvidence.bPositionDataPresent = PositionBuffer.GetVertexData()
        && PositionBuffer.GetNumVertices() == LOD.GetNumVertices();
    OutEvidence.bSkinWeightProfilePending =
        Outfit->IsSkinWeightProfilePending();
    OutEvidence.bUsingSkinWeightProfile =
        Outfit->IsUsingSkinWeightProfile();
    if (OutEvidence.bSkinWeightProfilePending)
    {
        return FailStructural(
            TEXT("Outfit skin-weight profile was still pending"),
            true);
    }
    if (OutEvidence.bUsingSkinWeightProfile)
    {
        return FailStructural(
            TEXT("Outfit used a skin-weight profile whose render-side buffer identity was not proven"));
    }
    FSkinWeightVertexBuffer* const SkinWeights =
        Outfit->GetSkinWeightBuffer(OutEvidence.ActualRenderedLOD);
    const FSkinWeightDataVertexBuffer* const SkinWeightData =
        SkinWeights ? SkinWeights->GetDataVertexBuffer() : nullptr;
    const FSkinWeightLookupVertexBuffer* const SkinWeightLookup =
        SkinWeights ? SkinWeights->GetLookupVertexBuffer() : nullptr;
    OutEvidence.bSkinWeightCPUAccessRequested = SkinWeights
        && SkinWeights->GetNeedsCPUAccess();
    OutEvidence.bSkinWeightDataPresent = SkinWeightData
        && SkinWeightData->GetWeightData();
    OutEvidence.bSkinWeightLookupPresent = SkinWeights
        && (!SkinWeights->GetVariableBonesPerVertex()
            || (SkinWeightLookup
                && SkinWeightLookup->GetNeedsCPUAccess()
                && SkinWeightLookup->GetLookupData()
                && SkinWeightLookup->GetNumVertices()
                    == LOD.GetNumVertices()));
    OutEvidence.bSkinWeightVertexCountMatches = SkinWeights
        && SkinWeights->GetNumVertices() == LOD.GetNumVertices();
    if (!OutEvidence.bPositionCPUAccessRequested
        || !OutEvidence.bPositionDataPresent
        || !OutEvidence.bSkinWeightCPUAccessRequested
        || !OutEvidence.bSkinWeightDataPresent
        || !OutEvidence.bSkinWeightLookupPresent
        || !OutEvidence.bSkinWeightVertexCountMatches)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual LOD CPU buffers were unavailable: position_access=%d position_data=%d weight_access=%d weight_data=%d lookup=%d weight_vertices=%d"),
            OutEvidence.bPositionCPUAccessRequested ? 1 : 0,
            OutEvidence.bPositionDataPresent ? 1 : 0,
            OutEvidence.bSkinWeightCPUAccessRequested ? 1 : 0,
            OutEvidence.bSkinWeightDataPresent ? 1 : 0,
            OutEvidence.bSkinWeightLookupPresent ? 1 : 0,
            OutEvidence.bSkinWeightVertexCountMatches ? 1 : 0));
    }
    const uint32 MaximumInfluences = SkinWeights->GetMaxBoneInfluences();
    if (MaximumInfluences == 0
        || MaximumInfluences > MAX_TOTAL_INFLUENCES)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual LOD reported invalid maximum influences %u"),
            MaximumInfluences));
    }

    for (const TPair<FName, float>& Curve : Outfit->GetMorphTargetCurves())
    {
        if (!FMath::IsFinite(Curve.Value))
        {
            ++OutEvidence.NonFiniteMorphWeightCount;
        }
        else if (!FMath::IsNearlyZero(Curve.Value, KINDA_SMALL_NUMBER))
        {
            ++OutEvidence.NonZeroMorphCurveCount;
        }
    }
    OutEvidence.MorphTargetMapEntryCount = Outfit->ActiveMorphTargets.Num();
    TSet<int32> NamedMorphWeightIndices;
    TArray<FString> ActiveMorphTargetParts;
    for (const auto& Morph : Outfit->ActiveMorphTargets)
    {
        if (!IsValid(Morph.Key)
            || !Outfit->MorphTargetWeights.IsValidIndex(Morph.Value))
        {
            ++OutEvidence.InvalidActiveMorphTargetCount;
            continue;
        }
        NamedMorphWeightIndices.Add(Morph.Value);
        const float Weight = Outfit->MorphTargetWeights[Morph.Value];
        if (!FMath::IsFinite(Weight))
        {
            ++OutEvidence.NonFiniteMorphWeightCount;
            continue;
        }
        if (!FMath::IsNearlyZero(Weight, KINDA_SMALL_NUMBER))
        {
            ++OutEvidence.ActiveMorphTargetCount;
            ActiveMorphTargetParts.Add(FString::Printf(
                TEXT("%s{%s}[%d]=%.6f"),
                *Morph.Key->GetName(),
                *Morph.Key->GetPathName(),
                Morph.Value,
                Weight));
        }
    }
    for (int32 WeightIndex = 0;
         WeightIndex < Outfit->MorphTargetWeights.Num();
         ++WeightIndex)
    {
        const float Weight = Outfit->MorphTargetWeights[WeightIndex];
        if (!FMath::IsFinite(Weight))
        {
            if (!NamedMorphWeightIndices.Contains(WeightIndex))
            {
                ++OutEvidence.NonFiniteMorphWeightCount;
            }
            continue;
        }
        if (!FMath::IsNearlyZero(Weight, KINDA_SMALL_NUMBER))
        {
            ++OutEvidence.NonZeroMorphTargetWeightCount;
            if (!NamedMorphWeightIndices.Contains(WeightIndex))
            {
                ActiveMorphTargetParts.Add(FString::Printf(
                    TEXT("<unmapped>[%d]=%.6f"),
                    WeightIndex,
                    Weight));
            }
        }
    }
    if (OutEvidence.InvalidActiveMorphTargetCount != 0
        || OutEvidence.NonFiniteMorphWeightCount != 0)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit active morph state was invalid: invalid_targets=%d non_finite_weights=%d"),
            OutEvidence.InvalidActiveMorphTargetCount,
            OutEvidence.NonFiniteMorphWeightCount));
    }
    ActiveMorphTargetParts.Sort();
    OutEvidence.ActiveMorphTargetSummary = FString::Join(
        ActiveMorphTargetParts, TEXT(","));
    OutEvidence.bHasActiveMorphTargets =
        OutEvidence.ActiveMorphTargetCount != 0
        || OutEvidence.NonZeroMorphTargetWeightCount != 0
        || OutEvidence.NonZeroMorphCurveCount != 0;

    if (Outfit->IsValidExternalMorphSetLODIndex(
            OutEvidence.ActualRenderedLOD))
    {
        const FExternalMorphWeightData& ExternalMorphWeights =
            Outfit->GetExternalMorphWeights(
                OutEvidence.ActualRenderedLOD);
        for (const auto& MorphSet : ExternalMorphWeights.MorphSets)
        {
            if (!FMath::IsFinite(MorphSet.Value.ActiveWeightThreshold)
                || MorphSet.Value.ActiveWeightThreshold < 0.0f)
            {
                ++OutEvidence.NonFiniteExternalMorphWeightCount;
            }
            for (const float Weight : MorphSet.Value.Weights)
            {
                if (!FMath::IsFinite(Weight))
                {
                    ++OutEvidence.NonFiniteExternalMorphWeightCount;
                }
                else if (FMath::Abs(Weight)
                    >= MorphSet.Value.ActiveWeightThreshold)
                {
                    ++OutEvidence.ActiveExternalMorphTargetCount;
                }
            }
        }
        if (OutEvidence.NonFiniteExternalMorphWeightCount != 0)
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit external morph state was invalid: active=%d non_finite_weights=%d"),
                OutEvidence.ActiveExternalMorphTargetCount,
                OutEvidence.NonFiniteExternalMorphWeightCount));
        }
        OutEvidence.bHasActiveExternalMorphTargets =
            OutEvidence.ActiveExternalMorphTargetCount != 0;
    }

    OutEvidence.bHasMeshDeformer =
        Outfit->HasMeshDeformer(OutEvidence.ActualRenderedLOD);
    UMeshDeformerInstance* const ActiveMeshDeformerInstance =
        Outfit->GetMeshDeformerInstanceForLOD(
            OutEvidence.ActualRenderedLOD);
    if (OutEvidence.bHasMeshDeformer)
    {
        if (!IsValid(ActiveMeshDeformerInstance))
        {
            return FailStructural(
                TEXT("Outfit advertised an active mesh deformer without a valid runtime instance"));
        }
        OutEvidence.ActiveMeshDeformerName =
            ActiveMeshDeformerInstance->GetName();
        OutEvidence.ActiveMeshDeformerPath =
            ActiveMeshDeformerInstance->GetPathName();
    }
    else if (ActiveMeshDeformerInstance)
    {
        return FailStructural(
            TEXT("Outfit active mesh-deformer state was internally inconsistent"));
    }
    OutEvidence.ClothingSimulationCount =
        Outfit->GetClothingSimulationInstances().Num();
    OutEvidence.bHasClothingSimulation =
        OutEvidence.ClothingSimulationCount != 0;
    OutEvidence.bBodyPostProcessDisabled =
        Body->GetDisablePostProcessBlueprint();
    OutEvidence.bBodyPostProcessShouldEvaluate =
        Body->ShouldEvaluatePostProcessInstance();
    OutEvidence.BodyPostProcessClassPath = Body->GetPostProcessInstance()
        ? Body->GetPostProcessInstance()->GetClass()->GetPathName()
        : FString();

    const FReferenceSkeleton& OutfitSkeleton =
        OutfitAsset->GetRefSkeleton();
    const FReferenceSkeleton& BodySkeleton = BodyAsset->GetRefSkeleton();
    const TSharedPtr<FSkelMeshRefPoseOverride>& RefPoseOverride =
        Outfit->GetRefPoseOverride();
    OutEvidence.bRefPoseOverridePresent = RefPoseOverride.IsValid();
    const TArray<FMatrix44f>& AssetInverseBind =
        OutfitAsset->GetRefBasesInvMatrix();
    const bool bRendererUsesRefPoseOverride = RefPoseOverride.IsValid()
        && RefPoseOverride->RefBasesInvMatrix.Num()
            == AssetInverseBind.Num();
    const TArray<FMatrix44f>& OutfitInverseBind =
        bRendererUsesRefPoseOverride
        ? RefPoseOverride->RefBasesInvMatrix
        : AssetInverseBind;
    const TArray<int32>& LeaderBoneMap = Outfit->GetLeaderBoneMap();
    const TArray<FTransform>& BodyComponentSpace =
        Body->GetComponentSpaceTransforms();
    OutEvidence.OutfitReferenceBoneCount = OutfitSkeleton.GetNum();
    OutEvidence.BodyReferenceBoneCount = BodySkeleton.GetNum();
    OutEvidence.BodyComponentSpaceTransformCount =
        BodyComponentSpace.Num();
    OutEvidence.LeaderBoneMapCount = LeaderBoneMap.Num();
    if (OutfitSkeleton.GetNum() <= 0
        || BodySkeleton.GetNum() <= 0
        || OutfitInverseBind.Num() != OutfitSkeleton.GetNum()
        || LeaderBoneMap.Num() != OutfitSkeleton.GetNum()
        || BodyComponentSpace.Num() != BodySkeleton.GetNum())
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit LeaderPose arrays were incomplete: outfit_ref=%d inverse=%d leader_map=%d body_ref=%d body_pose=%d"),
            OutfitSkeleton.GetNum(),
            OutfitInverseBind.Num(),
            LeaderBoneMap.Num(),
            BodySkeleton.GetNum(),
            BodyComponentSpace.Num()));
    }

    OutEvidence.BodyBoneRevisionBefore =
        Body->GetBoneTransformRevisionNumber();
    TSet<int32> UsedOutfitBones;
    TArray<uint8> VertexCoverage;
    VertexCoverage.Init(0, OutEvidence.VertexCount);
    OutEvidence.Sections.SetNum(LOD.RenderSections.Num());
    bool bFoundShirt = false;
    bool bFoundShort = false;
    for (int32 SectionIndex = 0;
         SectionIndex < LOD.RenderSections.Num();
         ++SectionIndex)
    {
        const FSkelMeshRenderSection& Section =
            LOD.RenderSections[SectionIndex];
        FCaptureRecord::FOutfitSkinSectionEvidence& Evidence =
            OutEvidence.Sections[SectionIndex];
        Evidence.SectionIndex = SectionIndex;
        Evidence.AuthoredMaterialIndex = Section.MaterialIndex;
        Evidence.BaseVertexIndex = static_cast<int32>(
            Section.BaseVertexIndex);
        Evidence.VertexCount = static_cast<int32>(Section.NumVertices);
        Evidence.TriangleCount = static_cast<int32>(
            Section.NumTriangles);
        Evidence.bHasClothingData = Section.HasClothingData();
        OutEvidence.bHasSectionClothingData =
            OutEvidence.bHasSectionClothingData
            || Evidence.bHasClothingData;
        if (Section.BaseVertexIndex > LOD.GetNumVertices()
            || Section.NumVertices
                > LOD.GetNumVertices() - Section.BaseVertexIndex
            || Section.NumVertices > static_cast<uint32>(MAX_int32)
            || Section.NumTriangles == 0
            || Section.NumTriangles > static_cast<uint32>(MAX_int32)
            || Section.MaxBoneInfluences <= 0
            || static_cast<uint32>(Section.MaxBoneInfluences)
                > MaximumInfluences
            || Section.BoneMap.IsEmpty())
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit section %d had invalid ranges/topology"),
                SectionIndex));
        }

        int32 MaterialIndex = Section.MaterialIndex;
        const FSkeletalMeshLODInfo* const LODInfo =
            OutfitAsset->GetLODInfo(OutEvidence.ActualRenderedLOD);
        if (!LODInfo)
        {
            return FailStructural(
                TEXT("Outfit actual LOD lacked its material mapping info"));
        }
        if (LODInfo->LODMaterialMap.IsValidIndex(SectionIndex)
            && LODInfo->LODMaterialMap[SectionIndex] != INDEX_NONE)
        {
            MaterialIndex = LODInfo->LODMaterialMap[SectionIndex];
        }
        if (!OutfitAsset->GetMaterials().IsValidIndex(MaterialIndex))
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit section %d resolved invalid material %d"),
                SectionIndex,
                MaterialIndex));
        }
        Evidence.ResolvedMaterialIndex = MaterialIndex;
        Evidence.MaterialSlot = OutfitAsset->GetMaterials()[MaterialIndex]
            .MaterialSlotName.ToString();
        UMaterialInterface* const Material = Outfit->GetMaterial(
            MaterialIndex);
        Evidence.MaterialPath = Material
            ? Material->GetPathName() : FString();
        Evidence.bEnabled = !Section.bDisabled
            && Outfit->IsMaterialSectionShown(
                MaterialIndex, OutEvidence.ActualRenderedLOD);
        if (!Evidence.bEnabled || !Material)
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit section %d was disabled, hidden, or materialless"),
                SectionIndex));
        }
        Evidence.bMaterialUsesWorldPositionOffset =
            Material->IsUsingWorldPositionOffset_Concurrent(
                GMaxRHIShaderPlatform);
        OutEvidence.bHasWorldPositionOffsetMaterial =
            OutEvidence.bHasWorldPositionOffsetMaterial
            || Evidence.bMaterialUsesWorldPositionOffset;
        const bool bShirt =
            Evidence.MaterialSlot == ExpectedShirtMaterialSlot
            && Evidence.MaterialPath == ExpectedShirtMaterialPath;
        const bool bShort =
            Evidence.MaterialSlot == ExpectedShortMaterialSlot
            && Evidence.MaterialPath == ExpectedShortMaterialPath;
        if ((!bShirt && !bShort)
            || (bShirt && bFoundShirt)
            || (bShort && bFoundShort))
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit section %d had unknown or duplicate garment semantics: slot=%s material=%s"),
                SectionIndex,
                *Evidence.MaterialSlot,
                *Evidence.MaterialPath));
        }
        bFoundShirt = bFoundShirt || bShirt;
        bFoundShort = bFoundShort || bShort;

        for (const FBoneIndexType OutfitBoneIndex : Section.BoneMap)
        {
            const int32 BoneIndex = static_cast<int32>(OutfitBoneIndex);
            if (!OutfitSkeleton.IsValidIndex(BoneIndex))
            {
                ++OutEvidence.InvalidSectionBoneMapCount;
                continue;
            }
            UsedOutfitBones.Add(BoneIndex);
        }
        for (uint32 VertexOffset = 0;
             VertexOffset < Section.NumVertices;
             ++VertexOffset)
        {
            const uint32 VertexIndex =
                Section.BaseVertexIndex + VertexOffset;
            if (VertexCoverage[VertexIndex] < MAX_uint8)
            {
                ++VertexCoverage[VertexIndex];
            }
        }
    }
    if (!bFoundShirt || !bFoundShort)
    {
        return FailStructural(
            TEXT("Outfit actual LOD did not contain exactly one Shirt and one Short section"));
    }
    if (OutEvidence.InvalidSectionBoneMapCount != 0)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual LOD sections contained %d invalid BoneMap entries"),
            OutEvidence.InvalidSectionBoneMapCount));
    }
    for (const int32 OutfitBoneIndex : UsedOutfitBones)
    {
        if (!LOD.ActiveBoneIndices.Contains(
                static_cast<FBoneIndexType>(OutfitBoneIndex)))
        {
            ++OutEvidence.InactiveUsedOutfitBoneCount;
        }
    }
    if (OutEvidence.InactiveUsedOutfitBoneCount != 0)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual LOD sections used %d bones absent from ActiveBoneIndices"),
            OutEvidence.InactiveUsedOutfitBoneCount));
    }
    OutEvidence.UsedOutfitBoneCount = UsedOutfitBones.Num();
    OutEvidence.bEveryVertexCoveredExactlyOnce = true;
    for (const uint8 Coverage : VertexCoverage)
    {
        if (Coverage != 1)
        {
            OutEvidence.bEveryVertexCoveredExactlyOnce = false;
            break;
        }
    }
    if (!OutEvidence.bEveryVertexCoveredExactlyOnce)
    {
        return FailStructural(
            TEXT("Outfit actual LOD sections did not cover every vertex exactly once"));
    }

    TArray<FString> UnsupportedFixedFunctionPaths;
    if (OutEvidence.bHasActiveMorphTargets)
    {
        UnsupportedFixedFunctionPaths.Add(TEXT("ACTIVE_MORPH_TARGETS"));
    }
    if (OutEvidence.bHasActiveExternalMorphTargets)
    {
        UnsupportedFixedFunctionPaths.Add(
            TEXT("ACTIVE_EXTERNAL_MORPH_TARGETS"));
    }
    if (OutEvidence.bHasMeshDeformer)
    {
        UnsupportedFixedFunctionPaths.Add(TEXT("MESH_DEFORMER"));
    }
    if (OutEvidence.bHasClothingSimulation)
    {
        UnsupportedFixedFunctionPaths.Add(TEXT("CLOTHING_SIMULATION"));
    }
    if (OutEvidence.bHasSectionClothingData)
    {
        UnsupportedFixedFunctionPaths.Add(TEXT("SECTION_CLOTHING_DATA"));
    }
    if (OutEvidence.bHasWorldPositionOffsetMaterial)
    {
        UnsupportedFixedFunctionPaths.Add(
            TEXT("WORLD_POSITION_OFFSET_MATERIAL"));
    }
    OutEvidence.bFixedFunctionPathComplete =
        UnsupportedFixedFunctionPaths.IsEmpty();
    OutEvidence.FixedFunctionSupportReason =
        OutEvidence.bFixedFunctionPathComplete
        ? TEXT("CPU_FIXED_FUNCTION_COMPLETE")
        : FString(TEXT("CPU_FIXED_FUNCTION_UNSUPPORTED_"))
            + FString::Join(UnsupportedFixedFunctionPaths, TEXT("+"));

    TArray<FString> NonCanonicalSemanticInputs;
    if (OutEvidence.bRefPoseOverridePresent)
    {
        NonCanonicalSemanticInputs.Add(
            TEXT("NONCANONICAL_REF_POSE_OVERRIDE_PRESENT"));
    }
    if (OutEvidence.bBodyPostProcessDisabled)
    {
        NonCanonicalSemanticInputs.Add(
            TEXT("NONCANONICAL_BODY_POST_PROCESS_DISABLED"));
    }
    if (!OutEvidence.bBodyPostProcessShouldEvaluate)
    {
        NonCanonicalSemanticInputs.Add(
            TEXT("NONCANONICAL_BODY_POST_PROCESS_NOT_EVALUATING"));
    }
    if (OutEvidence.BodyPostProcessClassPath
        != ExpectedBodyPostProcessClassPath)
    {
        NonCanonicalSemanticInputs.Add(FString::Printf(
            TEXT("NONCANONICAL_BODY_POST_PROCESS_CLASS expected=%s actual=%s"),
            ExpectedBodyPostProcessClassPath,
            *OutEvidence.BodyPostProcessClassPath));
    }
    for (int32 OutfitBoneIndex = 0;
         OutfitBoneIndex < OutfitSkeleton.GetNum();
         ++OutfitBoneIndex)
    {
        const int32 BodyBoneIndex = LeaderBoneMap[OutfitBoneIndex];
        if (!BodySkeleton.IsValidIndex(BodyBoneIndex)
            || !BodyComponentSpace.IsValidIndex(BodyBoneIndex))
        {
            ++OutEvidence.MissingLeaderMappingCount;
            continue;
        }
        if (OutfitSkeleton.GetBoneName(OutfitBoneIndex)
            != BodySkeleton.GetBoneName(BodyBoneIndex))
        {
            ++OutEvidence.LeaderNameMismatchCount;
        }
    }
    if (OutEvidence.MissingLeaderMappingCount != 0
        || OutEvidence.LeaderNameMismatchCount != 0)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit runtime LeaderMap was incomplete or name-incompatible: missing=%d name_mismatch=%d"),
            OutEvidence.MissingLeaderMappingCount,
            OutEvidence.LeaderNameMismatchCount));
    }

    const TArray<uint8>& BodyBoneVisibility =
        Body->GetBoneVisibilityStates();
    const TBitArray<>& BodyValidMeshPose =
        Body->GetValidMeshPoseTransforms();
    OutEvidence.bLeaderSafePoseValidationEnabled =
        UE::Anim::bUseSafeMeshPoseIndices;
    OutEvidence.bLeaderValidMeshPoseArrayPresent =
        !BodyValidMeshPose.IsEmpty();
    const bool bLeaderValidMeshPoseArraySized =
        !OutEvidence.bLeaderValidMeshPoseArrayPresent
        || BodyValidMeshPose.Num() == BodyComponentSpace.Num();
    if (BodyBoneVisibility.Num() != BodyComponentSpace.Num()
        || (OutEvidence.bLeaderSafePoseValidationEnabled
            && !bLeaderValidMeshPoseArraySized))
    {
        return FailStructural(FString::Printf(
            TEXT("Body live pose validity arrays were incomplete: visibility=%d valid_pose=%d transforms=%d"),
            BodyBoneVisibility.Num(),
            BodyValidMeshPose.Num(),
            BodyComponentSpace.Num()));
    }
    for (const int32 OutfitBoneIndex : UsedOutfitBones)
    {
        const int32 BodyBoneIndex = LeaderBoneMap[OutfitBoneIndex];
        if (BodyBoneVisibility[BodyBoneIndex] != BVS_Visible)
        {
            ++OutEvidence.HiddenLeaderBoneCount;
        }
        if (OutEvidence.bLeaderValidMeshPoseArrayPresent
            && bLeaderValidMeshPoseArraySized
            && !BodyValidMeshPose[BodyBoneIndex])
        {
            ++OutEvidence.InvalidLeaderPoseBoneCount;
        }
    }
    constexpr uint64 MaximumRawSkinWeight = 65535;
    for (int32 SectionIndex = 0;
         SectionIndex < LOD.RenderSections.Num();
         ++SectionIndex)
    {
        const FSkelMeshRenderSection& Section =
            LOD.RenderSections[SectionIndex];
        for (uint32 VertexOffset = 0;
             VertexOffset < Section.NumVertices;
             ++VertexOffset)
        {
            const uint32 VertexIndex =
                Section.BaseVertexIndex + VertexOffset;
            uint32 VertexWeightOffset = 0;
            uint32 VertexInfluenceCount = 0;
            SkinWeights->GetVertexInfluenceOffsetCount(
                VertexIndex,
                VertexWeightOffset,
                VertexInfluenceCount);
            const uint64 WeightDataSize =
                SkinWeightData->GetVertexDataSize();
            const uint64 VertexWeightSpan =
                static_cast<uint64>(VertexInfluenceCount)
                * SkinWeights->GetBoneIndexAndWeightByteSize();
            if (VertexInfluenceCount == 0
                || VertexInfluenceCount > MaximumInfluences
                || static_cast<uint64>(VertexWeightOffset)
                    > WeightDataSize
                || VertexWeightSpan > WeightDataSize
                    - static_cast<uint64>(VertexWeightOffset))
            {
                ++OutEvidence.InvalidInfluenceCount;
                continue;
            }

            uint64 RawWeightSum = 0;
            for (uint32 InfluenceIndex = 0;
                 InfluenceIndex < VertexInfluenceCount;
                 ++InfluenceIndex)
            {
                ++OutEvidence.CheckedInfluenceCount;
                const uint32 BoneMapIndex = SkinWeights->GetBoneIndex(
                    VertexIndex, InfluenceIndex);
                const uint16 RawWeight = SkinWeights->GetBoneWeight(
                    VertexIndex, InfluenceIndex);
                RawWeightSum += RawWeight;
                if (!Section.BoneMap.IsValidIndex(BoneMapIndex))
                {
                    ++OutEvidence.InvalidInfluenceCount;
                    continue;
                }
                const int32 OutfitBoneIndex =
                    Section.BoneMap[BoneMapIndex];
                if (!LeaderBoneMap.IsValidIndex(OutfitBoneIndex)
                    || !BodyComponentSpace.IsValidIndex(
                        LeaderBoneMap[OutfitBoneIndex]))
                {
                    ++OutEvidence.InvalidInfluenceCount;
                }
            }
            if (RawWeightSum + 1 < MaximumRawSkinWeight
                || RawWeightSum > MaximumRawSkinWeight + 1)
            {
                ++OutEvidence.NonNormalizedWeightVertexCount;
            }
        }
    }
    if (OutEvidence.InvalidInfluenceCount != 0
        || OutEvidence.NonNormalizedWeightVertexCount != 0)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit actual LOD skin weights were invalid: influence_errors=%d non_normalized_vertices=%d"),
            OutEvidence.InvalidInfluenceCount,
            OutEvidence.NonNormalizedWeightVertexCount));
    }

    TBitArray<> bRendererPaletteBoneComputed;
    bRendererPaletteBoneComputed.Init(false, OutfitInverseBind.Num());
    TArray<FString> RendererFallbackBones;
    TArray<FString> RendererFallbackSummaries;
    const bool bCheckLeaderValidMeshPose =
        OutEvidence.bLeaderSafePoseValidationEnabled
        && OutEvidence.bLeaderValidMeshPoseArrayPresent;
    for (const FBoneIndexType ActiveBoneIndex : LOD.ActiveBoneIndices)
    {
        const int32 OutfitBoneIndex = static_cast<int32>(ActiveBoneIndex);
        if (!OutfitSkeleton.IsValidIndex(OutfitBoneIndex)
            || bRendererPaletteBoneComputed[OutfitBoneIndex])
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit actual LOD contained invalid or duplicate active bone %d"),
                OutfitBoneIndex));
        }
        const int32 ParentBoneIndex =
            OutfitSkeleton.GetParentIndex(OutfitBoneIndex);
        if (ParentBoneIndex != INDEX_NONE
            && (!OutfitSkeleton.IsValidIndex(ParentBoneIndex)
                || !bRendererPaletteBoneComputed[ParentBoneIndex]))
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit active bone %d did not follow its computed parent %d in renderer palette order"),
                OutfitBoneIndex,
                ParentBoneIndex));
        }
        bRendererPaletteBoneComputed[OutfitBoneIndex] = true;

        const int32 BodyBoneIndex = LeaderBoneMap[OutfitBoneIndex];
        if (bCheckLeaderValidMeshPose
            && !BodyValidMeshPose[BodyBoneIndex])
        {
            ++OutEvidence.RendererFallbackBoneCount;
            const FString OutfitBoneLabel = FString::Printf(
                TEXT("%s[%d]"),
                *OutfitSkeleton.GetBoneName(OutfitBoneIndex).ToString(),
                OutfitBoneIndex);
            const FString BodyBoneLabel = FString::Printf(
                TEXT("%s[%d]"),
                *BodySkeleton.GetBoneName(BodyBoneIndex).ToString(),
                BodyBoneIndex);
            RendererFallbackBones.Add(OutfitBoneLabel);
            RendererFallbackSummaries.Add(
                ParentBoneIndex == INDEX_NONE
                ? FString::Printf(
                    TEXT("%s>%s:REF_LOCAL_ROOT"),
                    *OutfitBoneLabel,
                    *BodyBoneLabel)
                : FString::Printf(
                    TEXT("%s>%s:REF_LOCAL_PARENT=%s[%d]"),
                    *OutfitBoneLabel,
                    *BodyBoneLabel,
                    *OutfitSkeleton.GetBoneName(
                        ParentBoneIndex).ToString(),
                    ParentBoneIndex));
        }
    }
    for (const int32 OutfitBoneIndex : UsedOutfitBones)
    {
        if (!bRendererPaletteBoneComputed[OutfitBoneIndex])
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit used bone %d was absent from the renderer component palette"),
                OutfitBoneIndex));
        }
    }
    OutEvidence.RendererFallbackBoneList = FString::Join(
        RendererFallbackBones, TEXT(","));
    OutEvidence.RendererFallbackSummary = FString::Join(
        RendererFallbackSummaries, TEXT(","));
    OutEvidence.RendererFallbackReason =
        OutEvidence.RendererFallbackBoneCount == 0
        ? TEXT("NONE")
        : TEXT("LEADER_VALID_MESH_POSE_FALSE_SAFE_MODE");
    if (OutEvidence.RendererFallbackBoneCount
            != RendererFallbackBones.Num()
        || OutEvidence.RendererFallbackBoneCount
            != RendererFallbackSummaries.Num())
    {
        return FailStructural(
            TEXT("Outfit renderer fallback evidence count was inconsistent"));
    }
    TArray<FMatrix44f> RefToLocal;
    Outfit->GetCurrentRefToLocalMatrices(
        RefToLocal,
        OutEvidence.ActualRenderedLOD);
    if (RefToLocal.Num() != OutfitSkeleton.GetNum())
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit renderer palette contained %d matrices for %d reference bones"),
            RefToLocal.Num(),
            OutfitSkeleton.GetNum()));
    }
    for (int32 OutfitBoneIndex = 0;
         OutfitBoneIndex < RefToLocal.Num();
         ++OutfitBoneIndex)
    {
        if (!IsFiniteMatrix(RefToLocal[OutfitBoneIndex]))
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit renderer ref-to-local matrix %d was non-finite"),
                OutfitBoneIndex));
        }
    }
    const FString SemanticInputReason =
        NonCanonicalSemanticInputs.IsEmpty()
        ? TEXT("CANONICAL_RUNTIME_INPUTS")
        : FString::Join(NonCanonicalSemanticInputs, TEXT("+"));

    const FTransform OutfitToWorld = Outfit->GetComponentTransform();
    const FTransform BodyToWorld = Body->GetComponentTransform();
    const FVector OutfitScale = OutfitToWorld.GetScale3D();
    const FVector BodyScale = BodyToWorld.GetScale3D();
    if (OutfitToWorld.ContainsNaN() || BodyToWorld.ContainsNaN()
        || OutfitScale.GetAbsMin() <= SMALL_NUMBER
        || BodyScale.GetAbsMin() <= SMALL_NUMBER)
    {
        return FailStructural(
            TEXT("Body or Outfit component transform was non-finite or singular"));
    }

    auto ResolveBodyLandmarkPosition = [
        &BodyComponentSpace,
        &BodySkeleton,
        &BodyValidMeshPose,
        bCheckLeaderValidMeshPose](
            const FName BoneName,
            FVector& OutBodyPosition,
            FString& OutFailure)
    {
        const int32 BodyBoneIndex = BodySkeleton.FindBoneIndex(BoneName);
        if (!BodySkeleton.IsValidIndex(BodyBoneIndex)
            || !BodyComponentSpace.IsValidIndex(BodyBoneIndex))
        {
            OutFailure = FString::Printf(
                TEXT("Body live pose lacked landmark bone %s[%d]"),
                *BoneName.ToString(),
                BodyBoneIndex);
            return false;
        }
        if (bCheckLeaderValidMeshPose
            && !BodyValidMeshPose[BodyBoneIndex])
        {
            OutFailure = FString::Printf(
                TEXT("Body safe live pose marked landmark bone %s[%d] invalid"),
                *BoneName.ToString(),
                BodyBoneIndex);
            return false;
        }
        const FTransform& BodyBoneComponent =
            BodyComponentSpace[BodyBoneIndex];
        const FVector Translation = BodyBoneComponent.GetTranslation();
        const FVector Scale = BodyBoneComponent.GetScale3D();
        const FQuat Rotation = BodyBoneComponent.GetRotation();
        const bool bFiniteTransform =
            FMath::IsFinite(Translation.X)
            && FMath::IsFinite(Translation.Y)
            && FMath::IsFinite(Translation.Z)
            && FMath::IsFinite(Scale.X)
            && FMath::IsFinite(Scale.Y)
            && FMath::IsFinite(Scale.Z)
            && FMath::IsFinite(Rotation.X)
            && FMath::IsFinite(Rotation.Y)
            && FMath::IsFinite(Rotation.Z)
            && FMath::IsFinite(Rotation.W);
        if (!bFiniteTransform || !Rotation.IsNormalized())
        {
            OutFailure = FString::Printf(
                TEXT("Body live pose landmark transform %s[%d] was non-finite or non-normalized"),
                *BoneName.ToString(),
                BodyBoneIndex);
            return false;
        }
        OutBodyPosition = Translation;
        return true;
    };
    FVector CalfLBodyPosition;
    FVector CalfRBodyPosition;
    FVector FootLBodyPosition;
    FVector FootRBodyPosition;
    FString LandmarkFailure;
    if (!ResolveBodyLandmarkPosition(
            FName(TEXT("calf_l")), CalfLBodyPosition, LandmarkFailure)
        || !ResolveBodyLandmarkPosition(
            FName(TEXT("calf_r")), CalfRBodyPosition, LandmarkFailure)
        || !ResolveBodyLandmarkPosition(
            FName(TEXT("foot_l")), FootLBodyPosition, LandmarkFailure)
        || !ResolveBodyLandmarkPosition(
            FName(TEXT("foot_r")), FootRBodyPosition, LandmarkFailure))
    {
        return FailStructural(LandmarkFailure);
    }
    OutEvidence.KneePlaneBodyZCm = 0.5f * (
        CalfLBodyPosition.Z + CalfRBodyPosition.Z);
    OutEvidence.FootPlaneBodyZCm = 0.5f * (
        FootLBodyPosition.Z + FootRBodyPosition.Z);
    OutEvidence.KneeToFootSpanCm =
        OutEvidence.KneePlaneBodyZCm
        - OutEvidence.FootPlaneBodyZCm;
    if (!FMath::IsFinite(OutEvidence.KneePlaneBodyZCm)
        || !FMath::IsFinite(OutEvidence.FootPlaneBodyZCm)
        || !FMath::IsFinite(OutEvidence.KneeToFootSpanCm)
        || OutEvidence.KneeToFootSpanCm < 10.0f
        || OutEvidence.KneeToFootSpanCm > 100.0f)
    {
        return FailStructural(FString::Printf(
            TEXT("Body live-pose calf/foot planes were implausible: knee=%.3f foot=%.3f span=%.3f"),
            OutEvidence.KneePlaneBodyZCm,
            OutEvidence.FootPlaneBodyZCm,
            OutEvidence.KneeToFootSpanCm));
    }

    TArray<FVector3f> SkinnedPositions;
    USkinnedMeshComponent::ComputeSkinnedPositions(
        Outfit,
        SkinnedPositions,
        RefToLocal,
        LOD,
        *SkinWeights);
    if (SkinnedPositions.Num() != OutEvidence.VertexCount)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit CPU skin result count %d did not match %d vertices"),
            SkinnedPositions.Num(),
            OutEvidence.VertexCount));
    }

    auto DescribeInfluences = [
        &BodySkeleton,
        &LeaderBoneMap,
        &OutfitSkeleton,
        SkinWeights](
            const FSkelMeshRenderSection& Section,
            int32 VertexIndex)
    {
        TArray<FString> Parts;
        uint32 IgnoredWeightOffset = 0;
        uint32 InfluenceLimit = 0;
        SkinWeights->GetVertexInfluenceOffsetCount(
            VertexIndex,
            IgnoredWeightOffset,
            InfluenceLimit);
        for (uint32 InfluenceIndex = 0;
             InfluenceIndex < InfluenceLimit;
             ++InfluenceIndex)
        {
            const uint16 RawWeight = SkinWeights->GetBoneWeight(
                VertexIndex, InfluenceIndex);
            if (RawWeight == 0)
            {
                continue;
            }
            const uint32 BoneMapIndex = SkinWeights->GetBoneIndex(
                VertexIndex, InfluenceIndex);
            if (!Section.BoneMap.IsValidIndex(BoneMapIndex))
            {
                Parts.Add(FString::Printf(
                    TEXT("invalid_slot_%u:%u"),
                    BoneMapIndex,
                    RawWeight));
                continue;
            }
            const int32 OutfitBoneIndex = Section.BoneMap[BoneMapIndex];
            const int32 BodyBoneIndex = LeaderBoneMap.IsValidIndex(
                    OutfitBoneIndex)
                ? LeaderBoneMap[OutfitBoneIndex] : INDEX_NONE;
            Parts.Add(FString::Printf(
                TEXT("%s[%d]>%s[%d]:%.6f"),
                OutfitSkeleton.IsValidIndex(OutfitBoneIndex)
                    ? *OutfitSkeleton.GetBoneName(
                        OutfitBoneIndex).ToString()
                    : TEXT("<invalid>"),
                OutfitBoneIndex,
                BodySkeleton.IsValidIndex(BodyBoneIndex)
                    ? *BodySkeleton.GetBoneName(
                        BodyBoneIndex).ToString()
                    : TEXT("<invalid>"),
                BodyBoneIndex,
                static_cast<float>(RawWeight) / 65535.0f));
        }
        return FString::Join(Parts, TEXT(","));
    };

    TArray<FString> SemanticFailures;
    for (int32 SectionIndex = 0;
         SectionIndex < LOD.RenderSections.Num();
         ++SectionIndex)
    {
        const FSkelMeshRenderSection& Section =
            LOD.RenderSections[SectionIndex];
        FCaptureRecord::FOutfitSkinSectionEvidence& Evidence =
            OutEvidence.Sections[SectionIndex];
        bool bHasBounds = false;
        for (uint32 VertexOffset = 0;
             VertexOffset < Section.NumVertices;
             ++VertexOffset)
        {
            const int32 VertexIndex = static_cast<int32>(
                Section.BaseVertexIndex + VertexOffset);
            const FVector3f OutfitLocal = SkinnedPositions[VertexIndex];
            if (!FMath::IsFinite(OutfitLocal.X)
                || !FMath::IsFinite(OutfitLocal.Y)
                || !FMath::IsFinite(OutfitLocal.Z))
            {
                ++OutEvidence.NonFiniteVertexCount;
                continue;
            }
            const FVector BodyLocal = BodyToWorld.InverseTransformPosition(
                OutfitToWorld.TransformPosition(FVector(OutfitLocal)));
            if (BodyLocal.ContainsNaN()
                || !FMath::IsFinite(BodyLocal.X)
                || !FMath::IsFinite(BodyLocal.Y)
                || !FMath::IsFinite(BodyLocal.Z))
            {
                ++OutEvidence.NonFiniteVertexCount;
                continue;
            }
            if (!bHasBounds)
            {
                Evidence.BoundsMinimumBodyCm = BodyLocal;
                Evidence.BoundsMaximumBodyCm = BodyLocal;
                Evidence.WorstVertexBodyCm = BodyLocal;
                Evidence.WorstVertexIndex = VertexIndex;
                bHasBounds = true;
            }
            else
            {
                Evidence.BoundsMinimumBodyCm.X = FMath::Min(
                    Evidence.BoundsMinimumBodyCm.X, BodyLocal.X);
                Evidence.BoundsMinimumBodyCm.Y = FMath::Min(
                    Evidence.BoundsMinimumBodyCm.Y, BodyLocal.Y);
                Evidence.BoundsMinimumBodyCm.Z = FMath::Min(
                    Evidence.BoundsMinimumBodyCm.Z, BodyLocal.Z);
                Evidence.BoundsMaximumBodyCm.X = FMath::Max(
                    Evidence.BoundsMaximumBodyCm.X, BodyLocal.X);
                Evidence.BoundsMaximumBodyCm.Y = FMath::Max(
                    Evidence.BoundsMaximumBodyCm.Y, BodyLocal.Y);
                Evidence.BoundsMaximumBodyCm.Z = FMath::Max(
                    Evidence.BoundsMaximumBodyCm.Z, BodyLocal.Z);
                if (BodyLocal.Z < Evidence.WorstVertexBodyCm.Z)
                {
                    Evidence.WorstVertexBodyCm = BodyLocal;
                    Evidence.WorstVertexIndex = VertexIndex;
                }
            }
            ++Evidence.CheckedVertexCount;
            ++OutEvidence.CheckedVertexCount;
            if (BodyLocal.Z < OutEvidence.KneePlaneBodyZCm
                    - BelowKneeToleranceCm)
            {
                ++Evidence.BelowKneeVertexCount;
            }
            if (BodyLocal.Z < OutEvidence.FootPlaneBodyZCm
                    - BelowFootToleranceCm)
            {
                ++Evidence.BelowFootVertexCount;
            }
        }
        if (!bHasBounds
            || Evidence.CheckedVertexCount != Evidence.VertexCount)
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit section %d did not yield a complete finite skinned bound"),
                SectionIndex));
        }
        Evidence.HeightCm = Evidence.BoundsMaximumBodyCm.Z
            - Evidence.BoundsMinimumBodyCm.Z;
        Evidence.KneeClearanceCm = Evidence.BoundsMinimumBodyCm.Z
            - OutEvidence.KneePlaneBodyZCm;
        Evidence.FootClearanceCm = Evidence.BoundsMinimumBodyCm.Z
            - OutEvidence.FootPlaneBodyZCm;
        if (!FMath::IsFinite(Evidence.HeightCm)
            || !FMath::IsFinite(Evidence.KneeClearanceCm)
            || !FMath::IsFinite(Evidence.FootClearanceCm))
        {
            return FailStructural(FString::Printf(
                TEXT("Outfit section %d produced non-finite derived silhouette metrics"),
                SectionIndex));
        }
        Evidence.WorstVertexInfluences = DescribeInfluences(
            Section, Evidence.WorstVertexIndex);

        const bool bShirt =
            Evidence.MaterialSlot == ExpectedShirtMaterialSlot;
        const float MinimumKneeClearance = bShirt
            ? ShirtMinimumKneeClearanceCm
            : ShortMinimumKneeClearanceCm;
        const float MaximumHeight = bShirt
            ? ShirtMaximumHeightCm : ShortMaximumHeightCm;
        Evidence.bSemanticAccepted =
            OutEvidence.bFixedFunctionPathComplete
            && NonCanonicalSemanticInputs.IsEmpty()
            && Evidence.KneeClearanceCm >= MinimumKneeClearance
            && Evidence.FootClearanceCm
                >= GarmentMinimumFootClearanceCm
            && Evidence.HeightCm <= MaximumHeight
            && Evidence.BelowKneeVertexCount == 0
            && Evidence.BelowFootVertexCount == 0;
        Evidence.SemanticReason = FString::Printf(
            TEXT("%s support=%s inputs=%s knee_clearance=%.3f>=%.3f foot_clearance=%.3f>=%.3f height=%.3f<=%.3f below_knee=%d below_foot=%d worst_vertex=%d worst_influences=%s"),
            Evidence.bSemanticAccepted ? TEXT("PASS") : TEXT("REJECT"),
            *OutEvidence.FixedFunctionSupportReason,
            *SemanticInputReason,
            Evidence.KneeClearanceCm,
            MinimumKneeClearance,
            Evidence.FootClearanceCm,
            GarmentMinimumFootClearanceCm,
            Evidence.HeightCm,
            MaximumHeight,
            Evidence.BelowKneeVertexCount,
            Evidence.BelowFootVertexCount,
            Evidence.WorstVertexIndex,
            *Evidence.WorstVertexInfluences);
        if (!Evidence.bSemanticAccepted)
        {
            SemanticFailures.Add(FString::Printf(
                TEXT("section_%d_%s: %s"),
                SectionIndex,
                *Evidence.MaterialSlot,
                *Evidence.SemanticReason));
        }
    }
    if (OutEvidence.NonFiniteVertexCount != 0
        || OutEvidence.CheckedVertexCount != OutEvidence.VertexCount)
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit live skin result was incomplete: checked=%d/%d non_finite=%d"),
            OutEvidence.CheckedVertexCount,
            OutEvidence.VertexCount,
            OutEvidence.NonFiniteVertexCount));
    }

    OutEvidence.BodyBoneRevisionAfter =
        Body->GetBoneTransformRevisionNumber();
    const int32 ActualRenderedLODAfter = MeshObject->GetLOD();
    if (OutEvidence.BodyBoneRevisionAfter
            != OutEvidence.BodyBoneRevisionBefore
        || ActualRenderedLODAfter != OutEvidence.ActualRenderedLOD
        || Outfit->GetMeshObject() != MeshObject
        || !MeshObject->HaveValidDynamicData()
        || RenderData->CurrentFirstLODIdx != OutEvidence.CurrentFirstLOD
        || RenderData->PendingFirstLODIdx != OutEvidence.PendingFirstLOD
        || !LOD.IsDataReady()
        || Outfit->GetSkinWeightBuffer(
            OutEvidence.ActualRenderedLOD) != SkinWeights
        || Outfit->IsSkinWeightProfilePending()
            != OutEvidence.bSkinWeightProfilePending
        || Outfit->IsUsingSkinWeightProfile()
            != OutEvidence.bUsingSkinWeightProfile
        || UE::Anim::bUseSafeMeshPoseIndices
            != OutEvidence.bLeaderSafePoseValidationEnabled
        || (!Body->GetValidMeshPoseTransforms().IsEmpty())
            != OutEvidence.bLeaderValidMeshPoseArrayPresent
        || Body->IsRunningParallelEvaluation()
        || Outfit->IsRunningParallelEvaluation())
    {
        return FailStructural(FString::Printf(
            TEXT("Outfit live skin snapshot changed during sampling: body_revision=%u/%u lod=%d/%d"),
            OutEvidence.BodyBoneRevisionBefore,
            OutEvidence.BodyBoneRevisionAfter,
            OutEvidence.ActualRenderedLOD,
            ActualRenderedLODAfter),
            true);
    }

    OutEvidence.bStructurallyValid = true;
    OutEvidence.StructuralReason = OutEvidence.bFixedFunctionPathComplete
        ? TEXT("PASS")
        : TEXT("PASS_DIAGNOSTIC_CPU_FIXED_FUNCTION_BASE_ONLY");
    OutEvidence.bSemanticAccepted =
        OutEvidence.bFixedFunctionPathComplete
        && NonCanonicalSemanticInputs.IsEmpty()
        && SemanticFailures.IsEmpty();
    OutEvidence.SemanticReason = OutEvidence.bSemanticAccepted
        ? TEXT("PASS")
        : OutEvidence.bFixedFunctionPathComplete
            ? FString::Join(SemanticFailures, TEXT(" | "))
            : OutEvidence.FixedFunctionSupportReason
                + (SemanticFailures.IsEmpty()
                    ? FString()
                    : FString(TEXT(" | "))
                        + FString::Join(SemanticFailures, TEXT(" | ")));
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::
    PopulateMetaHumanRenderEvidence(
        FCaptureRecord& OutCapture,
        FString& OutError) const
{
    if (!PlayerController || !PlayerController->PlayerCameraManager
        || !Golfer || !AvatarBackend)
    {
        OutError = TEXT(
            "MetaHuman render evidence lost its camera or presentation authority");
        return false;
    }

    USkeletalMeshComponent* const AnimationSource =
        Golfer->GetSkeletalGolferMesh();
    USkeletalMeshComponent* const Body =
        AvatarBackend->GetVerifiedVisualBody();
    USkeletalMeshComponent* const Head =
        AvatarBackend->GetVerifiedVisualHead();
    USkeletalMeshComponent* const Outfit =
        AvatarBackend->GetVerifiedVisualOutfit();
    AActor* const VisualActor = AvatarBackend->GetActiveVisualActor();
    USceneComponent* const VisualRoot = IsValid(VisualActor)
        ? VisualActor->GetRootComponent() : nullptr;
    if (!IsValid(AnimationSource) || !IsValid(Body) || !IsValid(Head)
        || !IsValid(Outfit)
        || !IsValid(VisualActor) || VisualActor->IsHidden()
        || !IsValid(VisualRoot)
        || AvatarBackend->GetActiveAnimationSourceMesh() != AnimationSource
        || !VisualRoot->IsAttachedTo(AnimationSource)
        || Body->GetOwner() != VisualActor
        || Head->GetOwner() != VisualActor
        || Outfit->GetOwner() != VisualActor)
    {
        OutError = TEXT(
            "MetaHuman live render evidence lacked its verified actor/root/source/body/head/outfit tuple");
        return false;
    }

    OutCapture.bMetaHumanVisualRootUsesAbsoluteScale =
        VisualRoot->IsUsingAbsoluteScale();
    OutCapture.MetaHumanVisualRootWorldScale =
        VisualRoot->GetComponentScale();
    OutCapture.MetaHumanBodyWorldScale = Body->GetComponentScale();
    OutCapture.MetaHumanHeadWorldScale = Head->GetComponentScale();
    OutCapture.MetaHumanOutfitWorldScale = Outfit->GetComponentScale();
    const auto IsUnitWorldScale = [](const FVector& Scale)
    {
        return !Scale.ContainsNaN()
            && Scale.Equals(FVector::OneVector, 0.001f);
    };
    if (!OutCapture.bMetaHumanVisualRootUsesAbsoluteScale
        || !IsUnitWorldScale(OutCapture.MetaHumanVisualRootWorldScale)
        || !IsUnitWorldScale(OutCapture.MetaHumanBodyWorldScale)
        || !IsUnitWorldScale(OutCapture.MetaHumanHeadWorldScale)
        || !IsUnitWorldScale(OutCapture.MetaHumanOutfitWorldScale))
    {
        OutError = FString::Printf(
            TEXT("Fixed MetaHuman unit-world-scale evidence failed: root_absolute=%d root=%s body=%s head=%s outfit=%s"),
            OutCapture.bMetaHumanVisualRootUsesAbsoluteScale ? 1 : 0,
            *OutCapture.MetaHumanVisualRootWorldScale.ToCompactString(),
            *OutCapture.MetaHumanBodyWorldScale.ToCompactString(),
            *OutCapture.MetaHumanHeadWorldScale.ToCompactString(),
            *OutCapture.MetaHumanOutfitWorldScale.ToCompactString());
        return false;
    }

    TInlineComponentArray<ULODSyncComponent*> LODSyncComponents;
    VisualActor->GetComponents(LODSyncComponents);
    ULODSyncComponent* const LODSync = LODSyncComponents.Num() == 1
        && IsValid(LODSyncComponents[0])
        ? LODSyncComponents[0] : nullptr;
    if (!IsValid(LODSync))
    {
        OutError = TEXT(
            "MetaHuman live render evidence lacked exactly one LOD sync component");
        return false;
    }

    auto PopulateMesh = [this, AnimationSource, Body](
        USkeletalMeshComponent* Mesh,
        float MinimumRadius,
        float MaximumRadius,
        bool bOutfitEvidence,
        FCaptureRecord::FMeshRenderEvidence& Evidence,
        FString& Failure) -> bool
    {
        Evidence.ComponentPath = Mesh->GetPathName();
        USkeletalMesh* const Asset = Mesh->GetSkeletalMeshAsset();
        Evidence.SkeletalMeshPath = Asset ? Asset->GetPathName() : FString();
        Evidence.bRegistered = Mesh->IsRegistered();
        Evidence.bVisible = Mesh->IsVisible();
        Evidence.bHiddenInGame = Mesh->bHiddenInGame;
        Evidence.bShouldRender = Mesh->ShouldRender();
        Evidence.bRenderStateCreated = Mesh->IsRenderStateCreated();
        Evidence.bOwnerNoSee = Mesh->bOwnerNoSee;
        Evidence.bOnlyOwnerSee = Mesh->bOnlyOwnerSee;
        Evidence.bRenderInMainPass = Mesh->bRenderInMainPass;
        Evidence.bVisibleInSceneCaptureOnly =
            Mesh->bVisibleInSceneCaptureOnly;
        Evidence.LastRenderTime = Mesh->GetLastRenderTime();
        Evidence.LastRenderTimeOnScreen = Mesh->GetLastRenderTimeOnScreen();
        Evidence.bRecentlyRendered = Evidence.LastRenderTimeOnScreen > 0.0f
            && Mesh->GetWorld()
            && Mesh->GetWorld()->TimeSince(
                Evidence.LastRenderTimeOnScreen) <= 2.0f;
        Evidence.LODCount = Asset ? Asset->GetLODNum() : 0;
        Evidence.MaterialCount = Mesh->GetNumMaterials();
        Evidence.ReferenceBoneCount = Mesh->GetNumBones();
        Evidence.ComponentSpaceTransformCount =
            Mesh->GetNumComponentSpaceTransforms();
        Evidence.LeaderPoseComponentPath =
            Mesh->LeaderPoseComponent.IsValid()
            ? Mesh->LeaderPoseComponent->GetPathName() : FString();
        Evidence.AnimationInstanceClassPath = Mesh->GetAnimInstance()
            ? Mesh->GetAnimInstance()->GetClass()->GetPathName()
            : FString();
        Evidence.PostProcessClassPath = Mesh->GetPostProcessInstance()
            ? Mesh->GetPostProcessInstance()->GetClass()->GetPathName()
            : FString();
        Evidence.BoundsOrigin = Mesh->Bounds.Origin;
        Evidence.BoundsExtent = Mesh->Bounds.BoxExtent;
        Evidence.BoundsRadiusCm = Mesh->Bounds.SphereRadius;
        Evidence.DistanceToSourceCm = FVector::Distance(
            Mesh->Bounds.Origin, AnimationSource->Bounds.Origin);
        Evidence.bBoundsFinite = !AnimationSource->Bounds.ContainsNaN()
            && !Mesh->Bounds.ContainsNaN()
            && FMath::IsFinite(Evidence.BoundsRadiusCm)
            && Evidence.BoundsRadiusCm >= MinimumRadius
            && Evidence.BoundsRadiusCm <= MaximumRadius
            && FMath::IsFinite(Evidence.BoundsExtent.X)
            && FMath::IsFinite(Evidence.BoundsExtent.Y)
            && FMath::IsFinite(Evidence.BoundsExtent.Z)
            && Evidence.BoundsExtent.GetMin() > 0.1f
            && Evidence.DistanceToSourceCm <= 1000.0f;

        const USkeletalMeshComponent* CopyPoseSource = nullptr;
        const USceneComponent* AttachedParent = Mesh->GetAttachParent();
        while (AttachedParent && !CopyPoseSource)
        {
            CopyPoseSource = Cast<USkeletalMeshComponent>(AttachedParent);
            AttachedParent = AttachedParent->GetAttachParent();
        }
        const bool bLeaderPoseBranch = bOutfitEvidence
            && Mesh->LeaderPoseComponent.IsValid()
            && Mesh->LeaderPoseComponent.Get() == Body
            && Evidence.AnimationInstanceClassPath.IsEmpty()
            && Evidence.PostProcessClassPath.IsEmpty();
        const bool bPostProcessBranch = bOutfitEvidence
            && !Mesh->LeaderPoseComponent.IsValid()
            && Evidence.AnimationInstanceClassPath.IsEmpty()
            && Evidence.PostProcessClassPath
                == ExpectedClothingPostProcessClassPath
            && CopyPoseSource == Body;
        Evidence.bSupportedPoseSource = !bOutfitEvidence
            || bLeaderPoseBranch || bPostProcessBranch;
        Evidence.PoseSyncMode = !bOutfitEvidence ? TEXT("DIRECT")
            : bLeaderPoseBranch ? TEXT("LEADER_POSE_BODY")
            : bPostProcessBranch
                ? TEXT("POST_PROCESS_COPY_POSE_FROM_ATTACHED_BODY")
                : TEXT("UNSUPPORTED");
        Evidence.PoseSourceComponentPath = !bOutfitEvidence
            ? Mesh->GetPathName()
            : bLeaderPoseBranch ? Body->GetPathName()
            : bPostProcessBranch && CopyPoseSource
                ? CopyPoseSource->GetPathName() : FString();
        Evidence.bPoseFinite = Evidence.ReferenceBoneCount > 0;
        const USkeletalMeshComponent* const PoseMesh = bLeaderPoseBranch
            ? Body : Mesh;
        if (Evidence.bPoseFinite)
        {
            Evidence.bPoseFinite = PoseMesh->GetNumBones() > 0
                && PoseMesh->GetNumComponentSpaceTransforms()
                    == PoseMesh->GetNumBones();
        }
        if (Evidence.bPoseFinite)
        {
            for (const FTransform& BoneTransform :
                PoseMesh->GetComponentSpaceTransforms())
            {
                const FVector Scale = BoneTransform.GetScale3D();
                if (BoneTransform.ContainsNaN()
                    || !FMath::IsFinite(Scale.X)
                    || !FMath::IsFinite(Scale.Y)
                    || !FMath::IsFinite(Scale.Z))
                {
                    Evidence.bPoseFinite = false;
                    break;
                }
            }
        }

        Evidence.bSharedAnchorsConverged = !bOutfitEvidence;
        if (bOutfitEvidence && Evidence.bSupportedPoseSource)
        {
            static const TArray<FName> SharedAnchors = {
                FName(TEXT("root")),
                FName(TEXT("pelvis")),
                FName(TEXT("spine_01")),
                FName(TEXT("hand_l")),
                FName(TEXT("hand_r")),
            };
            Evidence.MaximumSharedAnchorDistanceCm = 0.0f;
            bool bAnchorsFinite = true;
            for (const FName Anchor : SharedAnchors)
            {
                if (Mesh->GetBoneIndex(Anchor) == INDEX_NONE
                    || Body->GetBoneIndex(Anchor) == INDEX_NONE)
                {
                    bAnchorsFinite = false;
                    break;
                }
                const FVector OutfitLocation = Mesh->GetBoneLocation(Anchor);
                const FVector BodyLocation = Body->GetBoneLocation(Anchor);
                const float DistanceCm = FVector::Distance(
                    OutfitLocation, BodyLocation);
                if (OutfitLocation.ContainsNaN()
                    || BodyLocation.ContainsNaN()
                    || !FMath::IsFinite(DistanceCm))
                {
                    bAnchorsFinite = false;
                    break;
                }
                Evidence.MaximumSharedAnchorDistanceCm = FMath::Max(
                    Evidence.MaximumSharedAnchorDistanceCm,
                    DistanceCm);
                ++Evidence.SharedAnchorCount;
            }
            Evidence.bSharedAnchorsConverged = bAnchorsFinite
                && Evidence.SharedAnchorCount == SharedAnchors.Num()
                && Evidence.MaximumSharedAnchorDistanceCm <= 5.0f;
        }

        TArray<FVector, TInlineAllocator<8>> ProjectionPoints;
        for (const float XSign : {-1.0f, 1.0f})
        {
            for (const float YSign : {-1.0f, 1.0f})
            {
                for (const float ZSign : {-1.0f, 1.0f})
                {
                    ProjectionPoints.Add(Evidence.BoundsOrigin + FVector(
                        XSign * Evidence.BoundsExtent.X,
                        YSign * Evidence.BoundsExtent.Y,
                        ZSign * Evidence.BoundsExtent.Z));
                }
            }
        }
        Evidence.ProjectedMin = FVector2D(
            TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
        Evidence.ProjectedMax = FVector2D(
            -TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
        bool bAllProjected = true;
        for (const FVector& Point : ProjectionPoints)
        {
            FVector2D Screen;
            if (!PlayerController->ProjectWorldLocationToScreen(
                    Point, Screen, true)
                || Screen.ContainsNaN())
            {
                bAllProjected = false;
                break;
            }
            Evidence.ProjectedMin.X = FMath::Min(
                Evidence.ProjectedMin.X, Screen.X);
            Evidence.ProjectedMin.Y = FMath::Min(
                Evidence.ProjectedMin.Y, Screen.Y);
            Evidence.ProjectedMax.X = FMath::Max(
                Evidence.ProjectedMax.X, Screen.X);
            Evidence.ProjectedMax.Y = FMath::Max(
                Evidence.ProjectedMax.Y, Screen.Y);
        }
        const FVector2D ProjectedSize =
            Evidence.ProjectedMax - Evidence.ProjectedMin;
        Evidence.bProjectedViewportOverlap = bAllProjected
            && ProjectedSize.X >= 4.0f
            && ProjectedSize.Y >= 4.0f
            && Evidence.ProjectedMax.X > 0.0f
            && Evidence.ProjectedMax.Y > 0.0f
            && Evidence.ProjectedMin.X < static_cast<float>(ExpectedWidth)
            && Evidence.ProjectedMin.Y < static_cast<float>(ExpectedHeight);
        Evidence.bProjectedViewportContained = bAllProjected
            && Evidence.ProjectedMin.X >= 32.0f
            && Evidence.ProjectedMin.Y >= 32.0f
            && Evidence.ProjectedMax.X
                <= static_cast<float>(ExpectedWidth) - 32.0f
            && Evidence.ProjectedMax.Y
                <= static_cast<float>(ExpectedHeight) - 32.0f;

        const bool bStructuralAccepted = Asset
            && Evidence.bRegistered
            && Evidence.bVisible
            && !Evidence.bHiddenInGame
            && Evidence.bShouldRender
            && !Evidence.bOwnerNoSee
            && !Evidence.bOnlyOwnerSee
            && Evidence.bRenderInMainPass
            && !Evidence.bVisibleInSceneCaptureOnly
            && Evidence.LODCount > 0
            && Evidence.MaterialCount > 0
            && Evidence.bSupportedPoseSource
            && Evidence.bSharedAnchorsConverged
            && Evidence.bBoundsFinite
            && Evidence.bPoseFinite
            && Evidence.bProjectedViewportOverlap;
        const bool bRendererReady = Evidence.bRenderStateCreated
            && Evidence.bRecentlyRendered;
        const bool bAccepted = bStructuralAccepted && bRendererReady;
        if (!bAccepted)
        {
            Failure = FString::Printf(
                TEXT("MetaHuman mesh did not reach a live renderable pose: component=%s registered=%d visible=%d hidden=%d should_render=%d render_state=%d recent=%d last_on_screen=%.3f main_pass=%d scene_capture_only=%d lods=%d materials=%d pose_bones=%d/%d pose_source=%d pose_mode=%s pose_source_component=%s leader=%s anim_instance=%s postprocess=%s shared_anchors=%d anchor_max_cm=%.3f anchors_converged=%d bounds=%d pose=%d projected=%d contained=%d projected_min=%s projected_max=%s radius=%.2f source_distance=%.2f"),
                *Evidence.ComponentPath,
                Evidence.bRegistered ? 1 : 0,
                Evidence.bVisible ? 1 : 0,
                Evidence.bHiddenInGame ? 1 : 0,
                Evidence.bShouldRender ? 1 : 0,
                Evidence.bRenderStateCreated ? 1 : 0,
                Evidence.bRecentlyRendered ? 1 : 0,
                Evidence.LastRenderTimeOnScreen,
                Evidence.bRenderInMainPass ? 1 : 0,
                Evidence.bVisibleInSceneCaptureOnly ? 1 : 0,
                Evidence.LODCount,
                Evidence.MaterialCount,
                Evidence.ComponentSpaceTransformCount,
                Evidence.ReferenceBoneCount,
                Evidence.bSupportedPoseSource ? 1 : 0,
                *Evidence.PoseSyncMode,
                *Evidence.PoseSourceComponentPath,
                *Evidence.LeaderPoseComponentPath,
                *Evidence.AnimationInstanceClassPath,
                *Evidence.PostProcessClassPath,
                Evidence.SharedAnchorCount,
                Evidence.MaximumSharedAnchorDistanceCm,
                Evidence.bSharedAnchorsConverged ? 1 : 0,
                Evidence.bBoundsFinite ? 1 : 0,
                Evidence.bPoseFinite ? 1 : 0,
                Evidence.bProjectedViewportOverlap ? 1 : 0,
                Evidence.bProjectedViewportContained ? 1 : 0,
                *Evidence.ProjectedMin.ToString(),
                *Evidence.ProjectedMax.ToString(),
                Evidence.BoundsRadiusCm,
                Evidence.DistanceToSourceCm);
            if (bStructuralAccepted && !bRendererReady)
            {
                Failure = FString(VisualRenderPendingPrefix)
                    + TEXT(" ") + Failure;
            }
        }
        return bAccepted;
    };

    auto PopulateHair = [this, AnimationSource, VisualActor, LODSync,
                         &OutCapture](
        FCaptureRecord::FPrimitiveRenderEvidence& Evidence,
        FString& Failure) -> bool
    {
        TInlineComponentArray<UGroomComponent*> Grooms;
        VisualActor->GetComponents(Grooms);
        UGroomComponent* Hair = nullptr;
        bool bDuplicate = false;
        for (UGroomComponent* Candidate : Grooms)
        {
            if (!IsValid(Candidate)
                || Candidate->GetFName() != FName(TEXT("Hair")))
            {
                continue;
            }
            if (Hair)
            {
                bDuplicate = true;
                break;
            }
            Hair = Candidate;
        }
        if (bDuplicate || !IsValid(Hair))
        {
            Failure = TEXT(
                "MetaHuman did not expose exactly one live Hair groom component");
            return false;
        }

        Evidence.ComponentPath = Hair->GetPathName();
        Evidence.ResourcePath = Hair->GroomAsset
            ? Hair->GroomAsset->GetPathName() : FString();
        Evidence.bRegistered = Hair->IsRegistered();
        Evidence.bVisible = Hair->IsVisible();
        Evidence.bHiddenInGame = Hair->bHiddenInGame;
        Evidence.bShouldRender = Hair->ShouldRender();
        Evidence.bRenderStateCreated = Hair->IsRenderStateCreated();
        Evidence.bOwnerNoSee = Hair->bOwnerNoSee;
        Evidence.bOnlyOwnerSee = Hair->bOnlyOwnerSee;
        Evidence.bRenderInMainPass = Hair->bRenderInMainPass;
        Evidence.bVisibleInSceneCaptureOnly =
            Hair->bVisibleInSceneCaptureOnly;
        Evidence.LastRenderTime = Hair->GetLastRenderTime();
        Evidence.LastRenderTimeOnScreen = Hair->GetLastRenderTimeOnScreen();
        Evidence.bRecentlyRendered = Evidence.LastRenderTimeOnScreen > 0.0f
            && Hair->GetWorld()
            && Hair->GetWorld()->TimeSince(
                Evidence.LastRenderTimeOnScreen) <= 2.0f;
        Evidence.MaterialCount = Hair->GetNumMaterials();
        Evidence.AssetGroupCount = Hair->GroomAsset
            ? Hair->GroomAsset->GetNumHairGroups() : 0;
        Evidence.ComponentGroupCount = static_cast<int32>(
            Hair->GetGroupCount());
        Evidence.LODCount = Hair->GetNumLODs();
        Evidence.DesiredSyncLOD = Hair->GetDesiredSyncLOD();
        Evidence.BestAvailableLOD = Hair->GetBestAvailableLOD();
        Evidence.ForcedLOD = Hair->GetForcedLOD();
        Evidence.bAssetValid = Hair->GroomAsset
            && Hair->GroomAsset->IsValid();
        Evidence.bAssetGroupsValid = Hair->GroomAsset
            && Hair->GroomAsset->AreGroupsValid();
        Evidence.bCompiling = Hair->IsCompiling();
        Evidence.bTickEvenWhenPaused =
            Hair->PrimaryComponentTick.bTickEvenWhenPaused;
        Evidence.bLODSyncRegistered = LODSync->IsRegistered();
        Evidence.bLODSyncTickEnabled = LODSync->IsComponentTickEnabled();
        Evidence.bLODSyncTickEvenWhenPaused =
            LODSync->PrimaryComponentTick.bTickEvenWhenPaused;
        Evidence.BoundsOrigin = Hair->Bounds.Origin;
        Evidence.BoundsExtent = Hair->Bounds.BoxExtent;
        Evidence.BoundsRadiusCm = Hair->Bounds.SphereRadius;
        Evidence.DistanceToSourceCm = FVector::Distance(
            Hair->Bounds.Origin, AnimationSource->Bounds.Origin);
        Evidence.bBoundsFinite = !AnimationSource->Bounds.ContainsNaN()
            && !Hair->Bounds.ContainsNaN()
            && FMath::IsFinite(Evidence.BoundsRadiusCm)
            && Evidence.BoundsRadiusCm >= 5.0f
            && Evidence.BoundsRadiusCm <= 250.0f
            && Evidence.BoundsExtent.GetMin() > 0.1f
            && Evidence.DistanceToSourceCm <= 1000.0f;

        TArray<FVector, TInlineAllocator<8>> ProjectionPoints;
        for (const float XSign : {-1.0f, 1.0f})
        {
            for (const float YSign : {-1.0f, 1.0f})
            {
                for (const float ZSign : {-1.0f, 1.0f})
                {
                    ProjectionPoints.Add(Evidence.BoundsOrigin + FVector(
                        XSign * Evidence.BoundsExtent.X,
                        YSign * Evidence.BoundsExtent.Y,
                        ZSign * Evidence.BoundsExtent.Z));
                }
            }
        }
        Evidence.ProjectedMin = FVector2D(
            TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
        Evidence.ProjectedMax = FVector2D(
            -TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
        bool bAllProjected = true;
        for (const FVector& Point : ProjectionPoints)
        {
            FVector2D Screen;
            if (!PlayerController->ProjectWorldLocationToScreen(
                    Point, Screen, true)
                || Screen.ContainsNaN())
            {
                bAllProjected = false;
                break;
            }
            Evidence.ProjectedMin.X = FMath::Min(
                Evidence.ProjectedMin.X, Screen.X);
            Evidence.ProjectedMin.Y = FMath::Min(
                Evidence.ProjectedMin.Y, Screen.Y);
            Evidence.ProjectedMax.X = FMath::Max(
                Evidence.ProjectedMax.X, Screen.X);
            Evidence.ProjectedMax.Y = FMath::Max(
                Evidence.ProjectedMax.Y, Screen.Y);
        }
        const FVector2D ProjectedSize =
            Evidence.ProjectedMax - Evidence.ProjectedMin;
        Evidence.bProjectedViewportOverlap = bAllProjected
            && ProjectedSize.X >= 4.0f
            && ProjectedSize.Y >= 4.0f
            && Evidence.ProjectedMax.X > 0.0f
            && Evidence.ProjectedMax.Y > 0.0f
            && Evidence.ProjectedMin.X < static_cast<float>(ExpectedWidth)
            && Evidence.ProjectedMin.Y < static_cast<float>(ExpectedHeight);
        Evidence.bProjectedViewportContained = bAllProjected
            && Evidence.ProjectedMin.X >= 32.0f
            && Evidence.ProjectedMin.Y >= 32.0f
            && Evidence.ProjectedMax.X
                <= static_cast<float>(ExpectedWidth) - 32.0f
            && Evidence.ProjectedMax.Y
                <= static_cast<float>(ExpectedHeight) - 32.0f;

        const bool bExpectedTickEvenWhenPaused =
            OutCapture.Filename.StartsWith(TEXT("02_"))
            || OutCapture.Filename.StartsWith(TEXT("03_"));
        const bool bStructuralAccepted = Evidence.ResourcePath
                == ExpectedHairGroomAssetPath
            && Evidence.bRegistered
            && Evidence.bVisible
            && !Evidence.bHiddenInGame
            && !Evidence.bOwnerNoSee
            && !Evidence.bOnlyOwnerSee
            && Evidence.bRenderInMainPass
            && !Evidence.bVisibleInSceneCaptureOnly
            && Evidence.bTickEvenWhenPaused
                == bExpectedTickEvenWhenPaused
            && Evidence.bLODSyncRegistered
            && Evidence.bLODSyncTickEnabled
            && Evidence.bLODSyncTickEvenWhenPaused
                == bExpectedTickEvenWhenPaused;
        const bool bResourceReady = Evidence.bAssetValid
            && Evidence.bAssetGroupsValid
            && !Evidence.bCompiling
            && Evidence.AssetGroupCount > 0
            && Evidence.ComponentGroupCount == Evidence.AssetGroupCount
            && Evidence.LODCount > 0
            && Evidence.BestAvailableLOD >= 0
            && Evidence.ForcedLOD >= 0
            && Evidence.ForcedLOD < Evidence.LODCount
            && Evidence.ForcedLOD >= Evidence.BestAvailableLOD
            && Evidence.MaterialCount > 0
            && Evidence.bShouldRender
            && Evidence.bBoundsFinite
            && Evidence.bProjectedViewportOverlap;
        const bool bAccepted = bStructuralAccepted
            && bResourceReady
            && Evidence.bRenderStateCreated
            && Evidence.bRecentlyRendered;
        if (!bAccepted)
        {
            Failure = FString::Printf(
                TEXT("MetaHuman hair did not reach a live main-view groom: component=%s resource=%s registered=%d visible=%d hidden=%d should_render=%d render_state=%d recent=%d last_on_screen=%.3f main_pass=%d scene_capture_only=%d materials=%d asset_valid=%d groups_valid=%d compiling=%d tick_paused=%d lodsync_registered=%d lodsync_tick=%d lodsync_paused=%d groups=%d/%d lods=%d desired_lod=%d best_lod=%d forced_lod=%d bounds=%d projected=%d contained=%d radius=%.2f source_distance=%.2f"),
                *Evidence.ComponentPath,
                *Evidence.ResourcePath,
                Evidence.bRegistered ? 1 : 0,
                Evidence.bVisible ? 1 : 0,
                Evidence.bHiddenInGame ? 1 : 0,
                Evidence.bShouldRender ? 1 : 0,
                Evidence.bRenderStateCreated ? 1 : 0,
                Evidence.bRecentlyRendered ? 1 : 0,
                Evidence.LastRenderTimeOnScreen,
                Evidence.bRenderInMainPass ? 1 : 0,
                Evidence.bVisibleInSceneCaptureOnly ? 1 : 0,
                Evidence.MaterialCount,
                Evidence.bAssetValid ? 1 : 0,
                Evidence.bAssetGroupsValid ? 1 : 0,
                Evidence.bCompiling ? 1 : 0,
                Evidence.bTickEvenWhenPaused ? 1 : 0,
                Evidence.bLODSyncRegistered ? 1 : 0,
                Evidence.bLODSyncTickEnabled ? 1 : 0,
                Evidence.bLODSyncTickEvenWhenPaused ? 1 : 0,
                Evidence.ComponentGroupCount,
                Evidence.AssetGroupCount,
                Evidence.LODCount,
                Evidence.DesiredSyncLOD,
                Evidence.BestAvailableLOD,
                Evidence.ForcedLOD,
                Evidence.bBoundsFinite ? 1 : 0,
                Evidence.bProjectedViewportOverlap ? 1 : 0,
                Evidence.bProjectedViewportContained ? 1 : 0,
                Evidence.BoundsRadiusCm,
                Evidence.DistanceToSourceCm);
            if (bStructuralAccepted)
            {
                Failure = FString(HairRenderPendingPrefix)
                    + TEXT(" ") + Failure;
            }
        }
        return bAccepted;
    };

    OutCapture.bMetaHumanRenderEvidenceCollected = true;
    FString BodyFailure;
    FString HeadFailure;
    FString OutfitFailure;
    FString HairFailure;
    const bool bBodyAccepted = PopulateMesh(
        Body, 30.0f, 500.0f, false,
        OutCapture.BodyRender, BodyFailure);
    const bool bHeadAccepted = PopulateMesh(
        Head, 5.0f, 250.0f, false,
        OutCapture.HeadRender, HeadFailure);
    OutCapture.bMetaHumanOutfitRenderEvidenceCollected = true;
    const bool bOutfitMeshAccepted = PopulateMesh(
        Outfit, 10.0f, 300.0f, true,
        OutCapture.OutfitRender, OutfitFailure);
    const bool bOutfitPathAccepted =
        OutCapture.OutfitRender.SkeletalMeshPath
        == ExpectedOutfitMeshAssetPath;
    const bool bOutfitAccepted = bOutfitMeshAccepted
        && bOutfitPathAccepted;
    if (!bOutfitPathAccepted)
    {
        OutfitFailure = FString::Printf(
            TEXT("MetaHuman outfit used an unexpected skeletal mesh: expected=%s actual=%s"),
            ExpectedOutfitMeshAssetPath,
            *OutCapture.OutfitRender.SkeletalMeshPath);
    }
    OutCapture.bMetaHumanHairRenderEvidenceCollected = true;
    const bool bHairAccepted = PopulateHair(
        OutCapture.HairRender, HairFailure);

    if (bBodyAccepted && bHeadAccepted && bOutfitAccepted
        && bHairAccepted)
    {
        OutCapture.bMetaHumanOutfitLiveSkinEvidenceCollected =
            PopulateOutfitLiveSkinEvidence(
                Body,
                Outfit,
                OutCapture.OutfitLiveSkin,
                OutError);
        if (!OutCapture.bMetaHumanOutfitLiveSkinEvidenceCollected)
        {
            return false;
        }

        UE_LOG(LogDiscGolfTour, Display,
            TEXT("%s: OUTFIT_LIVE_SKIN_DIAGNOSTIC scene=%s actual_lod=%d predicted_lod=%d desired_lod=%d vertices=%d sections=%d position_cpu=%d weights_cpu=%d leader_map=%d/%d used_bones=%d checked_vertices=%d checked_influences=%d knee_z=%.3f foot_z=%.3f fixed_function_complete=%d support_reason=%s structural=%d semantic=%d semantic_reason=%s"),
            Marker,
            *OutCapture.Scene,
            OutCapture.OutfitLiveSkin.ActualRenderedLOD,
            OutCapture.OutfitLiveSkin.PredictedLOD,
            OutCapture.OutfitLiveSkin.DesiredSyncLOD,
            OutCapture.OutfitLiveSkin.VertexCount,
            OutCapture.OutfitLiveSkin.SectionCount,
            OutCapture.OutfitLiveSkin.bPositionDataPresent ? 1 : 0,
            OutCapture.OutfitLiveSkin.bSkinWeightDataPresent ? 1 : 0,
            OutCapture.OutfitLiveSkin.LeaderBoneMapCount,
            OutCapture.OutfitLiveSkin.OutfitReferenceBoneCount,
            OutCapture.OutfitLiveSkin.UsedOutfitBoneCount,
            OutCapture.OutfitLiveSkin.CheckedVertexCount,
            OutCapture.OutfitLiveSkin.CheckedInfluenceCount,
            OutCapture.OutfitLiveSkin.KneePlaneBodyZCm,
            OutCapture.OutfitLiveSkin.FootPlaneBodyZCm,
            OutCapture.OutfitLiveSkin.bFixedFunctionPathComplete ? 1 : 0,
            *OutCapture.OutfitLiveSkin.FixedFunctionSupportReason,
            OutCapture.OutfitLiveSkin.bStructurallyValid ? 1 : 0,
            OutCapture.OutfitLiveSkin.bSemanticAccepted ? 1 : 0,
            *SafeSingleLine(OutCapture.OutfitLiveSkin.SemanticReason));
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("%s: OUTFIT_LIVE_SKIN_PATHS scene=%s morph_targets=%d morph_summary=%s external_morph_targets=%d mesh_deformer=%d mesh_deformer_path=%s clothing_simulations=%d section_cloth=%d wpo_material=%d body_postprocess_disabled=%d body_postprocess_should_evaluate=%d body_postprocess_class=%s"),
            Marker,
            *OutCapture.Scene,
            OutCapture.OutfitLiveSkin.ActiveMorphTargetCount,
            *OutCapture.OutfitLiveSkin.ActiveMorphTargetSummary,
            OutCapture.OutfitLiveSkin.ActiveExternalMorphTargetCount,
            OutCapture.OutfitLiveSkin.bHasMeshDeformer ? 1 : 0,
            *OutCapture.OutfitLiveSkin.ActiveMeshDeformerPath,
            OutCapture.OutfitLiveSkin.ClothingSimulationCount,
            OutCapture.OutfitLiveSkin.bHasSectionClothingData ? 1 : 0,
            OutCapture.OutfitLiveSkin.bHasWorldPositionOffsetMaterial ? 1 : 0,
            OutCapture.OutfitLiveSkin.bBodyPostProcessDisabled ? 1 : 0,
            OutCapture.OutfitLiveSkin.bBodyPostProcessShouldEvaluate ? 1 : 0,
            *OutCapture.OutfitLiveSkin.BodyPostProcessClassPath);
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("%s: OUTFIT_LIVE_SKIN_RENDERER_FALLBACK scene=%s safe_pose_validation=%d valid_pose_array=%d invalid_used_leader_bones=%d fallback_bones=%d fallback_reason=%s fallback_list=%s fallback_summary=%s"),
            Marker,
            *OutCapture.Scene,
            OutCapture.OutfitLiveSkin.bLeaderSafePoseValidationEnabled
                ? 1 : 0,
            OutCapture.OutfitLiveSkin.bLeaderValidMeshPoseArrayPresent
                ? 1 : 0,
            OutCapture.OutfitLiveSkin.InvalidLeaderPoseBoneCount,
            OutCapture.OutfitLiveSkin.RendererFallbackBoneCount,
            *OutCapture.OutfitLiveSkin.RendererFallbackReason,
            *SafeSingleLine(
                OutCapture.OutfitLiveSkin.RendererFallbackBoneList),
            *SafeSingleLine(
                OutCapture.OutfitLiveSkin.RendererFallbackSummary));
        for (const FCaptureRecord::FOutfitSkinSectionEvidence& Section
            : OutCapture.OutfitLiveSkin.Sections)
        {
            UE_LOG(LogDiscGolfTour, Display,
                TEXT("%s: OUTFIT_LIVE_SKIN_SECTION scene=%s lod=%d section=%d slot=%s material=%s vertices=%d bounds_min=%s bounds_max=%s height=%.3f knee_clearance=%.3f foot_clearance=%.3f below_knee=%d below_foot=%d worst_vertex=%d worst_position=%s influences=%s cloth=%d wpo=%d semantic=%d reason=%s"),
                Marker,
                *OutCapture.Scene,
                OutCapture.OutfitLiveSkin.ActualRenderedLOD,
                Section.SectionIndex,
                *Section.MaterialSlot,
                *Section.MaterialPath,
                Section.CheckedVertexCount,
                *Section.BoundsMinimumBodyCm.ToString(),
                *Section.BoundsMaximumBodyCm.ToString(),
                Section.HeightCm,
                Section.KneeClearanceCm,
                Section.FootClearanceCm,
                Section.BelowKneeVertexCount,
                Section.BelowFootVertexCount,
                Section.WorstVertexIndex,
                *Section.WorstVertexBodyCm.ToString(),
                *Section.WorstVertexInfluences,
                Section.bHasClothingData ? 1 : 0,
                Section.bMaterialUsesWorldPositionOffset ? 1 : 0,
                Section.bSemanticAccepted ? 1 : 0,
                *SafeSingleLine(Section.SemanticReason));
        }

    }

    const bool bFullBodyCreator =
        OutCapture.Filename.StartsWith(TEXT("02_"));
    const bool bHairCloseup =
        OutCapture.Filename.StartsWith(TEXT("03_"));
    if (bFullBodyCreator || bHairCloseup)
    {
        OutCapture.bCreatorFramingEvidenceCollected = true;
        constexpr float PreviewPaneMinX =
            static_cast<float>(ExpectedWidth) * 0.5f + 20.0f;
        constexpr float ViewportMaxX =
            static_cast<float>(ExpectedWidth) - 32.0f;
        constexpr float ViewportMaxY =
            static_cast<float>(ExpectedHeight) - 32.0f;
        if (bFullBodyCreator)
        {
            OutCapture.CreatorSemanticMin = FVector2D(
                TNumericLimits<float>::Max(), TNumericLimits<float>::Max());
            OutCapture.CreatorSemanticMax = FVector2D(
                -TNumericLimits<float>::Max(), -TNumericLimits<float>::Max());
            static const TArray<FName> RequiredBones = {
                FName(TEXT("Head")),
                FName(TEXT("hand_l")),
                FName(TEXT("hand_r")),
                FName(TEXT("ball_l")),
                FName(TEXT("ball_r")),
            };
            bool bAllSemanticPointsProjected = true;
            for (const FName BoneName : RequiredBones)
            {
                if (Body->GetBoneIndex(BoneName) == INDEX_NONE)
                {
                    bAllSemanticPointsProjected = false;
                    break;
                }
                FVector2D Screen;
                if (!PlayerController->ProjectWorldLocationToScreen(
                        Body->GetBoneLocation(BoneName), Screen, true)
                    || Screen.ContainsNaN())
                {
                    bAllSemanticPointsProjected = false;
                    break;
                }
                ++OutCapture.CreatorSemanticPointCount;
                OutCapture.CreatorSemanticMin.X = FMath::Min(
                    OutCapture.CreatorSemanticMin.X, Screen.X);
                OutCapture.CreatorSemanticMin.Y = FMath::Min(
                    OutCapture.CreatorSemanticMin.Y, Screen.Y);
                OutCapture.CreatorSemanticMax.X = FMath::Max(
                    OutCapture.CreatorSemanticMax.X, Screen.X);
                OutCapture.CreatorSemanticMax.Y = FMath::Max(
                    OutCapture.CreatorSemanticMax.Y, Screen.Y);
            }
            const float SemanticHeight =
                OutCapture.CreatorSemanticMax.Y
                - OutCapture.CreatorSemanticMin.Y;
            OutCapture.bCreatorFramingAccepted =
                bAllSemanticPointsProjected
                && OutCapture.CreatorSemanticPointCount
                    == RequiredBones.Num()
                && OutCapture.CreatorSemanticMin.X >= PreviewPaneMinX
                && OutCapture.CreatorSemanticMin.Y >= 32.0f
                && OutCapture.CreatorSemanticMax.X <= ViewportMaxX
                && OutCapture.CreatorSemanticMax.Y <= ViewportMaxY
                && SemanticHeight >= 300.0f
                && OutCapture.BodyRender.bProjectedViewportContained
                && OutCapture.HeadRender.bProjectedViewportContained
                && OutCapture.OutfitRender.bProjectedViewportContained
                && OutCapture.HairRender.bProjectedViewportContained;
        }
        else
        {
            const FVector2D HairSize =
                OutCapture.HairRender.ProjectedMax
                - OutCapture.HairRender.ProjectedMin;
            OutCapture.CreatorSemanticMin =
                OutCapture.HairRender.ProjectedMin;
            OutCapture.CreatorSemanticMax =
                OutCapture.HairRender.ProjectedMax;
            OutCapture.CreatorSemanticPointCount = 1;
            OutCapture.bCreatorFramingAccepted =
                OutCapture.HeadRender.bProjectedViewportContained
                && OutCapture.HairRender.bProjectedViewportContained
                && OutCapture.HairRender.ProjectedMin.X
                    >= PreviewPaneMinX
                && OutCapture.HairRender.ProjectedMin.Y >= 32.0f
                && OutCapture.HairRender.ProjectedMax.X <= ViewportMaxX
                && OutCapture.HairRender.ProjectedMax.Y <= ViewportMaxY
                && HairSize.Y >= 180.0f
                && HairSize.Y <= 500.0f;
        }
    }

    const bool bGripOrReleaseFrame =
        OutCapture.Filename.StartsWith(TEXT("07_"))
        || OutCapture.Filename.StartsWith(TEXT("08_"));
    if (bGripOrReleaseFrame)
    {
        OutCapture.bVisibleHandAlignmentEvidenceCollected = true;
        if (!bHasFrozenHandCorrectionEvidence)
        {
            OutError = TEXT(
                "MetaHuman hand frame omitted its immutable same-frame source snapshot and deterministic target correction");
            return false;
        }
        const FDiscGolfMetaHumanHandCorrectionEvidence& Correction =
            FrozenHandCorrectionEvidence;
        OutCapture.PresentationHandCorrectionMode =
            Correction.CorrectionMode;
        OutCapture.SourceHandWorldLocation =
            Correction.SourceHandWorldLocation;
        OutCapture.SourceDiscGripWorldLocation =
            Correction.SourceDiscGripWorldLocation;
        OutCapture.MetaHumanHandPreCorrectionWorldLocation =
            Correction.MetaHumanHandPreCorrectionWorldLocation;
        OutCapture.MetaHumanHandPostCorrectionWorldLocation =
            Correction.MetaHumanHandPostCorrectionWorldLocation;
        OutCapture.SourceHandToDiscGripDistanceCm =
            Correction.SourceHandToDiscGripDistanceCm;
        OutCapture.MetaHumanHandPreToSourceHandDistanceCm =
            Correction.MetaHumanHandPreToSourceHandDistanceCm;
        OutCapture.MetaHumanHandPostToSourceHandDistanceCm =
            Correction.MetaHumanHandPostToSourceHandDistanceCm;
        OutCapture.SourceBoneRevisionAtPreUpdate =
            Correction.SourceBoneRevisionAtPreUpdate;
        OutCapture.TargetBoneRevisionBeforeEvaluate =
            Correction.TargetBoneRevisionBeforeEvaluate;
        OutCapture.TargetBoneRevisionAtCapture =
            FrozenHandCorrectionTargetBoneRevision;
        OutCapture.SourceSampleFrameCounter =
            Correction.SourceSampleFrameCounter;
        OutCapture.TargetCorrectionFrameCounter =
            Correction.TargetCorrectionFrameCounter;
        OutCapture.ReleaseCallbackFrameCounter =
            ReleaseCallbackFrameCounter;
        OutCapture.CaptureFreezeFrameCounter =
            CaptureFreezeFrameCounter;
        OutCapture.ReleaseCallbackEventOrder =
            ReleaseCallbackEventOrder;
        OutCapture.CaptureFreezeEventOrder =
            CaptureFreezeEventOrder;
        OutCapture.bPresentationHandCorrectionSnapshotValid =
            Correction.bSnapshotValid;
        OutCapture.bPresentationHandCorrectionReachable =
            Correction.bReachable;
        OutCapture.bPresentationHandCorrectionApplied =
            Correction.bApplied;

        const bool bCorrectionFinite =
            !OutCapture.SourceHandWorldLocation.ContainsNaN()
            && !OutCapture.SourceDiscGripWorldLocation.ContainsNaN()
            && !OutCapture.MetaHumanHandPreCorrectionWorldLocation.ContainsNaN()
            && !OutCapture.MetaHumanHandPostCorrectionWorldLocation.ContainsNaN()
            && FMath::IsFinite(
                OutCapture.SourceHandToDiscGripDistanceCm)
            && FMath::IsFinite(
                OutCapture.MetaHumanHandPreToSourceHandDistanceCm)
            && FMath::IsFinite(
                OutCapture.MetaHumanHandPostToSourceHandDistanceCm);
        const bool bReleaseOrderingAccepted =
            OutCapture.Filename.StartsWith(TEXT("07_"))
            ? OutCapture.ReleaseCallbackFrameCounter == 0
                && OutCapture.ReleaseCallbackEventOrder == 0
            : OutCapture.ReleaseCallbackFrameCounter
                    == OutCapture.CaptureFreezeFrameCounter
                && OutCapture.ReleaseCallbackEventOrder > 0
                && OutCapture.CaptureFreezeEventOrder
                    == OutCapture.ReleaseCallbackEventOrder + 1;
        if (OutCapture.PresentationHandCorrectionMode
                != DiscGolfMetaHumanPresentation::HandCorrectionMode
            || !OutCapture.bPresentationHandCorrectionSnapshotValid
            || !OutCapture.bPresentationHandCorrectionReachable
            || !OutCapture.bPresentationHandCorrectionApplied
            || !bCorrectionFinite
            || OutCapture.SourceHandToDiscGripDistanceCm > 25.0f
            || OutCapture.MetaHumanHandPostToSourceHandDistanceCm > 0.05f
            || OutCapture.SourceSampleFrameCounter == 0
            || OutCapture.SourceSampleFrameCounter
                != OutCapture.TargetCorrectionFrameCounter
            || OutCapture.TargetCorrectionFrameCounter
                != OutCapture.CaptureFreezeFrameCounter
            || OutCapture.TargetBoneRevisionAtCapture
                == OutCapture.TargetBoneRevisionBeforeEvaluate
            || OutCapture.CaptureFreezeEventOrder == 0
            || !bReleaseOrderingAccepted)
        {
            OutError = FString::Printf(
                TEXT("MetaHuman deterministic hand correction evidence failed: mode=%s snapshot=%d reachable=%d applied=%d finite=%d source_grip_cm=%.3f pre_cm=%.3f post_cm=%.3f source_frame=%llu target_frame=%llu release_frame=%llu freeze_frame=%llu source_revision=%u target_before=%u target_capture=%u release_order=%llu freeze_order=%llu"),
                *OutCapture.PresentationHandCorrectionMode,
                OutCapture.bPresentationHandCorrectionSnapshotValid ? 1 : 0,
                OutCapture.bPresentationHandCorrectionReachable ? 1 : 0,
                OutCapture.bPresentationHandCorrectionApplied ? 1 : 0,
                bCorrectionFinite ? 1 : 0,
                OutCapture.SourceHandToDiscGripDistanceCm,
                OutCapture.MetaHumanHandPreToSourceHandDistanceCm,
                OutCapture.MetaHumanHandPostToSourceHandDistanceCm,
                OutCapture.SourceSampleFrameCounter,
                OutCapture.TargetCorrectionFrameCounter,
                OutCapture.ReleaseCallbackFrameCounter,
                OutCapture.CaptureFreezeFrameCounter,
                OutCapture.SourceBoneRevisionAtPreUpdate,
                OutCapture.TargetBoneRevisionBeforeEvaluate,
                OutCapture.TargetBoneRevisionAtCapture,
                OutCapture.ReleaseCallbackEventOrder,
                OutCapture.CaptureFreezeEventOrder);
            return false;
        }
        const FName TargetHandBone(TEXT("hand_r"));
        OutCapture.MetaHumanHandWorldLocation =
            Body->GetBoneLocation(TargetHandBone);
        if (OutCapture.Filename.StartsWith(TEXT("07_")))
        {
            UStaticMeshComponent* const HeldDisc =
                Golfer->GetHeldDiscVisual();
            OutCapture.VisibleDiscWorldLocation = IsValid(HeldDisc)
                ? HeldDisc->GetComponentLocation()
                : FVector(NAN, NAN, NAN);
        }
        else
        {
            OutCapture.VisibleDiscWorldLocation =
                OutCapture.GameplayDiscWorldLocation;
        }
        OutCapture.bVisibleHandLocationFinite =
            Body->GetBoneIndex(TargetHandBone) != INDEX_NONE
            && !OutCapture.MetaHumanHandWorldLocation.ContainsNaN();
        OutCapture.bVisibleDiscLocationFinite =
            !OutCapture.VisibleDiscWorldLocation.ContainsNaN();
        OutCapture.VisibleDiscToMetaHumanHandDistanceCm = FVector::Distance(
            OutCapture.VisibleDiscWorldLocation,
            OutCapture.MetaHumanHandWorldLocation);
        OutCapture.bVisibleHandDistanceFinite = FMath::IsFinite(
            OutCapture.VisibleDiscToMetaHumanHandDistanceCm);
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("%s: RENDER_DIAGNOSTIC scene=%s body_render=%d body_recent=%d body_bones=%d body_radius=%.2f body_projected=%d body_min=%s body_max=%s head_render=%d head_recent=%d head_bones=%d head_radius=%.2f head_projected=%d head_min=%s head_max=%s outfit_render=%d outfit_pose=%d outfit_radius=%.2f outfit_min=%s outfit_max=%s hair_render=%d hair_recent=%d hair_forced_lod=%d hair_radius=%.2f hair_projected=%d hair_min=%s hair_max=%s creator_framing=%d hand_distance=%.3f"),
        Marker,
        *OutCapture.Scene,
        OutCapture.BodyRender.bShouldRender ? 1 : 0,
        OutCapture.BodyRender.bRecentlyRendered ? 1 : 0,
        OutCapture.BodyRender.ComponentSpaceTransformCount,
        OutCapture.BodyRender.BoundsRadiusCm,
        OutCapture.BodyRender.bProjectedViewportOverlap ? 1 : 0,
        *OutCapture.BodyRender.ProjectedMin.ToString(),
        *OutCapture.BodyRender.ProjectedMax.ToString(),
        OutCapture.HeadRender.bShouldRender ? 1 : 0,
        OutCapture.HeadRender.bRecentlyRendered ? 1 : 0,
        OutCapture.HeadRender.ComponentSpaceTransformCount,
        OutCapture.HeadRender.BoundsRadiusCm,
        OutCapture.HeadRender.bProjectedViewportOverlap ? 1 : 0,
        *OutCapture.HeadRender.ProjectedMin.ToString(),
        *OutCapture.HeadRender.ProjectedMax.ToString(),
        OutCapture.OutfitRender.bShouldRender ? 1 : 0,
        OutCapture.OutfitRender.bPoseFinite ? 1 : 0,
        OutCapture.OutfitRender.BoundsRadiusCm,
        *OutCapture.OutfitRender.ProjectedMin.ToString(),
        *OutCapture.OutfitRender.ProjectedMax.ToString(),
        OutCapture.HairRender.bShouldRender ? 1 : 0,
        OutCapture.HairRender.bRecentlyRendered ? 1 : 0,
        OutCapture.HairRender.ForcedLOD,
        OutCapture.HairRender.BoundsRadiusCm,
        OutCapture.HairRender.bProjectedViewportOverlap ? 1 : 0,
        *OutCapture.HairRender.ProjectedMin.ToString(),
        *OutCapture.HairRender.ProjectedMax.ToString(),
        OutCapture.bCreatorFramingAccepted ? 1 : 0,
        OutCapture.VisibleDiscToMetaHumanHandDistanceCm);
    if (!bBodyAccepted || !bHeadAccepted || !bOutfitAccepted
        || !bHairAccepted
        || ((bFullBodyCreator || bHairCloseup)
            && !OutCapture.bCreatorFramingAccepted)
        || (bGripOrReleaseFrame
            && (!OutCapture.bVisibleHandLocationFinite
                || !OutCapture.bVisibleDiscLocationFinite
                || !OutCapture.bVisibleHandDistanceFinite
                || OutCapture.VisibleDiscToMetaHumanHandDistanceCm > 25.0f)))
    {
        const bool bBodyHardFailure = !bBodyAccepted
            && !BodyFailure.StartsWith(VisualRenderPendingPrefix);
        const bool bHeadHardFailure = !bHeadAccepted
            && !HeadFailure.StartsWith(VisualRenderPendingPrefix);
        const bool bOutfitHardFailure = !bOutfitAccepted
            && !OutfitFailure.StartsWith(VisualRenderPendingPrefix);
        const bool bHairHardFailure = !bHairAccepted
            && !HairFailure.StartsWith(HairRenderPendingPrefix);
        const bool bVisibleHandHardFailure = bGripOrReleaseFrame
            && (!OutCapture.bVisibleHandLocationFinite
                || !OutCapture.bVisibleDiscLocationFinite
                || !OutCapture.bVisibleHandDistanceFinite
                || OutCapture.VisibleDiscToMetaHumanHandDistanceCm > 25.0f);
        const bool bCreatorFramingHardFailure =
            (bFullBodyCreator || bHairCloseup)
            && !OutCapture.bCreatorFramingAccepted;
        OutError = bBodyHardFailure ? BodyFailure
            : bHeadHardFailure ? HeadFailure
            : bOutfitHardFailure ? OutfitFailure
            : bHairHardFailure ? HairFailure
            : bVisibleHandHardFailure
                ? FString::Printf(
                    TEXT("MetaHuman visible hand did not converge with the authoritative disc: hand_finite=%d disc_finite=%d distance_finite=%d distance_cm=%.3f"),
                    OutCapture.bVisibleHandLocationFinite ? 1 : 0,
                    OutCapture.bVisibleDiscLocationFinite ? 1 : 0,
                    OutCapture.bVisibleHandDistanceFinite ? 1 : 0,
                    OutCapture.VisibleDiscToMetaHumanHandDistanceCm)
            : bCreatorFramingHardFailure
                ? FString::Printf(
                    TEXT("MetaHuman creator framing failed: semantic_points=%d min=%s max=%s body_contained=%d body_min=%s body_max=%s head_contained=%d head_min=%s head_max=%s outfit_contained=%d outfit_min=%s outfit_max=%s hair_contained=%d hair_min=%s hair_max=%s"),
                    OutCapture.CreatorSemanticPointCount,
                    *OutCapture.CreatorSemanticMin.ToString(),
                    *OutCapture.CreatorSemanticMax.ToString(),
                    OutCapture.BodyRender.bProjectedViewportContained ? 1 : 0,
                    *OutCapture.BodyRender.ProjectedMin.ToString(),
                    *OutCapture.BodyRender.ProjectedMax.ToString(),
                    OutCapture.HeadRender.bProjectedViewportContained ? 1 : 0,
                    *OutCapture.HeadRender.ProjectedMin.ToString(),
                    *OutCapture.HeadRender.ProjectedMax.ToString(),
                    OutCapture.OutfitRender.bProjectedViewportContained ? 1 : 0,
                    *OutCapture.OutfitRender.ProjectedMin.ToString(),
                    *OutCapture.OutfitRender.ProjectedMax.ToString(),
                    OutCapture.HairRender.bProjectedViewportContained ? 1 : 0,
                    *OutCapture.HairRender.ProjectedMin.ToString(),
                    *OutCapture.HairRender.ProjectedMax.ToString())
            : !BodyFailure.IsEmpty() ? BodyFailure
            : !HeadFailure.IsEmpty() ? HeadFailure
            : !OutfitFailure.IsEmpty() ? OutfitFailure
            : !HairFailure.IsEmpty() ? HairFailure
            : TEXT("MetaHuman visual acceptance failed without a classified reason.");
        return false;
    }
    return true;
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::BeginPerformanceSegment(
    const TCHAR* Segment,
    FString& OutError)
{
    const EDGMetaHumanPresentationPolicy ExpectedPolicy =
        FString(Segment) == TEXT("creator_closeup")
        ? EDGMetaHumanPresentationPolicy::CharacterCreator
        : EDGMetaHumanPresentationPolicy::GameplayPerformance;
    PendingPerformancePolicyBegin = FPresentationPolicyEvidence();
    bHasPendingPerformancePolicyBegin =
        PopulatePresentationPolicyEvidence(
            ExpectedPolicy,
            PendingPerformancePolicyBegin,
            OutError);
    if (!bHasPendingPerformancePolicyBegin)
    {
        return false;
    }
    SegmentTracker.Reset();
    PerformanceRawFrameTimesMs.Reset();
    PerformanceRawFrameTimesMs.Reserve(4096);
    PerformanceMaxUsedPhysicalBytes = 0;
    PerformanceSegmentStartedSeconds = FPlatformTime::Seconds();
    LastPerformanceFrameSeconds = PerformanceSegmentStartedSeconds;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("%s: PERFORMANCE_SEGMENT_START phase=%s segment=%s duration=15.0"),
        Marker, *PhaseName, Segment);
    return true;
}

void ADiscGolfSession8BMetaHumanPackagedRunner::TickPerformanceSegment()
{
    const double Now = FPlatformTime::Seconds();
    const double IntervalSeconds = Now - LastPerformanceFrameSeconds;
    if (FMath::IsFinite(IntervalSeconds) && IntervalSeconds > 0.0)
    {
        PerformanceRawFrameTimesMs.Add(FMath::Clamp(
            static_cast<float>(IntervalSeconds * 1000.0),
            0.01f,
            1000.0f));
    }
    LastPerformanceFrameSeconds = Now;
    if (GameMode && GameMode->GetActiveDisc()
        && GameMode->GetActiveDisc()->GetFlightComponent()
        && !GameMode->GetActiveDisc()->GetFlightComponent()
            ->GetVelocityMps().IsNearlyZero())
    {
        bPerformanceSawAuthoritativeFlight = true;
    }
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::FinishPerformanceSegment(
    const TCHAR* Segment,
    FString& OutError)
{
    const EDGMetaHumanPresentationPolicy ExpectedPolicy =
        FString(Segment) == TEXT("creator_closeup")
        ? EDGMetaHumanPresentationPolicy::CharacterCreator
        : EDGMetaHumanPresentationPolicy::GameplayPerformance;
    FPerformanceRecord Record;
    Record.Segment = Segment;
    Record.DurationSeconds =
        FPlatformTime::Seconds() - PerformanceSegmentStartedSeconds;
    // GetStats is deliberately sampled only after the timed window is frozen:
    // it is slow on Windows, while the process peak conservatively covers the
    // segment without adding observer work to any recorded frame interval.
    const FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
    PerformanceMaxUsedPhysicalBytes = FMath::Max(
        MemoryStats.UsedPhysical,
        MemoryStats.PeakUsedPhysical);
    if (!bHasPendingPerformancePolicyBegin
        || !PendingPerformancePolicyBegin.bCollected
        || !PopulatePresentationPolicyEvidence(
            ExpectedPolicy,
            Record.PresentationPolicyEnd,
            OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("%s performance segment lost its outside-window policy snapshots"),
                Segment);
        }
        return false;
    }
    Record.PresentationPolicyBegin = PendingPerformancePolicyBegin;
    PendingPerformancePolicyBegin = FPresentationPolicyEvidence();
    bHasPendingPerformancePolicyBegin = false;
    if (Record.PresentationPolicyBegin.ExactStateFingerprint
            != Record.PresentationPolicyEnd.ExactStateFingerprint)
    {
        OutError = FString::Printf(
            TEXT("%s performance segment changed actor/helper/policy/LOD/tick/URO/counters inside the timed window"),
            Segment);
        return false;
    }

    if (Record.Segment == TEXT("rhbh_gameplay"))
    {
        if (PerformanceRecords.Num() != 1)
        {
            OutError = TEXT(
                "gameplay performance policy snapshot lacked exactly one preceding creator segment");
            return false;
        }
        const FPresentationPolicyEvidence& CreatorEnd =
            PerformanceRecords[0].PresentationPolicyEnd;
        const FPresentationPolicyEvidence& GameplayBegin =
            Record.PresentationPolicyBegin;
        const bool bComponentIdentityStable =
            CreatorEnd.ActiveVisualActor == GameplayBegin.ActiveVisualActor
            && CreatorEnd.AnimationSourceComponent
                == GameplayBegin.AnimationSourceComponent
            && CreatorEnd.LODSync.ComponentPath
                == GameplayBegin.LODSync.ComponentPath
            && CreatorEnd.OutfitRequiredBonesHelper.ComponentPath
                == GameplayBegin.OutfitRequiredBonesHelper.ComponentPath
            && CreatorEnd.SkeletalComponents.Num()
                == GameplayBegin.SkeletalComponents.Num()
            && CreatorEnd.GroomComponents.Num()
                == GameplayBegin.GroomComponents.Num();
        bool bNamedComponentIdentityStable = bComponentIdentityStable;
        for (int32 Index = 0;
             bNamedComponentIdentityStable
                && Index < CreatorEnd.SkeletalComponents.Num();
             ++Index)
        {
            const FSkeletalPolicyEvidence& Before =
                CreatorEnd.SkeletalComponents[Index];
            const FSkeletalPolicyEvidence& After =
                GameplayBegin.SkeletalComponents[Index];
            bNamedComponentIdentityStable = Before.Name == After.Name
                && Before.ComponentPath == After.ComponentPath
                && Before.SkeletalMeshPath == After.SkeletalMeshPath;
        }
        for (int32 Index = 0;
             bNamedComponentIdentityStable
                && Index < CreatorEnd.GroomComponents.Num();
             ++Index)
        {
            const FGroomPolicyEvidence& Before =
                CreatorEnd.GroomComponents[Index];
            const FGroomPolicyEvidence& After =
                GameplayBegin.GroomComponents[Index];
            bNamedComponentIdentityStable = Before.Name == After.Name
                && Before.ComponentPath == After.ComponentPath
                && Before.GroomAssetPath == After.GroomAssetPath;
        }
        if (!bNamedComponentIdentityStable
            || GameplayBegin.Counters.TransitionSuccess
                - CreatorEnd.Counters.TransitionSuccess != 1
            || GameplayBegin.Counters.TransitionFailure
                != CreatorEnd.Counters.TransitionFailure
            || GameplayBegin.Counters.RollbackSuccess
                != CreatorEnd.Counters.RollbackSuccess
            || GameplayBegin.Counters.RollbackFailure
                != CreatorEnd.Counters.RollbackFailure)
        {
            OutError = TEXT(
                "creator-to-gameplay performance boundary did not preserve actor/helper/component identity with exactly one committed transition and zero failure/rollback deltas");
            return false;
        }
    }

    const FDiscGolfPerformanceBudget& Budget = SegmentTracker.GetBudget();
    const int32 RawFrameCount = PerformanceRawFrameTimesMs.Num();
    const int32 BucketCount = FMath::Min(
        RawFrameCount, Budget.WindowSampleCount);
    for (int32 BucketIndex = 0; BucketIndex < BucketCount; ++BucketIndex)
    {
        const int32 StartIndex =
            BucketIndex * RawFrameCount / BucketCount;
        const int32 EndIndex =
            (BucketIndex + 1) * RawFrameCount / BucketCount;
        float BucketMaximumMs = 0.0f;
        for (int32 FrameIndex = StartIndex;
             FrameIndex < EndIndex;
             ++FrameIndex)
        {
            BucketMaximumMs = FMath::Max(
                BucketMaximumMs,
                PerformanceRawFrameTimesMs[FrameIndex]);
        }
        SegmentTracker.AddFrame(BucketMaximumMs / 1000.0f);
    }
    Record.Summary = SegmentTracker.Summarize(
        PerformanceMaxUsedPhysicalBytes);
    Record.RawSummary = SummarizeAllFrameIntervals(
        PerformanceRawFrameTimesMs,
        PerformanceMaxUsedPhysicalBytes,
        Budget);
    const EDiscGolfPerformanceBudgetState BoundedState =
        DiscGolfPerformance::Evaluate(Record.Summary, Budget);
    const EDiscGolfPerformanceBudgetState RawState =
        DiscGolfPerformance::Evaluate(Record.RawSummary, Budget);
    if (BoundedState == EDiscGolfPerformanceBudgetState::Fail
        || RawState == EDiscGolfPerformanceBudgetState::Fail)
    {
        Record.State = EDiscGolfPerformanceBudgetState::Fail;
    }
    else if (BoundedState == EDiscGolfPerformanceBudgetState::Warning
        || RawState == EDiscGolfPerformanceBudgetState::Warning)
    {
        Record.State = EDiscGolfPerformanceBudgetState::Warning;
    }
    else if (BoundedState == EDiscGolfPerformanceBudgetState::Pass
        && RawState == EDiscGolfPerformanceBudgetState::Pass)
    {
        Record.State = EDiscGolfPerformanceBudgetState::Pass;
    }
    else
    {
        Record.State = EDiscGolfPerformanceBudgetState::WarmingUp;
    }
    PerformanceRecords.Add(Record);

    const bool bCreatorSegment =
        Record.Segment == TEXT("creator_closeup");
    const bool bPauseContract = bCreatorSegment
        ? UGameplayStatics::IsGamePaused(this)
        : !UGameplayStatics::IsGamePaused(this);
    if (!bPauseContract
        || Record.DurationSeconds < PerformanceSegmentSeconds
        || Record.Summary.SampleCount < 120
        || Record.Summary.SampleCount > 600
        || Record.Summary.P95FrameTimeMs > AcceptedP95FrameMs
        || Record.Summary.HitchCount != 0
        || Record.RawSummary.SampleCount < Record.Summary.SampleCount
        || Record.RawSummary.P95FrameTimeMs > AcceptedP95FrameMs
        || Record.RawSummary.HitchCount != 0
        || Record.Summary.UsedPhysicalBytes > AcceptedUsedPhysicalBytes
        || Record.State != EDiscGolfPerformanceBudgetState::Pass)
    {
        OutError = FString::Printf(
            TEXT("%s performance failed: duration=%.3f bounded_samples=%d bounded_p95=%.3f bounded_hitches=%d raw_frames=%d raw_p95=%.3f raw_hitches=%d raw_hitch_rate=%.3f used_physical=%llu state=%s"),
            Segment,
            Record.DurationSeconds,
            Record.Summary.SampleCount,
            Record.Summary.P95FrameTimeMs,
            Record.Summary.HitchCount,
            Record.RawSummary.SampleCount,
            Record.RawSummary.P95FrameTimeMs,
            Record.RawSummary.HitchCount,
            Record.RawSummary.HitchRatePercent,
            static_cast<unsigned long long>(
                Record.Summary.UsedPhysicalBytes),
            *DiscGolfPerformance::StateName(Record.State));
        return false;
    }
    return true;
}

int32 ADiscGolfSession8BMetaHumanPackagedRunner::CountWorldDiscs() const
{
    int32 Count = 0;
    if (!GetWorld())
    {
        return Count;
    }
    for (TActorIterator<ADiscActor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && !It->IsActorBeingDestroyed())
        {
            ++Count;
        }
    }
    return Count;
}

int32 ADiscGolfSession8BMetaHumanPackagedRunner::CountActiveVisualActors(
    UClass* ActorClass) const
{
    if (!GetWorld() || !ActorClass)
    {
        return 0;
    }
    int32 Count = 0;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (IsValid(*It) && It->IsA(ActorClass)
            && !It->IsActorBeingDestroyed())
        {
            ++Count;
        }
    }
    return Count;
}

FString ADiscGolfSession8BMetaHumanPackagedRunner::SuccessStatus() const
{
    switch (Phase)
    {
        case EPhase::ApplyMetaHuman:
            return TEXT("PASS_DG_TO_METAHUMAN_APPLY");
        case EPhase::ReloadCancelFailureSwitchDG:
            return TEXT("PASS_RELOAD_CANCEL_FAILURE_ATOMICITY_AND_MH_TO_DG");
        case EPhase::ReloadDGRestoreMetaHumanVisualThrow:
            return TEXT("PASS_RELOAD_DG_RESTORE_METAHUMAN_AND_RHBH");
        case EPhase::MetaHumanPerformance:
            return TEXT("PASS_METAHUMAN_RENDERED_PERFORMANCE");
        case EPhase::Invalid:
        default:
            return TEXT("FAIL");
    }
}

bool ADiscGolfSession8BMetaHumanPackagedRunner::WritePhaseReport(
    const FString& Status,
    const FString& FailureCode,
    const FString& Reason) const
{
    if (OutputDirectory.IsEmpty() || ReportPath.IsEmpty())
    {
        return false;
    }
    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), Schema);
    Root->SetStringField(TEXT("captured_utc"),
        FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("status"), Status);
    Root->SetStringField(TEXT("phase_status"), Status);
    Root->SetStringField(TEXT("failure_code"), FailureCode);
    Root->SetStringField(TEXT("reason"), SafeSingleLine(Reason));
    Root->SetStringField(TEXT("phase"), PhaseName);
    Root->SetStringField(TEXT("run_id"), RunId);
    Root->SetStringField(TEXT("runtime_mode"), TEXT("packaged"));
    Root->SetStringField(TEXT("rhi"),
        GDynamicRHI ? FString(GDynamicRHI->GetName()) : TEXT("Unavailable"));
    Root->SetStringField(TEXT("gpu_brand"), GRHIAdapterName);
    Root->SetNumberField(TEXT("resolution_x"), ExpectedWidth);
    Root->SetNumberField(TEXT("resolution_y"), ExpectedHeight);
    Root->SetStringField(TEXT("user_dir"), UserDirectory);
    Root->SetStringField(TEXT("isolated_save_path"), SavePath);
    Root->SetStringField(TEXT("save_semantics"),
        TEXT("isolated_profile_save_only"));
    Root->SetBoolField(TEXT("practice_snapshot_save_suppressed"), true);
    Root->SetBoolField(TEXT("save_mutated_by_phase"),
        bSaveWasMutatedByPhase);
    const bool bFallbackUsed = Golfer
        && Golfer->GetCurrentFullCharacterCustomization().AvatarBackendId
            == FName(
                DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId)
        && Golfer->IsDGProxyPresentationVisible();
    Root->SetBoolField(TEXT("fallback_used"), bFallbackUsed);
    Root->SetBoolField(TEXT("manual_visual_review_required"), true);
    Root->SetNumberField(TEXT("manual_visual_review_required_int"), 1);

    Root->SetStringField(TEXT("backend_profile_id"),
        TEXT("metahuman_assembled"));
    Root->SetStringField(TEXT("backend_preferred_quality_profile_id"),
        TEXT("GameplayPerformance"));
    Root->SetStringField(TEXT("assembly_pipeline"),
        TEXT("UE_OPTIMIZED"));
    Root->SetStringField(TEXT("assembly_optimization_level"),
        TEXT("MEDIUM"));
    Root->SetStringField(TEXT("runtime_scalability_profile"),
        TEXT("OmenGameplay1080pHighFoliageV1"));
    Root->SetStringField(TEXT("performance_route"),
        TEXT("metahuman_creator_closeup_then_rhbh_gameplay"));
    Root->SetStringField(TEXT("customization_semantics"),
        TEXT("CURATED_FIXED_PRESET"));
    Root->SetStringField(TEXT("facial_hair_semantics"),
        TEXT("CLEAN_SHAVEN"));
    Root->SetStringField(TEXT("proxy_value_semantics"),
        TEXT("PROXY_ONLY_VALUES_PRESERVED_NOT_APPLIED"));
    Root->SetStringField(TEXT("backend_failure_semantics"),
        TEXT("FAILED_CANDIDATE_RETAINED_ACTIVE_VISUAL"));
    Root->SetStringField(TEXT("cosmetic_failure_semantics"),
        TEXT("UNAVAILABLE_CUSTOMIZATION_REJECTED_BEFORE_MUTATION"));
    Root->SetStringField(TEXT("release_authority"),
        TEXT("DiscGolfTourGameMode.RequestThrowFromGrip"));
    Root->SetStringField(TEXT("flight_authority"),
        TEXT("DiscActor.DiscFlightComponent"));
    Root->SetStringField(TEXT("animation_authority"),
        TEXT("dg_master_animation_source_preserved"));
    Root->SetStringField(TEXT("avatar_scope"),
        TEXT("metahuman_presentation_only"));
    Root->SetBoolField(TEXT("proxy_values_preserved"),
        bProxyValuesPreserved);
    Root->SetBoolField(TEXT("cosmetic_failure_probe_passed"),
        bCosmeticFailureProbePassed);
    Root->SetBoolField(TEXT("backend_failure_probe_passed"),
        bBackendFailureProbePassed);
    Root->SetBoolField(
        TEXT("gameplay_candidate_isolation_probe_passed"),
        bGameplayCandidateIsolationProbePassed);
    Root->SetBoolField(
        TEXT("presentation_policy_transition_probes_passed"),
        bPresentationPolicyTransitionProbesPassed);
    Root->SetStringField(TEXT("backend_failure_adapter_status"),
        BackendFailureAdapterStatus);
    Root->SetBoolField(TEXT("duplicate_release_no_op_passed"),
        bDuplicateReleaseNoOpPassed);
    Root->SetNumberField(TEXT("release_callback_count"),
        ReleaseCallbackCount);
    Root->SetNumberField(TEXT("recovery_callback_count"),
        RecoveryCallbackCount);
    Root->SetBoolField(TEXT("performance_saw_authoritative_flight"),
        bPerformanceSawAuthoritativeFlight);
    Root->SetBoolField(TEXT("trajectory_export_deferred_at_settlement"),
        bTrajectoryExportDeferredAtSettlement);
    Root->SetBoolField(TEXT("trajectory_summary_ready_before_flush"),
        bTrajectorySummaryReadyBeforeFlush);
    Root->SetBoolField(TEXT("trajectory_export_flushed_after_segment"),
        bTrajectoryExportFlushedAfterSegment);
    Root->SetBoolField(TEXT("trajectory_export_flush_passed"),
        bTrajectoryExportFlushPassed);
    Root->SetStringField(TEXT("deferred_trajectory_capture_id"),
        DeferredTrajectoryCaptureId);
    Root->SetStringField(TEXT("flushed_trajectory_capture_id"),
        FlushedTrajectoryCaptureId);
    const bool bStableThrowPresentationVerified =
        IsValid(StableThrowVisualActor)
        && IsValid(StableThrowVisualBody)
        && IsValid(StableThrowVisualHead)
        && AvatarBackend
        && AvatarBackend->GetActiveVisualActor()
            == StableThrowVisualActor
        && AvatarBackend->GetVerifiedVisualBody()
            == StableThrowVisualBody
        && AvatarBackend->GetVerifiedVisualHead()
            == StableThrowVisualHead;
    Root->SetBoolField(TEXT("stable_throw_presentation_verified"),
        bStableThrowPresentationVerified);
    UDiscGolfMetaHumanRetargetAnimInstance* StableRetargetInstance =
        IsValid(StableThrowVisualBody)
        ? Cast<UDiscGolfMetaHumanRetargetAnimInstance>(
            StableThrowVisualBody->GetAnimInstance())
        : nullptr;
    UIKRetargeter* StableCanonicalRetargeter = CanonicalMetaHumanProfile
        ? Cast<UIKRetargeter>(
            CanonicalMetaHumanProfile->RetargetAsset.LoadSynchronous())
        : nullptr;
    Root->SetBoolField(TEXT("stable_throw_retarget_verified"),
        bStableThrowPresentationVerified
        && IsValid(StableRetargetInstance)
        && StableRetargetInstance->GetClass()
            == UDiscGolfMetaHumanRetargetAnimInstance::StaticClass()
        && IsValid(StableCanonicalRetargeter)
        && StableRetargetInstance->IsConfiguredFor(
            StableCanonicalRetargeter,
            AvatarBackend->GetActiveAnimationSourceMesh()));
    Root->SetStringField(TEXT("stable_throw_visual_actor"),
        IsValid(StableThrowVisualActor)
            ? StableThrowVisualActor->GetPathName() : FString());
    Root->SetStringField(TEXT("stable_throw_visual_body"),
        IsValid(StableThrowVisualBody)
            ? StableThrowVisualBody->GetPathName() : FString());
    Root->SetStringField(TEXT("stable_throw_visual_head"),
        IsValid(StableThrowVisualHead)
            ? StableThrowVisualHead->GetPathName() : FString());

    FDGFullCharacterCustomization EvidenceCustomization = GameInstance
        ? GameInstance->GetFullCharacterCustomization()
        : FDGFullCharacterCustomization();
    DiscGolfFullCharacterRuntime::NormalizeForPersistence(
        EvidenceCustomization);
    const FDGEquippedOutfitEntry* Headwear =
        EvidenceCustomization.Outfit.Equipped.FindByPredicate(
            [](const FDGEquippedOutfitEntry& Entry)
            {
                return Entry.Slot == EDGOutfitSlot::Headwear;
            });
    Root->SetStringField(TEXT("preserved_proxy_face_preset_id"),
        EvidenceCustomization.Face.PresetId.ToString());
    Root->SetStringField(TEXT("preserved_proxy_hair_style_id"),
        EvidenceCustomization.Hair.HairStyleId.ToString());
    Root->SetStringField(TEXT("preserved_proxy_facial_hair_id"),
        EvidenceCustomization.Hair.FacialHairId.ToString());
    Root->SetStringField(TEXT("preserved_proxy_eyebrow_id"),
        EvidenceCustomization.Hair.EyebrowId.ToString());
    Root->SetStringField(TEXT("preserved_proxy_scar_id"),
        EvidenceCustomization.Appearance.ScarId.ToString());
    Root->SetNumberField(TEXT("preserved_proxy_tattoo_count"),
        EvidenceCustomization.Appearance.TattooIds.Num());
    Root->SetStringField(TEXT("preserved_proxy_tattoo_id"),
        EvidenceCustomization.Appearance.TattooIds.Num() == 1
            ? EvidenceCustomization.Appearance.TattooIds[0].ToString()
            : FString());
    Root->SetStringField(TEXT("preserved_proxy_headwear_item_id"),
        Headwear ? Headwear->ItemId.ToString() : FString());
    Root->SetStringField(TEXT("preserved_proxy_headwear_variant_id"),
        Headwear ? Headwear->VariantId.ToString() : FString());

    TArray<uint8> FinalSaveBytes;
    LoadSaveBytes(FinalSaveBytes);
    Root->SetNumberField(TEXT("save_bytes_before"),
        InitialSaveBytes.Num());
    Root->SetNumberField(TEXT("save_bytes_final"),
        FinalSaveBytes.Num());
    Root->SetNumberField(TEXT("capture_count"), Captures.Num());

    TArray<TSharedPtr<FJsonValue>> CaptureValues;
    auto VectorToJson = [](const FVector& Value)
    {
        TSharedRef<FJsonObject> Vector = MakeShared<FJsonObject>();
        Vector->SetNumberField(TEXT("x"), Value.X);
        Vector->SetNumberField(TEXT("y"), Value.Y);
        Vector->SetNumberField(TEXT("z"), Value.Z);
        return Vector;
    };
    auto Vector2DToJson = [](const FVector2D& Value)
    {
        TSharedRef<FJsonObject> Vector = MakeShared<FJsonObject>();
        Vector->SetNumberField(TEXT("x"), Value.X);
        Vector->SetNumberField(TEXT("y"), Value.Y);
        return Vector;
    };
    auto MeshRenderEvidenceToJson = [&VectorToJson, &Vector2DToJson](
        const FCaptureRecord::FMeshRenderEvidence& Evidence)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("component_path"), Evidence.ComponentPath);
        Json->SetStringField(TEXT("skeletal_mesh_path"), Evidence.SkeletalMeshPath);
        Json->SetNumberField(TEXT("bounds_radius_cm"), Evidence.BoundsRadiusCm);
        Json->SetNumberField(TEXT("distance_to_source_cm"), Evidence.DistanceToSourceCm);
        Json->SetNumberField(TEXT("last_render_time"), Evidence.LastRenderTime);
        Json->SetNumberField(TEXT("last_render_time_on_screen"),
            Evidence.LastRenderTimeOnScreen);
        Json->SetNumberField(TEXT("lod_count"), Evidence.LODCount);
        Json->SetNumberField(TEXT("material_count"), Evidence.MaterialCount);
        Json->SetNumberField(TEXT("reference_bone_count"),
            Evidence.ReferenceBoneCount);
        Json->SetNumberField(TEXT("component_space_transform_count"),
            Evidence.ComponentSpaceTransformCount);
        Json->SetBoolField(TEXT("registered"), Evidence.bRegistered);
        Json->SetBoolField(TEXT("visible"), Evidence.bVisible);
        Json->SetBoolField(TEXT("hidden_in_game"), Evidence.bHiddenInGame);
        Json->SetBoolField(TEXT("should_render"), Evidence.bShouldRender);
        Json->SetBoolField(TEXT("render_state_created"),
            Evidence.bRenderStateCreated);
        Json->SetBoolField(TEXT("recently_rendered"),
            Evidence.bRecentlyRendered);
        Json->SetBoolField(TEXT("owner_no_see"), Evidence.bOwnerNoSee);
        Json->SetBoolField(TEXT("only_owner_see"), Evidence.bOnlyOwnerSee);
        Json->SetBoolField(TEXT("render_in_main_pass"),
            Evidence.bRenderInMainPass);
        Json->SetBoolField(TEXT("visible_in_scene_capture_only"),
            Evidence.bVisibleInSceneCaptureOnly);
        Json->SetBoolField(TEXT("bounds_finite"), Evidence.bBoundsFinite);
        Json->SetBoolField(TEXT("pose_finite"), Evidence.bPoseFinite);
        Json->SetBoolField(TEXT("projected_viewport_overlap"),
            Evidence.bProjectedViewportOverlap);
        Json->SetBoolField(TEXT("projected_viewport_contained"),
            Evidence.bProjectedViewportContained);
        Json->SetStringField(TEXT("leader_pose_component_path"),
            Evidence.LeaderPoseComponentPath);
        Json->SetStringField(TEXT("animation_instance_class_path"),
            Evidence.AnimationInstanceClassPath);
        Json->SetStringField(TEXT("post_process_class_path"),
            Evidence.PostProcessClassPath);
        Json->SetStringField(TEXT("pose_source_component_path"),
            Evidence.PoseSourceComponentPath);
        Json->SetStringField(TEXT("pose_sync_mode"),
            Evidence.PoseSyncMode);
        Json->SetBoolField(TEXT("supported_pose_source"),
            Evidence.bSupportedPoseSource);
        Json->SetNumberField(TEXT("shared_anchor_count"),
            Evidence.SharedAnchorCount);
        Json->SetNumberField(TEXT("maximum_shared_anchor_distance_cm"),
            Evidence.MaximumSharedAnchorDistanceCm);
        Json->SetBoolField(TEXT("shared_anchors_converged"),
            Evidence.bSharedAnchorsConverged);
        Json->SetObjectField(TEXT("bounds_origin_cm"),
            VectorToJson(Evidence.BoundsOrigin));
        Json->SetObjectField(TEXT("bounds_extent_cm"),
            VectorToJson(Evidence.BoundsExtent));
        Json->SetObjectField(TEXT("projected_min_px"),
            Vector2DToJson(Evidence.ProjectedMin));
        Json->SetObjectField(TEXT("projected_max_px"),
            Vector2DToJson(Evidence.ProjectedMax));
        return Json;
    };
    auto PrimitiveRenderEvidenceToJson = [&VectorToJson, &Vector2DToJson](
        const FCaptureRecord::FPrimitiveRenderEvidence& Evidence)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("component_path"), Evidence.ComponentPath);
        Json->SetStringField(TEXT("resource_path"), Evidence.ResourcePath);
        Json->SetNumberField(TEXT("bounds_radius_cm"), Evidence.BoundsRadiusCm);
        Json->SetNumberField(TEXT("distance_to_source_cm"),
            Evidence.DistanceToSourceCm);
        Json->SetNumberField(TEXT("last_render_time"), Evidence.LastRenderTime);
        Json->SetNumberField(TEXT("last_render_time_on_screen"),
            Evidence.LastRenderTimeOnScreen);
        Json->SetNumberField(TEXT("material_count"), Evidence.MaterialCount);
        Json->SetNumberField(TEXT("asset_group_count"),
            Evidence.AssetGroupCount);
        Json->SetNumberField(TEXT("component_group_count"),
            Evidence.ComponentGroupCount);
        Json->SetNumberField(TEXT("lod_count"), Evidence.LODCount);
        Json->SetNumberField(TEXT("desired_sync_lod"),
            Evidence.DesiredSyncLOD);
        Json->SetNumberField(TEXT("best_available_lod"),
            Evidence.BestAvailableLOD);
        Json->SetNumberField(TEXT("forced_lod"), Evidence.ForcedLOD);
        Json->SetBoolField(TEXT("registered"), Evidence.bRegistered);
        Json->SetBoolField(TEXT("visible"), Evidence.bVisible);
        Json->SetBoolField(TEXT("hidden_in_game"), Evidence.bHiddenInGame);
        Json->SetBoolField(TEXT("should_render"), Evidence.bShouldRender);
        Json->SetBoolField(TEXT("render_state_created"),
            Evidence.bRenderStateCreated);
        Json->SetBoolField(TEXT("recently_rendered"),
            Evidence.bRecentlyRendered);
        Json->SetBoolField(TEXT("owner_no_see"), Evidence.bOwnerNoSee);
        Json->SetBoolField(TEXT("only_owner_see"), Evidence.bOnlyOwnerSee);
        Json->SetBoolField(TEXT("render_in_main_pass"),
            Evidence.bRenderInMainPass);
        Json->SetBoolField(TEXT("visible_in_scene_capture_only"),
            Evidence.bVisibleInSceneCaptureOnly);
        Json->SetBoolField(TEXT("bounds_finite"), Evidence.bBoundsFinite);
        Json->SetBoolField(TEXT("projected_viewport_overlap"),
            Evidence.bProjectedViewportOverlap);
        Json->SetBoolField(TEXT("asset_valid"), Evidence.bAssetValid);
        Json->SetBoolField(TEXT("asset_groups_valid"),
            Evidence.bAssetGroupsValid);
        Json->SetBoolField(TEXT("compiling"), Evidence.bCompiling);
        Json->SetBoolField(TEXT("tick_even_when_paused"),
            Evidence.bTickEvenWhenPaused);
        Json->SetBoolField(TEXT("projected_viewport_contained"),
            Evidence.bProjectedViewportContained);
        Json->SetBoolField(TEXT("lod_sync_registered"),
            Evidence.bLODSyncRegistered);
        Json->SetBoolField(TEXT("lod_sync_tick_enabled"),
            Evidence.bLODSyncTickEnabled);
        Json->SetBoolField(TEXT("lod_sync_tick_even_when_paused"),
            Evidence.bLODSyncTickEvenWhenPaused);
        Json->SetObjectField(TEXT("bounds_origin_cm"),
            VectorToJson(Evidence.BoundsOrigin));
        Json->SetObjectField(TEXT("bounds_extent_cm"),
            VectorToJson(Evidence.BoundsExtent));
        Json->SetObjectField(TEXT("projected_min_px"),
            Vector2DToJson(Evidence.ProjectedMin));
        Json->SetObjectField(TEXT("projected_max_px"),
            Vector2DToJson(Evidence.ProjectedMax));
        return Json;
    };
    auto OutfitSkinSectionEvidenceToJson = [&VectorToJson](
        const FCaptureRecord::FOutfitSkinSectionEvidence& Evidence)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("material_slot"), Evidence.MaterialSlot);
        Json->SetStringField(TEXT("material_path"), Evidence.MaterialPath);
        Json->SetStringField(TEXT("semantic_reason"),
            Evidence.SemanticReason);
        Json->SetStringField(TEXT("worst_vertex_influences"),
            Evidence.WorstVertexInfluences);
        Json->SetObjectField(TEXT("bounds_minimum_body_cm"),
            VectorToJson(Evidence.BoundsMinimumBodyCm));
        Json->SetObjectField(TEXT("bounds_maximum_body_cm"),
            VectorToJson(Evidence.BoundsMaximumBodyCm));
        Json->SetObjectField(TEXT("worst_vertex_body_cm"),
            VectorToJson(Evidence.WorstVertexBodyCm));
        Json->SetNumberField(TEXT("height_cm"), Evidence.HeightCm);
        Json->SetNumberField(TEXT("knee_clearance_cm"),
            Evidence.KneeClearanceCm);
        Json->SetNumberField(TEXT("foot_clearance_cm"),
            Evidence.FootClearanceCm);
        Json->SetNumberField(TEXT("section_index"), Evidence.SectionIndex);
        Json->SetNumberField(TEXT("authored_material_index"),
            Evidence.AuthoredMaterialIndex);
        Json->SetNumberField(TEXT("resolved_material_index"),
            Evidence.ResolvedMaterialIndex);
        Json->SetNumberField(TEXT("base_vertex_index"),
            Evidence.BaseVertexIndex);
        Json->SetNumberField(TEXT("vertex_count"), Evidence.VertexCount);
        Json->SetNumberField(TEXT("triangle_count"), Evidence.TriangleCount);
        Json->SetNumberField(TEXT("checked_vertex_count"),
            Evidence.CheckedVertexCount);
        Json->SetNumberField(TEXT("below_knee_vertex_count"),
            Evidence.BelowKneeVertexCount);
        Json->SetNumberField(TEXT("below_foot_vertex_count"),
            Evidence.BelowFootVertexCount);
        Json->SetNumberField(TEXT("worst_vertex_index"),
            Evidence.WorstVertexIndex);
        Json->SetBoolField(TEXT("enabled"), Evidence.bEnabled);
        Json->SetBoolField(TEXT("has_clothing_data"),
            Evidence.bHasClothingData);
        Json->SetBoolField(TEXT("material_uses_world_position_offset"),
            Evidence.bMaterialUsesWorldPositionOffset);
        Json->SetBoolField(TEXT("semantic_accepted"),
            Evidence.bSemanticAccepted);
        return Json;
    };
    auto OutfitLiveSkinEvidenceToJson = [
        &OutfitSkinSectionEvidenceToJson](
        const FCaptureRecord::FOutfitLiveSkinEvidence& Evidence)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("pose_sync_mode"), Evidence.PoseSyncMode);
        Json->SetStringField(TEXT("structural_reason"),
            Evidence.StructuralReason);
        Json->SetStringField(TEXT("semantic_reason"),
            Evidence.SemanticReason);
        Json->SetStringField(TEXT("fixed_function_support_reason"),
            Evidence.FixedFunctionSupportReason);
        Json->SetStringField(TEXT("active_morph_target_summary"),
            Evidence.ActiveMorphTargetSummary);
        Json->SetStringField(TEXT("active_mesh_deformer_name"),
            Evidence.ActiveMeshDeformerName);
        Json->SetStringField(TEXT("active_mesh_deformer_path"),
            Evidence.ActiveMeshDeformerPath);
        Json->SetStringField(TEXT("body_post_process_class_path"),
            Evidence.BodyPostProcessClassPath);
        Json->SetStringField(TEXT("renderer_fallback_bone_list"),
            Evidence.RendererFallbackBoneList);
        Json->SetStringField(TEXT("renderer_fallback_summary"),
            Evidence.RendererFallbackSummary);
        Json->SetStringField(TEXT("renderer_fallback_reason"),
            Evidence.RendererFallbackReason);
        Json->SetNumberField(TEXT("knee_plane_body_z_cm"),
            Evidence.KneePlaneBodyZCm);
        Json->SetNumberField(TEXT("foot_plane_body_z_cm"),
            Evidence.FootPlaneBodyZCm);
        Json->SetNumberField(TEXT("knee_to_foot_span_cm"),
            Evidence.KneeToFootSpanCm);
        Json->SetNumberField(TEXT("actual_rendered_lod"),
            Evidence.ActualRenderedLOD);
        Json->SetNumberField(TEXT("predicted_lod"), Evidence.PredictedLOD);
        Json->SetNumberField(TEXT("desired_sync_lod"),
            Evidence.DesiredSyncLOD);
        Json->SetNumberField(TEXT("best_available_lod"),
            Evidence.BestAvailableLOD);
        Json->SetNumberField(TEXT("force_rendered_lod"),
            Evidence.ForceRenderedLOD);
        Json->SetNumberField(TEXT("force_streamed_lod"),
            Evidence.ForceStreamedLOD);
        Json->SetNumberField(TEXT("forced_lod_legacy_one_based"),
            Evidence.ForcedLODLegacyOneBased);
        Json->SetNumberField(TEXT("computed_min_lod"),
            Evidence.ComputedMinLOD);
        Json->SetNumberField(TEXT("asset_min_lod"), Evidence.AssetMinLOD);
        Json->SetNumberField(TEXT("current_first_lod"),
            Evidence.CurrentFirstLOD);
        Json->SetNumberField(TEXT("pending_first_lod"),
            Evidence.PendingFirstLOD);
        Json->SetNumberField(TEXT("vertex_count"), Evidence.VertexCount);
        Json->SetNumberField(TEXT("section_count"), Evidence.SectionCount);
        Json->SetNumberField(TEXT("outfit_reference_bone_count"),
            Evidence.OutfitReferenceBoneCount);
        Json->SetNumberField(TEXT("body_reference_bone_count"),
            Evidence.BodyReferenceBoneCount);
        Json->SetNumberField(TEXT("body_component_space_transform_count"),
            Evidence.BodyComponentSpaceTransformCount);
        Json->SetNumberField(TEXT("leader_bone_map_count"),
            Evidence.LeaderBoneMapCount);
        Json->SetNumberField(TEXT("used_outfit_bone_count"),
            Evidence.UsedOutfitBoneCount);
        Json->SetNumberField(TEXT("missing_leader_mapping_count"),
            Evidence.MissingLeaderMappingCount);
        Json->SetNumberField(TEXT("leader_name_mismatch_count"),
            Evidence.LeaderNameMismatchCount);
        Json->SetNumberField(TEXT("checked_vertex_count"),
            Evidence.CheckedVertexCount);
        Json->SetNumberField(TEXT("checked_influence_count"),
            Evidence.CheckedInfluenceCount);
        Json->SetNumberField(TEXT("non_finite_vertex_count"),
            Evidence.NonFiniteVertexCount);
        Json->SetNumberField(TEXT("invalid_influence_count"),
            Evidence.InvalidInfluenceCount);
        Json->SetNumberField(TEXT("non_normalized_weight_vertex_count"),
            Evidence.NonNormalizedWeightVertexCount);
        Json->SetNumberField(TEXT("invalid_section_bone_map_count"),
            Evidence.InvalidSectionBoneMapCount);
        Json->SetNumberField(TEXT("inactive_used_outfit_bone_count"),
            Evidence.InactiveUsedOutfitBoneCount);
        Json->SetNumberField(TEXT("hidden_leader_bone_count"),
            Evidence.HiddenLeaderBoneCount);
        Json->SetNumberField(TEXT("invalid_leader_pose_bone_count"),
            Evidence.InvalidLeaderPoseBoneCount);
        Json->SetNumberField(TEXT("renderer_fallback_bone_count"),
            Evidence.RendererFallbackBoneCount);
        Json->SetNumberField(TEXT("morph_target_map_entry_count"),
            Evidence.MorphTargetMapEntryCount);
        Json->SetNumberField(TEXT("active_morph_target_count"),
            Evidence.ActiveMorphTargetCount);
        Json->SetNumberField(TEXT("non_zero_morph_target_weight_count"),
            Evidence.NonZeroMorphTargetWeightCount);
        Json->SetNumberField(TEXT("non_zero_morph_curve_count"),
            Evidence.NonZeroMorphCurveCount);
        Json->SetNumberField(TEXT("invalid_active_morph_target_count"),
            Evidence.InvalidActiveMorphTargetCount);
        Json->SetNumberField(TEXT("non_finite_morph_weight_count"),
            Evidence.NonFiniteMorphWeightCount);
        Json->SetNumberField(TEXT("active_external_morph_target_count"),
            Evidence.ActiveExternalMorphTargetCount);
        Json->SetNumberField(
            TEXT("non_finite_external_morph_weight_count"),
            Evidence.NonFiniteExternalMorphWeightCount);
        Json->SetNumberField(TEXT("clothing_simulation_count"),
            Evidence.ClothingSimulationCount);
        Json->SetNumberField(TEXT("body_bone_revision_before"),
            Evidence.BodyBoneRevisionBefore);
        Json->SetNumberField(TEXT("body_bone_revision_after"),
            Evidence.BodyBoneRevisionAfter);
        Json->SetBoolField(TEXT("mesh_object_present"),
            Evidence.bMeshObjectPresent);
        Json->SetBoolField(TEXT("mesh_object_dynamic_data_valid"),
            Evidence.bMeshObjectDynamicDataValid);
        Json->SetBoolField(TEXT("position_cpu_access_requested"),
            Evidence.bPositionCPUAccessRequested);
        Json->SetBoolField(TEXT("position_data_present"),
            Evidence.bPositionDataPresent);
        Json->SetBoolField(TEXT("skin_weight_cpu_access_requested"),
            Evidence.bSkinWeightCPUAccessRequested);
        Json->SetBoolField(TEXT("skin_weight_data_present"),
            Evidence.bSkinWeightDataPresent);
        Json->SetBoolField(TEXT("skin_weight_lookup_present"),
            Evidence.bSkinWeightLookupPresent);
        Json->SetBoolField(TEXT("skin_weight_vertex_count_matches"),
            Evidence.bSkinWeightVertexCountMatches);
        Json->SetBoolField(TEXT("skin_weight_profile_pending"),
            Evidence.bSkinWeightProfilePending);
        Json->SetBoolField(TEXT("using_skin_weight_profile"),
            Evidence.bUsingSkinWeightProfile);
        Json->SetBoolField(TEXT("every_vertex_covered_exactly_once"),
            Evidence.bEveryVertexCoveredExactlyOnce);
        Json->SetBoolField(TEXT("ref_pose_override_present"),
            Evidence.bRefPoseOverridePresent);
        Json->SetBoolField(TEXT("leader_safe_pose_validation_enabled"),
            Evidence.bLeaderSafePoseValidationEnabled);
        Json->SetBoolField(TEXT("leader_valid_mesh_pose_array_present"),
            Evidence.bLeaderValidMeshPoseArrayPresent);
        Json->SetBoolField(TEXT("body_post_process_disabled"),
            Evidence.bBodyPostProcessDisabled);
        Json->SetBoolField(TEXT("body_post_process_should_evaluate"),
            Evidence.bBodyPostProcessShouldEvaluate);
        Json->SetBoolField(TEXT("has_active_morph_targets"),
            Evidence.bHasActiveMorphTargets);
        Json->SetBoolField(TEXT("has_active_external_morph_targets"),
            Evidence.bHasActiveExternalMorphTargets);
        Json->SetBoolField(TEXT("has_mesh_deformer"),
            Evidence.bHasMeshDeformer);
        Json->SetBoolField(TEXT("has_clothing_simulation"),
            Evidence.bHasClothingSimulation);
        Json->SetBoolField(TEXT("has_section_clothing_data"),
            Evidence.bHasSectionClothingData);
        Json->SetBoolField(TEXT("has_world_position_offset_material"),
            Evidence.bHasWorldPositionOffsetMaterial);
        Json->SetBoolField(TEXT("fixed_function_path_complete"),
            Evidence.bFixedFunctionPathComplete);
        Json->SetBoolField(TEXT("structurally_valid"),
            Evidence.bStructurallyValid);
        Json->SetBoolField(TEXT("semantic_accepted"),
            Evidence.bSemanticAccepted);

        TSharedRef<FJsonObject> Thresholds = MakeShared<FJsonObject>();
        Thresholds->SetNumberField(TEXT("shirt_minimum_knee_clearance_cm"),
            ShirtMinimumKneeClearanceCm);
        Thresholds->SetNumberField(TEXT("short_minimum_knee_clearance_cm"),
            ShortMinimumKneeClearanceCm);
        Thresholds->SetNumberField(TEXT("garment_minimum_foot_clearance_cm"),
            GarmentMinimumFootClearanceCm);
        Thresholds->SetNumberField(TEXT("shirt_maximum_height_cm"),
            ShirtMaximumHeightCm);
        Thresholds->SetNumberField(TEXT("short_maximum_height_cm"),
            ShortMaximumHeightCm);
        Thresholds->SetNumberField(TEXT("below_knee_tolerance_cm"),
            BelowKneeToleranceCm);
        Thresholds->SetNumberField(TEXT("below_foot_tolerance_cm"),
            BelowFootToleranceCm);
        Json->SetObjectField(TEXT("semantic_thresholds"), Thresholds);

        TArray<TSharedPtr<FJsonValue>> SectionValues;
        SectionValues.Reserve(Evidence.Sections.Num());
        for (const FCaptureRecord::FOutfitSkinSectionEvidence& Section
            : Evidence.Sections)
        {
            SectionValues.Add(MakeShared<FJsonValueObject>(
                OutfitSkinSectionEvidenceToJson(Section)));
        }
        Json->SetArrayField(TEXT("sections"), SectionValues);
        return Json;
    };
    auto PresentationPolicyCountersToJson = [](
        const FPresentationPolicyCounters& Counters)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("transition_success_count"),
            Counters.TransitionSuccess);
        Json->SetNumberField(TEXT("transition_failure_count"),
            Counters.TransitionFailure);
        Json->SetNumberField(TEXT("rollback_success_count"),
            Counters.RollbackSuccess);
        Json->SetNumberField(TEXT("rollback_failure_count"),
            Counters.RollbackFailure);
        return Json;
    };
    auto PresentationPolicyEvidenceToJson = [
        &PresentationPolicyCountersToJson](
            const FPresentationPolicyEvidence& Evidence)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("quality_profile_id"),
            Evidence.QualityProfileId);
        Json->SetStringField(TEXT("requested_policy"),
            Evidence.RequestedPolicy);
        Json->SetStringField(TEXT("verified_policy"),
            Evidence.VerifiedPolicy);
        Json->SetBoolField(TEXT("policy_verified"),
            Evidence.bPolicyVerified);
        Json->SetStringField(TEXT("policy_status"),
            Evidence.PolicyStatus);
        Json->SetStringField(TEXT("active_visual_actor"),
            Evidence.ActiveVisualActor);
        Json->SetStringField(TEXT("animation_source_component"),
            Evidence.AnimationSourceComponent);
        Json->SetNumberField(TEXT("verified_forced_lod"),
            Evidence.VerifiedForcedLOD);
        Json->SetObjectField(TEXT("counters"),
            PresentationPolicyCountersToJson(Evidence.Counters));

        TSharedRef<FJsonObject> LODSync = MakeShared<FJsonObject>();
        LODSync->SetStringField(TEXT("component_path"),
            Evidence.LODSync.ComponentPath);
        LODSync->SetStringField(TEXT("contract_status"),
            Evidence.LODSync.ContractStatus);
        LODSync->SetBoolField(TEXT("contract_valid"),
            Evidence.LODSync.bContractValid);
        LODSync->SetNumberField(TEXT("num_lods"),
            Evidence.LODSync.NumLODs);
        LODSync->SetNumberField(TEXT("min_lod"),
            Evidence.LODSync.MinLOD);
        LODSync->SetNumberField(TEXT("forced_lod"),
            Evidence.LODSync.ForcedLOD);
        LODSync->SetBoolField(TEXT("registered"),
            Evidence.LODSync.bRegistered);
        LODSync->SetBoolField(TEXT("tick_enabled"),
            Evidence.LODSync.bTickEnabled);
        LODSync->SetBoolField(TEXT("tick_even_when_paused"),
            Evidence.LODSync.bTickEvenWhenPaused);
        TArray<TSharedPtr<FJsonValue>> SyncEntries;
        for (const FLODSyncComponentEntryEvidence& Entry
             : Evidence.LODSync.ComponentsToSync)
        {
            TSharedRef<FJsonObject> EntryJson = MakeShared<FJsonObject>();
            EntryJson->SetStringField(TEXT("name"), Entry.Name);
            EntryJson->SetStringField(TEXT("sync_option"),
                Entry.SyncOption);
            EntryJson->SetBoolField(TEXT("component_present"),
                Entry.bComponentPresent);
            SyncEntries.Add(MakeShared<FJsonValueObject>(EntryJson));
        }
        LODSync->SetArrayField(TEXT("components_to_sync"), SyncEntries);
        TArray<TSharedPtr<FJsonValue>> MappingEntries;
        for (const FLODSyncMappingEvidence& Mapping
             : Evidence.LODSync.CustomLODMapping)
        {
            TSharedRef<FJsonObject> MappingJson = MakeShared<FJsonObject>();
            MappingJson->SetStringField(TEXT("name"), Mapping.Name);
            TArray<TSharedPtr<FJsonValue>> LODValues;
            for (const int32 LOD : Mapping.Mapping)
            {
                LODValues.Add(MakeShared<FJsonValueNumber>(LOD));
            }
            MappingJson->SetArrayField(TEXT("mapping"), LODValues);
            MappingEntries.Add(MakeShared<FJsonValueObject>(MappingJson));
        }
        LODSync->SetArrayField(TEXT("custom_lod_mapping"), MappingEntries);
        TArray<TSharedPtr<FJsonValue>> MissingComponents;
        for (const FString& Name : Evidence.LODSync.MissingDeclaredComponents)
        {
            MissingComponents.Add(MakeShared<FJsonValueString>(Name));
        }
        LODSync->SetArrayField(TEXT("missing_declared_components"),
            MissingComponents);
        Json->SetObjectField(TEXT("lod_sync"), LODSync);

        TArray<TSharedPtr<FJsonValue>> SkeletalComponents;
        for (const FSkeletalPolicyEvidence& Mesh
             : Evidence.SkeletalComponents)
        {
            TSharedRef<FJsonObject> MeshJson = MakeShared<FJsonObject>();
            MeshJson->SetStringField(TEXT("name"), Mesh.Name);
            MeshJson->SetStringField(TEXT("component_path"),
                Mesh.ComponentPath);
            MeshJson->SetStringField(TEXT("skeletal_mesh_path"),
                Mesh.SkeletalMeshPath);
            MeshJson->SetStringField(TEXT("visibility_tick_option"),
                Mesh.VisibilityTickOption);
            MeshJson->SetNumberField(TEXT("lod_count"), Mesh.LODCount);
            MeshJson->SetNumberField(TEXT("expected_mapped_lod"),
                Mesh.ExpectedMappedLOD);
            MeshJson->SetNumberField(
                TEXT("forced_lod_legacy_one_based"),
                Mesh.ForcedLODLegacyOneBased);
            MeshJson->SetNumberField(TEXT("force_rendered_lod"),
                Mesh.ForceRenderedLOD);
            MeshJson->SetNumberField(TEXT("force_streamed_lod"),
                Mesh.ForceStreamedLOD);
            MeshJson->SetNumberField(TEXT("actual_rendered_lod"),
                Mesh.ActualRenderedLOD);
            MeshJson->SetNumberField(TEXT("predicted_lod"),
                Mesh.PredictedLOD);
            MeshJson->SetNumberField(TEXT("desired_sync_lod"),
                Mesh.DesiredSyncLOD);
            MeshJson->SetNumberField(TEXT("best_available_lod"),
                Mesh.BestAvailableLOD);
            MeshJson->SetBoolField(TEXT("registered"), Mesh.bRegistered);
            MeshJson->SetBoolField(TEXT("visible_presentation"),
                Mesh.bVisiblePresentation);
            MeshJson->SetBoolField(TEXT("tick_enabled"),
                Mesh.bTickEnabled);
            MeshJson->SetBoolField(TEXT("tick_even_when_paused"),
                Mesh.bTickEvenWhenPaused);
            MeshJson->SetBoolField(TEXT("update_rate_optimizations"),
                Mesh.bUpdateRateOptimizations);
            MeshJson->SetBoolField(TEXT("source_tick_prerequisite"),
                Mesh.bSourceTickPrerequisite);
            SkeletalComponents.Add(MakeShared<FJsonValueObject>(MeshJson));
        }
        Json->SetArrayField(TEXT("skeletal_components"),
            SkeletalComponents);

        TArray<TSharedPtr<FJsonValue>> GroomComponents;
        for (const FGroomPolicyEvidence& Groom
             : Evidence.GroomComponents)
        {
            TSharedRef<FJsonObject> GroomJson = MakeShared<FJsonObject>();
            GroomJson->SetStringField(TEXT("name"), Groom.Name);
            GroomJson->SetStringField(TEXT("component_path"),
                Groom.ComponentPath);
            GroomJson->SetStringField(TEXT("groom_asset_path"),
                Groom.GroomAssetPath);
            GroomJson->SetNumberField(TEXT("expected_mapped_lod"),
                Groom.ExpectedMappedLOD);
            GroomJson->SetNumberField(TEXT("forced_lod"),
                Groom.ForcedLOD);
            GroomJson->SetNumberField(TEXT("force_rendered_lod"),
                Groom.ForceRenderedLOD);
            GroomJson->SetNumberField(TEXT("force_streamed_lod"),
                Groom.ForceStreamedLOD);
            GroomJson->SetNumberField(TEXT("desired_sync_lod"),
                Groom.DesiredSyncLOD);
            GroomJson->SetNumberField(TEXT("best_available_lod"),
                Groom.BestAvailableLOD);
            GroomJson->SetBoolField(TEXT("mapped_by_lod_sync"),
                Groom.bMappedByLODSync);
            GroomJson->SetBoolField(TEXT("registered"),
                Groom.bRegistered);
            GroomJson->SetBoolField(TEXT("tick_enabled"),
                Groom.bTickEnabled);
            GroomJson->SetBoolField(TEXT("tick_even_when_paused"),
                Groom.bTickEvenWhenPaused);
            GroomComponents.Add(MakeShared<FJsonValueObject>(GroomJson));
        }
        Json->SetArrayField(TEXT("groom_components"), GroomComponents);

        const FOutfitRequiredBonesHelperEvidence& Helper =
            Evidence.OutfitRequiredBonesHelper;
        TSharedRef<FJsonObject> HelperJson = MakeShared<FJsonObject>();
        HelperJson->SetStringField(TEXT("component_path"),
            Helper.ComponentPath);
        HelperJson->SetStringField(TEXT("leader_pose_component_path"),
            Helper.LeaderPoseComponentPath);
        HelperJson->SetNumberField(TEXT("configured_actual_outfit_lod"),
            Helper.ConfiguredActualOutfitLOD);
        HelperJson->SetNumberField(TEXT("configured_predicted_outfit_lod"),
            Helper.ConfiguredPredictedOutfitLOD);
        HelperJson->SetNumberField(TEXT("configured_outfit_bone_count"),
            Helper.ConfiguredOutfitBoneCount);
        HelperJson->SetNumberField(
            TEXT("configured_ready_outfit_lod_count"),
            Helper.ConfiguredReadyOutfitLODCount);
        HelperJson->SetNumberField(TEXT("required_leader_bone_count"),
            Helper.RequiredLeaderBoneCount);
        HelperJson->SetNumberField(
            TEXT("mapped_outfit_used_leader_bone_count"),
            Helper.MappedOutfitUsedLeaderBoneCount);
        HelperJson->SetBoolField(TEXT("registered"), Helper.bRegistered);
        HelperJson->SetBoolField(TEXT("configured_for_body_and_outfit"),
            Helper.bConfiguredForBodyAndOutfit);
        HelperJson->SetBoolField(TEXT("assetless"), Helper.bAssetless);
        HelperJson->SetBoolField(TEXT("visible"), Helper.bVisible);
        HelperJson->SetBoolField(TEXT("hidden_in_game"),
            Helper.bHiddenInGame);
        HelperJson->SetBoolField(TEXT("should_render"),
            Helper.bShouldRender);
        HelperJson->SetBoolField(TEXT("render_in_main_pass"),
            Helper.bRenderInMainPass);
        HelperJson->SetBoolField(TEXT("visible_in_scene_capture_only"),
            Helper.bVisibleInSceneCaptureOnly);
        HelperJson->SetBoolField(TEXT("can_ever_tick"),
            Helper.bCanEverTick);
        HelperJson->SetBoolField(TEXT("tick_enabled"),
            Helper.bTickEnabled);
        HelperJson->SetBoolField(TEXT("collision_disabled"),
            Helper.bCollisionDisabled);
        HelperJson->SetBoolField(TEXT("generate_overlap_events"),
            Helper.bGenerateOverlapEvents);
        HelperJson->SetBoolField(TEXT("registered_as_body_follower"),
            Helper.bRegisteredAsBodyFollower);
        Json->SetObjectField(TEXT("outfit_required_bones_helper"),
            HelperJson);
        return Json;
    };
    auto PresentationPolicyProbeToJson = [
        &PresentationPolicyCountersToJson,
        &PresentationPolicyEvidenceToJson](
            const FPresentationPolicyProbeRecord& Probe)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("name"), Probe.Name);
        Json->SetStringField(TEXT("classification"), Probe.Classification);
        Json->SetStringField(TEXT("invocation"), Probe.Invocation);
        Json->SetBoolField(TEXT("call_result"), Probe.bCallResult);
        Json->SetBoolField(TEXT("expected_call_result"),
            Probe.bExpectedCallResult);
        Json->SetBoolField(TEXT("actor_identity_preserved"),
            Probe.bActorIdentityPreserved);
        Json->SetBoolField(TEXT("helper_identity_preserved"),
            Probe.bHelperIdentityPreserved);
        Json->SetBoolField(TEXT("stable_policy_state_preserved"),
            Probe.bStablePolicyStatePreserved);
        Json->SetObjectField(TEXT("counter_delta"),
            PresentationPolicyCountersToJson(Probe.CounterDelta));
        Json->SetObjectField(TEXT("before"),
            PresentationPolicyEvidenceToJson(Probe.Before));
        Json->SetObjectField(TEXT("after"),
            PresentationPolicyEvidenceToJson(Probe.After));
        return Json;
    };
    TArray<TSharedPtr<FJsonValue>> CandidateIsolationProbeValues;
    for (const FPresentationPolicyProbeRecord& Probe
         : PresentationPolicyCandidateIsolationProbes)
    {
        CandidateIsolationProbeValues.Add(MakeShared<FJsonValueObject>(
            PresentationPolicyProbeToJson(Probe)));
    }
    Root->SetArrayField(
        TEXT("presentation_policy_candidate_isolation_probes"),
        CandidateIsolationProbeValues);
    TArray<TSharedPtr<FJsonValue>> TransitionProbeValues;
    for (const FPresentationPolicyProbeRecord& Probe
         : PresentationPolicyTransitionProbes)
    {
        TransitionProbeValues.Add(MakeShared<FJsonValueObject>(
            PresentationPolicyProbeToJson(Probe)));
    }
    Root->SetArrayField(TEXT("presentation_policy_transition_probes"),
        TransitionProbeValues);
    for (const FCaptureRecord& Capture : Captures)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("filename"), Capture.Filename);
        Json->SetStringField(TEXT("scene"), Capture.Scene);
        Json->SetStringField(TEXT("backend_id"), Capture.BackendId);
        Json->SetStringField(TEXT("active_visual_actor"),
            Capture.ActiveVisualActor);
        Json->SetNumberField(TEXT("bytes"),
            static_cast<double>(Capture.Bytes));
        Json->SetNumberField(TEXT("width"), Capture.Width);
        Json->SetNumberField(TEXT("height"), Capture.Height);
        Json->SetNumberField(TEXT("strokes"), Capture.StrokeCount);
        Json->SetNumberField(TEXT("world_disc_count"),
            Capture.WorldDiscCount);
        Json->SetNumberField(TEXT("release_commit_count"),
            Capture.ReleaseCommitCount);
        Json->SetNumberField(TEXT("expected_strokes"),
            Capture.ExpectedStrokeCount);
        Json->SetNumberField(TEXT("expected_world_disc_count"),
            Capture.ExpectedWorldDiscCount);
        Json->SetNumberField(TEXT("expected_release_commit_count"),
            Capture.ExpectedReleaseCommitCount);
        Json->SetStringField(TEXT("throw_phase"), Capture.ThrowPhase);
        Json->SetBoolField(TEXT("creator_open"), Capture.bCreatorOpen);
        Json->SetBoolField(TEXT("dg_proxy_visible"),
            Capture.bDGProxyVisible);
        Json->SetBoolField(TEXT("metahuman_ready"),
            Capture.bMetaHumanReady);
        Json->SetBoolField(TEXT("held_disc_visible"),
            Capture.bHeldDiscVisible);
        Json->SetBoolField(TEXT("gameplay_disc_active"),
            Capture.bGameplayDiscActive);
        Json->SetBoolField(TEXT("release_evidence_collected"),
            Capture.bReleaseEvidenceCollected);
        Json->SetBoolField(TEXT("immutable_release_grip_valid"),
            Capture.bImmutableReleaseGripValid);
        Json->SetBoolField(TEXT("gameplay_disc_location_finite"),
            Capture.bGameplayDiscLocationFinite);
        Json->SetBoolField(TEXT("disc_to_grip_distance_finite"),
            Capture.bDiscToGripDistanceFinite);
        Json->SetBoolField(TEXT("released_disc_identity_verified"),
            Capture.bReleasedDiscIdentityVerified);
        Json->SetBoolField(TEXT("flight_component_identity_verified"),
            Capture.bFlightComponentIdentityVerified);
        Json->SetBoolField(TEXT("flight_is_flying"),
            Capture.bFlightIsFlying);
        Json->SetBoolField(TEXT("flight_tick_snapshot_present"),
            Capture.bFlightTickSnapshotPresent);
        Json->SetBoolField(TEXT("flight_tick_was_enabled_before_pause"),
            Capture.bFlightTickWasEnabledBeforePause);
        Json->SetBoolField(TEXT("flight_tick_suspended"),
            Capture.bFlightTickSuspended);
        Json->SetBoolField(TEXT("world_paused_for_capture"),
            Capture.bWorldPausedForCapture);
        TSharedRef<FJsonObject> Grip = MakeShared<FJsonObject>();
        Grip->SetNumberField(TEXT("x"), Capture.GripWorldLocation.X);
        Grip->SetNumberField(TEXT("y"), Capture.GripWorldLocation.Y);
        Grip->SetNumberField(TEXT("z"), Capture.GripWorldLocation.Z);
        Json->SetObjectField(TEXT("grip_world_cm"), Grip);
        TSharedRef<FJsonObject> Disc = MakeShared<FJsonObject>();
        Disc->SetNumberField(TEXT("x"),
            Capture.GameplayDiscWorldLocation.X);
        Disc->SetNumberField(TEXT("y"),
            Capture.GameplayDiscWorldLocation.Y);
        Disc->SetNumberField(TEXT("z"),
            Capture.GameplayDiscWorldLocation.Z);
        Json->SetObjectField(TEXT("gameplay_disc_world_cm"), Disc);
        Json->SetNumberField(TEXT("disc_to_grip_distance_cm"),
            Capture.DiscToGripDistanceCm);
        Json->SetBoolField(TEXT("visible_hand_alignment_evidence_collected"),
            Capture.bVisibleHandAlignmentEvidenceCollected);
        Json->SetBoolField(TEXT("visible_hand_location_finite"),
            Capture.bVisibleHandLocationFinite);
        Json->SetBoolField(TEXT("visible_disc_location_finite"),
            Capture.bVisibleDiscLocationFinite);
        Json->SetBoolField(TEXT("visible_hand_distance_finite"),
            Capture.bVisibleHandDistanceFinite);
        Json->SetObjectField(TEXT("metahuman_hand_world_cm"),
            VectorToJson(Capture.MetaHumanHandWorldLocation));
        Json->SetObjectField(TEXT("visible_disc_world_cm"),
            VectorToJson(Capture.VisibleDiscWorldLocation));
        Json->SetNumberField(
            TEXT("visible_disc_to_metahuman_hand_distance_cm"),
            Capture.VisibleDiscToMetaHumanHandDistanceCm);
        Json->SetStringField(TEXT("presentation_hand_correction_mode"),
            Capture.PresentationHandCorrectionMode);
        Json->SetBoolField(
            TEXT("presentation_hand_correction_snapshot_valid"),
            Capture.bPresentationHandCorrectionSnapshotValid);
        Json->SetBoolField(TEXT("presentation_hand_correction_reachable"),
            Capture.bPresentationHandCorrectionReachable);
        Json->SetBoolField(TEXT("presentation_hand_correction_applied"),
            Capture.bPresentationHandCorrectionApplied);
        Json->SetObjectField(TEXT("source_hand_r_world_cm"),
            VectorToJson(Capture.SourceHandWorldLocation));
        Json->SetObjectField(TEXT("source_disc_grip_r_world_cm"),
            VectorToJson(Capture.SourceDiscGripWorldLocation));
        Json->SetObjectField(
            TEXT("metahuman_hand_r_pre_correction_world_cm"),
            VectorToJson(
                Capture.MetaHumanHandPreCorrectionWorldLocation));
        Json->SetObjectField(
            TEXT("metahuman_hand_r_post_correction_world_cm"),
            VectorToJson(
                Capture.MetaHumanHandPostCorrectionWorldLocation));
        Json->SetNumberField(TEXT("source_hand_r_to_disc_grip_r_cm"),
            Capture.SourceHandToDiscGripDistanceCm);
        Json->SetNumberField(
            TEXT("metahuman_hand_r_pre_to_source_hand_r_cm"),
            Capture.MetaHumanHandPreToSourceHandDistanceCm);
        Json->SetNumberField(
            TEXT("metahuman_hand_r_post_to_source_hand_r_cm"),
            Capture.MetaHumanHandPostToSourceHandDistanceCm);
        Json->SetNumberField(TEXT("source_bone_revision_at_pre_update"),
            Capture.SourceBoneRevisionAtPreUpdate);
        Json->SetNumberField(
            TEXT("target_bone_revision_before_evaluate"),
            Capture.TargetBoneRevisionBeforeEvaluate);
        Json->SetNumberField(TEXT("target_bone_revision_at_capture"),
            Capture.TargetBoneRevisionAtCapture);
        Json->SetNumberField(TEXT("source_sample_frame_counter"),
            Capture.SourceSampleFrameCounter);
        Json->SetNumberField(TEXT("target_correction_frame_counter"),
            Capture.TargetCorrectionFrameCounter);
        Json->SetNumberField(TEXT("release_callback_frame_counter"),
            Capture.ReleaseCallbackFrameCounter);
        Json->SetNumberField(TEXT("capture_freeze_frame_counter"),
            Capture.CaptureFreezeFrameCounter);
        Json->SetNumberField(TEXT("release_callback_event_order"),
            Capture.ReleaseCallbackEventOrder);
        Json->SetNumberField(TEXT("capture_freeze_event_order"),
            Capture.CaptureFreezeEventOrder);
        Json->SetBoolField(TEXT("creator_framing_evidence_collected"),
            Capture.bCreatorFramingEvidenceCollected);
        Json->SetBoolField(TEXT("creator_framing_accepted"),
            Capture.bCreatorFramingAccepted);
        Json->SetNumberField(TEXT("creator_semantic_point_count"),
            Capture.CreatorSemanticPointCount);
        Json->SetObjectField(TEXT("creator_semantic_min_px"),
            Vector2DToJson(Capture.CreatorSemanticMin));
        Json->SetObjectField(TEXT("creator_semantic_max_px"),
            Vector2DToJson(Capture.CreatorSemanticMax));
        Json->SetBoolField(TEXT("metahuman_render_evidence_collected"),
            Capture.bMetaHumanRenderEvidenceCollected);
        Json->SetBoolField(TEXT("metahuman_hair_render_evidence_collected"),
            Capture.bMetaHumanHairRenderEvidenceCollected);
        Json->SetBoolField(TEXT("metahuman_outfit_render_evidence_collected"),
            Capture.bMetaHumanOutfitRenderEvidenceCollected);
        Json->SetBoolField(
            TEXT("metahuman_outfit_live_skin_evidence_collected"),
            Capture.bMetaHumanOutfitLiveSkinEvidenceCollected);
        if (Capture.bMetaHumanRenderEvidenceCollected)
        {
            Json->SetBoolField(
                TEXT("metahuman_visual_root_uses_absolute_scale"),
                Capture.bMetaHumanVisualRootUsesAbsoluteScale);
            Json->SetObjectField(
                TEXT("metahuman_visual_root_world_scale"),
                VectorToJson(Capture.MetaHumanVisualRootWorldScale));
            Json->SetObjectField(
                TEXT("metahuman_body_world_scale"),
                VectorToJson(Capture.MetaHumanBodyWorldScale));
            Json->SetObjectField(
                TEXT("metahuman_head_world_scale"),
                VectorToJson(Capture.MetaHumanHeadWorldScale));
            Json->SetObjectField(
                TEXT("metahuman_outfit_world_scale"),
                VectorToJson(Capture.MetaHumanOutfitWorldScale));
            Json->SetObjectField(TEXT("metahuman_body_render"),
                MeshRenderEvidenceToJson(Capture.BodyRender));
            Json->SetObjectField(TEXT("metahuman_head_render"),
                MeshRenderEvidenceToJson(Capture.HeadRender));
            Json->SetObjectField(TEXT("metahuman_outfit_render"),
                MeshRenderEvidenceToJson(Capture.OutfitRender));
            Json->SetObjectField(TEXT("metahuman_hair_render"),
                PrimitiveRenderEvidenceToJson(Capture.HairRender));
        }
        if (Capture.bMetaHumanOutfitLiveSkinEvidenceCollected)
        {
            Json->SetObjectField(TEXT("metahuman_outfit_live_skin"),
                OutfitLiveSkinEvidenceToJson(Capture.OutfitLiveSkin));
        }
        if (Capture.PresentationPolicy.bCollected)
        {
            Json->SetObjectField(TEXT("metahuman_presentation_policy"),
                PresentationPolicyEvidenceToJson(
                    Capture.PresentationPolicy));
        }
        CaptureValues.Add(MakeShared<FJsonValueObject>(Json));
    }
    Root->SetArrayField(TEXT("captures"), CaptureValues);

    TArray<TSharedPtr<FJsonValue>> PerformanceValues;
    for (const FPerformanceRecord& Record : PerformanceRecords)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("segment"), Record.Segment);
        Json->SetStringField(TEXT("status"),
            DiscGolfPerformance::StateName(Record.State));
        Json->SetNumberField(TEXT("duration_seconds"),
            Record.DurationSeconds);
        Json->SetNumberField(TEXT("sample_count"),
            Record.Summary.SampleCount);
        Json->SetNumberField(TEXT("average_frame_ms"),
            Record.Summary.AverageFrameTimeMs);
        Json->SetNumberField(TEXT("p95_frame_ms"),
            Record.Summary.P95FrameTimeMs);
        Json->SetNumberField(TEXT("max_frame_ms"),
            Record.Summary.MaxFrameTimeMs);
        Json->SetNumberField(TEXT("hitch_count"),
            Record.Summary.HitchCount);
        Json->SetNumberField(TEXT("hitch_rate_percent"),
            Record.Summary.HitchRatePercent);
        Json->SetNumberField(TEXT("used_physical_bytes"),
            static_cast<double>(Record.Summary.UsedPhysicalBytes));
        Json->SetStringField(TEXT("sample_reduction"),
            TEXT("CONTIGUOUS_BUCKET_MAXIMA_FULL_SEGMENT"));
        Json->SetStringField(TEXT("timing_clock"),
            TEXT("FPlatformTime::Seconds_MONOTONIC"));
        Json->SetNumberField(TEXT("raw_frame_count"),
            Record.RawSummary.SampleCount);
        Json->SetNumberField(TEXT("raw_average_frame_ms"),
            Record.RawSummary.AverageFrameTimeMs);
        Json->SetNumberField(TEXT("raw_p95_frame_ms"),
            Record.RawSummary.P95FrameTimeMs);
        Json->SetNumberField(TEXT("raw_max_frame_ms"),
            Record.RawSummary.MaxFrameTimeMs);
        Json->SetNumberField(TEXT("raw_hitch_count"),
            Record.RawSummary.HitchCount);
        Json->SetNumberField(TEXT("raw_hitch_rate_percent"),
            Record.RawSummary.HitchRatePercent);
        Json->SetObjectField(
            TEXT("metahuman_presentation_policy_begin"),
            PresentationPolicyEvidenceToJson(
                Record.PresentationPolicyBegin));
        Json->SetObjectField(
            TEXT("metahuman_presentation_policy_end"),
            PresentationPolicyEvidenceToJson(
                Record.PresentationPolicyEnd));
        PerformanceValues.Add(MakeShared<FJsonValueObject>(Json));
    }
    Root->SetArrayField(TEXT("performance_segments"), PerformanceValues);

    TSharedRef<FJsonObject> Budget = MakeShared<FJsonObject>();
    Budget->SetNumberField(TEXT("accepted_p95_frame_ms"),
        AcceptedP95FrameMs);
    Budget->SetNumberField(TEXT("accepted_hitch_count"), 0);
    Budget->SetNumberField(TEXT("hitch_frame_ms"), HitchFrameMs);
    Budget->SetNumberField(TEXT("accepted_used_physical_bytes"),
        static_cast<double>(AcceptedUsedPhysicalBytes));
    Budget->SetNumberField(TEXT("hard_failure_p95_frame_ms"),
        HardFailureP95FrameMs);
    Budget->SetNumberField(TEXT("hard_failure_hitch_rate_percent"),
        HardFailureHitchRatePercent);
    Budget->SetNumberField(TEXT("hard_failure_used_physical_bytes"),
        static_cast<double>(HardFailureUsedPhysicalBytes));
    Budget->SetNumberField(TEXT("minimum_samples"), 120);
    Budget->SetNumberField(TEXT("maximum_samples"), 600);
    Budget->SetNumberField(TEXT("residency_warmup_seconds"),
        PerformanceWarmupSeconds);
    Budget->SetNumberField(TEXT("segment_seconds"),
        PerformanceSegmentSeconds);
    Root->SetObjectField(TEXT("performance_contract"), Budget);

    FString JsonText;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&JsonText);
    return FJsonSerializer::Serialize(Root, Writer)
        && FFileHelper::SaveStringToFile(
            JsonText,
            *ReportPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void ADiscGolfSession8BMetaHumanPackagedRunner::Fail(
    const FString& Code,
    const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    Step = EStep::Finished;
    ResumeAfterThrowCapture();
    if (GameMode)
    {
        GameMode->DiscardDeferredTrajectoryExport();
    }
    const FString SafeReason = SafeSingleLine(Reason);
    WritePhaseReport(TEXT("FAIL"), Code, SafeReason);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("%s: FAIL phase=%s code=%s reason=%s"),
        Marker, *PhaseName, *Code, *SafeReason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession8BMetaHumanPackagedRunner::Pass()
{
    if (bFinished)
    {
        return;
    }
    ResumeAfterThrowCapture();
    FString Error;
    const int32 ExpectedCaptureCount =
        Phase == EPhase::ApplyMetaHuman ? 3
        : Phase == EPhase::ReloadCancelFailureSwitchDG ? 2
        : Phase == EPhase::ReloadDGRestoreMetaHumanVisualThrow ? 4
        : 0;
    if (Captures.Num() != ExpectedCaptureCount
        || !VerifyOnlyIsolatedProfileSave(Error)
        || (Phase == EPhase::ReloadCancelFailureSwitchDG
            && (!bGameplayCandidateIsolationProbePassed
                || PresentationPolicyCandidateIsolationProbes.Num() != 1))
        || (Phase != EPhase::ReloadCancelFailureSwitchDG
            && !PresentationPolicyCandidateIsolationProbes.IsEmpty())
        || (Phase == EPhase::MetaHumanPerformance
            && (PerformanceRecords.Num() != 2
                || !bPresentationPolicyTransitionProbesPassed
                || PresentationPolicyTransitionProbes.Num() != 4
                || bHasPendingPerformancePolicyBegin
                || !bTrajectoryExportDeferredAtSettlement
                || !bTrajectorySummaryReadyBeforeFlush
                || !bTrajectoryExportFlushedAfterSegment
                || !bTrajectoryExportFlushPassed
                || DeferredTrajectoryCaptureId.IsEmpty()
                || FlushedTrajectoryCaptureId
                    != DeferredTrajectoryCaptureId
                || !GameMode
                || GameMode->IsTrajectoryExportDeferralArmed()
                || GameMode->HasPendingDeferredTrajectoryExport()))
        || (Phase != EPhase::MetaHumanPerformance
            && !PresentationPolicyTransitionProbes.IsEmpty()))
    {
        Fail(TEXT("FINAL_BOUNDARY"), Error.IsEmpty()
            ? TEXT("capture/performance evidence cardinality was incomplete")
            : Error);
        return;
    }
    const FString Status = SuccessStatus();
    if (!WritePhaseReport(Status, FString(), FString()))
    {
        Fail(TEXT("REPORT_WRITE"),
            TEXT("phase report could not be written inside the isolated UserDir"));
        return;
    }
    bFinished = true;
    Step = EStep::Finished;
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("%s: PASS phase=%s status=%s captures=%d performance_segments=%d fallback_used=0 release_authority=DiscGolfTourGameMode.RequestThrowFromGrip flight_authority=DiscActor.DiscFlightComponent dg_master_animation_source_preserved=1 metahuman_presentation_only=1 isolated_profile_save_only=1 manual_visual_review_required=1"),
        Marker,
        *PhaseName,
        *Status,
        Captures.Num(),
        PerformanceRecords.Num());
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
