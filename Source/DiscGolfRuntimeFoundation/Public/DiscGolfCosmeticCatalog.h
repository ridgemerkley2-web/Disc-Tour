#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCosmeticItem.h"
#include "DiscGolfCosmeticCatalog.generated.h"

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfCosmeticCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TSoftObjectPtr<UDiscGolfCosmeticItem>> Items;

    UFUNCTION(BlueprintPure) UDiscGolfCosmeticItem* FindById(FName ItemId) const;
    UFUNCTION(BlueprintPure) TArray<UDiscGolfCosmeticItem*> GetByKind(EDGCosmeticKind Kind) const;
};
