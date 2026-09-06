#if WITH_DEV_AUTOMATION_TESTS

#include "Components/LODSyncComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfAvatarBackendProfile.h"

#include "../DiscGolfAvatarBackendRuntime.h"
#include "../DiscGolfMetaHumanAvatarBackendComponent.h"

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BPresentationPolicyContractTest,
    "DiscGolfTour.Character.Session8B.AvatarBackend.PresentationPolicyContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BPresentationPolicyContractTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    ULODSyncComponent* LODSync =
        NewObject<ULODSyncComponent>(GetTransientPackage());
    LODSync->NumLODs = 3;
    LODSync->MinLOD = 0;
    LODSync->ComponentsToSync = {
        FComponentSync(FName(TEXT("Body")), ESyncOption::Drive),
        FComponentSync(FName(TEXT("Face")), ESyncOption::Drive),
        FComponentSync(FName(TEXT("SkeletalMesh")), ESyncOption::Passive),
        FComponentSync(FName(TEXT("SkeletalMesh1")), ESyncOption::Passive),
        FComponentSync(FName(TEXT("SkeletalMesh2")), ESyncOption::Passive),
        FComponentSync(FName(TEXT("Hair")), ESyncOption::Passive),
        FComponentSync(FName(TEXT("Eyebrows")), ESyncOption::Passive),
        FComponentSync(FName(TEXT("Mustache")), ESyncOption::Passive),
        FComponentSync(FName(TEXT("Beard")), ESyncOption::Passive),
    };
    const auto AddMapping = [LODSync](
        FName Name, TArray<int32> Values)
    {
        FLODMappingData Mapping;
        Mapping.Mapping = Values;
        LODSync->CustomLODMapping.Add(Name, MoveTemp(Mapping));
    };
    AddMapping(TEXT("Hair"), TArray<int32>{3, 5, 7});
    AddMapping(TEXT("Beard"), TArray<int32>{3, 5, 7});
    AddMapping(TEXT("Mustache"), TArray<int32>{3, 5, 7});
    AddMapping(TEXT("Eyebrows"), TArray<int32>{3, 5, 7});
    AddMapping(TEXT("SkeletalMesh"), TArray<int32>{1, 2, 3});
    AddMapping(TEXT("SkeletalMesh1"), TArray<int32>{1, 2, 3});
    AddMapping(TEXT("SkeletalMesh2"), TArray<int32>{1, 2, 3});

    FString Status;
    TestTrue(TEXT("The exact three-tier fixed profile is accepted"),
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(LODSync, Status));
    TestTrue(TEXT("The schema reports exact creator/gameplay outcomes"),
        Status.Contains(TEXT("tier0 Body/Face0 Hair3 Outfit1"))
        && Status.Contains(TEXT("tier2 Body/Face2 Hair7 Outfit3")));
    constexpr int32 CreatorTier = 0;
    constexpr int32 GameplayTier = 2;
    const TArray<int32>& HairMapping =
        LODSync->CustomLODMapping.FindChecked(
            FName(TEXT("Hair"))).Mapping;
    const TArray<int32>& OutfitMapping =
        LODSync->CustomLODMapping.FindChecked(
            FName(TEXT("SkeletalMesh"))).Mapping;
    LODSync->ForcedLOD = CreatorTier;
    TestEqual(TEXT("Creator requests logical tier 0"),
        LODSync->ForcedLOD, 0);
    TestEqual(TEXT("Creator maps the Hair family to tier 3"),
        HairMapping[CreatorTier], 3);
    TestEqual(TEXT("Creator maps the Outfit to tier 1"),
        OutfitMapping[CreatorTier], 1);
    LODSync->ForcedLOD = GameplayTier;
    TestEqual(TEXT("Gameplay requests logical tier 2"),
        LODSync->ForcedLOD, 2);
    TestEqual(TEXT("Gameplay maps the Hair family to tier 7"),
        HairMapping[GameplayTier], 7);
    TestEqual(TEXT("Gameplay maps the Outfit to tier 3"),
        OutfitMapping[GameplayTier], 3);

    LODSync->NumLODs = 4;
    TestFalse(TEXT("A fourth logical tier is rejected"),
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(LODSync, Status));
    LODSync->NumLODs = 3;

    Swap(LODSync->ComponentsToSync[0], LODSync->ComponentsToSync[1]);
    TestFalse(TEXT("Driver order is part of the fixed contract"),
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(LODSync, Status));
    Swap(LODSync->ComponentsToSync[0], LODSync->ComponentsToSync[1]);

    const FName ExactHairName = LODSync->ComponentsToSync[5].Name;
    LODSync->ComponentsToSync[5].Name = TEXT("Hair5");
    TestFalse(TEXT("Renamed sync entries are rejected"),
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(LODSync, Status));
    LODSync->ComponentsToSync[5].Name = ExactHairName;

    LODSync->CustomLODMapping.FindChecked(
        FName(TEXT("Hair"))).Mapping[1] = 4;
    TestFalse(TEXT("A Hair-family mapping other than 3/5/7 is rejected"),
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(LODSync, Status));
    LODSync->CustomLODMapping.FindChecked(
        FName(TEXT("Hair"))).Mapping[1] = 5;

    FLODMappingData BodyMapping;
    BodyMapping.Mapping = {0, 1, 2};
    LODSync->CustomLODMapping.Add(TEXT("Body"), MoveTemp(BodyMapping));
    TestFalse(TEXT("Body and Face must remain unmapped drivers"),
        UDiscGolfMetaHumanAvatarBackendComponent::
            ValidateGameplayPerformanceLODSyncContract(LODSync, Status));
    LODSync->CustomLODMapping.Remove(TEXT("Body"));

    UDiscGolfMetaHumanAvatarBackendComponent* Backend =
        NewObject<UDiscGolfMetaHumanAvatarBackendComponent>(
            GetTransientPackage());
    TestFalse(TEXT("Unconfigured is never a selectable context"),
        Backend->SetPresentationPolicy(
            EDGMetaHumanPresentationPolicy::Unconfigured));
    TestFalse(TEXT("Out-of-range policy values are rejected"),
        Backend->SetPresentationPolicy(
            static_cast<EDGMetaHumanPresentationPolicy>(255)));
    TestTrue(TEXT("CharacterCreator queues without an active visual"),
        Backend->SetPresentationPolicy(
            EDGMetaHumanPresentationPolicy::CharacterCreator));
    TestEqual(TEXT("The queued creator enum remains exact"),
        Backend->GetRequestedPresentationPolicy(),
        EDGMetaHumanPresentationPolicy::CharacterCreator);
    TestTrue(TEXT("GameplayPerformance queues without an active visual"),
        Backend->SetPresentationPolicy(
            EDGMetaHumanPresentationPolicy::GameplayPerformance));
    TestTrue(TEXT("Queued status uses the stable GameplayPerformance name"),
        Backend->GetPresentationPolicyStatus().Contains(
            TEXT("GameplayPerformance")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BBackendRequestPreservationTest,
    "DiscGolfTour.Character.Session8B.AvatarBackend.RequestPreservation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BBackendRequestPreservationTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    const FName MetaHumanId(
        DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId);
    const FDiscGolfAvatarBackendResolution MissingMetaHuman =
        DiscGolfAvatarBackendRuntime::ResolveBackend(MetaHumanId, nullptr);
    TestEqual(TEXT("Fallback preserves the requested MetaHuman ID"),
        MissingMetaHuman.RequestedBackendId, MetaHumanId);
    TestEqual(TEXT("Missing content resolves presentation to DG master"),
        MissingMetaHuman.ResolvedBackendId,
        FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId));
    TestFalse(TEXT("Missing content never authorizes a visual attempt"),
        MissingMetaHuman.bMetaHumanAttemptAllowed);

    const FName UnknownId(TEXT("vendor_backend"));
    const FDiscGolfAvatarBackendResolution Unknown =
        DiscGolfAvatarBackendRuntime::ResolveBackend(UnknownId, nullptr);
    TestEqual(TEXT("Unknown fallback retains the diagnostic requested ID"),
        Unknown.RequestedBackendId, UnknownId);
    TestEqual(TEXT("Unknown fallback remains the DG master presentation"),
        Unknown.ResolvedBackendId,
        FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId));

    const FDiscGolfAvatarBackendResolution Empty =
        DiscGolfAvatarBackendRuntime::ResolveBackend(NAME_None, nullptr);
    TestEqual(TEXT("An empty request canonicalizes to DG master"),
        Empty.RequestedBackendId,
        FName(DiscGolfAvatarBackendRuntime::DGMasterBackendId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BProfileFailClosedBranchesTest,
    "DiscGolfTour.Character.Session8B.AvatarBackend.ProfileFailClosedBranches",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BProfileFailClosedBranchesTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    FString Reason;
    TestFalse(TEXT("A null profile is rejected"),
        DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            nullptr, Reason));
    TestTrue(TEXT("Null-profile rejection is diagnostic"), !Reason.IsEmpty());

    UDiscGolfAvatarBackendProfile* Profile =
        NewObject<UDiscGolfAvatarBackendProfile>(GetTransientPackage());
    Profile->BackendId = DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId;
    Profile->Backend = EDGAvatarBackend::MetaHumanPreset;
    Profile->MetaHumanRuntimeMode =
        EDGMetaHumanRuntimeMode::ShippingSafeAssembled;
    Profile->VisualActorClass = TSoftClassPtr<AActor>(FSoftObjectPath(
        TEXT("/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.BP_DG_MetaHuman_Default_C")));
    Profile->RetargetAsset = TSoftObjectPtr<UObject>(FSoftObjectPath(
        TEXT("/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.RTG_DGMaster_To_MetaHuman")));
    Profile->bUseRuntimeRetargeting = true;
    Profile->VisualBodyComponentTag = TEXT("DGVisualBody");
    Profile->VisualHeadComponentTag = TEXT("DGVisualHead");
    Profile->PreferredQualityProfileId = TEXT("GameplayHigh");
    Profile->bAllowRuntimeFaceSculpting = false;

    const auto ExpectRejected = [this, Profile, &Reason](
        const TCHAR* Label,
        const TCHAR* ExpectedReason)
    {
        Reason.Reset();
        TestFalse(Label,
            DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
                Profile, Reason));
        TestTrue(*FString::Printf(TEXT("%s reports its rejection reason"), Label),
            Reason.Contains(ExpectedReason));
    };

    Profile->BackendId = TEXT("vendor_backend");
    ExpectRejected(TEXT("Noncanonical backend IDs fail closed"), TEXT("identity"));
    Profile->BackendId = DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId;

    Profile->Backend = EDGAvatarBackend::DGMaster;
    ExpectRejected(TEXT("Mismatched backend enums fail closed"), TEXT("identity"));
    Profile->Backend = EDGAvatarBackend::MetaHumanPreset;

    Profile->MetaHumanRuntimeMode =
        EDGMetaHumanRuntimeMode::ExperimentalCollectionInstance;
    ExpectRejected(TEXT("Experimental instance mode fails closed"), TEXT("Experimental"));
    Profile->MetaHumanRuntimeMode =
        EDGMetaHumanRuntimeMode::ShippingSafeAssembled;

    const TSoftClassPtr<AActor> VisualActorClass = Profile->VisualActorClass;
    Profile->VisualActorClass.Reset();
    ExpectRejected(TEXT("A missing assembled actor class fails closed"), TEXT("actor class"));
    Profile->VisualActorClass = VisualActorClass;

    Profile->bUseRuntimeRetargeting = false;
    ExpectRejected(TEXT("Disabled runtime retargeting fails closed"), TEXT("retarget"));
    Profile->bUseRuntimeRetargeting = true;

    const TSoftObjectPtr<UObject> RetargetAsset = Profile->RetargetAsset;
    Profile->RetargetAsset.Reset();
    ExpectRejected(TEXT("A missing retarget asset fails closed"), TEXT("retarget"));
    Profile->RetargetAsset = RetargetAsset;

    Profile->VisualBodyComponentTag = NAME_None;
    ExpectRejected(TEXT("A missing body tag fails closed"), TEXT("tags"));
    Profile->VisualBodyComponentTag = TEXT("DGVisualBody");

    Profile->VisualHeadComponentTag = Profile->VisualBodyComponentTag;
    ExpectRejected(TEXT("Duplicate body/head tags fail closed"), TEXT("tags"));
    Profile->VisualHeadComponentTag = TEXT("DGVisualHead");

    TestTrue(TEXT("Restoring every required field restores eligibility"),
        DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            Profile, Reason));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession8BUnsafeVisualClassTest,
    "DiscGolfTour.Character.Session8B.AvatarBackend.UnsafeVisualClassRejection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession8BUnsafeVisualClassTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(),
        TEXT("Session8BUnsafeVisualClassWorld"));
    UWorld* World = UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
    if (!TestNotNull(TEXT("The isolated transient test world exists"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    AActor* Owner = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("The transient backend owner exists"), Owner))
    {
        return false;
    }

    USkeletalMeshComponent* SourceMesh =
        NewObject<USkeletalMeshComponent>(Owner, TEXT("AnimationSource"));
    Owner->AddInstanceComponent(SourceMesh);
    Owner->SetRootComponent(SourceMesh);
    USkeletalMesh* SourceAsset = LoadObject<USkeletalMesh>(
        nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
    if (!TestNotNull(TEXT("The engine-only skeletal fixture exists"), SourceAsset))
    {
        return false;
    }
    SourceMesh->SetSkeletalMeshAsset(SourceAsset);
    SourceMesh->RegisterComponent();

    UDiscGolfAvatarBackendComponent* Backend =
        NewObject<UDiscGolfAvatarBackendComponent>(Owner, TEXT("AvatarBackend"));
    Owner->AddInstanceComponent(Backend);
    Backend->RegisterComponent();

    UDiscGolfAvatarBackendProfile* Profile =
        NewObject<UDiscGolfAvatarBackendProfile>(Backend);
    Profile->BackendId = DiscGolfAvatarBackendRuntime::MetaHumanAssembledBackendId;
    Profile->Backend = EDGAvatarBackend::MetaHumanPreset;
    Backend->BackendProfile = Profile;

    const bool bFixtureReady = Backend->IsRegistered()
        && Backend->GetOwner() == Owner
        && Backend->GetWorld() == World
        && SourceMesh->IsRegistered()
        && SourceMesh->GetWorld() == World
        && SourceMesh->GetSkeletalMeshAsset() != nullptr;
    if (!TestTrue(TEXT("Every pre-class-validation build precondition is valid"),
            bFixtureReady))
    {
        return false;
    }

    const FDGFullCharacterCustomization Customization;
    const int32 ActorCountBefore = World->GetActorCount();
    Profile->VisualActorClass = TSoftClassPtr<AActor>(APawn::StaticClass());
    TestFalse(TEXT("Pawn visual classes are rejected before spawn"),
        Backend->BuildVisualBackend(SourceMesh, Customization));
    TestEqual(TEXT("Pawn rejection does not leak a candidate actor"),
        World->GetActorCount(), ActorCountBefore);
    TestNull(TEXT("Pawn rejection cannot publish an active visual"),
        Backend->GetActiveVisualActor());

    Profile->VisualActorClass =
        TSoftClassPtr<AActor>(APlayerController::StaticClass());
    TestFalse(TEXT("Controller visual classes are rejected before spawn"),
        Backend->BuildVisualBackend(SourceMesh, Customization));
    TestEqual(TEXT("Controller rejection does not leak a candidate actor"),
        World->GetActorCount(), ActorCountBefore);
    TestFalse(TEXT("Unsafe-class rejection never reports readiness"),
        Backend->IsVisualBackendReady());
    return true;
}

#endif
