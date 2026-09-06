#include "DiscGolfMetaHumanBodyTrackerBridgeUtility.h"

#include "Dom/JsonObject.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace DiscGolfMetaHumanBodyTrackerBridge
{
namespace
{
const TCHAR* InterfaceModuleName = TEXT("MetaHumanBodyTrackerInterface");
const FName ModularFeatureName(TEXT("MetaHumanBodyTrackerInterface"));
}

FCapabilityDecision EvaluateCapability(const FCapabilitySnapshot& Snapshot)
{
    FCapabilityDecision Decision;

    if (!Snapshot.bQueriedOnGameThread)
    {
        Decision.Code = TEXT("BLOCKED_GAME_THREAD_REQUIRED");
        return Decision;
    }

    if (!Snapshot.bInterfaceModuleExists)
    {
        Decision.Code = TEXT("BLOCKED_INTERFACE_MODULE_MISSING");
        return Decision;
    }

    if (Snapshot.ProviderCount == 0)
    {
        Decision.Code = TEXT("BLOCKED_IMPLEMENTATION_NOT_REGISTERED");
        return Decision;
    }

    if (Snapshot.ProviderCount != 1)
    {
        Decision.Code = TEXT("BLOCKED_AMBIGUOUS_IMPLEMENTATIONS");
        return Decision;
    }

    Decision.bReady = true;
    Decision.Code = TEXT("PASS_PROVIDER_REGISTERED");
    return Decision;
}

FString BuildCapabilityPayload(const FCapabilitySnapshot& Snapshot)
{
    const FCapabilityDecision Decision = EvaluateCapability(Snapshot);
    const bool bProviderRegistered = Snapshot.ProviderCount > 0;

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(
        TEXT("schema"),
        TEXT("DiscGolfTour.MetaHumanBodyTrackerCapability.v1"));
    Root->SetStringField(
        TEXT("status"),
        Decision.bReady ? TEXT("PASS_CAPABILITY") : TEXT("BLOCKED"));
    Root->SetStringField(TEXT("code"), Decision.Code);
    Root->SetStringField(TEXT("disk_mutation"), TEXT("NONE"));
    Root->SetStringField(TEXT("interface_module"),
        TEXT("MetaHumanBodyTrackerInterface"));
    Root->SetStringField(TEXT("modular_feature"),
        TEXT("MetaHumanBodyTrackerInterface"));
    Root->SetBoolField(
        TEXT("query_executed_on_game_thread"),
        Snapshot.bQueriedOnGameThread);
    Root->SetBoolField(
        TEXT("interface_module_exists"), Snapshot.bInterfaceModuleExists);
    Root->SetNumberField(TEXT("provider_count"), Snapshot.ProviderCount);
    Root->SetBoolField(TEXT("provider_registered"), bProviderRegistered);
    Root->SetBoolField(TEXT("unique_provider_ready"), Decision.bReady);

    // Capability is intentionally weaker than source/legal/asset preflight.
    Root->SetBoolField(TEXT("staging_ready"), false);
    Root->SetBoolField(TEXT("runtime_authority_changed"), false);
    Root->SetBoolField(TEXT("production_promotion_allowed"), false);

    FString Output;
    const TSharedRef<TJsonWriter<
        TCHAR,
        TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<
            TCHAR,
            TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        return FString::Printf(
            TEXT("{\"schema\":\"DiscGolfTour.MetaHumanBodyTrackerCapability.v1\","
                 "\"status\":\"BLOCKED\","
                 "\"code\":\"BLOCKED_JSON_SERIALIZATION\","
                 "\"disk_mutation\":\"NONE\","
                 "\"interface_module\":\"MetaHumanBodyTrackerInterface\","
                 "\"modular_feature\":\"MetaHumanBodyTrackerInterface\","
                 "\"query_executed_on_game_thread\":%s,"
                 "\"interface_module_exists\":%s,"
                 "\"provider_count\":%d,"
                 "\"provider_registered\":%s,"
                 "\"unique_provider_ready\":%s,"
                 "\"staging_ready\":false,"
                 "\"runtime_authority_changed\":false,"
                 "\"production_promotion_allowed\":false}"),
            Snapshot.bQueriedOnGameThread ? TEXT("true") : TEXT("false"),
            Snapshot.bInterfaceModuleExists ? TEXT("true") : TEXT("false"),
            Snapshot.ProviderCount,
            bProviderRegistered ? TEXT("true") : TEXT("false"),
            Decision.bReady ? TEXT("true") : TEXT("false"));
    }

    return Output;
}

FCapabilitySnapshot QueryLiveCapability()
{
    FCapabilitySnapshot Snapshot;
    Snapshot.bQueriedOnGameThread = IsInGameThread();
    if (!Snapshot.bQueriedOnGameThread)
    {
        return Snapshot;
    }

    Snapshot.bInterfaceModuleExists =
        FModuleManager::Get().ModuleExists(InterfaceModuleName);

    IModularFeatures::FScopedLockModularFeatureList ScopedFeatureLock;
    Snapshot.ProviderCount =
        IModularFeatures::Get().GetModularFeatureImplementationCount(
            ModularFeatureName);
    return Snapshot;
}
}

FString UDiscGolfMetaHumanBodyTrackerBridgeUtility::
    ProbeMetaHumanBodyTrackerCapability()
{
    using namespace DiscGolfMetaHumanBodyTrackerBridge;

    return BuildCapabilityPayload(QueryLiveCapability());
}
