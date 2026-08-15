#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCompetitionTypes.generated.h"

UENUM(BlueprintType)
enum class EDGCompetitionFormat : uint8
{
    StrokePlay,
    MatchPlay,
    Skins,
    DoublesBestShot
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGScorecardHole
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Score")
    int32 HoleNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Score")
    int32 Par = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Score")
    int32 Strokes = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Score")
    int32 PenaltyStrokes = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Score")
    bool bComplete = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGRoundScorecard
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Round")
    FName EventId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Round")
    FName CourseId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Round")
    int32 RoundNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Round")
    TArray<FDGScorecardHole> Holes;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGTournamentRoundDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tournament")
    int32 RoundNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tournament")
    FName CourseId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tournament")
    FName TeeSetId = TEXT("default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tournament")
    int32 HoleCount = 18;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGSponsorshipProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    FName BrandId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    int32 Tier = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    float Reputation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    bool bActive = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGCareerProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    int32 SeasonNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    int32 CareerCurrency = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    float PlayerRating = 850.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    int32 WorldRank = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    TArray<FName> CompletedEventIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    TArray<FDGRoundScorecard> RoundHistory;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Career")
    TArray<FDGSponsorshipProgress> Sponsorships;
};
