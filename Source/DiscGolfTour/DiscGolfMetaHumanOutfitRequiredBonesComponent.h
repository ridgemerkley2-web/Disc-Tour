#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "DiscGolfMetaHumanOutfitRequiredBonesComponent.generated.h"

class USkeletalMesh;

/**
 * Non-rendering follower used only to make a MetaHuman body evaluate every
 * bone consumed by every render-ready LOD of the generated Outfit. The authored
 * Outfit remains the sole visible garment and still follows the Body pose.
 */
UCLASS(Transient, NotBlueprintable)
class DISCGOLFTOUR_API UDiscGolfMetaHumanOutfitRequiredBonesComponent final
    : public USkeletalMeshComponent
{
    GENERATED_BODY()

public:
    explicit UDiscGolfMetaHumanOutfitRequiredBonesComponent(
        const FObjectInitializer& ObjectInitializer);

    bool ConfigureBoneContract(
        USkeletalMeshComponent* Outfit,
        USkeletalMeshComponent* Body,
        FString& OutStatus);

    bool IsConfiguredFor(
        const USkeletalMeshComponent* Outfit,
        const USkeletalMeshComponent* Body) const;

    int32 GetConfiguredActualOutfitLOD() const
    {
        return ConfiguredActualOutfitLOD;
    }

    int32 GetConfiguredPredictedOutfitLOD() const
    {
        return ConfiguredPredictedOutfitLOD;
    }

    int32 GetConfiguredOutfitBoneCount() const
    {
        return ConfiguredOutfitBoneCount;
    }

    int32 GetConfiguredReadyOutfitLODCount() const
    {
        return ConfiguredReadyOutfitLODCount;
    }

    int32 GetRequiredLeaderBoneCount() const
    {
        return RequiredLeaderBones.Num();
    }

    int32 GetMappedOutfitUsedLeaderBoneCount() const
    {
        return MappedOutfitUsedLeaderBones.Num();
    }

    bool DescribeMissingMappedOutfitUsedBones(
        const TArray<FBoneIndexType>& AvailableBodyBones,
        int32& OutMissingCount,
        FString& OutMissingList) const;

    bool DescribeMissingRequiredLeaderBones(
        const TArray<FBoneIndexType>& AvailableBodyBones,
        int32& OutMissingCount,
        FString& OutMissingList) const;

    virtual void GetAdditionalRequiredBonesForLeader(
        int32 LODIndex,
        TArray<FBoneIndexType>& InOutRequiredBones) const override;

private:
    void ResetBoneContract();

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> SourceOutfit;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> TargetBody;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMesh> ConfiguredOutfitAsset;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMesh> ConfiguredBodyAsset;

    TArray<FBoneIndexType> RequiredLeaderBones;
    TArray<FBoneIndexType> MappedOutfitUsedLeaderBones;
    int32 ConfiguredActualOutfitLOD = INDEX_NONE;
    int32 ConfiguredPredictedOutfitLOD = INDEX_NONE;
    int32 ConfiguredOutfitBoneCount = 0;
    int32 ConfiguredReadyOutfitLODCount = 0;
};
