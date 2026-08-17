#pragma once

#include "CoreMinimal.h"
#include "DiscGolfAvatarBackendTypes.h"
#include "DiscGolfAvatarBackendRuntime.generated.h"

class UDiscGolfAvatarBackendProfile;

/**
 * Read-only result of resolving a stable visual-backend request.
 * Gameplay, animation, release, inventory and flight remain owned by the DG pawn.
 */
USTRUCT(BlueprintType)
struct DISCGOLFTOUR_API FDiscGolfAvatarBackendResolution
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Avatar")
    FName RequestedBackendId = TEXT("dg_master");

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Avatar")
    FName ResolvedBackendId = TEXT("dg_master");

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Avatar")
    EDGAvatarBackend ResolvedBackend = EDGAvatarBackend::DGMaster;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Avatar")
    bool bMetaHumanAttemptAllowed = false;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Avatar")
    FString Status;
};

/**
 * Project policy for the optional visual backend. This layer resolves stable IDs
 * and validates profile metadata only; successful actor/retarget verification is
 * still required before the proxy presentation may be hidden.
 */
namespace DiscGolfAvatarBackendRuntime
{
    inline constexpr const TCHAR* DGMasterBackendId = TEXT("dg_master");
    inline constexpr const TCHAR* MetaHumanAssembledBackendId =
        TEXT("metahuman_assembled");

    inline constexpr const TCHAR* DGMasterProfileObjectPath =
        TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_DGMaster.DA_DG_AvatarBackend_DGMaster");
    inline constexpr const TCHAR* MetaHumanDefaultProfileObjectPath =
        TEXT("/Game/DiscGolf/Characters/Avatar/Data/DA_DG_AvatarBackend_MetaHuman_Default.DA_DG_AvatarBackend_MetaHuman_Default");

    /** Exact quality-profile vocabulary prepared by the BuildKit contract. */
    DISCGOLFTOUR_API const TArray<FName>& GetQualityProfileIds();

    /**
     * Validates only the data contract needed to attempt an assembled MetaHuman.
     * It never treats non-null soft paths as proof that the visual is runtime-ready.
     */
    DISCGOLFTOUR_API bool ValidateMetaHumanProfileContract(
        const UDiscGolfAvatarBackendProfile* Profile,
        FString& OutReason);

    /**
     * Unknown, absent or incomplete requests fail closed to the DG master.
     * A MetaHuman result only authorizes an attempt; the component must separately
     * verify the spawned body, head and retarget configuration.
     */
    DISCGOLFTOUR_API FDiscGolfAvatarBackendResolution ResolveBackend(
        FName RequestedBackendId,
        const UDiscGolfAvatarBackendProfile* MetaHumanProfile);
}
