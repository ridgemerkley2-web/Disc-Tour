#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAvatarBackendTypes.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfAvatarBackendComponent.generated.h"

class UDiscGolfAvatarBackendProfile;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGOnAvatarBackendReady,
    FDGAvatarBackendState,
    State
);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAvatarBackendComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfAvatarBackendComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Avatar")
    TObjectPtr<UDiscGolfAvatarBackendProfile> BackendProfile;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Avatar")
    TObjectPtr<AActor> SpawnedVisualActor;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Avatar")
    FDGOnAvatarBackendReady OnAvatarBackendReady;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar")
    bool BuildVisualBackend(
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar")
    void ApplyCustomizationToVisual(
        const FDGFullCharacterCustomization& Customization
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar")
    void DestroyVisualBackend();

    // Implement this in a project Blueprint subclass/component adapter.
    // For MetaHuman this is where Retarget Pose From Mesh / component wiring,
    // body/head discovery, material application and hair/outfit mapping occurs.
    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Avatar")
    void ConfigureVisualBackend(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization
    );

    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|Avatar")
    void ApplyVisualCustomization(
        AActor* VisualActor,
        const FDGFullCharacterCustomization& Customization
    );
};
