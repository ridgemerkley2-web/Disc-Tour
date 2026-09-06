#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfMetaHumanVisualContract.generated.h"

class UDiscGolfAvatarBackendProfile;
class USkeletalMeshComponent;

/**
 * Project-owned contract for an assembled MetaHuman presentation actor.
 * The Blueprint accepts a prevalidated profile/configuration request. A fixed
 * curated preset may intentionally apply no DG proxy-only appearance fields,
 * but must say so instead of claiming a mapping. Returning true does not prove
 * retarget readiness: the native adapter separately locates the tagged body/head,
 * installs the DG-source retarget instance, and verifies it before the visual can
 * be promoted to ready.
 */
UINTERFACE(BlueprintType, Blueprintable)
class DISCGOLFTOUR_API UDiscGolfMetaHumanVisualContract : public UInterface
{
    GENERATED_BODY()
};

class DISCGOLFTOUR_API IDiscGolfMetaHumanVisualContract
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable,
        Category="Disc Golf|Avatar|MetaHuman")
    bool ConfigureFromDGAnimationSource(
        USkeletalMeshComponent* AnimationSourceMesh,
        UDiscGolfAvatarBackendProfile* BackendProfile,
        const FDGFullCharacterCustomization& Customization,
        FString& OutStatus);

    UFUNCTION(BlueprintImplementableEvent, BlueprintCallable,
        Category="Disc Golf|Avatar|MetaHuman")
    bool ApplyMappedCustomization(
        UDiscGolfAvatarBackendProfile* BackendProfile,
        const FDGFullCharacterCustomization& Customization,
        FString& OutStatus);
};
