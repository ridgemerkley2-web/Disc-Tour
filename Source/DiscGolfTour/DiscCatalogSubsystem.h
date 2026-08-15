#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscGolfTypes.h"
#include "DiscCatalogSubsystem.generated.h"

UCLASS()
class DISCGOLFTOUR_API UDiscCatalogSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category="Disc Catalog")
    bool ResolveDisc(FName MoldId, EDiscPlastic Plastic, FResolvedDiscDefinition& OutDisc) const;

    const TArray<FDiscMoldDefinition>& GetFallbackCatalog() const { return FallbackCatalog; }
    const TArray<FDiscMoldDefinition>& GetActiveCatalog() const { return ActiveCatalog; }
    const TArray<FDiscPlasticDefinition>& GetActivePlasticCatalog() const { return ActivePlasticCatalog; }
    bool IsUsingPrimaryAssets() const { return bUsingPrimaryAssets; }
    const FString& GetCatalogSourceText() const { return CatalogSourceText; }

    static void BuildFallbackDefinitions(
        TArray<FDiscMoldDefinition>& OutMolds,
        TArray<FDiscPlasticDefinition>& OutPlastics);
    static bool ValidateDefinitions(
        const TArray<FDiscMoldDefinition>& Molds,
        const TArray<FDiscPlasticDefinition>& Plastics,
        FString& OutError);
    static bool ResolveFromDefinitions(
        const TArray<FDiscMoldDefinition>& Molds,
        const TArray<FDiscPlasticDefinition>& Plastics,
        FName MoldId,
        EDiscPlastic Plastic,
        FResolvedDiscDefinition& OutDisc);
    static bool LoadPrimaryAssetDefinitions(
        TArray<FDiscMoldDefinition>& OutMolds,
        TArray<FDiscPlasticDefinition>& OutPlastics,
        FString& OutError);
    static bool SelectCatalogDefinitions(
        const TArray<FDiscMoldDefinition>& CandidateMolds,
        const TArray<FDiscPlasticDefinition>& CandidatePlastics,
        const TArray<FDiscMoldDefinition>& FallbackMolds,
        const TArray<FDiscPlasticDefinition>& FallbackPlastics,
        TArray<FDiscMoldDefinition>& OutMolds,
        TArray<FDiscPlasticDefinition>& OutPlastics,
        bool& bOutUsedFallback,
        FString& OutReason);

private:
    UPROPERTY() TArray<FDiscMoldDefinition> FallbackCatalog;
    UPROPERTY() TArray<FDiscPlasticDefinition> FallbackPlasticCatalog;
    UPROPERTY() TArray<FDiscMoldDefinition> ActiveCatalog;
    UPROPERTY() TArray<FDiscPlasticDefinition> ActivePlasticCatalog;
    bool bUsingPrimaryAssets = false;
    FString CatalogSourceText = TEXT("UNINITIALIZED");
};
