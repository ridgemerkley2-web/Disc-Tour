#pragma once

#include "CoreMinimal.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfSession3SmokeRunner.h"
#include "DiscGolfSession6OutfitSmokeRunner.generated.h"

class UDiscBagComponent;

/**
 * Session 6 adds outfit preflight around the accepted Session 3 one-throw
 * contract. Each command-line fixture owns one process, so a failed cosmetic
 * row cannot contaminate another body profile or the missing-item trial.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession6OutfitSmokeRunner
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
        FTransform GripComponentTransform = FTransform::Identity;
    };

    FString RequestedFixture;
    FString RequestedProfile;
    FDGOutfitLoadout ExpectedCanonicalLoadout;
    FThrowAuthoritySnapshot AuthorityBeforeOutfit;
    int32 ExpectedEquippedCount = 0;
    bool bMissingItemFixture = false;
    bool bOutfitPreflightPassed = false;

    bool BuildFixtureLoadout(FDGOutfitLoadout& OutLoadout, FString& OutError) const;
    bool ValidateRequestedBodyProfile(FString& OutError) const;
    bool AddFirstCompatibleItem(
        EDGOutfitSlot Slot,
        int32 ItemIndex,
        int32 VariantIndex,
        FDGOutfitLoadout& InOutLoadout,
        FString& OutError) const;
    bool SnapshotThrowAuthority(FThrowAuthoritySnapshot& OutSnapshot) const;
    bool ValidateThrowAuthorityUnchanged(FString& OutError) const;
    bool ValidateEquippedPresentation(FString& OutError) const;
};
