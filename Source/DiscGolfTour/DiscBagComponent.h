#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTypes.h"
#include "DiscBagComponent.generated.h"

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscBagComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscBagComponent();

    UFUNCTION(BlueprintCallable) void SelectDiscIndex(int32 NewIndex);
    UFUNCTION(BlueprintCallable) bool SelectEquipment(FName MoldId, EDiscPlastic Plastic);
    UFUNCTION(BlueprintCallable) void CyclePlastic();
    UFUNCTION(BlueprintPure) FName GetSelectedMoldId() const;
    UFUNCTION(BlueprintPure) int32 GetSelectedIndex() const { return SelectedIndex; }
    UFUNCTION(BlueprintPure) EDiscPlastic GetSelectedPlastic() const { return SelectedPlastic; }
    const TArray<FName>& GetMoldIds() const { return MoldIds; }

private:
    UPROPERTY(EditAnywhere, Category="Bag") TArray<FName> MoldIds;
    UPROPERTY(EditAnywhere, Category="Bag") int32 SelectedIndex = 0;
    UPROPERTY(EditAnywhere, Category="Bag") EDiscPlastic SelectedPlastic = EDiscPlastic::Tour;
};
