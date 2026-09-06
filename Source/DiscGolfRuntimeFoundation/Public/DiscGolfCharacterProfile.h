#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfCharacterProfile.generated.h"

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfCharacterProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGBodyProfile Body;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGThrowStyle ThrowStyle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGHandedness Handedness = EDGHandedness::Right;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RightDiscGripBone = TEXT("disc_grip_r");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LeftDiscGripBone = TEXT("disc_grip_l");
};
