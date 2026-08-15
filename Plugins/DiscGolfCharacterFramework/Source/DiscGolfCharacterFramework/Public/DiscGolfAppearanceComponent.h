#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfAppearanceComponent.generated.h"

class USkeletalMeshComponent;

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfAppearanceComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Appearance")
    FName HeightMorph = TEXT("DG_Height");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Appearance")
    FName ShoulderWidthMorph = TEXT("DG_ShoulderWidth");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Appearance")
    FName TorsoLengthMorph = TEXT("DG_TorsoLength");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Appearance")
    FName LegLengthMorph = TEXT("DG_LegLength");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Appearance")
    FName HandScaleMorph = TEXT("DG_HandScale");

    // Visual morph application only. Control Rig/IK must still adjust skeleton proportions.
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Appearance")
    void ApplyStandardMorphs(USkeletalMeshComponent* MeshComp, const FDGBodyProfile& Body);
};
