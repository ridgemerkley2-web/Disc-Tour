#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterProfileRuntime.h"
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

private:
    bool SaveProfileInternal();
    UPROPERTY() TObjectPtr<UDiscGolfSaveGame> Profile;
    FString SaveSlot = TEXT("DiscGolfTour_Profile_0");
};
