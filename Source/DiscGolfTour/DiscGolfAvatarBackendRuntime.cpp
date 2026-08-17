#include "DiscGolfAvatarBackendRuntime.h"

#include "DiscGolfAvatarBackendProfile.h"

namespace
{
bool Session8ContainsQualityProfile(FName QualityProfileId)
{
    return DiscGolfAvatarBackendRuntime::GetQualityProfileIds().Contains(QualityProfileId);
}
}

const TArray<FName>& DiscGolfAvatarBackendRuntime::GetQualityProfileIds()
{
    static const TArray<FName> QualityProfileIds = {
        TEXT("Prototype"),
        TEXT("GameplayPerformance"),
        TEXT("GameplayHigh"),
        TEXT("Showcase")
    };
    return QualityProfileIds;
}

bool DiscGolfAvatarBackendRuntime::ValidateMetaHumanProfileContract(
    const UDiscGolfAvatarBackendProfile* Profile,
    FString& OutReason)
{
    OutReason.Reset();
    if (!Profile)
    {
        OutReason = TEXT("MetaHuman backend profile is missing.");
        return false;
    }
    if (Profile->BackendId != FName(MetaHumanAssembledBackendId)
        || Profile->Backend != EDGAvatarBackend::MetaHumanPreset)
    {
        OutReason = TEXT("MetaHuman backend profile identity is not canonical.");
        return false;
    }
    if (Profile->MetaHumanRuntimeMode
        != EDGMetaHumanRuntimeMode::ShippingSafeAssembled)
    {
        OutReason = TEXT("Experimental MetaHuman collection/instance mode is not accepted.");
        return false;
    }
    if (Profile->VisualActorClass.IsNull())
    {
        OutReason = TEXT("Assembled MetaHuman actor class is missing.");
        return false;
    }
    if (!Profile->bUseRuntimeRetargeting || Profile->RetargetAsset.IsNull())
    {
        OutReason = TEXT("DG-to-MetaHuman retarget configuration is missing.");
        return false;
    }
    if (Profile->VisualBodyComponentTag.IsNone()
        || Profile->VisualHeadComponentTag.IsNone()
        || Profile->VisualBodyComponentTag == Profile->VisualHeadComponentTag)
    {
        OutReason = TEXT("Distinct visual body/head component tags are required.");
        return false;
    }
    if (!Session8ContainsQualityProfile(Profile->PreferredQualityProfileId))
    {
        OutReason = TEXT("MetaHuman quality profile ID is not in the frozen contract.");
        return false;
    }
    if (Profile->bAllowRuntimeFaceSculpting)
    {
        OutReason = TEXT("Runtime MetaHuman sculpting is not accepted by this adapter.");
        return false;
    }

    OutReason = TEXT("Profile metadata permits a verified assembled-backend attempt.");
    return true;
}

FDiscGolfAvatarBackendResolution DiscGolfAvatarBackendRuntime::ResolveBackend(
    FName RequestedBackendId,
    const UDiscGolfAvatarBackendProfile* MetaHumanProfile)
{
    FDiscGolfAvatarBackendResolution Result;
    Result.RequestedBackendId = RequestedBackendId.IsNone()
        ? FName(DGMasterBackendId) : RequestedBackendId;

    if (Result.RequestedBackendId != FName(MetaHumanAssembledBackendId))
    {
        Result.Status = Result.RequestedBackendId == FName(DGMasterBackendId)
            ? TEXT("DG master proxy backend selected.")
            : TEXT("Unknown backend ID; using the DG master proxy fallback.");
        return Result;
    }

    FString ProfileReason;
    if (!ValidateMetaHumanProfileContract(MetaHumanProfile, ProfileReason))
    {
        Result.Status = FString::Printf(
            TEXT("MetaHuman backend unavailable; using DG master fallback: %s"),
            *ProfileReason);
        return Result;
    }

    Result.ResolvedBackendId = FName(MetaHumanAssembledBackendId);
    Result.ResolvedBackend = EDGAvatarBackend::MetaHumanPreset;
    Result.bMetaHumanAttemptAllowed = true;
    Result.Status = ProfileReason;
    return Result;
}
