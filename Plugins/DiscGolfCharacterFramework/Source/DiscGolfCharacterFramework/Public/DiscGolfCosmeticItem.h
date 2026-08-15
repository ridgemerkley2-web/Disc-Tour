#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCosmeticItem.generated.h"

class USkeletalMesh;
class UStaticMesh;

UENUM(BlueprintType)
enum class EDGCosmeticKind : uint8
{
    Hair,
    FacialHair,
    Eyebrow,
    Scar,
    Tattoo,
    Voice,
    PronounSet
};

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCosmeticItem : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FName ItemId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    EDGCosmeticKind Kind = EDGCosmeticKind::Hair;

    // Mesh-backed hair/brows/facial hair can use either skeletal or static mesh.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    TSoftObjectPtr<UStaticMesh> StaticMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FName AttachSocket = TEXT("head");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FTransform RelativeAttachmentTransform = FTransform::Identity;

    // Optional material/decal identifier used by project material logic.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Visual")
    FName MaterialVariantId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compatibility")
    float MinHeightCm = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Compatibility")
    float MaxHeightCm = 210.0f;
};
