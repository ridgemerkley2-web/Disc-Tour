#include "DiscGolfMetaHumanOutfitRequiredBonesComponent.h"

#include "Algo/Unique.h"
#include "AnimationRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalRenderPublic.h"

UDiscGolfMetaHumanOutfitRequiredBonesComponent::
    UDiscGolfMetaHumanOutfitRequiredBonesComponent(
        const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // This component is only a required-bone contract participant. Leaving its
    // skinned asset unset guarantees that it cannot create a garment proxy.
    PrimaryComponentTick.bCanEverTick = false;
    bAutoActivate = false;
    SetComponentTickEnabled(false);
    SetVisibility(false, true);
    SetHiddenInGame(true, true);
    SetRenderInMainPass(false);
    SetVisibleInRayTracing(false);
    SetCastShadow(false);
    SetSimulatePhysics(false);
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
}

void UDiscGolfMetaHumanOutfitRequiredBonesComponent::ResetBoneContract()
{
    SourceOutfit = nullptr;
    TargetBody = nullptr;
    ConfiguredOutfitAsset = nullptr;
    ConfiguredBodyAsset = nullptr;
    RequiredLeaderBones.Reset();
    MappedOutfitUsedLeaderBones.Reset();
    ConfiguredActualOutfitLOD = INDEX_NONE;
    ConfiguredPredictedOutfitLOD = INDEX_NONE;
    ConfiguredOutfitBoneCount = 0;
    ConfiguredReadyOutfitLODCount = 0;
}

bool UDiscGolfMetaHumanOutfitRequiredBonesComponent::ConfigureBoneContract(
    USkeletalMeshComponent* Outfit,
    USkeletalMeshComponent* Body,
    FString& OutStatus)
{
    ResetBoneContract();
    OutStatus.Reset();
    if (!IsInGameThread())
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones contract must be configured on the game thread.");
        return false;
    }
    if (!IsValid(Outfit) || !IsValid(Body) || Outfit == Body
        || !Outfit->IsRegistered() || !Body->IsRegistered()
        || Outfit->GetWorld() != Body->GetWorld()
        || Outfit->LeaderPoseComponent.Get() != Body)
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones contract lacked its registered Body leader.");
        return false;
    }

    USkeletalMesh* const OutfitAsset = Outfit->GetSkeletalMeshAsset();
    USkeletalMesh* const BodyAsset = Body->GetSkeletalMeshAsset();
    FSkeletalMeshRenderData* const OutfitRenderData = OutfitAsset
        ? OutfitAsset->GetResourceForRendering() : nullptr;
    if (!OutfitAsset || !BodyAsset || !OutfitRenderData
        || OutfitRenderData->LODRenderData.IsEmpty())
    {
        OutStatus = TEXT(
            "MetaHuman Outfit required-bones contract lacked live render data.");
        return false;
    }

    const FSkeletalMeshObject* const OutfitMeshObject =
        Outfit->GetMeshObject();
    const int32 ActualLOD = OutfitMeshObject
        ? OutfitMeshObject->GetLOD() : INDEX_NONE;
    const int32 PredictedLOD = Outfit->GetPredictedLODLevel();
    const bool bActualLODKnown = ActualLOD != INDEX_NONE;
    if (!OutfitRenderData->LODRenderData.IsValidIndex(PredictedLOD)
        || (bActualLODKnown
            && !OutfitRenderData->LODRenderData.IsValidIndex(ActualLOD)))
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit required-bones LODs were invalid: actual=%d predicted=%d available=%d."),
            ActualLOD,
            PredictedLOD,
            OutfitRenderData->LODRenderData.Num());
        return false;
    }

    const FReferenceSkeleton& OutfitSkeleton =
        OutfitAsset->GetRefSkeleton();
    const FReferenceSkeleton& BodySkeleton = BodyAsset->GetRefSkeleton();
    const TArray<int32>& OutfitLeaderBoneMap = Outfit->GetLeaderBoneMap();
    if (OutfitSkeleton.GetNum() <= 0 || BodySkeleton.GetNum() <= 0
        || OutfitLeaderBoneMap.Num() != OutfitSkeleton.GetNum())
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit required-bones mapping was incomplete: outfit=%d map=%d body=%d."),
            OutfitSkeleton.GetNum(),
            OutfitLeaderBoneMap.Num(),
            BodySkeleton.GetNum());
        return false;
    }

    TSet<int32> OutfitRenderBones;
    TSet<int32> OutfitSectionBones;
    int32 ReadyLODCount = 0;
    bool bScannedActualLOD = false;
    bool bScannedPredictedLOD = false;
    for (int32 LODIndex = 0;
         LODIndex < OutfitRenderData->LODRenderData.Num();
         ++LODIndex)
    {
        const FSkeletalMeshLODRenderData& LOD =
            OutfitRenderData->LODRenderData[LODIndex];
        if (!LOD.IsDataReady())
        {
            continue;
        }
        ++ReadyLODCount;
        bScannedActualLOD |= LODIndex == ActualLOD;
        bScannedPredictedLOD |= LODIndex == PredictedLOD;
        if (LOD.RequiredBones.IsEmpty() || LOD.ActiveBoneIndices.IsEmpty()
            || LOD.RenderSections.IsEmpty())
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit LOD %d required/active/section render data was incomplete."),
                LODIndex);
            return false;
        }

        for (const FBoneIndexType BoneIndex : LOD.RequiredBones)
        {
            OutfitRenderBones.Add(static_cast<int32>(BoneIndex));
        }
        for (const FBoneIndexType BoneIndex : LOD.ActiveBoneIndices)
        {
            OutfitRenderBones.Add(static_cast<int32>(BoneIndex));
        }
        for (const FSkelMeshRenderSection& Section : LOD.RenderSections)
        {
            if (Section.NumVertices == 0 || Section.BoneMap.IsEmpty())
            {
                OutStatus = FString::Printf(
                    TEXT("MetaHuman Outfit LOD %d contained an empty render section bone map."),
                    LODIndex);
                return false;
            }
            for (const FBoneIndexType BoneIndex : Section.BoneMap)
            {
                const int32 UsedBoneIndex = static_cast<int32>(BoneIndex);
                OutfitRenderBones.Add(UsedBoneIndex);
                OutfitSectionBones.Add(UsedBoneIndex);
            }
        }
    }
    if (ReadyLODCount <= 0 || OutfitRenderBones.IsEmpty()
        || OutfitSectionBones.IsEmpty())
    {
        OutStatus = TEXT(
            "MetaHuman Outfit live render LODs consumed no bones.");
        return false;
    }
    if ((bActualLODKnown && !bScannedActualLOD) || !bScannedPredictedLOD)
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit current LODs were not both render-ready during the all-LOD scan: actual=%d scanned=%d predicted=%d scanned=%d."),
            ActualLOD,
            bScannedActualLOD ? 1 : 0,
            PredictedLOD,
            bScannedPredictedLOD ? 1 : 0);
        return false;
    }

    TSet<int32> LeaderBonesWithParents;
    TSet<int32> MappedUsedLeaderBoneSet;
    for (const int32 OutfitBoneIndex : OutfitRenderBones)
    {
        if (!OutfitSkeleton.IsValidIndex(OutfitBoneIndex)
            || !OutfitLeaderBoneMap.IsValidIndex(OutfitBoneIndex))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit render bone %d was outside its reference mapping."),
                OutfitBoneIndex);
            return false;
        }
        const int32 BodyBoneIndex = OutfitLeaderBoneMap[OutfitBoneIndex];
        if (!BodySkeleton.IsValidIndex(BodyBoneIndex)
            || OutfitSkeleton.GetBoneName(OutfitBoneIndex)
                != BodySkeleton.GetBoneName(BodyBoneIndex))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit render bone %s[%d] lacked an exact Body mapping."),
                *OutfitSkeleton.GetBoneName(OutfitBoneIndex).ToString(),
                OutfitBoneIndex);
            return false;
        }

        MappedUsedLeaderBoneSet.Add(BodyBoneIndex);

        int32 RequiredBodyBone = BodyBoneIndex;
        int32 ParentWalkCount = 0;
        while (RequiredBodyBone != INDEX_NONE)
        {
            if (!BodySkeleton.IsValidIndex(RequiredBodyBone)
                || ParentWalkCount++ >= BodySkeleton.GetNum())
            {
                OutStatus = FString::Printf(
                    TEXT("MetaHuman Outfit Body parent chain was invalid at bone %d."),
                    RequiredBodyBone);
                return false;
            }
            LeaderBonesWithParents.Add(RequiredBodyBone);
            RequiredBodyBone =
                BodySkeleton.GetParentIndex(RequiredBodyBone);
        }
    }

    TArray<FBoneIndexType> MappedUsedBones;
    MappedUsedBones.Reserve(MappedUsedLeaderBoneSet.Num());
    for (const int32 BodyBoneIndex : MappedUsedLeaderBoneSet)
    {
        MappedUsedBones.Add(static_cast<FBoneIndexType>(BodyBoneIndex));
    }
    MappedUsedBones.Sort();
    MappedUsedBones.SetNum(Algo::Unique(MappedUsedBones));
    if (MappedUsedBones.Num() != OutfitRenderBones.Num())
    {
        OutStatus = FString::Printf(
            TEXT("MetaHuman Outfit render-used bone mapping was not one-to-one: outfit=%d body=%d."),
            OutfitRenderBones.Num(),
            MappedUsedBones.Num());
        return false;
    }

    TArray<FBoneIndexType> MappedRequiredBones;
    MappedRequiredBones.Reserve(LeaderBonesWithParents.Num());
    for (const int32 BodyBoneIndex : LeaderBonesWithParents)
    {
        MappedRequiredBones.Add(
            static_cast<FBoneIndexType>(BodyBoneIndex));
    }
    MappedRequiredBones.Sort();
    MappedRequiredBones.SetNum(Algo::Unique(MappedRequiredBones));
    if (MappedRequiredBones.IsEmpty() || MappedRequiredBones[0] != 0)
    {
        OutStatus = TEXT(
            "MetaHuman Outfit mapped required-bones set was not root-complete.");
        return false;
    }
    for (const FBoneIndexType BodyBoneIndex : MappedRequiredBones)
    {
        const int32 ParentIndex =
            BodySkeleton.GetParentIndex(static_cast<int32>(BodyBoneIndex));
        if (ParentIndex != INDEX_NONE
            && !MappedRequiredBones.Contains(
                static_cast<FBoneIndexType>(ParentIndex)))
        {
            OutStatus = FString::Printf(
                TEXT("MetaHuman Outfit mapped required-bones set omitted parent %d."),
                ParentIndex);
            return false;
        }
    }

    SourceOutfit = Outfit;
    TargetBody = Body;
    ConfiguredOutfitAsset = OutfitAsset;
    ConfiguredBodyAsset = BodyAsset;
    RequiredLeaderBones = MoveTemp(MappedRequiredBones);
    MappedOutfitUsedLeaderBones = MoveTemp(MappedUsedBones);
    ConfiguredActualOutfitLOD = ActualLOD;
    ConfiguredPredictedOutfitLOD = PredictedLOD;
    ConfiguredOutfitBoneCount = OutfitRenderBones.Num();
    ConfiguredReadyOutfitLODCount = ReadyLODCount;
    OutStatus = FString::Printf(
        TEXT("MetaHuman Outfit required-bones contract mapped %d render bones from %d ready LODs to %d parent-complete Body bones; initial LODs were %d/%d."),
        ConfiguredOutfitBoneCount,
        ConfiguredReadyOutfitLODCount,
        RequiredLeaderBones.Num(),
        ConfiguredActualOutfitLOD,
        ConfiguredPredictedOutfitLOD);
    return true;
}

bool UDiscGolfMetaHumanOutfitRequiredBonesComponent::IsConfiguredFor(
    const USkeletalMeshComponent* Outfit,
    const USkeletalMeshComponent* Body) const
{
    return IsValid(Outfit) && IsValid(Body)
        && Outfit->IsRegistered() && Body->IsRegistered()
        && Outfit->GetWorld() == Body->GetWorld()
        && SourceOutfit == Outfit && TargetBody == Body
        && ConfiguredOutfitAsset == Outfit->GetSkeletalMeshAsset()
        && ConfiguredBodyAsset == Body->GetSkeletalMeshAsset()
        && Outfit->LeaderPoseComponent.Get() == Body
        && !RequiredLeaderBones.IsEmpty()
        && !MappedOutfitUsedLeaderBones.IsEmpty()
        && ConfiguredPredictedOutfitLOD != INDEX_NONE
        && ConfiguredReadyOutfitLODCount > 0;
}

bool UDiscGolfMetaHumanOutfitRequiredBonesComponent::
    DescribeMissingMappedOutfitUsedBones(
        const TArray<FBoneIndexType>& AvailableBodyBones,
        int32& OutMissingCount,
        FString& OutMissingList) const
{
    OutMissingCount = 0;
    OutMissingList.Reset();
    if (!IsConfiguredFor(SourceOutfit.Get(), TargetBody.Get())
        || !ConfiguredBodyAsset)
    {
        return false;
    }

    const FReferenceSkeleton& BodySkeleton =
        ConfiguredBodyAsset->GetRefSkeleton();
    TArray<FString> MissingParts;
    for (const FBoneIndexType BoneIndex : MappedOutfitUsedLeaderBones)
    {
        if (AvailableBodyBones.Contains(BoneIndex))
        {
            continue;
        }
        const int32 BodyBoneIndex = static_cast<int32>(BoneIndex);
        if (!BodySkeleton.IsValidIndex(BodyBoneIndex))
        {
            return false;
        }
        MissingParts.Add(FString::Printf(
            TEXT("%s[%d]"),
            *BodySkeleton.GetBoneName(BodyBoneIndex).ToString(),
            BodyBoneIndex));
    }
    OutMissingCount = MissingParts.Num();
    OutMissingList = MissingParts.IsEmpty()
        ? TEXT("none") : FString::Join(MissingParts, TEXT(","));
    return true;
}

bool UDiscGolfMetaHumanOutfitRequiredBonesComponent::
    DescribeMissingRequiredLeaderBones(
        const TArray<FBoneIndexType>& AvailableBodyBones,
        int32& OutMissingCount,
        FString& OutMissingList) const
{
    OutMissingCount = 0;
    OutMissingList.Reset();
    if (!IsConfiguredFor(SourceOutfit.Get(), TargetBody.Get())
        || !ConfiguredBodyAsset)
    {
        return false;
    }

    const FReferenceSkeleton& BodySkeleton =
        ConfiguredBodyAsset->GetRefSkeleton();
    TArray<FString> MissingParts;
    for (const FBoneIndexType BoneIndex : RequiredLeaderBones)
    {
        if (AvailableBodyBones.Contains(BoneIndex))
        {
            continue;
        }
        const int32 BodyBoneIndex = static_cast<int32>(BoneIndex);
        if (!BodySkeleton.IsValidIndex(BodyBoneIndex))
        {
            return false;
        }
        MissingParts.Add(FString::Printf(
            TEXT("%s[%d]"),
            *BodySkeleton.GetBoneName(BodyBoneIndex).ToString(),
            BodyBoneIndex));
    }
    OutMissingCount = MissingParts.Num();
    OutMissingList = MissingParts.IsEmpty()
        ? TEXT("none") : FString::Join(MissingParts, TEXT(","));
    return true;
}

void UDiscGolfMetaHumanOutfitRequiredBonesComponent::
    GetAdditionalRequiredBonesForLeader(
        int32 LODIndex,
        TArray<FBoneIndexType>& InOutRequiredBones) const
{
    if (!IsConfiguredFor(SourceOutfit.Get(), TargetBody.Get())
        || LeaderPoseComponent.Get() != TargetBody)
    {
        ensureMsgf(false,
            TEXT("MetaHuman Outfit required-bones follower was queried without its validated contract."));
        return;
    }

    USkeletalMesh* const BodyAsset = TargetBody->GetSkeletalMeshAsset();
    FSkeletalMeshRenderData* const BodyRenderData = BodyAsset
        ? BodyAsset->GetResourceForRendering() : nullptr;
    if (!BodyRenderData
        || !BodyRenderData->LODRenderData.IsValidIndex(LODIndex))
    {
        ensureMsgf(false,
            TEXT("MetaHuman Outfit required-bones follower received invalid Body LOD %d."),
            LODIndex);
        return;
    }

    InOutRequiredBones.Sort();
    InOutRequiredBones.SetNum(Algo::Unique(InOutRequiredBones));
    MergeInBoneIndexArrays(InOutRequiredBones, RequiredLeaderBones);
    FAnimationRuntime::EnsureParentsPresent(
        InOutRequiredBones, BodyAsset->GetRefSkeleton());
    InOutRequiredBones.Sort();
    InOutRequiredBones.SetNum(Algo::Unique(InOutRequiredBones));
}
