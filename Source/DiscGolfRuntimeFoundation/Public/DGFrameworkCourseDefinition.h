#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.h"
#include "Engine/DataAsset.h"
#include "DGFrameworkCourseDefinition.generated.h"

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfCourseDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CourseId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BrandId = TEXT("dg_generic");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BiomeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGHoleDefinition> Holes;
};
