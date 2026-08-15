#include "DiscBagComponent.h"

UDiscBagComponent::UDiscBagComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MoldIds = { TEXT("Apex"), TEXT("Vector"), TEXT("Line"), TEXT("Compass"), TEXT("Touch") };
}

void UDiscBagComponent::SelectDiscIndex(int32 NewIndex)
{
    if (MoldIds.Num() == 0) return;
    SelectedIndex = FMath::Clamp(NewIndex, 0, MoldIds.Num() - 1);
}

bool UDiscBagComponent::SelectEquipment(FName MoldId, EDiscPlastic Plastic)
{
    const int32 MoldIndex = MoldIds.IndexOfByKey(MoldId);
    const bool bValidPlastic = Plastic == EDiscPlastic::Base
        || Plastic == EDiscPlastic::Tour
        || Plastic == EDiscPlastic::Crystal;
    if (MoldIndex == INDEX_NONE || !bValidPlastic) return false;
    SelectedIndex = MoldIndex;
    SelectedPlastic = Plastic;
    return true;
}

void UDiscBagComponent::CyclePlastic()
{
    switch (SelectedPlastic)
    {
        case EDiscPlastic::Base: SelectedPlastic = EDiscPlastic::Tour; break;
        case EDiscPlastic::Tour: SelectedPlastic = EDiscPlastic::Crystal; break;
        case EDiscPlastic::Crystal: SelectedPlastic = EDiscPlastic::Base; break;
        default: SelectedPlastic = EDiscPlastic::Tour; break;
    }
}

FName UDiscBagComponent::GetSelectedMoldId() const
{
    return MoldIds.IsValidIndex(SelectedIndex) ? MoldIds[SelectedIndex] : NAME_None;
}
