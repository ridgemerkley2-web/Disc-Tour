#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "../DiscGolfPresentationAudioRouterComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12PresentationContractFailClosedTest,
    "DiscGolfTour.Session12.Contract.FailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12PresentationContractFailClosedTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Json;
    const FString Path = FPaths::Combine(
        FPaths::ProjectDir(), TEXT("Config/DG_Session12PresentationContract.json"));
    if (!TestTrue(TEXT("Session 12 contract is present"), FFileHelper::LoadFileToString(Json, *Path)))
    {
        return false;
    }

    TSharedPtr<FJsonObject> Contract;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!TestTrue(TEXT("Session 12 contract parses"),
        FJsonSerializer::Deserialize(Reader, Contract) && Contract.IsValid()))
    {
        return false;
    }

    TestEqual(TEXT("Contract schema is frozen at v1"),
        Contract->GetIntegerField(TEXT("schema_version")), 1);
    bool bAllCapabilitiesPass = true;
    const TArray<TSharedPtr<FJsonValue>>& Capabilities =
        Contract->GetArrayField(TEXT("implementation_capabilities"));
    TestEqual(TEXT("Session 12 contract has exactly seven bounded capabilities"),
        Capabilities.Num(), 7);
    for (const TSharedPtr<FJsonValue>& Value : Capabilities)
    {
        const TSharedPtr<FJsonObject> Capability = Value.IsValid() ? Value->AsObject() : nullptr;
        bAllCapabilitiesPass = bAllCapabilitiesPass && Capability.IsValid()
            && Capability->GetStringField(TEXT("status")) == TEXT("TECHNICAL_PASS");
    }
    TestEqual(TEXT("Feature completeness exactly follows all-capability acceptance"),
        Contract->GetBoolField(TEXT("feature_complete")), bAllCapabilitiesPass);
    TestTrue(TEXT("Public release remains blocked"),
        Contract->GetBoolField(TEXT("releaseBlocked")));
    TestFalse(TEXT("Public release cannot be ready"),
        Contract->GetBoolField(TEXT("release_ready")));

    const TArray<TSharedPtr<FJsonValue>>& Blockers =
        Contract->GetArrayField(TEXT("release_blockers"));
    TestEqual(TEXT("Session 12 retains ten inherited blockers and adds exactly one"),
        Blockers.Num(), 11);
    if (Blockers.Num() == 11)
    {
        TestEqual(TEXT("The Session 12 blocker is appended last"),
            Blockers.Last()->AsString(),
            FString(TEXT("SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING")));
    }

    const TSharedPtr<FJsonObject> Audio =
        Contract->GetObjectField(TEXT("audio_router_contract"));
    TestEqual(TEXT("Project-owned router remains audio owner"),
        Audio->GetStringField(TEXT("owner")),
        FString(TEXT("UDiscGolfPresentationAudioRouterComponent")));
    TestFalse(TEXT("Plugin audio component is not activated"),
        Audio->GetBoolField(TEXT("plugin_audio_component_activated")));
    TestFalse(TEXT("Router cannot tick"),
        Audio->GetBoolField(TEXT("component_tick")));
    TestFalse(TEXT("Synchronous routing loads remain forbidden"),
        Audio->GetBoolField(TEXT("synchronous_asset_load_during_routing")));
    TestFalse(TEXT("No production audio asset claim is made"),
        Audio->GetBoolField(TEXT("production_audio_assets_present")));

    const TSharedPtr<FJsonObject> Optional =
        Contract->GetObjectField(TEXT("optional_engine_policy"));
    TestFalse(TEXT("Gameplay Cameras is not a hard dependency"),
        Optional->GetBoolField(TEXT("hard_gameplay_cameras_dependency")));
    TestFalse(TEXT("CommonUI is not a hard dependency"),
        Optional->GetBoolField(TEXT("hard_common_ui_dependency")));
    TestFalse(TEXT("Engine replay is not authoritative"),
        Optional->GetBoolField(TEXT("engine_replay_is_authoritative")));
    TestFalse(TEXT("MetaSound Builder is not a runtime dependency"),
        Optional->GetBoolField(TEXT("metasound_builder_runtime_dependency")));

    const UDiscGolfPresentationAudioRouterComponent* DefaultRouter =
        GetDefault<UDiscGolfPresentationAudioRouterComponent>();
    TestFalse(TEXT("Router CDO confirms no Tick"),
        DefaultRouter->PrimaryComponentTick.bCanEverTick);
    TestEqual(TEXT("Router CDO has a truthful empty fallback table"),
        DefaultRouter->FallbackSounds.Num(), 0);
    return !HasAnyErrors();
}

#endif
