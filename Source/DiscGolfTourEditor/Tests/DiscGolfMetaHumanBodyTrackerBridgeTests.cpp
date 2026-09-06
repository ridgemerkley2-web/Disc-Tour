#if WITH_DEV_AUTOMATION_TESTS

#include "DiscGolfMetaHumanBodyTrackerBridgeUtility.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfMetaHumanBodyTrackerCapabilityDecisionTest,
    "DiscGolfTour.Character.Mocap.MetaHumanBodyTracker.CapabilityDecision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfMetaHumanBodyTrackerCapabilityDecisionTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    using namespace DiscGolfMetaHumanBodyTrackerBridge;

    FCapabilitySnapshot Snapshot;
    Snapshot.bQueriedOnGameThread = false;
    Snapshot.bInterfaceModuleExists = true;
    Snapshot.ProviderCount = 1;
    FCapabilityDecision Decision = EvaluateCapability(Snapshot);
    TestFalse(TEXT("An off-thread query fails before registry access"),
        Decision.bReady);
    TestEqual(TEXT("Off-thread query code is explicit"), Decision.Code,
        FString(TEXT("BLOCKED_GAME_THREAD_REQUIRED")));

    Snapshot.bQueriedOnGameThread = true;
    Snapshot.bInterfaceModuleExists = false;
    Snapshot.ProviderCount = 1;
    Decision = EvaluateCapability(Snapshot);
    TestFalse(TEXT("A missing interface fails despite a fabricated provider"),
        Decision.bReady);
    TestEqual(TEXT("Missing-interface code is explicit"), Decision.Code,
        FString(TEXT("BLOCKED_INTERFACE_MODULE_MISSING")));

    Snapshot.bInterfaceModuleExists = true;
    Snapshot.ProviderCount = 0;
    Decision = EvaluateCapability(Snapshot);
    TestFalse(TEXT("Zero providers fails closed"), Decision.bReady);
    TestEqual(TEXT("Missing-provider code is explicit"), Decision.Code,
        FString(TEXT("BLOCKED_IMPLEMENTATION_NOT_REGISTERED")));

    Snapshot.ProviderCount = 2;
    Decision = EvaluateCapability(Snapshot);
    TestFalse(TEXT("Multiple providers fail closed"), Decision.bReady);
    TestEqual(TEXT("Ambiguous-provider code is explicit"), Decision.Code,
        FString(TEXT("BLOCKED_AMBIGUOUS_IMPLEMENTATIONS")));

    Snapshot.ProviderCount = 1;
    Decision = EvaluateCapability(Snapshot);
    TestTrue(TEXT("Exactly one provider passes capability"), Decision.bReady);
    TestEqual(TEXT("Registered-provider code is explicit"), Decision.Code,
        FString(TEXT("PASS_PROVIDER_REGISTERED")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfMetaHumanBodyTrackerPayloadContractTest,
    "DiscGolfTour.Character.Mocap.MetaHumanBodyTracker.PayloadContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfMetaHumanBodyTrackerPayloadContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    using namespace DiscGolfMetaHumanBodyTrackerBridge;

    struct FPayloadCase
    {
        bool bQueriedOnGameThread;
        bool bInterfaceModuleExists;
        int32 ProviderCount;
    };

    const FPayloadCase Cases[] = {
        {false, true, 1},
        {true, false, 1},
        {true, true, 0},
        {true, true, 1},
        {true, true, 2},
    };

    for (const FPayloadCase& Case : Cases)
    {
        FCapabilitySnapshot Snapshot;
        Snapshot.bQueriedOnGameThread = Case.bQueriedOnGameThread;
        Snapshot.bInterfaceModuleExists = Case.bInterfaceModuleExists;
        Snapshot.ProviderCount = Case.ProviderCount;
        const FCapabilityDecision Decision = EvaluateCapability(Snapshot);

        TSharedPtr<FJsonObject> Root;
        const FString Payload = BuildCapabilityPayload(Snapshot);
        const TSharedRef<TJsonReader<>> Reader =
            TJsonReaderFactory<>::Create(Payload);
        const FString CaseLabel = FString::Printf(
            TEXT("game_thread=%s,module=%s,providers=%d"),
            Case.bQueriedOnGameThread ? TEXT("true") : TEXT("false"),
            Case.bInterfaceModuleExists ? TEXT("true") : TEXT("false"),
            Case.ProviderCount);
        TestTrue(*FString::Printf(TEXT("%s returns valid JSON"), *CaseLabel),
            FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
        if (!Root.IsValid())
        {
            continue;
        }

        FString Schema;
        FString Status;
        FString Code;
        bool bQueryExecutedOnGameThread = !Case.bQueriedOnGameThread;
        bool bInterfaceModuleExists = !Case.bInterfaceModuleExists;
        bool bProviderRegistered = Case.ProviderCount <= 0;
        bool bUniqueProviderReady = !Decision.bReady;
        double ProviderCount = -1.0;
        TestTrue(*FString::Printf(TEXT("%s has schema"), *CaseLabel),
            Root->TryGetStringField(TEXT("schema"), Schema));
        TestTrue(*FString::Printf(TEXT("%s has status"), *CaseLabel),
            Root->TryGetStringField(TEXT("status"), Status));
        TestTrue(*FString::Printf(TEXT("%s has code"), *CaseLabel),
            Root->TryGetStringField(TEXT("code"), Code));
        TestTrue(*FString::Printf(TEXT("%s has query thread evidence"),
            *CaseLabel), Root->TryGetBoolField(
                TEXT("query_executed_on_game_thread"),
                bQueryExecutedOnGameThread));
        TestTrue(*FString::Printf(TEXT("%s has module evidence"), *CaseLabel),
            Root->TryGetBoolField(
                TEXT("interface_module_exists"), bInterfaceModuleExists));
        TestTrue(*FString::Printf(TEXT("%s has provider count"), *CaseLabel),
            Root->TryGetNumberField(TEXT("provider_count"), ProviderCount));
        TestTrue(*FString::Printf(TEXT("%s has registration evidence"),
            *CaseLabel), Root->TryGetBoolField(
                TEXT("provider_registered"), bProviderRegistered));
        TestTrue(*FString::Printf(TEXT("%s has unique-provider evidence"),
            *CaseLabel), Root->TryGetBoolField(
                TEXT("unique_provider_ready"), bUniqueProviderReady));

        TestEqual(*FString::Printf(TEXT("%s schema is v1"), *CaseLabel),
            Schema,
            FString(TEXT("DiscGolfTour.MetaHumanBodyTrackerCapability.v1")));
        TestEqual(*FString::Printf(TEXT("%s status matches decision"),
            *CaseLabel), Status,
            Decision.bReady ? FString(TEXT("PASS_CAPABILITY"))
                            : FString(TEXT("BLOCKED")));
        TestEqual(*FString::Printf(TEXT("%s code matches decision"),
            *CaseLabel), Code, Decision.Code);
        TestEqual(*FString::Printf(TEXT("%s query evidence matches"),
            *CaseLabel), bQueryExecutedOnGameThread,
            Case.bQueriedOnGameThread);
        TestEqual(*FString::Printf(TEXT("%s module evidence matches"),
            *CaseLabel), bInterfaceModuleExists,
            Case.bInterfaceModuleExists);
        TestEqual(*FString::Printf(TEXT("%s provider count matches"),
            *CaseLabel), ProviderCount,
            static_cast<double>(Case.ProviderCount));
        TestEqual(*FString::Printf(TEXT("%s provider registration matches"),
            *CaseLabel), bProviderRegistered, Case.ProviderCount > 0);
        TestEqual(*FString::Printf(TEXT("%s unique-provider gate matches"),
            *CaseLabel), bUniqueProviderReady, Decision.bReady);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfMetaHumanBodyTrackerLiveProbeTest,
    "DiscGolfTour.Character.Mocap.MetaHumanBodyTracker.LiveProbeContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfMetaHumanBodyTrackerLiveProbeTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const FString Payload =
        UDiscGolfMetaHumanBodyTrackerBridgeUtility::
            ProbeMetaHumanBodyTrackerCapability();
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Payload);
    TestTrue(TEXT("The live probe returns valid JSON"),
        FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid());
    if (!Root.IsValid())
    {
        return false;
    }

    FString DiskMutation;
    FString Schema;
    FString Status;
    FString Code;
    double ProviderCount = -1.0;
    bool bQueryExecutedOnGameThread = false;
    bool bInterfaceModuleExists = false;
    bool bProviderRegistered = false;
    bool bUniqueProviderReady = false;
    bool bStagingReady = true;
    bool bRuntimeAuthorityChanged = true;
    bool bProductionPromotionAllowed = true;
    TestTrue(TEXT("Schema is present"),
        Root->TryGetStringField(TEXT("schema"), Schema));
    TestEqual(TEXT("Schema is the stable v1 contract"), Schema,
        FString(TEXT("DiscGolfTour.MetaHumanBodyTrackerCapability.v1")));
    TestTrue(TEXT("Status is present"),
        Root->TryGetStringField(TEXT("status"), Status));
    TestTrue(TEXT("Code is present"),
        Root->TryGetStringField(TEXT("code"), Code));
    TestTrue(TEXT("Query thread evidence is present"),
        Root->TryGetBoolField(
            TEXT("query_executed_on_game_thread"),
            bQueryExecutedOnGameThread));
    TestTrue(TEXT("The Blueprint probe executes on the game thread"),
        bQueryExecutedOnGameThread);
    TestTrue(TEXT("Interface-module evidence is present"),
        Root->TryGetBoolField(
            TEXT("interface_module_exists"), bInterfaceModuleExists));
    TestTrue(TEXT("The installed interface module exists"),
        bInterfaceModuleExists);
    TestTrue(TEXT("Provider count is present"),
        Root->TryGetNumberField(TEXT("provider_count"), ProviderCount));
    TestEqual(TEXT("Exactly one provider is registered"), ProviderCount, 1.0);
    TestTrue(TEXT("Provider-registration evidence is present"),
        Root->TryGetBoolField(
            TEXT("provider_registered"), bProviderRegistered));
    TestTrue(TEXT("A provider is registered"), bProviderRegistered);
    TestTrue(TEXT("Unique-provider evidence is present"),
        Root->TryGetBoolField(
            TEXT("unique_provider_ready"), bUniqueProviderReady));
    TestTrue(TEXT("The provider selection is unambiguous"),
        bUniqueProviderReady);
    TestEqual(TEXT("Capability status is an exact pass"), Status,
        FString(TEXT("PASS_CAPABILITY")));
    TestEqual(TEXT("Capability code is an exact pass"), Code,
        FString(TEXT("PASS_PROVIDER_REGISTERED")));
    TestTrue(TEXT("Disk mutation is present"),
        Root->TryGetStringField(TEXT("disk_mutation"), DiskMutation));
    TestEqual(TEXT("The probe performs no disk mutation"), DiskMutation,
        FString(TEXT("NONE")));
    TestTrue(TEXT("Staging readiness is present"),
        Root->TryGetBoolField(TEXT("staging_ready"), bStagingReady));
    TestFalse(TEXT("Capability alone never authorizes staging"), bStagingReady);
    TestTrue(TEXT("Runtime authority evidence is present"),
        Root->TryGetBoolField(
            TEXT("runtime_authority_changed"), bRuntimeAuthorityChanged));
    TestFalse(TEXT("The probe never changes runtime authority"),
        bRuntimeAuthorityChanged);
    TestTrue(TEXT("Production-promotion evidence is present"),
        Root->TryGetBoolField(
            TEXT("production_promotion_allowed"),
            bProductionPromotionAllowed));
    TestFalse(TEXT("Capability alone never authorizes production promotion"),
        bProductionPromotionAllowed);
    return true;
}

#endif
