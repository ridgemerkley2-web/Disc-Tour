#pragma once

#include "CoreMinimal.h"
#include "DiscGolfDiscTypes.generated.h"

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGDiscInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid InstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName DiscDefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName PlasticId = TEXT("Tour");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float MassGrams = 175.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Wear01 = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FLinearColor Color = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName StampId = TEXT("dg_generic_default");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString Nickname;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bFavorite = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGDiscBagLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName BagEquipmentId = TEXT("dg_generic_bag_default");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Capacity = 24;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FDGDiscInstance> Discs;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid SelectedDiscInstanceId;
};
