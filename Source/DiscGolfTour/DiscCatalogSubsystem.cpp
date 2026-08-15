#include "DiscCatalogSubsystem.h"

#include "DiscEquipmentDataAssets.h"
#include "DiscGolfTour.h"
#include "Engine/AssetManager.h"

namespace
{
FDiscMoldDefinition MakeDisc(const TCHAR* Id, const TCHAR* Name, int32 Speed, int32 Glide, float Turn, float Fade)
{
    FDiscMoldDefinition Disc;
    Disc.MoldId = FName(Id);
    Disc.DisplayName = FText::FromString(Name);
    Disc.Speed = Speed;
    Disc.Glide = Glide;
    Disc.Turn = Turn;
    Disc.Fade = Fade;

    const float SpeedNorm = FMath::Clamp((Speed - 2.0f) / 10.0f, 0.0f, 1.0f);
    Disc.Aero.CD0 = FMath::Lerp(0.135f, 0.080f, SpeedNorm);
    Disc.Aero.CLa = 1.72f + 0.05f * Glide;
    Disc.Aero.CL0 = 0.22f + 0.018f * Glide;
    Disc.Aero.HighSpeedTurnMomentNm = FMath::Max(0.0f, -Turn) * 0.0035f;
    Disc.Aero.LowSpeedFadeMomentNm = FMath::Max(0.0f, Fade) * 0.0025f;
    Disc.Aero.TurnStartsAboveMps = FMath::Lerp(13.5f, 20.0f, SpeedNorm);
    Disc.Aero.FadeStartsBelowMps = FMath::Lerp(12.0f, 17.0f, SpeedNorm);
    return Disc;
}

FDiscPlasticDefinition MakePlastic(
    const TCHAR* Id,
    EDiscPlastic Plastic,
    float TurnScale,
    float FadeScale,
    float RestitutionScale,
    float FrictionScale)
{
    FDiscPlasticDefinition Definition;
    Definition.PlasticId = FName(Id);
    Definition.Plastic = Plastic;
    Definition.HighSpeedTurnMomentScale = TurnScale;
    Definition.LowSpeedFadeMomentScale = FadeScale;
    Definition.GroundRestitutionScale = RestitutionScale;
    Definition.GroundFrictionScale = FrictionScale;
    return Definition;
}

bool IsFinitePositive(float Value)
{
    return FMath::IsFinite(Value) && Value > 0.0f;
}
}

void UDiscCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    BuildFallbackDefinitions(FallbackCatalog, FallbackPlasticCatalog);

    TArray<FDiscMoldDefinition> AssetMolds;
    TArray<FDiscPlasticDefinition> AssetPlastics;
    FString LoadError;
    const bool bAssetsLoaded = LoadPrimaryAssetDefinitions(AssetMolds, AssetPlastics, LoadError);

    FString SelectionReason;
    bool bUsedFallback = true;
    SelectCatalogDefinitions(
        bAssetsLoaded ? AssetMolds : TArray<FDiscMoldDefinition>(),
        bAssetsLoaded ? AssetPlastics : TArray<FDiscPlasticDefinition>(),
        FallbackCatalog,
        FallbackPlasticCatalog,
        ActiveCatalog,
        ActivePlasticCatalog,
        bUsedFallback,
        SelectionReason);
    bUsingPrimaryAssets = !bUsedFallback;

    if (bUsingPrimaryAssets)
    {
        CatalogSourceText = FString::Printf(TEXT("PRIMARY ASSETS - %d molds / %d plastics"),
            ActiveCatalog.Num(), ActivePlasticCatalog.Num());
        UE_LOG(LogDiscGolfTour, Display, TEXT("Disc catalog loaded from cooked Primary Data Assets: %d molds, %d plastics"),
            ActiveCatalog.Num(), ActivePlasticCatalog.Num());
    }
    else
    {
        const FString Reason = bAssetsLoaded ? SelectionReason : LoadError;
        CatalogSourceText = FString::Printf(TEXT("SOURCE FALLBACK - %s"), *Reason);
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Disc catalog using deterministic source fallback: %s"), *Reason);
    }
}

void UDiscCatalogSubsystem::BuildFallbackDefinitions(
    TArray<FDiscMoldDefinition>& OutMolds,
    TArray<FDiscPlasticDefinition>& OutPlastics)
{
    OutMolds.Reset();
    OutMolds.Add(MakeDisc(TEXT("Apex"), TEXT("Apex"), 12, 5, -1.0f, 3.0f));
    OutMolds.Add(MakeDisc(TEXT("Vector"), TEXT("Vector"), 9, 5, -2.0f, 2.0f));
    OutMolds.Add(MakeDisc(TEXT("Line"), TEXT("Line"), 7, 5, -1.0f, 2.0f));
    OutMolds.Add(MakeDisc(TEXT("Compass"), TEXT("Compass"), 5, 5, 0.0f, 1.0f));
    OutMolds.Add(MakeDisc(TEXT("Touch"), TEXT("Touch"), 2, 3, 0.0f, 1.0f));

    OutPlastics.Reset();
    OutPlastics.Add(MakePlastic(TEXT("Base"), EDiscPlastic::Base, 1.18f, 0.88f, 0.72f, 1.18f));
    OutPlastics.Add(MakePlastic(TEXT("Tour"), EDiscPlastic::Tour, 1.0f, 1.0f, 1.0f, 1.0f));
    OutPlastics.Add(MakePlastic(TEXT("Crystal"), EDiscPlastic::Crystal, 0.84f, 1.12f, 1.20f, 0.86f));
}

bool UDiscCatalogSubsystem::ValidateDefinitions(
    const TArray<FDiscMoldDefinition>& Molds,
    const TArray<FDiscPlasticDefinition>& Plastics,
    FString& OutError)
{
    if (Molds.IsEmpty())
    {
        OutError = TEXT("No mold definitions were found");
        return false;
    }
    if (Plastics.IsEmpty())
    {
        OutError = TEXT("No plastic definitions were found");
        return false;
    }

    TSet<FName> MoldIds;
    for (const FDiscMoldDefinition& Mold : Molds)
    {
        if (Mold.MoldId.IsNone())
        {
            OutError = TEXT("A mold has no stable MoldId");
            return false;
        }
        if (MoldIds.Contains(Mold.MoldId))
        {
            OutError = FString::Printf(TEXT("Duplicate mold ID: %s"), *Mold.MoldId.ToString());
            return false;
        }
        MoldIds.Add(Mold.MoldId);

        const FDiscAeroProfile& Aero = Mold.Aero;
        if (Mold.DisplayName.IsEmpty() || Mold.Speed < 1 || Mold.Speed > 15
            || Mold.Glide < 1 || Mold.Glide > 7 || !FMath::IsFinite(Mold.Turn) || !FMath::IsFinite(Mold.Fade)
            || !IsFinitePositive(Aero.MassKg) || !IsFinitePositive(Aero.DiameterM)
            || !IsFinitePositive(Aero.AreaM2) || !IsFinitePositive(Aero.InertiaAxialKgM2)
            || !IsFinitePositive(Aero.InertiaPlanarKgM2) || !FMath::IsFinite(Aero.CL0)
            || !FMath::IsFinite(Aero.CLa) || !IsFinitePositive(Aero.CD0) || !IsFinitePositive(Aero.CDa)
            || !FMath::IsFinite(Aero.HighSpeedTurnMomentNm) || !FMath::IsFinite(Aero.LowSpeedFadeMomentNm)
            || !IsFinitePositive(Aero.TurnStartsAboveMps) || !IsFinitePositive(Aero.FadeStartsBelowMps)
            || !IsFinitePositive(Aero.GroundRestitution) || !IsFinitePositive(Aero.GroundFriction))
        {
            OutError = FString::Printf(TEXT("Mold %s has incomplete or non-finite calibration data"),
                *Mold.MoldId.ToString());
            return false;
        }
    }

    TSet<FName> PlasticIds;
    TSet<EDiscPlastic> PlasticTypes;
    for (const FDiscPlasticDefinition& Plastic : Plastics)
    {
        if (Plastic.PlasticId.IsNone())
        {
            OutError = TEXT("A plastic has no stable PlasticId");
            return false;
        }
        if (PlasticIds.Contains(Plastic.PlasticId))
        {
            OutError = FString::Printf(TEXT("Duplicate plastic ID: %s"), *Plastic.PlasticId.ToString());
            return false;
        }
        if (PlasticTypes.Contains(Plastic.Plastic))
        {
            OutError = FString::Printf(TEXT("Duplicate plastic enum mapping for %s"), *Plastic.PlasticId.ToString());
            return false;
        }
        PlasticIds.Add(Plastic.PlasticId);
        PlasticTypes.Add(Plastic.Plastic);

        if (!IsFinitePositive(Plastic.HighSpeedTurnMomentScale)
            || !IsFinitePositive(Plastic.LowSpeedFadeMomentScale)
            || !IsFinitePositive(Plastic.GroundRestitutionScale)
            || !IsFinitePositive(Plastic.GroundFrictionScale))
        {
            OutError = FString::Printf(TEXT("Plastic %s has incomplete or non-positive modifiers"),
                *Plastic.PlasticId.ToString());
            return false;
        }
    }

    if (!PlasticTypes.Contains(EDiscPlastic::Base)
        || !PlasticTypes.Contains(EDiscPlastic::Tour)
        || !PlasticTypes.Contains(EDiscPlastic::Crystal))
    {
        OutError = TEXT("Plastic catalog must define Base, Tour, and Crystal exactly once");
        return false;
    }

    OutError.Reset();
    return true;
}

bool UDiscCatalogSubsystem::ResolveFromDefinitions(
    const TArray<FDiscMoldDefinition>& Molds,
    const TArray<FDiscPlasticDefinition>& Plastics,
    FName MoldId,
    EDiscPlastic Plastic,
    FResolvedDiscDefinition& OutDisc)
{
    OutDisc = FResolvedDiscDefinition();
    const FDiscMoldDefinition* FoundMold = Molds.FindByPredicate([MoldId](const FDiscMoldDefinition& Entry)
    {
        return Entry.MoldId == MoldId;
    });
    const FDiscPlasticDefinition* FoundPlastic = Plastics.FindByPredicate([Plastic](const FDiscPlasticDefinition& Entry)
    {
        return Entry.Plastic == Plastic;
    });
    if (!FoundMold || !FoundPlastic) return false;

    OutDisc.MoldId = FoundMold->MoldId;
    OutDisc.DisplayName = FoundMold->DisplayName;
    OutDisc.Speed = FoundMold->Speed;
    OutDisc.Glide = FoundMold->Glide;
    OutDisc.Turn = FoundMold->Turn;
    OutDisc.Fade = FoundMold->Fade;
    OutDisc.Plastic = Plastic;
    OutDisc.Aero = FoundMold->Aero;
    OutDisc.Aero.HighSpeedTurnMomentNm *= FoundPlastic->HighSpeedTurnMomentScale;
    OutDisc.Aero.LowSpeedFadeMomentNm *= FoundPlastic->LowSpeedFadeMomentScale;
    OutDisc.Aero.GroundRestitution *= FoundPlastic->GroundRestitutionScale;
    OutDisc.Aero.GroundFriction *= FoundPlastic->GroundFrictionScale;
    return true;
}

bool UDiscCatalogSubsystem::LoadPrimaryAssetDefinitions(
    TArray<FDiscMoldDefinition>& OutMolds,
    TArray<FDiscPlasticDefinition>& OutPlastics,
    FString& OutError)
{
    OutMolds.Reset();
    OutPlastics.Reset();
    UAssetManager& Manager = UAssetManager::Get();
    Manager.ScanPathsForPrimaryAssets(
        FPrimaryAssetType(TEXT("DiscMold")),
        { TEXT("/Game/Data/Discs/Molds") },
        UDiscMoldDataAsset::StaticClass(), false, false, true);
    Manager.ScanPathsForPrimaryAssets(
        FPrimaryAssetType(TEXT("DiscPlastic")),
        { TEXT("/Game/Data/Discs/Plastics") },
        UDiscPlasticDataAsset::StaticClass(), false, false, true);
    TArray<FPrimaryAssetId> MoldIds;
    TArray<FPrimaryAssetId> PlasticIds;
    Manager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("DiscMold")), MoldIds);
    Manager.GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("DiscPlastic")), PlasticIds);
    MoldIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });
    PlasticIds.Sort([](const FPrimaryAssetId& A, const FPrimaryAssetId& B) { return A.ToString() < B.ToString(); });

    if (MoldIds.IsEmpty() || PlasticIds.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Asset Manager found %d mold and %d plastic assets"), MoldIds.Num(), PlasticIds.Num());
        return false;
    }

    for (const FPrimaryAssetId& Id : MoldIds)
    {
        const FSoftObjectPath Path = Manager.GetPrimaryAssetPath(Id);
        const UDiscMoldDataAsset* Asset = Cast<UDiscMoldDataAsset>(Path.TryLoad());
        if (!Asset)
        {
            OutError = FString::Printf(TEXT("Could not load mold asset %s at %s"), *Id.ToString(), *Path.ToString());
            return false;
        }
        OutMolds.Add(Asset->Mold);
    }
    for (const FPrimaryAssetId& Id : PlasticIds)
    {
        const FSoftObjectPath Path = Manager.GetPrimaryAssetPath(Id);
        const UDiscPlasticDataAsset* Asset = Cast<UDiscPlasticDataAsset>(Path.TryLoad());
        if (!Asset)
        {
            OutError = FString::Printf(TEXT("Could not load plastic asset %s at %s"), *Id.ToString(), *Path.ToString());
            return false;
        }
        OutPlastics.Add(Asset->PlasticDefinition);
    }

    return ValidateDefinitions(OutMolds, OutPlastics, OutError);
}

bool UDiscCatalogSubsystem::SelectCatalogDefinitions(
    const TArray<FDiscMoldDefinition>& CandidateMolds,
    const TArray<FDiscPlasticDefinition>& CandidatePlastics,
    const TArray<FDiscMoldDefinition>& FallbackMolds,
    const TArray<FDiscPlasticDefinition>& FallbackPlastics,
    TArray<FDiscMoldDefinition>& OutMolds,
    TArray<FDiscPlasticDefinition>& OutPlastics,
    bool& bOutUsedFallback,
    FString& OutReason)
{
    FString CandidateError;
    bool bCandidateValid = ValidateDefinitions(CandidateMolds, CandidatePlastics, CandidateError);
    if (bCandidateValid)
    {
        for (const FDiscMoldDefinition& Required : FallbackMolds)
        {
            if (!CandidateMolds.ContainsByPredicate([&Required](const FDiscMoldDefinition& Candidate)
                { return Candidate.MoldId == Required.MoldId; }))
            {
                CandidateError = FString::Printf(TEXT("Primary asset catalog is missing required mold %s"),
                    *Required.MoldId.ToString());
                bCandidateValid = false;
                break;
            }
        }
    }

    if (bCandidateValid)
    {
        OutMolds = CandidateMolds;
        OutPlastics = CandidatePlastics;
        bOutUsedFallback = false;
        OutReason = TEXT("Primary asset catalog accepted");
        return true;
    }

    FString FallbackError;
    if (!ValidateDefinitions(FallbackMolds, FallbackPlastics, FallbackError))
    {
        OutMolds.Reset();
        OutPlastics.Reset();
        bOutUsedFallback = true;
        OutReason = FString::Printf(TEXT("Candidate invalid (%s); fallback invalid (%s)"),
            *CandidateError, *FallbackError);
        return false;
    }

    OutMolds = FallbackMolds;
    OutPlastics = FallbackPlastics;
    bOutUsedFallback = true;
    OutReason = CandidateError;
    return true;
}

bool UDiscCatalogSubsystem::ResolveDisc(FName MoldId, EDiscPlastic Plastic, FResolvedDiscDefinition& OutDisc) const
{
    return ResolveFromDefinitions(ActiveCatalog, ActivePlasticCatalog, MoldId, Plastic, OutDisc);
}
