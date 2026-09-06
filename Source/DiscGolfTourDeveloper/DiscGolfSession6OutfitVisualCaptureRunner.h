#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession6OutfitVisualCaptureRunner.generated.h"

class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class UDiscGolfRHBHThrowAdapterComponent;

/** Rendered, no-save Session 6 creator/outfit/body/throw evidence. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession6OutfitVisualCaptureRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession6OutfitVisualCaptureRunner();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    void Start();

private:
    struct FFileStamp
    {
        int64 Size = INDEX_NONE;
        int64 TimestampTicks = 0;
        bool operator==(const FFileStamp& Other) const
        {
            return Size == Other.Size && TimestampTicks == Other.TimestampTicks;
        }
    };

    struct FCapture
    {
        struct FKeyBoneProjection
        {
            FString Bone;
            FVector2D Screen = FVector2D::ZeroVector;
            bool bProjected = false;
            bool bInsideViewport = false;
            bool bInsideSafeMargin = false;
        };

        FString Filename;
        FString Evidence;
        FString Sha1;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        int32 ExpectedEquippedCount = 0;
        int32 VisibleEquippedComponentCount = 0;
        int32 ExpectedSkeletalOutfitComponentCount = 0;
        int32 VisibleSkeletalOutfitComponentCount = 0;
        int32 ExpectedSkeletalMaterialSlotCount = 0;
        int32 CanonicalMaterialMidCount = 0;
        int32 SkeletalUsageReadyMidCount = 0;
        int32 VariantParameterMatchedMidCount = 0;
        int32 ProjectedKeyBoneCount = 0;
        int32 KeyBonesInsideViewport = 0;
        int32 KeyBonesInsideSafeMargin = 0;
        float SubjectScreenHeightFraction = 0.0f;
        float SubjectScreenWidthFraction = 0.0f;
        FVector2D ProjectedBoundsMin = FVector2D::ZeroVector;
        FVector2D ProjectedBoundsMax = FVector2D::ZeroVector;
        float MontagePositionSeconds = -1.0f;
        float BodyHeightCm = 0.0f;
        float CameraFovDeg = 0.0f;
        FString ThrowPhase;
        bool bBodyFramed = false;
        bool bFinitePose = false;
        bool bOutfitComponentsVisible = false;
        bool bVisibleSkeletalOutfitMaterialsAreCanonicalMids = false;
        bool bCanonicalMaterialHasSkeletalMeshUsage = false;
        bool bSelectedVariantMaterialParametersMatch = false;
        bool bCreatorOutfitTabRequired = false;
        bool bCreatorOutfitTabPrepared = false;
        bool bCreatorViewTargetIsGolfer = false;
        bool bCreatorPreviewComposedInRightPane = false;
        TArray<FString> SelectedSkeletalVariants;
        TArray<FString> SkeletalMaterialDiagnostics;
        TArray<FKeyBoneProjection> KeyBoneProjections;
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ThrowAdapter;

    bool bStarted = false;
    bool bFinished = false;
    bool bCapturePending = false;
    bool bCaptureIssued = false;
    bool bCreatorWasOpened = false;
    bool bPreviewCameraActive = false;
    bool bVisualThrowStarted = false;
    bool bPersistentSnapshotReady = false;
    bool bCreatorOutfitTabPrepared = false;
    bool bCapturePausedWorld = false;
    bool bCreatorApplySaved = false;
    bool bCreatorApplyReloaded = false;
    bool bCreatorReloadReconstructedOutfit = false;
    bool bValidationTempSlotDeleted = false;
    bool bCreatorCancelRestoredApplied = false;
    bool bValidationCaptureFovLocked = false;
    int32 PendingCaptureIndex = INDEX_NONE;
    int32 BaselineDiscs = 0;
    int32 BaselineStrokes = 0;
    int32 ValidationReleaseCallbacks = 0;
    int32 PausedThrowCaptureCount = 0;
    double PendingStartedSeconds = 0.0;
    float OpeningCameraFovDeg = 0.0f;
    FString PendingCapturePath;
    FString OutputDirectory;
    FString ManifestPath;
    FDGOutfitLoadout OpeningOutfit;
    FDGOutfitLoadout CreatorAppliedOutfit;
    FDGBodyProfile OpeningBody;
    FDGThrowStyle OpeningStyle;
    EDGHandedness OpeningHandedness = EDGHandedness::Right;
    TArray<FCapture> Captures;
    TMap<FString, FFileStamp> InitialPackages;
    TMap<FString, FFileStamp> InitialSaveGames;
    TArray<FString> ChangedPackages;
    TArray<FString> ChangedSaveGames;

    bool PrepareCapture(int32 Index);
    bool ApplySlots(
        const TArray<EDGOutfitSlot>& Slots,
        int32 ItemIndex,
        int32 VariantIndex,
        FString& OutError);
    bool ApplyFullOutfit(int32 VariantIndex, FString& OutError);
    bool ApplyProfileAsset(const TCHAR* AssetPath, FString& OutError);
    bool BeginVisualThrow(FString& OutError);
    bool ProveCreatorApplyReloadCancel(FString& OutError);
    bool PauseWorldForThrowCapture(FString& OutError);
    void ResumeWorldAfterThrowCapture(bool bResumeMontage);
    bool HandleValidationLaunch(
        const FThrowCommand& Command,
        const FTransform& GripWorldTransform);
    void RequestCapture(int32 Index, const FString& Evidence);
    bool PollCapture();
    bool ValidateCaptureScene(int32 Index, FCapture& InOutCapture, FString& OutError) const;
    bool ValidateVisibleSkeletalOutfitMaterials(
        FCapture& InOutCapture,
        FString& OutError) const;
    int32 CountVisibleEquippedComponents() const;
    float GetActiveMontagePosition() const;
    FString GetCurrentThrowPhaseLabel() const;
    void ShowEvidenceLabel(const FString& Heading, const FString& Detail) const;
    int32 CountWorldDiscs() const;
    void SnapshotFiles(
        const FString& Root,
        bool bPackagesOnly,
        TMap<FString, FFileStamp>& Out) const;
    void DiffFiles(
        const TMap<FString, FFileStamp>& Before,
        const TMap<FString, FFileStamp>& After,
        TArray<FString>& Out) const;
    bool VerifyNoPersistentWrites();
    void RestoreRuntimeState();
    void WriteManifest(bool bPassed, const FString& Error);
    void Fail(const FString& Reason);
    void Pass();
};
