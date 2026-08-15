#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCosmeticItem.h"
#include "DiscGolfCosmeticCatalog.generated.h"

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCosmeticCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cosmetics")
    TArray<TSoftObjectPtr<UDiscGolfCosmeticItem>> Items;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Cosmetics")
    UDiscGolfCosmeticItem* FindById(FName ItemId) const;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Cosmetics")
    TArray<UDiscGolfCosmeticItem*> GetByKind(EDGCosmeticKind Kind) const;
};
