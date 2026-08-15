#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfCareerSaveGame.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCareerSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    FDGCareerProgress Progress;
};
