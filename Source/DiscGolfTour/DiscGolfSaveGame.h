#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfTypes.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfSaveGame.generated.h"

namespace DiscGolfSaveSchema
{
    inline constexpr int32 CurrentVersion = 8;

    constexpr bool IsCurrent(const int32 Version)
    {
        return Version == CurrentVersion;
    }
}

UCLASS()
class DISCGOLFTOUR_API UDiscGolfSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 SaveSchemaVersion = DiscGolfSaveSchema::CurrentVersion;
    UPROPERTY(BlueprintReadWrite) FString PlayerName = TEXT("Player");
    UPROPERTY(BlueprintReadWrite) int32 CareerXP = 0;
    UPROPERTY(BlueprintReadWrite) int32 CareerCash = 0;
    UPROPERTY(BlueprintReadWrite) TArray<FName> UnlockedMolds;
    UPROPERTY(BlueprintReadWrite) int32 PreferredGraphicsPreset = 2;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bHasPracticeRoundSnapshot = false;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 PracticeStrokes = 0;
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 PracticePenaltyStrokes = 0;
    UPROPERTY(BlueprintReadWrite, SaveGame) bool bPracticeHoleComplete = false;
    UPROPERTY(BlueprintReadWrite, SaveGame) FDiscGolfLieState PracticeLieState;
    UPROPERTY(BlueprintReadWrite, SaveGame) FName PracticeCourseId = TEXT("RegressionCourse");
    UPROPERTY(BlueprintReadWrite, SaveGame) FName PracticeLayoutId = TEXT("Practice");
    UPROPERTY(BlueprintReadWrite, SaveGame) int32 PracticeHoleNumber = 1;
    UPROPERTY(BlueprintReadWrite, SaveGame) FDiscGolfRoundState PracticeRoundState;
    UPROPERTY(BlueprintReadWrite, SaveGame) FName PracticeMoldId = TEXT("Apex");
    UPROPERTY(BlueprintReadWrite, SaveGame) EDiscPlastic PracticePlastic = EDiscPlastic::Tour;
    UPROPERTY(BlueprintReadWrite, SaveGame) FDiscGolfPlayerSettings PlayerSettings;
    UPROPERTY(BlueprintReadWrite, SaveGame) FDiscGolfCharacterProfileSaveData CharacterProfile;
    /** Stable catalog IDs only; visual assets are resolved by the installed outfit framework. */
    UPROPERTY(BlueprintReadWrite, SaveGame) FDGOutfitLoadout OutfitLoadout;
};
