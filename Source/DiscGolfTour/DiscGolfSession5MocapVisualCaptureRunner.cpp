#include "DiscGolfSession5MocapVisualCaptureRunner.h"

#include "AnimNotify_DiscRelease.h"
#include "AnimNotify_ThrowFinished.h"
#include "AnimNotify_ThrowPhase.h"
#include "DiscActor.h"
#include "DiscGolferPawn.h"
#include "DiscGolfCharacterProfile.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfSession5MocapValidationPaths.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "DiscGolfTourPlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/Skeleton.h"
#include "AnimationRuntime.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/HUD.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HighResScreenshot.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"

namespace DiscGolfSession5MocapVisual
{
constexpr double Session5PoseTimeoutSeconds = 8.0;
constexpr double Session5ScreenshotTimeoutSeconds = 30.0;
constexpr double Session5PoseSettleSeconds = 0.25;
constexpr int32 Session5ExpectedWidth = 1920;
constexpr int32 Session5ExpectedHeight = 1080;
constexpr int64 Session5MinimumReadablePngBytes = 32768;
constexpr float Session5MinimumBodyHeightCm = 80.0f;
constexpr float Session5MaximumBodyHeightCm = 260.0f;
constexpr float Session5MinimumScreenHeightFraction = 0.28f;
constexpr float Session5MaximumScreenHeightFraction = 0.86f;
constexpr float Session5MinimumScreenWidthFraction = 0.025f;
constexpr float Session5MaximumScreenWidthFraction = 0.90f;
constexpr float Session5MaximumBoneLengthRatioError = 0.15f;
constexpr float Session5ProfileFixtureSpacingCm = 155.0f;
constexpr float Session5GroupHorizontalFrameFill = 0.88f;
constexpr float Session5GroupVerticalFrameFill = 0.85f;
constexpr float Session5GroupCameraClearanceCm = 25.0f;
const TCHAR* Session5EvidenceMaterialPath =
    TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial");
const TCHAR* Session5ProfileBoneLengthPolicy =
    TEXT("ACCEPTED_DG_PROFILE_FACTORS_DERIVE_EXPECTED_SEGMENT_LENGTHS_THEN_REQUIRE_15_PERCENT_MAX_ERROR");

const TCHAR* Session5CaptureFilenames[] = {
    TEXT("01_Source_Motion_Reference_Stage.png"),
    TEXT("02_Raw_Retarget.png"),
    TEXT("03_Cleaned_Retarget.png"),
    TEXT("04_Plant_Brace.png"),
    TEXT("05_Reachback.png"),
    TEXT("06_Exact_Release.png"),
    TEXT("07_FollowThrough.png"),
    TEXT("08_ShortCompact_Result.png"),
    TEXT("09_Baseline_Result.png"),
    TEXT("10_TallLongArms_Result.png")
};

const TCHAR* Session5ProfilePaths[] = {
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact.DA_DG_Test_ShortCompact"),
    TEXT("/Game/DiscGolf/Characters/Profiles/DA_DG_DefaultCharacter.DA_DG_DefaultCharacter"),
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms.DA_DG_Test_TallLongArms")
};

const TCHAR* Session5ProfileNames[] = {
    TEXT("ShortCompact"),
    TEXT("Baseline"),
    TEXT("TallLongArms")
};

int32 ReadSession5BigEndianInt32(const uint8* Bytes)
{
    return (static_cast<int32>(Bytes[0]) << 24)
        | (static_cast<int32>(Bytes[1]) << 16)
        | (static_cast<int32>(Bytes[2]) << 8)
        | static_cast<int32>(Bytes[3]);
}

bool IsSession5FiniteVector(const FVector& Value)
{
    return !Value.ContainsNaN()
        && FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

bool Session5CommandsMatchExactly(const FThrowCommand& A, const FThrowCommand& B)
{
    return A.MoldId == B.MoldId
        && A.Plastic == B.Plastic
        && A.ThrowStyle == B.ThrowStyle
        && A.ShotContext == B.ShotContext
        && A.Direction == B.Direction
        && A.Power01 == B.Power01
        && A.HyzerDeg == B.HyzerDeg
        && A.NoseAngleDeg == B.NoseAngleDeg
        && A.LaunchAngleDeg == B.LaunchAngleDeg
        && A.TimingError == B.TimingError;
}

const TCHAR* Session5RecoveryReasonLabel(EDiscGolfRHBHThrowRecoveryReason Reason)
{
    switch (Reason)
    {
        case EDiscGolfRHBHThrowRecoveryReason::ThrowFinished:
            return TEXT("ThrowFinished");
        case EDiscGolfRHBHThrowRecoveryReason::ThrowFinishedBeforeRelease:
            return TEXT("ThrowFinishedBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::CancelledBeforeRelease:
            return TEXT("CancelledBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::InterruptedBeforeRelease:
            return TEXT("InterruptedBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::InterruptedAfterRelease:
            return TEXT("InterruptedAfterRelease");
        case EDiscGolfRHBHThrowRecoveryReason::WatchdogBeforeRelease:
            return TEXT("WatchdogBeforeRelease");
        case EDiscGolfRHBHThrowRecoveryReason::WatchdogAfterRelease:
            return TEXT("WatchdogAfterRelease");
        case EDiscGolfRHBHThrowRecoveryReason::None:
        default:
            return TEXT("None");
    }
}

struct FSession5ProfileFactors
{
    float Height = 1.0f;
    float Wingspan = 1.0f;
    float Shoulder = 1.0f;
    float Torso = 1.0f;
    float Leg = 1.0f;
    float Hand = 1.0f;
    float Arm = 1.0f;
    float Build = 1.0f;
    float Head = 1.0f;
};

float SanitizeSession5ProfileValue(
    const float Value,
    const float Fallback,
    const float Minimum,
    const float Maximum)
{
    return FMath::Clamp(FMath::IsFinite(Value) ? Value : Fallback, Minimum, Maximum);
}

FSession5ProfileFactors BuildSession5ProfileFactors(const FDGBodyProfile& Body)
{
    // Mirror the accepted FRigUnit_DGApplyCharacterProfile factors solely to
    // derive validation expectations. The runtime rig remains profile authority.
    constexpr float BaselineHeightCm = 183.0f;
    constexpr float BaselineMassKg = 82.0f;
    const float HeightCm = SanitizeSession5ProfileValue(
        Body.HeightCm, BaselineHeightCm, 150.0f, 210.0f);
    const float WingspanScale = SanitizeSession5ProfileValue(
        Body.WingspanScale, 1.0f, 0.92f, 1.08f);
    const float ShoulderScale = SanitizeSession5ProfileValue(
        Body.ShoulderWidthScale, 1.0f, 0.92f, 1.08f);
    const float TorsoScale = SanitizeSession5ProfileValue(
        Body.TorsoLengthScale, 1.0f, 0.94f, 1.06f);
    const float LegScale = SanitizeSession5ProfileValue(
        Body.LegLengthScale, 1.0f, 0.94f, 1.06f);
    const float HandScale = SanitizeSession5ProfileValue(
        Body.HandScale, 1.0f, 0.94f, 1.06f);
    const float MassKg = SanitizeSession5ProfileValue(
        Body.MassKg, BaselineMassKg, 45.0f, 160.0f);

    FSession5ProfileFactors Factors;
    Factors.Height = HeightCm / BaselineHeightCm;
    Factors.Wingspan = FMath::Clamp(Factors.Height * WingspanScale, 0.70f, 1.35f);
    Factors.Shoulder = FMath::Clamp(Factors.Height * ShoulderScale, 0.70f, 1.35f);
    Factors.Torso = FMath::Clamp(Factors.Height * TorsoScale, 0.70f, 1.35f);
    Factors.Leg = FMath::Clamp(Factors.Height * LegScale, 0.70f, 1.35f);
    Factors.Hand = FMath::Clamp(FMath::Sqrt(Factors.Height) * HandScale, 0.75f, 1.30f);
    Factors.Head = FMath::Clamp(FMath::Pow(Factors.Height, 0.25f), 0.90f, 1.10f);
    const float ExpectedMassAtHeight = BaselineMassKg * FMath::Square(Factors.Height);
    Factors.Build = FMath::Clamp(
        FMath::Pow(MassKg / FMath::Max(ExpectedMassAtHeight, 1.0f), 0.25f),
        0.88f,
        1.16f);
    constexpr float BaselineHalfSpanCm = 81.0f;
    constexpr float BaselineShoulderContributionCm = 19.0f;
    constexpr float BaselineLongArmContributionCm = 56.0f;
    constexpr float BaselineGripContributionCm = 6.0f;
    const float DesiredLongArmCm = BaselineHalfSpanCm * Factors.Wingspan
        - BaselineShoulderContributionCm * Factors.Shoulder
        - BaselineGripContributionCm * Factors.Hand;
    Factors.Arm = FMath::Clamp(
        DesiredLongArmCm / BaselineLongArmContributionCm, 0.70f, 1.35f);
    return Factors;
}

bool Session5BoneStartsWith(const FName BoneName, const TCHAR* Prefix)
{
    return BoneName.ToString().StartsWith(Prefix, ESearchCase::CaseSensitive);
}

float Session5ExpectedProfileSegmentScale(
    const FName BoneName,
    const FVector& ReferenceLocalOffset,
    const FDGBodyProfile& Body)
{
    const FSession5ProfileFactors Factors = BuildSession5ProfileFactors(Body);
    FVector ScaledOffset = ReferenceLocalOffset;
    if (BoneName == FName(TEXT("pelvis")))
    {
        ScaledOffset *= Factors.Leg;
    }
    else if (Session5BoneStartsWith(BoneName, TEXT("spine_"))
        || BoneName == FName(TEXT("neck_01")))
    {
        ScaledOffset *= Factors.Torso;
    }
    else if (BoneName == FName(TEXT("head")))
    {
        ScaledOffset *= Factors.Head;
    }
    else if (Session5BoneStartsWith(BoneName, TEXT("clavicle_"))
        || (Session5BoneStartsWith(BoneName, TEXT("upperarm_"))
            && !Session5BoneStartsWith(BoneName, TEXT("upperarm_twist_"))))
    {
        ScaledOffset *= Factors.Shoulder;
    }
    else if (Session5BoneStartsWith(BoneName, TEXT("lowerarm_"))
        || Session5BoneStartsWith(BoneName, TEXT("hand_"))
        || Session5BoneStartsWith(BoneName, TEXT("upperarm_twist_")))
    {
        ScaledOffset *= Factors.Arm;
    }
    else if (Session5BoneStartsWith(BoneName, TEXT("thigh_"))
        && !Session5BoneStartsWith(BoneName, TEXT("thigh_twist_")))
    {
        ScaledOffset = FVector(
            ReferenceLocalOffset.X * Factors.Build,
            ReferenceLocalOffset.Y * Factors.Build,
            ReferenceLocalOffset.Z * Factors.Leg);
    }
    else if (Session5BoneStartsWith(BoneName, TEXT("calf_"))
        || Session5BoneStartsWith(BoneName, TEXT("foot_"))
        || Session5BoneStartsWith(BoneName, TEXT("thigh_twist_")))
    {
        ScaledOffset *= Factors.Leg;
    }
    const float ReferenceLength = ReferenceLocalOffset.Size();
    return ReferenceLength > UE_SMALL_NUMBER
        ? ScaledOffset.Size() / ReferenceLength
        : 1.0f;
}
}

using namespace DiscGolfSession5MocapVisual;

ADiscGolfSession5MocapVisualCaptureRunner::ADiscGolfSession5MocapVisualCaptureRunner()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}

void ADiscGolfSession5MocapVisualCaptureRunner::Start()
{
    if (bStarted || bFinished)
    {
        return;
    }
    bStarted = true;

    GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
    PlayerController = Cast<ADiscGolfTourPlayerController>(
        UGameplayStatics::GetPlayerController(this, 0));
    PossessedGolfer = Cast<ADiscGolferPawn>(
        UGameplayStatics::GetPlayerPawn(this, 0));
    OutputDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("CharacterFramework/Screenshots/Session5_MocapPipeline"));
    ManifestPath = FPaths::Combine(
        OutputDirectory,
        TEXT("Session5_MocapPipeline_CaptureManifest.json"));

    if (!DiscGolfSession5MocapValidation::IsPipelineRuntimeValidationRequested()
        || !GameMode || !PlayerController || !PossessedGolfer
        || !PossessedGolfer->IsSession5PipelineValidationMontageActive())
    {
        Fail(TEXT("explicit unattended Session 5 visual authority or pipeline montage override was unavailable"));
        return;
    }

    IFileManager::Get().MakeDirectory(*OutputDirectory, true);
    for (const TCHAR* Filename : Session5CaptureFilenames)
    {
        IFileManager::Get().Delete(
            *FPaths::Combine(OutputDirectory, Filename), false, true, true);
    }
    IFileManager::Get().Delete(*ManifestPath, false, true, true);
    CaptureRecords.SetNum(UE_ARRAY_COUNT(Session5CaptureFilenames));

    SnapshotPersistentFiles(FPaths::ProjectContentDir(), true, InitialPackageFiles);
    SnapshotPersistentFiles(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")),
        false,
        InitialSaveGameFiles);

    GameMode->SkipCurrentPresentation();
    BaselineWorldDiscCount = CountWorldDiscs();
    BaselineStrokes = GameMode->GetStrokes();
    InitialViewTarget = PlayerController->GetViewTarget();
    bInitialGolferHidden = PossessedGolfer->IsHidden();
    PossessedGolfer->SetActorHiddenInGame(true);
    if (AHUD* Hud = PlayerController->GetHUD())
    {
        bInitialHudVisible = Hud->bShowHUD;
        Hud->bShowHUD = false;
    }

    StageTransform = PossessedGolfer->GetActorTransform();
    StageTransform.AddToTranslation(FVector(0.0f, 0.0f, 10000.0f));
    if (!PreflightAssets()
        || !ResolveMontageContract()
        || !CreateCaptureStage()
        || !PrepareSequenceStage(
            SourceSequence,
            SourceMesh,
            FMath::Clamp(ReleaseSeconds / PipelineMontage->GetPlayLength(), 0.05f, 0.95f)))
    {
        if (!bFinished)
        {
            Fail(TEXT("Session 5 visual preflight or source-reference stage failed"));
        }
        return;
    }

    SetStage(EStage::WaitingForSource);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION5_MOCAP_VISUAL_CAPTURE: START synthetic_test_only=1 output=%s"),
        *OutputDirectory);
}

void ADiscGolfSession5MocapVisualCaptureRunner::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bStarted || bFinished)
    {
        return;
    }
    if (!GameMode || !PlayerController || !PossessedGolfer)
    {
        Fail(TEXT("runtime authority disappeared during capture"));
        return;
    }
    if (!ValidateNoGameplayMutation())
    {
        Fail(TEXT("a visual fixture changed gameplay disc or stroke authority"));
        return;
    }
    if (FixtureReleaseCallbackCount > ProfilePawns.Num())
    {
        Fail(TEXT("visual fixtures emitted more than one release callback each"));
        return;
    }
    if (bScreenshotPending)
    {
        if (CaptureCamera)
        {
            PlayerController->SetViewTarget(CaptureCamera);
            if (PlayerController->PlayerCameraManager)
            {
                PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
            }
        }
        PollPendingCapture();
        return;
    }

    switch (Stage)
    {
        case EStage::WaitingForSource:
            if (SecondsInStage() >= Session5PoseSettleSeconds)
            {
                PositionCameraForSequence();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | SYNTHETIC SOURCE MOTION"),
                    TEXT("PIPELINE VALIDATION ONLY | DO_NOT_SHIP | RAW SOURCE PRESERVED"));
                RequestCapture(0,
                    TEXT("Explicitly synthetic source-motion/reference stage on its source skeleton."),
                    TEXT("Source / RAW / SYNTHETIC_TEST"),
                    EStage::WaitingForRetargeted,
                    1);
            }
            break;

        case EStage::WaitingForRetargeted:
            if (SecondsInStage() >= Session5PoseSettleSeconds)
            {
                PositionCameraForSequence();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | RAW IK RETARGET"),
                    TEXT("SOURCE IK -> RETARGETER -> SKEL_DG_MASTER | NO CLEANUP BAKED INTO SOURCE"));
                RequestCapture(1,
                    TEXT("Raw IK-retarget result on the frozen DG master skeleton."),
                    TEXT("Retargeted / RAW_RTG"),
                    EStage::WaitingForCleaned,
                    1);
            }
            break;

        case EStage::WaitingForCleaned:
            if (SecondsInStage() >= Session5PoseSettleSeconds)
            {
                PositionCameraForSequence();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | NON-DESTRUCTIVE CLEANUP"),
                    TEXT("DISTINCT CLEANED ASSET | CR_DG_MASTER COMPATIBLE | SOURCE + RTG RETAINED"));
                RequestCapture(2,
                    TEXT("Distinct cleaned retarget stage; source and raw retarget remain separate assets."),
                    TEXT("Cleaned / CLN"),
                    EStage::WaitingForReachback,
                    1);
            }
            break;

        case EStage::WaitingForReachback:
            if (MinimumMontagePosition() >= ReachbackSeconds)
            {
                PauseProfileMontages(true);
                ShowAllProfiles();
                PositionCameraForProfiles();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | REACHBACK"),
                    TEXT("PIPELINE MONTAGE | SHORT + BASELINE + TALL | SAME MASTER SKELETON"));
                RequestCapture(4,
                    TEXT("Pipeline montage reachback across all three accepted body profiles."),
                    TEXT("ReachBack"),
                    EStage::WaitingForPlant,
                    3);
            }
            else if (SecondsInStage() > Session5PoseTimeoutSeconds)
            {
                Fail(TEXT("pipeline montage did not reach ReachBack"));
            }
            break;

        case EStage::WaitingForPlant:
            if (MinimumMontagePosition() >= PlantSeconds)
            {
                PauseProfileMontages(true);
                ShowAllProfiles();
                PositionCameraForProfiles();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | PLANT / BRACE"),
                    TEXT("CLEANUP LAYER COMPATIBILITY | FINITE FEET / KNEES / SPINE"));
                RequestCapture(3,
                    TEXT("Plant/brace phase across ShortCompact, Baseline, and TallLongArms."),
                    TEXT("Plant / Brace"),
                    EStage::WaitingForRelease,
                    3);
            }
            else if (SecondsInStage() > Session5PoseTimeoutSeconds)
            {
                Fail(TEXT("pipeline montage did not reach Plant"));
            }
            break;

        case EStage::WaitingForRelease:
            if (MinimumMontagePosition() >= ReleaseSeconds
                && FixtureReleaseCallbackCount == ProfilePawns.Num())
            {
                PauseProfileMontages(true);
                ShowAllProfiles();
                PositionCameraForProfiles();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | EXACT DG RELEASE DISC"),
                    TEXT("3 PROFILES | 1 VALIDATION CALLBACK EACH | 0 GAMEPLAY DISCS / 0 STROKES"));
                RequestCapture(5,
                    TEXT("Exact release evidence after one validation-only cached-command callback per profile."),
                    TEXT("DG Release Disc"),
                    EStage::WaitingForFollowThrough,
                    3);
            }
            else if (SecondsInStage() > Session5PoseTimeoutSeconds)
            {
                Fail(TEXT("pipeline montage did not reach one exact release per profile"));
            }
            break;

        case EStage::WaitingForFollowThrough:
            if (MinimumMontagePosition() >= FollowThroughSeconds)
            {
                PauseProfileMontages(true);
                ShowAllProfiles();
                PositionCameraForProfiles();
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | FOLLOW-THROUGH"),
                    TEXT("PIPELINE MONTAGE | RELEASE COUNTS REMAIN EXACTLY ONE"));
                RequestCapture(6,
                    TEXT("Stable follow-through across all three accepted body profiles."),
                    TEXT("FollowThrough"),
                    EStage::WaitingForShortProfile,
                    3);
            }
            else if (SecondsInStage() > Session5PoseTimeoutSeconds)
            {
                Fail(TEXT("pipeline montage did not reach FollowThrough"));
            }
            break;

        case EStage::WaitingForShortProfile:
            if (SecondsInStage() >= Session5PoseSettleSeconds)
            {
                SetOnlyProfileVisible(0);
                PositionCameraForProfiles(true, 0);
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | SHORT COMPACT RESULT"),
                    TEXT("FINITE PIPELINE POSE | USABLE THROWING HAND | RELEASE ONCE"));
                RequestCapture(7,
                    TEXT("ShortCompact pipeline-output compatibility result."),
                    TEXT("ShortCompact"),
                    EStage::WaitingForBaselineProfile,
                    1);
            }
            break;

        case EStage::WaitingForBaselineProfile:
            if (SecondsInStage() >= Session5PoseSettleSeconds)
            {
                SetOnlyProfileVisible(1);
                PositionCameraForProfiles(true, 1);
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | BASELINE RESULT"),
                    TEXT("FINITE PIPELINE POSE | USABLE THROWING HAND | RELEASE ONCE"));
                RequestCapture(8,
                    TEXT("Baseline pipeline-output compatibility result."),
                    TEXT("Baseline"),
                    EStage::WaitingForTallProfile,
                    1);
            }
            break;

        case EStage::WaitingForTallProfile:
            if (SecondsInStage() >= Session5PoseSettleSeconds)
            {
                SetOnlyProfileVisible(2);
                PositionCameraForProfiles(true, 2);
                ShowEvidenceLabel(
                    TEXT("SESSION 5 | TALL / LONG ARMS RESULT"),
                    TEXT("FINITE PIPELINE POSE | USABLE THROWING HAND | RELEASE ONCE"));
                RequestCapture(9,
                    TEXT("TallLongArms pipeline-output compatibility result."),
                    TEXT("TallLongArms"),
                    EStage::WaitingForRecovery,
                    1);
            }
            break;

        case EStage::WaitingForRecovery:
        {
            bool bAllRecovered = true;
            bool bAllThrowFinished = true;
            for (const UDiscGolfRHBHThrowAdapterComponent* Adapter : ProfileAdapters)
            {
                bAllRecovered &= Adapter && !Adapter->IsThrowActive();
                bAllThrowFinished &= Adapter
                    && Adapter->GetRecoveryReason()
                        == EDiscGolfRHBHThrowRecoveryReason::ThrowFinished;
            }
            if (bAllRecovered)
            {
                if (bAllThrowFinished)
                {
                    Pass();
                }
                else
                {
                    Fail(TEXT("a profile recovered for a reason other than DG Throw Finished"));
                }
            }
            else if (SecondsInStage() > Session5PoseTimeoutSeconds)
            {
                Fail(TEXT("profile montages did not reach DG Throw Finished recovery"));
            }
            break;
        }

        case EStage::WaitingForRuntime:
        case EStage::Capturing:
        case EStage::Finished:
        default:
            break;
    }
}

void ADiscGolfSession5MocapVisualCaptureRunner::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    RestoreRuntimeState();
    Super::EndPlay(EndPlayReason);
}

void ADiscGolfSession5MocapVisualCaptureRunner::SetStage(EStage NewStage)
{
    Stage = NewStage;
    StageStartSeconds = FPlatformTime::Seconds();
}

double ADiscGolfSession5MocapVisualCaptureRunner::SecondsInStage() const
{
    return FPlatformTime::Seconds() - StageStartSeconds;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::PreflightAssets()
{
    SourceSequence = LoadObject<UAnimSequence>(
        nullptr, DiscGolfSession5MocapValidation::SourceSequence);
    RetargetedSequence = LoadObject<UAnimSequence>(
        nullptr, DiscGolfSession5MocapValidation::RetargetedSequence);
    CleanedSequence = LoadObject<UAnimSequence>(
        nullptr, DiscGolfSession5MocapValidation::CleanedSequence);
    ProductionSequence = LoadObject<UAnimSequence>(
        nullptr, DiscGolfSession5MocapValidation::ProductionSequence);
    PipelineMontage = LoadObject<UAnimMontage>(
        nullptr, DiscGolfSession5MocapValidation::PipelineTestMontage);
    SourceMesh = LoadObject<USkeletalMesh>(
        nullptr, DiscGolfSession5MocapValidation::SourceSkeletalMesh);
    TargetMesh = LoadObject<USkeletalMesh>(
        nullptr, DiscGolfSession5MocapValidation::TargetSkeletalMesh);

    UObject* Assets[] = {
        SourceSequence,
        RetargetedSequence,
        CleanedSequence,
        ProductionSequence,
        PipelineMontage,
        SourceMesh,
        TargetMesh
    };
    for (UObject* Asset : Assets)
    {
        if (!Asset)
        {
            return false;
        }
        RequiredAssets.Add(Asset);
    }
    for (const TCHAR* ProfilePath : Session5ProfilePaths)
    {
        UDiscGolfCharacterProfile* Profile =
            LoadObject<UDiscGolfCharacterProfile>(nullptr, ProfilePath);
        if (!Profile)
        {
            return false;
        }
        RequiredAssets.Add(Profile);
    }

    return SourceSequence->GetSkeleton() == SourceMesh->GetSkeleton()
        && RetargetedSequence->GetSkeleton() == TargetMesh->GetSkeleton()
        && CleanedSequence->GetSkeleton() == TargetMesh->GetSkeleton()
        && ProductionSequence->GetSkeleton() == TargetMesh->GetSkeleton()
        && PipelineMontage->GetSkeleton() == TargetMesh->GetSkeleton();
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ResolveMontageContract()
{
    ReleaseNotifyCount = 0;
    FinishNotifyCount = 0;
    ReachbackSeconds = PlantSeconds = ReleaseSeconds = FollowThroughSeconds = RecoverySeconds = -1.0f;
    int32 RequiredPhaseCount = 0;
    TSet<EDGThrowPhase> SeenPhases;

    for (const FAnimNotifyEvent& Event : PipelineMontage->Notifies)
    {
        const float Time = Event.GetTriggerTime();
        if (const UAnimNotify_ThrowPhase* Phase = Cast<UAnimNotify_ThrowPhase>(Event.Notify))
        {
            SeenPhases.Add(Phase->Phase);
            switch (Phase->Phase)
            {
                case EDGThrowPhase::ReachBack: ReachbackSeconds = Time; break;
                case EDGThrowPhase::Plant: PlantSeconds = Time; break;
                case EDGThrowPhase::FollowThrough: FollowThroughSeconds = Time; break;
                case EDGThrowPhase::Recovery: RecoverySeconds = Time; break;
                default: break;
            }
        }
        else if (Cast<UAnimNotify_DiscRelease>(Event.Notify))
        {
            ++ReleaseNotifyCount;
            ReleaseSeconds = Time;
        }
        else if (Cast<UAnimNotify_ThrowFinished>(Event.Notify))
        {
            ++FinishNotifyCount;
        }
    }

    const EDGThrowPhase RequiredPhases[] = {
        EDGThrowPhase::Aim,
        EDGThrowPhase::RunUp,
        EDGThrowPhase::ReachBack,
        EDGThrowPhase::Plant,
        EDGThrowPhase::Acceleration,
        EDGThrowPhase::FollowThrough,
        EDGThrowPhase::Recovery
    };
    for (EDGThrowPhase Phase : RequiredPhases)
    {
        RequiredPhaseCount += SeenPhases.Contains(Phase) ? 1 : 0;
    }

    const bool bProductionSegment = PipelineMontage->SlotAnimTracks.Num() == 1
        && PipelineMontage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() == 1
        && PipelineMontage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()
            == ProductionSequence;
    return bProductionSegment
        && ReleaseNotifyCount == 1
        && FinishNotifyCount == 1
        && RequiredPhaseCount == UE_ARRAY_COUNT(RequiredPhases)
        && ReachbackSeconds >= 0.0f
        && PlantSeconds > ReachbackSeconds
        && ReleaseSeconds > PlantSeconds
        && FollowThroughSeconds > ReleaseSeconds
        && RecoverySeconds > FollowThroughSeconds;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::CreateCaptureStage()
{
    CaptureCamera = GetWorld()->SpawnActor<ACameraActor>();
    if (!CaptureCamera)
    {
        return false;
    }
    CaptureCamera->SetActorEnableCollision(false);

    const auto CreateLight = [this](
        const TCHAR* Name,
        const FVector& RelativeLocation,
        float Intensity,
        const FColor& Color)
    {
        UPointLightComponent* Light = NewObject<UPointLightComponent>(CaptureCamera, Name);
        if (!Light)
        {
            return static_cast<UPointLightComponent*>(nullptr);
        }
        CaptureCamera->AddInstanceComponent(Light);
        Light->SetupAttachment(CaptureCamera->GetRootComponent());
        Light->SetRelativeLocation(RelativeLocation);
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensityUnits(ELightUnits::Lumens);
        Light->SetIntensity(Intensity);
        Light->SetAttenuationRadius(3000.0f);
        Light->SetLightColor(Color);
        Light->SetCastShadows(false);
        Light->SetVolumetricScatteringIntensity(0.0f);
        Light->RegisterComponent();
        return Light;
    };

    KeyLight = CreateLight(
        TEXT("Session5MocapCaptureKey"),
        FVector(250.0f, -220.0f, 250.0f),
        5000.0f,
        FColor(255, 244, 226));
    FillLight = CreateLight(
        TEXT("Session5MocapCaptureFill"),
        FVector(120.0f, 240.0f, 100.0f),
        1600.0f,
        FColor(205, 224, 255));
    return KeyLight && FillLight;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::PrepareSequenceStage(
    UAnimSequence* Sequence,
    USkeletalMesh* Mesh,
    float NormalizedTime)
{
    DestroySequenceStage();
    if (!Sequence || !Mesh || Sequence->GetSkeleton() != Mesh->GetSkeleton())
    {
        return false;
    }

    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    StageActor = GetWorld()->SpawnActor<ASkeletalMeshActor>(
        ASkeletalMeshActor::StaticClass(), StageTransform, Parameters);
    USkeletalMeshComponent* MeshComponent = StageActor
        ? StageActor->GetSkeletalMeshComponent() : nullptr;
    if (!MeshComponent)
    {
        DestroySequenceStage();
        return false;
    }

    StageActor->SetActorEnableCollision(false);
    MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MeshComponent->SetSkeletalMeshAsset(Mesh);
    MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
    MeshComponent->SetAnimation(Sequence);
    MeshComponent->Play(false);
    MeshComponent->SetPosition(
        Sequence->GetPlayLength() * FMath::Clamp(NormalizedTime, 0.0f, 1.0f),
        false);
    MeshComponent->SetPlayRate(0.0f);
    MeshComponent->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    RefreshMeshForCapture(MeshComponent);
    ApplyEvidenceMaterial(MeshComponent);
    return true;
}

void ADiscGolfSession5MocapVisualCaptureRunner::DestroySequenceStage()
{
    if (IsValid(StageActor))
    {
        StageActor->Destroy();
    }
    StageActor = nullptr;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::SpawnProfileFixtures()
{
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Session5ProfilePaths); ++Index)
    {
        const float Centered = static_cast<float>(Index) - 1.0f;
        FTransform Transform = StageTransform;
        Transform.AddToTranslation(
            StageTransform.GetRotation().GetRightVector()
                * (Centered * Session5ProfileFixtureSpacingCm));
        if (!SpawnProfileFixture(Index, Transform))
        {
            return false;
        }
    }
    return ProfilePawns.Num() == UE_ARRAY_COUNT(Session5ProfilePaths);
}

bool ADiscGolfSession5MocapVisualCaptureRunner::SpawnProfileFixture(
    int32 ProfileIndex,
    const FTransform& Transform)
{
    UDiscGolfCharacterProfile* Profile = LoadObject<UDiscGolfCharacterProfile>(
        nullptr, Session5ProfilePaths[ProfileIndex]);
    if (!Profile)
    {
        return false;
    }

    FActorSpawnParameters Parameters;
    Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ADiscGolferPawn* Pawn = GetWorld()->SpawnActor<ADiscGolferPawn>(
        ADiscGolferPawn::StaticClass(), Transform, Parameters);
    UDiscGolfRHBHThrowAdapterComponent* Adapter =
        Pawn ? Pawn->GetRHBHThrowAdapter() : nullptr;
    USkeletalMeshComponent* Mesh = Pawn ? Pawn->GetSkeletalGolferMesh() : nullptr;
    if (!Pawn || !Adapter || !Mesh
        || !Pawn->IsSession5PipelineValidationMontageActive()
        || Pawn->GetActiveRHBHThrowMontagePath()
            != DiscGolfSession5MocapValidation::PipelineTestMontage
        || !Pawn->PreviewCharacterCreatorProfile(
            Profile->Body, Profile->ThrowStyle, Profile->Handedness))
    {
        if (Pawn)
        {
            Pawn->Destroy();
        }
        return false;
    }

    Pawn->SetActorEnableCollision(false);
    Adapter->GetAuthoritativeLaunchDelegate().Unbind();
    Adapter->GetAuthoritativeLaunchDelegate().BindUObject(
        this,
        &ADiscGolfSession5MocapVisualCaptureRunner::HandleFixtureRelease);
    ApplyEvidenceMaterial(Mesh);
    ProfilePawns.Add(Pawn);
    ProfileAdapters.Add(Adapter);
    ProfileMeshes.Add(Mesh);
    ProfileBodies.Add(Profile->Body);
    return true;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::BeginProfileThrows()
{
    FixtureReleaseCallbackCount = 0;
    FixtureCommand.MoldId = TEXT("Session5MocapVisualSyntheticTest");
    FixtureCommand.Plastic = EDiscPlastic::Tour;
    FixtureCommand.ThrowStyle = EThrowStyle::Backhand;
    FixtureCommand.ShotContext = EDiscShotContext::Drive;
    FixtureCommand.Direction = StageTransform.GetRotation().GetForwardVector();
    FixtureCommand.Power01 = 0.72f;
    FixtureCommand.HyzerDeg = 3.0f;
    FixtureCommand.NoseAngleDeg = -1.5f;
    FixtureCommand.LaunchAngleDeg = 8.0f;
    FixtureCommand.TimingError = 0.0f;

    for (int32 Index = 0; Index < ProfilePawns.Num(); ++Index)
    {
        if (!ProfilePawns[Index]
            || !ProfileAdapters[Index]
            || !ProfilePawns[Index]->TryStartAnimatedRHBHThrow(FixtureCommand)
            || !ProfileAdapters[Index]->IsThrowActive()
            || !Session5CommandsMatchExactly(
                ProfileAdapters[Index]->GetLastAuthoritativeCommand(), FixtureCommand))
        {
            return false;
        }
    }
    return true;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::HandleFixtureRelease(
    const FThrowCommand& AuthoritativeCommand,
    const FTransform& GripWorldTransform)
{
    if (!Session5CommandsMatchExactly(AuthoritativeCommand, FixtureCommand)
        || !GripWorldTransform.IsValid()
        || !IsSession5FiniteVector(GripWorldTransform.GetLocation())
        || !ValidateNoGameplayMutation())
    {
        return false;
    }
    ++FixtureReleaseCallbackCount;
    return true;
}

float ADiscGolfSession5MocapVisualCaptureRunner::MinimumMontagePosition() const
{
    float Minimum = TNumericLimits<float>::Max();
    for (const USkeletalMeshComponent* Mesh : ProfileMeshes)
    {
        const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
        if (!Anim || !PipelineMontage)
        {
            return -1.0f;
        }
        Minimum = FMath::Min(Minimum, Anim->Montage_GetPosition(PipelineMontage));
    }
    return Minimum == TNumericLimits<float>::Max() ? -1.0f : Minimum;
}

void ADiscGolfSession5MocapVisualCaptureRunner::PauseProfileMontages(bool bPause) const
{
    for (USkeletalMeshComponent* Mesh : ProfileMeshes)
    {
        if (UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr)
        {
            if (bPause)
            {
                Anim->Montage_Pause(PipelineMontage);
            }
            else
            {
                Anim->Montage_Resume(PipelineMontage);
            }
        }
    }
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ProfilePosesFinite() const
{
    for (const USkeletalMeshComponent* Mesh : ProfileMeshes)
    {
        if (!Mesh || !IsSession5FiniteVector(Mesh->Bounds.Origin)
            || !IsSession5FiniteVector(Mesh->Bounds.BoxExtent)
            || Mesh->Bounds.BoxExtent.IsNearlyZero()
            || Mesh->Bounds.BoxExtent.GetMax() > 300.0f
            || Mesh->Bounds.BoxExtent.Z < 45.0f)
        {
            return false;
        }
        const FName RequiredBones[] = {
            TEXT("pelvis"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("disc_grip_r")
        };
        for (FName Bone : RequiredBones)
        {
            const int32 BoneIndex = Mesh->GetBoneIndex(Bone);
            if (BoneIndex == INDEX_NONE
                || !IsSession5FiniteVector(Mesh->GetBoneTransform(BoneIndex).GetLocation()))
            {
                return false;
            }
        }
    }
    return !ProfileMeshes.IsEmpty();
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ValidateHeldDiscState(
    bool bExpectedVisible) const
{
    for (int32 Index = 0; Index < ProfilePawns.Num(); ++Index)
    {
        const ADiscGolferPawn* Pawn = ProfilePawns[Index];
        const USkeletalMeshComponent* Mesh = ProfileMeshes[Index];
        const UStaticMeshComponent* HeldDisc = Pawn ? Pawn->GetHeldDiscVisual() : nullptr;
        if (!Pawn || !Mesh || !HeldDisc)
        {
            return false;
        }
        const bool bVisible = HeldDisc->IsVisible() && !HeldDisc->bHiddenInGame;
        const FVector Scale = HeldDisc->GetRelativeScale3D();
        const FTransform Grip = Mesh->GetSocketTransform(TEXT("disc_grip_r"), RTS_World);
        const float GripDistance = FVector::Dist(
            Grip.GetLocation(), HeldDisc->GetComponentLocation());
        if (bVisible != bExpectedVisible
            || !IsSession5FiniteVector(Scale)
            || Scale.GetMin() <= 0.0f
            || !Scale.Equals(FVector(0.21f, 0.21f, 0.015f), 0.002f)
            || HeldDisc->GetAttachSocketName() != FName(TEXT("disc_grip_r"))
            || !Grip.IsValid()
            || !IsSession5FiniteVector(Grip.GetLocation())
            || (bExpectedVisible && GripDistance > 2.5f))
        {
            return false;
        }
    }
    return !ProfilePawns.IsEmpty();
}

void ADiscGolfSession5MocapVisualCaptureRunner::SetOnlyProfileVisible(
    int32 VisibleIndex) const
{
    for (int32 Index = 0; Index < ProfilePawns.Num(); ++Index)
    {
        if (ProfilePawns[Index])
        {
            ProfilePawns[Index]->SetActorHiddenInGame(Index != VisibleIndex);
        }
    }
}

void ADiscGolfSession5MocapVisualCaptureRunner::ShowAllProfiles() const
{
    for (ADiscGolferPawn* Pawn : ProfilePawns)
    {
        if (Pawn)
        {
            Pawn->SetActorHiddenInGame(false);
        }
    }
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ValidateNoGameplayMutation() const
{
    return GameMode
        && CountWorldDiscs() == BaselineWorldDiscCount
        && GameMode->GetStrokes() == BaselineStrokes;
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ValidateProfileReleaseCounts() const
{
    if (FixtureReleaseCallbackCount != ProfileAdapters.Num())
    {
        return false;
    }
    for (const UDiscGolfRHBHThrowAdapterComponent* Adapter : ProfileAdapters)
    {
        if (!Adapter
            || !Adapter->HasCommittedRelease()
            || Adapter->GetReleaseCommitCountForAttempt() != 1)
        {
            return false;
        }
    }
    return true;
}

void ADiscGolfSession5MocapVisualCaptureRunner::RefreshMeshForCapture(
    USkeletalMeshComponent* Mesh) const
{
    if (!Mesh)
    {
        return;
    }
    // A screenshot may be requested in the same tick that a single-node pose is
    // selected or a montage is paused.  Force both the game-thread pose and its
    // render data current before deriving the evidence camera from key bones.
    Mesh->VisibilityBasedAnimTickOption =
        EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    Mesh->TickAnimation(0.0f, false);
    Mesh->RefreshBoneTransforms();
    Mesh->UpdateComponentToWorld();
    Mesh->UpdateBounds();
    Mesh->MarkRenderDynamicDataDirty();
    Mesh->MarkRenderStateDirty();
}

bool ADiscGolfSession5MocapVisualCaptureRunner::CollectSubjectKeyPoints(
    const USkeletalMeshComponent* Mesh,
    TArray<FVector>& OutPoints) const
{
    OutPoints.Reset();
    if (!Mesh || !Mesh->GetSkeletalMeshAsset())
    {
        return false;
    }
    // Stable order: head, feet, hands, pelvis.  The same points drive both the
    // camera and the gate, so stale/conservative component bounds cannot pass a
    // shot whose evaluated body is microscopic or outside the viewport.
    static const FName RequiredPoints[] = {
        TEXT("head"), TEXT("foot_l"), TEXT("foot_r"),
        TEXT("hand_l"), TEXT("hand_r"), TEXT("pelvis")
    };
    OutPoints.Reserve(UE_ARRAY_COUNT(RequiredPoints));
    for (const FName BoneName : RequiredPoints)
    {
        if (Mesh->GetBoneIndex(BoneName) == INDEX_NONE)
        {
            OutPoints.Reset();
            return false;
        }
        const FVector Location = Mesh->GetBoneLocation(
            BoneName, EBoneSpaces::WorldSpace);
        if (!IsSession5FiniteVector(Location))
        {
            OutPoints.Reset();
            return false;
        }
        OutPoints.Add(Location);
    }
    return OutPoints.Num() == UE_ARRAY_COUNT(RequiredPoints);
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ValidateBoneLengthInvariant(
    const USkeletalMeshComponent* Mesh,
    const FDGBodyProfile* ExpectedProfile,
    float& OutMaxRatioError) const
{
    OutMaxRatioError = 0.0f;
    const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
    if (!Asset)
    {
        return false;
    }

    const FReferenceSkeleton& RefSkeleton = Asset->GetRefSkeleton();
    TArray<FTransform> ReferenceComponentTransforms;
    FAnimationRuntime::FillUpComponentSpaceTransforms(
        RefSkeleton,
        RefSkeleton.GetRefBonePose(),
        ReferenceComponentTransforms);
    if (ReferenceComponentTransforms.Num() != RefSkeleton.GetNum())
    {
        return false;
    }

    static const FName RequiredBones[] = {
        TEXT("pelvis"),
        TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("neck_01"), TEXT("head"),
        TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
        TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("calf_l"), TEXT("foot_l"),
        TEXT("thigh_r"), TEXT("calf_r"), TEXT("foot_r")
    };
    int32 ComparedSegments = 0;
    for (const FName BoneName : RequiredBones)
    {
        const int32 BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
        if (BoneIndex <= 0)
        {
            return false;
        }
        const int32 ParentIndex = RefSkeleton.GetParentIndex(BoneIndex);
        if (ParentIndex == INDEX_NONE)
        {
            return false;
        }
        const FVector ReferenceChild = Mesh->GetComponentTransform().TransformPosition(
            ReferenceComponentTransforms[BoneIndex].GetTranslation());
        const FVector ReferenceParent = Mesh->GetComponentTransform().TransformPosition(
            ReferenceComponentTransforms[ParentIndex].GetTranslation());
        float ExpectedLength = FVector::Dist(ReferenceChild, ReferenceParent);
        if (ExpectedProfile)
        {
            ExpectedLength *= Session5ExpectedProfileSegmentScale(
                BoneName,
                RefSkeleton.GetRefBonePose()[BoneIndex].GetTranslation(),
                *ExpectedProfile);
        }
        if (ExpectedLength < 0.5f)
        {
            continue;
        }
        const FName ParentName = RefSkeleton.GetBoneName(ParentIndex);
        const FVector Child = Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
        const FVector Parent = Mesh->GetBoneLocation(ParentName, EBoneSpaces::WorldSpace);
        if (!IsSession5FiniteVector(Child) || !IsSession5FiniteVector(Parent))
        {
            return false;
        }
        const float RatioError = FMath::Abs(
            FVector::Dist(Child, Parent) / ExpectedLength - 1.0f);
        OutMaxRatioError = FMath::Max(OutMaxRatioError, RatioError);
        ++ComparedSegments;
    }
    return ComparedSegments >= 16
        && OutMaxRatioError <= Session5MaximumBoneLengthRatioError;
}

void ADiscGolfSession5MocapVisualCaptureRunner::PositionCameraForSubjects(
    const TArray<USkeletalMeshComponent*>& Meshes,
    float FieldOfViewDegrees)
{
    if (!CaptureCamera || !PlayerController || Meshes.IsEmpty())
    {
        return;
    }

    FBox KeyPointBounds(ForceInit);
    int32 CollectedSubjects = 0;
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        RefreshMeshForCapture(Mesh);
        TArray<FVector> Points;
        if (CollectSubjectKeyPoints(Mesh, Points))
        {
            for (const FVector& Point : Points)
            {
                KeyPointBounds += Point;
            }
            ++CollectedSubjects;
        }
    }
    if (CollectedSubjects != Meshes.Num() || !KeyPointBounds.IsValid)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("DG_SESSION5_MOCAP_VISUAL_CAMERA: could not collect evaluated key bones subjects=%d/%d"),
            CollectedSubjects,
            Meshes.Num());
        return;
    }

    UCameraComponent* CameraComponent = CaptureCamera->GetCameraComponent();
    if (!CameraComponent)
    {
        return;
    }
    CameraComponent->SetFieldOfView(FieldOfViewDegrees);

    float AspectRatio = 16.0f / 9.0f;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
        if (Size.X > 0 && Size.Y > 0)
        {
            AspectRatio = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
        }
    }
    const float HorizontalTan = FMath::Tan(FMath::DegreesToRadians(
        FieldOfViewDegrees * 0.5f));
    const float VerticalTan = HorizontalTan / FMath::Max(AspectRatio, 0.1f);
    const FVector Target = KeyPointBounds.GetCenter();
    const FVector ViewDirection =
        (StageTransform.GetRotation().GetForwardVector()
            + StageTransform.GetRotation().GetRightVector() * 0.24f).GetSafeNormal();
    const FQuat CameraRotation = (-ViewDirection).Rotation().Quaternion();

    float MaximumDepth = 0.0f;
    float MaximumHorizontal = 0.0f;
    float MaximumVertical = 0.0f;
    for (USkeletalMeshComponent* Mesh : Meshes)
    {
        TArray<FVector> Points;
        if (!CollectSubjectKeyPoints(Mesh, Points))
        {
            return;
        }
        for (const FVector& Point : Points)
        {
            const FVector LocalOffset = CameraRotation.UnrotateVector(Point - Target);
            MaximumDepth = FMath::Max(MaximumDepth, FMath::Abs(LocalOffset.X));
            MaximumHorizontal = FMath::Max(MaximumHorizontal, FMath::Abs(LocalOffset.Y));
            MaximumVertical = FMath::Max(MaximumVertical, FMath::Abs(LocalOffset.Z));
        }
    }
    // Multi-profile evidence must keep the compact fixture readable without
    // weakening the projected-coverage gate.  The selected fills remain inside
    // the later 0.92 horizontal / 0.90 vertical point-framing limits, while the
    // 25 cm clearance remains outside the expanded evaluated key-bone bounds.
    const bool bProfileGroup = Meshes.Num() > 1;
    const float HorizontalFill = bProfileGroup
        ? Session5GroupHorizontalFrameFill : 0.78f;
    const float VerticalFill = bProfileGroup
        ? Session5GroupVerticalFrameFill : 0.72f;
    const float CameraClearance = bProfileGroup
        ? Session5GroupCameraClearanceCm : 40.0f;
    const float HorizontalDistance = MaximumHorizontal
        / FMath::Max(HorizontalTan * HorizontalFill, 0.01f);
    const float VerticalDistance = MaximumVertical
        / FMath::Max(VerticalTan * VerticalFill, 0.01f);
    const float Distance = FMath::Clamp(
        FMath::Max(HorizontalDistance, VerticalDistance)
            + MaximumDepth + CameraClearance,
        300.0f,
        2200.0f);
    const FVector Location = Target + ViewDirection * Distance;
    CaptureCamera->SetActorLocation(Location);
    CaptureCamera->SetActorRotation((Target - Location).Rotation());
    PlayerController->SetViewTarget(CaptureCamera);
    if (PlayerController->PlayerCameraManager)
    {
        PlayerController->PlayerCameraManager->UpdateCamera(0.0f);
    }
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION5_MOCAP_VISUAL_CAMERA: subjects=%d target=%s camera=%s key_extent=%s distance=%.1f fov=%.1f"),
        Meshes.Num(),
        *Target.ToCompactString(),
        *Location.ToCompactString(),
        *KeyPointBounds.GetExtent().ToCompactString(),
        Distance,
        FieldOfViewDegrees);
}

void ADiscGolfSession5MocapVisualCaptureRunner::PositionCameraForSequence()
{
    USkeletalMeshComponent* Mesh = StageActor
        ? StageActor->GetSkeletalMeshComponent() : nullptr;
    if (!CaptureCamera || !Mesh)
    {
        return;
    }
    RefreshMeshForCapture(Mesh);
    TArray<USkeletalMeshComponent*> Subjects = { Mesh };
    PositionCameraForSubjects(Subjects, 36.0f);
}

void ADiscGolfSession5MocapVisualCaptureRunner::PositionCameraForProfiles(
    bool bSingleProfile,
    int32 ProfileIndex)
{
    if (!CaptureCamera || ProfileMeshes.IsEmpty())
    {
        return;
    }
    TArray<USkeletalMeshComponent*> Subjects;
    for (int32 Index = 0; Index < ProfileMeshes.Num(); ++Index)
    {
        if ((!bSingleProfile || Index == ProfileIndex) && ProfileMeshes[Index])
        {
            RefreshMeshForCapture(ProfileMeshes[Index]);
            Subjects.Add(ProfileMeshes[Index]);
        }
    }
    PositionCameraForSubjects(Subjects, bSingleProfile ? 34.0f : 38.0f);
}

bool ADiscGolfSession5MocapVisualCaptureRunner::ValidateVisibleFraming(
    int32 CaptureIndex,
    int32 ExpectedSubjects)
{
    if (!CaptureRecords.IsValidIndex(CaptureIndex) || !CaptureCamera
        || !CaptureCamera->GetCameraComponent())
    {
        return false;
    }

    TArray<USkeletalMeshComponent*> Subjects;
    if (StageActor && !StageActor->IsHidden())
    {
        Subjects.Add(StageActor->GetSkeletalMeshComponent());
    }
    for (int32 Index = 0; Index < ProfilePawns.Num(); ++Index)
    {
        if (ProfilePawns[Index] && !ProfilePawns[Index]->IsHidden())
        {
            Subjects.Add(ProfileMeshes[Index]);
        }
    }

    FCaptureRecord& Record = CaptureRecords[CaptureIndex];
    Record.VisibleSubjects = Subjects.Num();
    Record.FramedSubjects = 0;
    Record.ProfileAwareSubjects = 0;
    Record.MinSubjectScreenHeightFraction = TNumericLimits<float>::Max();
    Record.MaxSubjectScreenHeightFraction = 0.0f;
    Record.MinSubjectScreenWidthFraction = TNumericLimits<float>::Max();
    Record.MaxSubjectScreenWidthFraction = 0.0f;
    Record.MaxBoneLengthRatioError = 0.0f;
    Record.bFinitePose = Subjects.Num() == ExpectedSubjects;
    Record.bPlausibleBoneLengths = Subjects.Num() == ExpectedSubjects;
    Record.bCameraOutsideSubjects = Subjects.Num() == ExpectedSubjects;

    const FTransform CameraTransform(
        CaptureCamera->GetActorRotation(),
        CaptureCamera->GetActorLocation());
    const float HorizontalTan = FMath::Tan(FMath::DegreesToRadians(
        CaptureCamera->GetCameraComponent()->FieldOfView * 0.5f));
    float AspectRatio = 16.0f / 9.0f;
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
        if (Size.X > 0 && Size.Y > 0)
        {
            AspectRatio = static_cast<float>(Size.X) / static_cast<float>(Size.Y);
        }
    }
    const float VerticalTan = HorizontalTan / FMath::Max(AspectRatio, 0.1f);
    const auto Project = [&CameraTransform, HorizontalTan, VerticalTan](
        const FVector& Point)
    {
        const FVector Local = CameraTransform.InverseTransformPosition(Point);
        if (Local.X <= 1.0f)
        {
            return FVector2D(1000.0f, 1000.0f);
        }
        return FVector2D(
            Local.Y / (Local.X * HorizontalTan),
            Local.Z / (Local.X * VerticalTan));
    };

    for (USkeletalMeshComponent* Mesh : Subjects)
    {
        RefreshMeshForCapture(Mesh);
        TArray<FVector> Points;
        const bool bHasPoints = CollectSubjectKeyPoints(Mesh, Points);
        Record.bFinitePose &= bHasPoints;
        if (!bHasPoints)
        {
            Record.bPlausibleBoneLengths = false;
            Record.bCameraOutsideSubjects = false;
            continue;
        }

        const FVector Feet = (Points[1] + Points[2]) * 0.5f;
        const float BodyHeightCm = FVector::Dist(Points[0], Feet);
        const float PelvisToHeadCm = FVector::Dist(Points[5], Points[0]);
        const FDGBodyProfile* ExpectedProfile = nullptr;
        FString ExpectedProfileName(TEXT("BaseSkeleton"));
        for (int32 ProfileIndex = 0; ProfileIndex < ProfileMeshes.Num(); ++ProfileIndex)
        {
            if (ProfileMeshes[ProfileIndex].Get() == Mesh
                && ProfileBodies.IsValidIndex(ProfileIndex))
            {
                ExpectedProfile = &ProfileBodies[ProfileIndex];
                ExpectedProfileName = ProfileIndex < UE_ARRAY_COUNT(Session5ProfileNames)
                    ? Session5ProfileNames[ProfileIndex]
                    : TEXT("UnknownProfile");
                ++Record.ProfileAwareSubjects;
                break;
            }
        }
        float BoneRatioError = 0.0f;
        const bool bBoneLengthsPlausible = BodyHeightCm >= Session5MinimumBodyHeightCm
            && BodyHeightCm <= Session5MaximumBodyHeightCm
            && PelvisToHeadCm >= 25.0f
            && PelvisToHeadCm <= 150.0f
            && ValidateBoneLengthInvariant(Mesh, ExpectedProfile, BoneRatioError);
        Record.bPlausibleBoneLengths &= bBoneLengthsPlausible;
        Record.MaxBoneLengthRatioError = FMath::Max(
            Record.MaxBoneLengthRatioError, BoneRatioError);

        FBox KeyPointBounds(ForceInit);
        float MinimumX = TNumericLimits<float>::Max();
        float MaximumX = -TNumericLimits<float>::Max();
        bool bFramed = true;
        for (const FVector& Point : Points)
        {
            KeyPointBounds += Point;
            const FVector2D Projected = Project(Point);
            MinimumX = FMath::Min(MinimumX, Projected.X);
            MaximumX = FMath::Max(MaximumX, Projected.X);
            bFramed &= FMath::Abs(Projected.X) <= 0.92f
                && FMath::Abs(Projected.Y) <= 0.90f;
        }
        const float ScreenHeight = FMath::Abs(
            Project(Points[0]).Y - Project(Feet).Y) * 0.5f;
        const float ScreenWidth = (MaximumX - MinimumX) * 0.5f;
        const bool bReadableScale = ScreenHeight >= Session5MinimumScreenHeightFraction
            && ScreenHeight <= Session5MaximumScreenHeightFraction
            && ScreenWidth >= Session5MinimumScreenWidthFraction
            && ScreenWidth <= Session5MaximumScreenWidthFraction;
        Record.FramedSubjects += bFramed && bReadableScale ? 1 : 0;
        Record.MinSubjectScreenHeightFraction = FMath::Min(
            Record.MinSubjectScreenHeightFraction, ScreenHeight);
        Record.MaxSubjectScreenHeightFraction = FMath::Max(
            Record.MaxSubjectScreenHeightFraction, ScreenHeight);
        Record.MinSubjectScreenWidthFraction = FMath::Min(
            Record.MinSubjectScreenWidthFraction, ScreenWidth);
        Record.MaxSubjectScreenWidthFraction = FMath::Max(
            Record.MaxSubjectScreenWidthFraction, ScreenWidth);
        Record.bCameraOutsideSubjects &= !KeyPointBounds.ExpandBy(20.0f).IsInside(
            CaptureCamera->GetActorLocation());

        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION5_MOCAP_VISUAL_PROJECTION: capture=%d profile_expectation=%s body_height_cm=%.2f pelvis_head_cm=%.2f screen_height=%.4f screen_width=%.4f framed=%d readable_scale=%d bone_lengths=%d max_ratio_error=%.4f"),
            CaptureIndex + 1,
            *ExpectedProfileName,
            BodyHeightCm,
            PelvisToHeadCm,
            ScreenHeight,
            ScreenWidth,
            bFramed ? 1 : 0,
            bReadableScale ? 1 : 0,
            bBoneLengthsPlausible ? 1 : 0,
            BoneRatioError);
    }

    if (Record.MinSubjectScreenHeightFraction == TNumericLimits<float>::Max())
    {
        Record.MinSubjectScreenHeightFraction = 0.0f;
    }
    if (Record.MinSubjectScreenWidthFraction == TNumericLimits<float>::Max())
    {
        Record.MinSubjectScreenWidthFraction = 0.0f;
    }
    Record.bAllSubjectsFramed = Record.FramedSubjects == ExpectedSubjects;
    Record.bReadableProjectedScale = Record.MinSubjectScreenHeightFraction
            >= Session5MinimumScreenHeightFraction
        && Record.MaxSubjectScreenHeightFraction <= Session5MaximumScreenHeightFraction
        && Record.MinSubjectScreenWidthFraction >= Session5MinimumScreenWidthFraction
        && Record.MaxSubjectScreenWidthFraction <= Session5MaximumScreenWidthFraction;
    return Record.bFinitePose
        && Record.bAllSubjectsFramed
        && Record.bReadableProjectedScale
        && Record.bPlausibleBoneLengths
        && Record.bCameraOutsideSubjects;
}

void ADiscGolfSession5MocapVisualCaptureRunner::ShowEvidenceLabel(
    const FString& Heading,
    const FString& Detail) const
{
    if (!GEngine)
    {
        return;
    }
    GEngine->ClearOnScreenDebugMessages();
    GEngine->AddOnScreenDebugMessage(
        501,
        90.0f,
        FColor(255, 220, 70),
        FString::Printf(TEXT("SYNTHETIC PIPELINE TEST | DO NOT SHIP\n%s"), *Heading),
        true,
        FVector2D(1.35f, 1.35f));
    GEngine->AddOnScreenDebugMessage(
        502,
        90.0f,
        FColor::White,
        FString::Printf(
            TEXT("%s | PIPELINE VALIDATION ONLY; NO PRODUCTION MOTION CLAIM"),
            *Detail),
        true,
        FVector2D(1.0f, 1.0f));
}

void ADiscGolfSession5MocapVisualCaptureRunner::RequestCapture(
    int32 CaptureIndex,
    const FString& Evidence,
    const FString& StageLabel,
    EStage NextStage,
    int32 ExpectedSubjects)
{
    if (bScreenshotPending
        || !CaptureRecords.IsValidIndex(CaptureIndex)
        || !CaptureRecords[CaptureIndex].Filename.IsEmpty())
    {
        Fail(TEXT("capture request was invalid, duplicated, or overlapped another screenshot"));
        return;
    }

    FCaptureRecord& Record = CaptureRecords[CaptureIndex];
    Record.Filename = Session5CaptureFilenames[CaptureIndex];
    Record.Evidence = Evidence;
    Record.StageLabel = StageLabel;
    if (StageActor)
    {
        USkeletalMeshComponent* Mesh = StageActor->GetSkeletalMeshComponent();
        UAnimSingleNodeInstance* SingleNode = Mesh ? Mesh->GetSingleNodeInstance() : nullptr;
        UAnimationAsset* CurrentAnimation = SingleNode ? SingleNode->GetCurrentAsset() : nullptr;
        const UAnimSequence* ExpectedSequence = CaptureIndex == 0
            ? SourceSequence : (CaptureIndex == 1 ? RetargetedSequence : CleanedSequence);
        Record.AnimationAssetPath = CurrentAnimation
            ? CurrentAnimation->GetPathName() : FString();
        Record.SkeletalMeshPath = Mesh && Mesh->GetSkeletalMeshAsset()
            ? Mesh->GetSkeletalMeshAsset()->GetPathName() : FString();
        Record.bStageIdentityPassed = ExpectedSequence
            && Mesh
            && Mesh->GetSkeletalMeshAsset()
            && CurrentAnimation == ExpectedSequence
            && Record.AnimationAssetPath == ExpectedSequence->GetPathName()
            && Record.SkeletalMeshPath
                == (CaptureIndex == 0 ? SourceMesh->GetPathName() : TargetMesh->GetPathName());
    }
    else
    {
        Record.AnimationAssetPath = ProductionSequence ? ProductionSequence->GetPathName() : FString();
        Record.SkeletalMeshPath = TargetMesh ? TargetMesh->GetPathName() : FString();
        Record.MontagePath = PipelineMontage ? PipelineMontage->GetPathName() : FString();
        Record.MontagePositionSeconds = MinimumMontagePosition();
        Record.ExpectedPhaseSeconds = CaptureIndex == 4
            ? ReachbackSeconds
            : CaptureIndex == 3
                ? PlantSeconds
                : CaptureIndex == 5
                    ? ReleaseSeconds
                    : FollowThroughSeconds;
        bool bAllProfileMeshesMatch = !ProfileMeshes.IsEmpty();
        bool bMontageIsActive = !ProfileMeshes.IsEmpty();
        for (const USkeletalMeshComponent* Mesh : ProfileMeshes)
        {
            const UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
            bAllProfileMeshesMatch &= Mesh
                && Mesh->GetSkeletalMeshAsset() == TargetMesh;
            bMontageIsActive &= Anim
                && Anim->Montage_IsActive(PipelineMontage);
        }
        Record.bStageIdentityPassed = ProductionSequence
            && TargetMesh
            && PipelineMontage
            && bAllProfileMeshesMatch
            && bMontageIsActive
            && Record.AnimationAssetPath
                == DiscGolfSession5MocapValidation::ProductionSequence
            && Record.SkeletalMeshPath
                == DiscGolfSession5MocapValidation::TargetSkeletalMesh
            && Record.MontagePath
                == DiscGolfSession5MocapValidation::PipelineTestMontage
            && Record.MontagePositionSeconds >= Record.ExpectedPhaseSeconds
            && Record.MontagePositionSeconds <= Record.ExpectedPhaseSeconds + 0.20f;
        Record.bHeldDiscGateRequired = true;
        Record.bHeldDiscGatePassed = ValidateHeldDiscState(
            CaptureIndex == 3 || CaptureIndex == 4);
    }
    Record.WorldDiscDelta = CountWorldDiscs() - BaselineWorldDiscCount;
    Record.StrokeDelta = GameMode->GetStrokes() - BaselineStrokes;
    if (!ValidateVisibleFraming(CaptureIndex, ExpectedSubjects))
    {
        Fail(FString::Printf(TEXT("capture %d had invalid or unframed subjects"), CaptureIndex + 1));
        return;
    }

    PendingCaptureIndex = CaptureIndex;
    PendingCapturePath = FPaths::Combine(OutputDirectory, Record.Filename);
    IFileManager::Get().Delete(*PendingCapturePath, false, true, true);
    bScreenshotPending = true;
    bScreenshotIssued = false;
    (void)NextStage;
    SetStage(EStage::Capturing);
    bCapturePausedWorld = UGameplayStatics::SetGamePaused(this, true);
    if (!bCapturePausedWorld)
    {
        Fail(TEXT("visual runner could not pause transient world time for a stable capture"));
        return;
    }
    PlayerController->SetViewTarget(CaptureCamera);
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION5_MOCAP_VISUAL_CAPTURE: FRAMED %d/10 %s"),
        CaptureIndex + 1,
        *PendingCapturePath);
}

bool ADiscGolfSession5MocapVisualCaptureRunner::PollPendingCapture()
{
    if (!bScreenshotIssued)
    {
        bScreenshotIssued = true;
        StageStartSeconds = FPlatformTime::Seconds();
        FScreenshotRequest::RequestScreenshot(
            PendingCapturePath,
            true,
            false,
            false,
            FIntRect(),
            true);
        return false;
    }

    const int64 FileSize = IFileManager::Get().FileSize(*PendingCapturePath);
    if (FileSize >= Session5MinimumReadablePngBytes)
    {
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *PendingCapturePath)
            || Bytes.Num() < 24
            || Bytes[0] != 0x89 || Bytes[1] != 0x50
            || Bytes[2] != 0x4E || Bytes[3] != 0x47)
        {
            Fail(TEXT("capture was not a readable PNG"));
            return false;
        }

        FCaptureRecord& Record = CaptureRecords[PendingCaptureIndex];
        Record.Bytes = FileSize;
        Record.Width = ReadSession5BigEndianInt32(&Bytes[16]);
        Record.Height = ReadSession5BigEndianInt32(&Bytes[20]);
        Record.Sha1 = ComputeSha1(Bytes);
        Record.bReadable = Record.Width == Session5ExpectedWidth
            && Record.Height == Session5ExpectedHeight
            && Record.Sha1.Len() == FSHAHash::GetStringLen();
        if (!Record.bReadable)
        {
            Fail(FString::Printf(
                TEXT("capture %d failed exact 1920x1080/hash gates (%dx%d bytes=%lld)"),
                PendingCaptureIndex + 1,
                Record.Width,
                Record.Height,
                Record.Bytes));
            return false;
        }

        const int32 CompletedIndex = PendingCaptureIndex;
        UGameplayStatics::SetGamePaused(this, false);
        bCapturePausedWorld = false;
        bScreenshotPending = false;
        bScreenshotIssued = false;
        PendingCaptureIndex = INDEX_NONE;
        PendingCapturePath.Reset();
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("DG_SESSION5_MOCAP_VISUAL_CAPTURE: CAPTURED %d/10 bytes=%lld sha1=%s"),
            CompletedIndex + 1,
            Record.Bytes,
            *Record.Sha1);
        HandleCaptureCompleted(CompletedIndex);
        return true;
    }

    if (SecondsInStage() > Session5ScreenshotTimeoutSeconds)
    {
        Fail(FString::Printf(TEXT("screenshot timed out: %s"), *PendingCapturePath));
    }
    return false;
}

void ADiscGolfSession5MocapVisualCaptureRunner::HandleCaptureCompleted(
    int32 CaptureIndex)
{
    switch (CaptureIndex)
    {
        case 0:
            if (!PrepareSequenceStage(
                    RetargetedSequence,
                    TargetMesh,
                    FMath::Clamp(ReleaseSeconds / PipelineMontage->GetPlayLength(), 0.05f, 0.95f)))
            {
                Fail(TEXT("raw retarget evidence stage could not be prepared"));
                return;
            }
            SetStage(EStage::WaitingForRetargeted);
            break;

        case 1:
            if (!PrepareSequenceStage(
                    CleanedSequence,
                    TargetMesh,
                    FMath::Clamp(ReleaseSeconds / PipelineMontage->GetPlayLength(), 0.05f, 0.95f)))
            {
                Fail(TEXT("cleaned retarget evidence stage could not be prepared"));
                return;
            }
            SetStage(EStage::WaitingForCleaned);
            break;

        case 2:
            DestroySequenceStage();
            if (!SpawnProfileFixtures() || !BeginProfileThrows())
            {
                Fail(TEXT("three pipeline/profile montage fixtures could not start"));
                return;
            }
            ShowAllProfiles();
            SetStage(EStage::WaitingForReachback);
            break;

        case 4:
            PauseProfileMontages(false);
            SetStage(EStage::WaitingForPlant);
            break;

        case 3:
            PauseProfileMontages(false);
            SetStage(EStage::WaitingForRelease);
            break;

        case 5:
            if (!ValidateProfileReleaseCounts())
            {
                Fail(TEXT("release counts were not exactly one after release evidence"));
                return;
            }
            PauseProfileMontages(false);
            SetStage(EStage::WaitingForFollowThrough);
            break;

        case 6:
            if (!ProfilePosesFinite() || !ValidateProfileReleaseCounts())
            {
                Fail(TEXT("follow-through profile compatibility gate failed"));
                return;
            }
            SetOnlyProfileVisible(0);
            SetStage(EStage::WaitingForShortProfile);
            break;

        case 7:
            SetOnlyProfileVisible(1);
            SetStage(EStage::WaitingForBaselineProfile);
            break;

        case 8:
            SetOnlyProfileVisible(2);
            SetStage(EStage::WaitingForTallProfile);
            break;

        case 9:
            ShowAllProfiles();
            PauseProfileMontages(false);
            SetStage(EStage::WaitingForRecovery);
            break;

        default:
            Fail(TEXT("unexpected capture completion index"));
            break;
    }
}

FString ADiscGolfSession5MocapVisualCaptureRunner::ComputeSha1(
    const TArray<uint8>& Bytes) const
{
    return Bytes.IsEmpty()
        ? FString()
        : FSHA1::HashBuffer(Bytes.GetData(), static_cast<uint64>(Bytes.Num())).ToString();
}

void ADiscGolfSession5MocapVisualCaptureRunner::SnapshotPersistentFiles(
    const FString& RootDirectory,
    bool bPackagesOnly,
    TMap<FString, FFileStamp>& OutFiles) const
{
    OutFiles.Reset();
    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*RootDirectory))
    {
        return;
    }
    PlatformFile.IterateDirectoryRecursively(
        *RootDirectory,
        [bPackagesOnly, &OutFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory)
        {
            if (bIsDirectory)
            {
                return true;
            }
            FString Path = FPaths::ConvertRelativePathToFull(FilenameOrDirectory);
            FPaths::MakeStandardFilename(Path);
            const FString Extension = FPaths::GetExtension(Path, true).ToLower();
            if (bPackagesOnly
                && Extension != TEXT(".uasset")
                && Extension != TEXT(".umap"))
            {
                return true;
            }
            FFileStamp Stamp;
            Stamp.Size = IFileManager::Get().FileSize(*Path);
            Stamp.TimestampTicks = IFileManager::Get().GetTimeStamp(*Path).GetTicks();
            OutFiles.Add(Path, Stamp);
            return true;
        });
}

void ADiscGolfSession5MocapVisualCaptureRunner::DiffPersistentFiles(
    const TMap<FString, FFileStamp>& Before,
    const TMap<FString, FFileStamp>& After,
    TArray<FString>& OutChanged) const
{
    OutChanged.Reset();
    TSet<FString> Paths;
    for (const TPair<FString, FFileStamp>& Pair : Before)
    {
        Paths.Add(Pair.Key);
    }
    for (const TPair<FString, FFileStamp>& Pair : After)
    {
        Paths.Add(Pair.Key);
    }
    for (const FString& Path : Paths)
    {
        const FFileStamp* OldStamp = Before.Find(Path);
        const FFileStamp* NewStamp = After.Find(Path);
        if (!OldStamp || !NewStamp || !(*OldStamp == *NewStamp))
        {
            OutChanged.Add(Path);
        }
    }
    OutChanged.Sort();
}

bool ADiscGolfSession5MocapVisualCaptureRunner::VerifyNoPersistentWrites()
{
    TMap<FString, FFileStamp> FinalPackages;
    TMap<FString, FFileStamp> FinalSaveGames;
    SnapshotPersistentFiles(FPaths::ProjectContentDir(), true, FinalPackages);
    SnapshotPersistentFiles(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames")),
        false,
        FinalSaveGames);
    DiffPersistentFiles(InitialPackageFiles, FinalPackages, ChangedPackageFiles);
    DiffPersistentFiles(InitialSaveGameFiles, FinalSaveGames, ChangedSaveGameFiles);
    return ChangedPackageFiles.IsEmpty() && ChangedSaveGameFiles.IsEmpty();
}

int32 ADiscGolfSession5MocapVisualCaptureRunner::CountWorldDiscs() const
{
    int32 Count = 0;
    if (GetWorld())
    {
        for (TActorIterator<ADiscActor> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It) && !It->IsActorBeingDestroyed())
            {
                ++Count;
            }
        }
    }
    return Count;
}

void ADiscGolfSession5MocapVisualCaptureRunner::ApplyEvidenceMaterial(
    USkeletalMeshComponent* Mesh) const
{
    if (!Mesh)
    {
        return;
    }
    UMaterialInterface* EvidenceMaterial = LoadObject<UMaterialInterface>(
        nullptr,
        Session5EvidenceMaterialPath);
    if (EvidenceMaterial)
    {
        // Pipeline meshes are allowed to acquire multiple render sections.  A
        // slot-zero-only override made the old manifest capable of passing a
        // geometrically valid but visually black subject.
        const int32 MaterialSlotCount = FMath::Max(1, Mesh->GetNumMaterials());
        for (int32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
        {
            Mesh->SetMaterial(SlotIndex, EvidenceMaterial);
        }
    }
    Mesh->SetVisibility(true, true);
    Mesh->SetHiddenInGame(false, true);
    Mesh->SetCastShadow(false);
    RefreshMeshForCapture(Mesh);
    Mesh->MarkRenderStateDirty();
}

void ADiscGolfSession5MocapVisualCaptureRunner::RestoreRuntimeState()
{
    if (bCapturePausedWorld)
    {
        UGameplayStatics::SetGamePaused(this, false);
        bCapturePausedWorld = false;
    }
    DestroySequenceStage();
    ProfileRecoveryReasonLabels.Reset();
    for (UDiscGolfRHBHThrowAdapterComponent* Adapter : ProfileAdapters)
    {
        ProfileRecoveryReasonLabels.Add(Session5RecoveryReasonLabel(Adapter
            ? Adapter->GetRecoveryReason()
            : EDiscGolfRHBHThrowRecoveryReason::None));
        if (Adapter)
        {
            Adapter->GetAuthoritativeLaunchDelegate().Unbind();
        }
    }
    for (ADiscGolferPawn* Pawn : ProfilePawns)
    {
        if (IsValid(Pawn))
        {
            Pawn->Destroy();
        }
    }
    ProfilePawns.Reset();
    ProfileAdapters.Reset();
    ProfileMeshes.Reset();
    ProfileBodies.Reset();

    if (PossessedGolfer)
    {
        PossessedGolfer->SetActorHiddenInGame(bInitialGolferHidden);
    }
    if (PlayerController)
    {
        if (AHUD* Hud = PlayerController->GetHUD())
        {
            Hud->bShowHUD = bInitialHudVisible;
        }
        if (InitialViewTarget.IsValid())
        {
            PlayerController->SetViewTarget(InitialViewTarget.Get());
        }
    }
    if (IsValid(CaptureCamera))
    {
        CaptureCamera->Destroy();
    }
    CaptureCamera = nullptr;
    KeyLight = nullptr;
    FillLight = nullptr;
    if (GEngine)
    {
        GEngine->ClearOnScreenDebugMessages();
    }
}

void ADiscGolfSession5MocapVisualCaptureRunner::WriteManifest(
    bool bPassed,
    const FString& Error)
{
    if (ManifestPath.IsEmpty())
    {
        return;
    }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("status"), bPassed ? TEXT("PASS") : TEXT("FAIL"));
    Root->SetStringField(TEXT("schema"), TEXT("DiscGolfTour.Session5MocapVisualEvidence.v2"));
    Root->SetStringField(
        TEXT("scope"),
        TEXT("SYNTHETIC_TEST_PIPELINE_VALIDATION_NOT_PRODUCTION_ANIMATION_ACCEPTANCE"));
    Root->SetStringField(TEXT("shipping_status"), TEXT("DO_NOT_SHIP"));
    Root->SetStringField(TEXT("error"), Error);
    Root->SetStringField(TEXT("output_directory"), OutputDirectory);
    Root->SetNumberField(TEXT("expected_capture_count"), 10);
    Root->SetNumberField(TEXT("expected_width"), Session5ExpectedWidth);
    Root->SetNumberField(TEXT("expected_height"), Session5ExpectedHeight);
    Root->SetStringField(TEXT("evidence_material"), Session5EvidenceMaterialPath);
    Root->SetStringField(
        TEXT("evidence_material_policy"),
        TEXT("NEUTRAL_HIGH_CONTRAST_TRANSIENT_OVERRIDE_ALL_SKELETAL_MATERIAL_SLOTS"));
    Root->SetStringField(
        TEXT("profile_bone_length_policy"),
        Session5ProfileBoneLengthPolicy);
    Root->SetNumberField(
        TEXT("profile_fixture_spacing_cm"),
        Session5ProfileFixtureSpacingCm);
    Root->SetNumberField(
        TEXT("profile_group_horizontal_frame_fill"),
        Session5GroupHorizontalFrameFill);
    Root->SetNumberField(
        TEXT("profile_group_vertical_frame_fill"),
        Session5GroupVerticalFrameFill);
    Root->SetNumberField(
        TEXT("profile_group_camera_clearance_cm"),
        Session5GroupCameraClearanceCm);

    TSharedRef<FJsonObject> Assets = MakeShared<FJsonObject>();
    Assets->SetStringField(TEXT("source_sequence"), DiscGolfSession5MocapValidation::SourceSequence);
    Assets->SetStringField(TEXT("source_mesh"), DiscGolfSession5MocapValidation::SourceSkeletalMesh);
    Assets->SetStringField(TEXT("raw_retarget"), DiscGolfSession5MocapValidation::RetargetedSequence);
    Assets->SetStringField(TEXT("cleaned_retarget"), DiscGolfSession5MocapValidation::CleanedSequence);
    Assets->SetStringField(TEXT("production_test_sequence"), DiscGolfSession5MocapValidation::ProductionSequence);
    Assets->SetStringField(TEXT("pipeline_test_montage"), DiscGolfSession5MocapValidation::PipelineTestMontage);
    Assets->SetStringField(TEXT("target_mesh"), DiscGolfSession5MocapValidation::TargetSkeletalMesh);
    Root->SetObjectField(TEXT("assets"), Assets);

    TSharedRef<FJsonObject> Events = MakeShared<FJsonObject>();
    Events->SetNumberField(TEXT("release_notify_count"), ReleaseNotifyCount);
    Events->SetNumberField(TEXT("finish_notify_count"), FinishNotifyCount);
    Events->SetNumberField(TEXT("reachback_seconds"), ReachbackSeconds);
    Events->SetNumberField(TEXT("plant_seconds"), PlantSeconds);
    Events->SetNumberField(TEXT("release_seconds"), ReleaseSeconds);
    Events->SetNumberField(TEXT("follow_through_seconds"), FollowThroughSeconds);
    Events->SetNumberField(TEXT("recovery_seconds"), RecoverySeconds);
    Events->SetNumberField(TEXT("validation_release_callbacks"), FixtureReleaseCallbackCount);
    Root->SetObjectField(TEXT("montage_contract"), Events);

    TArray<TSharedPtr<FJsonValue>> Recoveries;
    for (int32 Index = 0; Index < ProfileRecoveryReasonLabels.Num(); ++Index)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(
            TEXT("profile"),
            Index < UE_ARRAY_COUNT(Session5ProfileNames)
                ? Session5ProfileNames[Index] : TEXT("Unknown"));
        Object->SetStringField(
            TEXT("reason"),
            ProfileRecoveryReasonLabels[Index]);
        Object->SetBoolField(
            TEXT("throw_finished"),
            ProfileRecoveryReasonLabels[Index] == TEXT("ThrowFinished"));
        Recoveries.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("profile_recoveries"), Recoveries);

    TArray<TSharedPtr<FJsonValue>> Captures;
    for (const FCaptureRecord& Capture : CaptureRecords)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("filename"), Capture.Filename);
        Object->SetStringField(
            TEXT("path"), FPaths::Combine(OutputDirectory, Capture.Filename));
        Object->SetStringField(TEXT("evidence"), Capture.Evidence);
        Object->SetStringField(TEXT("stage"), Capture.StageLabel);
        Object->SetStringField(TEXT("animation_asset"), Capture.AnimationAssetPath);
        Object->SetStringField(TEXT("skeletal_mesh"), Capture.SkeletalMeshPath);
        Object->SetStringField(TEXT("montage"), Capture.MontagePath);
        Object->SetStringField(TEXT("sha1"), Capture.Sha1);
        Object->SetNumberField(TEXT("bytes"), static_cast<double>(Capture.Bytes));
        Object->SetNumberField(TEXT("width"), Capture.Width);
        Object->SetNumberField(TEXT("height"), Capture.Height);
        Object->SetNumberField(TEXT("visible_subjects"), Capture.VisibleSubjects);
        Object->SetNumberField(TEXT("framed_subjects"), Capture.FramedSubjects);
        Object->SetNumberField(
            TEXT("profile_aware_subjects"), Capture.ProfileAwareSubjects);
        Object->SetNumberField(
            TEXT("min_subject_screen_height_fraction"),
            Capture.MinSubjectScreenHeightFraction);
        Object->SetNumberField(
            TEXT("max_subject_screen_height_fraction"),
            Capture.MaxSubjectScreenHeightFraction);
        Object->SetNumberField(
            TEXT("min_subject_screen_width_fraction"),
            Capture.MinSubjectScreenWidthFraction);
        Object->SetNumberField(
            TEXT("max_subject_screen_width_fraction"),
            Capture.MaxSubjectScreenWidthFraction);
        Object->SetNumberField(
            TEXT("max_bone_length_ratio_error"),
            Capture.MaxBoneLengthRatioError);
        Object->SetNumberField(
            TEXT("montage_position_seconds"), Capture.MontagePositionSeconds);
        Object->SetNumberField(
            TEXT("expected_phase_seconds"), Capture.ExpectedPhaseSeconds);
        Object->SetBoolField(TEXT("readable"), Capture.bReadable);
        Object->SetBoolField(TEXT("finite_pose"), Capture.bFinitePose);
        Object->SetBoolField(
            TEXT("all_subjects_projected_and_framed"),
            Capture.bAllSubjectsFramed);
        Object->SetBoolField(
            TEXT("projected_body_coverage_readable"),
            Capture.bReadableProjectedScale);
        Object->SetBoolField(
            TEXT("bone_lengths_and_world_body_height_plausible"),
            Capture.bPlausibleBoneLengths);
        Object->SetBoolField(
            TEXT("camera_outside_key_bone_bounds"),
            Capture.bCameraOutsideSubjects);
        Object->SetBoolField(
            TEXT("stage_asset_identity"), Capture.bStageIdentityPassed);
        Object->SetBoolField(
            TEXT("held_disc_gate_required"), Capture.bHeldDiscGateRequired);
        Object->SetBoolField(
            TEXT("held_disc_scale_alignment_or_hidden_state"),
            Capture.bHeldDiscGatePassed);
        Object->SetNumberField(TEXT("world_disc_delta"), Capture.WorldDiscDelta);
        Object->SetNumberField(TEXT("stroke_delta"), Capture.StrokeDelta);
        Captures.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("captures"), Captures);

    TSharedRef<FJsonObject> Isolation = MakeShared<FJsonObject>();
    Isolation->SetNumberField(TEXT("baseline_world_discs"), BaselineWorldDiscCount);
    Isolation->SetNumberField(TEXT("final_world_discs"), CountWorldDiscs());
    Isolation->SetNumberField(TEXT("baseline_strokes"), BaselineStrokes);
    Isolation->SetNumberField(TEXT("final_strokes"), GameMode ? GameMode->GetStrokes() : -1);
    Isolation->SetBoolField(
        TEXT("world_paused_after_restore"),
        UGameplayStatics::IsGamePaused(this));
    Isolation->SetStringField(
        TEXT("fixture_release_mode"),
        TEXT("TRANSIENT VALIDATION CALLBACK; NO GAMEPLAY DISC OR STROKE"));
    Root->SetObjectField(TEXT("gameplay_isolation"), Isolation);

    const auto StringsToJson = [](const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Json;
        for (const FString& Value : Values)
        {
            Json.Add(MakeShared<FJsonValueString>(Value));
        }
        return Json;
    };
    TSharedRef<FJsonObject> Writes = MakeShared<FJsonObject>();
    Writes->SetBoolField(
        TEXT("passed"),
        ChangedPackageFiles.IsEmpty() && ChangedSaveGameFiles.IsEmpty());
    Writes->SetArrayField(
        TEXT("changed_uasset_or_umap_files"), StringsToJson(ChangedPackageFiles));
    Writes->SetArrayField(
        TEXT("changed_save_game_files"), StringsToJson(ChangedSaveGameFiles));
    Writes->SetStringField(
        TEXT("allowed_outputs"),
        TEXT("Ten PNG files and this JSON manifest under Saved/CharacterFramework/Screenshots/Session5_MocapPipeline."));
    Writes->SetBoolField(TEXT("package_save_calls"), false);
    Writes->SetBoolField(TEXT("level_save_calls"), false);
    Root->SetObjectField(TEXT("persistent_write_guard"), Writes);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    FFileHelper::SaveStringToFile(Json, *ManifestPath);
}

void ADiscGolfSession5MocapVisualCaptureRunner::Fail(const FString& Reason)
{
    if (bFinished)
    {
        return;
    }
    bFinished = true;
    SetStage(EStage::Finished);
    VerifyNoPersistentWrites();
    RestoreRuntimeState();
    WriteManifest(false, Reason);
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("DG_SESSION5_MOCAP_VISUAL_CAPTURE: FAIL %s manifest=%s"),
        *Reason,
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession5MocapVisualCaptureRunner::Pass()
{
    if (bFinished)
    {
        return;
    }

    bool bCapturesPassed = CaptureRecords.Num()
        == UE_ARRAY_COUNT(Session5CaptureFilenames);
    for (const FCaptureRecord& Capture : CaptureRecords)
    {
        bCapturesPassed &= !Capture.Filename.IsEmpty()
            && Capture.bReadable
            && Capture.bFinitePose
            && Capture.bAllSubjectsFramed
            && Capture.bReadableProjectedScale
            && Capture.bPlausibleBoneLengths
            && Capture.bCameraOutsideSubjects
            && Capture.bStageIdentityPassed
            && (!Capture.bHeldDiscGateRequired || Capture.bHeldDiscGatePassed)
            && Capture.Width == Session5ExpectedWidth
            && Capture.Height == Session5ExpectedHeight
            && Capture.Bytes >= Session5MinimumReadablePngBytes
            && Capture.Sha1.Len() == FSHAHash::GetStringLen()
            && Capture.WorldDiscDelta == 0
            && Capture.StrokeDelta == 0;
    }
    const bool bEventsPassed = ReleaseNotifyCount == 1
        && FinishNotifyCount == 1
        && ValidateProfileReleaseCounts();
    bool bRecoveriesPassed = ProfileAdapters.Num()
        == UE_ARRAY_COUNT(Session5ProfileNames);
    for (const UDiscGolfRHBHThrowAdapterComponent* Adapter : ProfileAdapters)
    {
        bRecoveriesPassed &= Adapter
            && Adapter->GetRecoveryReason()
                == EDiscGolfRHBHThrowRecoveryReason::ThrowFinished;
    }
    const bool bNoMutation = ValidateNoGameplayMutation()
        && !UGameplayStatics::IsGamePaused(this);
    const bool bNoWrites = VerifyNoPersistentWrites();
    if (!bCapturesPassed || !bEventsPassed || !bRecoveriesPassed
        || !bNoMutation || !bNoWrites)
    {
        Fail(TEXT("final Session 5 visual invariant was incomplete"));
        return;
    }

    bFinished = true;
    SetStage(EStage::Finished);
    RestoreRuntimeState();
    WriteManifest(true, TEXT(""));
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION5_MOCAP_VISUAL_CAPTURE: PASS captures=10 synthetic_test_only=1 profiles=3 release_notifies=1 finish_notifies=1 fixture_callbacks=3 package_writes=0 savegame_writes=0 manifest=%s"),
        *ManifestPath);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
