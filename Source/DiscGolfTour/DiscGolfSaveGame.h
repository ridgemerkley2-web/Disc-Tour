#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DiscGolfTypes.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfPlayerExperience.h"
#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfSaveGame.generated.h"

namespace DiscGolfSaveSchema
{
    inline constexpr int32 CurrentVersion = 10;

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
    /** Schema-10 authority for the complete character; contains stable IDs and values only. */
    UPROPERTY(BlueprintReadWrite, SaveGame) FDGFullCharacterCustomization CharacterCustomization;

    // Schema-8 migration and accepted Session 4/6 compatibility mirrors.
    // Keep both names and SaveGame flags: CPF_Deprecated would prevent old
    // archives from loading these fields. Schema-10 runtime reads the complete
    // CharacterCustomization payload exclusively.
    UPROPERTY(BlueprintReadWrite, SaveGame) FDiscGolfCharacterProfileSaveData CharacterProfile;
    UPROPERTY(BlueprintReadWrite, SaveGame) FDGOutfitLoadout OutfitLoadout;
};
