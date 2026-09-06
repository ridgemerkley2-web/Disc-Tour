#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscBagComponent.h"
#include "../DiscCatalogSubsystem.h"
#include "../DiscEquipmentSaveGame.h"
#include "../DiscGolfMath.h"
#include "Kismet/GameplayStatics.h"

#include <limits>

namespace
{
bool ResolveFallbackDisc(FName MoldId, EDiscPlastic Plastic, FResolvedDiscDefinition& OutDisc)
{
    TArray<FDiscMoldDefinition> Molds;
    TArray<FDiscPlasticDefinition> Plastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);
    return UDiscCatalogSubsystem::ResolveFromDefinitions(Molds, Plastics, MoldId, Plastic, OutDisc);
}

bool NearlyEqualAero(const FDiscAeroProfile& A, const FDiscAeroProfile& B)
{
    return FMath::IsNearlyEqual(A.MassKg, B.MassKg)
        && FMath::IsNearlyEqual(A.DiameterM, B.DiameterM)
        && FMath::IsNearlyEqual(A.AreaM2, B.AreaM2)
        && FMath::IsNearlyEqual(A.InertiaAxialKgM2, B.InertiaAxialKgM2)
        && FMath::IsNearlyEqual(A.InertiaPlanarKgM2, B.InertiaPlanarKgM2)
        && FMath::IsNearlyEqual(A.CL0, B.CL0)
        && FMath::IsNearlyEqual(A.CLa, B.CLa)
        && FMath::IsNearlyEqual(A.CD0, B.CD0)
        && FMath::IsNearlyEqual(A.CDa, B.CDa)
        && FMath::IsNearlyEqual(A.CM0, B.CM0)
        && FMath::IsNearlyEqual(A.CMa, B.CMa)
        && FMath::IsNearlyEqual(A.HighSpeedTurnMomentNm, B.HighSpeedTurnMomentNm)
        && FMath::IsNearlyEqual(A.LowSpeedFadeMomentNm, B.LowSpeedFadeMomentNm)
        && FMath::IsNearlyEqual(A.TurnStartsAboveMps, B.TurnStartsAboveMps)
        && FMath::IsNearlyEqual(A.FadeStartsBelowMps, B.FadeStartsBelowMps)
        && FMath::IsNearlyEqual(A.SpinDecayPerSecond, B.SpinDecayPerSecond)
        && FMath::IsNearlyEqual(A.GroundRestitution, B.GroundRestitution)
        && FMath::IsNearlyEqual(A.GroundFriction, B.GroundFriction);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentDefaultLoadoutTest,
    "DiscGolfTour.Session11.Equipment.DefaultLoadoutIsStableAndGeneric",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentDefaultLoadoutTest::RunTest(const FString& Parameters)
{
    const FDGDiscBagLoadout First = UDiscBagComponent::BuildDefaultLoadout();
    const FDGDiscBagLoadout Second = UDiscBagComponent::BuildDefaultLoadout();
    FString Error;
    TestTrue(TEXT("Default loadout validates"), UDiscBagComponent::ValidateLoadout(First, Error));
    if (!Error.IsEmpty())
    {
        AddInfo(Error);
    }
    TestEqual(TEXT("Default bag has five generic discs"), First.Discs.Num(), 5);
    TestEqual(TEXT("Default bag has bounded capacity"), First.Capacity, 24);
    TestTrue(TEXT("Default selection is a bag member"),
        First.Discs.ContainsByPredicate(
            [&First](const FDGDiscInstance& Disc)
            {
                return Disc.InstanceId == First.SelectedDiscInstanceId;
            }));

    for (int32 Index = 0; Index < First.Discs.Num(); ++Index)
    {
        TestTrue(FString::Printf(TEXT("Disc %d uses a valid deterministic GUID"), Index),
            First.Discs[Index].InstanceId.IsValid());
        TestTrue(FString::Printf(TEXT("Disc %d identity is stable across construction"), Index),
            First.Discs[Index].InstanceId == Second.Discs[Index].InstanceId);
        TestEqual(FString::Printf(TEXT("Disc %d uses the generic stamp"), Index),
            First.Discs[Index].StampId, FName(TEXT("dg_generic_default")));
        TestEqual(FString::Printf(TEXT("Disc %d defaults to Tour plastic"), Index),
            First.Discs[Index].PlasticId, FName(TEXT("Tour")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentLoadoutValidationTest,
    "DiscGolfTour.Session11.Equipment.LoadoutValidationRejectsInvalidIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentLoadoutValidationTest::RunTest(const FString& Parameters)
{
    FString Error;
    const FDGDiscBagLoadout Valid = UDiscBagComponent::BuildDefaultLoadout();

    FDGDiscBagLoadout Invalid = Valid;
    Invalid.Capacity = 4;
    TestFalse(TEXT("Disc count above capacity is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[1].InstanceId = Invalid.Discs[0].InstanceId;
    TestFalse(TEXT("Duplicate instance identity is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.SelectedDiscInstanceId = FGuid(0xBAD00001, 0xBAD00002, 0xBAD00003, 0xBAD00004);
    TestFalse(TEXT("Selection outside the bag is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].MassGrams = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite mass is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].Wear01 = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite wear is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].Color.R = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite color is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].MassGrams = 129.99f;
    TestFalse(TEXT("Mass below 130 grams is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].MassGrams = 200.01f;
    TestFalse(TEXT("Mass above 200 grams is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].PlasticId = TEXT("Unknown");
    TestFalse(TEXT("Unknown plastic identity is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    Invalid = Valid;
    Invalid.Discs[0].StampId = TEXT("unapproved_default");
    TestFalse(TEXT("Non-generic development stamp is rejected"), UDiscBagComponent::ValidateLoadout(Invalid, Error));

    FDGDiscBagLoadout NicknameBoundary = Valid;
    NicknameBoundary.Discs[0].Nickname = FString::ChrN(64, TEXT('N'));
    TestTrue(TEXT("A 64-character nickname remains valid"),
        UDiscBagComponent::ValidateLoadout(NicknameBoundary, Error));
    NicknameBoundary.Discs[0].Nickname.AppendChar(TEXT('N'));
    TestFalse(TEXT("A 65-character nickname is rejected"),
        UDiscBagComponent::ValidateLoadout(NicknameBoundary, Error));
    TestTrue(TEXT("Nickname rejection reports the shared bound"),
        Error.Contains(TEXT("64")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentLegacySelectionTest,
    "DiscGolfTour.Session11.Equipment.LegacySelectionIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentLegacySelectionTest::RunTest(const FString& Parameters)
{
    UDiscBagComponent* Bag = NewObject<UDiscBagComponent>();
    TestTrue(TEXT("Legacy mold/plastic selection succeeds"),
        Bag->SelectEquipment(TEXT("Line"), EDiscPlastic::Crystal));

    FDGDiscInstance Selected;
    TestTrue(TEXT("Selected instance is available"), Bag->GetSelectedDiscInstance(Selected));
    TestEqual(TEXT("Legacy mold maps to exact selected instance"), Selected.DiscDefinitionId, FName(TEXT("Line")));
    TestEqual(TEXT("Legacy plastic maps to exact selected instance"), Selected.PlasticId, FName(TEXT("Crystal")));
    TestTrue(TEXT("Selected GUID matches exact selected instance"),
        Bag->GetSelectedDiscInstanceId() == Selected.InstanceId);

    const FGuid BeforeRejectedSelection = Selected.InstanceId;
    TestFalse(TEXT("Unknown mold is rejected"),
        Bag->SelectEquipment(TEXT("Unknown"), EDiscPlastic::Tour));
    TestFalse(TEXT("Invalid plastic enum is rejected"),
        Bag->SelectEquipment(TEXT("Apex"), static_cast<EDiscPlastic>(255)));
    TestTrue(TEXT("Rejected legacy selections preserve exact identity"),
        Bag->GetSelectedDiscInstanceId() == BeforeRejectedSelection);
    TestEqual(TEXT("Rejected legacy selections preserve plastic"),
        Bag->GetSelectedPlastic(), EDiscPlastic::Crystal);

    Bag->SelectDiscIndex(1);
    TestTrue(TEXT("Legacy index selects Vector instance"), Bag->GetSelectedDiscInstance(Selected));
    TestEqual(TEXT("Legacy index changes mold"), Selected.DiscDefinitionId, FName(TEXT("Vector")));
    TestEqual(TEXT("Legacy index preserves active plastic"), Selected.PlasticId, FName(TEXT("Crystal")));

    Bag->CyclePlastic();
    TestTrue(TEXT("Cycle retains exact selected instance"), Bag->GetSelectedDiscInstance(Selected));
    TestEqual(TEXT("Crystal cycles to Base"), Selected.PlasticId, FName(TEXT("Base")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentFavoriteTest,
    "DiscGolfTour.Session11.Equipment.FavoriteToggleIsInstanceScoped",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentFavoriteTest::RunTest(const FString& Parameters)
{
    UDiscBagComponent* Bag = NewObject<UDiscBagComponent>();
    const FGuid ApexId = Bag->GetSelectedDiscInstanceId();
    TestTrue(TEXT("Selected favorite toggles on"), Bag->ToggleSelectedFavorite());

    FDGDiscInstance Apex;
    TestTrue(TEXT("Favorite instance remains selected"), Bag->GetSelectedDiscInstance(Apex));
    TestTrue(TEXT("Selected instance is marked favorite"), Apex.bFavorite);
    TestTrue(TEXT("Status exposes favorite state"), Bag->GetSelectedEquipmentStatusText().Contains(TEXT("Favorite")));

    Bag->SelectDiscIndex(1);
    FDGDiscInstance Vector;
    TestTrue(TEXT("Second instance can be selected"), Bag->GetSelectedDiscInstance(Vector));
    TestFalse(TEXT("Favorite does not leak to a second instance"), Vector.bFavorite);
    TestTrue(TEXT("Exact instance API can restore Apex"), Bag->SelectDiscInstance(ApexId));
    TestTrue(TEXT("Restored Apex remains favorite"), Bag->GetSelectedDiscInstance(Apex) && Apex.bFavorite);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentMassResolutionTest,
    "DiscGolfTour.Session11.Equipment.MassResolutionPreservesParityAndWearBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentMassResolutionTest::RunTest(const FString& Parameters)
{
    FResolvedDiscDefinition CatalogDisc;
    TestTrue(TEXT("Fallback Apex/Tour resolves"),
        ResolveFallbackDisc(TEXT("Apex"), EDiscPlastic::Tour, CatalogDisc));

    FDGDiscInstance Instance = UDiscBagComponent::BuildDefaultLoadout().Discs[0];
    FResolvedDiscDefinition At175;
    FString Error;
    TestTrue(TEXT("175 gram instance resolves"),
        UDiscBagComponent::ResolveDiscInstance(Instance, CatalogDisc, At175, Error));
    if (!Error.IsEmpty())
    {
        AddInfo(Error);
    }
    TestTrue(TEXT("175 gram instance preserves baseline aero exactly"), NearlyEqualAero(At175.Aero, CatalogDisc.Aero));
    TestTrue(TEXT("Resolved snapshot has stable instance identity"), At175.DiscInstanceId == Instance.InstanceId);
    TestEqual(TEXT("Resolved snapshot has mass metadata"), At175.DiscMassGrams, 175.0f);
    TestFalse(TEXT("Wear physics is explicitly uncalibrated"), At175.bWearAffectsPhysics);
    TestTrue(TEXT("Resolved equipment snapshot satisfies the shared runtime contract"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(At175));

    Instance.MassGrams = 140.0f;
    Instance.Wear01 = 0.9f;
    Instance.Nickname = TEXT("Lab control");
    Instance.bFavorite = true;
    FResolvedDiscDefinition At140;
    TestTrue(TEXT("140 gram instance resolves"),
        UDiscBagComponent::ResolveDiscInstance(Instance, CatalogDisc, At140, Error));
    const float ExpectedScale = 140.0f / 175.0f;
    TestTrue(TEXT("Mass converts from grams to SI kilograms"), FMath::IsNearlyEqual(At140.Aero.MassKg, 0.140f));
    TestTrue(TEXT("Axial inertia scales proportionally with mass"),
        FMath::IsNearlyEqual(At140.Aero.InertiaAxialKgM2, CatalogDisc.Aero.InertiaAxialKgM2 * ExpectedScale));
    TestTrue(TEXT("Planar inertia scales proportionally with mass"),
        FMath::IsNearlyEqual(At140.Aero.InertiaPlanarKgM2, CatalogDisc.Aero.InertiaPlanarKgM2 * ExpectedScale));
    TestTrue(TEXT("Wear does not alter the aerodynamic coefficient family"),
        FMath::IsNearlyEqual(At140.Aero.CL0, CatalogDisc.Aero.CL0)
        && FMath::IsNearlyEqual(At140.Aero.CLa, CatalogDisc.Aero.CLa)
        && FMath::IsNearlyEqual(At140.Aero.CD0, CatalogDisc.Aero.CD0)
        && FMath::IsNearlyEqual(At140.Aero.CDa, CatalogDisc.Aero.CDa)
        && FMath::IsNearlyEqual(At140.Aero.HighSpeedTurnMomentNm, CatalogDisc.Aero.HighSpeedTurnMomentNm)
        && FMath::IsNearlyEqual(At140.Aero.LowSpeedFadeMomentNm, CatalogDisc.Aero.LowSpeedFadeMomentNm));
    TestEqual(TEXT("Wear is retained as metadata"), At140.DiscWear01, 0.9f);
    TestEqual(TEXT("Nickname is retained as metadata"), At140.DiscNickname, FString(TEXT("Lab control")));
    TestTrue(TEXT("Favorite is retained as metadata"), At140.bDiscFavorite);
    TestTrue(TEXT("Mass-adjusted equipment snapshot satisfies the shared runtime contract"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(At140));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentResolutionMismatchTest,
    "DiscGolfTour.Session11.Equipment.ResolutionRejectsCatalogMismatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentResolutionMismatchTest::RunTest(const FString& Parameters)
{
    FResolvedDiscDefinition CatalogDisc;
    TestTrue(TEXT("Fallback Apex/Tour resolves"),
        ResolveFallbackDisc(TEXT("Apex"), EDiscPlastic::Tour, CatalogDisc));

    const FDGDiscInstance Apex = UDiscBagComponent::BuildDefaultLoadout().Discs[0];
    FResolvedDiscDefinition Output;
    Output.MoldId = TEXT("Sentinel");
    FString Error;

    FDGDiscInstance MismatchedMold = Apex;
    MismatchedMold.DiscDefinitionId = TEXT("Vector");
    TestFalse(TEXT("Catalog/instance mold mismatch is rejected"),
        UDiscBagComponent::ResolveDiscInstance(MismatchedMold, CatalogDisc, Output, Error));
    TestEqual(TEXT("Rejected mold mismatch does not mutate output"), Output.MoldId, FName(TEXT("Sentinel")));

    FDGDiscInstance MismatchedPlastic = Apex;
    MismatchedPlastic.PlasticId = TEXT("Base");
    TestFalse(TEXT("Catalog/instance plastic mismatch is rejected"),
        UDiscBagComponent::ResolveDiscInstance(MismatchedPlastic, CatalogDisc, Output, Error));
    TestEqual(TEXT("Rejected plastic mismatch does not mutate output"), Output.MoldId, FName(TEXT("Sentinel")));

    FResolvedDiscDefinition UnsafeScalingCatalog = CatalogDisc;
    UnsafeScalingCatalog.DiscMassGrams = 130.0f;
    UnsafeScalingCatalog.Aero.MassKg = 0.130f;
    UnsafeScalingCatalog.Aero.InertiaAxialKgM2 = 0.8f;
    UnsafeScalingCatalog.Aero.InertiaPlanarKgM2 = 0.8f;
    TestTrue(TEXT("Pre-scaling catalog fixture satisfies the shared runtime contract"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(UnsafeScalingCatalog));

    FDGDiscInstance HeavyInstance = Apex;
    HeavyInstance.MassGrams = 200.0f;
    Output.MoldId = TEXT("Sentinel");
    TestFalse(TEXT("A mass adjustment that produces unsafe inertia is rejected"),
        UDiscBagComponent::ResolveDiscInstance(
            HeavyInstance, UnsafeScalingCatalog, Output, Error));
    TestTrue(TEXT("Rejected final snapshot is reset instead of partially published"),
        Output.MoldId.IsNone());
    TestTrue(TEXT("Final shared-contract rejection identifies the unsafe field"),
        Error.Contains(TEXT("Aero.InertiaAxialKgM2")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscEquipmentSeparateSaveTest,
    "DiscGolfTour.Session11.Equipment.SeparateSchemaV1SaveIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscEquipmentSeparateSaveTest::RunTest(const FString& Parameters)
{
    const FString Slot = FString::Printf(TEXT("DGT_S11_EquipmentTest_%s"),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));
    constexpr int32 UserIndex = 0;

    UDiscBagComponent* Source = NewObject<UDiscBagComponent>();
    TestTrue(TEXT("Source selects Line Crystal"),
        Source->SelectEquipment(TEXT("Line"), EDiscPlastic::Crystal));
    TestTrue(TEXT("Source favorite toggles"), Source->ToggleSelectedFavorite());
    const FGuid ExpectedInstanceId = Source->GetSelectedDiscInstanceId();
    FString Error;
    TestTrue(TEXT("Validated equipment writes a separate schema-v1 slot"),
        Source->SaveEquipmentToSlot(Slot, UserIndex, Error));
    if (!Error.IsEmpty()) AddInfo(Error);

    UDiscBagComponent* Restored = NewObject<UDiscBagComponent>();
    TestTrue(TEXT("Separate equipment slot reloads"),
        Restored->LoadEquipmentFromSlot(Slot, UserIndex, Error));
    FDGDiscInstance RestoredDisc;
    TestTrue(TEXT("Restored selection resolves"), Restored->GetSelectedDiscInstance(RestoredDisc));
    TestTrue(TEXT("Stable instance identity survives serialization"),
        RestoredDisc.InstanceId == ExpectedInstanceId);
    TestEqual(TEXT("Plastic survives serialization"), RestoredDisc.PlasticId, FName(TEXT("Crystal")));
    TestTrue(TEXT("Favorite survives serialization"), RestoredDisc.bFavorite);

    UDiscEquipmentSaveGame* Corrupt = Cast<UDiscEquipmentSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UDiscEquipmentSaveGame::StaticClass()));
    if (!TestNotNull(TEXT("Corrupt save fixture is created"), Corrupt))
    {
        UGameplayStatics::DeleteGameInSlot(Slot, UserIndex);
        return false;
    }
    Corrupt->Loadout = UDiscBagComponent::BuildDefaultLoadout();
    Corrupt->Loadout.SelectedDiscInstanceId = FGuid::NewGuid();
    TestTrue(TEXT("Corrupt fixture is written to the disposable test slot"),
        UGameplayStatics::SaveGameToSlot(Corrupt, Slot, UserIndex));
    const FGuid BeforeRejectedLoad = Restored->GetSelectedDiscInstanceId();
    TestFalse(TEXT("Invalid selected identity is rejected on load"),
        Restored->LoadEquipmentFromSlot(Slot, UserIndex, Error));
    TestTrue(TEXT("Rejected load is atomic"),
        Restored->GetSelectedDiscInstanceId() == BeforeRejectedLoad);
    TestTrue(TEXT("Rejected load reports selected-membership failure"),
        Error.Contains(TEXT("not a member")));

    TestTrue(TEXT("Disposable equipment test slot is removed"),
        UGameplayStatics::DeleteGameInSlot(Slot, UserIndex));
    return true;
}

#endif
