#include "DiscGolfFullCharacterRuntime.h"

#include "DiscGolfCharacterProfileRuntime.h"
#include "DiscGolfCosmeticCatalog.h"
#include "DiscGolfOutfitCatalog.h"
#include "DiscGolfOutfitRuntime.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Misc/Char.h"

namespace
{
float SanitizeFullCharacterFinite(
    float Value,
    float DefaultValue,
    float MinValue,
    float MaxValue)
{
    return FMath::IsFinite(Value)
        ? FMath::Clamp(Value, MinValue, MaxValue)
        : DefaultValue;
}

FLinearColor SanitizeColor(FLinearColor Value, const FLinearColor& DefaultValue)
{
    if (!FMath::IsFinite(Value.R) || !FMath::IsFinite(Value.G)
        || !FMath::IsFinite(Value.B) || !FMath::IsFinite(Value.A))
    {
        return DefaultValue;
    }
    Value.R = FMath::Clamp(Value.R, 0.0f, 1.0f);
    Value.G = FMath::Clamp(Value.G, 0.0f, 1.0f);
    Value.B = FMath::Clamp(Value.B, 0.0f, 1.0f);
    Value.A = 1.0f;
    return Value;
}

bool ColorsExactlyMatch(const FLinearColor& A, const FLinearColor& B)
{
    return A.R == B.R && A.G == B.G && A.B == B.B && A.A == B.A;
}

bool BodyExactlyMatches(const FDGBodyProfile& A, const FDGBodyProfile& B)
{
    return A.HeightCm == B.HeightCm
        && A.WingspanScale == B.WingspanScale
        && A.ShoulderWidthScale == B.ShoulderWidthScale
        && A.TorsoLengthScale == B.TorsoLengthScale
        && A.LegLengthScale == B.LegLengthScale
        && A.HandScale == B.HandScale
        && A.MassKg == B.MassKg;
}

bool BodyBuildExactlyMatches(const FDGBodyBuildProfile& A, const FDGBodyBuildProfile& B)
{
    return A.Muscularity == B.Muscularity
        && A.BodyFat == B.BodyFat
        && A.Chest == B.Chest
        && A.Waist == B.Waist
        && A.Hips == B.Hips
        && A.Arms == B.Arms
        && A.Legs == B.Legs;
}

bool ThrowStyleExactlyMatches(const FDGThrowStyle& A, const FDGThrowStyle& B)
{
    return A.RunUpIntensity == B.RunUpIntensity
        && A.ReachBackAmount == B.ReachBackAmount
        && A.TorsoRotation == B.TorsoRotation
        && A.BraceIntensity == B.BraceIntensity
        && A.Explosiveness == B.Explosiveness
        && A.FollowThrough == B.FollowThrough
        && A.PowerMultiplier == B.PowerMultiplier
        && A.SpinMultiplier == B.SpinMultiplier;
}

bool MorphMapsExactlyMatch(const TMap<FName, float>& A, const TMap<FName, float>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (const TPair<FName, float>& Pair : A)
    {
        const float* Other = B.Find(Pair.Key);
        if (!Other || *Other != Pair.Value)
        {
            return false;
        }
    }
    return true;
}

bool NameArraysExactlyMatch(const TArray<FName>& A, const TArray<FName>& B)
{
    if (A.Num() != B.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < A.Num(); ++Index)
    {
        if (A[Index] != B[Index])
        {
            return false;
        }
    }
    return true;
}

FName DefaultIdForKind(EDGCosmeticKind Kind)
{
    switch (Kind)
    {
        case EDGCosmeticKind::Hair: return TEXT("hair_none");
        case EDGCosmeticKind::FacialHair: return TEXT("facialhair_none");
        case EDGCosmeticKind::Eyebrow: return TEXT("brow_default");
        case EDGCosmeticKind::Scar: return TEXT("scar_none");
        case EDGCosmeticKind::Tattoo: return TEXT("tattoo_none");
        case EDGCosmeticKind::Voice: return TEXT("voice_default");
        case EDGCosmeticKind::PronounSet: return TEXT("pronouns_default");
        default: return NAME_None;
    }
}

bool IsNoneCosmetic(EDGCosmeticKind Kind, FName ItemId)
{
    return (Kind == EDGCosmeticKind::Hair && ItemId == FName(TEXT("hair_none")))
        || (Kind == EDGCosmeticKind::FacialHair && ItemId == FName(TEXT("facialhair_none")))
        || (Kind == EDGCosmeticKind::Scar && ItemId == FName(TEXT("scar_none")))
        || (Kind == EDGCosmeticKind::Tattoo && ItemId == FName(TEXT("tattoo_none")));
}

bool IsRenderableCatalogItem(
    const UDiscGolfCosmeticItem* Item,
    EDGCosmeticKind ExpectedKind,
    float HeightCm,
    FString& OutReason)
{
    OutReason.Reset();
    if (!Item || Item->ItemId.IsNone() || Item->Kind != ExpectedKind)
    {
        OutReason = TEXT("catalog kind or stable ID is invalid");
        return false;
    }
    if (!FMath::IsFinite(HeightCm)
        || HeightCm < Item->MinHeightCm || HeightCm > Item->MaxHeightCm)
    {
        OutReason = FString::Printf(
            TEXT("supports %.0f-%.0f cm"), Item->MinHeightCm, Item->MaxHeightCm);
        return false;
    }

    if (ExpectedKind == EDGCosmeticKind::Hair
        || ExpectedKind == EDGCosmeticKind::FacialHair
        || ExpectedKind == EDGCosmeticKind::Eyebrow)
    {
        if (IsNoneCosmetic(ExpectedKind, Item->ItemId))
        {
            if (Item->SkeletalMesh.IsNull() && Item->StaticMesh.IsNull())
            {
                return true;
            }
            OutReason = TEXT("none entry must not reference a proxy mesh");
            return false;
        }
        const bool bHasSkeletalReference = !Item->SkeletalMesh.IsNull();
        const bool bHasStaticReference = !Item->StaticMesh.IsNull();
        if (bHasSkeletalReference == bHasStaticReference)
        {
            OutReason = TEXT("requires exactly one proxy mesh reference");
            return false;
        }
        const bool bSelectedMeshLoads = bHasSkeletalReference
            ? Item->SkeletalMesh.LoadSynchronous() != nullptr
            : Item->StaticMesh.LoadSynchronous() != nullptr;
        if (!bSelectedMeshLoads)
        {
            OutReason = TEXT("the selected proxy mesh is missing");
            return false;
        }
    }
    else if ((ExpectedKind == EDGCosmeticKind::Scar
            || ExpectedKind == EDGCosmeticKind::Tattoo)
        && !IsNoneCosmetic(ExpectedKind, Item->ItemId)
        && Item->MaterialVariantId.IsNone())
    {
        OutReason = TEXT("marking hook is unavailable");
        return false;
    }
    return true;
}

bool ResolveId(
    FName Requested,
    EDGCosmeticKind Kind,
    float HeightCm,
    const UDiscGolfCosmeticCatalog* Catalog,
    FName& OutResolved,
    FString& OutWarning)
{
    OutWarning.Reset();
    const FName Fallback = DefaultIdForKind(Kind);
    OutResolved = Requested.IsNone() ? Fallback : Requested;
    if (!Catalog)
    {
        OutResolved = Fallback;
        OutWarning = FString::Printf(
            TEXT("Cosmetic catalog unavailable; %s fell back to %s."),
            *Requested.ToString(), *Fallback.ToString());
        return false;
    }

    FString Reason;
    const UDiscGolfCosmeticItem* RequestedItem = Catalog->FindById(OutResolved);
    if (IsRenderableCatalogItem(RequestedItem, Kind, HeightCm, Reason))
    {
        return true;
    }

    const FName Missing = OutResolved;
    OutResolved = Fallback;
    const UDiscGolfCosmeticItem* FallbackItem = Catalog->FindById(Fallback);
    FString FallbackReason;
    const bool bFallbackValid = IsRenderableCatalogItem(
        FallbackItem, Kind, HeightCm, FallbackReason);
    OutWarning = FString::Printf(
        TEXT("Missing or incompatible %s cosmetic %s; using %s%s."),
        *UEnum::GetValueAsString(Kind),
        *Missing.ToString(),
        *Fallback.ToString(),
        bFallbackValid ? TEXT("") : TEXT(" (visual fallback is also unavailable)"));
    return false;
}

FLinearColor PickColor(FRandomStream& Random, TConstArrayView<FLinearColor> Palette)
{
    return Palette[Random.RandRange(0, Palette.Num() - 1)];
}

template <typename OptionType>
const OptionType* PickOption(FRandomStream& Random, const TArray<OptionType>& Options)
{
    return Options.IsEmpty() ? nullptr : &Options[Random.RandRange(0, Options.Num() - 1)];
}
}

const TArray<FName>& DiscGolfFullCharacterRuntime::GetFaceMorphKeys()
{
    static const TArray<FName> Keys = {
        TEXT("head_width"), TEXT("head_height"), TEXT("brow_height"),
        TEXT("brow_depth"), TEXT("eye_size"), TEXT("eye_spacing"),
        TEXT("eye_depth"), TEXT("nose_width"), TEXT("nose_length"),
        TEXT("nose_bridge"), TEXT("cheek_width"), TEXT("cheek_fullness"),
        TEXT("jaw_width"), TEXT("jaw_height"), TEXT("chin_width"),
        TEXT("chin_length"), TEXT("mouth_width"), TEXT("lip_fullness"),
        TEXT("ear_size"), TEXT("ear_angle")
    };
    return Keys;
}

const TArray<FName>& DiscGolfFullCharacterRuntime::GetVisibleProxyFaceMorphKeys()
{
    // The prepared proxy generator visibly authors exactly these five. The
    // other fifteen remain real data/UI/runtime mappings, not fake visual claims.
    static const TArray<FName> Keys = {
        TEXT("head_width"), TEXT("head_height"), TEXT("cheek_fullness"),
        TEXT("jaw_width"), TEXT("chin_length")
    };
    return Keys;
}

FText DiscGolfFullCharacterRuntime::GetFaceMorphDisplayName(FName MorphKey)
{
    FString Label = MorphKey.ToString().Replace(TEXT("_"), TEXT(" "));
    Label.ToUpperInline();
    return FText::FromString(Label);
}

FDGFullCharacterCustomization DiscGolfFullCharacterRuntime::MakeDefaultCustomization()
{
    FDGFullCharacterCustomization Result;
    ApplyFacePreset(TEXT("face_default"), Result.Face);
    NormalizeForPersistence(Result);
    return Result;
}

void DiscGolfFullCharacterRuntime::NormalizeForPersistence(
    FDGFullCharacterCustomization& InOutCharacter)
{
    const FDGFullCharacterCustomization Defaults;

    FString SafeDisplayName;
    SafeDisplayName.Reserve(InOutCharacter.Identity.DisplayName.Len());
    for (const TCHAR Character : InOutCharacter.Identity.DisplayName)
    {
        if (!FChar::IsControl(Character) && !FChar::IsLinebreak(Character))
        {
            SafeDisplayName.AppendChar(Character);
        }
    }
    InOutCharacter.Identity.DisplayName = MoveTemp(SafeDisplayName);
    InOutCharacter.Identity.DisplayName.TrimStartAndEndInline();
    InOutCharacter.Identity.DisplayName = InOutCharacter.Identity.DisplayName.Left(
        MaximumDisplayNameLength);
    if (InOutCharacter.Identity.DisplayName.IsEmpty())
    {
        InOutCharacter.Identity.DisplayName = Defaults.Identity.DisplayName;
    }
    if (InOutCharacter.Identity.VoiceId.IsNone())
    {
        InOutCharacter.Identity.VoiceId = Defaults.Identity.VoiceId;
    }
    if (InOutCharacter.Identity.PronounSetId.IsNone())
    {
        InOutCharacter.Identity.PronounSetId = Defaults.Identity.PronounSetId;
    }
    if (InOutCharacter.Identity.Handedness != EDGHandedness::Left)
    {
        InOutCharacter.Identity.Handedness = EDGHandedness::Right;
    }

    FDiscGolfCharacterProfileSaveData Safe =
        FDiscGolfCharacterProfileSaveData::FromFramework(
            InOutCharacter.Body,
            InOutCharacter.ThrowStyle,
            InOutCharacter.Identity.Handedness,
            InOutCharacter.BodyBuild);
    Safe.Sanitize();
    InOutCharacter.Body = Safe.ToBodyProfile();
    InOutCharacter.BodyBuild = Safe.ToBodyBuildProfile();
    InOutCharacter.ThrowStyle = Safe.ToThrowStyle();
    InOutCharacter.Identity.Handedness = Safe.GetHandedness();

    static const TSet<FName> ValidPresetIds = {
        TEXT("face_default"), TEXT("face_square"),
        TEXT("face_narrow"), TEXT("face_round")
    };
    if (!ValidPresetIds.Contains(InOutCharacter.Face.PresetId))
    {
        InOutCharacter.Face.PresetId = TEXT("face_default");
    }
    TMap<FName, float> SafeMorphs;
    for (FName Key : GetFaceMorphKeys())
    {
        const float* Existing = InOutCharacter.Face.MorphValues.Find(Key);
        SafeMorphs.Add(Key, Existing && FMath::IsFinite(*Existing)
            ? FMath::Clamp(*Existing, -1.0f, 1.0f) : 0.0f);
    }
    InOutCharacter.Face.MorphValues = MoveTemp(SafeMorphs);

    if (InOutCharacter.Hair.HairStyleId.IsNone())
    {
        InOutCharacter.Hair.HairStyleId = TEXT("hair_none");
    }
    if (InOutCharacter.Hair.FacialHairId.IsNone())
    {
        InOutCharacter.Hair.FacialHairId = TEXT("facialhair_none");
    }
    if (InOutCharacter.Hair.EyebrowId.IsNone())
    {
        InOutCharacter.Hair.EyebrowId = TEXT("brow_default");
    }
    InOutCharacter.Hair.HairColor = SanitizeColor(
        InOutCharacter.Hair.HairColor, Defaults.Hair.HairColor);
    InOutCharacter.Hair.FacialHairColor = SanitizeColor(
        InOutCharacter.Hair.FacialHairColor, Defaults.Hair.FacialHairColor);
    InOutCharacter.Hair.EyebrowColor = SanitizeColor(
        InOutCharacter.Hair.EyebrowColor, Defaults.Hair.EyebrowColor);

    InOutCharacter.Appearance.SkinTone = SanitizeColor(
        InOutCharacter.Appearance.SkinTone, Defaults.Appearance.SkinTone);
    InOutCharacter.Appearance.EyeColor = SanitizeColor(
        InOutCharacter.Appearance.EyeColor, Defaults.Appearance.EyeColor);
    InOutCharacter.Appearance.Complexion = SanitizeFullCharacterFinite(
        InOutCharacter.Appearance.Complexion,
        Defaults.Appearance.Complexion, 0.0f, 1.0f);
    InOutCharacter.Appearance.Freckles = SanitizeFullCharacterFinite(
        InOutCharacter.Appearance.Freckles,
        Defaults.Appearance.Freckles, 0.0f, 1.0f);
    InOutCharacter.Appearance.SunExposure = SanitizeFullCharacterFinite(
        InOutCharacter.Appearance.SunExposure,
        Defaults.Appearance.SunExposure, 0.0f, 1.0f);
    if (InOutCharacter.Appearance.ScarId.IsNone())
    {
        InOutCharacter.Appearance.ScarId = TEXT("scar_none");
    }
    TArray<FName> SafeTattoos;
    for (FName TattooId : InOutCharacter.Appearance.TattooIds)
    {
        if (!TattooId.IsNone() && TattooId != FName(TEXT("tattoo_none")))
        {
            SafeTattoos.AddUnique(TattooId);
        }
    }
    SafeTattoos.Sort([](FName A, FName B)
    {
        return A.LexicalLess(B);
    });
    SafeTattoos.SetNum(FMath::Min(SafeTattoos.Num(), 8));
    InOutCharacter.Appearance.TattooIds = MoveTemp(SafeTattoos);
    InOutCharacter.Outfit = DiscGolfOutfitRuntime::NormalizeForPersistence(
        InOutCharacter.Outfit);
}

bool DiscGolfFullCharacterRuntime::AreCustomizationsEquivalent(
    const FDGFullCharacterCustomization& A,
    const FDGFullCharacterCustomization& B)
{
    return A.Identity.DisplayName == B.Identity.DisplayName
        && A.Identity.Handedness == B.Identity.Handedness
        && A.Identity.VoiceId == B.Identity.VoiceId
        && A.Identity.PronounSetId == B.Identity.PronounSetId
        && BodyExactlyMatches(A.Body, B.Body)
        && BodyBuildExactlyMatches(A.BodyBuild, B.BodyBuild)
        && A.Face.PresetId == B.Face.PresetId
        && MorphMapsExactlyMatch(A.Face.MorphValues, B.Face.MorphValues)
        && A.Hair.HairStyleId == B.Hair.HairStyleId
        && A.Hair.FacialHairId == B.Hair.FacialHairId
        && A.Hair.EyebrowId == B.Hair.EyebrowId
        && ColorsExactlyMatch(A.Hair.HairColor, B.Hair.HairColor)
        && ColorsExactlyMatch(A.Hair.FacialHairColor, B.Hair.FacialHairColor)
        && ColorsExactlyMatch(A.Hair.EyebrowColor, B.Hair.EyebrowColor)
        && ColorsExactlyMatch(A.Appearance.SkinTone, B.Appearance.SkinTone)
        && ColorsExactlyMatch(A.Appearance.EyeColor, B.Appearance.EyeColor)
        && A.Appearance.Complexion == B.Appearance.Complexion
        && A.Appearance.Freckles == B.Appearance.Freckles
        && A.Appearance.SunExposure == B.Appearance.SunExposure
        && A.Appearance.ScarId == B.Appearance.ScarId
        && NameArraysExactlyMatch(A.Appearance.TattooIds, B.Appearance.TattooIds)
        && ThrowStyleExactlyMatches(A.ThrowStyle, B.ThrowStyle)
        && DiscGolfOutfitRuntime::AreLoadoutsEquivalent(A.Outfit, B.Outfit);
}

bool DiscGolfFullCharacterRuntime::ApplyFacePreset(
    FName PresetId,
    FDGFaceProfile& InOutFace)
{
    if (PresetId != FName(TEXT("face_default"))
        && PresetId != FName(TEXT("face_square"))
        && PresetId != FName(TEXT("face_narrow"))
        && PresetId != FName(TEXT("face_round")))
    {
        return false;
    }

    InOutFace.PresetId = PresetId;
    InOutFace.MorphValues.Reset();
    for (FName Key : GetFaceMorphKeys())
    {
        InOutFace.MorphValues.Add(Key, 0.0f);
    }
    if (PresetId == FName(TEXT("face_square")))
    {
        InOutFace.MorphValues[TEXT("head_width")] = 0.25f;
        InOutFace.MorphValues[TEXT("jaw_width")] = 0.55f;
        InOutFace.MorphValues[TEXT("jaw_height")] = -0.10f;
        InOutFace.MorphValues[TEXT("chin_width")] = 0.25f;
        InOutFace.MorphValues[TEXT("cheek_width")] = 0.15f;
    }
    else if (PresetId == FName(TEXT("face_narrow")))
    {
        InOutFace.MorphValues[TEXT("head_width")] = -0.35f;
        InOutFace.MorphValues[TEXT("jaw_width")] = -0.30f;
        InOutFace.MorphValues[TEXT("cheek_width")] = -0.20f;
        InOutFace.MorphValues[TEXT("chin_length")] = 0.15f;
    }
    else if (PresetId == FName(TEXT("face_round")))
    {
        InOutFace.MorphValues[TEXT("head_width")] = 0.35f;
        InOutFace.MorphValues[TEXT("cheek_fullness")] = 0.45f;
        InOutFace.MorphValues[TEXT("jaw_width")] = 0.15f;
        InOutFace.MorphValues[TEXT("chin_length")] = -0.15f;
    }
    return true;
}

FDiscGolfFullCustomizationResolution DiscGolfFullCharacterRuntime::ResolveForRuntime(
    const FDGFullCharacterCustomization& Requested,
    const UDiscGolfCosmeticCatalog* CosmeticCatalog,
    const UDiscGolfOutfitCatalog* OutfitCatalog)
{
    FDiscGolfFullCustomizationResolution Result;
    Result.Character = Requested;
    NormalizeForPersistence(Result.Character);
    Result.bCosmeticCatalogAvailable = CosmeticCatalog != nullptr;

    const auto ResolveSingle = [&](FName RequestedId, EDGCosmeticKind Kind, FName& OutId)
    {
        FString Warning;
        if (!ResolveId(
                RequestedId, Kind, Result.Character.Body.HeightCm,
                CosmeticCatalog, OutId, Warning))
        {
            Result.bAllCosmeticsResolved = false;
            if (!Warning.IsEmpty())
            {
                Result.Warnings.Add(Warning);
            }
        }
    };
    ResolveSingle(Result.Character.Hair.HairStyleId,
        EDGCosmeticKind::Hair, Result.Character.Hair.HairStyleId);
    ResolveSingle(Result.Character.Hair.FacialHairId,
        EDGCosmeticKind::FacialHair, Result.Character.Hair.FacialHairId);
    ResolveSingle(Result.Character.Hair.EyebrowId,
        EDGCosmeticKind::Eyebrow, Result.Character.Hair.EyebrowId);
    ResolveSingle(Result.Character.Appearance.ScarId,
        EDGCosmeticKind::Scar, Result.Character.Appearance.ScarId);
    ResolveSingle(Result.Character.Identity.VoiceId,
        EDGCosmeticKind::Voice, Result.Character.Identity.VoiceId);
    ResolveSingle(Result.Character.Identity.PronounSetId,
        EDGCosmeticKind::PronounSet, Result.Character.Identity.PronounSetId);

    TArray<FName> ResolvedTattoos;
    for (FName TattooId : Result.Character.Appearance.TattooIds)
    {
        FString Reason;
        const UDiscGolfCosmeticItem* Item = CosmeticCatalog
            ? CosmeticCatalog->FindById(TattooId) : nullptr;
        if (TattooId == FName(TEXT("tattoo_none")))
        {
            continue;
        }
        if (IsRenderableCatalogItem(
                Item, EDGCosmeticKind::Tattoo,
                Result.Character.Body.HeightCm, Reason))
        {
            ResolvedTattoos.AddUnique(TattooId);
        }
        else
        {
            Result.bAllCosmeticsResolved = false;
            Result.Warnings.Add(FString::Printf(
                TEXT("Missing tattoo cosmetic %s was skipped safely."),
                *TattooId.ToString()));
        }
    }
    Result.Character.Appearance.TattooIds = MoveTemp(ResolvedTattoos);

    const FDiscGolfOutfitResolution Outfit =
        DiscGolfOutfitRuntime::ResolveCanonicalLoadout(
            Result.Character.Outfit, OutfitCatalog, Result.Character.Body);
    Result.Character.Outfit = Outfit.Loadout;
    if (!Outfit.bAllEntriesResolved)
    {
        Result.bAllCosmeticsResolved = false;
        Result.Warnings.Append(Outfit.Warnings);
    }
    return Result;
}

TArray<FDiscGolfCosmeticOption> DiscGolfFullCharacterRuntime::GetCosmeticOptions(
    const UDiscGolfCosmeticCatalog* Catalog,
    EDGCosmeticKind Kind,
    float HeightCm)
{
    TArray<FDiscGolfCosmeticOption> Result;
    if (!Catalog)
    {
        return Result;
    }
    for (UDiscGolfCosmeticItem* Item : Catalog->GetByKind(Kind))
    {
        if (!Item || Item->ItemId.IsNone())
        {
            continue;
        }
        FDiscGolfCosmeticOption Option;
        Option.ItemId = Item->ItemId;
        Option.DisplayName = Item->DisplayName.IsEmpty()
            ? FText::FromName(Item->ItemId) : Item->DisplayName;
        Option.Kind = Item->Kind;
        Option.bCompatible = IsRenderableCatalogItem(Item, Kind, HeightCm,
            Option.CompatibilityReason);
        Result.Add(MoveTemp(Option));
    }
    Result.Sort([](const FDiscGolfCosmeticOption& A, const FDiscGolfCosmeticOption& B)
    {
        return A.ItemId.LexicalLess(B.ItemId);
    });
    return Result;
}

bool DiscGolfFullCharacterRuntime::SetCosmeticSelection(
    FDGFullCharacterCustomization& InOutCharacter,
    EDGCosmeticKind Kind,
    FName ItemId,
    const UDiscGolfCosmeticCatalog* Catalog,
    FString& OutStatus)
{
    OutStatus.Reset();
    FDGFullCharacterCustomization Candidate = InOutCharacter;
    NormalizeForPersistence(Candidate);
    const UDiscGolfCosmeticItem* Item = Catalog ? Catalog->FindById(ItemId) : nullptr;
    FString Reason;
    if (!IsRenderableCatalogItem(Item, Kind, Candidate.Body.HeightCm, Reason))
    {
        OutStatus = FString::Printf(TEXT("Cosmetic %s is unavailable: %s."),
            *ItemId.ToString(), *Reason);
        return false;
    }

    switch (Kind)
    {
        case EDGCosmeticKind::Hair: Candidate.Hair.HairStyleId = ItemId; break;
        case EDGCosmeticKind::FacialHair: Candidate.Hair.FacialHairId = ItemId; break;
        case EDGCosmeticKind::Eyebrow: Candidate.Hair.EyebrowId = ItemId; break;
        case EDGCosmeticKind::Scar: Candidate.Appearance.ScarId = ItemId; break;
        case EDGCosmeticKind::Tattoo:
            Candidate.Appearance.TattooIds.Reset();
            if (ItemId != FName(TEXT("tattoo_none")))
            {
                Candidate.Appearance.TattooIds.Add(ItemId);
            }
            break;
        case EDGCosmeticKind::Voice: Candidate.Identity.VoiceId = ItemId; break;
        case EDGCosmeticKind::PronounSet:
            Candidate.Identity.PronounSetId = ItemId;
            break;
        default:
            OutStatus = TEXT("That cosmetic category is not selectable in Session 7.");
            return false;
    }
    InOutCharacter = MoveTemp(Candidate);
    OutStatus = FString::Printf(TEXT("Selected %s."), *ItemId.ToString());
    return true;
}

FDGFullCharacterCustomization DiscGolfFullCharacterRuntime::Randomize(
    const FDGFullCharacterCustomization& Current,
    const FDiscGolfCharacterRandomizeLocks& Locks,
    const UDiscGolfCosmeticCatalog* CosmeticCatalog,
    const UDiscGolfOutfitCatalog* OutfitCatalog,
    FRandomStream& Random)
{
    FDGFullCharacterCustomization Result = Current;
    NormalizeForPersistence(Result);
    const FDGFullCharacterCustomization NormalizedCurrent = Result;

    if (!Locks.bIdentity)
    {
        Result.Identity.Handedness = Random.RandRange(0, 1) == 0
            ? EDGHandedness::Right : EDGHandedness::Left;
        const auto SelectIdentityKind = [&](EDGCosmeticKind Kind, FName& OutId)
        {
            TArray<FDiscGolfCosmeticOption> Options = GetCosmeticOptions(
                CosmeticCatalog, Kind, Result.Body.HeightCm);
            Options.RemoveAll([](const FDiscGolfCosmeticOption& Option)
            {
                return !Option.bCompatible;
            });
            if (const FDiscGolfCosmeticOption* Picked = PickOption(Random, Options))
            {
                OutId = Picked->ItemId;
            }
            else
            {
                OutId = DefaultIdForKind(Kind);
            }
        };
        SelectIdentityKind(EDGCosmeticKind::Voice, Result.Identity.VoiceId);
        SelectIdentityKind(
            EDGCosmeticKind::PronounSet, Result.Identity.PronounSetId);
    }
    if (!Locks.bBody)
    {
        using namespace DiscGolfCharacterCreatorSchema;
        Result.Body.HeightCm = Random.FRandRange(MinHeightCm, MaxHeightCm);
        Result.Body.WingspanScale = Random.FRandRange(MinWingspanScale, MaxWingspanScale);
        Result.Body.ShoulderWidthScale = Random.FRandRange(
            MinShoulderWidthScale, MaxShoulderWidthScale);
        Result.Body.TorsoLengthScale = Random.FRandRange(
            MinTorsoLengthScale, MaxTorsoLengthScale);
        Result.Body.LegLengthScale = Random.FRandRange(
            MinLegLengthScale, MaxLegLengthScale);
        Result.Body.HandScale = Random.FRandRange(MinHandScale, MaxHandScale);
        Result.Body.MassKg = Random.FRandRange(MinMassKg, MaxMassKg);
        Result.BodyBuild.Muscularity = Random.FRand();
        Result.BodyBuild.BodyFat = Random.FRand();
        Result.BodyBuild.Chest = Random.FRandRange(-1.0f, 1.0f);
        Result.BodyBuild.Waist = Random.FRandRange(-1.0f, 1.0f);
        Result.BodyBuild.Hips = Random.FRandRange(-1.0f, 1.0f);
        Result.BodyBuild.Arms = Random.FRandRange(-1.0f, 1.0f);
        Result.BodyBuild.Legs = Random.FRandRange(-1.0f, 1.0f);
    }
    if (!Locks.bFace)
    {
        static const FName Presets[] = {
            TEXT("face_default"), TEXT("face_square"),
            TEXT("face_narrow"), TEXT("face_round")
        };
        ApplyFacePreset(Presets[Random.RandRange(0, 3)], Result.Face);
    }

    static const FLinearColor HairPalette[] = {
        FLinearColor(0.02f, 0.015f, 0.01f, 1.0f),
        FLinearColor(0.05f, 0.03f, 0.02f, 1.0f),
        FLinearColor(0.13f, 0.07f, 0.035f, 1.0f),
        FLinearColor(0.60f, 0.45f, 0.22f, 1.0f),
        FLinearColor(0.34f, 0.34f, 0.33f, 1.0f)
    };
    if (!Locks.bHair)
    {
        const auto SelectKind = [&](EDGCosmeticKind Kind, FName& OutId)
        {
            TArray<FDiscGolfCosmeticOption> Options = GetCosmeticOptions(
                CosmeticCatalog, Kind, Result.Body.HeightCm);
            Options.RemoveAll([](const FDiscGolfCosmeticOption& Option)
            {
                return !Option.bCompatible;
            });
            if (const FDiscGolfCosmeticOption* Picked = PickOption(Random, Options))
            {
                OutId = Picked->ItemId;
            }
            else
            {
                OutId = DefaultIdForKind(Kind);
            }
        };
        SelectKind(EDGCosmeticKind::Hair, Result.Hair.HairStyleId);
        SelectKind(EDGCosmeticKind::FacialHair, Result.Hair.FacialHairId);
        SelectKind(EDGCosmeticKind::Eyebrow, Result.Hair.EyebrowId);
        Result.Hair.HairColor = PickColor(Random, HairPalette);
        Result.Hair.FacialHairColor = PickColor(Random, HairPalette);
        Result.Hair.EyebrowColor = PickColor(Random, HairPalette);
    }

    static const FLinearColor SkinPalette[] = {
        FLinearColor(0.16f, 0.08f, 0.05f, 1.0f),
        FLinearColor(0.28f, 0.14f, 0.08f, 1.0f),
        FLinearColor(0.40f, 0.23f, 0.15f, 1.0f),
        FLinearColor(0.55f, 0.35f, 0.24f, 1.0f),
        FLinearColor(0.68f, 0.47f, 0.34f, 1.0f),
        FLinearColor(0.78f, 0.60f, 0.47f, 1.0f),
        FLinearColor(0.88f, 0.72f, 0.60f, 1.0f)
    };
    static const FLinearColor EyePalette[] = {
        FLinearColor(0.08f, 0.18f, 0.26f, 1.0f),
        FLinearColor(0.15f, 0.24f, 0.20f, 1.0f),
        FLinearColor(0.26f, 0.16f, 0.07f, 1.0f),
        FLinearColor(0.35f, 0.35f, 0.32f, 1.0f)
    };
    if (!Locks.bAppearance)
    {
        Result.Appearance.SkinTone = PickColor(Random, SkinPalette);
        Result.Appearance.EyeColor = PickColor(Random, EyePalette);
        Result.Appearance.Complexion = Random.FRand();
        Result.Appearance.Freckles = Random.FRand();
        Result.Appearance.SunExposure = Random.FRand();

        TArray<FDiscGolfCosmeticOption> Scars = GetCosmeticOptions(
            CosmeticCatalog, EDGCosmeticKind::Scar, Result.Body.HeightCm);
        Scars.RemoveAll([](const FDiscGolfCosmeticOption& Option)
        {
            return !Option.bCompatible;
        });
        const FDiscGolfCosmeticOption* PickedScar = PickOption(Random, Scars);
        Result.Appearance.ScarId = PickedScar
            ? PickedScar->ItemId : FName(TEXT("scar_none"));

        TArray<FDiscGolfCosmeticOption> Tattoos = GetCosmeticOptions(
            CosmeticCatalog, EDGCosmeticKind::Tattoo, Result.Body.HeightCm);
        Tattoos.RemoveAll([](const FDiscGolfCosmeticOption& Option)
        {
            return !Option.bCompatible;
        });
        Result.Appearance.TattooIds.Reset();
        if (const FDiscGolfCosmeticOption* Picked = PickOption(Random, Tattoos))
        {
            if (Picked->ItemId != FName(TEXT("tattoo_none")))
            {
                Result.Appearance.TattooIds.Add(Picked->ItemId);
            }
        }
    }
    if (!Locks.bThrowStyle)
    {
        Result.ThrowStyle.RunUpIntensity = Random.FRand();
        Result.ThrowStyle.ReachBackAmount = Random.FRand();
        Result.ThrowStyle.TorsoRotation = Random.FRand();
        Result.ThrowStyle.BraceIntensity = Random.FRand();
        Result.ThrowStyle.Explosiveness = Random.FRand();
        Result.ThrowStyle.FollowThrough = Random.FRand();
        Result.ThrowStyle.PowerMultiplier = 1.0f;
        Result.ThrowStyle.SpinMultiplier = 1.0f;
    }

    if (!Locks.bOutfit)
    {
        Result.Outfit.Equipped.Reset();
        for (EDGOutfitSlot Slot : DiscGolfOutfitRuntime::GetOrderedSlots())
        {
            TArray<FDiscGolfOutfitOption> Options =
                DiscGolfOutfitRuntime::GetOptionsForSlot(
                    OutfitCatalog, Slot, Result.Body);
            Options.RemoveAll([](const FDiscGolfOutfitOption& Option)
            {
                return !Option.bCompatible || Option.VariantIds.IsEmpty();
            });
            const FDiscGolfOutfitOption* Picked = PickOption(Random, Options);
            if (!Picked)
            {
                continue;
            }
            FString Ignored;
            DiscGolfOutfitRuntime::SetSlotSelection(
                Result.Outfit,
                Slot,
                Picked->ItemId,
                Picked->VariantIds[Random.RandRange(0, Picked->VariantIds.Num() - 1)],
                OutfitCatalog,
                Result.Body,
                Ignored);
        }
    }
    else if (!Locks.bBody)
    {
        // A locked outfit wins over an incompatible randomized body. Keeping
        // the previous body is the only result that honors both promises.
        const FDiscGolfOutfitResolution LockedOutfit =
            DiscGolfOutfitRuntime::ResolveCanonicalLoadout(
                NormalizedCurrent.Outfit, OutfitCatalog, Result.Body);
        if (!LockedOutfit.bAllEntriesResolved
            || !DiscGolfOutfitRuntime::AreLoadoutsEquivalent(
                LockedOutfit.Loadout, NormalizedCurrent.Outfit))
        {
            Result.Body = NormalizedCurrent.Body;
            Result.BodyBuild = NormalizedCurrent.BodyBuild;
        }
        Result.Outfit = NormalizedCurrent.Outfit;
    }

    NormalizeForPersistence(Result);
    FDGFullCharacterCustomization Resolved =
        ResolveForRuntime(Result, CosmeticCatalog, OutfitCatalog).Character;

    // Resolution is allowed to repair randomized fields, but a category lock
    // is an exact draft-data promise. Reapply every locked normalized category
    // after catalog fallback so unavailable content cannot silently defeat a
    // user's lock while randomizing an unrelated category.
    if (Locks.bIdentity) Resolved.Identity = NormalizedCurrent.Identity;
    if (Locks.bBody)
    {
        Resolved.Body = NormalizedCurrent.Body;
        Resolved.BodyBuild = NormalizedCurrent.BodyBuild;
    }
    if (Locks.bFace) Resolved.Face = NormalizedCurrent.Face;
    if (Locks.bHair) Resolved.Hair = NormalizedCurrent.Hair;
    if (Locks.bAppearance) Resolved.Appearance = NormalizedCurrent.Appearance;
    if (Locks.bThrowStyle) Resolved.ThrowStyle = NormalizedCurrent.ThrowStyle;
    if (Locks.bOutfit) Resolved.Outfit = NormalizedCurrent.Outfit;
    NormalizeForPersistence(Resolved);
    return Resolved;
}
