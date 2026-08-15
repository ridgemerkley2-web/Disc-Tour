#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfCharacterCustomizationComponent.generated.h"

class UDiscGolfCosmeticCatalog;
class UDiscGolfOutfitComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UDiscGolfCosmeticItem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDGOnCharacterCustomizationChanged);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCharacterCustomizationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfCharacterCustomizationComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Customization")
    TObjectPtr<UDiscGolfCosmeticCatalog> CosmeticCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Customization")
    FDGFullCharacterCustomization Current;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Customization")
    FDGOnCharacterCustomizationChanged OnCustomizationChanged;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Customization")
    void ApplyAll(
        USkeletalMeshComponent* BodyMesh,
        USkeletalMeshComponent* HeadMesh,
        UDiscGolfOutfitComponent* OutfitComponent
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Customization")
    void ApplyBodyMorphs(USkeletalMeshComponent* MeshComp);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Customization")
    void ApplyFaceMorphs(USkeletalMeshComponent* HeadMesh);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Customization")
    void ApplySkinAndEyeMaterials(
        USkeletalMeshComponent* BodyMesh,
        USkeletalMeshComponent* HeadMesh
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Customization")
    void RebuildHair(
        USkeletalMeshComponent* HeadMesh
    );

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Customization")
    void ClearHairComponents();

private:
    UPROPERTY(Transient)
    TArray<TObjectPtr<UActorComponent>> SpawnedHairComponents;

    void ApplyColorToAllMaterials(
        USkeletalMeshComponent* MeshComp,
        FName ParameterName,
        FLinearColor Value
    );

    void ApplyScalarToAllMaterials(
        USkeletalMeshComponent* MeshComp,
        FName ParameterName,
        float Value
    );

    void SpawnCosmeticMesh(
        UDiscGolfCosmeticItem* Item,
        USkeletalMeshComponent* HeadMesh,
        FLinearColor Color,
        FName ColorParameterName
    );
};
