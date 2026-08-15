#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfOutfitItem.generated.h"

class USkeletalMesh;
class UStaticMesh;

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfOutfitItem : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Must remain stable after shipping/saving characters.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    EDGOutfitSlot Slot = EDGOutfitSlot::Top;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    // Static accessories use this socket. Skeletal clothing usually leaves it empty.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FName AttachSocket = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FTransform RelativeAttachmentTransform = FTransform::Identity;

    // Default for normal modular clothing.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    bool bUseLeaderPose = true;

    // Enable only for clothing configured to use compatible cloth binding.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    bool bBindClothToLeaderPose = false;

    // Allows the item to request hidden body regions/hair to reduce clipping.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coverage")
    TArray<EDGBodyRegion> CoveredBodyRegions;

    // Optional slot incompatibilities. Example: a full-body rain suit could conflict with Top + Bottom.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compatibility")
    TArray<EDGOutfitSlot> ConflictingSlots;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compatibility")
    float MinHeightCm = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compatibility")
    float MaxHeightCm = 210.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Variants")
    TArray<FDGOutfitVariant> Variants;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Outfit")
    bool SupportsHeight(float HeightCm) const
    {
        return HeightCm >= MinHeightCm && HeightCm <= MaxHeightCm;
    }

    UFUNCTION(BlueprintPure, Category="Disc Golf|Outfit")
    bool FindVariant(FName VariantId, FDGOutfitVariant& OutVariant) const;
};
