#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCourseTypes.h"
#include "DGFrameworkCourseDefinition.generated.h"

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCourseDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    FName CourseId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    FName BrandId = TEXT("premium_disc_golf");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    FName BiomeId = TEXT("temperate_forest");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Course")
    TArray<FDGHoleDefinition> Holes;
};
