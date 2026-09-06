#include "DiscGolfCharacterCustomizationComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

void UDiscGolfCharacterCustomizationComponent::ApplyBodyMorphs(USkeletalMeshComponent* Mesh) const
{
    if (!IsValid(Mesh))
    {
        return;
    }
    const float UniformScale = FMath::Clamp(Current.Body.HeightCm / 183.0f, 0.82f, 1.15f);
    Mesh->SetRelativeScale3D(FVector(UniformScale));
}

void UDiscGolfCharacterCustomizationComponent::ApplyFaceMorphs(USkeletalMeshComponent* Mesh) const
{
    if (!IsValid(Mesh))
    {
        return;
    }
    for (const TPair<FName, float>& Morph : Current.Face.MorphValues)
    {
        Mesh->SetMorphTarget(Morph.Key, FMath::Clamp(Morph.Value, -1.0f, 1.0f));
    }
}

void UDiscGolfCharacterCustomizationComponent::ApplySkinAndEyeMaterials(
    UObject* BodyMaterialOwner,
    USkeletalMeshComponent* HeadMesh) const
{
    (void)BodyMaterialOwner;
    if (!IsValid(HeadMesh))
    {
        return;
    }
    for (int32 Index = 0; Index < HeadMesh->GetNumMaterials(); ++Index)
    {
        UMaterialInstanceDynamic* Material = HeadMesh->CreateAndSetMaterialInstanceDynamic(Index);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("DG_SkinTone"), Current.Appearance.SkinTone);
            Material->SetVectorParameterValue(TEXT("DG_EyeColor"), Current.Appearance.EyeColor);
        }
    }
}

void UDiscGolfCharacterCustomizationComponent::RebuildHair(USkeletalMeshComponent* HeadMesh) const
{
    if (IsValid(HeadMesh))
    {
        HeadMesh->MarkRenderStateDirty();
    }
}
