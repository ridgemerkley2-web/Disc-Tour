#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfSession8CookClosureRunner.generated.h"

class UDiscGolfAvatarBackendProfile;
class UObject;

/**
 * Packaged-build-only proof that Session 8's explicit runtime cook closure is
 * present while its validation fixtures and reserved MetaHuman entry points
 * remain absent. This actor is read-only and never exercises save or gameplay
 * mutation paths.
 */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfSession8CookClosureRunner : public AActor
{
    GENERATED_BODY()

public:
    void Start();

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UObject>> LoadedRuntimeAssets;

    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfAvatarBackendProfile> LoadedBackendProfile;

    void Fail(const FString& Reason) const;
    void Pass() const;
};
