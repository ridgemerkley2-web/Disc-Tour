#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfMetaHumanVisualContract.generated.h"

class UDiscGolfAvatarBackendProfile;
class USkeletalMeshComponent;

/**
 * Project-owned, dependency-free contract for an assembled MetaHuman actor.
 * A future assembled actor Blueprint must implement both operations and return
 * success only after its tagged body/head and DG-source retarget are usable.
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
