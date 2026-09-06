#include "DiscGolfMetaHumanAvatarBackendComponent.h"

#include "Animation/AnimInstance.h"
#include "Components/LODSyncComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GroomComponent.h"
#include "HAL/IConsoleManager.h"
#include "LODSyncInterface.h"
#include "MetaHumanComponentUE.h"
#include "Retargeter/IKRetargeter.h"
#include "SkeletalRenderPublic.h"

#include "DiscGolfAvatarBackendProfile.h"

#include "DiscGolfAvatarBackendRuntime.h"
#include "DiscGolfTour.h"
#include "DiscGolfMetaHumanRetargetAnimInstance.h"
#include "DiscGolfMetaHumanOutfitRequiredBonesComponent.h"
#include "DiscGolfMetaHumanVisualContract.h"
#include "UObject/UObjectGlobals.h"

namespace
{
constexpr const TCHAR* Session8HairGroomAssetPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/")
    TEXT("MHC_DG_Golfer_Default/Grooms/Hair_S_Clean.Hair_S_Clean");
constexpr const TCHAR* Session8OutfitMeshAssetPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Generated/")
    TEXT("MHC_DG_Golfer_Default/Clothing/")
    TEXT("MHC_DG_Golfer_Default_Outfits.MHC_DG_Golfer_Default_Outfits");
constexpr const TCHAR* Session8ClothingPostProcessClassPath =
    TEXT("/Game/DiscGolf/Characters/MetaHuman/Common/Animation/")
    TEXT("ABP_Clothing_PostProcess.ABP_Clothing_PostProcess_C");

USkeletalMeshComponent* Session8FindUniqueTaggedSkeletalMesh(
    AActor* VisualActor,
    FName RequiredTag,
    bool& bOutDuplicate)
{
    bOutDuplicate = false;
    USkeletalMeshComponent* Result = nullptr;
    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Candidate : SkeletalMeshes)
    {
        if (!IsValid(Candidate) || !Candidate->ComponentHasTag(RequiredTag))
        {
            continue;
        }
        if (Result)
        {
            bOutDuplicate = true;
            return nullptr;
        }
        Result = Candidate;
    }
    return Result;
}

void Session8EnforceTaggedMeshSafety(USkeletalMeshComponent* Mesh)
{
    if (!IsValid(Mesh))
    {
        return;
    }
    Mesh->SetSimulatePhysics(false);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCanEverAffectNavigation(false);
}

USkeletalMeshComponent* Session8FindUniqueOutfitMesh(
    AActor* VisualActor,
    bool& bOutDuplicate)
{
    bOutDuplicate = false;
    USkeletalMeshComponent* Result = nullptr;
    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Candidate : SkeletalMeshes)
    {
        USkeletalMesh* const Asset = IsValid(Candidate)
            ? Candidate->GetSkeletalMeshAsset() : nullptr;
        if (!Asset || Asset->GetPathName() != Session8OutfitMeshAssetPath)
        {
            continue;
        }
        if (Result)
        {
            bOutDuplicate = true;
            return nullptr;
        }
        Result = Candidate;
    }
    return Result;
}

ULODSyncComponent* Session8FindUniqueLODSync(
    AActor* VisualActor,
    bool& bOutDuplicate)
{
    bOutDuplicate = false;
    ULODSyncComponent* Result = nullptr;
    TInlineComponentArray<ULODSyncComponent*> Components;
    VisualActor->GetComponents(Components);
    for (ULODSyncComponent* Candidate : Components)
    {
        if (!IsValid(Candidate))
        {
            continue;
        }
        if (Result)
        {
            bOutDuplicate = true;
            return nullptr;
        }
        Result = Candidate;
    }
    return Result;
}

UGroomComponent* Session8FindUniqueNamedGroom(
    AActor* VisualActor,
    FName RequiredName,
    bool& bOutDuplicate)
{
    bOutDuplicate = false;
    UGroomComponent* Result = nullptr;
    TInlineComponentArray<UGroomComponent*> Grooms;
    VisualActor->GetComponents(Grooms);
    for (UGroomComponent* Candidate : Grooms)
    {
        if (!IsValid(Candidate) || Candidate->GetFName() != RequiredName)
        {
            continue;
        }
        if (Result)
        {
            bOutDuplicate = true;
            return nullptr;
        }
        Result = Candidate;
    }
    return Result;
}

bool Session8IsVisiblePresentationMesh(
    const USkeletalMeshComponent* Mesh)
{
    return IsValid(Mesh)
        && !Mesh->IsA<UDiscGolfMetaHumanOutfitRequiredBonesComponent>()
        && Mesh->GetSkeletalMeshAsset()
        && Mesh->IsVisible()
        && !Mesh->bHiddenInGame
        && Mesh->bRenderInMainPass
        && !Mesh->bVisibleInSceneCaptureOnly;
}

const TCHAR* Session8PresentationPolicyName(
    EDGMetaHumanPresentationPolicy Policy)
{
    switch (Policy)
    {
    case EDGMetaHumanPresentationPolicy::CharacterCreator:
        return TEXT("CharacterCreator");
    case EDGMetaHumanPresentationPolicy::GameplayPerformance:
        return TEXT("GameplayPerformance");
    default:
        return TEXT("Unconfigured");
    }
}

bool Session8IsolateFixedPresetScale(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    FString& OutStatus)
{
    USceneComponent* const VisualRoot = IsValid(VisualActor)
        ? VisualActor->GetRootComponent() : nullptr;
    if (!IsValid(VisualRoot) || !IsValid(AnimationSourceMesh)
        || !VisualRoot->IsAttachedTo(AnimationSourceMesh))
    {
        OutStatus = TEXT(
            "Fixed MetaHuman scale isolation requires the exact DG-source attachment.");
        return false;
    }

    // The DG source intentionally scales with the selected body height. The
    // curated MetaHuman is a fixed preset, so inheriting that parent scale is
    // an undeclared customization mapping. Keep location/rotation attached,
    // but make the visual root's unit scale absolute.
    VisualRoot->SetAbsolute(false, false, true);
    VisualRoot->SetRelativeScale3D(FVector::OneVector);
    VisualRoot->UpdateComponentToWorld();
    const FVector RelativeScale = VisualRoot->GetRelativeScale3D();
    const FVector WorldScale = VisualRoot->GetComponentScale();
    if (!VisualRoot->IsUsingAbsoluteScale()
        || !RelativeScale.Equals(FVector::OneVector, 0.001f)
        || !WorldScale.Equals(FVector::OneVector, 0.001f))
    {
        OutStatus = FString::Printf(
            TEXT("Fixed MetaHuman scale isolation failed: absolute=%d relative=%s world=%s."),
            VisualRoot->IsUsingAbsoluteScale() ? 1 : 0,
            *RelativeScale.ToCompactString(),
            *WorldScale.ToCompactString());
        return false;
    }
    OutStatus = TEXT(
        "Fixed MetaHuman root retains unit world scale independent of DG body height.");
    return true;
}

bool Session8HasSourceTickPrerequisite(
    const USkeletalMeshComponent* Mesh,
    const USkeletalMeshComponent* AnimationSourceMesh)
{
    if (!IsValid(Mesh) || !IsValid(AnimationSourceMesh))
    {
        return false;
    }
    for (const FTickPrerequisite& Prerequisite :
         Mesh->PrimaryComponentTick.GetPrerequisites())
    {
        if (Prerequisite.PrerequisiteObject.Get() == AnimationSourceMesh
            && Prerequisite.Get()
                == &AnimationSourceMesh->PrimaryComponentTick)
        {
            return true;
        }
    }
    return false;
}

bool Session8HasExactLODMapping(
    const ULODSyncComponent* LODSync,
    FName ComponentName,
    const TArray<int32>& Expected)
{
    const FLODMappingData* const Mapping = IsValid(LODSync)
        ? LODSync->CustomLODMapping.Find(ComponentName) : nullptr;
    return Mapping && Mapping->Mapping == Expected;
}

bool Session8HasEnabledSyncEntry(
    const ULODSyncComponent* LODSync,
    FName ComponentName)
{
    int32 MatchingEntryCount = 0;
    bool bEnabled = false;
    if (IsValid(LODSync))
    {
        for (const FComponentSync& Entry : LODSync->ComponentsToSync)
        {
            if (Entry.Name == ComponentName)
            {
                ++MatchingEntryCount;
                bEnabled = Entry.SyncOption != ESyncOption::Disabled;
            }
        }
    }
    return MatchingEntryCount == 1 && bEnabled;
}

bool Session8HasExternalForcedLODOverride(FString& OutStatus)
{
    IConsoleVariable* const ForceLOD =
        IConsoleManager::Get().FindConsoleVariable(TEXT("r.ForceLOD"));
    if (ForceLOD && ForceLOD->GetInt() >= 0)
    {
        OutStatus = FString::Printf(
            TEXT("Global r.ForceLOD=%d overrides the MetaHuman presentation policy."),
            ForceLOD->GetInt());
        return true;
    }
    return false;
}

void Session8RefreshCreatorGrooms(AActor* VisualActor);

bool Session8ApplyPresentationPolicyState(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    EDGMetaHumanPresentationPolicy Policy,
    FString& OutStatus)
{
    OutStatus.Reset();
    const bool bCreator =
        Policy == EDGMetaHumanPresentationPolicy::CharacterCreator;
    const bool bGameplay =
        Policy == EDGMetaHumanPresentationPolicy::GameplayPerformance;
    bool bDuplicateLODSync = false;
    ULODSyncComponent* const LODSync = IsValid(VisualActor)
        ? Session8FindUniqueLODSync(VisualActor, bDuplicateLODSync)
        : nullptr;
    if ((!bCreator && !bGameplay) || !IsValid(AnimationSourceMesh)
        || bDuplicateLODSync || !IsValid(LODSync)
        || !LODSync->IsRegistered())
    {
        OutStatus = TEXT(
            "Presentation policy apply lacked a valid context, DG source, or unique registered LOD sync component.");
        return false;
    }

    int32 SkeletalMeshCount = 0;
    int32 VisiblePresentationMeshCount = 0;
    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
    {
        if (!IsValid(Mesh)
            || Mesh->IsA<UDiscGolfMetaHumanOutfitRequiredBonesComponent>())
        {
            continue;
        }
        ++SkeletalMeshCount;
        Mesh->SetComponentTickEnabled(true);
        Mesh->PrimaryComponentTick.bTickEvenWhenPaused = bCreator;
        Mesh->bEnableUpdateRateOptimizations = false;
        Mesh->AddTickPrerequisiteComponent(AnimationSourceMesh);
        if (bCreator)
        {
            Mesh->VisibilityBasedAnimTickOption =
                EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        }
        else
        {
            Mesh->VisibilityBasedAnimTickOption =
                EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        }
        VisiblePresentationMeshCount +=
            Session8IsVisiblePresentationMesh(Mesh) ? 1 : 0;
    }

    int32 GroomCount = 0;
    TInlineComponentArray<UGroomComponent*> Grooms;
    VisualActor->GetComponents(Grooms);
    for (UGroomComponent* Groom : Grooms)
    {
        if (!IsValid(Groom))
        {
            continue;
        }
        ++GroomCount;
        Groom->PrimaryComponentTick.bTickEvenWhenPaused = bCreator;
        Groom->SetComponentTickEnabled(true);
    }

    if (SkeletalMeshCount <= 0 || GroomCount <= 0
        || (bGameplay && VisiblePresentationMeshCount <= 0))
    {
        OutStatus = TEXT(
            "Presentation policy apply found no complete skeletal/Groom presentation set.");
        return false;
    }

    LODSync->ForcedLOD = bCreator ? 0 : 2;
    LODSync->PrimaryComponentTick.bTickEvenWhenPaused = bCreator;
    LODSync->SetComponentTickEnabled(true);
    LODSync->UpdateLOD();
    if (bCreator)
    {
        Session8RefreshCreatorGrooms(VisualActor);
    }
    OutStatus = FString::Printf(
        TEXT("%s policy state applied: forced_lod=%d skeletal_meshes=%d visible_presentation_meshes=%d grooms=%d."),
        Session8PresentationPolicyName(Policy),
        LODSync->ForcedLOD,
        SkeletalMeshCount,
        VisiblePresentationMeshCount,
        GroomCount);
    return true;
}

struct FSession8SkeletalPolicySnapshot
{
    TWeakObjectPtr<USkeletalMeshComponent> Mesh;
    EVisibilityBasedAnimTickOption VisibilityTickOption =
        EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    bool bEnableUpdateRateOptimizations = false;
    bool bTickEnabled = false;
    bool bTickEvenWhenPaused = false;
};

struct FSession8GroomPolicySnapshot
{
    TWeakObjectPtr<UGroomComponent> Groom;
    bool bTickEnabled = false;
    bool bTickEvenWhenPaused = false;
};

struct FSession8PresentationPolicySnapshot
{
    TWeakObjectPtr<AActor> VisualActor;
    TWeakObjectPtr<ULODSyncComponent> LODSync;
    int32 ForcedLOD = INDEX_NONE;
    bool bLODSyncTickEnabled = false;
    bool bLODSyncTickEvenWhenPaused = false;
    TArray<FSession8SkeletalPolicySnapshot> SkeletalMeshes;
    TArray<FSession8GroomPolicySnapshot> Grooms;
};

bool Session8CapturePresentationPolicySnapshot(
    AActor* VisualActor,
    FSession8PresentationPolicySnapshot& OutSnapshot,
    FString& OutStatus)
{
    OutSnapshot = FSession8PresentationPolicySnapshot();
    OutStatus.Reset();
    bool bDuplicateLODSync = false;
    ULODSyncComponent* const LODSync =
        IsValid(VisualActor)
        ? Session8FindUniqueLODSync(VisualActor, bDuplicateLODSync)
        : nullptr;
    if (bDuplicateLODSync || !IsValid(LODSync)
        || !LODSync->IsRegistered() || LODSync->GetOwner() != VisualActor)
    {
        OutStatus = TEXT(
            "Presentation policy snapshot requires one registered LOD sync component.");
        return false;
    }

    OutSnapshot.VisualActor = VisualActor;
    OutSnapshot.LODSync = LODSync;
    OutSnapshot.ForcedLOD = LODSync->ForcedLOD;
    OutSnapshot.bLODSyncTickEnabled = LODSync->IsComponentTickEnabled();
    OutSnapshot.bLODSyncTickEvenWhenPaused =
        LODSync->PrimaryComponentTick.bTickEvenWhenPaused;

    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
    {
        if (!IsValid(Mesh)
            || Mesh->IsA<UDiscGolfMetaHumanOutfitRequiredBonesComponent>())
        {
            continue;
        }
        FSession8SkeletalPolicySnapshot& Entry =
            OutSnapshot.SkeletalMeshes.AddDefaulted_GetRef();
        Entry.Mesh = Mesh;
        Entry.VisibilityTickOption = Mesh->VisibilityBasedAnimTickOption;
        Entry.bEnableUpdateRateOptimizations =
            Mesh->bEnableUpdateRateOptimizations;
        Entry.bTickEnabled = Mesh->IsComponentTickEnabled();
        Entry.bTickEvenWhenPaused =
            Mesh->PrimaryComponentTick.bTickEvenWhenPaused;
    }

    TInlineComponentArray<UGroomComponent*> Grooms;
    VisualActor->GetComponents(Grooms);
    for (UGroomComponent* Groom : Grooms)
    {
        if (!IsValid(Groom))
        {
            continue;
        }
        FSession8GroomPolicySnapshot& Entry =
            OutSnapshot.Grooms.AddDefaulted_GetRef();
        Entry.Groom = Groom;
        Entry.bTickEnabled = Groom->IsComponentTickEnabled();
        Entry.bTickEvenWhenPaused =
            Groom->PrimaryComponentTick.bTickEvenWhenPaused;
    }
    return !OutSnapshot.SkeletalMeshes.IsEmpty();
}

bool Session8RestorePresentationPolicySnapshot(
    const FSession8PresentationPolicySnapshot& Snapshot,
    FString& OutStatus)
{
    OutStatus.Reset();
    AActor* const VisualActor = Snapshot.VisualActor.Get();
    ULODSyncComponent* const LODSync = Snapshot.LODSync.Get();
    if (!IsValid(VisualActor) || VisualActor->IsActorBeingDestroyed()
        || !IsValid(LODSync) || !LODSync->IsRegistered()
        || LODSync->GetOwner() != VisualActor)
    {
        OutStatus = TEXT(
            "Presentation policy rollback lost its exact visual actor/LOD sync identity.");
        return false;
    }

    for (const FSession8SkeletalPolicySnapshot& Entry :
         Snapshot.SkeletalMeshes)
    {
        USkeletalMeshComponent* const Mesh = Entry.Mesh.Get();
        if (!IsValid(Mesh))
        {
            OutStatus = TEXT(
                "Presentation policy rollback lost a skeletal mesh.");
            return false;
        }
        Mesh->VisibilityBasedAnimTickOption = Entry.VisibilityTickOption;
        Mesh->bEnableUpdateRateOptimizations =
            Entry.bEnableUpdateRateOptimizations;
        Mesh->SetComponentTickEnabled(Entry.bTickEnabled);
        Mesh->PrimaryComponentTick.bTickEvenWhenPaused =
            Entry.bTickEvenWhenPaused;
    }
    for (const FSession8GroomPolicySnapshot& Entry : Snapshot.Grooms)
    {
        UGroomComponent* const Groom = Entry.Groom.Get();
        if (!IsValid(Groom))
        {
            OutStatus = TEXT(
                "Presentation policy rollback lost a Groom component.");
            return false;
        }
        Groom->SetComponentTickEnabled(Entry.bTickEnabled);
        Groom->PrimaryComponentTick.bTickEvenWhenPaused =
            Entry.bTickEvenWhenPaused;
    }

    LODSync->ForcedLOD = Snapshot.ForcedLOD;
    LODSync->PrimaryComponentTick.bTickEvenWhenPaused =
        Snapshot.bLODSyncTickEvenWhenPaused;
    LODSync->SetComponentTickEnabled(Snapshot.bLODSyncTickEnabled);
    LODSync->UpdateLOD();

    if (LODSync->ForcedLOD != Snapshot.ForcedLOD
        || LODSync->IsComponentTickEnabled()
            != Snapshot.bLODSyncTickEnabled
        || LODSync->PrimaryComponentTick.bTickEvenWhenPaused
            != Snapshot.bLODSyncTickEvenWhenPaused)
    {
        OutStatus = TEXT(
            "Presentation policy rollback did not restore the LOD sync snapshot.");
        return false;
    }
    for (const FSession8SkeletalPolicySnapshot& Entry :
         Snapshot.SkeletalMeshes)
    {
        const USkeletalMeshComponent* const Mesh = Entry.Mesh.Get();
        if (!IsValid(Mesh)
            || Mesh->VisibilityBasedAnimTickOption
                != Entry.VisibilityTickOption
            || Mesh->bEnableUpdateRateOptimizations
                != Entry.bEnableUpdateRateOptimizations
            || Mesh->IsComponentTickEnabled() != Entry.bTickEnabled
            || Mesh->PrimaryComponentTick.bTickEvenWhenPaused
                != Entry.bTickEvenWhenPaused)
        {
            OutStatus = TEXT(
                "Presentation policy rollback did not restore a skeletal tick snapshot.");
            return false;
        }
    }
    for (const FSession8GroomPolicySnapshot& Entry : Snapshot.Grooms)
    {
        const UGroomComponent* const Groom = Entry.Groom.Get();
        if (!IsValid(Groom)
            || Groom->IsComponentTickEnabled() != Entry.bTickEnabled
            || Groom->PrimaryComponentTick.bTickEvenWhenPaused
                != Entry.bTickEvenWhenPaused)
        {
            OutStatus = TEXT(
                "Presentation policy rollback did not restore a Groom tick snapshot.");
            return false;
        }
    }
    OutStatus = TEXT("Previous presentation policy snapshot restored.");
    return true;
}

void Session8RefreshCreatorGrooms(AActor* VisualActor)
{
    TInlineComponentArray<UGroomComponent*> Grooms;
    VisualActor->GetComponents(Grooms);
    for (UGroomComponent* Groom : Grooms)
    {
        if (!IsValid(Groom))
        {
            continue;
        }
        // UGroomComponent detects hidden-to-visible transitions in its tick.
        // Candidate configuration and promotion happen in one game-thread
        // call stack, so execute the hidden tick before promotion and the
        // visible tick after promotion. This preserves the engine's own
        // Niagara reset path even while the creator keeps the world paused.
        if (Groom->IsRegistered())
        {
            Groom->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
            Groom->UpdateBounds();
            Groom->MarkRenderTransformDirty();
            Groom->MarkRenderDynamicDataDirty();
            Groom->MarkRenderStateDirty();
        }
    }
}

bool Session8ValidateMappedLODOutcomes(
    AActor* VisualActor,
    const UDiscGolfAvatarBackendProfile* ExpectedProfile,
    EDGMetaHumanPresentationPolicy Policy,
    FString& OutStatus)
{
    OutStatus.Reset();
    if (!IsValid(VisualActor) || !IsValid(ExpectedProfile))
    {
        OutStatus = TEXT(
            "Mapped LOD outcome validation lacked its visual actor or profile.");
        return false;
    }

    bool bDuplicateBody = false;
    bool bDuplicateHead = false;
    USkeletalMeshComponent* const Body = Session8FindUniqueTaggedSkeletalMesh(
        VisualActor,
        ExpectedProfile->VisualBodyComponentTag,
        bDuplicateBody);
    USkeletalMeshComponent* const Face = Session8FindUniqueTaggedSkeletalMesh(
        VisualActor,
        ExpectedProfile->VisualHeadComponentTag,
        bDuplicateHead);
    bool bDuplicateOutfit = false;
    USkeletalMeshComponent* const Outfit = Session8FindUniqueOutfitMesh(
        VisualActor, bDuplicateOutfit);
    static const FName GroomNames[] = {
        FName(TEXT("Hair")),
        FName(TEXT("Eyebrows")),
        FName(TEXT("Mustache")),
        FName(TEXT("Beard")),
    };
    UGroomComponent* GroomComponents[UE_ARRAY_COUNT(GroomNames)] = {};
    for (int32 GroomIndex = 0;
         GroomIndex < UE_ARRAY_COUNT(GroomNames);
         ++GroomIndex)
    {
        bool bDuplicateGroom = false;
        GroomComponents[GroomIndex] = Session8FindUniqueNamedGroom(
            VisualActor, GroomNames[GroomIndex], bDuplicateGroom);
        if (bDuplicateGroom
            || !IsValid(GroomComponents[GroomIndex])
            || !GroomComponents[GroomIndex]->IsRegistered()
            || GroomComponents[GroomIndex]->GetOwner() != VisualActor)
        {
            OutStatus = FString::Printf(
                TEXT("Mapped LOD outcome validation requires one registered %s Groom owned by the visual actor."),
                *GroomNames[GroomIndex].ToString());
            return false;
        }
    }
    if (bDuplicateBody || bDuplicateHead || bDuplicateOutfit
        || !IsValid(Body) || !IsValid(Face) || !IsValid(Outfit))
    {
        OutStatus = TEXT(
            "Mapped LOD outcome validation did not find unique live Body, Face, and Outfit components.");
        return false;
    }

    const bool bCreator =
        Policy == EDGMetaHumanPresentationPolicy::CharacterCreator;
    const int32 ExpectedBodyLOD = bCreator ? 0 : 2;
    const int32 ExpectedFaceLOD = bCreator ? 0 : 2;
    const int32 ExpectedHairLOD = bCreator ? 3 : 7;
    const int32 ExpectedOutfitLOD = bCreator ? 1 : 3;
    const ILODSyncInterface* const BodyLOD =
        static_cast<const ILODSyncInterface*>(Body);
    const ILODSyncInterface* const FaceLOD =
        static_cast<const ILODSyncInterface*>(Face);
    const ILODSyncInterface* const OutfitLOD =
        static_cast<const ILODSyncInterface*>(Outfit);
    const bool bSkeletalOutcomesMatch = BodyLOD && FaceLOD && OutfitLOD
        && BodyLOD->GetForceRenderedLOD() == ExpectedBodyLOD
        && BodyLOD->GetForceStreamedLOD() == ExpectedBodyLOD
        && FaceLOD->GetForceRenderedLOD() == ExpectedFaceLOD
        && FaceLOD->GetForceStreamedLOD() == ExpectedFaceLOD
        && OutfitLOD->GetForceRenderedLOD() == ExpectedOutfitLOD
        && OutfitLOD->GetForceStreamedLOD() == ExpectedOutfitLOD;
    if (!bSkeletalOutcomesMatch)
    {
        OutStatus = FString::Printf(
            TEXT("%s mapped skeletal LOD outcomes were not exact: Body=%d/%d expected=%d Face=%d/%d expected=%d Outfit=%d/%d expected=%d."),
            Session8PresentationPolicyName(Policy),
            BodyLOD ? BodyLOD->GetForceRenderedLOD() : INDEX_NONE,
            BodyLOD ? BodyLOD->GetForceStreamedLOD() : INDEX_NONE,
            ExpectedBodyLOD,
            FaceLOD ? FaceLOD->GetForceRenderedLOD() : INDEX_NONE,
            FaceLOD ? FaceLOD->GetForceStreamedLOD() : INDEX_NONE,
            ExpectedFaceLOD,
            OutfitLOD ? OutfitLOD->GetForceRenderedLOD() : INDEX_NONE,
            OutfitLOD ? OutfitLOD->GetForceStreamedLOD() : INDEX_NONE,
            ExpectedOutfitLOD);
        return false;
    }

    for (int32 GroomIndex = 0;
         GroomIndex < UE_ARRAY_COUNT(GroomNames);
         ++GroomIndex)
    {
        const ILODSyncInterface* const GroomLOD =
            static_cast<const ILODSyncInterface*>(
                GroomComponents[GroomIndex]);
        // Groom streaming is intentionally unsupported by its LOD-sync
        // interface in UE 5.8. Prove the mapped rendered tier and the engine's
        // INDEX_NONE streaming sentinel; never drive Groom LOD directly.
        if (!GroomLOD
            || GroomLOD->GetForceRenderedLOD() != ExpectedHairLOD
            || GroomLOD->GetForceStreamedLOD() != INDEX_NONE)
        {
            OutStatus = FString::Printf(
                TEXT("%s mapped %s Groom outcome was rendered=%d streamed=%d, expected rendered=%d streamed=%d."),
                Session8PresentationPolicyName(Policy),
                *GroomNames[GroomIndex].ToString(),
                GroomLOD
                    ? GroomLOD->GetForceRenderedLOD() : INDEX_NONE,
                GroomLOD
                    ? GroomLOD->GetForceStreamedLOD() : INDEX_NONE,
                ExpectedHairLOD,
                INDEX_NONE);
            return false;
        }
    }

    OutStatus = FString::Printf(
        TEXT("%s mapped LOD outcomes verified: Body=%d Face=%d Hair-family=%d/streamed-none Outfit=%d."),
        Session8PresentationPolicyName(Policy),
        ExpectedBodyLOD,
        ExpectedFaceLOD,
        ExpectedHairLOD,
        ExpectedOutfitLOD);
    return true;
}

bool Session8IsFiniteEvaluatedPose(const USkeletalMeshComponent* Mesh)
{
    if (!IsValid(Mesh) || Mesh->GetNumBones() <= 0
        || Mesh->GetNumComponentSpaceTransforms() != Mesh->GetNumBones())
    {
        return false;
    }
    for (const FTransform& BoneTransform : Mesh->GetComponentSpaceTransforms())
    {
        const FVector Scale = BoneTransform.GetScale3D();
        if (BoneTransform.ContainsNaN()
            || !FMath::IsFinite(Scale.X)
            || !FMath::IsFinite(Scale.Y)
            || !FMath::IsFinite(Scale.Z))
        {
            return false;
        }
    }
    return true;
}

FString Session8DescribePoseOutliers(const USkeletalMeshComponent* Mesh)
{
    if (!IsValid(Mesh))
    {
        return TEXT("invalid_mesh");
    }

    FString Result;
    int32 ReportedCount = 0;
    const TArray<FTransform>& ComponentSpaceTransforms =
        Mesh->GetComponentSpaceTransforms();
    for (int32 BoneIndex = 0;
         BoneIndex < ComponentSpaceTransforms.Num();
         ++BoneIndex)
    {
        const FTransform& BoneTransform = ComponentSpaceTransforms[BoneIndex];
        const FVector Translation = BoneTransform.GetTranslation();
        if (Translation.GetAbsMax() <= 1000.0f)
        {
            continue;
        }

        if (!Result.IsEmpty())
        {
            Result += TEXT(";");
        }
        Result += FString::Printf(
            TEXT("%s=%s"),
            *Mesh->GetBoneName(BoneIndex).ToString(),
            *Translation.ToCompactString());
        ++ReportedCount;
        if (ReportedCount >= 12)
        {
            Result += TEXT(";truncated");
            break;
        }
    }
    return Result.IsEmpty() ? TEXT("none") : Result;
}

FString Session8DescribePoseAnchors(
    const USkeletalMeshComponent* Mesh,
    const TArray<FName>& BoneNames)
{
    if (!IsValid(Mesh))
    {
        return TEXT("invalid_mesh");
    }

    FString Result;
    const TArray<FTransform>& ComponentSpaceTransforms =
        Mesh->GetComponentSpaceTransforms();
    for (const FName BoneName : BoneNames)
    {
        if (!Result.IsEmpty())
        {
            Result += TEXT(";");
        }
        const int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
        if (!ComponentSpaceTransforms.IsValidIndex(BoneIndex))
        {
            Result += BoneName.ToString() + TEXT("=missing");
            continue;
        }
        Result += FString::Printf(
            TEXT("%s=%s"),
            *BoneName.ToString(),
            *ComponentSpaceTransforms[BoneIndex].GetTranslation().ToCompactString());
    }
    return Result;
}

bool Session8HasPlausibleLiveBounds(
    const USkeletalMeshComponent* Mesh,
    const USkeletalMeshComponent* AnimationSourceMesh,
    float MinimumRadius,
    float MaximumRadius)
{
    if (!IsValid(Mesh) || !IsValid(AnimationSourceMesh)
        || AnimationSourceMesh->Bounds.ContainsNaN()
        || Mesh->Bounds.ContainsNaN()
        || !FMath::IsFinite(Mesh->Bounds.SphereRadius)
        || Mesh->Bounds.SphereRadius < MinimumRadius
        || Mesh->Bounds.SphereRadius > MaximumRadius
        || !FMath::IsFinite(AnimationSourceMesh->Bounds.Origin.X)
        || !FMath::IsFinite(AnimationSourceMesh->Bounds.Origin.Y)
        || !FMath::IsFinite(AnimationSourceMesh->Bounds.Origin.Z)
        || FVector::Distance(
            Mesh->Bounds.Origin, AnimationSourceMesh->Bounds.Origin) > 1000.0f)
    {
        return false;
    }
    const FVector Extent = Mesh->Bounds.BoxExtent;
    return FMath::IsFinite(Extent.X)
        && FMath::IsFinite(Extent.Y)
        && FMath::IsFinite(Extent.Z)
        && Extent.GetMin() > 0.1f;
}

bool Session8HasSupportedOutfitPoseSource(
    const USkeletalMeshComponent* Outfit,
    const USkeletalMeshComponent* Body)
{
    if (!IsValid(Outfit) || !IsValid(Body))
    {
        return false;
    }
    const UAnimInstance* const MainAnimInstance = Outfit->GetAnimInstance();
    const UAnimInstance* const PostProcess = Outfit->GetPostProcessInstance();
    if (Outfit->LeaderPoseComponent.IsValid())
    {
        return Outfit->LeaderPoseComponent.Get() == Body
            && !IsValid(MainAnimInstance)
            && !IsValid(PostProcess);
    }

    const USceneComponent* AttachedParent = Outfit->GetAttachParent();
    const USkeletalMeshComponent* CopyPoseSource = nullptr;
    while (AttachedParent && !CopyPoseSource)
    {
        CopyPoseSource = Cast<USkeletalMeshComponent>(AttachedParent);
        AttachedParent = AttachedParent->GetAttachParent();
    }
    return !IsValid(MainAnimInstance)
        && IsValid(PostProcess)
        && PostProcess->GetClass()->GetPathName()
            == Session8ClothingPostProcessClassPath
        && CopyPoseSource == Body;
}

bool Session8OutfitAnchorsMatchBody(
    const USkeletalMeshComponent* Outfit,
    const USkeletalMeshComponent* Body,
    float& OutMaximumDistanceCm,
    int32& OutAnchorCount)
{
    OutMaximumDistanceCm = 0.0f;
    OutAnchorCount = 0;
    if (!IsValid(Outfit) || !IsValid(Body))
    {
        return false;
    }

    static const TArray<FName> SharedAnchors = {
        FName(TEXT("root")),
        FName(TEXT("pelvis")),
        FName(TEXT("spine_01")),
        FName(TEXT("hand_l")),
        FName(TEXT("hand_r")),
    };
    for (const FName Anchor : SharedAnchors)
    {
        if (Outfit->GetBoneIndex(Anchor) == INDEX_NONE
            || Body->GetBoneIndex(Anchor) == INDEX_NONE)
        {
            return false;
        }
        const FVector OutfitLocation = Outfit->GetBoneLocation(Anchor);
        const FVector BodyLocation = Body->GetBoneLocation(Anchor);
        if (OutfitLocation.ContainsNaN() || BodyLocation.ContainsNaN())
        {
            return false;
        }
        const float DistanceCm = FVector::Distance(
            OutfitLocation, BodyLocation);
        if (!FMath::IsFinite(DistanceCm))
        {
            return false;
        }
        OutMaximumDistanceCm = FMath::Max(
            OutMaximumDistanceCm, DistanceCm);
        ++OutAnchorCount;
    }
    return OutAnchorCount == SharedAnchors.Num()
        && OutMaximumDistanceCm <= 5.0f;
}

bool Session8IsFiniteOutfitPose(
    const USkeletalMeshComponent* Outfit,
    const USkeletalMeshComponent* Body)
{
    if (!Session8HasSupportedOutfitPoseSource(Outfit, Body)
        || Outfit->GetNumBones() <= 0)
    {
        return false;
    }
    if (Outfit->LeaderPoseComponent.IsValid())
    {
        return Session8IsFiniteEvaluatedPose(Body);
    }
    return Session8IsFiniteEvaluatedPose(Outfit);
}

struct FSession8OutfitRequiredBonesCoverageAudit
{
    int32 BodyActualLOD = INDEX_NONE;
    int32 BodyPredictedLOD = INDEX_NONE;
    int32 ReadyBodyLODCount = 0;
    FString ReadyBodyLODs = TEXT("none");
    int32 MissingExpectedRequiredCount = 0;
    FString MissingExpectedRequired = TEXT("none");
    int32 MissingExpectedFillCount = 0;
    FString MissingExpectedFill = TEXT("none");
    int32 MissingUsedRequiredCount = 0;
    FString MissingUsedRequired = TEXT("none");
    int32 MissingUsedFillCount = 0;
    FString MissingUsedFill = TEXT("none");
};

void Session8AppendLODMissingCoverage(
    int32 LODIndex,
    int32 MissingCount,
    const FString& MissingList,
    int32& InOutTotalMissingCount,
    TArray<FString>& InOutLODEntries)
{
    InOutTotalMissingCount += MissingCount;
    if (MissingCount > 0)
    {
        InOutLODEntries.Add(FString::Printf(
            TEXT("lod%d:%s"), LODIndex, *MissingList));
    }
}

bool Session8AuditOutfitRequiredBonesCoverage(
    USkeletalMeshComponent* Body,
    UDiscGolfMetaHumanOutfitRequiredBonesComponent* Helper,
    FSession8OutfitRequiredBonesCoverageAudit& OutAudit,
    FString& OutStatus)
{
    OutAudit = FSession8OutfitRequiredBonesCoverageAudit();
    OutStatus.Reset();
    if (!IsValid(Body) || !IsValid(Helper) || !Body->IsRegistered())
    {
        OutStatus = TEXT(
            "MetaHuman Outfit all-Body-LOD coverage audit lacked its registered Body or configured helper.");
        return false;
    }

    USkeletalMesh* const BodyAsset = Body->GetSkeletalMeshAsset();
    FSkeletalMeshRenderData* const BodyRenderData = BodyAsset
        ? BodyAsset->GetResourceForRendering() : nullptr;
    const FSkeletalMeshObject* const BodyMeshObject = Body->GetMeshObject();
    OutAudit.BodyActualLOD = BodyMeshObject
        ? BodyMeshObject->GetLOD() : INDEX_NONE;
    OutAudit.BodyPredictedLOD = Body->GetPredictedLODLevel();
    const bool bActualLODKnown = OutAudit.BodyActualLOD != INDEX_NONE;
    if (!BodyRenderData
        || !BodyRenderData->LODRenderData.IsValidIndex(
            OutAudit.BodyPredictedLOD)
        || (bActualLODKnown
            && !BodyRenderData->LODRenderData.IsValidIndex(
                OutAudit.BodyActualLOD)))
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Body all-LOD coverage audit had invalid current LODs: actual=%d predicted=%d."),
            OutAudit.BodyActualLOD,
            OutAudit.BodyPredictedLOD);
        return false;
    }

    bool bScannedActualLOD = false;
    bool bScannedPredictedLOD = false;
    TArray<FString> ReadyLODEntries;
    TArray<FString> MissingExpectedRequiredEntries;
    TArray<FString> MissingExpectedFillEntries;
    TArray<FString> MissingUsedRequiredEntries;
    TArray<FString> MissingUsedFillEntries;
    for (int32 LODIndex = 0;
         LODIndex < BodyRenderData->LODRenderData.Num();
         ++LODIndex)
    {
        const FSkeletalMeshLODRenderData& LOD =
            BodyRenderData->LODRenderData[LODIndex];
        if (!LOD.IsDataReady())
        {
            continue;
        }

        ++OutAudit.ReadyBodyLODCount;
        ReadyLODEntries.Add(FString::FromInt(LODIndex));
        bScannedActualLOD |= LODIndex == OutAudit.BodyActualLOD;
        bScannedPredictedLOD |= LODIndex == OutAudit.BodyPredictedLOD;

        TArray<FBoneIndexType> RequiredBones;
        TArray<FBoneIndexType> FillBones;
        Body->ComputeRequiredBones(
            RequiredBones, FillBones, LODIndex, false);

        int32 MissingCount = 0;
        FString MissingList;
        if (!Helper->DescribeMissingRequiredLeaderBones(
                RequiredBones, MissingCount, MissingList))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit Body LOD %d expected RequiredBones coverage audit was invalid."),
                LODIndex);
            return false;
        }
        Session8AppendLODMissingCoverage(
            LODIndex,
            MissingCount,
            MissingList,
            OutAudit.MissingExpectedRequiredCount,
            MissingExpectedRequiredEntries);

        if (!Helper->DescribeMissingRequiredLeaderBones(
                FillBones, MissingCount, MissingList))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit Body LOD %d expected FillComponentSpace coverage audit was invalid."),
                LODIndex);
            return false;
        }
        Session8AppendLODMissingCoverage(
            LODIndex,
            MissingCount,
            MissingList,
            OutAudit.MissingExpectedFillCount,
            MissingExpectedFillEntries);

        if (!Helper->DescribeMissingMappedOutfitUsedBones(
                RequiredBones, MissingCount, MissingList))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit Body LOD %d all-Outfit-LOD used RequiredBones coverage audit was invalid."),
                LODIndex);
            return false;
        }
        Session8AppendLODMissingCoverage(
            LODIndex,
            MissingCount,
            MissingList,
            OutAudit.MissingUsedRequiredCount,
            MissingUsedRequiredEntries);

        if (!Helper->DescribeMissingMappedOutfitUsedBones(
                FillBones, MissingCount, MissingList))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit Body LOD %d all-Outfit-LOD used FillComponentSpace coverage audit was invalid."),
                LODIndex);
            return false;
        }
        Session8AppendLODMissingCoverage(
            LODIndex,
            MissingCount,
            MissingList,
            OutAudit.MissingUsedFillCount,
            MissingUsedFillEntries);
    }

    if (OutAudit.ReadyBodyLODCount <= 0
        || (bActualLODKnown && !bScannedActualLOD)
        || !bScannedPredictedLOD)
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Body current LODs were not both render-ready during the all-LOD coverage audit: actual=%d scanned=%d predicted=%d scanned=%d ready_count=%d."),
            OutAudit.BodyActualLOD,
            bScannedActualLOD ? 1 : 0,
            OutAudit.BodyPredictedLOD,
            bScannedPredictedLOD ? 1 : 0,
            OutAudit.ReadyBodyLODCount);
        return false;
    }

    OutAudit.ReadyBodyLODs = FString::Join(ReadyLODEntries, TEXT(","));
    OutAudit.MissingExpectedRequired =
        MissingExpectedRequiredEntries.IsEmpty()
        ? TEXT("none")
        : FString::Join(MissingExpectedRequiredEntries, TEXT(";"));
    OutAudit.MissingExpectedFill = MissingExpectedFillEntries.IsEmpty()
        ? TEXT("none")
        : FString::Join(MissingExpectedFillEntries, TEXT(";"));
    OutAudit.MissingUsedRequired = MissingUsedRequiredEntries.IsEmpty()
        ? TEXT("none")
        : FString::Join(MissingUsedRequiredEntries, TEXT(";"));
    OutAudit.MissingUsedFill = MissingUsedFillEntries.IsEmpty()
        ? TEXT("none")
        : FString::Join(MissingUsedFillEntries, TEXT(";"));
    return true;
}
}

bool UDiscGolfMetaHumanAvatarBackendComponent::
    ValidateGameplayPerformanceLODSyncContract(
        const ULODSyncComponent* LODSync,
        FString& OutStatus)
{
    OutStatus.Reset();
    if (!IsValid(LODSync))
    {
        OutStatus = TEXT("GameplayPerformance LOD sync component is invalid.");
        return false;
    }
    if (LODSync->NumLODs != 3 || LODSync->MinLOD != 0)
    {
        OutStatus = FString::Printf(
            TEXT("GameplayPerformance requires NumLODs=3 and MinLOD=0; actual NumLODs=%d MinLOD=%d."),
            LODSync->NumLODs,
            LODSync->MinLOD);
        return false;
    }

    struct FExpectedSyncEntry
    {
        const TCHAR* Name;
        ESyncOption Option;
    };
    static const FExpectedSyncEntry ExpectedEntries[] = {
        {TEXT("Body"), ESyncOption::Drive},
        {TEXT("Face"), ESyncOption::Drive},
        {TEXT("SkeletalMesh"), ESyncOption::Passive},
        {TEXT("SkeletalMesh1"), ESyncOption::Passive},
        {TEXT("SkeletalMesh2"), ESyncOption::Passive},
        {TEXT("Hair"), ESyncOption::Passive},
        {TEXT("Eyebrows"), ESyncOption::Passive},
        {TEXT("Mustache"), ESyncOption::Passive},
        {TEXT("Beard"), ESyncOption::Passive},
    };
    if (LODSync->ComponentsToSync.Num() != UE_ARRAY_COUNT(ExpectedEntries))
    {
        OutStatus = FString::Printf(
            TEXT("GameplayPerformance requires exactly nine ordered sync entries; actual=%d."),
            LODSync->ComponentsToSync.Num());
        return false;
    }
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedEntries); ++Index)
    {
        const FComponentSync& Actual = LODSync->ComponentsToSync[Index];
        const FExpectedSyncEntry& Expected = ExpectedEntries[Index];
        if (Actual.Name != FName(Expected.Name)
            || Actual.SyncOption != Expected.Option)
        {
            OutStatus = FString::Printf(
                TEXT("GameplayPerformance sync entry %d was %s/%d, expected %s/%d."),
                Index,
                *Actual.Name.ToString(),
                static_cast<int32>(Actual.SyncOption),
                Expected.Name,
                static_cast<int32>(Expected.Option));
            return false;
        }
    }

    static const TArray<int32> HairFamilyMapping = {3, 5, 7};
    static const TArray<int32> SkeletalMapping = {1, 2, 3};
    static const FName HairFamilyNames[] = {
        FName(TEXT("Hair")),
        FName(TEXT("Beard")),
        FName(TEXT("Mustache")),
        FName(TEXT("Eyebrows")),
    };
    static const FName SkeletalNames[] = {
        FName(TEXT("SkeletalMesh")),
        FName(TEXT("SkeletalMesh1")),
        FName(TEXT("SkeletalMesh2")),
    };
    if (LODSync->CustomLODMapping.Num() != 7
        || LODSync->CustomLODMapping.Contains(FName(TEXT("Body")))
        || LODSync->CustomLODMapping.Contains(FName(TEXT("Face"))))
    {
        OutStatus = TEXT(
            "GameplayPerformance requires exactly seven custom mappings and no Body/Face mapping.");
        return false;
    }
    for (const FName Name : HairFamilyNames)
    {
        if (!Session8HasEnabledSyncEntry(LODSync, Name)
            || !Session8HasExactLODMapping(
                LODSync, Name, HairFamilyMapping))
        {
            OutStatus = FString::Printf(
                TEXT("GameplayPerformance Hair-family mapping for %s must be exactly [3,5,7]."),
                *Name.ToString());
            return false;
        }
    }
    for (const FName Name : SkeletalNames)
    {
        if (!Session8HasEnabledSyncEntry(LODSync, Name)
            || !Session8HasExactLODMapping(
                LODSync, Name, SkeletalMapping))
        {
            OutStatus = FString::Printf(
                TEXT("GameplayPerformance skeletal mapping for %s must be exactly [1,2,3]."),
                *Name.ToString());
            return false;
        }
    }
    if (!Session8HasEnabledSyncEntry(LODSync, FName(TEXT("Body")))
        || !Session8HasEnabledSyncEntry(LODSync, FName(TEXT("Face"))))
    {
        OutStatus = TEXT(
            "GameplayPerformance requires enabled, unmapped Body and Face sync drivers.");
        return false;
    }

    OutStatus = TEXT(
        "GameplayPerformance LOD sync schema verified: tier0 Body/Face0 Hair3 Outfit1; tier2 Body/Face2 Hair7 Outfit3.");
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::
    ValidatePresentationPolicyState(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const UDiscGolfAvatarBackendProfile* ExpectedProfile,
        EDGMetaHumanPresentationPolicy Policy,
        bool bRequireLiveLODOutcomes,
        FString& OutStatus) const
{
    OutStatus.Reset();
    const bool bCreator =
        Policy == EDGMetaHumanPresentationPolicy::CharacterCreator;
    const bool bGameplay =
        Policy == EDGMetaHumanPresentationPolicy::GameplayPerformance;
    const USceneComponent* const VisualRoot = IsValid(VisualActor)
        ? VisualActor->GetRootComponent() : nullptr;
    if ((!bCreator && !bGameplay) || !IsValid(VisualActor)
        || !IsValid(AnimationSourceMesh) || !IsValid(ExpectedProfile)
        || !VisualRoot || !VisualRoot->IsAttachedTo(AnimationSourceMesh)
        || VisualActor->GetOwner() != GetOwner()
        || VisualActor->GetWorld() != AnimationSourceMesh->GetWorld())
    {
        OutStatus = TEXT(
            "Presentation policy validation lost the verified visual/DG-source ownership boundary.");
        return false;
    }
    if (bGameplay
        && ExpectedProfile->PreferredQualityProfileId
            != FName(TEXT("GameplayPerformance")))
    {
        OutStatus = FString::Printf(
            TEXT("Gameplay policy is valid only for the GameplayPerformance profile; actual=%s."),
            *ExpectedProfile->PreferredQualityProfileId.ToString());
        return false;
    }

    FString OverrideStatus;
    if (Session8HasExternalForcedLODOverride(OverrideStatus))
    {
        OutStatus = OverrideStatus;
        return false;
    }
    bool bDuplicateLODSync = false;
    ULODSyncComponent* const LODSync = Session8FindUniqueLODSync(
        VisualActor, bDuplicateLODSync);
    FString LODContractStatus;
    if (bDuplicateLODSync || !IsValid(LODSync)
        || !LODSync->IsRegistered()
        || !ValidateGameplayPerformanceLODSyncContract(
            LODSync, LODContractStatus))
    {
        OutStatus = LODContractStatus.IsEmpty()
            ? TEXT("Presentation policy lost its unique registered LOD sync component.")
            : LODContractStatus;
        return false;
    }

    const int32 ExpectedForcedLOD = bCreator ? 0 : 2;
    if (LODSync->ForcedLOD != ExpectedForcedLOD
        || !LODSync->IsComponentTickEnabled()
        || LODSync->PrimaryComponentTick.bTickEvenWhenPaused != bCreator)
    {
        OutStatus = FString::Printf(
            TEXT("%s LOD sync state was not exact: forced_lod=%d tick_enabled=%d tick_when_paused=%d."),
            Session8PresentationPolicyName(Policy),
            LODSync->ForcedLOD,
            LODSync->IsComponentTickEnabled() ? 1 : 0,
            LODSync->PrimaryComponentTick.bTickEvenWhenPaused ? 1 : 0);
        return false;
    }

    int32 SkeletalMeshCount = 0;
    int32 VisiblePresentationMeshCount = 0;
    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
    {
        if (!IsValid(Mesh)
            || Mesh->IsA<UDiscGolfMetaHumanOutfitRequiredBonesComponent>())
        {
            continue;
        }
        ++SkeletalMeshCount;
        const bool bVisiblePresentation =
            Session8IsVisiblePresentationMesh(Mesh);
        VisiblePresentationMeshCount += bVisiblePresentation ? 1 : 0;
        if (!Mesh->IsComponentTickEnabled()
            || Mesh->bEnableUpdateRateOptimizations
            || Mesh->PrimaryComponentTick.bTickEvenWhenPaused != bCreator
            || !Session8HasSourceTickPrerequisite(
                Mesh, AnimationSourceMesh)
            || (bCreator
                && Mesh->VisibilityBasedAnimTickOption
                    != EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
            || (bGameplay
                && Mesh->VisibilityBasedAnimTickOption
                    != EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered))
        {
            OutStatus = FString::Printf(
                TEXT("%s skeletal policy failed on %s: visible=%d visibility_tick=%d tick_enabled=%d uro=%d tick_when_paused=%d source_prerequisite=%d."),
                Session8PresentationPolicyName(Policy),
                *Mesh->GetName(),
                bVisiblePresentation ? 1 : 0,
                static_cast<int32>(Mesh->VisibilityBasedAnimTickOption),
                Mesh->IsComponentTickEnabled() ? 1 : 0,
                Mesh->bEnableUpdateRateOptimizations ? 1 : 0,
                Mesh->PrimaryComponentTick.bTickEvenWhenPaused ? 1 : 0,
                Session8HasSourceTickPrerequisite(
                    Mesh, AnimationSourceMesh) ? 1 : 0);
            return false;
        }
    }
    if (SkeletalMeshCount <= 0
        || (bGameplay && VisiblePresentationMeshCount <= 0))
    {
        OutStatus = TEXT(
            "Presentation policy validation found no eligible skeletal presentation meshes.");
        return false;
    }

    int32 GroomCount = 0;
    TInlineComponentArray<UGroomComponent*> Grooms;
    VisualActor->GetComponents(Grooms);
    for (const UGroomComponent* Groom : Grooms)
    {
        if (!IsValid(Groom))
        {
            continue;
        }
        ++GroomCount;
        if (!Groom->IsComponentTickEnabled()
            || Groom->PrimaryComponentTick.bTickEvenWhenPaused != bCreator)
        {
            OutStatus = FString::Printf(
                TEXT("%s Groom policy failed on %s: tick_enabled=%d tick_when_paused=%d."),
                Session8PresentationPolicyName(Policy),
                *Groom->GetName(),
                Groom->IsComponentTickEnabled() ? 1 : 0,
                Groom->PrimaryComponentTick.bTickEvenWhenPaused ? 1 : 0);
            return false;
        }
    }
    if (GroomCount <= 0)
    {
        OutStatus = TEXT(
            "Presentation policy validation found no Groom components.");
        return false;
    }

    TInlineComponentArray<UMetaHumanComponentUE*> MetaHumanComponents;
    VisualActor->GetComponents(MetaHumanComponents);
    if (MetaHumanComponents.Num() != 1
        || !IsValid(MetaHumanComponents[0])
        || !MetaHumanComponents[0]->IsRegistered())
    {
        OutStatus = TEXT(
            "Presentation policy validation did not preserve the unique MetaHuman component.");
        return false;
    }

    FString LiveLODStatus;
    if (bRequireLiveLODOutcomes
        && !Session8ValidateMappedLODOutcomes(
            VisualActor, ExpectedProfile, Policy, LiveLODStatus))
    {
        OutStatus = LiveLODStatus;
        return false;
    }
    OutStatus = FString::Printf(
        TEXT("%s policy verified: forced_lod=%d skeletal_meshes=%d visible_presentation_meshes=%d grooms=%d live_lods=%d. %s"),
        Session8PresentationPolicyName(Policy),
        ExpectedForcedLOD,
        SkeletalMeshCount,
        VisiblePresentationMeshCount,
        GroomCount,
        bRequireLiveLODOutcomes ? 1 : 0,
        *LiveLODStatus);
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::
    ApplyPresentationPolicyTransactionally(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const UDiscGolfAvatarBackendProfile* ExpectedProfile,
        EDGMetaHumanPresentationPolicy Policy,
        UDiscGolfMetaHumanOutfitRequiredBonesComponent* ExpectedHelper,
        USkeletalMeshComponent* ExpectedBody,
        USkeletalMeshComponent* ExpectedOutfit,
        bool bRequireLiveLODOutcomes,
        FString& OutStatus,
        bool* bOutMutationAttempted,
        bool* bOutRollbackAttempted,
        bool* bOutRollbackSucceeded)
{
    OutStatus.Reset();
    if (bOutMutationAttempted)
    {
        *bOutMutationAttempted = false;
    }
    if (bOutRollbackAttempted)
    {
        *bOutRollbackAttempted = false;
    }
    if (bOutRollbackSucceeded)
    {
        *bOutRollbackSucceeded = false;
    }
    if (Policy != EDGMetaHumanPresentationPolicy::CharacterCreator
        && Policy
            != EDGMetaHumanPresentationPolicy::GameplayPerformance)
    {
        OutStatus = TEXT("The requested presentation policy is not selectable.");
        return false;
    }

    FSession8PresentationPolicySnapshot Snapshot;
    FString SnapshotStatus;
    if (!Session8CapturePresentationPolicySnapshot(
            VisualActor, Snapshot, SnapshotStatus))
    {
        OutStatus = SnapshotStatus;
        return false;
    }

    FString ContractStatus;
    if (!ValidateGameplayPerformanceLODSyncContract(
            Snapshot.LODSync.Get(), ContractStatus))
    {
        OutStatus = ContractStatus;
        return false;
    }
    if (Policy == EDGMetaHumanPresentationPolicy::GameplayPerformance
        && (!IsValid(ExpectedProfile)
            || ExpectedProfile->PreferredQualityProfileId
                != FName(TEXT("GameplayPerformance"))))
    {
        OutStatus = IsValid(ExpectedProfile)
            ? FString::Printf(
                TEXT("Gameplay policy rejected non-GameplayPerformance profile %s."),
                *ExpectedProfile->PreferredQualityProfileId.ToString())
            : TEXT("Gameplay policy rejected a missing profile.");
        return false;
    }
    FString OverrideStatus;
    if (Session8HasExternalForcedLODOverride(OverrideStatus))
    {
        OutStatus = OverrideStatus;
        return false;
    }

    auto RollBack = [this,
                     &Snapshot,
                     ExpectedHelper,
                     ExpectedBody,
                     ExpectedOutfit,
                     bOutRollbackAttempted,
                     bOutRollbackSucceeded](
        const FString& Failure,
        FString& Result)
    {
        if (bOutRollbackAttempted)
        {
            *bOutRollbackAttempted = true;
        }
        FString RestoreStatus;
        bool bRestored = Session8RestorePresentationPolicySnapshot(
            Snapshot, RestoreStatus);
        FString HelperRestoreStatus;
        if (bRestored && IsValid(ExpectedHelper))
        {
            bRestored = IsValid(ExpectedBody) && IsValid(ExpectedOutfit)
                && RefreshAndVerifyOutfitRequiredBonesHelper(
                    ExpectedHelper,
                    ExpectedBody,
                    ExpectedOutfit,
                    HelperRestoreStatus);
        }
        if (bOutRollbackSucceeded)
        {
            *bOutRollbackSucceeded = bRestored;
        }
        Result = FString::Printf(
            TEXT("%s Rollback %s: %s %s"),
            *Failure,
            bRestored ? TEXT("succeeded") : TEXT("failed"),
            *RestoreStatus,
            *HelperRestoreStatus);
        return false;
    };

    FString ApplyStatus;
    if (bOutMutationAttempted)
    {
        *bOutMutationAttempted = true;
    }
    if (!Session8ApplyPresentationPolicyState(
            VisualActor, AnimationSourceMesh, Policy, ApplyStatus))
    {
        return RollBack(ApplyStatus, OutStatus);
    }

    FString HelperStatus;
    if (IsValid(ExpectedHelper)
        && (!IsValid(ExpectedBody) || !IsValid(ExpectedOutfit)
            || !ExpectedHelper->IsConfiguredFor(
                ExpectedOutfit, ExpectedBody)
            || !RefreshAndVerifyOutfitRequiredBonesHelper(
                ExpectedHelper,
                ExpectedBody,
                ExpectedOutfit,
                HelperStatus)))
    {
        const FString Failure = FString::Printf(
            TEXT("%s policy did not preserve the Outfit required-bones helper: %s"),
            Session8PresentationPolicyName(Policy),
            *HelperStatus);
        return RollBack(Failure, OutStatus);
    }

    FString VerificationStatus;
    if (!ValidatePresentationPolicyState(
            VisualActor,
            AnimationSourceMesh,
            ExpectedProfile,
            Policy,
            bRequireLiveLODOutcomes,
            VerificationStatus))
    {
        return RollBack(VerificationStatus, OutStatus);
    }

    OutStatus = FString::Printf(
        TEXT("%s %s %s"),
        *ApplyStatus,
        *VerificationStatus,
        *HelperStatus);
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::SetPresentationPolicy(
    EDGMetaHumanPresentationPolicy Policy)
{
    CommitPendingPresentationPolicyEvidence();
    AActor* const ActiveVisual = GetActiveVisualActor();
    if (Policy != EDGMetaHumanPresentationPolicy::CharacterCreator
        && Policy
            != EDGMetaHumanPresentationPolicy::GameplayPerformance)
    {
        if (IsValid(ActiveVisual))
        {
            ++PresentationPolicyTransitionFailureCount;
        }
        LastPresentationPolicyStatus =
            TEXT("The requested presentation policy is not selectable.");
        LastAdapterStatus = LastPresentationPolicyStatus;
        return false;
    }

    if (!IsValid(ActiveVisual))
    {
        RequestedPresentationPolicy = Policy;
        LastPresentationPolicyStatus = FString::Printf(
            TEXT("%s policy queued for the next verified visual."),
            Session8PresentationPolicyName(Policy));
        return true;
    }
    if (RequestedPresentationPolicy == Policy
        && GetVerifiedPresentationPolicy() == Policy
        && IsPresentationPolicyVerified())
    {
        // A verified same-context request is a real no-op: no component
        // mutation and no public transaction bookkeeping changes.
        return true;
    }
    if (ActiveVisual != VerifiedVisualActor
        || !IsValid(VerifiedVisualBody)
        || !IsValid(VerifiedVisualOutfit)
        || !IsValid(OutfitRequiredBonesHelper))
    {
        ++PresentationPolicyTransitionFailureCount;
        LastPresentationPolicyStatus = TEXT(
            "Active MetaHuman policy transition rejected stale verified visual/helper state.");
        LastAdapterStatus = LastPresentationPolicyStatus;
        return false;
    }

    const EDGMetaHumanPresentationPolicy PreviousPolicy =
        GetVerifiedPresentationPolicy();
    FString TransitionStatus;
    bool bRollbackAttempted = false;
    bool bRollbackSucceeded = false;
    if (!ApplyPresentationPolicyTransactionally(
            ActiveVisual,
            GetActiveAnimationSourceMesh(),
            GetActiveBackendProfile(),
            Policy,
            OutfitRequiredBonesHelper,
            VerifiedVisualBody,
            VerifiedVisualOutfit,
            true,
            TransitionStatus,
            nullptr,
            &bRollbackAttempted,
            &bRollbackSucceeded))
    {
        ++PresentationPolicyTransitionFailureCount;
        if (bRollbackAttempted)
        {
            if (bRollbackSucceeded)
            {
                ++PresentationPolicyRollbackSuccessCount;
            }
            else
            {
                ++PresentationPolicyRollbackFailureCount;
                DestroyVisualBackend();
                ResetOutfitRequiredBonesHelper();
                VerifiedVisualActor = nullptr;
                VerifiedVisualBody = nullptr;
                VerifiedVisualHead = nullptr;
                VerifiedVisualOutfit = nullptr;
                VerifiedPresentationPolicy =
                    EDGMetaHumanPresentationPolicy::Unconfigured;
                TransitionStatus += TEXT(
                    " Rollback failure destroyed the visual backend fail-closed.");
            }
        }
        LastPresentationPolicyStatus = TransitionStatus;
        LastAdapterStatus = TransitionStatus;
        return false;
    }

    RequestedPresentationPolicy = Policy;
    VerifiedPresentationPolicy = Policy;
    LastPresentationPolicyStatus = TransitionStatus;
    if (PreviousPolicy != Policy)
    {
        ++PresentationPolicyTransitionSuccessCount;
    }
    return true;
}

void UDiscGolfMetaHumanAvatarBackendComponent::
    CommitPendingPresentationPolicyEvidence() const
{
    AActor* const PendingActor = PendingPresentationPolicyActor.Get();
    if (!IsValid(PendingActor) || PendingActor->IsActorBeingDestroyed())
    {
        PendingPresentationPolicyActor = nullptr;
        PendingPresentationPolicyStatus.Reset();
        bPendingPresentationPolicyTransitionSuccess = false;
        return;
    }
    if (!IsVisualBackendReady()
        || GetActiveVisualActor() != PendingActor
        || VerifiedVisualActor.Get() != PendingActor)
    {
        return;
    }

    LastPresentationPolicyStatus = PendingPresentationPolicyStatus;
    if (bPendingPresentationPolicyTransitionSuccess)
    {
        ++PresentationPolicyTransitionSuccessCount;
    }
    PendingPresentationPolicyActor = nullptr;
    PendingPresentationPolicyStatus.Reset();
    bPendingPresentationPolicyTransitionSuccess = false;
}

EDGMetaHumanPresentationPolicy
UDiscGolfMetaHumanAvatarBackendComponent::
    GetVerifiedPresentationPolicy() const
{
    CommitPendingPresentationPolicyEvidence();
    return IsVisualBackendReady()
        && GetActiveVisualActor() == VerifiedVisualActor
        ? VerifiedPresentationPolicy
        : EDGMetaHumanPresentationPolicy::Unconfigured;
}

FString UDiscGolfMetaHumanAvatarBackendComponent::
    GetPresentationPolicyStatus() const
{
    CommitPendingPresentationPolicyEvidence();
    return LastPresentationPolicyStatus;
}

int32 UDiscGolfMetaHumanAvatarBackendComponent::
    GetPresentationPolicyTransitionSuccessCount() const
{
    CommitPendingPresentationPolicyEvidence();
    return PresentationPolicyTransitionSuccessCount;
}

int32 UDiscGolfMetaHumanAvatarBackendComponent::
    GetPresentationPolicyTransitionFailureCount() const
{
    CommitPendingPresentationPolicyEvidence();
    return PresentationPolicyTransitionFailureCount;
}

int32 UDiscGolfMetaHumanAvatarBackendComponent::
    GetPresentationPolicyRollbackSuccessCount() const
{
    CommitPendingPresentationPolicyEvidence();
    return PresentationPolicyRollbackSuccessCount;
}

int32 UDiscGolfMetaHumanAvatarBackendComponent::
    GetPresentationPolicyRollbackFailureCount() const
{
    CommitPendingPresentationPolicyEvidence();
    return PresentationPolicyRollbackFailureCount;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::
    IsPresentationPolicyVerified() const
{
    const EDGMetaHumanPresentationPolicy Policy =
        GetVerifiedPresentationPolicy();
    if (Policy == EDGMetaHumanPresentationPolicy::Unconfigured
        || !IsValid(VerifiedVisualBody)
        || !IsValid(VerifiedVisualOutfit)
        || !IsValid(OutfitRequiredBonesHelper)
        || !OutfitRequiredBonesHelper->IsConfiguredFor(
            VerifiedVisualOutfit, VerifiedVisualBody))
    {
        return false;
    }
    FString Status;
    return ValidatePresentationPolicyState(
        GetActiveVisualActor(),
        GetActiveAnimationSourceMesh(),
        GetActiveBackendProfile(),
        Policy,
        true,
        Status);
}

int32 UDiscGolfMetaHumanAvatarBackendComponent::
    GetVerifiedPresentationForcedLOD() const
{
    AActor* const ActiveVisual = GetActiveVisualActor();
    if (GetVerifiedPresentationPolicy()
            == EDGMetaHumanPresentationPolicy::Unconfigured
        || !IsValid(ActiveVisual))
    {
        return INDEX_NONE;
    }
    bool bDuplicateLODSync = false;
    const ULODSyncComponent* const LODSync =
        Session8FindUniqueLODSync(ActiveVisual, bDuplicateLODSync);
    return !bDuplicateLODSync && IsValid(LODSync)
        ? LODSync->ForcedLOD : INDEX_NONE;
}

USkeletalMeshComponent*
UDiscGolfMetaHumanAvatarBackendComponent::GetVerifiedVisualBody() const
{
    return IsVisualBackendReady()
        && GetActiveVisualActor() == VerifiedVisualActor
        && IsValid(VerifiedVisualBody)
        ? VerifiedVisualBody.Get() : nullptr;
}

USkeletalMeshComponent*
UDiscGolfMetaHumanAvatarBackendComponent::GetVerifiedVisualHead() const
{
    return IsVisualBackendReady()
        && GetActiveVisualActor() == VerifiedVisualActor
        && IsValid(VerifiedVisualHead)
        ? VerifiedVisualHead.Get() : nullptr;
}

USkeletalMeshComponent*
UDiscGolfMetaHumanAvatarBackendComponent::GetVerifiedVisualOutfit() const
{
    return IsVisualBackendReady()
        && GetActiveVisualActor() == VerifiedVisualActor
        && IsValid(VerifiedVisualOutfit)
        ? VerifiedVisualOutfit.Get() : nullptr;
}

void UDiscGolfMetaHumanAvatarBackendComponent::OnRegister()
{
    Super::OnRegister();
    OnAvatarBackendReady.AddUniqueDynamic(
        this,
        &UDiscGolfMetaHumanAvatarBackendComponent::HandleAvatarBackendReady);
}

void UDiscGolfMetaHumanAvatarBackendComponent::HandleAvatarBackendReady(
    FDGAvatarBackendState State)
{
    (void)State;
    // The framework broadcasts only after publishing the candidate as its
    // active visual. This is the first exact point at which promotion evidence
    // and its committed-context transition counter may become public.
    CommitPendingPresentationPolicyEvidence();
}

bool UDiscGolfMetaHumanAvatarBackendComponent::ConfigureVisualBackend_Implementation(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const FDGFullCharacterCustomization& Customization)
{
    CommitPendingPresentationPolicyEvidence();
    if (!IsVisualBackendReady() && !GetActiveVisualActor())
    {
        // The framework destroy API is intentionally nonvirtual. Normalize
        // derived caches here so an immediate DG -> MetaHuman reselect in the
        // same creator session cannot observe valid-but-stale UObjects.
        ResetOutfitRequiredBonesHelper();
        VerifiedVisualActor = nullptr;
        VerifiedVisualBody = nullptr;
        VerifiedVisualHead = nullptr;
        VerifiedVisualOutfit = nullptr;
        VerifiedPresentationPolicy =
            EDGMetaHumanPresentationPolicy::Unconfigured;
    }

    UDiscGolfAvatarBackendProfile* CandidateProfile = BackendProfile.Get();
    FString ProfileStatus;
    if (!DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
            CandidateProfile, ProfileStatus))
    {
        LastAdapterStatus = ProfileStatus;
        return false;
    }
    if (!IsValid(VisualActor) || !IsValid(AnimationSourceMesh)
        || VisualActor->IsA<APawn>() || VisualActor->IsA<AController>()
        || VisualActor->FindComponentByClass<UMovementComponent>()
        || !VisualActor->GetClass()->ImplementsInterface(
            UDiscGolfMetaHumanVisualContract::StaticClass()))
    {
        LastAdapterStatus =
            TEXT("Assembled visual does not implement the verified project contract.");
        return false;
    }
    UIKRetargeter* Retargeter = Cast<UIKRetargeter>(
        CandidateProfile->RetargetAsset.LoadSynchronous());
    if (!Retargeter || !Retargeter->HasSourceIKRig()
        || !Retargeter->HasTargetIKRig())
    {
        LastAdapterStatus =
            TEXT("DG-to-MetaHuman retargeter or its source/target rigs are invalid.");
        return false;
    }

    FString ScaleIsolationStatus;
    if (!Session8IsolateFixedPresetScale(
            VisualActor, AnimationSourceMesh, ScaleIsolationStatus))
    {
        LastAdapterStatus = ScaleIsolationStatus;
        return false;
    }

    FString ContractStatus;
    if (!IDiscGolfMetaHumanVisualContract::Execute_ConfigureFromDGAnimationSource(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            Customization,
            ContractStatus))
    {
        LastAdapterStatus = ContractStatus.IsEmpty()
            ? TEXT("Assembled visual rejected DG-source configuration.")
            : ContractStatus;
        return false;
    }
    if (!Session8IsolateFixedPresetScale(
            VisualActor, AnimationSourceMesh, ScaleIsolationStatus))
    {
        LastAdapterStatus = ScaleIsolationStatus;
        return false;
    }

    USkeletalMeshComponent* CandidateBody = nullptr;
    USkeletalMeshComponent* CandidateHead = nullptr;
    FString ValidationStatus;
    if (!ValidateConfiguredVisual(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            false,
            CandidateBody,
            CandidateHead,
            ValidationStatus))
    {
        LastAdapterStatus = ValidationStatus;
        return false;
    }

    FString CreatorPolicyStatus;
    if (!ApplyPresentationPolicyTransactionally(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            EDGMetaHumanPresentationPolicy::CharacterCreator,
            nullptr,
            CandidateBody,
            nullptr,
            false,
            CreatorPolicyStatus))
    {
        ++PresentationPolicyTransitionFailureCount;
        LastAdapterStatus = CreatorPolicyStatus;
        return false;
    }

    CandidateBody->SetAnimInstanceClass(
        UDiscGolfMetaHumanRetargetAnimInstance::StaticClass());
    CandidateBody->InitAnim(true);
    UDiscGolfMetaHumanRetargetAnimInstance* RetargetInstance =
        Cast<UDiscGolfMetaHumanRetargetAnimInstance>(
            CandidateBody->GetAnimInstance());
    FString RetargetStatus;
    if (!RetargetInstance
        || !RetargetInstance->ConfigureAndVerify(
            Retargeter, AnimationSourceMesh, RetargetStatus))
    {
        LastAdapterStatus = RetargetStatus.IsEmpty()
            ? TEXT("Native DG-to-MetaHuman retarget instance did not initialize.")
            : RetargetStatus;
        return false;
    }

    // The creator pauses the world. Force one evaluated retarget pose now;
    // subsequent paused frames remain source-ordered by the tick prerequisites.
    CandidateBody->TickAnimation(0.0f, false);
    CandidateBody->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
    CandidateBody->RefreshBoneTransforms(nullptr);
    CandidateBody->RefreshFollowerComponents();

    if (!ValidateConfiguredVisual(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            true,
            CandidateBody,
            CandidateHead,
            ValidationStatus))
    {
        LastAdapterStatus = ValidationStatus;
        return false;
    }

    LastAdapterStatus = FString::Printf(
        TEXT("Assembled visual body/head and DG retarget configured under the hidden creator policy; awaiting visible activation. %s %s"),
        *CreatorPolicyStatus,
        *ScaleIsolationStatus);
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::FinalizeVisualBackendActivation(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const FDGFullCharacterCustomization& Customization)
{
    if (!Super::FinalizeVisualBackendActivation(
            VisualActor, AnimationSourceMesh, Customization))
    {
        LastAdapterStatus = TEXT(
            "Assembled visual activation lost its DG animation source.");
        return false;
    }
    CommitPendingPresentationPolicyEvidence();

    AActor* const PreviousActiveActor = GetActiveVisualActor();
    UDiscGolfMetaHumanOutfitRequiredBonesComponent* const PreviousHelper =
        OutfitRequiredBonesHelper.Get();
    USkeletalMeshComponent* const PreviousBody = VerifiedVisualBody.Get();
    USkeletalMeshComponent* const PreviousOutfit = VerifiedVisualOutfit.Get();
    const bool bHadPreviousActiveVisual = IsValid(PreviousActiveActor);
    TOptional<FSession8PresentationPolicySnapshot>
        PreviousActivePolicySnapshot;
    bool bActivePolicyMutationOccurred = false;
    const EDGMetaHumanPresentationPolicy PreviousRequestedPolicy =
        RequestedPresentationPolicy;
    const EDGMetaHumanPresentationPolicy PreviousVerifiedPolicy =
        VerifiedPresentationPolicy;
    const FString PreviousPresentationPolicyStatus =
        LastPresentationPolicyStatus;
    if (bHadPreviousActiveVisual)
    {
        FString PreviousHelperStatus;
        if (VerifiedVisualActor.Get() != PreviousActiveActor
            || !IsValid(PreviousHelper) || !IsValid(PreviousBody)
            || !IsValid(PreviousOutfit)
            || !RefreshAndVerifyOutfitRequiredBonesHelper(
                PreviousHelper,
                PreviousBody,
                PreviousOutfit,
                PreviousHelperStatus))
        {
            LastAdapterStatus = FString::Printf(
                TEXT("Active MetaHuman Outfit required-bones transaction was invalid before candidate activation: %s"),
                *PreviousHelperStatus);
            return false;
        }
        if (PreviousActiveActor == VisualActor)
        {
            FSession8PresentationPolicySnapshot Snapshot;
            FString SnapshotStatus;
            if (!Session8CapturePresentationPolicySnapshot(
                    PreviousActiveActor, Snapshot, SnapshotStatus))
            {
                LastAdapterStatus = FString::Printf(
                    TEXT("Active MetaHuman policy transaction could not snapshot the same visual before staging: %s"),
                    *SnapshotStatus);
                return false;
            }
            PreviousActivePolicySnapshot.Emplace(MoveTemp(Snapshot));
        }
    }
    else if (IsValid(PreviousHelper) || IsValid(VerifiedVisualActor)
        || IsValid(PreviousBody) || IsValid(PreviousOutfit))
    {
        LastAdapterStatus = TEXT(
            "MetaHuman Outfit required-bones transaction found stale verified state without an active visual.");
        return false;
    }

    UDiscGolfMetaHumanOutfitRequiredBonesComponent* CandidateHelper = nullptr;
    auto RejectCandidate = [this,
                            &CandidateHelper,
                            bHadPreviousActiveVisual,
                            PreviousHelper,
                            PreviousBody,
                            PreviousOutfit,
                            &PreviousActivePolicySnapshot,
                            &bActivePolicyMutationOccurred,
                            PreviousRequestedPolicy,
                            PreviousVerifiedPolicy,
                            PreviousPresentationPolicyStatus](
        const FString& FailureStatus,
        bool bPolicyApplyFailure = false)
    {
        if (CandidateHelper != PreviousHelper)
        {
            DestroyOutfitRequiredBonesHelper(CandidateHelper);
        }
        CandidateHelper = nullptr;

        FString PolicyRestoreStatus;
        bool bPreviousRestored = true;
        const bool bActivePolicyRollbackAttempted =
            bActivePolicyMutationOccurred
            && PreviousActivePolicySnapshot.IsSet();
        if (bActivePolicyRollbackAttempted)
        {
            bPreviousRestored =
                Session8RestorePresentationPolicySnapshot(
                    PreviousActivePolicySnapshot.GetValue(),
                    PolicyRestoreStatus);
        }
        FString HelperRestoreStatus;
        if (bPreviousRestored && bHadPreviousActiveVisual)
        {
            bPreviousRestored =
                RefreshAndVerifyOutfitRequiredBonesHelper(
                    PreviousHelper,
                    PreviousBody,
                    PreviousOutfit,
                    HelperRestoreStatus);
        }
        RequestedPresentationPolicy = PreviousRequestedPolicy;
        VerifiedPresentationPolicy = PreviousVerifiedPolicy;
        LastPresentationPolicyStatus = PreviousPresentationPolicyStatus;
        if (bPolicyApplyFailure || bActivePolicyMutationOccurred)
        {
            ++PresentationPolicyTransitionFailureCount;
        }
        if (bActivePolicyRollbackAttempted)
        {
            if (bPreviousRestored)
            {
                ++PresentationPolicyRollbackSuccessCount;
            }
            else
            {
                ++PresentationPolicyRollbackFailureCount;
                DestroyVisualBackend();
                ResetOutfitRequiredBonesHelper();
                VerifiedVisualActor = nullptr;
                VerifiedVisualBody = nullptr;
                VerifiedVisualHead = nullptr;
                VerifiedVisualOutfit = nullptr;
                VerifiedPresentationPolicy =
                    EDGMetaHumanPresentationPolicy::Unconfigured;
                PendingPresentationPolicyActor = nullptr;
                PendingPresentationPolicyStatus.Reset();
                bPendingPresentationPolicyTransitionSuccess = false;
                LastPresentationPolicyStatus = FString::Printf(
                    TEXT("%s Active presentation rollback failed; visual backend destroyed fail-closed. %s %s"),
                    *FailureStatus,
                    *PolicyRestoreStatus,
                    *HelperRestoreStatus);
                LastAdapterStatus = LastPresentationPolicyStatus;
                return false;
            }
        }
        if (!bHadPreviousActiveVisual)
        {
            LastAdapterStatus = FailureStatus;
        }
        else if (bPreviousRestored)
        {
            LastAdapterStatus = FString::Printf(
                TEXT("%s Previous presentation policy/Outfit contract restored: %s %s"),
                *FailureStatus,
                *PolicyRestoreStatus,
                *HelperRestoreStatus);
            UE_LOG(
                LogDiscGolfTour,
                Display,
                TEXT("%s"),
                *LastAdapterStatus);
        }
        else
        {
            LastAdapterStatus = FString::Printf(
                TEXT("%s Previous presentation/Outfit contract could not be retained; visual backend destroyed fail-closed: %s %s"),
                *FailureStatus,
                *PolicyRestoreStatus,
                *HelperRestoreStatus);
            DestroyVisualBackend();
            ResetOutfitRequiredBonesHelper();
            VerifiedVisualActor = nullptr;
            VerifiedVisualBody = nullptr;
            VerifiedVisualHead = nullptr;
            VerifiedVisualOutfit = nullptr;
            VerifiedPresentationPolicy =
                EDGMetaHumanPresentationPolicy::Unconfigured;
            PendingPresentationPolicyActor = nullptr;
            PendingPresentationPolicyStatus.Reset();
            bPendingPresentationPolicyTransitionSuccess = false;
            LastPresentationPolicyStatus = LastAdapterStatus;
        }
        return false;
    };

    UDiscGolfAvatarBackendProfile* CandidateProfile = BackendProfile.Get();
    USkeletalMeshComponent* CandidateBody = nullptr;
    USkeletalMeshComponent* CandidateHead = nullptr;
    FString ValidationStatus;
    if (!ValidateConfiguredVisual(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            true,
            CandidateBody,
            CandidateHead,
            ValidationStatus))
    {
        return RejectCandidate(ValidationStatus);
    }

    // The generated Blueprint deliberately authored presentation meshes with
    // OnlyTickPoseWhenRendered. Configuration happens while the candidate is
    // hidden, so normalize the live runtime instance and evaluate once after
    // the actor becomes visible. This keeps the DG source authoritative while
    // preventing a stale hidden reference pose/bounds state from being culled.
    FString VisibleCreatorPolicyStatus;
    bool bVisibleCreatorMutationAttempted = false;
    const bool bVisibleCreatorApplied =
        ApplyPresentationPolicyTransactionally(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            EDGMetaHumanPresentationPolicy::CharacterCreator,
            nullptr,
            CandidateBody,
            nullptr,
            true,
            VisibleCreatorPolicyStatus,
            &bVisibleCreatorMutationAttempted);
    bActivePolicyMutationOccurred =
        bActivePolicyMutationOccurred
        || (PreviousActiveActor == VisualActor
            && bVisibleCreatorMutationAttempted);
    if (!bVisibleCreatorApplied)
    {
        return RejectCandidate(VisibleCreatorPolicyStatus, true);
    }
    CandidateBody->TickAnimation(0.0f, false);
    CandidateBody->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
    CandidateBody->RefreshBoneTransforms(nullptr);
    CandidateBody->RefreshFollowerComponents();

    bool bDuplicateOutfit = false;
    USkeletalMeshComponent* const CandidateOutfit =
        Session8FindUniqueOutfitMesh(VisualActor, bDuplicateOutfit);
    if (bDuplicateOutfit || !IsValid(CandidateOutfit))
    {
        return RejectCandidate(TEXT(
            "Assembled visual did not retain its unique generated outfit mesh."));
    }
    CandidateOutfit->AddTickPrerequisiteComponent(CandidateBody);
    CandidateOutfit->TickAnimation(0.0f, false);
    CandidateOutfit->TickComponent(
        0.0f, ELevelTick::LEVELTICK_All, nullptr);
    CandidateOutfit->RefreshBoneTransforms(nullptr);

    if (IsValid(PreviousHelper) && PreviousBody == CandidateBody
        && PreviousOutfit != CandidateOutfit)
    {
        return RejectCandidate(TEXT(
            "Fixed MetaHuman customization replaced its Outfit component inside one active transaction."));
    }

    FString RequiredBonesStatus;
    if (IsValid(PreviousHelper) && PreviousBody == CandidateBody
        && PreviousOutfit == CandidateOutfit)
    {
        CandidateHelper = PreviousHelper;
        if (!RefreshAndVerifyOutfitRequiredBonesHelper(
                CandidateHelper,
                CandidateBody,
                CandidateOutfit,
                RequiredBonesStatus))
        {
            return RejectCandidate(TEXT(
                "Repeated MetaHuman finalization could not retain the verified Outfit required-bones helper transactionally."));
        }
    }
    else if (!InstallOutfitRequiredBonesHelper(
            VisualActor,
            CandidateBody,
            CandidateOutfit,
            CandidateHelper,
            RequiredBonesStatus))
    {
        return RejectCandidate(RequiredBonesStatus);
    }

    // Re-sample the authored Outfit only after the Body has recalculated and
    // evaluated the helper-contributed garment bones.
    CandidateOutfit->TickAnimation(0.0f, false);
    CandidateOutfit->TickComponent(
        0.0f, ELevelTick::LEVELTICK_All, nullptr);
    CandidateOutfit->RefreshBoneTransforms(nullptr);
    CandidateOutfit->UpdateBounds();
    CandidateOutfit->MarkRenderTransformDirty();
    CandidateOutfit->MarkRenderDynamicDataDirty();
    CandidateOutfit->MarkRenderStateDirty();

    // Re-apply the creator context after helper installation. This drives the
    // Groom hidden-to-visible reset and verifies the helper under exact tier 0.
    FString PostHelperCreatorPolicyStatus;
    bool bPostHelperCreatorMutationAttempted = false;
    const bool bPostHelperCreatorApplied =
        ApplyPresentationPolicyTransactionally(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            EDGMetaHumanPresentationPolicy::CharacterCreator,
            CandidateHelper,
            CandidateBody,
            CandidateOutfit,
            true,
            PostHelperCreatorPolicyStatus,
            &bPostHelperCreatorMutationAttempted);
    bActivePolicyMutationOccurred =
        bActivePolicyMutationOccurred
        || (PreviousActiveActor == VisualActor
            && bPostHelperCreatorMutationAttempted);
    if (!bPostHelperCreatorApplied)
    {
        return RejectCandidate(PostHelperCreatorPolicyStatus, true);
    }
    RequiredBonesStatus += FString::Printf(
        TEXT(" Post-helper creator-policy verification: %s"),
        *PostHelperCreatorPolicyStatus);

    TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
    VisualActor->GetComponents(SkeletalMeshes);
    for (USkeletalMeshComponent* Mesh : SkeletalMeshes)
    {
        if (!IsValid(Mesh))
        {
            continue;
        }
        Mesh->UpdateBounds();
        Mesh->MarkRenderTransformDirty();
        Mesh->MarkRenderDynamicDataDirty();
        Mesh->MarkRenderStateDirty();
    }

    const bool bBodyPoseFinite = Session8IsFiniteEvaluatedPose(CandidateBody);
    const bool bBodyBoundsPlausible = Session8HasPlausibleLiveBounds(
        CandidateBody, AnimationSourceMesh, 30.0f, 500.0f);
    const bool bHeadBoundsPlausible = Session8HasPlausibleLiveBounds(
        CandidateHead, AnimationSourceMesh, 5.0f, 250.0f);
    const bool bOutfitBoundsPlausible = Session8HasPlausibleLiveBounds(
        CandidateOutfit, AnimationSourceMesh, 10.0f, 300.0f);
    const bool bOutfitPoseFinite = Session8IsFiniteOutfitPose(
        CandidateOutfit, CandidateBody);
    float OutfitMaximumAnchorDistanceCm = 0.0f;
    int32 OutfitAnchorCount = 0;
    const bool bOutfitAnchorsMatch = Session8OutfitAnchorsMatchBody(
        CandidateOutfit,
        CandidateBody,
        OutfitMaximumAnchorDistanceCm,
        OutfitAnchorCount);
    const int32 BodyLODCount = CandidateBody->GetSkeletalMeshAsset()
        ? CandidateBody->GetSkeletalMeshAsset()->GetLODNum() : 0;
    const int32 HeadLODCount = CandidateHead->GetSkeletalMeshAsset()
        ? CandidateHead->GetSkeletalMeshAsset()->GetLODNum() : 0;
    const FString BodyPoseOutliers =
        Session8DescribePoseOutliers(CandidateBody);
    const FString SourcePoseOutliers =
        Session8DescribePoseOutliers(AnimationSourceMesh);
    const FString BodyPoseAnchors = Session8DescribePoseAnchors(
        CandidateBody,
        {FName(TEXT("root")), FName(TEXT("pelvis")), FName(TEXT("spine_01")),
         FName(TEXT("spine_05")), FName(TEXT("Head"))});
    const FString SourcePoseAnchors = Session8DescribePoseAnchors(
        AnimationSourceMesh,
        {FName(TEXT("root")), FName(TEXT("pelvis")), FName(TEXT("spine_01")),
         FName(TEXT("spine_04")), FName(TEXT("head"))});
    if (VisualActor->IsHidden()
        || !CandidateBody->IsVisible()
        || !CandidateHead->IsVisible()
        || CandidateBody->bHiddenInGame
        || CandidateHead->bHiddenInGame
        || CandidateBody->bOwnerNoSee
        || CandidateHead->bOwnerNoSee
        || CandidateBody->bOnlyOwnerSee
        || CandidateHead->bOnlyOwnerSee
        || !CandidateBody->bRenderInMainPass
        || !CandidateHead->bRenderInMainPass
        || CandidateBody->bVisibleInSceneCaptureOnly
        || CandidateHead->bVisibleInSceneCaptureOnly
        || !CandidateBody->ShouldRender()
        || !CandidateHead->ShouldRender()
        || BodyLODCount <= 0
        || HeadLODCount <= 0
        || CandidateBody->GetNumMaterials() <= 0
        || CandidateHead->GetNumMaterials() <= 0
        || !CandidateOutfit->IsVisible()
        || CandidateOutfit->bHiddenInGame
        || CandidateOutfit->bOwnerNoSee
        || CandidateOutfit->bOnlyOwnerSee
        || !CandidateOutfit->bRenderInMainPass
        || CandidateOutfit->bVisibleInSceneCaptureOnly
        || !CandidateOutfit->ShouldRender()
        || CandidateOutfit->GetNumLODs() <= 0
        || CandidateOutfit->GetNumMaterials() <= 0
        || !bOutfitPoseFinite
        || !bOutfitAnchorsMatch
        || !bOutfitBoundsPlausible
        || !bBodyPoseFinite
        || !bBodyBoundsPlausible
        || !bHeadBoundsPlausible)
    {
        const FString LiveGateFailure = FString::Printf(
            TEXT("Assembled visual live gate failed: actor_hidden=%d ")
            TEXT("body_visible=%d body_hidden=%d body_owner_no_see=%d ")
            TEXT("body_only_owner_see=%d body_main_pass=%d body_capture_only=%d ")
            TEXT("body_should_render=%d body_lods=%d body_materials=%d ")
            TEXT("body_bones=%d body_pose_transforms=%d body_pose_finite=%d ")
            TEXT("body_bounds_ok=%d body_bounds_origin=%s body_bounds_extent=%s ")
            TEXT("body_bounds_radius=%.3f body_pose_outliers=%s ")
            TEXT("body_pose_anchors=%s source_pose_outliers=%s ")
            TEXT("source_pose_anchors=%s ")
            TEXT("head_visible=%d head_hidden=%d ")
            TEXT("head_owner_no_see=%d head_only_owner_see=%d head_main_pass=%d ")
            TEXT("head_capture_only=%d head_should_render=%d head_lods=%d ")
            TEXT("head_materials=%d head_bounds_ok=%d head_bounds_origin=%s ")
            TEXT("head_bounds_extent=%s head_bounds_radius=%.3f ")
            TEXT("outfit_visible=%d outfit_hidden=%d outfit_should_render=%d ")
            TEXT("outfit_lods=%d outfit_materials=%d outfit_pose=%d ")
            TEXT("outfit_bounds_ok=%d outfit_anchors=%d outfit_anchor_max_cm=%.3f ")
            TEXT("outfit_leader=%s outfit_postprocess=%s ")
            TEXT("outfit_bounds_origin=%s outfit_bounds_extent=%s ")
            TEXT("outfit_bounds_radius=%.3f source_bounds_origin=%s."),
            VisualActor->IsHidden() ? 1 : 0,
            CandidateBody->IsVisible() ? 1 : 0,
            CandidateBody->bHiddenInGame ? 1 : 0,
            CandidateBody->bOwnerNoSee ? 1 : 0,
            CandidateBody->bOnlyOwnerSee ? 1 : 0,
            CandidateBody->bRenderInMainPass ? 1 : 0,
            CandidateBody->bVisibleInSceneCaptureOnly ? 1 : 0,
            CandidateBody->ShouldRender() ? 1 : 0,
            BodyLODCount,
            CandidateBody->GetNumMaterials(),
            CandidateBody->GetNumBones(),
            CandidateBody->GetNumComponentSpaceTransforms(),
            bBodyPoseFinite ? 1 : 0,
            bBodyBoundsPlausible ? 1 : 0,
            *CandidateBody->Bounds.Origin.ToCompactString(),
            *CandidateBody->Bounds.BoxExtent.ToCompactString(),
            CandidateBody->Bounds.SphereRadius,
            *BodyPoseOutliers,
            *BodyPoseAnchors,
            *SourcePoseOutliers,
            *SourcePoseAnchors,
            CandidateHead->IsVisible() ? 1 : 0,
            CandidateHead->bHiddenInGame ? 1 : 0,
            CandidateHead->bOwnerNoSee ? 1 : 0,
            CandidateHead->bOnlyOwnerSee ? 1 : 0,
            CandidateHead->bRenderInMainPass ? 1 : 0,
            CandidateHead->bVisibleInSceneCaptureOnly ? 1 : 0,
            CandidateHead->ShouldRender() ? 1 : 0,
            HeadLODCount,
            CandidateHead->GetNumMaterials(),
            bHeadBoundsPlausible ? 1 : 0,
            *CandidateHead->Bounds.Origin.ToCompactString(),
            *CandidateHead->Bounds.BoxExtent.ToCompactString(),
            CandidateHead->Bounds.SphereRadius,
            CandidateOutfit->IsVisible() ? 1 : 0,
            CandidateOutfit->bHiddenInGame ? 1 : 0,
            CandidateOutfit->ShouldRender() ? 1 : 0,
            CandidateOutfit->GetNumLODs(),
            CandidateOutfit->GetNumMaterials(),
            bOutfitPoseFinite ? 1 : 0,
            bOutfitBoundsPlausible ? 1 : 0,
            OutfitAnchorCount,
            OutfitMaximumAnchorDistanceCm,
            *GetNameSafe(CandidateOutfit->LeaderPoseComponent.Get()),
            *GetNameSafe(CandidateOutfit->GetPostProcessInstance()),
            *CandidateOutfit->Bounds.Origin.ToCompactString(),
            *CandidateOutfit->Bounds.BoxExtent.ToCompactString(),
            CandidateOutfit->Bounds.SphereRadius,
            *AnimationSourceMesh->Bounds.Origin.ToCompactString());
        return RejectCandidate(LiveGateFailure);
    }

    FString TargetPolicyStatus;
    bool bTargetPolicyMutationAttempted = false;
    const bool bTargetPolicyApplied =
        ApplyPresentationPolicyTransactionally(
            VisualActor,
            AnimationSourceMesh,
            CandidateProfile,
            RequestedPresentationPolicy,
            CandidateHelper,
            CandidateBody,
            CandidateOutfit,
            true,
            TargetPolicyStatus,
            &bTargetPolicyMutationAttempted);
    bActivePolicyMutationOccurred =
        bActivePolicyMutationOccurred
        || (PreviousActiveActor == VisualActor
            && bTargetPolicyMutationAttempted);
    if (!bTargetPolicyApplied)
    {
        return RejectCandidate(FString::Printf(
            TEXT("Candidate could not enter requested %s policy: %s"),
            Session8PresentationPolicyName(RequestedPresentationPolicy),
            *TargetPolicyStatus),
            true);
    }

    VerifiedVisualActor = VisualActor;
    VerifiedVisualBody = CandidateBody;
    VerifiedVisualHead = CandidateHead;
    VerifiedVisualOutfit = CandidateOutfit;
    OutfitRequiredBonesHelper = CandidateHelper;
    VerifiedPresentationPolicy = RequestedPresentationPolicy;
    PendingPresentationPolicyActor = VisualActor;
    PendingPresentationPolicyStatus = TargetPolicyStatus;
    bPendingPresentationPolicyTransitionSuccess =
        !bHadPreviousActiveVisual
        || PreviousVerifiedPolicy != RequestedPresentationPolicy;
    if (PreviousHelper != CandidateHelper)
    {
        DestroyOutfitRequiredBonesHelper(PreviousHelper);
    }
    LastAdapterStatus = FString::Printf(
        TEXT("Assembled visual body/head, live pose/bounds, Outfit required bones, DG retarget, and %s policy verified. %s %s"),
        Session8PresentationPolicyName(VerifiedPresentationPolicy),
        *RequiredBonesStatus,
        *TargetPolicyStatus);
    UE_LOG(LogDiscGolfTour, Display, TEXT("%s"), *LastAdapterStatus);
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::
    InstallOutfitRequiredBonesHelper(
        AActor* VisualActor,
        USkeletalMeshComponent* Body,
        USkeletalMeshComponent* Outfit,
        UDiscGolfMetaHumanOutfitRequiredBonesComponent*& OutHelper,
        FString& OutStatus)
{
    OutHelper = nullptr;
    OutStatus.Reset();
    if (!IsValid(VisualActor) || !IsValid(Body) || !IsValid(Outfit)
        || VisualActor->GetWorld() != Body->GetWorld()
        || Body->GetWorld() != Outfit->GetWorld()
        || Body->GetOwner() != VisualActor
        || Outfit->GetOwner() != VisualActor)
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones helper owner/world contract was invalid.");
        return false;
    }

    USkeletalMesh* const BodyAsset = Body->GetSkeletalMeshAsset();
    FSkeletalMeshRenderData* const BodyRenderData = BodyAsset
        ? BodyAsset->GetResourceForRendering() : nullptr;
    const FSkeletalMeshObject* const BodyMeshObject = Body->GetMeshObject();
    const int32 BodyActualLOD = BodyMeshObject
        ? BodyMeshObject->GetLOD() : INDEX_NONE;
    const int32 BodyPredictedLOD = Body->GetPredictedLODLevel();
    const bool bActualLODKnown = BodyActualLOD != INDEX_NONE;
    if (!BodyRenderData
        || !BodyRenderData->LODRenderData.IsValidIndex(BodyPredictedLOD)
        || !BodyRenderData->LODRenderData[BodyPredictedLOD].IsDataReady()
        || (bActualLODKnown
            && (!BodyRenderData->LODRenderData.IsValidIndex(BodyActualLOD)
                || !BodyRenderData->LODRenderData[BodyActualLOD].IsDataReady())))
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Body required-bones rebuild LODs were invalid: actual=%d predicted=%d."),
            BodyActualLOD,
            BodyPredictedLOD);
        return false;
    }

    UDiscGolfMetaHumanOutfitRequiredBonesComponent* const Helper =
        NewObject<UDiscGolfMetaHumanOutfitRequiredBonesComponent>(
            VisualActor,
            MakeUniqueObjectName(
                VisualActor,
                UDiscGolfMetaHumanOutfitRequiredBonesComponent::StaticClass(),
                FName(TEXT("DG_MetaHumanOutfitRequiredBones"))),
            RF_Transient);
    if (!Helper)
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones helper could not be allocated.");
        return false;
    }

    FString ContractStatus;
    if (!Helper->ConfigureBoneContract(Outfit, Body, ContractStatus))
    {
        DestroyOutfitRequiredBonesHelper(Helper);
        OutStatus = ContractStatus;
        return false;
    }

    FSession8OutfitRequiredBonesCoverageAudit BaselineAudit;
    FString CoverageAuditStatus;
    if (!Session8AuditOutfitRequiredBonesCoverage(
            Body, Helper, BaselineAudit, CoverageAuditStatus))
    {
        DestroyOutfitRequiredBonesHelper(Helper);
        OutStatus = CoverageAuditStatus;
        return false;
    }
    if (BaselineAudit.MissingUsedRequiredCount == 0
        && BaselineAudit.MissingUsedFillCount == 0)
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit required-bones helper rejected as a proven all-LOD no-op: body_current_lod=%d/%d body_ready_lods=%d[%s] outfit_initial_lod=%d/%d outfit_ready_lods=%d all_outfit_lod_used_bones=%d baseline_all_body_lod_required_used_missing=0 baseline_required_list=%s baseline_all_body_lod_fill_used_missing=0 baseline_fill_list=%s."),
            BaselineAudit.BodyActualLOD,
            BaselineAudit.BodyPredictedLOD,
            BaselineAudit.ReadyBodyLODCount,
            *BaselineAudit.ReadyBodyLODs,
            Helper->GetConfiguredActualOutfitLOD(),
            Helper->GetConfiguredPredictedOutfitLOD(),
            Helper->GetConfiguredReadyOutfitLODCount(),
            Helper->GetMappedOutfitUsedLeaderBoneCount(),
            *BaselineAudit.MissingUsedRequired,
            *BaselineAudit.MissingUsedFill);
        DestroyOutfitRequiredBonesHelper(Helper);
        return false;
    }

    VisualActor->AddInstanceComponent(Helper);
    Helper->SetupAttachment(Body);
    Helper->RegisterComponent();
    Helper->SetLeaderPoseComponent(Body, true, false);

    const bool bRegisteredFollower =
        Body->GetFollowerPoseComponents().ContainsByPredicate(
            [Helper](const TWeakObjectPtr<USkinnedMeshComponent>& Follower)
            {
                return Follower.Get() == Helper;
            });
    if (!Helper->IsRegistered()
        || Helper->GetWorld() != Body->GetWorld()
        || Helper->LeaderPoseComponent.Get() != Body
        || !Helper->IsConfiguredFor(Outfit, Body)
        || Helper->GetSkinnedAsset() != nullptr
        || Helper->IsVisible()
        || !Helper->bHiddenInGame
        || Helper->ShouldRender()
        || Helper->IsComponentTickEnabled()
        || Helper->GetCollisionEnabled() != ECollisionEnabled::NoCollision
        || Helper->GetGenerateOverlapEvents()
        || !bRegisteredFollower
        || Helper->GetRequiredLeaderBoneCount() <= 0)
    {
        DestroyOutfitRequiredBonesHelper(Helper);
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones helper did not register as a non-rendering Body follower.");
        return false;
    }

    TArray<FBoneIndexType> ContributedBones;
    Helper->GetAdditionalRequiredBonesForLeader(
        BodyPredictedLOD, ContributedBones);
    if (ContributedBones.Num() != Helper->GetRequiredLeaderBoneCount()
        || ContributedBones.IsEmpty() || ContributedBones[0] != 0)
    {
        DestroyOutfitRequiredBonesHelper(Helper);
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones helper contribution was not exact and parent-complete.");
        return false;
    }

    Body->RecalcRequiredBones(BodyPredictedLOD);
    Body->TickAnimation(0.0f, false);
    Body->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
    Body->RefreshBoneTransforms(nullptr);
    Body->RefreshFollowerComponents();
    FSession8OutfitRequiredBonesCoverageAudit FinalAudit;
    if (!Session8AuditOutfitRequiredBonesCoverage(
            Body, Helper, FinalAudit, CoverageAuditStatus))
    {
        DestroyOutfitRequiredBonesHelper(Helper);
        OutStatus = CoverageAuditStatus;
        return false;
    }
    if (FinalAudit.MissingExpectedRequiredCount != 0
        || FinalAudit.MissingExpectedFillCount != 0
        || FinalAudit.MissingUsedRequiredCount != 0
        || FinalAudit.MissingUsedFillCount != 0
        || !Helper->IsConfiguredFor(Outfit, Body)
        || Body->GetNumComponentSpaceTransforms() != Body->GetNumBones())
    {
        const FSkeletalMeshObject* const CurrentOutfitMeshObject =
            Outfit->GetMeshObject();
        const int32 CurrentActualOutfitLOD = CurrentOutfitMeshObject
            ? CurrentOutfitMeshObject->GetLOD() : INDEX_NONE;
        OutStatus = FString::Printf(
            TEXT("MetaHuman Body Outfit all-LOD required-bones refresh failed: body_current_lod=%d/%d body_ready_lods=%d[%s] outfit_initial_lod=%d/%d outfit_current_lod=%d/%d outfit_ready_lods=%d baseline_required_used_missing=%d baseline_required_used_list=%s baseline_fill_used_missing=%d baseline_fill_used_list=%s final_required_expected_missing=%d final_required_expected_list=%s final_fill_expected_missing=%d final_fill_expected_list=%s final_required_all_outfit_lod_used_missing=%d final_required_used_list=%s final_fill_all_outfit_lod_used_missing=%d final_fill_used_list=%s."),
            FinalAudit.BodyActualLOD,
            FinalAudit.BodyPredictedLOD,
            FinalAudit.ReadyBodyLODCount,
            *FinalAudit.ReadyBodyLODs,
            Helper->GetConfiguredActualOutfitLOD(),
            Helper->GetConfiguredPredictedOutfitLOD(),
            CurrentActualOutfitLOD,
            Outfit->GetPredictedLODLevel(),
            Helper->GetConfiguredReadyOutfitLODCount(),
            BaselineAudit.MissingUsedRequiredCount,
            *BaselineAudit.MissingUsedRequired,
            BaselineAudit.MissingUsedFillCount,
            *BaselineAudit.MissingUsedFill,
            FinalAudit.MissingExpectedRequiredCount,
            *FinalAudit.MissingExpectedRequired,
            FinalAudit.MissingExpectedFillCount,
            *FinalAudit.MissingExpectedFill,
            FinalAudit.MissingUsedRequiredCount,
            *FinalAudit.MissingUsedRequired,
            FinalAudit.MissingUsedFillCount,
            *FinalAudit.MissingUsedFill);
        DestroyOutfitRequiredBonesHelper(Helper);
        return false;
    }

    OutStatus = FString::Printf(
        TEXT("MetaHuman Outfit required-bones helper installed: body_current_lod=%d/%d body_ready_lods=%d[%s] outfit_initial_lod=%d/%d outfit_ready_lods=%d all_outfit_lod_bones=%d all_outfit_lod_used_bones=%d parent_complete_body_bones=%d baseline_all_body_lod_required_used_missing=%d baseline_required_used_list=%s baseline_all_body_lod_fill_used_missing=%d baseline_fill_used_list=%s final_all_body_lod_required_expected_missing=0 final_required_expected_list=%s final_all_body_lod_fill_expected_missing=0 final_fill_expected_list=%s final_all_body_lod_required_used_missing=0 final_required_used_list=%s final_all_body_lod_fill_used_missing=0 final_fill_used_list=%s."),
        FinalAudit.BodyActualLOD,
        FinalAudit.BodyPredictedLOD,
        FinalAudit.ReadyBodyLODCount,
        *FinalAudit.ReadyBodyLODs,
        Helper->GetConfiguredActualOutfitLOD(),
        Helper->GetConfiguredPredictedOutfitLOD(),
        Helper->GetConfiguredReadyOutfitLODCount(),
        Helper->GetConfiguredOutfitBoneCount(),
        Helper->GetMappedOutfitUsedLeaderBoneCount(),
        Helper->GetRequiredLeaderBoneCount(),
        BaselineAudit.MissingUsedRequiredCount,
        *BaselineAudit.MissingUsedRequired,
        BaselineAudit.MissingUsedFillCount,
        *BaselineAudit.MissingUsedFill,
        *FinalAudit.MissingExpectedRequired,
        *FinalAudit.MissingExpectedFill,
        *FinalAudit.MissingUsedRequired,
        *FinalAudit.MissingUsedFill);
    UE_LOG(LogDiscGolfTour, Display, TEXT("%s"), *OutStatus);
    OutHelper = Helper;
    return true;
}

bool UDiscGolfMetaHumanAvatarBackendComponent::
    RefreshAndVerifyOutfitRequiredBonesHelper(
        UDiscGolfMetaHumanOutfitRequiredBonesComponent* Helper,
        USkeletalMeshComponent* Body,
        USkeletalMeshComponent* Outfit,
        FString& OutStatus)
{
    OutStatus.Reset();
    if (!IsInGameThread() || !IsValid(Helper) || !IsValid(Body)
        || !IsValid(Outfit) || !Helper->IsRegistered()
        || !Body->IsRegistered() || !Outfit->IsRegistered()
        || Helper->GetOwner() != Body->GetOwner()
        || Outfit->GetOwner() != Body->GetOwner()
        || Helper->GetWorld() != Body->GetWorld()
        || Outfit->GetWorld() != Body->GetWorld()
        || Helper->GetAttachParent() != Body)
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones helper could not be refreshed because its owner, world, registration, or Body attachment was invalid.");
        return false;
    }

    if (!Helper->IsConfiguredFor(Outfit, Body))
    {
        FString ContractStatus;
        if (!Helper->ConfigureBoneContract(Outfit, Body, ContractStatus))
        {
            OutStatus = ContractStatus.IsEmpty()
                ? TEXT("MetaHuman Outfit all-LOD required-bones contract could not be refreshed.")
                : ContractStatus;
            return false;
        }
    }

    Helper->SetLeaderPoseComponent(Body, true, false);
    const bool bRegisteredFollower =
        Body->GetFollowerPoseComponents().ContainsByPredicate(
            [Helper](const TWeakObjectPtr<USkinnedMeshComponent>& Follower)
            {
                return Follower.Get() == Helper;
            });
    if (Helper->LeaderPoseComponent.Get() != Body
        || Helper->GetSkinnedAsset() != nullptr
        || Helper->IsVisible() || !Helper->bHiddenInGame
        || Helper->ShouldRender() || Helper->IsComponentTickEnabled()
        || Helper->GetCollisionEnabled() != ECollisionEnabled::NoCollision
        || Helper->GetGenerateOverlapEvents() || !bRegisteredFollower
        || Helper->GetConfiguredReadyOutfitLODCount() <= 0
        || Helper->GetRequiredLeaderBoneCount() <= 0
        || Helper->GetMappedOutfitUsedLeaderBoneCount() <= 0)
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones helper no longer satisfied its non-rendering Body follower contract.");
        return false;
    }

    TArray<FBoneIndexType> ContributedBones;
    Helper->GetAdditionalRequiredBonesForLeader(
        Body->GetPredictedLODLevel(), ContributedBones);
    int32 MissingContributedBoneCount = 0;
    FString MissingContributedBoneList;
    if (ContributedBones.Num() != Helper->GetRequiredLeaderBoneCount()
        || ContributedBones.IsEmpty() || ContributedBones[0] != 0
        || !Helper->DescribeMissingRequiredLeaderBones(
            ContributedBones,
            MissingContributedBoneCount,
            MissingContributedBoneList)
        || MissingContributedBoneCount != 0)
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit required-bones helper contribution was no longer exact and parent-complete: missing=%d list=%s."),
            MissingContributedBoneCount,
            *MissingContributedBoneList);
        return false;
    }

    USkeletalMesh* const BodyAsset = Body->GetSkeletalMeshAsset();
    FSkeletalMeshRenderData* const BodyRenderData = BodyAsset
        ? BodyAsset->GetResourceForRendering() : nullptr;
    const FSkeletalMeshObject* const BodyMeshObject = Body->GetMeshObject();
    const int32 BodyActualLOD = BodyMeshObject
        ? BodyMeshObject->GetLOD() : INDEX_NONE;
    const int32 BodyPredictedLOD = Body->GetPredictedLODLevel();
    const bool bActualLODKnown = BodyActualLOD != INDEX_NONE;
    if (!BodyRenderData
        || !BodyRenderData->LODRenderData.IsValidIndex(BodyPredictedLOD)
        || !BodyRenderData->LODRenderData[BodyPredictedLOD].IsDataReady()
        || (bActualLODKnown
            && (!BodyRenderData->LODRenderData.IsValidIndex(BodyActualLOD)
                || !BodyRenderData->LODRenderData[BodyActualLOD].IsDataReady())))
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Body required-bones restore LODs were invalid: actual=%d predicted=%d."),
            BodyActualLOD,
            BodyPredictedLOD);
        return false;
    }

    Body->RecalcRequiredBones(BodyPredictedLOD);
    Body->TickAnimation(0.0f, false);
    Body->TickComponent(0.0f, ELevelTick::LEVELTICK_All, nullptr);
    Body->RefreshBoneTransforms(nullptr);
    Body->RefreshFollowerComponents();

    FSession8OutfitRequiredBonesCoverageAudit FinalAudit;
    FString CoverageAuditStatus;
    if (!Session8AuditOutfitRequiredBonesCoverage(
            Body, Helper, FinalAudit, CoverageAuditStatus))
    {
        OutStatus = CoverageAuditStatus;
        return false;
    }
    if (FinalAudit.MissingExpectedRequiredCount != 0
        || FinalAudit.MissingExpectedFillCount != 0
        || FinalAudit.MissingUsedRequiredCount != 0
        || FinalAudit.MissingUsedFillCount != 0
        || !Helper->IsConfiguredFor(Outfit, Body)
        || Body->GetNumComponentSpaceTransforms() != Body->GetNumBones())
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit required-bones helper all-LOD refresh postcondition failed: body_current_lod=%d/%d body_ready_lods=%d[%s] outfit_initial_lod=%d/%d outfit_current_lod=%d/%d outfit_ready_lods=%d final_required_expected_missing=%d final_required_expected_list=%s final_fill_expected_missing=%d final_fill_expected_list=%s final_required_all_outfit_lod_used_missing=%d final_required_used_list=%s final_fill_all_outfit_lod_used_missing=%d final_fill_used_list=%s."),
            FinalAudit.BodyActualLOD,
            FinalAudit.BodyPredictedLOD,
            FinalAudit.ReadyBodyLODCount,
            *FinalAudit.ReadyBodyLODs,
            Helper->GetConfiguredActualOutfitLOD(),
            Helper->GetConfiguredPredictedOutfitLOD(),
            Outfit->GetMeshObject()
                ? Outfit->GetMeshObject()->GetLOD() : INDEX_NONE,
            Outfit->GetPredictedLODLevel(),
            Helper->GetConfiguredReadyOutfitLODCount(),
            FinalAudit.MissingExpectedRequiredCount,
            *FinalAudit.MissingExpectedRequired,
            FinalAudit.MissingExpectedFillCount,
            *FinalAudit.MissingExpectedFill,
            FinalAudit.MissingUsedRequiredCount,
            *FinalAudit.MissingUsedRequired,
            FinalAudit.MissingUsedFillCount,
            *FinalAudit.MissingUsedFill);
        return false;
    }

    OutStatus = FString::Printf(
        TEXT("MetaHuman Outfit required-bones helper refreshed: body_current_lod=%d/%d body_ready_lods=%d[%s] outfit_initial_lod=%d/%d outfit_current_lod=%d/%d outfit_ready_lods=%d all_outfit_lod_used_bones=%d parent_complete_body_bones=%d final_all_body_lod_required_expected_missing=0 final_all_body_lod_fill_expected_missing=0 final_all_body_lod_required_used_missing=0 final_all_body_lod_fill_used_missing=0."),
        FinalAudit.BodyActualLOD,
        FinalAudit.BodyPredictedLOD,
        FinalAudit.ReadyBodyLODCount,
        *FinalAudit.ReadyBodyLODs,
        Helper->GetConfiguredActualOutfitLOD(),
        Helper->GetConfiguredPredictedOutfitLOD(),
        Outfit->GetMeshObject()
            ? Outfit->GetMeshObject()->GetLOD() : INDEX_NONE,
        Outfit->GetPredictedLODLevel(),
        Helper->GetConfiguredReadyOutfitLODCount(),
        Helper->GetMappedOutfitUsedLeaderBoneCount(),
        Helper->GetRequiredLeaderBoneCount());
    return true;
}

void UDiscGolfMetaHumanAvatarBackendComponent::
    DestroyOutfitRequiredBonesHelper(
        UDiscGolfMetaHumanOutfitRequiredBonesComponent* Helper)
{
    if (!IsValid(Helper))
    {
        return;
    }
    if (Helper->LeaderPoseComponent.IsValid())
    {
        Helper->SetLeaderPoseComponent(nullptr, true, false);
    }
    Helper->DestroyComponent();
}

void UDiscGolfMetaHumanAvatarBackendComponent::
    ResetOutfitRequiredBonesHelper()
{
    UDiscGolfMetaHumanOutfitRequiredBonesComponent* const Helper =
        OutfitRequiredBonesHelper.Get();
    OutfitRequiredBonesHelper = nullptr;
    DestroyOutfitRequiredBonesHelper(Helper);
}

bool UDiscGolfMetaHumanAvatarBackendComponent::ApplyVisualCustomization_Implementation(
    AActor* VisualActor,
    const FDGFullCharacterCustomization& Customization)
{
    UDiscGolfAvatarBackendProfile* ActiveProfile = GetActiveBackendProfile();
    if (!IsValid(VisualActor)
        || VisualActor != VerifiedVisualActor
        || !IsValid(ActiveProfile)
        || !VisualActor->GetClass()->ImplementsInterface(
            UDiscGolfMetaHumanVisualContract::StaticClass()))
    {
        LastAdapterStatus = TEXT("No verified assembled visual is active.");
        return false;
    }

    FSession8PresentationPolicySnapshot PolicySnapshot;
    FString PolicySnapshotStatus;
    if (!Session8CapturePresentationPolicySnapshot(
            VisualActor, PolicySnapshot, PolicySnapshotStatus))
    {
        LastAdapterStatus = PolicySnapshotStatus;
        return false;
    }
    const EDGMetaHumanPresentationPolicy PreviousRequestedPolicy =
        RequestedPresentationPolicy;
    const EDGMetaHumanPresentationPolicy PreviousVerifiedPolicy =
        VerifiedPresentationPolicy;
    const FString PreviousPresentationPolicyStatus =
        LastPresentationPolicyStatus;
    auto RollBackApply = [this,
                          &PolicySnapshot,
                          PreviousRequestedPolicy,
                          PreviousVerifiedPolicy,
                          PreviousPresentationPolicyStatus](
        const FString& Failure)
    {
        if (!IsVisualBackendReady()
            || GetActiveVisualActor() != PolicySnapshot.VisualActor.Get())
        {
            RequestedPresentationPolicy = PreviousRequestedPolicy;
            VerifiedPresentationPolicy =
                EDGMetaHumanPresentationPolicy::Unconfigured;
            LastAdapterStatus = FString::Printf(
                TEXT("%s Apply rollback could not retain the active visual; backend remains fail-closed."),
                *Failure);
            return false;
        }
        FString PolicyRestoreStatus;
        bool bRestored = Session8RestorePresentationPolicySnapshot(
            PolicySnapshot, PolicyRestoreStatus);
        FString HelperRestoreStatus;
        if (bRestored && IsValid(OutfitRequiredBonesHelper)
            && IsValid(VerifiedVisualBody)
            && IsValid(VerifiedVisualOutfit))
        {
            bRestored = RefreshAndVerifyOutfitRequiredBonesHelper(
                OutfitRequiredBonesHelper,
                VerifiedVisualBody,
                VerifiedVisualOutfit,
                HelperRestoreStatus);
        }
        RequestedPresentationPolicy = PreviousRequestedPolicy;
        VerifiedPresentationPolicy = PreviousVerifiedPolicy;
        LastPresentationPolicyStatus = PreviousPresentationPolicyStatus;
        LastAdapterStatus = FString::Printf(
            TEXT("%s Apply rollback %s: %s %s"),
            *Failure,
            bRestored ? TEXT("succeeded") : TEXT("failed"),
            *PolicyRestoreStatus,
            *HelperRestoreStatus);
        return false;
    };

    FString ScaleIsolationStatus;
    if (!Session8IsolateFixedPresetScale(
            VisualActor,
            GetActiveAnimationSourceMesh(),
            ScaleIsolationStatus))
    {
        return RollBackApply(ScaleIsolationStatus);
    }

    FString ContractStatus;
    if (!IDiscGolfMetaHumanVisualContract::Execute_ApplyMappedCustomization(
            VisualActor,
            ActiveProfile,
            Customization,
            ContractStatus))
    {
        const FString Failure = ContractStatus.IsEmpty()
            ? TEXT("Assembled visual rejected mapped customization.")
            : ContractStatus;
        return RollBackApply(Failure);
    }
    if (!Session8IsolateFixedPresetScale(
            VisualActor,
            GetActiveAnimationSourceMesh(),
            ScaleIsolationStatus))
    {
        return RollBackApply(ScaleIsolationStatus);
    }

    USkeletalMeshComponent* CandidateBody = nullptr;
    USkeletalMeshComponent* CandidateHead = nullptr;
    FString ValidationStatus;
    if (!ValidateConfiguredVisual(
            VisualActor,
            GetActiveAnimationSourceMesh(),
            ActiveProfile,
            true,
            CandidateBody,
            CandidateHead,
            ValidationStatus))
    {
        return RollBackApply(ValidationStatus);
    }

    if (BackendProfile.Get() != ActiveProfile
        || !FinalizeVisualBackendActivation(
            VisualActor,
            GetActiveAnimationSourceMesh(),
            Customization))
    {
        const FString Failure = LastAdapterStatus.IsEmpty()
            ? TEXT("Fixed assembled preset did not retain a live renderable presentation after apply.")
            : LastAdapterStatus;
        return RollBackApply(Failure);
    }
    LastAdapterStatus = TEXT(
        "Fixed clean-shaven assembled preset verified; DG proxy-only values "
        "were preserved but not applied to this visual.");
    return true;
}

void UDiscGolfMetaHumanAvatarBackendComponent::OnUnregister()
{
    OnAvatarBackendReady.RemoveDynamic(
        this,
        &UDiscGolfMetaHumanAvatarBackendComponent::HandleAvatarBackendReady);
    ResetOutfitRequiredBonesHelper();
    Super::OnUnregister();
    VerifiedVisualActor = nullptr;
    VerifiedVisualBody = nullptr;
    VerifiedVisualHead = nullptr;
    VerifiedVisualOutfit = nullptr;
    RequestedPresentationPolicy =
        EDGMetaHumanPresentationPolicy::GameplayPerformance;
    VerifiedPresentationPolicy =
        EDGMetaHumanPresentationPolicy::Unconfigured;
    LastPresentationPolicyStatus =
        TEXT("GameplayPerformance policy is queued for the next verified visual.");
    PendingPresentationPolicyActor = nullptr;
    PendingPresentationPolicyStatus.Reset();
    bPendingPresentationPolicyTransitionSuccess = false;
    PresentationPolicyTransitionSuccessCount = 0;
    PresentationPolicyTransitionFailureCount = 0;
    PresentationPolicyRollbackSuccessCount = 0;
    PresentationPolicyRollbackFailureCount = 0;
    LastAdapterStatus = TEXT("DG master proxy fallback active.");
}

bool UDiscGolfMetaHumanAvatarBackendComponent::ValidateConfiguredVisual(
    AActor* VisualActor,
    USkeletalMeshComponent* AnimationSourceMesh,
    const UDiscGolfAvatarBackendProfile* ExpectedProfile,
    bool bRequireVerifiedRetarget,
    USkeletalMeshComponent*& OutBody,
    USkeletalMeshComponent*& OutHead,
    FString& OutStatus) const
{
    OutBody = nullptr;
    OutHead = nullptr;
    OutStatus.Reset();
    if (!IsRegistered() || !IsValid(VisualActor) || !IsValid(AnimationSourceMesh)
        || !IsValid(ExpectedProfile) || !AnimationSourceMesh->IsRegistered()
        || !AnimationSourceMesh->GetSkeletalMeshAsset()
        || GetWorld() != AnimationSourceMesh->GetWorld())
    {
        OutStatus = TEXT("DG animation source is not usable.");
        return false;
    }
    const AActor* Owner = GetOwner();
    const USceneComponent* VisualRoot = VisualActor->GetRootComponent();
    if (!IsValid(Owner) || VisualActor->GetOwner() != Owner
        || VisualActor->GetWorld() != AnimationSourceMesh->GetWorld()
        || !VisualRoot || !VisualRoot->IsAttachedTo(AnimationSourceMesh))
    {
        OutStatus = TEXT("Assembled visual owner/world/source attachment is invalid.");
        return false;
    }
    if (!VisualRoot->GetRelativeTransform().Equals(
            FTransform::Identity, 0.01f))
    {
        OutStatus = TEXT(
            "Assembled visual root must retain the identity transform relative to the DG animation source.");
        return false;
    }
    if (!VisualRoot->IsUsingAbsoluteScale()
        || !VisualRoot->GetComponentScale().Equals(
            FVector::OneVector, 0.001f))
    {
        OutStatus = TEXT(
            "Fixed MetaHuman root must isolate parent height scale and retain unit world scale.");
        return false;
    }
    if (VisualActor->IsA<APawn>() || VisualActor->IsA<AController>()
        || VisualActor->FindComponentByClass<UMovementComponent>())
    {
        OutStatus = TEXT("Assembled visual must be a presentation-only actor.");
        return false;
    }

    TInlineComponentArray<UMetaHumanComponentUE*> MetaHumanComponents;
    VisualActor->GetComponents(MetaHumanComponents);
    if (MetaHumanComponents.Num() != 1
        || !IsValid(MetaHumanComponents[0])
        || !MetaHumanComponents[0]->IsRegistered())
    {
        OutStatus = TEXT("Assembled visual must expose exactly one MetaHuman component.");
        return false;
    }

    bool bDuplicateLODSync = false;
    ULODSyncComponent* const LODSync =
        Session8FindUniqueLODSync(VisualActor, bDuplicateLODSync);
    if (bDuplicateLODSync || !IsValid(LODSync) || !LODSync->IsRegistered())
    {
        OutStatus = TEXT(
            "Assembled visual must expose exactly one registered LOD sync component.");
        return false;
    }

    bool bDuplicateHair = false;
    bool bDuplicateBeard = false;
    bool bDuplicateMustache = false;
    UGroomComponent* const Hair = Session8FindUniqueNamedGroom(
        VisualActor, FName(TEXT("Hair")), bDuplicateHair);
    UGroomComponent* const Beard = Session8FindUniqueNamedGroom(
        VisualActor, FName(TEXT("Beard")), bDuplicateBeard);
    UGroomComponent* const Mustache = Session8FindUniqueNamedGroom(
        VisualActor, FName(TEXT("Mustache")), bDuplicateMustache);
    if (bDuplicateHair || bDuplicateBeard || bDuplicateMustache
        || !IsValid(Hair) || !Hair->IsRegistered() || !Hair->GroomAsset
        || Hair->GroomAsset->GetPathName() != Session8HairGroomAssetPath
        || !IsValid(Beard) || !Beard->IsRegistered() || Beard->GroomAsset
        || !IsValid(Mustache) || !Mustache->IsRegistered()
        || Mustache->GroomAsset)
    {
        OutStatus = TEXT(
            "Fixed assembled visual must expose Hair_S_Clean and empty Beard/Mustache grooms.");
        return false;
    }

    bool bDuplicateBody = false;
    bool bDuplicateHead = false;
    OutBody = Session8FindUniqueTaggedSkeletalMesh(
        VisualActor, ExpectedProfile->VisualBodyComponentTag, bDuplicateBody);
    OutHead = Session8FindUniqueTaggedSkeletalMesh(
        VisualActor, ExpectedProfile->VisualHeadComponentTag, bDuplicateHead);
    if (bDuplicateBody || bDuplicateHead || !OutBody || !OutHead
        || OutBody == OutHead)
    {
        OutStatus = TEXT("Assembled visual must expose one distinct tagged body and head.");
        return false;
    }
    if (!OutBody->IsRegistered() || !OutHead->IsRegistered()
        || !OutBody->GetSkeletalMeshAsset() || !OutHead->GetSkeletalMeshAsset())
    {
        OutStatus = TEXT("Tagged assembled body/head skeletal meshes are not ready.");
        return false;
    }
    bool bDuplicateOutfit = false;
    USkeletalMeshComponent* const Outfit =
        Session8FindUniqueOutfitMesh(VisualActor, bDuplicateOutfit);
    if (bDuplicateOutfit || !IsValid(Outfit) || !Outfit->IsRegistered())
    {
        OutStatus = TEXT(
            "Assembled visual must expose exactly one generated outfit mesh.");
        return false;
    }
    if (!OutBody->GetComponentScale().Equals(FVector::OneVector, 0.001f)
        || !OutHead->GetComponentScale().Equals(FVector::OneVector, 0.001f)
        || !Outfit->GetComponentScale().Equals(FVector::OneVector, 0.001f))
    {
        OutStatus = FString::Printf(
            TEXT("Fixed MetaHuman presentation scale drifted: body=%s head=%s outfit=%s."),
            *OutBody->GetComponentScale().ToCompactString(),
            *OutHead->GetComponentScale().ToCompactString(),
            *Outfit->GetComponentScale().ToCompactString());
        return false;
    }
    UIKRetargeter* Retargeter = Cast<UIKRetargeter>(
        ExpectedProfile->RetargetAsset.Get());
    if (bRequireVerifiedRetarget)
    {
        UDiscGolfMetaHumanRetargetAnimInstance* RetargetInstance =
            Cast<UDiscGolfMetaHumanRetargetAnimInstance>(
                OutBody->GetAnimInstance());
        if (!Retargeter || !RetargetInstance
            || OutBody->GetAnimClass()
                != UDiscGolfMetaHumanRetargetAnimInstance::StaticClass()
            || !RetargetInstance->IsConfiguredFor(
                Retargeter, AnimationSourceMesh))
        {
            OutStatus = TEXT("Tagged assembled body retarget processor is not verified.");
            return false;
        }
    }

    Session8EnforceTaggedMeshSafety(OutBody);
    Session8EnforceTaggedMeshSafety(OutHead);
    Session8EnforceTaggedMeshSafety(Outfit);
    return true;
}
