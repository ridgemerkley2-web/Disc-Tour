#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession3VisualCaptureRunner.generated.h"

class ACameraActor;
class ADiscActor;
class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class UAnimMontage;
class UDiscGolfCharacterProfile;
class UDiscGolfRHBHThrowAdapterComponent;
class UDiscGolfThrowComponent;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Dedicated, command-line-only Session 3 visual evidence harness.
 *
 * The first five captures use the possessed gameplay pawn, the authored RHBH
 * montage, the project release adapter, and the single authoritative gameplay
 * disc. The final three captures are transient body-profile compatibility
 * fixtures: they exercise the same pawn/framework/montage, but deliberately
 * replace the fixture launch callback so the evidence pass cannot add strokes
 * or gameplay discs after the accepted live throw.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfSession3VisualCaptureRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession3VisualCaptureRunner();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void Start();

private:
    enum class EStage : uint8
    {
        WaitingForGameplay,
        BuildingLiveCommand,
        WaitingForHeldPose,
        CapturingHeld,
        WaitingForRelease,
        CapturingExactRelease,
        WaitingForPostRelease,
        CapturingPostRelease,
        WaitingForFollowThrough,
        CapturingFollowThrough,
        WaitingForGameplayRecovery,
        CapturingGameplayRecovery,
        StartingProfile,
        WaitingForProfilePose,
        CapturingProfile,
        WaitingForProfileRecovery,
        Finished
    };

    struct FCaptureRecord
    {
        FString Filename;
        FString Evidence;
        FString Source;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        float CameraToSubjectCm = 0.0f;
        float SubjectBoundsRadiusCm = 0.0f;
        bool bCameraOutsideSubjectBounds = false;
        bool bSubjectFramed = false;
        bool bFocusPointFramed = false;
        bool bLineOfSightClear = false;
        bool bBoneLengthsInvariant = false;
        float MaxBoneLengthRatioError = 0.0f;
        float SubjectScreenHeightFraction = 0.0f;
        float FocusProjectedDiameterPixels = 0.0f;
        bool bSubjectReadableScale = false;
        bool bEvidenceMeshReadable = false;
        FString EvidenceComponent;
        FVector EvidenceComponentWorldLocation = FVector::ZeroVector;
        float EvidenceActorToComponentCm = 0.0f;
        float EvidenceFocusAlignmentCm = 0.0f;
        bool bEvidenceFocusAnchoredToComponent = false;
    };

    struct FProfileRecord
    {
        FString Name;
        FString AssetPath;
        float HeightCm = 0.0f;
        float WingspanScale = 0.0f;
        int32 ReleaseCount = 0;
        bool bAnimationStarted = false;
        bool bGripTransformUsable = false;
        bool bFollowThroughReached = false;
        bool bRecovered = false;
        bool bPoseFiniteAndPlausible = false;
        FString RecoveryReason;
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ThrowAdapter;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> GolferMesh;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> HeldDiscVisual;
    UPROPERTY() TObjectPtr<UAnimMontage> ThrowMontage;
    UPROPERTY() TObjectPtr<ADiscActor> LiveGameplayDisc;
    UPROPERTY() TObjectPtr<ACameraActor> CaptureCamera;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> HeldDiscEvidenceMaterial;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GameplayDiscEvidenceMaterial;
    UPROPERTY() TObjectPtr<UPointLightComponent> CaptureKeyLight;
    UPROPERTY() TObjectPtr<UPointLightComponent> CaptureFillLight;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> PendingEvidenceMesh;

    UPROPERTY() TObjectPtr<ADiscGolferPawn> ProfileFixture;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ProfileAdapter;
    UPROPERTY() TObjectPtr<UDiscGolfThrowComponent> ProfileThrowComponent;
    UPROPERTY() TObjectPtr<USkeletalMeshComponent> ProfileMesh;

    FThrowCommand LiveCommand;
    FTransform InitialGolferTransform;
    EStage Stage = EStage::WaitingForGameplay;
    double StageStartRealSeconds = 0.0;
    bool bStarted = false;
    bool bFinished = false;
    bool bScreenshotPending = false;
    bool bScreenshotIssued = false;
    int32 PendingCaptureIndex = INDEX_NONE;
    FString PendingCapturePath;
    double PendingCaptureStartRealSeconds = 0.0;
    float SavedGlobalTimeDilation = 1.0f;
    int32 BaselineWorldDiscCount = 0;
    int32 BaselineStrokes = 0;
    int32 BaselineLiveReleaseCount = 0;
    int32 LiveReleaseCallbackCount = 0;
    bool bLiveThrowRecovered = false;
    bool bLiveFollowThroughReached = false;
    FString LiveRecoveryReason;
    int32 ProfileIndex = 0;
    int32 ProfileLaunchCount = 0;
    int32 ProfileReleaseCallbackCount = 0;
    bool bProfileRecoveredCallback = false;
    FString ProfileRecoveryReason;
    bool bLastCameraLineOfSightClear = false;
    FColor PendingEvidenceMarkerColor = FColor::Transparent;
    FString OutputDirectory;
    FString ManifestPath;
    TArray<FCaptureRecord> CaptureRecords;
    TArray<FProfileRecord> ProfileRecords;

    void SetStage(EStage NewStage);
    double SecondsInStage() const;
    bool BuildTimingCommand(FThrowCommand& OutCommand, bool bBeginOnly);
    void BeginLiveThrow();
    void PrepareHeldCapture();
    void PreparePostReleaseCapture();
    void PrepareFollowThroughCapture();
    void PrepareRecoveredCapture();
    void BeginProfileFixture();
    void PrepareProfileCapture();
    void FinishProfileFixture();
    void DestroyProfileFixture();

    void RequestCapture(int32 CaptureIndex, const FString& Evidence, const FString& Source);
    bool PollPendingCapture();
    void HandleCaptureCompleted(int32 CaptureIndex);
    void PositionFullBodyCamera(
        const AActor* Subject,
        const USkeletalMeshComponent* Mesh,
        float FieldOfViewDegrees = 38.0f);
    void PositionGripCloseupCamera(
        const AActor* Subject,
        const USkeletalMeshComponent* Mesh,
        const FVector& FocusPoint,
        float FieldOfViewDegrees = 32.0f);
    void PositionPostReleaseCamera();
    FVector ResolveExternalCameraLocation(
        const AActor* Subject,
        const FVector& Target,
        float DistanceCm,
        float HeightBiasCm,
        float SideBiasCm);
    bool ValidateCameraFraming(
        int32 CaptureIndex,
        const AActor* Subject,
        const USkeletalMeshComponent* Mesh,
        const FVector& FocusPoint,
        bool bRequireFullBody,
        float FocusRadiusCm = 0.0f,
        const UStaticMeshComponent* EvidenceMesh = nullptr);
    void ShowEvidenceLabel(const FString& Heading, const FString& Detail) const;

    USkeletalMeshComponent* FindSkeletalMesh(AActor* ActorOwner) const;
    UStaticMeshComponent* FindNamedStaticMesh(AActor* ActorOwner, FName ComponentName) const;
    int32 CountWorldDiscs() const;
    float GetMontagePosition(USkeletalMeshComponent* Mesh) const;
    bool IsProfilePoseFiniteAndPlausible() const;
    bool ValidateBoneLengthInvariant(
        const USkeletalMeshComponent* Mesh,
        float& OutMaxRatioError) const;
    float GetProvisionalDiscRadiusCm(const UStaticMeshComponent* DiscMesh) const;
    FVector GetDiscEvidenceLocation(const UStaticMeshComponent* DiscMesh) const;
    void SetLiveDiscSimulationPaused(bool bPaused) const;
    void ApplyNeutralProxyMaterials(USkeletalMeshComponent* Mesh) const;
    UMaterialInstanceDynamic* ApplyDiscEvidenceMaterial(
        UStaticMeshComponent* DiscMesh,
        const FLinearColor& Color) const;
    void DrawDiscEvidenceMarker(const FVector& Location, const FColor& Color) const;
    void ClearEvidenceMarkers() const;
    void RestoreCaptureTimeDilation();
    void WriteManifest(bool bPassed, const FString& Error);
    void Fail(const FString& Reason);
    void Pass();

    UFUNCTION()
    void HandleLiveRelease(int64 AttemptSerial, bool bAuthoritativeLaunchAccepted);

    UFUNCTION()
    void HandleLiveRecovery(int64 AttemptSerial, bool bDiscWasReleased);

    UFUNCTION()
    void HandleProfileRelease(int64 AttemptSerial, bool bLaunchAccepted);

    UFUNCTION()
    void HandleProfileRecovery(int64 AttemptSerial, bool bDiscWasReleased);

    bool HandleProfileFixtureLaunch(
        const FThrowCommand& AuthoritativeCommand,
        const FTransform& GripWorldTransform);
};
