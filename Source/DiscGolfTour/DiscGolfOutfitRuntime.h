#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfOutfitTypes.h"
#include "DiscGolfOutfitRuntime.generated.h"

class UDiscGolfOutfitCatalog;

/** Lightweight, catalog-backed view model used by the native creator UI. */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfOutfitOption
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Outfit")
    FName ItemId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Outfit")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Outfit")
    EDGOutfitSlot Slot = EDGOutfitSlot::Top;

    UPROPERTY(BlueprintReadOnly, Category="Outfit")
    TArray<FName> VariantIds;

    UPROPERTY(BlueprintReadOnly, Category="Outfit")
    bool bCompatible = false;

    UPROPERTY(BlueprintReadOnly, Category="Outfit")
    FString CompatibilityReason;
};

/** Result of resolving asset-independent saved IDs against the installed catalog. */
struct DISCGOLFTOUR_API FDiscGolfOutfitResolution
{
    FDGOutfitLoadout Loadout;
    TArray<FString> Warnings;
    bool bCatalogAvailable = false;
    bool bAllEntriesResolved = true;
};

namespace DiscGolfOutfitRuntime
{
#if DG_WITH_DEVELOPMENT_CONTENT
    inline constexpr const TCHAR* CatalogObjectPath =
        TEXT("/Game/DiscGolf/Outfits/Data/DA_DG_OutfitCatalog.DA_DG_OutfitCatalog");
#else
    inline constexpr const TCHAR* CatalogObjectPath = TEXT("");
#endif

    /** Frozen v1 order. EDGOutfitSlot is serialized as an enum byte, so additions must be append-only. */
    DISCGOLFTOUR_API const TArray<EDGOutfitSlot>& GetOrderedSlots();
    DISCGOLFTOUR_API FText GetSlotDisplayName(EDGOutfitSlot Slot);

    DISCGOLFTOUR_API const FDGEquippedOutfitEntry* FindEntryForSlot(
        const FDGOutfitLoadout& Loadout,
        EDGOutfitSlot Slot);

    /** Structural normalization only: one non-empty entry per slot in frozen slot order. */
    DISCGOLFTOUR_API FDGOutfitLoadout NormalizeForPersistence(
        const FDGOutfitLoadout& Loadout);

    /**
     * Replaces one draft slot. The newly selected item wins any reciprocal declared conflicts.
     * ItemId=None is the stable unequipped representation.
     */
    DISCGOLFTOUR_API bool SetSlotSelection(
        FDGOutfitLoadout& InOutLoadout,
        EDGOutfitSlot Slot,
        FName ItemId,
        FName VariantId,
        const UDiscGolfOutfitCatalog* Catalog,
        const FDGBodyProfile& BodyProfile,
        FString& OutStatus);

    /** Resolves stable IDs, canonical variants, compatibility, conflicts, and missing items. */
    DISCGOLFTOUR_API FDiscGolfOutfitResolution ResolveCanonicalLoadout(
        const FDGOutfitLoadout& Requested,
        const UDiscGolfOutfitCatalog* Catalog,
        const FDGBodyProfile& BodyProfile);

    DISCGOLFTOUR_API bool AreLoadoutsEquivalent(
        const FDGOutfitLoadout& A,
        const FDGOutfitLoadout& B);

    DISCGOLFTOUR_API TArray<FDiscGolfOutfitOption> GetOptionsForSlot(
        const UDiscGolfOutfitCatalog* Catalog,
        EDGOutfitSlot Slot,
        const FDGBodyProfile& BodyProfile);
}
