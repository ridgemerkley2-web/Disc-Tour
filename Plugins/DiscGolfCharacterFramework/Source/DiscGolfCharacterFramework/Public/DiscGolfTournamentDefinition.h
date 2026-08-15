#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCompetitionTypes.h"
#include "DiscGolfTournamentDefinition.generated.h"

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfTournamentDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tournament")
    FName EventId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tournament")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tournament")
    FName PresentingBrandId = TEXT("premium_disc_golf");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tournament")
    EDGCompetitionFormat Format = EDGCompetitionFormat::StrokePlay;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tournament")
    TArray<FDGTournamentRoundDefinition> Rounds;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tournament")
    TArray<int32> PayoutByPlace;
};
