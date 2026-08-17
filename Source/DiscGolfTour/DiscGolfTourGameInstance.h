#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfPlayerExperience.h"
#include "Engine/GameInstance.h"
#include "DiscGolfTourGameInstance.generated.h"

class UDiscGolfSaveGame;

namespace DiscGolfProfilePersistence
{
    enum class EMigrationResult : uint8
    {
        AlreadyCurrent,
        Migrated,
        FutureSchemaRejected
    };

    /** Pure in-memory migration seam used by Init and focused automation. */
    DISCGOLFTOUR_API EMigrationResult MigrateToCurrent(UDiscGolfSaveGame& InOutProfile);

    /**
     * Resolves the narrowly gated Session 6 validation slot override.
     * Returns false and clears OutSaveSlot unless the visual-capture and
     * no-save guards are both present and the requested slot is safe.
     */
    DISCGOLFTOUR_API bool TryResolveSession6OutfitValidationSaveSlot(
        const TCHAR* CommandLine,
        FString& OutSaveSlot);

    /** Strict isolated-slot grammar for Session 7 visual and throw validation. */
    DISCGOLFTOUR_API bool TryResolveSession7FullCharacterValidationSaveSlot(
        const TCHAR* CommandLine,
        FString& OutSaveSlot);
}

UCLASS()
class DISCGOLFTOUR_API UDiscGolfTourGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    UFUNCTION(BlueprintCallable) void SaveProfile();
    UFUNCTION(BlueprintPure) UDiscGolfSaveGame* GetProfile() const { return Profile; }
    UFUNCTION(BlueprintPure) FDiscGolfPlayerSettings GetPlayerSettings() const;
    UFUNCTION(BlueprintCallable) void UpdatePlayerSettings(const FDiscGolfPlayerSettings& Settings);

    UFUNCTION(BlueprintPure)
    FDiscGolfCharacterProfileSaveData GetCharacterProfile() const;

    UFUNCTION(BlueprintPure)
    FDGFullCharacterCustomization GetFullCharacterCustomization() const;

    /** Sole schema-9 full-character save transaction. */
    UFUNCTION(BlueprintCallable)
    bool UpdateFullCharacterCustomization(
        const FDGFullCharacterCustomization& CharacterCustomization);

    UFUNCTION(BlueprintCallable)
    bool UpdateCharacterProfile(const FDiscGolfCharacterProfileSaveData& CharacterProfile);

    /** Framework-type convenience boundary for the pawn; not a second stored profile. */
    bool GetCharacterProfile(
        FDGBodyProfile& OutBody,
        FDGThrowStyle& OutThrowStyle,
        EDGHandedness& OutHandedness) const;

    bool UpdateCharacterProfile(
        const FDGBodyProfile& Body,
        const FDGThrowStyle& ThrowStyle,
        EDGHandedness Handedness);

    UFUNCTION(BlueprintPure)
    FDGOutfitLoadout GetOutfitLoadout() const;

    /** One authoritative save transaction for the existing profile plus modular outfit IDs. */
    UFUNCTION(BlueprintCallable)
    bool UpdateCharacterProfileAndOutfit(
        const FDiscGolfCharacterProfileSaveData& CharacterProfile,
        const FDGOutfitLoadout& OutfitLoadout);

    /** Empty in normal gameplay; read-only C++ seam for Session 6 cleanup/proof. */
    FString GetSession6OutfitValidationSaveSlot() const
    {
        return bUsingSession6OutfitValidationSaveSlot ? SaveSlot : FString();
    }

    /** Empty in normal gameplay; read-only C++ seam for Session 7 cleanup/proof. */
    FString GetSession7FullCharacterValidationSaveSlot() const
    {
        return bUsingSession7FullCharacterValidationSaveSlot ? SaveSlot : FString();
    }

private:
    bool SaveProfileInternal();
    UPROPERTY() TObjectPtr<UDiscGolfSaveGame> Profile;
    FString SaveSlot = TEXT("DiscGolfTour_Profile_0");
    bool bUsingSession6OutfitValidationSaveSlot = false;
    bool bUsingSession7FullCharacterValidationSaveSlot = false;
};
