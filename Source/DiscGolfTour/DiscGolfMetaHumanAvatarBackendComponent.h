#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAvatarBackendComponent.h"
#include "DiscGolfMetaHumanAvatarBackendComponent.generated.h"

class USkeletalMeshComponent;

/**
 * Project-side adapter for a future assembled MetaHuman actor. It deliberately
 * has no MetaHuman module dependency: the actor must implement the project
 * visual contract and expose one tagged body and one tagged head.
 */
UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscGolfMetaHumanAvatarBackendComponent
    : public UDiscGolfAvatarBackendComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    USkeletalMeshComponent* GetVerifiedVisualBody() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    USkeletalMeshComponent* GetVerifiedVisualHead() const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|Avatar|MetaHuman")
    FString GetLastAdapterStatus() const { return LastAdapterStatus; }

    virtual bool ConfigureVisualBackend_Implementation(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const FDGFullCharacterCustomization& Customization) override;

    virtual bool ApplyVisualCustomization_Implementation(
        AActor* VisualActor,
        const FDGFullCharacterCustomization& Customization) override;

protected:
    virtual void OnUnregister() override;

private:
    bool ValidateConfiguredVisual(
        AActor* VisualActor,
        USkeletalMeshComponent* AnimationSourceMesh,
        const UDiscGolfAvatarBackendProfile* ExpectedProfile,
        USkeletalMeshComponent*& OutBody,
        USkeletalMeshComponent*& OutHead,
        FString& OutStatus) const;

    UPROPERTY(Transient)
    TObjectPtr<AActor> VerifiedVisualActor;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> VerifiedVisualBody;

    UPROPERTY(Transient)
    TObjectPtr<USkeletalMeshComponent> VerifiedVisualHead;

    UPROPERTY(Transient)
    FString LastAdapterStatus = TEXT("DG master proxy fallback active.");
};
