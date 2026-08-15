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

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Pole;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Tray;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> TopBand;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> CatchVolume;
    UPROPERTY(VisibleAnywhere) TArray<TObjectPtr<UStaticMeshComponent>> ChainStrands;

    UFUNCTION() void OnCatchVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
