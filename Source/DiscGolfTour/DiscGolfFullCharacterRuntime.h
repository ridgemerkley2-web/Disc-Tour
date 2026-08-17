#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCosmeticItem.h"
#include "DiscGolfCustomizationTypes.h"
#include "DiscGolfFullCharacterRuntime.generated.h"

class UDiscGolfCosmeticCatalog;
class UDiscGolfOutfitCatalog;

/** Catalog-backed option exposed by the one native character creator. */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfCosmeticOption
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Character Creator|Cosmetic")
    FName ItemId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Character Creator|Cosmetic")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category="Character Creator|Cosmetic")
    EDGCosmeticKind Kind = EDGCosmeticKind::Hair;

    UPROPERTY(BlueprintReadOnly, Category="Character Creator|Cosmetic")
    bool bCompatible = false;

    UPROPERTY(BlueprintReadOnly, Category="Character Creator|Cosmetic")
    FString CompatibilityReason;
};

/** Transient creator locks. They are UI state, not a second saved character payload. */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfCharacterRandomizeLocks
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bIdentity = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bBody = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bFace = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bHair = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bAppearance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bThrowStyle = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character Creator|Randomize")
    bool bOutfit = false;
};

/** Result of resolving stable cosmetic and outfit IDs against installed catalogs. */
struct DISCGOLFTOUR_API FDiscGolfFullCustomizationResolution
{
    FDGFullCharacterCustomization Character;
    TArray<FString> Warnings;
    bool bCosmeticCatalogAvailable = false;
    bool bAllCosmeticsResolved = true;
};

/**
 * Project boundary around the installed framework's FDGFullCharacterCustomization.
 * This owns sanitation, presets, stable-ID fallback and deterministic randomization;
 * it does not own a pawn, save slot, animation, release, inventory or flight.
 */
namespace DiscGolfFullCharacterRuntime
{
    inline constexpr const TCHAR* CosmeticCatalogObjectPath =
        TEXT("/Game/DiscGolf/Characters/Customization/Data/DA_DG_CosmeticCatalog.DA_DG_CosmeticCatalog");
    inline constexpr const TCHAR* HeadMeshObjectPath =
        TEXT("/Game/DiscGolf/Characters/Customization/Head/SK_DG_Head_Proxy.SK_DG_Head_Proxy");
    inline constexpr const TCHAR* HeadMaterialObjectPath =
        TEXT("/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HeadProxy.M_DG_HeadProxy");
    inline constexpr const TCHAR* HairMaterialObjectPath =
        TEXT("/Game/DiscGolf/Materials/CharacterCustomization/M_DG_HairProxy.M_DG_HairProxy");

    inline constexpr int32 MaximumDisplayNameLength = 32;

    DISCGOLFTOUR_API const TArray<FName>& GetFaceMorphKeys();
    DISCGOLFTOUR_API const TArray<FName>& GetVisibleProxyFaceMorphKeys();
    DISCGOLFTOUR_API FText GetFaceMorphDisplayName(FName MorphKey);

    DISCGOLFTOUR_API FDGFullCharacterCustomization MakeDefaultCustomization();
    DISCGOLFTOUR_API void NormalizeForPersistence(
        FDGFullCharacterCustomization& InOutCharacter);
    DISCGOLFTOUR_API bool AreCustomizationsEquivalent(
        const FDGFullCharacterCustomization& A,
        const FDGFullCharacterCustomization& B);

    /** Presets initialize all twenty values; later slider edits remain authoritative. */
    DISCGOLFTOUR_API bool ApplyFacePreset(FName PresetId, FDGFaceProfile& InOutFace);

    DISCGOLFTOUR_API FDiscGolfFullCustomizationResolution ResolveForRuntime(
        const FDGFullCharacterCustomization& Requested,
        const UDiscGolfCosmeticCatalog* CosmeticCatalog,
        const UDiscGolfOutfitCatalog* OutfitCatalog);

    DISCGOLFTOUR_API TArray<FDiscGolfCosmeticOption> GetCosmeticOptions(
        const UDiscGolfCosmeticCatalog* Catalog,
        EDGCosmeticKind Kind,
        float HeightCm);

    DISCGOLFTOUR_API bool SetCosmeticSelection(
        FDGFullCharacterCustomization& InOutCharacter,
        EDGCosmeticKind Kind,
        FName ItemId,
        const UDiscGolfCosmeticCatalog* Catalog,
        FString& OutStatus);

    DISCGOLFTOUR_API FDGFullCharacterCustomization Randomize(
        const FDGFullCharacterCustomization& Current,
        const FDiscGolfCharacterRandomizeLocks& Locks,
        const UDiscGolfCosmeticCatalog* CosmeticCatalog,
        const UDiscGolfOutfitCatalog* OutfitCatalog,
        FRandomStream& Random);
}
