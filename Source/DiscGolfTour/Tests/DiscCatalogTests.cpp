#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscCatalogSubsystem.h"
#include "../DiscBagComponent.h"
#include "../DiscGolfMath.h"

#include <limits>

namespace
{
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

bool NearlyEqualResolved(const FResolvedDiscDefinition& A, const FResolvedDiscDefinition& B)
{
    return A.MoldId == B.MoldId
        && A.DisplayName.ToString() == B.DisplayName.ToString()
        && A.Speed == B.Speed && A.Glide == B.Glide
        && FMath::IsNearlyEqual(A.Turn, B.Turn)
        && FMath::IsNearlyEqual(A.Fade, B.Fade)
        && A.Plastic == B.Plastic
        && FMath::IsNearlyEqual(A.DiscMassGrams, B.DiscMassGrams)
        && NearlyEqualAero(A.Aero, B.Aero);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogPrimaryAssetsRegisteredTest,
    "DiscGolfTour.Data.Catalog.PrimaryAssetsRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogPrimaryAssetsRegisteredTest::RunTest(const FString& Parameters)
{
    TArray<FDiscMoldDefinition> Molds;
    TArray<FDiscPlasticDefinition> Plastics;
    FString Error;
    TestTrue(TEXT("Asset Manager loads a valid disc catalog"),
        UDiscCatalogSubsystem::LoadPrimaryAssetDefinitions(Molds, Plastics, Error));
    if (!Error.IsEmpty()) AddInfo(Error);
    TestEqual(TEXT("Five cooked mold assets are registered"), Molds.Num(), 5);
    TestEqual(TEXT("Three cooked plastic assets are registered"), Plastics.Num(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogAssetFallbackParityTest,
    "DiscGolfTour.Data.Catalog.AssetFallbackParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogAssetFallbackParityTest::RunTest(const FString& Parameters)
{
    TArray<FDiscMoldDefinition> Assets;
    TArray<FDiscPlasticDefinition> AssetPlastics;
    FString Error;
    if (!UDiscCatalogSubsystem::LoadPrimaryAssetDefinitions(Assets, AssetPlastics, Error))
    {
        AddError(Error);
        return false;
    }

    TArray<FDiscMoldDefinition> Fallback;
    TArray<FDiscPlasticDefinition> FallbackPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Fallback, FallbackPlastics);
    const EDiscPlastic PlasticTypes[] = { EDiscPlastic::Base, EDiscPlastic::Tour, EDiscPlastic::Crystal };
    for (const FDiscMoldDefinition& Mold : Fallback)
    {
        for (EDiscPlastic Plastic : PlasticTypes)
        {
            FResolvedDiscDefinition FromAsset;
            FResolvedDiscDefinition FromFallback;
            const bool bAssetResolved = UDiscCatalogSubsystem::ResolveFromDefinitions(
                Assets, AssetPlastics, Mold.MoldId, Plastic, FromAsset);
            const bool bFallbackResolved = UDiscCatalogSubsystem::ResolveFromDefinitions(
                Fallback, FallbackPlastics, Mold.MoldId, Plastic, FromFallback);
            TestTrue(FString::Printf(TEXT("Asset resolves %s/%d"), *Mold.MoldId.ToString(), static_cast<int32>(Plastic)),
                bAssetResolved);
            TestTrue(FString::Printf(TEXT("Fallback resolves %s/%d"), *Mold.MoldId.ToString(), static_cast<int32>(Plastic)),
                bFallbackResolved);
            TestTrue(FString::Printf(TEXT("Asset/fallback parity for %s/%d"), *Mold.MoldId.ToString(), static_cast<int32>(Plastic)),
                NearlyEqualResolved(FromAsset, FromFallback));
            TestTrue(FString::Printf(TEXT("Asset snapshot satisfies the shared runtime contract for %s/%d"),
                *Mold.MoldId.ToString(), static_cast<int32>(Plastic)),
                bAssetResolved && DiscGolfMath::IsResolvedDiscDefinitionValid(FromAsset));
            TestTrue(FString::Printf(TEXT("Fallback snapshot satisfies the shared runtime contract for %s/%d"),
                *Mold.MoldId.ToString(), static_cast<int32>(Plastic)),
                bFallbackResolved && DiscGolfMath::IsResolvedDiscDefinitionValid(FromFallback));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogDuplicateIdsTest,
    "DiscGolfTour.Data.Catalog.DuplicateIdsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogDuplicateIdsTest::RunTest(const FString& Parameters)
{
    TArray<FDiscMoldDefinition> Molds;
    TArray<FDiscPlasticDefinition> Plastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);
    const FDiscMoldDefinition Duplicate = Molds[0];
    Molds.Add(Duplicate);
    FString Error;
    TestFalse(TEXT("Duplicate mold IDs fail validation"),
        UDiscCatalogSubsystem::ValidateDefinitions(Molds, Plastics, Error));
    TestTrue(TEXT("Duplicate failure identifies the mold"), Error.Contains(TEXT("Duplicate mold ID: Apex")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogMissingIdsTest,
    "DiscGolfTour.Data.Catalog.MissingIdsRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogMissingIdsTest::RunTest(const FString& Parameters)
{
    TArray<FDiscMoldDefinition> Molds;
    TArray<FDiscPlasticDefinition> Plastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);
    Molds[2].MoldId = NAME_None;
    FString Error;
    TestFalse(TEXT("A missing stable mold ID fails validation"),
        UDiscCatalogSubsystem::ValidateDefinitions(Molds, Plastics, Error));
    TestTrue(TEXT("Missing-ID failure is explicit"), Error.Contains(TEXT("no stable MoldId")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogNonFiniteAeroRejectedTest,
    "DiscGolfTour.Data.Catalog.NonFiniteAeroRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogNonFiniteAeroRejectedTest::RunTest(const FString& Parameters)
{
    struct FFieldCase
    {
        const TCHAR* Name;
        float FDiscAeroProfile::* Member;
    };

    const FFieldCase Fields[] = {
        { TEXT("MassKg"), &FDiscAeroProfile::MassKg },
        { TEXT("DiameterM"), &FDiscAeroProfile::DiameterM },
        { TEXT("AreaM2"), &FDiscAeroProfile::AreaM2 },
        { TEXT("InertiaAxialKgM2"), &FDiscAeroProfile::InertiaAxialKgM2 },
        { TEXT("InertiaPlanarKgM2"), &FDiscAeroProfile::InertiaPlanarKgM2 },
        { TEXT("CL0"), &FDiscAeroProfile::CL0 },
        { TEXT("CLa"), &FDiscAeroProfile::CLa },
        { TEXT("CD0"), &FDiscAeroProfile::CD0 },
        { TEXT("CDa"), &FDiscAeroProfile::CDa },
        { TEXT("CM0"), &FDiscAeroProfile::CM0 },
        { TEXT("CMa"), &FDiscAeroProfile::CMa },
        { TEXT("HighSpeedTurnMomentNm"), &FDiscAeroProfile::HighSpeedTurnMomentNm },
        { TEXT("LowSpeedFadeMomentNm"), &FDiscAeroProfile::LowSpeedFadeMomentNm },
        { TEXT("TurnStartsAboveMps"), &FDiscAeroProfile::TurnStartsAboveMps },
        { TEXT("FadeStartsBelowMps"), &FDiscAeroProfile::FadeStartsBelowMps },
        { TEXT("SpinDecayPerSecond"), &FDiscAeroProfile::SpinDecayPerSecond },
        { TEXT("GroundRestitution"), &FDiscAeroProfile::GroundRestitution },
        { TEXT("GroundFriction"), &FDiscAeroProfile::GroundFriction }
    };
    const float NonFiniteValues[] = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity()
    };
    const TCHAR* ValueNames[] = { TEXT("NaN"), TEXT("Infinity") };

    for (const FFieldCase& Field : Fields)
    {
        for (int32 ValueIndex = 0; ValueIndex < UE_ARRAY_COUNT(NonFiniteValues); ++ValueIndex)
        {
            TArray<FDiscMoldDefinition> Molds;
            TArray<FDiscPlasticDefinition> Plastics;
            UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);
            (Molds[0].Aero).*(Field.Member) = NonFiniteValues[ValueIndex];

            FString Error;
            const FString Context = FString::Printf(
                TEXT("%s %s"), Field.Name, ValueNames[ValueIndex]);
            TestFalse(*FString::Printf(TEXT("%s fails catalog validation"), *Context),
                UDiscCatalogSubsystem::ValidateDefinitions(Molds, Plastics, Error));
            TestTrue(*FString::Printf(TEXT("%s identifies the invalid field"), *Context),
                Error.Contains(Field.Name));

            FResolvedDiscDefinition Resolved;
            Resolved.MoldId = TEXT("Sentinel");
            TestFalse(*FString::Printf(TEXT("%s cannot resolve"), *Context),
                UDiscCatalogSubsystem::ResolveFromDefinitions(
                    Molds, Plastics, TEXT("Apex"), EDiscPlastic::Tour, Resolved));
            TestTrue(*FString::Printf(TEXT("%s leaves no partial resolved disc"), *Context),
                Resolved.MoldId.IsNone());
        }
    }

    struct FPlasticFieldCase
    {
        const TCHAR* Name;
        float FDiscPlasticDefinition::* Member;
    };
    const FPlasticFieldCase PlasticFields[] = {
        { TEXT("HighSpeedTurnMomentScale"), &FDiscPlasticDefinition::HighSpeedTurnMomentScale },
        { TEXT("LowSpeedFadeMomentScale"), &FDiscPlasticDefinition::LowSpeedFadeMomentScale },
        { TEXT("GroundRestitutionScale"), &FDiscPlasticDefinition::GroundRestitutionScale },
        { TEXT("GroundFrictionScale"), &FDiscPlasticDefinition::GroundFrictionScale }
    };
    for (const FPlasticFieldCase& Field : PlasticFields)
    {
        for (int32 ValueIndex = 0; ValueIndex < UE_ARRAY_COUNT(NonFiniteValues); ++ValueIndex)
        {
            TArray<FDiscMoldDefinition> Molds;
            TArray<FDiscPlasticDefinition> Plastics;
            UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);
            (Plastics[0]).*(Field.Member) = NonFiniteValues[ValueIndex];

            FString Error;
            const FString Context = FString::Printf(
                TEXT("%s %s"), Field.Name, ValueNames[ValueIndex]);
            TestFalse(*FString::Printf(TEXT("%s fails catalog validation"), *Context),
                UDiscCatalogSubsystem::ValidateDefinitions(Molds, Plastics, Error));
            FResolvedDiscDefinition Resolved;
            TestFalse(*FString::Printf(TEXT("%s cannot resolve"), *Context),
                UDiscCatalogSubsystem::ResolveFromDefinitions(
                    Molds, Plastics, TEXT("Apex"), EDiscPlastic::Base, Resolved));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogAeroRangeInvariantsTest,
    "DiscGolfTour.Data.Catalog.AeroRangeInvariants",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogAeroRangeInvariantsTest::RunTest(const FString& Parameters)
{
    struct FRangeCase
    {
        const TCHAR* Name;
        float FDiscAeroProfile::* Member;
        float InvalidValue;
    };

    const FRangeCase InvalidRanges[] = {
        { TEXT("MassKg zero"), &FDiscAeroProfile::MassKg, 0.0f },
        { TEXT("MassKg ceiling"), &FDiscAeroProfile::MassKg, 1.01f },
        { TEXT("DiameterM zero"), &FDiscAeroProfile::DiameterM, 0.0f },
        { TEXT("DiameterM ceiling"), &FDiscAeroProfile::DiameterM, 1.01f },
        { TEXT("AreaM2 zero"), &FDiscAeroProfile::AreaM2, 0.0f },
        { TEXT("AreaM2 ceiling"), &FDiscAeroProfile::AreaM2, 1.01f },
        { TEXT("InertiaAxialKgM2 zero"), &FDiscAeroProfile::InertiaAxialKgM2, 0.0f },
        { TEXT("InertiaAxialKgM2 ceiling"), &FDiscAeroProfile::InertiaAxialKgM2, 1.01f },
        { TEXT("InertiaPlanarKgM2 zero"), &FDiscAeroProfile::InertiaPlanarKgM2, 0.0f },
        { TEXT("InertiaPlanarKgM2 ceiling"), &FDiscAeroProfile::InertiaPlanarKgM2, 1.01f },
        { TEXT("CL0 ceiling"), &FDiscAeroProfile::CL0, 10.01f },
        { TEXT("CLa zero"), &FDiscAeroProfile::CLa, 0.0f },
        { TEXT("CLa ceiling"), &FDiscAeroProfile::CLa, 10.01f },
        { TEXT("CD0 zero"), &FDiscAeroProfile::CD0, 0.0f },
        { TEXT("CD0 ceiling"), &FDiscAeroProfile::CD0, 10.01f },
        { TEXT("CDa zero"), &FDiscAeroProfile::CDa, 0.0f },
        { TEXT("CDa ceiling"), &FDiscAeroProfile::CDa, 10.01f },
        { TEXT("CM0 ceiling"), &FDiscAeroProfile::CM0, 10.01f },
        { TEXT("CMa ceiling"), &FDiscAeroProfile::CMa, -10.01f },
        { TEXT("HighSpeedTurnMomentNm negative"), &FDiscAeroProfile::HighSpeedTurnMomentNm, -0.001f },
        { TEXT("HighSpeedTurnMomentNm ceiling"), &FDiscAeroProfile::HighSpeedTurnMomentNm, 1.01f },
        { TEXT("LowSpeedFadeMomentNm negative"), &FDiscAeroProfile::LowSpeedFadeMomentNm, -0.001f },
        { TEXT("LowSpeedFadeMomentNm ceiling"), &FDiscAeroProfile::LowSpeedFadeMomentNm, 1.01f },
        { TEXT("TurnStartsAboveMps zero"), &FDiscAeroProfile::TurnStartsAboveMps, 0.0f },
        { TEXT("TurnStartsAboveMps ceiling"), &FDiscAeroProfile::TurnStartsAboveMps, 100.01f },
        { TEXT("FadeStartsBelowMps zero"), &FDiscAeroProfile::FadeStartsBelowMps, 0.0f },
        { TEXT("FadeStartsBelowMps ceiling"), &FDiscAeroProfile::FadeStartsBelowMps, 100.01f },
        { TEXT("SpinDecayPerSecond negative"), &FDiscAeroProfile::SpinDecayPerSecond, -0.001f },
        { TEXT("SpinDecayPerSecond ceiling"), &FDiscAeroProfile::SpinDecayPerSecond, 100.01f },
        { TEXT("GroundRestitution zero"), &FDiscAeroProfile::GroundRestitution, 0.0f },
        { TEXT("GroundRestitution ceiling"), &FDiscAeroProfile::GroundRestitution, 1.01f },
        { TEXT("GroundFriction zero"), &FDiscAeroProfile::GroundFriction, 0.0f },
        { TEXT("GroundFriction ceiling"), &FDiscAeroProfile::GroundFriction, 1.01f }
    };

    for (const FRangeCase& Range : InvalidRanges)
    {
        TArray<FDiscMoldDefinition> Molds;
        TArray<FDiscPlasticDefinition> Plastics;
        UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);
        (Molds[0].Aero).*(Range.Member) = Range.InvalidValue;

        FString Error;
        TestFalse(*FString::Printf(TEXT("%s is rejected"), Range.Name),
            UDiscCatalogSubsystem::ValidateDefinitions(Molds, Plastics, Error));
        FResolvedDiscDefinition Resolved;
        TestFalse(*FString::Printf(TEXT("%s cannot resolve"), Range.Name),
            UDiscCatalogSubsystem::ResolveFromDefinitions(
                Molds, Plastics, TEXT("Apex"), EDiscPlastic::Tour, Resolved));
    }

    TArray<FDiscMoldDefinition> NeutralMolds;
    TArray<FDiscPlasticDefinition> NeutralPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(NeutralMolds, NeutralPlastics);
    NeutralMolds[0].Aero.HighSpeedTurnMomentNm = 0.0f;
    NeutralMolds[0].Aero.LowSpeedFadeMomentNm = 0.0f;
    NeutralMolds[0].Aero.SpinDecayPerSecond = 0.0f;
    FString Error;
    TestTrue(TEXT("Zero stability moments and zero decay remain valid neutral calibration"),
        UDiscCatalogSubsystem::ValidateDefinitions(NeutralMolds, NeutralPlastics, Error));

    TArray<FDiscMoldDefinition> MassMolds;
    TArray<FDiscPlasticDefinition> MassPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(MassMolds, MassPlastics);
    MassMolds[0].Aero.MassKg = 0.180f;
    TestTrue(TEXT("A non-default in-contract catalog mass validates"),
        UDiscCatalogSubsystem::ValidateDefinitions(MassMolds, MassPlastics, Error));
    FResolvedDiscDefinition MassResolved;
    TestTrue(TEXT("A non-default in-contract catalog mass resolves"),
        UDiscCatalogSubsystem::ResolveFromDefinitions(
            MassMolds, MassPlastics, TEXT("Apex"), EDiscPlastic::Tour, MassResolved));
    TestTrue(TEXT("Resolved mass metadata exactly follows catalog aerodynamic mass"),
        FMath::IsNearlyEqual(MassResolved.DiscMassGrams, 180.0f, 0.01f)
        && DiscGolfMath::IsResolvedDiscDefinitionValid(MassResolved));
    MassMolds[0].Aero.MassKg = 0.129f;
    TestFalse(TEXT("Catalog mass below the resolved-disc envelope is rejected"),
        UDiscCatalogSubsystem::ValidateDefinitions(MassMolds, MassPlastics, Error));
    MassMolds[0].Aero.MassKg = 0.201f;
    TestFalse(TEXT("Catalog mass above the resolved-disc envelope is rejected"),
        UDiscCatalogSubsystem::ValidateDefinitions(MassMolds, MassPlastics, Error));

    TArray<FDiscMoldDefinition> CombinedMolds;
    TArray<FDiscPlasticDefinition> CombinedPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(CombinedMolds, CombinedPlastics);
    CombinedMolds[0].Aero.HighSpeedTurnMomentNm = 0.2f;
    CombinedPlastics[0].HighSpeedTurnMomentScale = 10.0f;
    TestFalse(TEXT("Individually bounded rows cannot produce an out-of-range post-plastic Aero result"),
        UDiscCatalogSubsystem::ValidateDefinitions(CombinedMolds, CombinedPlastics, Error));
    TestTrue(TEXT("Post-plastic rejection retains the shared Aero field diagnostic"),
        Error.Contains(TEXT("Aero.HighSpeedTurnMomentNm")));

    TArray<FDiscMoldDefinition> UnknownEnumMolds;
    TArray<FDiscPlasticDefinition> UnknownEnumPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(UnknownEnumMolds, UnknownEnumPlastics);
    UnknownEnumPlastics[0].Plastic = static_cast<EDiscPlastic>(255);
    TestFalse(TEXT("Unknown Primary Asset plastic enum mappings are rejected"),
        UDiscCatalogSubsystem::ValidateDefinitions(UnknownEnumMolds, UnknownEnumPlastics, Error));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogNonFinitePrimaryUsesFallbackTest,
    "DiscGolfTour.Data.Catalog.NonFinitePrimaryUsesFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogNonFinitePrimaryUsesFallbackTest::RunTest(const FString& Parameters)
{
    TArray<FDiscMoldDefinition> FallbackMolds;
    TArray<FDiscPlasticDefinition> FallbackPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(FallbackMolds, FallbackPlastics);
    TArray<FDiscMoldDefinition> CandidateMolds = FallbackMolds;
    TArray<FDiscPlasticDefinition> CandidatePlastics = FallbackPlastics;
    CandidateMolds[0].Aero.CM0 = std::numeric_limits<float>::quiet_NaN();

    TArray<FDiscMoldDefinition> SelectedMolds;
    TArray<FDiscPlasticDefinition> SelectedPlastics;
    bool bUsedFallback = false;
    FString Reason;
    TestTrue(TEXT("A non-finite Primary Asset catalog selects the deterministic fallback"),
        UDiscCatalogSubsystem::SelectCatalogDefinitions(
            CandidateMolds, CandidatePlastics, FallbackMolds, FallbackPlastics,
            SelectedMolds, SelectedPlastics, bUsedFallback, Reason));
    TestTrue(TEXT("The invalid Primary Asset catalog is not active"), bUsedFallback);
    TestTrue(TEXT("The fallback reason identifies CM0"), Reason.Contains(TEXT("CM0")));

    FResolvedDiscDefinition Resolved;
    TestTrue(TEXT("The selected fallback remains resolvable"),
        UDiscCatalogSubsystem::ResolveFromDefinitions(
            SelectedMolds, SelectedPlastics, TEXT("Apex"), EDiscPlastic::Tour, Resolved));
    TestTrue(TEXT("No non-finite Primary Asset CM0 leaks into the resolved disc"),
        FMath::IsFinite(Resolved.Aero.CM0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscCatalogInvalidAssetsUseFallbackTest,
    "DiscGolfTour.Data.Catalog.InvalidAssetsUseFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscCatalogInvalidAssetsUseFallbackTest::RunTest(const FString& Parameters)
{
    TArray<FDiscMoldDefinition> Fallback;
    TArray<FDiscPlasticDefinition> FallbackPlastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Fallback, FallbackPlastics);
    TArray<FDiscMoldDefinition> Incomplete = Fallback;
    Incomplete.RemoveAt(1);
    TArray<FDiscMoldDefinition> Selected;
    TArray<FDiscPlasticDefinition> SelectedPlastics;
    bool bUsedFallback = false;
    FString Reason;
    TestTrue(TEXT("Selection succeeds through the valid fallback"),
        UDiscCatalogSubsystem::SelectCatalogDefinitions(
            Incomplete, FallbackPlastics, Fallback, FallbackPlastics,
            Selected, SelectedPlastics, bUsedFallback, Reason));
    TestTrue(TEXT("Incomplete primary assets select fallback atomically"), bUsedFallback);
    TestEqual(TEXT("No partial candidate molds leak into the active catalog"), Selected.Num(), Fallback.Num());
    TestTrue(TEXT("Fallback reason identifies the missing mold"), Reason.Contains(TEXT("Vector")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscBagEquipmentSelectionTest,
    "DiscGolfTour.Data.Catalog.BagEquipmentSelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscBagEquipmentSelectionTest::RunTest(const FString& Parameters)
{
    UDiscBagComponent* Bag = NewObject<UDiscBagComponent>();
    TestTrue(TEXT("A stable mold/plastic selection is accepted"),
        Bag->SelectEquipment(TEXT("Line"), EDiscPlastic::Crystal));
    TestEqual(TEXT("Selected mold is updated"), Bag->GetSelectedMoldId(), FName(TEXT("Line")));
    TestEqual(TEXT("Selected plastic is updated"), Bag->GetSelectedPlastic(), EDiscPlastic::Crystal);

    TestFalse(TEXT("An unknown mold is rejected"),
        Bag->SelectEquipment(TEXT("Unknown"), EDiscPlastic::Tour));
    TestFalse(TEXT("An invalid plastic enum is rejected"),
        Bag->SelectEquipment(TEXT("Apex"), static_cast<EDiscPlastic>(255)));
    TestEqual(TEXT("Rejected selections do not mutate the mold"),
        Bag->GetSelectedMoldId(), FName(TEXT("Line")));
    TestEqual(TEXT("Rejected selections do not mutate the plastic"),
        Bag->GetSelectedPlastic(), EDiscPlastic::Crystal);
    return true;
}

#endif
