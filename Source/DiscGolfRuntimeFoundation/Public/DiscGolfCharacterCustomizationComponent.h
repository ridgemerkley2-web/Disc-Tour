#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfCharacterCustomizationComponent.generated.h"

class UDiscGolfCosmeticCatalog;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfCharacterCustomizationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGFullCharacterCustomization Current;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDiscGolfCosmeticCatalog> CosmeticCatalog;

    UFUNCTION(BlueprintCallable) void ApplyBodyMorphs(USkeletalMeshComponent* Mesh) const;
    UFUNCTION(BlueprintCallable) void ApplyFaceMorphs(USkeletalMeshComponent* Mesh) const;
    UFUNCTION(BlueprintCallable) void ApplySkinAndEyeMaterials(UObject* BodyMaterialOwner, USkeletalMeshComponent* HeadMesh) const;
    UFUNCTION(BlueprintCallable) void RebuildHair(USkeletalMeshComponent* HeadMesh) const;
};
