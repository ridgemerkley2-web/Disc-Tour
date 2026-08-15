#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfCharacterProfile.generated.h"

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCharacterProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UDiscGolfCharacterProfile();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
    FDGBodyProfile Body;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character")
    FDGThrowStyle ThrowStyle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Throw")
    TMap<EDGThrowType, FDGThrowCapability> Capabilities;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rig")
    FName RightDiscGripBone = TEXT("disc_grip_r");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rig")
    FName LeftDiscGripBone = TEXT("disc_grip_l");

    UFUNCTION(BlueprintPure, Category="Disc Golf|Character")
    FDGThrowCapability GetCapability(EDGThrowType ThrowType) const;
};
