#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscThrowLabTypes.h"
#include "DiscThrowLabSaveGame.generated.h"

/** Separate development save. It deliberately does not participate in the player profile schema. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API UDiscThrowLabSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 1;

    UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = CurrentSchemaVersion;
    UPROPERTY(BlueprintReadOnly, SaveGame) TArray<FDiscThrowLabRecord> Records;
    UPROPERTY(BlueprintReadOnly, SaveGame) FString SelectedRecordId;
    UPROPERTY(BlueprintReadOnly, SaveGame) FString FirstComparisonRecordId;
    UPROPERTY(BlueprintReadOnly, SaveGame) FString SecondComparisonRecordId;
};
