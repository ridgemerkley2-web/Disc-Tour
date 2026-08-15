#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfAnimationLibrary.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGThrowAnimationEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    EDGThrowType ThrowType = EDGThrowType::Backhand;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    FName StyleId = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    TSoftObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.0", ClampMax="1.0"))
    float RecommendedPowerMin = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation", meta=(ClampMin="0.0", ClampMax="1.0"))
    float RecommendedPowerMax = 1.0f;
};

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAnimationLibrary : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    TArray<FDGThrowAnimationEntry> Entries;
};
