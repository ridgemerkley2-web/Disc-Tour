#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfAvatarBackendTypes.h"
#include "DiscGolfAvatarBackendProfile.generated.h"

UCLASS(BlueprintType)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfAvatarBackendProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BackendId = TEXT("dg_master");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGAvatarBackend Backend = EDGAvatarBackend::DGMaster;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGMetaHumanRuntimeMode MetaHumanRuntimeMode = EDGMetaHumanRuntimeMode::Disabled;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftClassPtr<AActor> VisualActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bUseRuntimeRetargeting = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UObject> RetargetAsset;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName VisualBodyComponentTag = TEXT("Body");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName VisualHeadComponentTag = TEXT("Face");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PreferredQualityProfileId = TEXT("GameplayPerformance");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowRuntimeFaceSculpting = false;
};
