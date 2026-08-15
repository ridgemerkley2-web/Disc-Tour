#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscCatalogSubsystem.h"
#include "../DiscBagComponent.h"

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
