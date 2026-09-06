#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfDiscTypes.h"
#include "DiscGolfTypes.h"
#include "DiscBagComponent.generated.h"

class UDiscGolfRHBHThrowAdapterComponent;

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UDiscBagComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscBagComponent();
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable) void SelectDiscIndex(int32 NewIndex);
    UFUNCTION(BlueprintCallable) bool SelectEquipment(FName MoldId, EDiscPlastic Plastic);
    UFUNCTION(BlueprintCallable) void CyclePlastic();
    UFUNCTION(BlueprintPure) FName GetSelectedMoldId() const;
    UFUNCTION(BlueprintPure) int32 GetSelectedIndex() const { return SelectedIndex; }
    UFUNCTION(BlueprintPure) EDiscPlastic GetSelectedPlastic() const { return SelectedPlastic; }
    UFUNCTION(BlueprintPure) bool GetSelectedDiscInstance(FDGDiscInstance& OutDisc) const;
    UFUNCTION(BlueprintPure) FGuid GetSelectedDiscInstanceId() const;
    UFUNCTION(BlueprintCallable) bool SelectDiscInstance(FGuid InstanceId);
    UFUNCTION(BlueprintCallable) bool ToggleSelectedFavorite();
    UFUNCTION(BlueprintPure) FString GetSelectedEquipmentStatusText() const;
    UFUNCTION(BlueprintPure) FDGDiscBagLoadout GetEquipmentLoadout() const { return EquipmentLoadout; }
    UFUNCTION(BlueprintCallable) bool ApplyEquipmentLoadout(
        const FDGDiscBagLoadout& Loadout,
        FString& OutError);
    UFUNCTION(BlueprintCallable) bool SaveEquipment(FString& OutError) const;
    UFUNCTION(BlueprintCallable) bool LoadEquipment(FString& OutError);
    bool SaveEquipmentToSlot(const FString& SlotName, int32 UserIndex, FString& OutError) const;
    bool LoadEquipmentFromSlot(const FString& SlotName, int32 UserIndex, FString& OutError);

    bool ResolveSelectedDiscInstance(
        const FResolvedDiscDefinition& CatalogDisc,
        FResolvedDiscDefinition& OutResolved,
        FString& OutError) const;

    static bool ResolveDiscInstance(
        const FDGDiscInstance& Instance,
        const FResolvedDiscDefinition& CatalogDisc,
        FResolvedDiscDefinition& OutResolved,
        FString& OutError);

    static bool ValidateDiscInstance(const FDGDiscInstance& Instance, FString& OutError);
    static bool ValidateLoadout(const FDGDiscBagLoadout& Loadout, FString& OutError);
    static FDGDiscBagLoadout BuildDefaultLoadout();

    const TArray<FName>& GetMoldIds() const { return MoldIds; }
    bool IsEquipmentMutationLocked() const { return bEquipmentMutationLocked; }

private:
    friend class UDiscGolfRHBHThrowAdapterComponent;

    static FString DefaultEquipmentSaveSlot();
    bool CommitSelection(int32 NewIndex, EDiscPlastic NewPlastic);
    void SetEquipmentMutationLocked(bool bLocked) { bEquipmentMutationLocked = bLocked; }

    UPROPERTY(EditAnywhere, Category="Bag") TArray<FName> MoldIds;
    UPROPERTY(EditAnywhere, Category="Bag") int32 SelectedIndex = 0;
    UPROPERTY(EditAnywhere, Category="Bag") EDiscPlastic SelectedPlastic = EDiscPlastic::Tour;
    UPROPERTY(EditAnywhere, Category="Bag") FDGDiscBagLoadout EquipmentLoadout;
    bool bEquipmentMutationLocked = false;
};
