#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "Engine/DataAsset.h"
#include "DiscGolfAnimationLibrary.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGThrowAnimationEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGThrowType ThrowType = EDGThrowType::Backhand;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGHandedness Handedness = EDGHandedness::Right;
    /** Stable presentation family such as Drive, Approach, or Putt. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MotionFamilyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName StyleId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecommendedPowerMin = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RecommendedPowerMax = 1.0f;
};

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfAnimationLibrary : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGThrowAnimationEntry> Entries;
};
