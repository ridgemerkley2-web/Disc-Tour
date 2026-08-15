#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfTelemetryTypes.h"
#include "DiscGolfThrowLabSaveGame.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfThrowLabSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw Lab")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Throw Lab")
    TArray<FDGThrowTelemetryRecord> SavedThrows;
};
