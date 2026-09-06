#include "DiscGolfSession8CookClosureRunner.h"

#include "DiscActor.h"
#include "DiscFlightComponent.h"
#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfAnimInstance.h"
#include "DiscGolfCharacterCreatorWidget.h"
#include "DiscGolfCharacterCustomizationComponent.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.h"
#include "DiscGolfMetaHumanVisualContract.h"
#include "DiscGolfOutfitComponent.h"
#include "DiscGolfRHBHThrowAdapterComponent.h"
#include "DiscGolfRuntimeCookManifest.h"
#include "DiscGolferPawn.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfTour.h"
#include "DiscGolfTourGameMode.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRig.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "HAL/PlatformProperties.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Retargeter/IKRetargeter.h"
#include "Rig/IKRigDefinition.h"

namespace DiscGolfSession8CookClosure
{
constexpr const TCHAR* ManifestObjectPath =
    TEXT("/Game/DiscGolf/Cook/DA_DG_RuntimeCookManifest.DA_DG_RuntimeCookManifest");
constexpr const TCHAR* DGMasterProfileObjectPath =
    TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster.DA_DG_AvatarBackend_DGMaster");
constexpr const TCHAR* MetaHumanProfileObjectPath =
    TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default.DA_DG_AvatarBackend_MetaHuman_Default");
constexpr const TCHAR* MetaHumanWrapperClassPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/BP_DG_MetaHuman_Default.BP_DG_MetaHuman_Default_C");
constexpr const TCHAR* MetaHumanRetargetObjectPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/RTG_DGMaster_To_MetaHuman.RTG_DGMaster_To_MetaHuman");
constexpr const TCHAR* MetaHumanTargetIKObjectPath =
    TEXT("/MetaHumanCharacter/Animation/Retargeting/IK_MH_IKRig.IK_MH_IKRig");
constexpr const TCHAR* MetaHumanSourcePackage =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Source/MHC_DG_Golfer_Default");

const TCHAR* const ExpectedExcludedPackages[] = {
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_ShortCompact"),
    TEXT("/Game/DiscGolf/Tests/Profiles/DA_DG_Test_TallLongArms"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Cleaned/A_DG_RHBH_Synthetic_CLN"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Production/AM_DG_RHBH_SyntheticPipelineTest_v001"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Production/A_DG_RHBH_SyntheticPipelineTest_v001"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Production/DA_DG_AnimationLibrary"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Retargeted/A_DG_RHBH_Synthetic_RTG"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Rigs/IK_DG_RHBH_SyntheticSource"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Rigs/RTG_DG_RHBH_Synthetic_To_Master"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Source/A_DG_RHBH_SyntheticSource_RAW"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Source/SKEL_DG_RHBH_SyntheticSource"),
    TEXT("/Game/DiscGolf/Animation/Mocap/Source/SK_DG_RHBH_SyntheticSource")
};

const TCHAR* const ConflictingRunnerFlags[] = {
    TEXT("Session3OneThrowSmokeTest"),
    TEXT("Session3VisualCapture"),
    TEXT("Session4VisualCapture"),
    TEXT("Session5MocapPipelineSmokeTest"),
    TEXT("Session5MocapVisualCapture"),
    TEXT("Session6OutfitThrowSmokeTest"),
    TEXT("Session6OutfitVisualCapture"),
    TEXT("Session7FullCharacterThrowSmokeTest"),
    TEXT("Session7FullCharacterVisualCapture")
};

bool ValidateInvocation(FString& OutError)
{
    const TCHAR* CommandLine = FCommandLine::Get();
    if (!FPlatformProperties::RequiresCookedData())
    {
        OutError = TEXT("the cook-closure smoke requires a cooked packaged platform");
        return false;
    }
    if (!FApp::IsUnattended()
        || !FParse::Param(CommandLine, TEXT("Session8CookClosureSmokeTest"))
        || !FParse::Param(CommandLine, TEXT("Session8ValidationNoSave")))
    {
        OutError = TEXT(
            "the packaged smoke requires unattended Session8CookClosureSmokeTest and Session8ValidationNoSave together");
        return false;
    }
    for (const TCHAR* ConflictingFlag : ConflictingRunnerFlags)
    {
        if (FParse::Param(CommandLine, ConflictingFlag))
        {
            OutError = FString::Printf(
                TEXT("conflicting validation runner flag was present: -%s"),
                ConflictingFlag);
            return false;
        }
    }

    FString RequestedUserDir;
    if (!FParse::Value(CommandLine, TEXT("UserDir="), RequestedUserDir)
        || RequestedUserDir.IsEmpty() || FPaths::IsRelative(RequestedUserDir))
    {
        OutError = TEXT("an absolute external -UserDir ending in a GUID is required");
        return false;
    }
    FString NormalizedUserDir = FPaths::ConvertRelativePathToFull(RequestedUserDir);
    FPaths::NormalizeDirectoryName(NormalizedUserDir);
    FString ActiveUserDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectUserDir());
    FPaths::NormalizeDirectoryName(ActiveUserDir);
    FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FPaths::NormalizeDirectoryName(ProjectDir);
    FGuid UserDirGuid;
    if (!FPaths::IsSamePath(NormalizedUserDir, ActiveUserDir)
        || FPaths::IsSamePath(NormalizedUserDir, ProjectDir)
        || FPaths::IsUnderDirectory(NormalizedUserDir, ProjectDir)
        || !FGuid::Parse(FPaths::GetCleanFilename(NormalizedUserDir), UserDirGuid)
        || !UserDirGuid.IsValid())
    {
        OutError = TEXT(
            "-UserDir was not the active absolute external GUID directory");
        return false;
    }
    return true;
}

bool IsCookedPackageAvailable(const FString& LongPackageName)
{
    return FindPackage(nullptr, *LongPackageName) != nullptr
        || FPackageName::DoesPackageExist(LongPackageName);
}

UObject* LoadCookedRuntimeExport(
    const FSoftObjectPath& AssetPath,
    bool& bOutUsedGeneratedClass)
{
    bOutUsedGeneratedClass = false;
    const FString PackageName = AssetPath.GetLongPackageName();
    if (!FPackageName::DoesPackageExist(PackageName))
    {
        return nullptr;
    }
    // Blueprint-backed editor assets (Control Rig, AnimBP, and Widget BP) are
    // stripped from cooked builds while their generated runtime classes remain
    // in the same package. The manifest still owns the package dependency; the
    // packaged proof must validate that runtime export rather than requiring an
    // editor-only UObject to survive cooking.
    UClass* ExpectedBaseClass = nullptr;
    if (AssetPath == FSoftObjectPath(
            TEXT("/Game/DiscGolf/Rigs/CR_DG_Master.CR_DG_Master")))
    {
        ExpectedBaseClass = UControlRig::StaticClass();
    }
    else if (AssetPath == FSoftObjectPath(
                 TEXT("/Game/DiscGolf/Animation/ABP_DG_Player.ABP_DG_Player")))
    {
        ExpectedBaseClass = UDiscGolfAnimInstance::StaticClass();
    }
    else if (AssetPath == FSoftObjectPath(
                 TEXT("/Game/DiscGolf/UI/WBP_DG_CharacterCreator.WBP_DG_CharacterCreator")))
    {
        ExpectedBaseClass = UDiscGolfCharacterCreatorWidget::StaticClass();
    }
    if (!ExpectedBaseClass)
    {
        return AssetPath.TryLoad();
    }

    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    const FString GeneratedClassPath = FString::Printf(
        TEXT("%s.%s_C"), *PackageName, *AssetName);
    UClass* GeneratedClass = LoadObject<UClass>(nullptr, *GeneratedClassPath);
    if (!GeneratedClass || !GeneratedClass->IsChildOf(ExpectedBaseClass))
    {
        return nullptr;
    }
    bOutUsedGeneratedClass = true;
    return GeneratedClass;
}

UObject* LoadCookedMetaHumanExport(
    const FSoftObjectPath& AssetPath,
    bool& bOutUsedGeneratedClass)
{
    bOutUsedGeneratedClass = false;
    const FString PackageName = AssetPath.GetLongPackageName();
    if (!FPackageName::DoesPackageExist(PackageName))
    {
        return nullptr;
    }
    if (UObject* DirectAsset = AssetPath.TryLoad())
    {
        return DirectAsset;
    }

    // Some accepted assembly outputs are Blueprint-family editor assets. Their
    // canonical asset object is stripped in cooked data while the generated
    // runtime class remains in the exact same frozen package.
    const FString AssetName = FPackageName::GetLongPackageAssetName(PackageName);
    const FString GeneratedClassPath = FString::Printf(
        TEXT("%s.%s_C"), *PackageName, *AssetName);
    UClass* GeneratedClass = LoadObject<UClass>(nullptr, *GeneratedClassPath);
    if (!GeneratedClass)
    {
        return nullptr;
    }
    bOutUsedGeneratedClass = true;
    return GeneratedClass;
}

bool ValidateForbiddenPackages(
    const UDiscGolfRuntimeCookManifest* Manifest,
    FString& OutError)
{
    TSet<FString> ActualExcluded;
    for (const FString& PackageName : Manifest->ExplicitlyExcludedPackages)
    {
        ActualExcluded.Add(PackageName);
    }
    TSet<FString> ExpectedExcluded;
    for (const TCHAR* PackageName : ExpectedExcludedPackages)
    {
        ExpectedExcluded.Add(PackageName);
    }
    bool bExactExcludedSet =
        ActualExcluded.Num() == UE_ARRAY_COUNT(ExpectedExcludedPackages);
    for (const FString& PackageName : ExpectedExcluded)
    {
        bExactExcludedSet &= ActualExcluded.Contains(PackageName);
    }
    if (!bExactExcludedSet)
    {
        OutError = TEXT("the manifest's 12 excluded packages are not the frozen fixture set");
        return false;
    }
    for (const TCHAR* PackageName : ExpectedExcludedPackages)
    {
        if (IsCookedPackageAvailable(PackageName))
        {
            OutError = FString::Printf(
                TEXT("excluded package is present in the packaged closure: %s"),
                PackageName);
            return false;
        }
    }

    if (Manifest->ExplicitlyExcludedMetaHumanPackages.Num() != 1
        || Manifest->ExplicitlyExcludedMetaHumanPackages[0]
            != MetaHumanSourcePackage
        || IsCookedPackageAvailable(MetaHumanSourcePackage))
    {
        OutError = TEXT(
            "the editor-only source MHC exclusion differs or leaked into the packaged closure");
        return false;
    }
    for (const TSoftObjectPtr<UObject>& Asset : Manifest->MetaHumanRuntimeAssets)
    {
        const FString PackageName =
            Asset.ToSoftObjectPath().GetLongPackageName();
        if (PackageName == MetaHumanSourcePackage
            || PackageName.Contains(TEXT("/Source/"), ESearchCase::IgnoreCase)
            || PackageName.Contains(TEXT("Beard"), ESearchCase::IgnoreCase)
            || PackageName.Contains(TEXT("Mustache"), ESearchCase::IgnoreCase)
            || PackageName.Contains(TEXT("Stubble"), ESearchCase::IgnoreCase))
        {
            OutError = FString::Printf(
                TEXT("editor-only or unaccepted facial-hair package was cook-rooted: %s"),
                *PackageName);
            return false;
        }
    }
    return true;
}

bool ValidatePrimaryAssets(
    UDiscGolfRuntimeCookManifest*& OutManifest,
    UDiscGolfAvatarBackendProfile*& OutDGMasterProfile,
    UDiscGolfAvatarBackendProfile*& OutMetaHumanProfile,
    FString& OutError)
{
    UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
    if (!AssetManager)
    {
        OutError = TEXT("AssetManager was not initialized");
        return false;
    }

    const FPrimaryAssetId ExpectedManifestId(
        UDiscGolfRuntimeCookManifest::PrimaryAssetType, TEXT("dg_runtime_v1"));
    const FSoftObjectPath ExpectedManifestPath(ManifestObjectPath);
    TArray<FPrimaryAssetId> RegisteredManifestIds;
    if (!AssetManager->GetPrimaryAssetIdList(
            UDiscGolfRuntimeCookManifest::PrimaryAssetType,
            RegisteredManifestIds)
        || RegisteredManifestIds.Num() != 1
        || RegisteredManifestIds[0] != ExpectedManifestId
        || AssetManager->GetPrimaryAssetPath(ExpectedManifestId)
            != ExpectedManifestPath)
    {
        OutError = TEXT("the canonical runtime cook manifest is not the sole registered primary asset of its type");
        return false;
    }

    OutManifest = LoadObject<UDiscGolfRuntimeCookManifest>(
        nullptr, ManifestObjectPath);
    FString ContractError;
    if (!OutManifest || !OutManifest->ValidateRuntimeContract(ContractError)
        || OutManifest->GetPrimaryAssetId() != ExpectedManifestId
        || AssetManager->GetPrimaryAssetIdForObject(OutManifest)
            != ExpectedManifestId)
    {
        OutError = OutManifest && !ContractError.IsEmpty()
            ? FString::Printf(TEXT("runtime manifest contract failed: %s"), *ContractError)
            : TEXT("the registered runtime cook manifest could not be loaded and identified");
        return false;
    }

    OutDGMasterProfile =
        OutManifest->AvatarBackendProfiles[0].LoadSynchronous();
    OutMetaHumanProfile =
        OutManifest->AvatarBackendProfiles[1].LoadSynchronous();
    const FSoftObjectPath ExpectedDGMasterProfilePath(DGMasterProfileObjectPath);
    const FSoftObjectPath ExpectedMetaHumanProfilePath(MetaHumanProfileObjectPath);
    const FPrimaryAssetType ProfilePrimaryAssetType(
        TEXT("DiscGolfAvatarBackendProfile"));
    const FPrimaryAssetId ExpectedDGMasterProfileId(
        ProfilePrimaryAssetType, TEXT("DA_DG_AvatarBackend_DGMaster"));
    const FPrimaryAssetId ExpectedMetaHumanProfileId(
        ProfilePrimaryAssetType, TEXT("DA_DG_AvatarBackend_MetaHuman_Default"));
    TArray<FPrimaryAssetId> RegisteredProfileIds;
    const FPrimaryAssetId RegisteredDGMasterProfileId = OutDGMasterProfile
        ? AssetManager->GetPrimaryAssetIdForObject(OutDGMasterProfile)
        : FPrimaryAssetId();
    const FPrimaryAssetId RegisteredMetaHumanProfileId = OutMetaHumanProfile
        ? AssetManager->GetPrimaryAssetIdForObject(OutMetaHumanProfile)
        : FPrimaryAssetId();
    if (!AssetManager->GetPrimaryAssetIdList(
            ProfilePrimaryAssetType, RegisteredProfileIds)
        || RegisteredProfileIds.Num() != 2
        || !RegisteredProfileIds.Contains(ExpectedDGMasterProfileId)
        || !RegisteredProfileIds.Contains(ExpectedMetaHumanProfileId)
        || !OutDGMasterProfile
        || RegisteredDGMasterProfileId != ExpectedDGMasterProfileId
        || AssetManager->GetPrimaryAssetPath(RegisteredDGMasterProfileId)
            != ExpectedDGMasterProfilePath
        || OutDGMasterProfile->BackendId != FName(TEXT("dg_master"))
        || OutDGMasterProfile->Backend != EDGAvatarBackend::DGMaster
        || !OutDGMasterProfile->VisualActorClass.IsNull()
        || !OutDGMasterProfile->RetargetAsset.IsNull()
        || OutDGMasterProfile->bUseRuntimeRetargeting
        || OutDGMasterProfile->VisualBodyComponentTag != FName(TEXT("DGVisualBody"))
        || OutDGMasterProfile->VisualHeadComponentTag != FName(TEXT("DGVisualHead"))
        || OutDGMasterProfile->PreferredQualityProfileId != FName(TEXT("Prototype"))
        || OutDGMasterProfile->bAllowRuntimeFaceSculpting)
    {
        OutError = TEXT(
            "the DGMaster backend profile is unavailable, unregistered, or not the frozen dormant fallback");
        return false;
    }

    FString ProfileContractReason;
    UClass* WrapperClass = OutMetaHumanProfile
        ? OutMetaHumanProfile->VisualActorClass.LoadSynchronous() : nullptr;
    UIKRetargeter* Retargeter = OutMetaHumanProfile
        ? Cast<UIKRetargeter>(
            OutMetaHumanProfile->RetargetAsset.LoadSynchronous()) : nullptr;
    UIKRigDefinition* TargetIKRig = LoadObject<UIKRigDefinition>(
        nullptr, MetaHumanTargetIKObjectPath);
    if (!OutMetaHumanProfile
        || RegisteredMetaHumanProfileId != ExpectedMetaHumanProfileId
        || AssetManager->GetPrimaryAssetPath(RegisteredMetaHumanProfileId)
            != ExpectedMetaHumanProfilePath
        || !DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            OutMetaHumanProfile, ProfileContractReason)
        || OutMetaHumanProfile->PreferredQualityProfileId
            != FName(TEXT("GameplayPerformance"))
        || OutMetaHumanProfile->VisualActorClass.ToSoftObjectPath()
            != FSoftObjectPath(MetaHumanWrapperClassPath)
        || OutMetaHumanProfile->RetargetAsset.ToSoftObjectPath()
            != FSoftObjectPath(MetaHumanRetargetObjectPath)
        || !WrapperClass || !WrapperClass->IsChildOf(AActor::StaticClass())
        || WrapperClass->IsChildOf(APawn::StaticClass())
        || WrapperClass->IsChildOf(AController::StaticClass())
        || !WrapperClass->ImplementsInterface(
            UDiscGolfMetaHumanVisualContract::StaticClass())
        || !Retargeter || !Retargeter->HasSourceIKRig()
        || !Retargeter->HasTargetIKRig() || !TargetIKRig)
    {
        OutError = ProfileContractReason.IsEmpty()
            ? TEXT("the assembled MetaHuman profile/wrapper/retarget/target-IK contract is unavailable or differs")
            : FString::Printf(
                TEXT("assembled MetaHuman profile contract failed: %s"),
                *ProfileContractReason);
        return false;
    }
    return true;
}

bool ValidateNativeAuthority(UWorld* World, FString& OutError)
{
    if (!World)
    {
        OutError = TEXT("runner world was unavailable");
        return false;
    }

    ADiscGolferPawn* Golfer = nullptr;
    int32 PawnCount = 0;
    int32 TotalPawnCount = 0;
    int32 OutfitCount = 0;
    int32 CustomizationCount = 0;
    int32 AvatarAdapterCount = 0;
    int32 RHBHAdapterCount = 0;
    int32 FrameworkThrowCount = 0;
    int32 DiscActorCount = 0;
    UDiscGolfThrowComponent* FrameworkThrow = nullptr;
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        AActor* Actor = *ActorIt;
        TotalPawnCount += Actor->IsA<APawn>() ? 1 : 0;
        if (ADiscGolferPawn* CandidateGolfer = Cast<ADiscGolferPawn>(Actor))
        {
            ++PawnCount;
            Golfer = CandidateGolfer;
        }
        if (Actor->IsA<ADiscActor>())
        {
            ++DiscActorCount;
        }
        TInlineComponentArray<UActorComponent*> Components;
        Actor->GetComponents(Components);
        for (UActorComponent* Component : Components)
        {
            if (!Component)
            {
                OutError = TEXT("world actor exposed a null component entry");
                return false;
            }
            OutfitCount += Component->IsA<UDiscGolfOutfitComponent>() ? 1 : 0;
            CustomizationCount +=
                Component->IsA<UDiscGolfCharacterCustomizationComponent>() ? 1 : 0;
            AvatarAdapterCount +=
                Component->IsA<UDiscGolfAvatarBackendComponent>() ? 1 : 0;
            RHBHAdapterCount +=
                Component->IsA<UDiscGolfRHBHThrowAdapterComponent>() ? 1 : 0;
            FrameworkThrowCount +=
                Component->IsA<UDiscGolfThrowComponent>() ? 1 : 0;
            if (UDiscGolfThrowComponent* CandidateThrow =
                    Cast<UDiscGolfThrowComponent>(Component))
            {
                FrameworkThrow = CandidateThrow;
            }
        }
    }

    ADiscGolfTourGameMode* GameMode = World->GetAuthGameMode<ADiscGolfTourGameMode>();
    UDiscGolfOutfitComponent* Outfit = Golfer ? Golfer->GetOutfitComponent() : nullptr;
    UDiscGolfCharacterCustomizationComponent* Customization = Golfer
        ? Golfer->GetCharacterCustomizationComponent() : nullptr;
    UDiscGolfMetaHumanAvatarBackendComponent* Avatar = Golfer
        ? Golfer->GetAvatarBackendComponent() : nullptr;
    UDiscGolfRHBHThrowAdapterComponent* RHBH = Golfer
        ? Golfer->GetRHBHThrowAdapter() : nullptr;
    if (PawnCount != 1 || TotalPawnCount != 1
        || OutfitCount != 1 || CustomizationCount != 1
        || AvatarAdapterCount != 1 || RHBHAdapterCount != 1
        || FrameworkThrowCount != 1 || DiscActorCount != 0
        || !Golfer || Golfer->GetClass() != ADiscGolferPawn::StaticClass()
        || !Outfit || Outfit->GetClass() != UDiscGolfOutfitComponent::StaticClass()
        || !Outfit->IsRegistered() || Outfit->GetOwner() != Golfer
        || Outfit->CreationMethod != EComponentCreationMethod::Native
        || !Customization
        || Customization->GetClass()
            != UDiscGolfCharacterCustomizationComponent::StaticClass()
        || !Customization->IsRegistered() || Customization->GetOwner() != Golfer
        || Customization->CreationMethod != EComponentCreationMethod::Native
        || !Avatar
        || Avatar->GetClass()
            != UDiscGolfMetaHumanAvatarBackendComponent::StaticClass()
        || !Avatar->IsRegistered() || Avatar->GetOwner() != Golfer
        || Avatar->CreationMethod != EComponentCreationMethod::Native
        || Avatar->BackendProfile || Avatar->IsVisualBackendReady()
        || Avatar->GetActiveVisualActor() || Avatar->GetActiveBackendProfile()
        || Avatar->GetActiveAnimationSourceMesh()
        || !RHBH
        || RHBH->GetClass() != UDiscGolfRHBHThrowAdapterComponent::StaticClass()
        || !RHBH->IsRegistered() || RHBH->GetOwner() != Golfer
        || RHBH->CreationMethod != EComponentCreationMethod::Native
        || !RHBH->IsReadyForAnimatedThrow()
        || !RHBH->GetAuthoritativeLaunchDelegate().IsBound()
        || !FrameworkThrow || !FrameworkThrow->IsRegistered()
        || FrameworkThrow->GetOwner() != Golfer
        || FrameworkThrow->GetClass() != UDiscGolfThrowComponent::StaticClass()
        || FrameworkThrow->CreationMethod != EComponentCreationMethod::Native
        || !GameMode
        || GameMode->GetClass() != ADiscGolfTourGameMode::StaticClass()
        || GameMode->GetActiveDisc())
    {
        OutError = FString::Printf(
            TEXT("native pawn/component or dormant adapter authority differs (dg_pawn=%d total_pawns=%d outfit=%d customization=%d avatar=%d rhbh=%d framework_throw=%d active_discs=%d)"),
            PawnCount, TotalPawnCount, OutfitCount, CustomizationCount,
            AvatarAdapterCount, RHBHAdapterCount, FrameworkThrowCount,
            DiscActorCount);
        return false;
    }

    USkeletalMeshComponent* SourceMesh = Golfer->GetSkeletalGolferMesh();
    const ADiscActor* DiscCDO = GetDefault<ADiscActor>();
    UDiscFlightComponent* FlightAuthority = DiscCDO
        ? DiscCDO->GetFlightComponent() : nullptr;
    TInlineComponentArray<UDiscFlightComponent*> FlightComponents;
    if (DiscCDO)
    {
        DiscCDO->GetComponents(FlightComponents);
    }
    if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset()
        || SourceMesh->GetBoneIndex(TEXT("disc_grip_r")) == INDEX_NONE
        || !DiscCDO || DiscCDO->GetClass() != ADiscActor::StaticClass()
        || !FlightAuthority
        || FlightAuthority->GetClass() != UDiscFlightComponent::StaticClass()
        || FlightComponents.Num() != 1
        || FlightComponents[0] != FlightAuthority)
    {
        OutError = TEXT("accepted disc_grip_r release source or native DiscActor/DiscFlightComponent flight source differs");
        return false;
    }
    return true;
}
}

void ADiscGolfSession8CookClosureRunner::Start()
{
    FString Error;
    if (!DiscGolfSession8CookClosure::ValidateInvocation(Error))
    {
        Fail(Error);
        return;
    }

    UDiscGolfRuntimeCookManifest* Manifest = nullptr;
    UDiscGolfAvatarBackendProfile* DGMasterProfile = nullptr;
    UDiscGolfAvatarBackendProfile* MetaHumanProfile = nullptr;
    if (!DiscGolfSession8CookClosure::ValidatePrimaryAssets(
            Manifest, DGMasterProfile, MetaHumanProfile, Error)
        || !DiscGolfSession8CookClosure::ValidateForbiddenPackages(
            Manifest, Error))
    {
        Fail(Error);
        return;
    }
    LoadedBackendProfiles = {DGMasterProfile, MetaHumanProfile};
    LoadedRuntimeAssets.Reset(Manifest->RuntimeAssets.Num());
    TSet<FString> LoadedPackages;
    int32 LoadedDirectAssetCount = 0;
    int32 LoadedGeneratedClassCount = 0;
    for (const TSoftObjectPtr<UObject>& RuntimeAsset : Manifest->RuntimeAssets)
    {
        const FSoftObjectPath AssetPath = RuntimeAsset.ToSoftObjectPath();
        bool bUsedGeneratedClass = false;
        UObject* LoadedAsset =
            DiscGolfSession8CookClosure::LoadCookedRuntimeExport(
                AssetPath, bUsedGeneratedClass);
        const FString ExpectedPackage = AssetPath.GetLongPackageName();
        if (!LoadedAsset || !LoadedAsset->GetPackage()
            || LoadedAsset->GetPackage()->GetName() != ExpectedPackage
            || LoadedPackages.Contains(ExpectedPackage))
        {
            Fail(FString::Printf(
                TEXT("runtime package exposed neither its canonical asset nor generated class export: %s"),
                *AssetPath.ToString()));
            return;
        }
        LoadedDirectAssetCount += bUsedGeneratedClass ? 0 : 1;
        LoadedGeneratedClassCount += bUsedGeneratedClass ? 1 : 0;
        LoadedPackages.Add(ExpectedPackage);
        LoadedRuntimeAssets.Add(LoadedAsset);
    }
    if (LoadedRuntimeAssets.Num() != 69 || LoadedPackages.Num() != 69
        || LoadedDirectAssetCount != 66 || LoadedGeneratedClassCount != 3)
    {
        Fail(FString::Printf(
            TEXT("runtime closure did not retain exactly 69 package exports as 66 direct assets and three frozen generated classes (exports=%d packages=%d direct=%d generated=%d)"),
            LoadedRuntimeAssets.Num(), LoadedPackages.Num(),
            LoadedDirectAssetCount,
            LoadedGeneratedClassCount));
        return;
    }

    LoadedMetaHumanRuntimeAssets.Reset(Manifest->MetaHumanRuntimeAssets.Num());
    TSet<FString> LoadedMetaHumanPackages;
    int32 LoadedMetaHumanDirectAssetCount = 0;
    int32 LoadedMetaHumanGeneratedClassCount = 0;
    for (const TSoftObjectPtr<UObject>& RuntimeAsset
        : Manifest->MetaHumanRuntimeAssets)
    {
        const FSoftObjectPath AssetPath = RuntimeAsset.ToSoftObjectPath();
        bool bUsedGeneratedClass = false;
        UObject* LoadedAsset =
            DiscGolfSession8CookClosure::LoadCookedMetaHumanExport(
                AssetPath, bUsedGeneratedClass);
        const FString ExpectedPackage = AssetPath.GetLongPackageName();
        if (!LoadedAsset || !LoadedAsset->GetPackage()
            || LoadedAsset->GetPackage()->GetName() != ExpectedPackage
            || LoadedMetaHumanPackages.Contains(ExpectedPackage))
        {
            Fail(FString::Printf(
                TEXT("MetaHuman package exposed neither its canonical asset nor generated class export: %s"),
                *AssetPath.ToString()));
            return;
        }
        LoadedMetaHumanDirectAssetCount += bUsedGeneratedClass ? 0 : 1;
        LoadedMetaHumanGeneratedClassCount += bUsedGeneratedClass ? 1 : 0;
        LoadedMetaHumanPackages.Add(ExpectedPackage);
        LoadedMetaHumanRuntimeAssets.Add(LoadedAsset);
    }
    if (LoadedMetaHumanRuntimeAssets.Num() != 264
        || LoadedMetaHumanPackages.Num() != 264
        || LoadedMetaHumanDirectAssetCount
            + LoadedMetaHumanGeneratedClassCount != 264)
    {
        Fail(FString::Printf(
            TEXT("separate MetaHuman closure did not retain exactly 264 package exports (exports=%d packages=%d direct=%d generated=%d)"),
            LoadedMetaHumanRuntimeAssets.Num(), LoadedMetaHumanPackages.Num(),
            LoadedMetaHumanDirectAssetCount,
            LoadedMetaHumanGeneratedClassCount));
        return;
    }
    if (!DiscGolfSession8CookClosure::ValidateNativeAuthority(
            GetWorld(), Error))
    {
        Fail(Error);
        return;
    }
    Pass(LoadedMetaHumanDirectAssetCount,
        LoadedMetaHumanGeneratedClassCount);
}

void ADiscGolfSession8CookClosureRunner::Fail(const FString& Reason) const
{
    FString SingleLineReason = Reason;
    SingleLineReason.ReplaceInline(TEXT("\r"), TEXT(" "));
    SingleLineReason.ReplaceInline(TEXT("\n"), TEXT(" "));
    UE_LOG(LogDiscGolfTour, Error,
        TEXT("DG_SESSION8_COOK_CLOSURE_SMOKE: FAIL reason=%s"),
        *SingleLineReason);
    FPlatformMisc::RequestExitWithStatus(false, 1);
}

void ADiscGolfSession8CookClosureRunner::Pass(
    int32 MetaHumanDirectAssets,
    int32 MetaHumanGeneratedClasses) const
{
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("DG_SESSION8_COOK_CLOSURE_SMOKE: PASS runtime_packages=69 runtime_exports=69 direct_assets=66 generated_classes=3 metahuman_runtime_packages=264 metahuman_runtime_exports=264 metahuman_direct_assets=%d metahuman_generated_classes=%d backend_profiles=2 excluded_absent=12 editor_source_mhc_absent=1 unrelated_metahuman_manifest_roots=0 native_pawn=1 outfit=1 customization=1 avatar_adapter=1 release_authority=DiscGolfTourGameMode.RequestThrowFromGrip flight_authority=DiscActor.DiscFlightComponent practice_snapshot_save_suppressed=1"),
        MetaHumanDirectAssets, MetaHumanGeneratedClasses);
    FPlatformMisc::RequestExitWithStatus(false, 0);
}
