#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfOutfitComponent.generated.h"

class UDiscGolfOutfitCatalog;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGBodyCoverageChanged,
    const TArray<EDGBodyRegion>&,
    CoveredRegions);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfOutfitComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDiscGolfOutfitCatalog> Catalog;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGOutfitLoadout CurrentLoadout;
    UPROPERTY(BlueprintAssignable) FDGBodyCoverageChanged OnBodyCoverageChanged;

    UFUNCTION(BlueprintCallable)
    bool ApplyLoadout(const FDGOutfitLoadout& Loadout, USkeletalMeshComponent* BodyMesh, const FDGBodyProfile& Body);

    UFUNCTION(BlueprintCallable) void ClearOutfit();
    UFUNCTION(BlueprintPure) TArray<EDGBodyRegion> GetCoveredBodyRegions() const;
    UFUNCTION(BlueprintCallable) void ReapplyBodyMorphs(const FDGBodyProfile& Body);
};
