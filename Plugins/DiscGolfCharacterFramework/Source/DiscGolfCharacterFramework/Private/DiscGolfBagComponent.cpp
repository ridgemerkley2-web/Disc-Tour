#include "DiscGolfBagComponent.h"
#include "DiscGolfDiscCatalog.h"
#include "DiscGolfDiscDefinition.h"
#include "DiscGolfPlasticDefinition.h"

UDiscGolfBagComponent::UDiscGolfBagComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

FDGDiscInstance UDiscGolfBagComponent::CreateDiscInstance(
    FName DiscDefinitionId,
    FName PlasticId,
    float MassGrams,
    FLinearColor Color,
    FName StampId) const
{
    FDGDiscInstance Disc;
    Disc.InstanceId = FGuid::NewGuid();
    Disc.DiscDefinitionId = DiscDefinitionId;
    Disc.PlasticId = PlasticId;
    Disc.MassGrams = MassGrams;
    Disc.Color = Color;
    Disc.StampId = StampId.IsNone() ? TEXT("premium_default") : StampId;

    if (Catalog)
    {
        if (const UDiscGolfDiscDefinition* Definition = Catalog->FindDiscById(DiscDefinitionId))
        {
            Disc.MassGrams = FMath::Clamp(MassGrams, Definition->MinMassGrams, Definition->MaxMassGrams);
            if (StampId.IsNone())
            {
                Disc.StampId = Definition->DefaultStampId;
            }
        }
    }

    return Disc;
}

bool UDiscGolfBagComponent::CanAddDisc() const
{
    return CurrentBag.Discs.Num() < FMath::Max(0, CurrentBag.Capacity);
}

bool UDiscGolfBagComponent::AddDisc(const FDGDiscInstance& Disc)
{
    if (!CanAddDisc() || !Disc.InstanceId.IsValid())
    {
        return false;
    }

    if (Catalog)
    {
        if (!Catalog->FindDiscById(Disc.DiscDefinitionId) ||
            !Catalog->FindPlasticById(Disc.PlasticId))
        {
            return false;
        }
    }

    for (const FDGDiscInstance& Existing : CurrentBag.Discs)
    {
        if (Existing.InstanceId == Disc.InstanceId)
        {
            return false;
        }
    }

    CurrentBag.Discs.Add(Disc);

    if (!CurrentBag.SelectedDiscInstanceId.IsValid())
    {
        CurrentBag.SelectedDiscInstanceId = Disc.InstanceId;
        OnSelectedDiscChanged.Broadcast(Disc);
    }

    OnBagChanged.Broadcast();
    return true;
}

bool UDiscGolfBagComponent::RemoveDisc(FGuid InstanceId)
{
    const int32 Removed = CurrentBag.Discs.RemoveAll(
        [InstanceId](const FDGDiscInstance& Disc)
        {
            return Disc.InstanceId == InstanceId;
        });

    if (Removed <= 0)
    {
        return false;
    }

    if (CurrentBag.SelectedDiscInstanceId == InstanceId)
    {
        CurrentBag.SelectedDiscInstanceId.Invalidate();

        if (CurrentBag.Discs.Num() > 0)
        {
            CurrentBag.SelectedDiscInstanceId = CurrentBag.Discs[0].InstanceId;
            OnSelectedDiscChanged.Broadcast(CurrentBag.Discs[0]);
        }
    }

    OnBagChanged.Broadcast();
    return true;
}

bool UDiscGolfBagComponent::SelectDisc(FGuid InstanceId)
{
    for (const FDGDiscInstance& Disc : CurrentBag.Discs)
    {
        if (Disc.InstanceId == InstanceId)
        {
            CurrentBag.SelectedDiscInstanceId = InstanceId;
            OnSelectedDiscChanged.Broadcast(Disc);
            OnBagChanged.Broadcast();
            return true;
        }
    }

    return false;
}

bool UDiscGolfBagComponent::GetSelectedDisc(FDGDiscInstance& OutDisc) const
{
    for (const FDGDiscInstance& Disc : CurrentBag.Discs)
    {
        if (Disc.InstanceId == CurrentBag.SelectedDiscInstanceId)
        {
            OutDisc = Disc;
            return true;
        }
    }

    return false;
}

bool UDiscGolfBagComponent::SetWear(FGuid InstanceId, float Wear01)
{
    for (FDGDiscInstance& Disc : CurrentBag.Discs)
    {
        if (Disc.InstanceId == InstanceId)
        {
            Disc.Wear01 = FMath::Clamp(Wear01, 0.0f, 1.0f);
            OnBagChanged.Broadcast();
            return true;
        }
    }

    return false;
}

void UDiscGolfBagComponent::SetLoadout(const FDGDiscBagLoadout& NewLoadout)
{
    CurrentBag = NewLoadout;
    CurrentBag.Capacity = FMath::Max(0, CurrentBag.Capacity);

    if (CurrentBag.Discs.Num() > CurrentBag.Capacity)
    {
        CurrentBag.Discs.SetNum(CurrentBag.Capacity);
    }

    FDGDiscInstance Selected;
    if (!GetSelectedDisc(Selected))
    {
        CurrentBag.SelectedDiscInstanceId.Invalidate();
        if (CurrentBag.Discs.Num() > 0)
        {
            CurrentBag.SelectedDiscInstanceId = CurrentBag.Discs[0].InstanceId;
            Selected = CurrentBag.Discs[0];
            OnSelectedDiscChanged.Broadcast(Selected);
        }
    }

    OnBagChanged.Broadcast();
}
