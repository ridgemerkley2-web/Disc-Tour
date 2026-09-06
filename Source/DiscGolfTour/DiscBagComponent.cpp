#include "DiscBagComponent.h"
#include "DiscEquipmentSaveGame.h"
#include "DiscGolfMath.h"
#include "Kismet/GameplayStatics.h"

namespace
{
constexpr int32 DefaultBagCapacity = 24;
constexpr float MinDiscMassGrams = 130.0f;
constexpr float MaxDiscMassGrams = 200.0f;

const FName GenericBagId(TEXT("dg_generic_bag_default"));
const FName GenericStampId(TEXT("dg_generic_default"));
const FName BasePlasticId(TEXT("Base"));
const FName TourPlasticId(TEXT("Tour"));
const FName CrystalPlasticId(TEXT("Crystal"));

const TArray<FName>& GetSupportedMoldIds()
{
    static const TArray<FName> Ids = {
        TEXT("Apex"), TEXT("Vector"), TEXT("Line"), TEXT("Compass"), TEXT("Touch")
    };
    return Ids;
}

bool TryGetPlastic(FName PlasticId, EDiscPlastic& OutPlastic)
{
    if (PlasticId == BasePlasticId)
    {
        OutPlastic = EDiscPlastic::Base;
        return true;
    }
    if (PlasticId == TourPlasticId)
    {
        OutPlastic = EDiscPlastic::Tour;
        return true;
    }
    if (PlasticId == CrystalPlasticId)
    {
        OutPlastic = EDiscPlastic::Crystal;
        return true;
    }
    return false;
}

FName GetPlasticId(EDiscPlastic Plastic)
{
    switch (Plastic)
    {
        case EDiscPlastic::Base: return BasePlasticId;
        case EDiscPlastic::Tour: return TourPlasticId;
        case EDiscPlastic::Crystal: return CrystalPlasticId;
        default: return NAME_None;
    }
}

bool IsFiniteColor(const FLinearColor& Color)
{
    return FMath::IsFinite(Color.R)
        && FMath::IsFinite(Color.G)
        && FMath::IsFinite(Color.B)
        && FMath::IsFinite(Color.A);
}

FDGDiscInstance MakeGenericDisc(
    const FGuid& InstanceId,
    FName MoldId,
    float MassGrams,
    const FLinearColor& Color)
{
    FDGDiscInstance Disc;
    Disc.InstanceId = InstanceId;
    Disc.DiscDefinitionId = MoldId;
    Disc.PlasticId = TourPlasticId;
    Disc.MassGrams = MassGrams;
    Disc.Wear01 = 0.0f;
    Disc.Color = Color;
    Disc.StampId = GenericStampId;
    Disc.Nickname.Reset();
    Disc.bFavorite = false;
    return Disc;
}
}

UDiscBagComponent::UDiscBagComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    MoldIds = GetSupportedMoldIds();
    EquipmentLoadout = BuildDefaultLoadout();
}

void UDiscBagComponent::BeginPlay()
{
    Super::BeginPlay();
#if !DG_RELEASE_V05_SCOPE
    FString Error;
    if (!LoadEquipment(Error) && !Error.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("Equipment load rejected; using validated defaults: %s"), *Error);
    }
#endif
}

FDGDiscBagLoadout UDiscBagComponent::BuildDefaultLoadout()
{
    FDGDiscBagLoadout Loadout;
    Loadout.BagEquipmentId = GenericBagId;
    Loadout.Capacity = DefaultBagCapacity;
    Loadout.Discs = {
        MakeGenericDisc(FGuid(0xD6110001, 0x53455353, 0x494F4E31, 0x31000001), TEXT("Apex"), 175.0f, FLinearColor(0.91f, 0.18f, 0.13f, 1.0f)),
        MakeGenericDisc(FGuid(0xD6110002, 0x53455353, 0x494F4E31, 0x31000002), TEXT("Vector"), 175.0f, FLinearColor(0.12f, 0.42f, 0.92f, 1.0f)),
        MakeGenericDisc(FGuid(0xD6110003, 0x53455353, 0x494F4E31, 0x31000003), TEXT("Line"), 175.0f, FLinearColor(0.96f, 0.70f, 0.12f, 1.0f)),
        MakeGenericDisc(FGuid(0xD6110004, 0x53455353, 0x494F4E31, 0x31000004), TEXT("Compass"), 175.0f, FLinearColor(0.18f, 0.72f, 0.35f, 1.0f)),
        MakeGenericDisc(FGuid(0xD6110005, 0x53455353, 0x494F4E31, 0x31000005), TEXT("Touch"), 175.0f, FLinearColor(0.68f, 0.28f, 0.88f, 1.0f))
    };
    Loadout.SelectedDiscInstanceId = Loadout.Discs[0].InstanceId;
    return Loadout;
}

bool UDiscBagComponent::ValidateDiscInstance(const FDGDiscInstance& Instance, FString& OutError)
{
    OutError.Reset();
    if (!Instance.InstanceId.IsValid())
    {
        OutError = TEXT("Disc instance requires a valid stable InstanceId.");
        return false;
    }
    if (!GetSupportedMoldIds().Contains(Instance.DiscDefinitionId))
    {
        OutError = FString::Printf(TEXT("Unsupported DiscDefinitionId: %s."), *Instance.DiscDefinitionId.ToString());
        return false;
    }

    EDiscPlastic Plastic = EDiscPlastic::Tour;
    if (!TryGetPlastic(Instance.PlasticId, Plastic))
    {
        OutError = FString::Printf(TEXT("Unsupported PlasticId: %s."), *Instance.PlasticId.ToString());
        return false;
    }
    if (!FMath::IsFinite(Instance.MassGrams)
        || Instance.MassGrams < MinDiscMassGrams
        || Instance.MassGrams > MaxDiscMassGrams)
    {
        OutError = TEXT("Disc mass must be finite and between 130 and 200 grams.");
        return false;
    }
    if (!FMath::IsFinite(Instance.Wear01) || Instance.Wear01 < 0.0f || Instance.Wear01 > 1.0f)
    {
        OutError = TEXT("Disc wear must be finite and between zero and one.");
        return false;
    }
    if (!IsFiniteColor(Instance.Color))
    {
        OutError = TEXT("Disc color channels must be finite.");
        return false;
    }
    if (Instance.StampId != GenericStampId)
    {
        OutError = TEXT("Session 11 equipment requires the generic development stamp.");
        return false;
    }
    if (Instance.Nickname.Len() > DiscGolfMath::ResolvedDiscMaximumNicknameCharacters)
    {
        OutError = TEXT("Disc nickname must be no longer than 64 characters.");
        return false;
    }
    return true;
}

bool UDiscBagComponent::ValidateLoadout(const FDGDiscBagLoadout& Loadout, FString& OutError)
{
    OutError.Reset();
    if (Loadout.BagEquipmentId.IsNone())
    {
        OutError = TEXT("Bag loadout requires a stable equipment identity.");
        return false;
    }
    if (Loadout.Capacity < 1 || Loadout.Capacity > DefaultBagCapacity)
    {
        OutError = TEXT("Bag capacity must be between one and 24 discs.");
        return false;
    }
    if (Loadout.Discs.IsEmpty() || Loadout.Discs.Num() > Loadout.Capacity)
    {
        OutError = TEXT("Bag disc count must be non-zero and no greater than capacity.");
        return false;
    }
    if (!Loadout.SelectedDiscInstanceId.IsValid())
    {
        OutError = TEXT("Bag loadout requires one selected disc instance.");
        return false;
    }

    TSet<FGuid> InstanceIds;
    bool bSelectedDiscFound = false;
    for (const FDGDiscInstance& Disc : Loadout.Discs)
    {
        if (!ValidateDiscInstance(Disc, OutError))
        {
            return false;
        }
        if (InstanceIds.Contains(Disc.InstanceId))
        {
            OutError = FString::Printf(TEXT("Duplicate disc InstanceId: %s."), *Disc.InstanceId.ToString());
            return false;
        }
        InstanceIds.Add(Disc.InstanceId);
        bSelectedDiscFound |= Disc.InstanceId == Loadout.SelectedDiscInstanceId;
    }

    if (!bSelectedDiscFound)
    {
        OutError = TEXT("SelectedDiscInstanceId is not a member of the bag.");
        return false;
    }
    return true;
}

bool UDiscBagComponent::ApplyEquipmentLoadout(
    const FDGDiscBagLoadout& Loadout,
    FString& OutError)
{
    if (bEquipmentMutationLocked)
    {
        OutError = TEXT("Equipment cannot change during an active throw transaction.");
        return false;
    }

    if (!ValidateLoadout(Loadout, OutError))
    {
        return false;
    }

    const FDGDiscInstance* SelectedDisc = Loadout.Discs.FindByPredicate(
        [&Loadout](const FDGDiscInstance& Disc)
        {
            return Disc.InstanceId == Loadout.SelectedDiscInstanceId;
        });
    EDiscPlastic SelectedDiscPlastic = EDiscPlastic::Tour;
    const int32 NewSelectedIndex = SelectedDisc
        ? MoldIds.IndexOfByKey(SelectedDisc->DiscDefinitionId) : INDEX_NONE;
    if (!SelectedDisc || NewSelectedIndex == INDEX_NONE
        || !TryGetPlastic(SelectedDisc->PlasticId, SelectedDiscPlastic))
    {
        OutError = TEXT("Validated loadout selection could not map to the project catalog.");
        return false;
    }

    EquipmentLoadout = Loadout;
    SelectedIndex = NewSelectedIndex;
    SelectedPlastic = SelectedDiscPlastic;
    OutError.Reset();
    return true;
}

FString UDiscBagComponent::DefaultEquipmentSaveSlot()
{
#if DG_RELEASE_V05_SCOPE
    return FString();
#else
    return TEXT("DGT_Equipment_Dev_v1");
#endif
}

bool UDiscBagComponent::SaveEquipment(FString& OutError) const
{
#if DG_RELEASE_V05_SCOPE
    // The isolated Session 11 payload is development evidence, not an approved
    // v0.5 player-save schema. Shipping keeps the validated bag in memory and
    // persists the selected mold/plastic through the authoritative profile.
    OutError.Reset();
    return true;
#else
    return SaveEquipmentToSlot(DefaultEquipmentSaveSlot(), 0, OutError);
#endif
}

bool UDiscBagComponent::LoadEquipment(FString& OutError)
{
    if (bEquipmentMutationLocked)
    {
        OutError = TEXT("Equipment cannot load during an active throw transaction.");
        return false;
    }

#if DG_RELEASE_V05_SCOPE
    OutError.Reset();
    return true;
#else
    return LoadEquipmentFromSlot(DefaultEquipmentSaveSlot(), 0, OutError);
#endif
}

bool UDiscBagComponent::SaveEquipmentToSlot(
    const FString& SlotName,
    int32 UserIndex,
    FString& OutError) const
{
    OutError.Reset();
#if DG_RELEASE_V05_SCOPE
    (void)SlotName;
    (void)UserIndex;
    OutError = TEXT("Isolated equipment persistence is unavailable in this release build.");
    return false;
#else
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        OutError = TEXT("Equipment save requires a stable slot and non-negative user index.");
        return false;
    }
    if (!ValidateLoadout(EquipmentLoadout, OutError))
    {
        return false;
    }

    UDiscEquipmentSaveGame* Save = Cast<UDiscEquipmentSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UDiscEquipmentSaveGame::StaticClass()));
    if (!Save)
    {
        OutError = TEXT("Equipment save object could not be created.");
        return false;
    }
    Save->SchemaVersion = UDiscEquipmentSaveGame::CurrentSchemaVersion;
    Save->Loadout = EquipmentLoadout;
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex))
    {
        OutError = TEXT("Equipment save could not be written.");
        return false;
    }
    return true;
#endif
}

bool UDiscBagComponent::LoadEquipmentFromSlot(
    const FString& SlotName,
    int32 UserIndex,
    FString& OutError)
{
    if (bEquipmentMutationLocked)
    {
        OutError = TEXT("Equipment cannot load during an active throw transaction.");
        return false;
    }

    OutError.Reset();
#if DG_RELEASE_V05_SCOPE
    (void)SlotName;
    (void)UserIndex;
    OutError = TEXT("Isolated equipment persistence is unavailable in this release build.");
    return false;
#else
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        OutError = TEXT("Equipment load requires a stable slot and non-negative user index.");
        return false;
    }
    if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
    {
        return true;
    }

    const UDiscEquipmentSaveGame* Save = Cast<UDiscEquipmentSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    if (!Save || Save->SchemaVersion != UDiscEquipmentSaveGame::CurrentSchemaVersion)
    {
        OutError = TEXT("Equipment save schema is missing or unsupported.");
        return false;
    }

    // ApplyEquipmentLoadout validates the complete candidate before mutating any
    // active selection, so corrupt/missing content cannot partially leak in.
    return ApplyEquipmentLoadout(Save->Loadout, OutError);
#endif
}

void UDiscBagComponent::SelectDiscIndex(int32 NewIndex)
{
    if (bEquipmentMutationLocked || MoldIds.IsEmpty())
    {
        return;
    }
    CommitSelection(FMath::Clamp(NewIndex, 0, MoldIds.Num() - 1), SelectedPlastic);
}

bool UDiscBagComponent::SelectEquipment(FName MoldId, EDiscPlastic Plastic)
{
    if (bEquipmentMutationLocked)
    {
        return false;
    }

    const int32 MoldIndex = MoldIds.IndexOfByKey(MoldId);
    return MoldIndex != INDEX_NONE && CommitSelection(MoldIndex, Plastic);
}

bool UDiscBagComponent::CommitSelection(int32 NewIndex, EDiscPlastic NewPlastic)
{
    if (bEquipmentMutationLocked)
    {
        return false;
    }

    if (!MoldIds.IsValidIndex(NewIndex))
    {
        return false;
    }
    const FName PlasticId = GetPlasticId(NewPlastic);
    if (PlasticId.IsNone())
    {
        return false;
    }

    FDGDiscBagLoadout Candidate = EquipmentLoadout;
    FDGDiscInstance* CandidateDisc = Candidate.Discs.FindByPredicate(
        [this, NewIndex](const FDGDiscInstance& Disc)
        {
            return Disc.DiscDefinitionId == MoldIds[NewIndex];
        });
    if (!CandidateDisc)
    {
        return false;
    }

    CandidateDisc->PlasticId = PlasticId;
    Candidate.SelectedDiscInstanceId = CandidateDisc->InstanceId;
    FString Error;
    if (!ValidateLoadout(Candidate, Error))
    {
        return false;
    }

    EquipmentLoadout = MoveTemp(Candidate);
    SelectedIndex = NewIndex;
    SelectedPlastic = NewPlastic;
    return true;
}

void UDiscBagComponent::CyclePlastic()
{
    if (bEquipmentMutationLocked)
    {
        return;
    }

    EDiscPlastic NextPlastic = EDiscPlastic::Tour;
    switch (SelectedPlastic)
    {
        case EDiscPlastic::Base: NextPlastic = EDiscPlastic::Tour; break;
        case EDiscPlastic::Tour: NextPlastic = EDiscPlastic::Crystal; break;
        case EDiscPlastic::Crystal: NextPlastic = EDiscPlastic::Base; break;
        default: return;
    }
    CommitSelection(SelectedIndex, NextPlastic);
}

FName UDiscBagComponent::GetSelectedMoldId() const
{
    FDGDiscInstance SelectedDisc;
    return GetSelectedDiscInstance(SelectedDisc)
        ? SelectedDisc.DiscDefinitionId
        : (MoldIds.IsValidIndex(SelectedIndex) ? MoldIds[SelectedIndex] : NAME_None);
}

bool UDiscBagComponent::GetSelectedDiscInstance(FDGDiscInstance& OutDisc) const
{
    const FDGDiscInstance* SelectedDisc = EquipmentLoadout.Discs.FindByPredicate(
        [this](const FDGDiscInstance& Disc)
        {
            return Disc.InstanceId == EquipmentLoadout.SelectedDiscInstanceId;
        });
    if (!SelectedDisc)
    {
        return false;
    }
    OutDisc = *SelectedDisc;
    return true;
}

FGuid UDiscBagComponent::GetSelectedDiscInstanceId() const
{
    FDGDiscInstance SelectedDisc;
    return GetSelectedDiscInstance(SelectedDisc) ? SelectedDisc.InstanceId : FGuid();
}

bool UDiscBagComponent::SelectDiscInstance(FGuid InstanceId)
{
    if (bEquipmentMutationLocked)
    {
        return false;
    }

    const FDGDiscInstance* Disc = EquipmentLoadout.Discs.FindByPredicate(
        [InstanceId](const FDGDiscInstance& Candidate)
        {
            return Candidate.InstanceId == InstanceId;
        });
    if (!Disc)
    {
        return false;
    }

    EDiscPlastic Plastic = EDiscPlastic::Tour;
    const int32 MoldIndex = MoldIds.IndexOfByKey(Disc->DiscDefinitionId);
    if (MoldIndex == INDEX_NONE || !TryGetPlastic(Disc->PlasticId, Plastic))
    {
        return false;
    }

    FDGDiscBagLoadout Candidate = EquipmentLoadout;
    Candidate.SelectedDiscInstanceId = InstanceId;
    FString Error;
    if (!ValidateLoadout(Candidate, Error))
    {
        return false;
    }

    EquipmentLoadout = MoveTemp(Candidate);
    SelectedIndex = MoldIndex;
    SelectedPlastic = Plastic;
    return true;
}

bool UDiscBagComponent::ToggleSelectedFavorite()
{
    if (bEquipmentMutationLocked)
    {
        return false;
    }

    FDGDiscBagLoadout Candidate = EquipmentLoadout;
    FDGDiscInstance* Disc = Candidate.Discs.FindByPredicate(
        [&Candidate](const FDGDiscInstance& Entry)
        {
            return Entry.InstanceId == Candidate.SelectedDiscInstanceId;
        });
    if (!Disc)
    {
        return false;
    }

    Disc->bFavorite = !Disc->bFavorite;
    FString Error;
    if (!ValidateLoadout(Candidate, Error))
    {
        return false;
    }
    EquipmentLoadout = MoveTemp(Candidate);
    return true;
}

FString UDiscBagComponent::GetSelectedEquipmentStatusText() const
{
    FDGDiscInstance Disc;
    if (!GetSelectedDiscInstance(Disc))
    {
        return TEXT("No valid disc selected");
    }

    return FString::Printf(
        TEXT("%s | %s | %.0f g | Wear %.0f%% | %s"),
        *Disc.DiscDefinitionId.ToString(),
        *Disc.PlasticId.ToString(),
        Disc.MassGrams,
        Disc.Wear01 * 100.0f,
        Disc.bFavorite ? TEXT("Favorite") : TEXT("Not favorite"));
}

bool UDiscBagComponent::ResolveSelectedDiscInstance(
    const FResolvedDiscDefinition& CatalogDisc,
    FResolvedDiscDefinition& OutResolved,
    FString& OutError) const
{
    FDGDiscInstance Disc;
    if (!GetSelectedDiscInstance(Disc))
    {
        OutError = TEXT("The bag has no selected disc instance.");
        return false;
    }
    return ResolveDiscInstance(Disc, CatalogDisc, OutResolved, OutError);
}

bool UDiscBagComponent::ResolveDiscInstance(
    const FDGDiscInstance& Instance,
    const FResolvedDiscDefinition& CatalogDisc,
    FResolvedDiscDefinition& OutResolved,
    FString& OutError)
{
    OutError.Reset();
    if (!ValidateDiscInstance(Instance, OutError))
    {
        return false;
    }
    if (CatalogDisc.MoldId != Instance.DiscDefinitionId)
    {
        OutError = TEXT("Disc instance definition does not match the catalog-resolved mold.");
        return false;
    }

    EDiscPlastic InstancePlastic = EDiscPlastic::Tour;
    if (!TryGetPlastic(Instance.PlasticId, InstancePlastic) || CatalogDisc.Plastic != InstancePlastic)
    {
        OutError = TEXT("Disc instance plastic does not match the catalog-resolved plastic.");
        return false;
    }
    FString CatalogError;
    if (!DiscGolfMath::IsResolvedDiscDefinitionValid(CatalogDisc, &CatalogError))
    {
        OutResolved = FResolvedDiscDefinition();
        OutError = FString::Printf(
            TEXT("Catalog disc failed runtime validation: %s"), *CatalogError);
        return false;
    }

    FResolvedDiscDefinition Candidate = CatalogDisc;
    const float CatalogMassGrams = CatalogDisc.Aero.MassKg * 1000.0f;
    if (!FMath::IsNearlyEqual(Instance.MassGrams, CatalogMassGrams, 0.0001f))
    {
        const float InstanceMassKg = Instance.MassGrams / 1000.0f;
        const float MassScale = InstanceMassKg / CatalogDisc.Aero.MassKg;
        Candidate.Aero.MassKg = InstanceMassKg;
        Candidate.Aero.InertiaAxialKgM2 *= MassScale;
        Candidate.Aero.InertiaPlanarKgM2 *= MassScale;
    }

    Candidate.DiscInstanceId = Instance.InstanceId;
    Candidate.DiscMassGrams = Instance.MassGrams;
    Candidate.DiscWear01 = Instance.Wear01;
    Candidate.DiscColor = Instance.Color;
    Candidate.DiscStampId = Instance.StampId;
    Candidate.DiscNickname = Instance.Nickname;
    Candidate.bDiscFavorite = Instance.bFavorite;
    Candidate.bWearAffectsPhysics = false;

    FString ResolvedError;
    if (!DiscGolfMath::IsResolvedDiscDefinitionValid(Candidate, &ResolvedError))
    {
        OutResolved = FResolvedDiscDefinition();
        OutError = FString::Printf(
            TEXT("Resolved disc instance failed runtime validation: %s"), *ResolvedError);
        return false;
    }

    OutResolved = MoveTemp(Candidate);
    return true;
}
