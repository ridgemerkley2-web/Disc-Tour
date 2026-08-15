#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfBrandProfile.generated.h"

class UTexture2D;

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfBrandProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // Stable internal identifier, e.g. premium_disc_golf.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    FName BrandId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    FText DisplayName;

    // This is an internal project approval flag, not a legal conclusion.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    bool bApprovedForInGameUse = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    FString ApprovalBasis;

    // Intentionally empty until an authorized source logo is supplied.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    TSoftObjectPtr<UTexture2D> PrimaryLogo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    FLinearColor PrimaryColor = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    FLinearColor SecondaryColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Brand")
    FLinearColor AccentColor = FLinearColor(0.65f, 0.82f, 0.20f, 1.0f);
};
