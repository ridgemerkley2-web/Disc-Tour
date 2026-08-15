#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfDiscCatalog.generated.h"

class UDiscGolfDiscDefinition;
class UDiscGolfPlasticDefinition;

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfDiscCatalog : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc")
    TArray<TSoftObjectPtr<UDiscGolfDiscDefinition>> DiscDefinitions;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Plastic")
    TArray<TSoftObjectPtr<UDiscGolfPlasticDefinition>> PlasticDefinitions;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Catalog")
    UDiscGolfDiscDefinition* FindDiscById(FName DiscId) const;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Catalog")
    UDiscGolfPlasticDefinition* FindPlasticById(FName PlasticId) const;
};
