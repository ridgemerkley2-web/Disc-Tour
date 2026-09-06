#include "DiscGolfAppearanceComponent.h"

#include "Components/SkeletalMeshComponent.h"

void UDiscGolfAppearanceComponent::ApplyStandardMorphs(
    USkeletalMeshComponent* Mesh,
    const FDGBodyProfile& Body) const
{
    if (!IsValid(Mesh))
    {
        return;
    }

    const float UniformScale = FMath::Clamp(Body.HeightCm / 183.0f, 0.82f, 1.15f);
    Mesh->SetRelativeScale3D(FVector(UniformScale));
}
