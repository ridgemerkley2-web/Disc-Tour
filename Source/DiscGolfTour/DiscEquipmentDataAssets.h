#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfTypes.h"
#include "DiscEquipmentDataAssets.generated.h"

/** Cooked, stable-ID definition for one disc mold. */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API UDiscMoldDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    FDiscMoldDefinition Mold;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};

/** Cooked, stable-ID definition for one plastic family. */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API UDiscPlasticDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    FDiscPlasticDefinition PlasticDefinition;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
