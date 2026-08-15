#pragma once

#include "CoreMinimal.h"
#include "DiscGolfPlayabilityTypes.generated.h"

UENUM(BlueprintType)
enum class EDGPlayabilityGateLevel : uint8
{
    Smoke,
    CoreLoop,
    Round,
    Persistence
};

UENUM(BlueprintType)
enum class EDGPlayabilityStatus : uint8
{
    NotRun,
    Running,
    Passed,
    Failed,
    Blocked
};

UENUM(BlueprintType)
enum class EDGPlayabilityFailureCode : uint8
{
    None,

    BootFailed,
    MainMenuUnavailable,
    PlayerProfileUnavailable,
    CourseLoadFailed,
    HoleSpawnFailed,
    TeeStateInvalid,

    BagUnavailable,
    NoSelectableDisc,
    DiscSelectionFailed,

    AimStateUnavailable,
    ThrowCouldNotStart,
    ReleaseNotifyMissing,
    DiscLaunchFailed,
    PhysicsDidNotAdvance,
    DiscNeverSettled,
    DiscStateInvalid,

    LieNotUpdated,
    LieInvalid,
    PlayerCouldNotContinue,

    PenaltyRuleFailed,
    OutOfBoundsRuleFailed,
    WaterRuleFailed,
    MandoRuleFailed,

    BasketDetectionFailed,
    HoleCompletionFailed,
    ScoreUpdateFailed,
    NextHoleFailed,

    PauseFailed,
    ResumeFailed,
    CameraStateStuck,
    InputContextStuck,
    ReplayStateStuck,

    DuplicatePlayerDetected,
    DuplicateDiscDetected,

    RoundCompletionFailed,
    ResultsScreenFailed,

    SaveFailed,
    LoadFailed,
    SaveMismatch,

    MissingAssetRecoveryFailed,
    InvalidCosmeticRecoveryFailed,

    SoftLockDetected,
    Timeout,
    UnexpectedError
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGPlayabilityCheckResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    FName CheckId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    EDGPlayabilityGateLevel GateLevel = EDGPlayabilityGateLevel::Smoke;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    EDGPlayabilityStatus Status = EDGPlayabilityStatus::NotRun;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    EDGPlayabilityFailureCode FailureCode = EDGPlayabilityFailureCode::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    FString Message;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    float DurationSeconds = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    bool bBlocking = true;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGPlayabilityGateReport
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    FGuid RunId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    FDateTime StartedUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    FDateTime FinishedUtc;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    EDGPlayabilityStatus OverallStatus = EDGPlayabilityStatus::NotRun;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    TArray<FDGPlayabilityCheckResult> Results;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    int32 PassedChecks = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    int32 FailedChecks = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Playability")
    int32 BlockingFailures = 0;
};
