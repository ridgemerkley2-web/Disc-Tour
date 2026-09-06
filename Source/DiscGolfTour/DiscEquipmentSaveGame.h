#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfDiscTypes.h"
#include "DiscEquipmentSaveGame.generated.h"

/**
 * Isolated Session 11 equipment payload. The production player profile remains
 * schema 10; this value store can be rejected atomically without risking round,
 * character, settings, or entitlement data.
 */
UCLASS()
class DISCGOLFTOUR_API UDiscEquipmentSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 1;

    UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = CurrentSchemaVersion;
    UPROPERTY(BlueprintReadOnly, SaveGame) FDGDiscBagLoadout Loadout;
};
