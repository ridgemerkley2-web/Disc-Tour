#include "DiscGolfAppearanceComponent.h"
#include "Components/SkeletalMeshComponent.h"

UDiscGolfAppearanceComponent::UDiscGolfAppearanceComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

static float DGNormalizeCentered(float Value, float Center, float HalfRange)
{
    if (HalfRange <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    return FMath::Clamp((Value - Center) / HalfRange, -1.0f, 1.0f);
}

void UDiscGolfAppearanceComponent::ApplyStandardMorphs(USkeletalMeshComponent* MeshComp, const FDGBodyProfile& Body)
{
    if (!MeshComp)
    {
        return;
    }

    MeshComp->SetMorphTarget(HeightMorph, DGNormalizeCentered(Body.HeightCm, 183.0f, 27.0f));
    MeshComp->SetMorphTarget(ShoulderWidthMorph, DGNormalizeCentered(Body.ShoulderWidthScale, 1.0f, 0.08f));
    MeshComp->SetMorphTarget(TorsoLengthMorph, DGNormalizeCentered(Body.TorsoLengthScale, 1.0f, 0.06f));
    MeshComp->SetMorphTarget(LegLengthMorph, DGNormalizeCentered(Body.LegLengthScale, 1.0f, 0.06f));
    MeshComp->SetMorphTarget(HandScaleMorph, DGNormalizeCentered(Body.HandScale, 1.0f, 0.06f));
}
