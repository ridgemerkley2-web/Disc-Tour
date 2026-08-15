#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfFlightRegressionSuite.generated.h"

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGMetricRange
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float Minimum = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float Maximum = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    bool bEnabled = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGFlightRegressionCase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FName CaseId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FName DiscDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FName PlasticId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float MassGrams = 175.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float ReleaseSpeedMps = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float SpinRpm = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float HyzerDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    float NoseDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FVector WindVelocityMps = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FDGMetricRange CarryDistanceM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FDGMetricRange TotalDistanceM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FDGMetricRange ApexHeightM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FDGMetricRange FlightTimeSeconds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    FDGMetricRange LateralDeviationM;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Regression")
    bool bCalibrated = false;
};

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfFlightRegressionSuite : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Regression")
    TArray<FDGFlightRegressionCase> Cases;
};
