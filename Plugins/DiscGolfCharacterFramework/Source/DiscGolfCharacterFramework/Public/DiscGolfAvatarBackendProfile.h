#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfAvatarBackendTypes.h"
#include "DiscGolfAvatarBackendProfile.generated.h"

class AActor;

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAvatarBackendProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Avatar")
    FName BackendId = TEXT("dg_master");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Avatar")
    EDGAvatarBackend Backend = EDGAvatarBackend::DGMaster;

    // An assembled MetaHuman actor Blueprint may be referenced here without
    // creating a hard C++ dependency on MetaHuman plugins.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Avatar")
    TSoftClassPtr<AActor> VisualActorClass;

    // For MetaHuman, keep ShippingSafeAssembled as the default.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Avatar")
    EDGMetaHumanRuntimeMode MetaHumanRuntimeMode = EDGMetaHumanRuntimeMode::ShippingSafeAssembled;

    // A project asset can be stored here (typically an IK Retargeter) without
    // requiring the runtime module to directly link the IK Rig plugin.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    TSoftObjectPtr<UObject> RetargetAsset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation")
    bool bUseRuntimeRetargeting = true;

    // Tags used by project-specific Blueprint integration to locate components.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Components")
    FName VisualBodyComponentTag = TEXT("DGVisualBody");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Components")
    FName VisualHeadComponentTag = TEXT("DGVisualHead");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quality")
    FName PreferredQualityProfileId = TEXT("GameplayHigh");

    // v1.3 deliberately treats runtime MetaHuman face sculpting as unsupported
    // unless a future project-specific adapter explicitly proves otherwise.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Safety")
    bool bAllowRuntimeFaceSculpting = false;
};
