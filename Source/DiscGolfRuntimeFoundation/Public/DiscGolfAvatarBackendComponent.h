#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAvatarBackendProfile.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfAvatarBackendComponent.generated.h"

class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FDGAvatarBackendReady,
    FDGAvatarBackendState,
    State);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfAvatarBackendComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfAvatarBackendComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDiscGolfAvatarBackendProfile> BackendProfile;
    UPROPERTY(BlueprintAssignable) FDGAvatarBackendReady OnAvatarBackendReady;

    UFUNCTION(BlueprintCallable)
    bool BuildVisualBackend(USkeletalMeshComponent* AnimationSourceMesh, const FDGFullCharacterCustomization& Customization);

    UFUNCTION(BlueprintCallable)
    bool ApplyCustomizationToVisual(const FDGFullCharacterCustomization& Customization);

    UFUNCTION(BlueprintCallable) void DestroyVisualBackend();
    UFUNCTION(BlueprintPure) bool IsVisualBackendReady() const { return BackendState.bVisualReady; }
    UFUNCTION(BlueprintPure) AActor* GetActiveVisualActor() const { return ActiveVisualActor; }
    UFUNCTION(BlueprintPure) USkeletalMeshComponent* GetActiveAnimationSourceMesh() const { return ActiveAnimationSourceMesh; }
    UFUNCTION(BlueprintPure) UDiscGolfAvatarBackendProfile* GetActiveBackendProfile() const { return ActiveBackendProfile; }
    UFUNCTION(BlueprintPure) FDGAvatarBackendState GetAvatarBackendState() const { return BackendState; }

    UFUNCTION(BlueprintNativeEvent)
    bool ConfigureVisualBackend(AActor* VisualActor, USkeletalMeshComponent* AnimationSourceMesh, const FDGFullCharacterCustomization& Customization);

    UFUNCTION(BlueprintNativeEvent)
    bool ApplyVisualCustomization(AActor* VisualActor, const FDGFullCharacterCustomization& Customization);

protected:
    virtual bool FinalizeVisualBackendActivation(AActor* VisualActor, USkeletalMeshComponent* AnimationSourceMesh, const FDGFullCharacterCustomization& Customization);

private:
    UPROPERTY(Transient) TObjectPtr<AActor> ActiveVisualActor;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> ActiveAnimationSourceMesh;
    UPROPERTY(Transient) TObjectPtr<UDiscGolfAvatarBackendProfile> ActiveBackendProfile;
    UPROPERTY(Transient) FDGAvatarBackendState BackendState;
};
