#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BasketActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class ADiscActor;
class UPrimitiveComponent;

UCLASS()
class DISCGOLFTOUR_API ABasketActor : public AActor
{
    GENERATED_BODY()

public:
    ABasketActor();

    /**
     * Resolves a disc that was already inside the catch volume when flight
     * began. GameMode calls this immediately after committing an accepted
     * launch so contact is evaluated at solver time zero, never on a later
     * render tick.
     */
    bool EvaluateOverlappingDiscContact(ADiscActor* Disc);

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pole;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Tray;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TopBand;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> CatchVolume;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> ChainStrands;

    bool EvaluateDiscContact(ADiscActor* Disc);
    UFUNCTION() void OnCatchVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
