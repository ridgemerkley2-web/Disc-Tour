#if WITH_DEV_AUTOMATION_TESTS

#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#include "DiscGolfAvatarBackendProfile.h"

#include "../DiscGolfAvatarBackendRuntime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfAvatarBackendFallbackTest,
    "DiscGolfTour.Character.Session8.AvatarBackend.FailClosedResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfAvatarBackendFallbackTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FDiscGolfAvatarBackendResolution DefaultResult =
        DiscGolfAvatarBackendRuntime::ResolveBackend(NAME_None, nullptr);
    TestEqual(TEXT("Empty requests resolve to DG master"),
        DefaultResult.ResolvedBackendId,
        FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId));
    TestFalse(TEXT("DG master never claims a MetaHuman attempt"),
        DefaultResult.bMetaHumanAttemptAllowed);

    const FDiscGolfAvatarBackendResolution UnknownResult =
        DiscGolfAvatarBackendRuntime::ResolveBackend(TEXT("vendor_backend"), nullptr);
    TestEqual(TEXT("Unknown IDs fail closed to DG master"),
        UnknownResult.ResolvedBackend, EDGAvatarBackend::DGMaster);

    const FDiscGolfAvatarBackendResolution MissingResult =
        DiscGolfAvatarBackendRuntime::ResolveBackend(
            DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId, nullptr);
    TestEqual(TEXT("Missing MetaHuman content preserves the proxy fallback"),
        MissingResult.ResolvedBackendId,
        FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId));
    TestFalse(TEXT("Missing content cannot be reported ready"),
        MissingResult.bMetaHumanAttemptAllowed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfAvatarBackendProfileContractTest,
    "DiscGolfTour.Character.Session8.AvatarBackend.ProfileContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfAvatarBackendProfileContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    UDiscGolfAvatarBackendProfile* Profile =
        NewObject<UDiscGolfAvatarBackendProfile>(GetTransientPackage());
    FString Reason;
    TestFalse(TEXT("The default/incomplete profile is rejected"),
        DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(Profile, Reason));

    Profile->BackendId = DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId;
    Profile->Backend = EDGAvatarBackend::MetaHumanPreset;
    Profile->MetaHumanRuntimeMode = EDGMetaHumanRuntimeMode::ShippingSafeAssembled;
    Profile->VisualActorClass = TSoftClassPtr<AActor>(
        FSoftObjectPath(TEXT("/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.BP_DG_MetaHuman_Default_C")));
    Profile->RetargetAsset = TSoftObjectPtr<UObject>(
        FSoftObjectPath(TEXT("/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.RTG_DGMaster_To_MetaHuman")));
    Profile->bUseRuntimeRetargeting = true;
    Profile->VisualBodyComponentTag = TEXT("DGVisualBody");
    Profile->VisualHeadComponentTag = TEXT("DGVisualHead");
    Profile->PreferredQualityProfileId = TEXT("GameplayHigh");
    Profile->bAllowRuntimeFaceSculpting = false;

    TestTrue(TEXT("A complete shipping-safe metadata contract may be attempted"),
        DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(Profile, Reason));
    const FDiscGolfAvatarBackendResolution ValidResult =
        DiscGolfAvatarBackendRuntime::ResolveBackend(
            DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId, Profile);
    TestEqual(TEXT("The stable MetaHuman ID resolves only after contract validation"),
        ValidResult.ResolvedBackend, EDGAvatarBackend::MetaHumanPreset);
    TestTrue(TEXT("Metadata success permits only a later verified build attempt"),
        ValidResult.bMetaHumanAttemptAllowed);

    Profile->bAllowRuntimeFaceSculpting = true;
    TestFalse(TEXT("Unsupported runtime sculpting fails closed"),
        DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(Profile, Reason));
    Profile->bAllowRuntimeFaceSculpting = false;
    Profile->PreferredQualityProfileId = TEXT("CinematicUnmeasured");
    TestFalse(TEXT("Unmeasured quality IDs fail closed"),
        DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(Profile, Reason));
    return true;
}

#endif
