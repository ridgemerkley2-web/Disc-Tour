#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfOutfitComponent.generated.h"

class UDiscGolfOutfitCatalog;
class UDiscGolfOutfitItem;
class USkeletalMeshComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FDGOnOutfitSlotChanged,
    EDGOutfitSlot, Slot,
    FName, ItemId
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnBodyCoverageChanged,
    const TArray<EDGBodyRegion>&, CoveredRegions
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfOutfitComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfOutfitComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Outfit")
    TObjectPtr<UDiscGolfOutfitCatalog> Catalog;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Outfit")
    FDGOutfitLoadout CurrentLoadout;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Outfit")
    FDGOnOutfitSlotChanged OnOutfitSlotChanged;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Outfit")
    FDGOnBodyCoverageChanged OnBodyCoverageChanged;

    // Equip by stable catalog ID.
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    bool EquipById(
        FName ItemId,
        FName VariantId,
        USkeletalMeshComponent* LeaderBodyMesh,
        const FDGBodyProfile& BodyProfile
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    void UnequipSlot(EDGOutfitSlot Slot);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    void ClearOutfit();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    bool ApplyLoadout(
        const FDGOutfitLoadout& Loadout,
        USkeletalMeshComponent* LeaderBodyMesh,
        const FDGBodyProfile& BodyProfile
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Outfit")
    void ReapplyBodyMorphs(const FDGBodyProfile& BodyProfile);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Outfit")
    TArray<EDGBodyRegion> GetCoveredBodyRegions() const;

private:
    UPROPERTY(Transient)
    TMap<EDGOutfitSlot, TObjectPtr<USkeletalMeshComponent>> SkeletalSlotComponents;

    UPROPERTY(Transient)
    TMap<EDGOutfitSlot, TObjectPtr<UStaticMeshComponent>> StaticSlotComponents;

    UPROPERTY(Transient)
    TMap<EDGOutfitSlot, TObjectPtr<UDiscGolfOutfitItem>> EquippedItems;

    bool EquipResolvedItem(
        UDiscGolfOutfitItem* Item,
        FName VariantId,
        USkeletalMeshComponent* LeaderBodyMesh,
        const FDGBodyProfile& BodyProfile
    );

    void ApplyVariantToMesh(USkeletalMeshComponent* MeshComp, const FDGOutfitVariant& Variant);
    void ApplyVariantToMesh(UStaticMeshComponent* MeshComp, const FDGOutfitVariant& Variant);
    void ApplyStandardMorphs(USkeletalMeshComponent* MeshComp, const FDGBodyProfile& BodyProfile);
    void UpdateLoadoutEntry(EDGOutfitSlot Slot, FName ItemId, FName VariantId);
    void BroadcastCoverage();
};
