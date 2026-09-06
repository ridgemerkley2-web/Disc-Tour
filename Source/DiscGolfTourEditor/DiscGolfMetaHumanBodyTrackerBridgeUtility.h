#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfMetaHumanBodyTrackerBridgeUtility.generated.h"

namespace DiscGolfMetaHumanBodyTrackerBridge
{
struct FCapabilitySnapshot
{
    bool bQueriedOnGameThread = false;
    bool bInterfaceModuleExists = false;
    int32 ProviderCount = 0;
};

struct FCapabilityDecision
{
    bool bReady = false;
    FString Code;
};

/** Pure decision seam used by automation to prove fail-closed behavior. */
DISCGOLFTOUREDITOR_API FCapabilityDecision EvaluateCapability(
    const FCapabilitySnapshot& Snapshot);

/**
 * Builds the stable v1 JSON envelope for a supplied capability snapshot.
 * This is a pure serialization seam and never queries or starts a provider.
 */
DISCGOLFTOUREDITOR_API FString BuildCapabilityPayload(
    const FCapabilitySnapshot& Snapshot);

/**
 * Read-only live query. Off-game-thread calls return a blocked snapshot without
 * touching the module or modular-feature registries.
 */
DISCGOLFTOUREDITOR_API FCapabilitySnapshot QueryLiveCapability();
}

/**
 * Read-only capability gate for the MetaHuman body-tracker staging bridge.
 * A passing capability probe never authorizes staging or production promotion.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfMetaHumanBodyTrackerBridgeUtility
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Disc Golf|Character|Mocap|MetaHuman")
    static FString ProbeMetaHumanBodyTrackerCapability();
};
