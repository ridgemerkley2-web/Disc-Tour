#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfOutfitItem.generated.h"

class USkeletalMesh;
class UStaticMesh;

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfOutfitItem : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGOutfitSlot Slot = EDGOutfitSlot::Top;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGOutfitVariant> Variants;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EDGOutfitSlot> ConflictingSlots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EDGBodyRegion> CoveredBodyRegions;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinHeightCm = 150.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHeightCm = 210.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> StaticMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AttachSocket = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform RelativeAttachmentTransform = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseLeaderPose = true;

    UFUNCTION(BlueprintPure) bool SupportsHeight(float HeightCm) const;
    UFUNCTION(BlueprintPure) bool FindVariant(FName VariantId, FDGOutfitVariant& OutVariant) const;
};
