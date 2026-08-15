#include "DiscEquipmentDataAssets.h"

FPrimaryAssetId UDiscMoldDataAsset::GetPrimaryAssetId() const
{
    return Mold.MoldId.IsNone()
        ? FPrimaryAssetId()
        : FPrimaryAssetId(TEXT("DiscMold"), Mold.MoldId);
}

FPrimaryAssetId UDiscPlasticDataAsset::GetPrimaryAssetId() const
{
    return PlasticDefinition.PlasticId.IsNone()
        ? FPrimaryAssetId()
        : FPrimaryAssetId(TEXT("DiscPlastic"), PlasticDefinition.PlasticId);
}
