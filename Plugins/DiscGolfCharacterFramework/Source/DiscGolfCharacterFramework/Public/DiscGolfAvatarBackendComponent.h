#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAvatarBackendTypes.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfAvatarBackendComponent.generated.h"

class AActor;
class UDiscGolfAvatarBackendProfile;
class USkeletalMeshComponent;
class UWorld;

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

    UPROPERTY(Transient, BlueprintReadOnly, Category="Disc Golf|Avatar")
    TObjectPtr<AActor> SpawnedVisualActor;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Avatar")
    FDGOnAvatarBackendReady OnAvatarBackendReady;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar")
    bool BuildVisualBackend(
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization
    );

    // API note: this previously returned void. Callers may still ignore the
    // result, but adapters should use it to retain the last verified visual.
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar")
    bool ApplyCustomizationToVisual(
        const FDGFullCharacterCustomization& Customization
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Avatar")
    void DestroyVisualBackend();

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar")
    bool IsVisualBackendReady() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar")
    FDGAvatarBackendState GetAvatarBackendState() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar")
    AActor* GetActiveVisualActor() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar")
    UDiscGolfAvatarBackendProfile* GetActiveBackendProfile() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar")
    USkeletalMeshComponent* GetActiveAnimationSourceMesh() const;

    // Implement this in a project Blueprint subclass/component adapter.
    // For MetaHuman this is where Retarget Pose From Mesh / component wiring,
    // body/head discovery, material application and hair/outfit mapping occurs.
    // API note: the adapter hooks were void BlueprintImplementableEvents. They
    // are now bool BlueprintNativeEvents so native and Blueprint adapters must
    // explicitly prove success; the native base deliberately fails closed.
    // Return true only after the candidate is completely configured and safe
    // to present.
    UFUNCTION(BlueprintNativeEvent, Category="Disc Golf|Avatar")
    bool ConfigureVisualBackend(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization
    );
    virtual bool ConfigureVisualBackend_Implementation(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization
    );

    // Implementations must leave the active visual unchanged when returning
    // false. Presentation safety is re-applied after every call.
    UFUNCTION(BlueprintNativeEvent, Category="Disc Golf|Avatar")
    bool ApplyVisualCustomization(
        AActor* VisualActor,
        const FDGFullCharacterCustomization& Customization
    );
    virtual bool ApplyVisualCustomization_Implementation(
        AActor* VisualActor,
        const FDGFullCharacterCustomization& Customization
    );

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnUnregister() override;

private:
    static bool IsAnimationSourceUsable(
        const USkeletalMeshComponent* AnimationSourceMesh,
        const UWorld* ExpectedWorld
    );
    static void EnforcePresentationOnly(AActor* VisualActor, bool bHidden);
    static void AddSourceTickPrerequisites(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh
    );
    static void DestroyVisualActor(AActor* VisualActor);
    void ResetActiveBackendState();

    UPROPERTY(Transient)
    TObjectPtr<AActor> PendingVisualActor;

    UPROPERTY(Transient)
    TObjectPtr<UDiscGolfAvatarBackendProfile> ActiveBackendProfile;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> ActiveAnimationSourceMesh;

    UPROPERTY(Transient)
    FDGAvatarBackendState ActiveBackendState;

    bool bBuildInProgress = false;
    bool bApplyInProgress = false;
};
