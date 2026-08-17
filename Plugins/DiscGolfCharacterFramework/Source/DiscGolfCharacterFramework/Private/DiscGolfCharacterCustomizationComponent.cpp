#include "DiscGolfCharacterCustomizationComponent.h"
#include "DiscGolfCosmeticCatalog.h"
#include "DiscGolfCosmeticItem.h"
#include "DiscGolfOutfitComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

UDiscGolfCharacterCustomizationComponent::UDiscGolfCharacterCustomizationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

static float DGCentered(float Value, float Center, float HalfRange)
{
    if (HalfRange <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    return FMath::Clamp((Value - Center) / HalfRange, -1.0f, 1.0f);
}

static void DGSetMorphTargetIfAvailable(
    USkeletalMeshComponent* MeshComp,
    FName MorphName,
    float Value)
{
    USkeletalMesh* Mesh = MeshComp ? MeshComp->GetSkeletalMeshAsset() : nullptr;
    if (Mesh && Mesh->FindMorphTarget(MorphName))
    {
        MeshComp->SetMorphTarget(MorphName, Value);
    }
}

void UDiscGolfCharacterCustomizationComponent::ApplyBodyMorphs(USkeletalMeshComponent* MeshComp)
{
    if (!MeshComp)
    {
        return;
    }

    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Height"),
        DGCentered(Current.Body.HeightCm, 183.0f, 27.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_ShoulderWidth"),
        DGCentered(Current.Body.ShoulderWidthScale, 1.0f, 0.08f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_TorsoLength"),
        DGCentered(Current.Body.TorsoLengthScale, 1.0f, 0.06f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_LegLength"),
        DGCentered(Current.Body.LegLengthScale, 1.0f, 0.06f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_HandScale"),
        DGCentered(Current.Body.HandScale, 1.0f, 0.06f));

    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_Muscularity"),
        FMath::Clamp(Current.BodyBuild.Muscularity, 0.0f, 1.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_BodyFat"),
        FMath::Clamp(Current.BodyBuild.BodyFat, 0.0f, 1.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_Chest"),
        FMath::Clamp(Current.BodyBuild.Chest, -1.0f, 1.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_Waist"),
        FMath::Clamp(Current.BodyBuild.Waist, -1.0f, 1.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_Hips"),
        FMath::Clamp(Current.BodyBuild.Hips, -1.0f, 1.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_Arms"),
        FMath::Clamp(Current.BodyBuild.Arms, -1.0f, 1.0f));
    DGSetMorphTargetIfAvailable(MeshComp, TEXT("DG_Body_Legs"),
        FMath::Clamp(Current.BodyBuild.Legs, -1.0f, 1.0f));
}

void UDiscGolfCharacterCustomizationComponent::ApplyFaceMorphs(USkeletalMeshComponent* HeadMesh)
{
    if (!HeadMesh)
    {
        return;
    }

    static const TMap<FName, FName> MorphMap = {
        {TEXT("head_width"), TEXT("DG_Face_HeadWidth")},
        {TEXT("head_height"), TEXT("DG_Face_HeadHeight")},
        {TEXT("brow_height"), TEXT("DG_Face_BrowHeight")},
        {TEXT("brow_depth"), TEXT("DG_Face_BrowDepth")},
        {TEXT("eye_size"), TEXT("DG_Face_EyeSize")},
        {TEXT("eye_spacing"), TEXT("DG_Face_EyeSpacing")},
        {TEXT("eye_depth"), TEXT("DG_Face_EyeDepth")},
        {TEXT("nose_width"), TEXT("DG_Face_NoseWidth")},
        {TEXT("nose_length"), TEXT("DG_Face_NoseLength")},
        {TEXT("nose_bridge"), TEXT("DG_Face_NoseBridge")},
        {TEXT("cheek_width"), TEXT("DG_Face_CheekWidth")},
        {TEXT("cheek_fullness"), TEXT("DG_Face_CheekFullness")},
        {TEXT("jaw_width"), TEXT("DG_Face_JawWidth")},
        {TEXT("jaw_height"), TEXT("DG_Face_JawHeight")},
        {TEXT("chin_width"), TEXT("DG_Face_ChinWidth")},
        {TEXT("chin_length"), TEXT("DG_Face_ChinLength")},
        {TEXT("mouth_width"), TEXT("DG_Face_MouthWidth")},
        {TEXT("lip_fullness"), TEXT("DG_Face_LipFullness")},
        {TEXT("ear_size"), TEXT("DG_Face_EarSize")},
        {TEXT("ear_angle"), TEXT("DG_Face_EarAngle")}
    };

    // Drive the complete frozen contract every time. Iterating only the keys
    // present in the new profile leaves a previously selected preset stuck on
    // omitted targets when Reset/Cancel returns to a sparse/default map.
    for (const TPair<FName, FName>& Pair : MorphMap)
    {
        const float Value = Current.Face.MorphValues.FindRef(Pair.Key);
        DGSetMorphTargetIfAvailable(
            HeadMesh, Pair.Value, FMath::Clamp(Value, -1.0f, 1.0f));
    }
}

void UDiscGolfCharacterCustomizationComponent::ApplyColorToAllMaterials(
    USkeletalMeshComponent* MeshComp,
    FName ParameterName,
    FLinearColor Value)
{
    if (!MeshComp)
    {
        return;
    }

    const int32 Count = MeshComp->GetNumMaterials();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(Index));
        if (!MID)
        {
            MID = MeshComp->CreateAndSetMaterialInstanceDynamic(Index);
        }
        if (MID)
        {
            MID->SetVectorParameterValue(ParameterName, Value);
        }
    }
}

void UDiscGolfCharacterCustomizationComponent::ApplyScalarToAllMaterials(
    USkeletalMeshComponent* MeshComp,
    FName ParameterName,
    float Value)
{
    if (!MeshComp)
    {
        return;
    }

    const int32 Count = MeshComp->GetNumMaterials();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MeshComp->GetMaterial(Index));
        if (!MID)
        {
            MID = MeshComp->CreateAndSetMaterialInstanceDynamic(Index);
        }
        if (MID)
        {
            MID->SetScalarParameterValue(ParameterName, Value);
        }
    }
}

void UDiscGolfCharacterCustomizationComponent::ApplySkinAndEyeMaterials(
    USkeletalMeshComponent* BodyMesh,
    USkeletalMeshComponent* HeadMesh)
{
    ApplyColorToAllMaterials(BodyMesh, TEXT("DG_SkinTone"), Current.Appearance.SkinTone);
    ApplyColorToAllMaterials(HeadMesh, TEXT("DG_SkinTone"), Current.Appearance.SkinTone);
    ApplyColorToAllMaterials(HeadMesh, TEXT("DG_EyeColor"), Current.Appearance.EyeColor);

    ApplyScalarToAllMaterials(BodyMesh, TEXT("DG_Complexion"), Current.Appearance.Complexion);
    ApplyScalarToAllMaterials(HeadMesh, TEXT("DG_Complexion"), Current.Appearance.Complexion);
    ApplyScalarToAllMaterials(HeadMesh, TEXT("DG_Freckles"), Current.Appearance.Freckles);
    ApplyScalarToAllMaterials(BodyMesh, TEXT("DG_SunExposure"), Current.Appearance.SunExposure);
    ApplyScalarToAllMaterials(HeadMesh, TEXT("DG_SunExposure"), Current.Appearance.SunExposure);
}

void UDiscGolfCharacterCustomizationComponent::ClearHairComponents()
{
    for (UActorComponent* Component : SpawnedHairComponents)
    {
        if (Component)
        {
            Component->DestroyComponent();
        }
    }
    SpawnedHairComponents.Reset();
}

void UDiscGolfCharacterCustomizationComponent::SpawnCosmeticMesh(
    UDiscGolfCosmeticItem* Item,
    USkeletalMeshComponent* HeadMesh,
    FLinearColor Color,
    FName ColorParameterName)
{
    if (!Item || !HeadMesh || !GetOwner())
    {
        return;
    }

    if (!Item->SkeletalMesh.IsNull())
    {
        if (USkeletalMesh* Mesh = Item->SkeletalMesh.LoadSynchronous())
        {
            USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(GetOwner(), NAME_None, RF_Transient);
            Comp->SetupAttachment(HeadMesh, Item->AttachSocket);
            Comp->SetSkeletalMesh(Mesh);
            Comp->SetRelativeTransform(Item->RelativeAttachmentTransform);
            Comp->SetLeaderPoseComponent(HeadMesh);
            Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Comp->SetGenerateOverlapEvents(false);
            Comp->SetCanEverAffectNavigation(false);
            Comp->SetSimulatePhysics(false);
            Comp->ComponentTags.AddUnique(TEXT("DG_CustomizationCosmetic"));
            Comp->ComponentTags.AddUnique(Item->ItemId);
            Comp->RegisterComponent();

            const int32 Count = Comp->GetNumMaterials();
            for (int32 Index = 0; Index < Count; ++Index)
            {
                UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(Index));
                if (!MID)
                {
                    MID = Comp->CreateAndSetMaterialInstanceDynamic(Index);
                }
                if (MID)
                {
                    MID->SetVectorParameterValue(ColorParameterName, Color);
                }
            }

            SpawnedHairComponents.Add(Comp);
        }
        return;
    }

    if (!Item->StaticMesh.IsNull())
    {
        if (UStaticMesh* Mesh = Item->StaticMesh.LoadSynchronous())
        {
            UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
            Comp->SetupAttachment(HeadMesh, Item->AttachSocket);
            Comp->SetStaticMesh(Mesh);
            Comp->SetRelativeTransform(Item->RelativeAttachmentTransform);
            // The accepted master skeleton has a legacy root/socket scale.
            // Accessories follow the head socket's translation/rotation but
            // retain their authored centimeter scale, matching outfit safety.
            Comp->SetAbsolute(false, false, true);
            Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Comp->SetGenerateOverlapEvents(false);
            Comp->SetCanEverAffectNavigation(false);
            Comp->SetSimulatePhysics(false);
            Comp->ComponentTags.AddUnique(TEXT("DG_CustomizationCosmetic"));
            Comp->ComponentTags.AddUnique(Item->ItemId);
            Comp->RegisterComponent();

            const int32 Count = Comp->GetNumMaterials();
            for (int32 Index = 0; Index < Count; ++Index)
            {
                UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(Index));
                if (!MID)
                {
                    MID = Comp->CreateAndSetMaterialInstanceDynamic(Index);
                }
                if (MID)
                {
                    MID->SetVectorParameterValue(ColorParameterName, Color);
                }
            }

            SpawnedHairComponents.Add(Comp);
        }
    }
}

void UDiscGolfCharacterCustomizationComponent::RebuildHair(USkeletalMeshComponent* HeadMesh)
{
    ClearHairComponents();

    if (!CosmeticCatalog || !HeadMesh)
    {
        return;
    }

    if (UDiscGolfCosmeticItem* Hair = CosmeticCatalog->FindById(Current.Hair.HairStyleId))
    {
        SpawnCosmeticMesh(Hair, HeadMesh, Current.Hair.HairColor, TEXT("DG_HairColor"));
    }

    if (UDiscGolfCosmeticItem* Beard = CosmeticCatalog->FindById(Current.Hair.FacialHairId))
    {
        SpawnCosmeticMesh(Beard, HeadMesh, Current.Hair.FacialHairColor, TEXT("DG_HairColor"));
    }

    if (UDiscGolfCosmeticItem* Brow = CosmeticCatalog->FindById(Current.Hair.EyebrowId))
    {
        SpawnCosmeticMesh(Brow, HeadMesh, Current.Hair.EyebrowColor, TEXT("DG_HairColor"));
    }
}

void UDiscGolfCharacterCustomizationComponent::ApplyAll(
    USkeletalMeshComponent* BodyMesh,
    USkeletalMeshComponent* HeadMesh,
    UDiscGolfOutfitComponent* OutfitComponent)
{
    ApplyBodyMorphs(BodyMesh);
    ApplyBodyMorphs(HeadMesh);
    ApplyFaceMorphs(HeadMesh);
    ApplySkinAndEyeMaterials(BodyMesh, HeadMesh);
    RebuildHair(HeadMesh);

    if (OutfitComponent)
    {
        OutfitComponent->ReapplyBodyMorphs(Current.Body);
        OutfitComponent->ApplyLoadout(Current.Outfit, BodyMesh, Current.Body);
    }

    OnCustomizationChanged.Broadcast();
}
