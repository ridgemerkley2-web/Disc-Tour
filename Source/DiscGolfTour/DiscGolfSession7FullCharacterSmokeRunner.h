#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfSession3SmokeRunner.h"
#include "DiscGolfSession7FullCharacterSmokeRunner.generated.h"

class ADiscGolfTourPlayerController;
class UDiscBagComponent;
class UDiscGolfSaveGame;
class USkeletalMesh;

/**
 * Session 7's isolated 12-row preflight around the accepted Session 3 throw.
 *
 * The runner may write only the command-line validated GUID save slot. It
 * deletes that slot before delegating to the real one-throw path and again on
 * every exit. Cosmetics never author a throw, spawn a disc, or advance play.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfSession7FullCharacterSmokeRunner
    : public ADiscGolfSession3SmokeRunner
{
    GENERATED_BODY()

public:
    virtual void Start() override;

protected:
    virtual void Fail(const FString& Reason) override;
    virtual void Pass() override;

private:
    struct FThrowAuthoritySnapshot
    {
        float Power01 = 0.0f;
        float HyzerDeg = 0.0f;
        float NoseDeg = 0.0f;
        float LaunchAngleDeg = 0.0f;
        EThrowStyle ThrowStyle = EThrowStyle::Backhand;
        EDiscShotContext ShotContext = EDiscShotContext::Drive;
        FVector AimDirection = FVector::ZeroVector;
        TObjectPtr<UDiscBagComponent> DiscBag;
        TObjectPtr<USkeletalMesh> GripSkeletalMesh;
        int32 GripBoneIndex = INDEX_NONE;
        FTransform GripComponentTransform = FTransform::Identity;
    };

    UPROPERTY() TObjectPtr<ADiscGolfTourPlayerController> PlayerController;
    FString RequestedFixture;
    FString RequestedProfile;
    FString ValidationSaveSlot;
    FString FixturePassTokens;
    FDGFullCharacterCustomization OpeningCustomization;
    FDGFullCharacterCustomization ExpectedCustomization;
    FThrowAuthoritySnapshot AuthorityBeforeCustomization;
    TObjectPtr<UActorComponent> OpeningCustomizationAuthority;
    TObjectPtr<UActorComponent> OpeningOutfitAuthority;
    bool bPreflightPassed = false;
    bool bHairHatSelectionPreserved = false;
    bool bValidationTempSlotDeleted = false;

    bool ValidateRequestedProfile(FString& OutError) const;
    bool SnapshotThrowAuthority(FThrowAuthoritySnapshot& OutSnapshot) const;
    bool ValidateThrowAuthorityUnchanged(FString& OutError) const;
    bool RunHairCoveragePreservationTrial(FString& OutError);
    bool BuildAndApplyFixture(FString& OutError);
    bool RunSchema8MigrationTrial(FString& OutError);
    bool RunRandomizeApplyReloadTrial(FString& OutError);
    bool RunRandomizeCancelTrial(FString& OutError);
    bool BuildOutfit(
        bool bFull,
        bool bIncludeHeadwear,
        FDGOutfitLoadout& OutLoadout,
        FString& OutError) const;
    bool AddFirstCompatibleOutfitItem(
        EDGOutfitSlot Slot,
        int32 VariantIndex,
        FDGOutfitLoadout& InOutLoadout,
        FString& OutError) const;
    bool ValidateRuntimePresentation(FString& OutError) const;
    bool ValidateSelectedCosmeticComponents(FString& OutError) const;
    bool CleanupValidationSaveSlot();
    FString GetValidationSavePath() const;
};
