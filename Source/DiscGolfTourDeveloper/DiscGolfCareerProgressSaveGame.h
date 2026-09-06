#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfCareerProgressSaveGame.generated.h"

/** Isolated schema-v1 career proof. It deliberately does not alter the production profile save. */
UCLASS()
class DISCGOLFTOURDEVELOPER_API UDiscGolfCareerProgressSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    static constexpr int32 CurrentSchemaVersion = 1;

    UPROPERTY(BlueprintReadOnly, SaveGame) int32 SchemaVersion = CurrentSchemaVersion;
    UPROPERTY(BlueprintReadOnly, SaveGame) FDGCareerProgress Progress;
};
