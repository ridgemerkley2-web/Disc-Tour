#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession7FullCharacterVisualCaptureRunner.generated.h"

class ADiscGolferPawn;
class ADiscGolfTourGameMode;
class ADiscGolfTourPlayerController;
class UDiscGolfRHBHThrowAdapterComponent;

/** Exact, rendered, isolated 24-shot Session 7 evidence harness. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession7FullCharacterVisualCaptureRunner
    : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfSession7FullCharacterVisualCaptureRunner();
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
            return Size == Other.Size
                && TimestampTicks == Other.TimestampTicks;
        }
    };

    struct FCapture
    {
        FString Filename;
        FString Evidence;
        FString Sha1;
        FString ActiveTab;
        FString ProfileId;
        FString ThrowPhase;
        FString HairStyleId;
        FString HeadwearId;
        TArray<FString> VisibleControlIds;
        TMap<FName, float> VisibleProxyMorphValues;
        FLinearColor HairColor = FLinearColor::Black;
        FLinearColor SkinTone = FLinearColor::Black;
        FLinearColor EyeColor = FLinearColor::Black;
        FIntRect HeadRoi;
        int64 Bytes = 0;
        int32 Width = 0;
        int32 Height = 0;
        int32 ActiveTabIndex = INDEX_NONE;
        int32 MinimumVisibleControlCount = 0;
        int32 EquippedOutfitCount = 0;
        int32 ValidatedPresentationComponentCount = 0;
        float BodyHeightCm = 0.0f;
        float MontagePositionSeconds = -1.0f;
        float CameraFovDeg = 0.0f;
        bool bCreatorOpen = false;
        bool bPreviewInRightPane = false;
        bool bViewTargetIsGolfer = false;
        bool bExpectedControlsVisible = false;
        bool bHeadCloseup = false;
        bool bCameraFovValid = false;
        bool bHeadFramed = false;
        bool bBodyFramed = false;
        bool bFinitePose = false;
        bool bOneCustomizationComponent = false;
        bool bOneOutfitComponent = false;
        bool bHeadLeaderPose = false;
        bool bVisibleProxyMorphsApplied = false;
        bool bCosmeticsAttached = false;
        bool bCosmeticsCollisionFree = false;
        bool bPresentationMaterialsValid = false;
        bool bHeadMaterialIsCanonicalMid = false;
        bool bHeadMaterialHasMorphTargetUsage = false;
        bool bOutfitAttachmentsAndMaterialsValid = false;
        bool bHairHiddenByOutfitCoverage = false;
        bool bHairComponentVisible = false;
        int32 WorldDiscDelta = 0;
        int32 StrokeDelta = 0;
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourGameMode> GameMode;
    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    UPROPERTY() TObjectPtr<ADiscGolferPawn> Golfer;
    UPROPERTY() TObjectPtr<UDiscGolfRHBHThrowAdapterComponent> ThrowAdapter;

    bool bStarted = false;
    bool bFinished = false;
    bool bCapturePending = false;
    bool bCaptureIssued = false;
    bool bCapturePausedWorld = false;
    bool bPreviewCameraActive = false;
    bool bVisualThrowStarted = false;
    bool bPersistentSnapshotReady = false;
    bool bValidationCaptureFovLocked = false;
    bool bDraftPreservedAcrossTabs = false;
    bool bCurrentTabResetScoped = false;
    bool bResetAllRestoredDefaults = false;
    bool bRandomizeLocksRespected = false;
    bool bRandomizeCatalogValid = false;
    bool bCreatorApplySaved = false;
    bool bCreatorApplyReloaded = false;
    bool bCreatorCancelRestoredApplied = false;
    bool bSchema8MigrationReloaded = false;
    bool bMissingCosmeticFallbacksResolved = false;
    bool bValidationTempSlotDeleted = false;
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
    FString ValidationSaveSlot;
    FString ActiveProfileId = TEXT("Baseline");
    FDGFullCharacterCustomization OpeningCustomization;
    FDGFullCharacterCustomization EvidenceDraft;
    TArray<FCapture> Captures;
    TMap<FString, FFileStamp> InitialPackages;
    TMap<FString, FFileStamp> InitialSaveGames;
    TArray<FString> ChangedPackages;
    TArray<FString> ChangedSaveGames;

    bool ProveCreatorAndPersistenceContracts(FString& OutError);
    bool ProveSchema8Migration(FString& OutError);
    bool ProveMissingFallbacks(FString& OutError);
    bool PrepareCapture(int32 Index);
    bool PrepareCreatorDraft(
        int32 TabIndex,
        const FDGFullCharacterCustomization& Draft,
        FString& OutError);
    bool ApplyCompleteProfile(
        const TCHAR* ProfileAssetPath,
        FName ProfileId,
        int32 OutfitVariantIndex,
        FString& OutError);
    bool BuildFullOutfit(
        const FDGBodyProfile& Body,
        bool bIncludeHeadwear,
        int32 VariantIndex,
        FDGOutfitLoadout& OutLoadout,
        FString& OutError) const;
    bool SetFirstOutfitItem(
        EDGOutfitSlot Slot,
        const FDGBodyProfile& Body,
        int32 VariantIndex,
        FDGOutfitLoadout& InOutLoadout,
        FString& OutError) const;
    bool SetCap(FDGFullCharacterCustomization& InOutDraft, FString& OutError) const;
    bool BeginVisualThrow(FString& OutError);
    bool HandleValidationLaunch(
        const FThrowCommand& Command,
        const FTransform& GripWorldTransform);
    bool PauseWorldForThrowCapture(FString& OutError);
    void ResumeWorldAfterThrowCapture(bool bResumeMontage);
    void RequestCapture(int32 Index);
    bool PollCapture();
    bool ValidateCaptureScene(
        int32 Index,
        FCapture& InOutCapture,
        FString& OutError) const;
    bool ValidateCosmeticComponents(
        bool& bOutAttached,
        bool& bOutCollisionFree,
        bool& bOutMaterialsValid,
        FString& OutError) const;
    void GetExpectedControlIds(int32 TabIndex, TArray<FString>& OutIds) const;
    FString GetThrowPhaseLabel() const;
    float GetActiveMontagePosition() const;
    int32 CountWorldDiscs() const;
    bool HasVisibleCustomizationComponent(FName ItemId) const;
    FString GetHeadwearId() const;
    bool CleanupValidationSaveSlot();
    FString GetValidationSavePath() const;
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
