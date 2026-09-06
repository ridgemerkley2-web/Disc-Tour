#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCompetitionTypes.generated.h"

UENUM(BlueprintType)
enum class EDGCompetitionFormat : uint8
{
    StrokePlay,
    MatchPlay
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGTournamentRoundDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RoundNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CourseId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TeeSetId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HoleCount = 0;
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGScorecardHole
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 HoleNumber = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Par = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Strokes = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 PenaltyStrokes = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bComplete = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGRoundScorecard
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName EventId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName CourseId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 RoundNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FDGScorecardHole> Holes;
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGSponsorshipProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName BrandId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Tier = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Reputation = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bActive = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDGCareerProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SeasonNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 CareerCurrency = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float PlayerRating = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 WorldRank = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FName> CompletedEventIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FDGRoundScorecard> RoundHistory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FDGSponsorshipProgress> Sponsorships;
};
