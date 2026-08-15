#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAvatarBackendTypes.generated.h"

UENUM(BlueprintType)
enum class EDGAvatarBackend : uint8
{
    DGMaster UMETA(DisplayName="DG Master Custom Character"),
    MetaHumanPreset UMETA(DisplayName="MetaHuman Preset Backend")
};

UENUM(BlueprintType)
enum class EDGMetaHumanRuntimeMode : uint8
{
    ShippingSafeAssembled UMETA(DisplayName="Shipping-Safe Assembled Character"),
    ExperimentalCollectionInstance UMETA(DisplayName="Experimental Collection/Instance")
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGAvatarBackendState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Avatar")
    EDGAvatarBackend Backend = EDGAvatarBackend::DGMaster;

    UPROPERTY(BlueprintReadOnly, Category="Avatar")
    FName BackendId = TEXT("dg_master");

    UPROPERTY(BlueprintReadOnly, Category="Avatar")
    bool bVisualReady = false;
};
