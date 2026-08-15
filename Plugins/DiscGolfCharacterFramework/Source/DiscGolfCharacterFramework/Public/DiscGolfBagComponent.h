#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfBagComponent.generated.h"

class UDiscGolfDiscCatalog;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDGOnBagChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDGOnSelectedDiscChanged, FDGDiscInstance, Disc);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfBagComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfBagComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Bag")
    TObjectPtr<UDiscGolfDiscCatalog> Catalog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Bag")
    FDGDiscBagLoadout CurrentBag;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Bag")
    FDGOnBagChanged OnBagChanged;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Bag")
    FDGOnSelectedDiscChanged OnSelectedDiscChanged;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    FDGDiscInstance CreateDiscInstance(
        FName DiscDefinitionId,
        FName PlasticId,
        float MassGrams,
        FLinearColor Color,
        FName StampId
    ) const;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    bool AddDisc(const FDGDiscInstance& Disc);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    bool RemoveDisc(FGuid InstanceId);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    bool SelectDisc(FGuid InstanceId);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    bool GetSelectedDisc(FDGDiscInstance& OutDisc) const;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    bool SetWear(FGuid InstanceId, float Wear01);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Bag")
    void SetLoadout(const FDGDiscBagLoadout& NewLoadout);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Bag")
    bool CanAddDisc() const;
};
