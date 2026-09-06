#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAvatarBackendTypes.generated.h"

UENUM(BlueprintType)
enum class EDGAvatarBackend : uint8
{
    DGMaster,
    MetaHumanPreset
};

UENUM(BlueprintType)
enum class EDGMetaHumanRuntimeMode : uint8
{
    // Values 0 and 1 are a frozen serialized asset contract inherited from
    // the accepted framework packages. Never reorder or renumber them.
    ShippingSafeAssembled = 0,
    ExperimentalCollectionInstance = 1,
    Disabled = 2,
    Experimental = 3
};

static_assert(
    static_cast<uint8>(EDGMetaHumanRuntimeMode::ShippingSafeAssembled) == 0,
    "ShippingSafeAssembled must retain its frozen serialized value.");
static_assert(
    static_cast<uint8>(EDGMetaHumanRuntimeMode::ExperimentalCollectionInstance) == 1,
    "ExperimentalCollectionInstance must retain its frozen serialized value.");

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGAvatarBackendState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BackendId = TEXT("dg_master");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGAvatarBackend Backend = EDGAvatarBackend::DGMaster;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bVisualReady = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Status;
};
