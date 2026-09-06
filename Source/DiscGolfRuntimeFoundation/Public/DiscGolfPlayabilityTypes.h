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
struct DISCGOLFRUNTIMEFOUNDATION_API FDGPlayabilityCheckResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CheckId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGPlayabilityGateLevel GateLevel = EDGPlayabilityGateLevel::Smoke;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGPlayabilityStatus Status = EDGPlayabilityStatus::NotRun;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGPlayabilityFailureCode FailureCode = EDGPlayabilityFailureCode::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Message;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DurationSeconds = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bBlocking = true;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGPlayabilityGateReport
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid RunId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDateTime StartedUtc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDateTime FinishedUtc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGPlayabilityCheckResult> Results;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PassedChecks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FailedChecks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BlockingFailures = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGPlayabilityStatus OverallStatus = EDGPlayabilityStatus::NotRun;
};
