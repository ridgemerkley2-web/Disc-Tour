#pragma once

#include "CoreMinimal.h"
#include "DiscGolfRoundState.generated.h"

struct FDiscGolfHoleBlockoutDefinition;

USTRUCT(BlueprintType)
struct FDiscGolfRoundHoleScore
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HoleNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText HoleName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Par = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Strokes = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PenaltyStrokes = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCompleted = false;

    int32 ScoreToPar() const { return bCompleted ? Strokes - Par : 0; }
};

USTRUCT(BlueprintType)
struct FDiscGolfRoundState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CourseId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LayoutId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText CourseName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CurrentHoleIndex = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfRoundHoleScore> HoleScores;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRoundComplete = false;
};

namespace DiscGolfRound
{
    DISCGOLFTOUR_API bool Initialize(
        FDiscGolfRoundState& OutRound,
        FName CourseId,
        FName LayoutId,
        const FText& CourseName,
        const TArray<FDiscGolfHoleBlockoutDefinition>& HoleDefinitions,
        FString& OutError);
    DISCGOLFTOUR_API bool RecordCurrentHole(
        FDiscGolfRoundState& Round,
        int32 Strokes,
        int32 PenaltyStrokes,
        FString& OutError);
    DISCGOLFTOUR_API bool CanAdvance(const FDiscGolfRoundState& Round);
    DISCGOLFTOUR_API bool Advance(FDiscGolfRoundState& Round, FString& OutError);
    DISCGOLFTOUR_API int32 CompletedHoleCount(const FDiscGolfRoundState& Round);
    DISCGOLFTOUR_API int32 TotalStrokes(const FDiscGolfRoundState& Round);
    DISCGOLFTOUR_API int32 TotalPenaltyStrokes(const FDiscGolfRoundState& Round);
    DISCGOLFTOUR_API int32 CompletedPar(const FDiscGolfRoundState& Round);
    DISCGOLFTOUR_API int32 ScoreToPar(const FDiscGolfRoundState& Round);
    DISCGOLFTOUR_API FString ScoreLabel(int32 ScoreToPar);
}
