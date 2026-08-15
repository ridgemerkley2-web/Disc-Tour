#include "DiscGolfDiscCatalog.h"
#include "DiscGolfDiscDefinition.h"
#include "DiscGolfPlasticDefinition.h"

UDiscGolfDiscDefinition* UDiscGolfDiscCatalog::FindDiscById(FName DiscId) const
{
    if (DiscId.IsNone())
    {
        return nullptr;
    }

    for (const TSoftObjectPtr<UDiscGolfDiscDefinition>& SoftDisc : DiscDefinitions)
    {
        if (UDiscGolfDiscDefinition* Disc = SoftDisc.LoadSynchronous())
        {
            if (Disc->DiscId == DiscId)
            {
                return Disc;
            }
        }
    }

    return nullptr;
}

UDiscGolfPlasticDefinition* UDiscGolfDiscCatalog::FindPlasticById(FName PlasticId) const
{
    if (PlasticId.IsNone())
    {
        return nullptr;
    }

    for (const TSoftObjectPtr<UDiscGolfPlasticDefinition>& SoftPlastic : PlasticDefinitions)
    {
        if (UDiscGolfPlasticDefinition* Plastic = SoftPlastic.LoadSynchronous())
        {
            if (Plastic->PlasticId == PlasticId)
            {
                return Plastic;
            }
        }
    }

    return nullptr;
}
