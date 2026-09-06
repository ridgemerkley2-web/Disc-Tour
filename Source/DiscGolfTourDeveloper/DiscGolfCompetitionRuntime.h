#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfCompetitionRuntime.generated.h"

struct FDiscGolfRoundState;

/** Project-owned, value-only Session 14 event definition. It never owns scoring authority. */
USTRUCT(BlueprintType)
struct DISCGOLFTOURDEVELOPER_API FDiscGolfCompetitionRuntimeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EventId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PresentingBrandId = TEXT("dg_generic");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGCompetitionFormat Format = EDGCompetitionFormat::StrokePlay;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGTournamentRoundDefinition> Rounds;

    /** Session 14 has exactly one round, so its contiguous par contract is stored here. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<int32> HolePars;
};

namespace DiscGolfCompetitionRuntime
{
    inline constexpr int32 CurrentSchemaVersion = 1;
    inline constexpr int32 Session14HoleCount = 3;

    /** Original, generic source fallback. No unapproved or real-world brand is activated. */
    DISCGOLFTOURDEVELOPER_API FDiscGolfCompetitionRuntimeDefinition BuildSourceFallbackEvent();

    /** Strict read-only validation of the bounded Session 14 source event. */
    DISCGOLFTOURDEVELOPER_API bool ValidateEvent(
        const FDiscGolfCompetitionRuntimeDefinition& Event,
        FString& OutError);

    /**
     * Converts the completed project round into framework scorecard data atomically.
     * Project Strokes already includes penalties, so base strokes are derived by subtraction.
     */
    DISCGOLFTOURDEVELOPER_API bool BuildCompletedScorecard(
        const FDiscGolfCompetitionRuntimeDefinition& Event,
        const FDiscGolfRoundState& Round,
        FDGRoundScorecard& OutScorecard,
        FString& OutError);

    /** Strict read-only scorecard validation against the single source event. */
    DISCGOLFTOURDEVELOPER_API bool ValidateCompletedScorecard(
        const FDiscGolfCompetitionRuntimeDefinition& Event,
        const FDGRoundScorecard& Scorecard,
        FString& OutError);

    /** Stable identity used to reject duplicate persisted round results. */
    DISCGOLFTOURDEVELOPER_API FName MakeResultId(const FDGRoundScorecard& Scorecard);
}
