#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfDiscDefinition.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfDiscDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName DiscId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName BrandId = TEXT("premium_disc_golf");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    EDGDiscClass DiscClass = EDGDiscClass::Midrange;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    FDGDiscFlightNumbers FlightNumbers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    FDGDiscGeometry Geometry;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    FDGDiscAerodynamics Aerodynamics;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    float MinMassGrams = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    float MaxMassGrams = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<UStaticMesh> DiscMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FName DefaultStampId = TEXT("premium_default");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Development")
    bool bDevelopmentPlaceholder = true;
};
