#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfCharacterSaveGame.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCharacterSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // v1.2 full character-creator save payload.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Character")
    FDGFullCharacterCustomization Character;
};
