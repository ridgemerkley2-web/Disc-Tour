#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfEnvironmentTypes.h"
#include "DiscGolfVegetationInteractionActor.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EDiscGolfVegetationInteractionType : uint8
{
    LightCanopy,
    DenseCanopy,
    Shrub
};

/** Non-blocking decorative canopy/shrub drag proxy. Trunks stay solid collision. */
UCLASS(BlueprintType)
class DISCGOLFTOUR_API ADiscGolfVegetationInteractionActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfVegetationInteractionActor();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Disc Interaction")
    TObjectPtr<UBoxComponent> InteractionVolume;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Interaction")
    EDiscGolfVegetationInteractionType InteractionType = EDiscGolfVegetationInteractionType::LightCanopy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Interaction")
    FDiscGolfEnvironmentInteractionProfile Profile;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Interaction")
    FVector BoxExtentCm = FVector(240.0f, 240.0f, 180.0f);

    UFUNCTION(BlueprintPure, Category="Disc Interaction")
    bool HasValidInteractionContract() const;
};
