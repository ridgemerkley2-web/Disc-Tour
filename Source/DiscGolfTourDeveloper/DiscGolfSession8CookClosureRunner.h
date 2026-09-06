#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession8CookClosureRunner.generated.h"

class UDiscGolfAvatarBackendProfile;
class UObject;

/**
 * Packaged-build-only proof that Session 8's explicit runtime cook closure is
 * present while its validation fixtures and editor-only source MHC remain
 * absent. The legacy 69 and accepted MetaHuman closure are counted separately.
 * This actor is read-only and never exercises save or gameplay mutation paths.
 */
UCLASS()
class DISCGOLFTOURDEVELOPER_API ADiscGolfSession8CookClosureRunner : public AActor
{
    GENERATED_BODY()

public:
    void Start();

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> LoadedRuntimeAssets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> LoadedMetaHumanRuntimeAssets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UDiscGolfAvatarBackendProfile>> LoadedBackendProfiles;

    void Fail(const FString& Reason) const;
    void Pass(int32 MetaHumanDirectAssets, int32 MetaHumanGeneratedClasses) const;
};
