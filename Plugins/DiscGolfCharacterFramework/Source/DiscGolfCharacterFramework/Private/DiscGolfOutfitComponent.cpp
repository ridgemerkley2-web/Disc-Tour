#include "DiscGolfOutfitComponent.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitItem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"

UDiscGolfOutfitComponent::UDiscGolfOutfitComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

static float DGOutfitNormalizeCentered(float Value, float Center, float HalfRange)
{
    if (HalfRange <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    return FMath::Clamp((Value - Center) / HalfRange, -1.0f, 1.0f);
}

void UDiscGolfOutfitComponent::ApplyStandardMorphs(
    USkeletalMeshComponent* MeshComp,
    const FDGBodyProfile& BodyProfile)
{
    if (!MeshComp)
    {
        return;
    }

    // Clothing authored for the DG system can carry these same morph names.
    MeshComp->SetMorphTarget(TEXT("DG_Height"), DGOutfitNormalizeCentered(BodyProfile.HeightCm, 183.0f, 27.0f));
    MeshComp->SetMorphTarget(TEXT("DG_ShoulderWidth"), DGOutfitNormalizeCentered(BodyProfile.ShoulderWidthScale, 1.0f, 0.08f));
    MeshComp->SetMorphTarget(TEXT("DG_TorsoLength"), DGOutfitNormalizeCentered(BodyProfile.TorsoLengthScale, 1.0f, 0.06f));
    MeshComp->SetMorphTarget(TEXT("DG_LegLength"), DGOutfitNormalizeCentered(BodyProfile.LegLengthScale, 1.0f, 0.06f));
    MeshComp->SetMorphTarget(TEXT("DG_HandScale"), DGOutfitNormalizeCentered(BodyProfile.HandScale, 1.0f, 0.06f));
}

void UDiscGolfOutfitComponent::ApplyVariantToMesh(
    USkeletalMeshComponent* MeshComp,
    const FDGOutfitVariant& Variant)
{
    if (!MeshComp)
    {
        return;
    }

    for (int32 Index = 0; Index < Variant.MaterialOverrides.Num(); ++Index)
    {
        if (UMaterialInterface* Mat = Variant.MaterialOverrides[Index].LoadSynchronous())
        {
            MeshComp->SetMaterial(Index, Mat);
        }
    }

    const int32 MaterialCount = MeshComp->GetNumMaterials();
    for (int32 Index = 0; Index < MaterialCount; ++Index)
    {
        if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(Index))
        {
            for (const TPair<FName, FLinearColor>& Pair : Variant.VectorParameters)
            {
                MID->SetVectorParameterValue(Pair.Key, Pair.Value);
            }

            for (const TPair<FName, float>& Pair : Variant.ScalarParameters)
            {
                MID->SetScalarParameterValue(Pair.Key, Pair.Value);
            }
        }
    }
}

void UDiscGolfOutfitComponent::ApplyVariantToMesh(
    UStaticMeshComponent* MeshComp,
    const FDGOutfitVariant& Variant)
{
    if (!MeshComp)
    {
        return;
    }

    for (int32 Index = 0; Index < Variant.MaterialOverrides.Num(); ++Index)
    {
        if (UMaterialInterface* Mat = Variant.MaterialOverrides[Index].LoadSynchronous())
        {
            MeshComp->SetMaterial(Index, Mat);
        }
    }

    const int32 MaterialCount = MeshComp->GetNumMaterials();
    for (int32 Index = 0; Index < MaterialCount; ++Index)
    {
        if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(Index))
        {
            for (const TPair<FName, FLinearColor>& Pair : Variant.VectorParameters)
            {
                MID->SetVectorParameterValue(Pair.Key, Pair.Value);
            }

            for (const TPair<FName, float>& Pair : Variant.ScalarParameters)
            {
                MID->SetScalarParameterValue(Pair.Key, Pair.Value);
            }
        }
    }
}

bool UDiscGolfOutfitComponent::EquipById(
    FName ItemId,
    FName VariantId,
    USkeletalMeshComponent* LeaderBodyMesh,
    const FDGBodyProfile& BodyProfile)
{
    if (!Catalog)
    {
        return false;
    }

    UDiscGolfOutfitItem* Item = Catalog->FindItemById(ItemId);
    if (!Item)
    {
        return false;
    }

    return EquipResolvedItem(Item, VariantId, LeaderBodyMesh, BodyProfile);
}

bool UDiscGolfOutfitComponent::EquipResolvedItem(
    UDiscGolfOutfitItem* Item,
    FName VariantId,
    USkeletalMeshComponent* LeaderBodyMesh,
    const FDGBodyProfile& BodyProfile)
{
    if (!Item || !LeaderBodyMesh || !GetOwner())
    {
        return false;
    }

    if (!Item->SupportsHeight(BodyProfile.HeightCm))
    {
        return false;
    }

    // Resolve declared conflicts before equipping.
    for (EDGOutfitSlot Conflict : Item->ConflictingSlots)
    {
        UnequipSlot(Conflict);
    }

    UnequipSlot(Item->Slot);

    FDGOutfitVariant Variant;
    const bool bHasVariant = Item->FindVariant(VariantId, Variant);
    if (!bHasVariant)
    {
        VariantId = NAME_None;
    }

    if (!Item->SkeletalMesh.IsNull())
    {
        USkeletalMesh* Mesh = Item->SkeletalMesh.LoadSynchronous();
        if (!Mesh)
        {
            return false;
        }

        USkeletalMeshComponent* Comp = NewObject<USkeletalMeshComponent>(
            GetOwner(),
            NAME_None,
            RF_Transient
        );

        Comp->SetupAttachment(LeaderBodyMesh);
        Comp->RegisterComponent();
        Comp->SetSkeletalMesh(Mesh);

        if (Item->bUseLeaderPose)
        {
            Comp->SetLeaderPoseComponent(LeaderBodyMesh);
        }

        ApplyStandardMorphs(Comp, BodyProfile);
        if (bHasVariant)
        {
            ApplyVariantToMesh(Comp, Variant);
        }

        // Cloth binding is intentionally opt-in per item.
        // If the installed engine/item setup supports it, this binds follower cloth to the leader.
        if (Item->bBindClothToLeaderPose && Item->bUseLeaderPose)
        {
            Comp->BindClothToLeaderPoseComponent();
        }

        SkeletalSlotComponents.Add(Item->Slot, Comp);
    }
    else if (!Item->StaticMesh.IsNull())
    {
        UStaticMesh* Mesh = Item->StaticMesh.LoadSynchronous();
        if (!Mesh)
        {
            return false;
        }

        UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(
            GetOwner(),
            NAME_None,
            RF_Transient
        );

        Comp->SetupAttachment(LeaderBodyMesh, Item->AttachSocket);
        Comp->SetRelativeTransform(Item->RelativeAttachmentTransform);
        Comp->RegisterComponent();
        Comp->SetStaticMesh(Mesh);

        if (bHasVariant)
        {
            ApplyVariantToMesh(Comp, Variant);
        }

        StaticSlotComponents.Add(Item->Slot, Comp);
    }
    else
    {
        return false;
    }

    EquippedItems.Add(Item->Slot, Item);
    UpdateLoadoutEntry(Item->Slot, Item->ItemId, VariantId);
    OnOutfitSlotChanged.Broadcast(Item->Slot, Item->ItemId);
    BroadcastCoverage();
    return true;
}

void UDiscGolfOutfitComponent::UnequipSlot(EDGOutfitSlot Slot)
{
    if (TObjectPtr<USkeletalMeshComponent>* Found = SkeletalSlotComponents.Find(Slot))
    {
        if (USkeletalMeshComponent* Comp = Found->Get())
        {
            Comp->DestroyComponent();
        }
        SkeletalSlotComponents.Remove(Slot);
    }

    if (TObjectPtr<UStaticMeshComponent>* Found = StaticSlotComponents.Find(Slot))
    {
        if (UStaticMeshComponent* Comp = Found->Get())
        {
            Comp->DestroyComponent();
        }
        StaticSlotComponents.Remove(Slot);
    }

    EquippedItems.Remove(Slot);

    CurrentLoadout.Equipped.RemoveAll(
        [Slot](const FDGEquippedOutfitEntry& Entry)
        {
            return Entry.Slot == Slot;
        }
    );

    OnOutfitSlotChanged.Broadcast(Slot, NAME_None);
    BroadcastCoverage();
}

void UDiscGolfOutfitComponent::ClearOutfit()
{
    TArray<EDGOutfitSlot> Slots;

    for (const TPair<EDGOutfitSlot, TObjectPtr<UDiscGolfOutfitItem>>& Pair : EquippedItems)
    {
        Slots.Add(Pair.Key);
    }

    for (EDGOutfitSlot Slot : Slots)
    {
        UnequipSlot(Slot);
    }

    CurrentLoadout.Equipped.Reset();
    BroadcastCoverage();
}

bool UDiscGolfOutfitComponent::ApplyLoadout(
    const FDGOutfitLoadout& Loadout,
    USkeletalMeshComponent* LeaderBodyMesh,
    const FDGBodyProfile& BodyProfile)
{
    if (!Catalog || !LeaderBodyMesh)
    {
        return false;
    }

    ClearOutfit();

    bool bAllSucceeded = true;
    for (const FDGEquippedOutfitEntry& Entry : Loadout.Equipped)
    {
        UDiscGolfOutfitItem* Item = Catalog->FindItemById(Entry.ItemId);
        if (!Item || Item->Slot != Entry.Slot ||
            !EquipResolvedItem(Item, Entry.VariantId, LeaderBodyMesh, BodyProfile))
        {
            bAllSucceeded = false;
        }
    }

    return bAllSucceeded;
}

void UDiscGolfOutfitComponent::ReapplyBodyMorphs(const FDGBodyProfile& BodyProfile)
{
    for (const TPair<EDGOutfitSlot, TObjectPtr<USkeletalMeshComponent>>& Pair : SkeletalSlotComponents)
    {
        ApplyStandardMorphs(Pair.Value.Get(), BodyProfile);
    }
}

TArray<EDGBodyRegion> UDiscGolfOutfitComponent::GetCoveredBodyRegions() const
{
    TArray<EDGBodyRegion> Result;

    for (const TPair<EDGOutfitSlot, TObjectPtr<UDiscGolfOutfitItem>>& Pair : EquippedItems)
    {
        const UDiscGolfOutfitItem* Item = Pair.Value.Get();
        if (!Item)
        {
            continue;
        }

        for (EDGBodyRegion Region : Item->CoveredBodyRegions)
        {
            Result.AddUnique(Region);
        }
    }

    return Result;
}

void UDiscGolfOutfitComponent::UpdateLoadoutEntry(
    EDGOutfitSlot Slot,
    FName ItemId,
    FName VariantId)
{
    for (FDGEquippedOutfitEntry& Entry : CurrentLoadout.Equipped)
    {
        if (Entry.Slot == Slot)
        {
            Entry.ItemId = ItemId;
            Entry.VariantId = VariantId;
            return;
        }
    }

    FDGEquippedOutfitEntry NewEntry;
    NewEntry.Slot = Slot;
    NewEntry.ItemId = ItemId;
    NewEntry.VariantId = VariantId;
    CurrentLoadout.Equipped.Add(NewEntry);
}

void UDiscGolfOutfitComponent::BroadcastCoverage()
{
    OnBodyCoverageChanged.Broadcast(GetCoveredBodyRegions());
}
