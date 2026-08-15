#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfPlasticDefinition.generated.h"

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfPlasticDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName PlasticId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName BrandId = TEXT("premium_disc_golf");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plastic")
    FDGPlasticPerformance Performance;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    float Opacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    float Roughness = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Development")
    bool bDevelopmentPlaceholder = true;
};
