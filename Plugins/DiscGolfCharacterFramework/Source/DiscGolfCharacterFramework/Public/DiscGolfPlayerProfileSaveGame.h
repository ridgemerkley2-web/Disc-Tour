#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfSettingsTypes.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfPlayerProfileSaveGame.generated.h"

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfPlayerProfileSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    int32 SchemaVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    FDGFullCharacterCustomization Character;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    FDGDiscBagLoadout Bag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    FDGUserSettingsProfile Settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    FDGCareerProgress Career;
};
