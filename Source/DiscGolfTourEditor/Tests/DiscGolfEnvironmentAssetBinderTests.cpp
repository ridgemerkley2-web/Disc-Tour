#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "DiscGolfEnvironmentAssetBinder.h"
#include "DiscGolfEnvironmentDataAssets.h"
#include "Engine/StaticMesh.h"
#include "Misc/FileHelper.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderMissingAssetTest,
    "DiscGolfTour.Environment.AssetBinder.MissingAssetHandling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderMissingAssetTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FString Notes;
    const TArray<EDiscGolfEnvironmentBindingStatus> Statuses =
        UDiscGolfEnvironmentAssetBinder::ValidateMeshForCategory(
            EDiscGolfEnvironmentAssetCategory::Fern, nullptr, Notes);
    TestTrue(TEXT("Missing mesh is explicit"),
        Statuses.Contains(EDiscGolfEnvironmentBindingStatus::Missing));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderFoliageCollisionTest,
    "DiscGolfTour.Environment.AssetBinder.GrassFernNoBlockingCollision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderFoliageCollisionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    FString GrassNotes;
    FString FernNotes;
    TestTrue(TEXT("Engine cube is available as a deliberately invalid foliage mesh"), Cube != nullptr);
    TestTrue(TEXT("Grass with simple collision is rejected"),
        UDiscGolfEnvironmentAssetBinder::ValidateMeshForCategory(
            EDiscGolfEnvironmentAssetCategory::Grass, Cube, GrassNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));
    TestTrue(TEXT("Fern with simple collision is rejected"),
        UDiscGolfEnvironmentAssetBinder::ValidateMeshForCategory(
            EDiscGolfEnvironmentAssetCategory::Fern, Cube, FernNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));

    FDiscGolfEnvironmentAssetSlot GrassSlot;
    GrassSlot.Category = EDiscGolfEnvironmentAssetCategory::Grass;
    GrassSlot.CollisionMode = EDiscGolfEnvironmentCollisionMode::None;
    FDiscGolfEnvironmentMeshVariant GrassVariant;
    GrassVariant.VisualMesh = const_cast<UStaticMesh*>(Cube);
    FString ReadinessNotes;
    TestTrue(TEXT("Assigned grass still rejects blocking collision during readiness validation"),
        UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
            GrassSlot, GrassVariant, ReadinessNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderTreeProxyTest,
    "DiscGolfTour.Environment.AssetBinder.TreeCollisionProxyClassification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderTreeProxyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    FString Notes;
    const TArray<EDiscGolfEnvironmentBindingStatus> Statuses =
        UDiscGolfEnvironmentAssetBinder::ValidateMeshForCategory(
            EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, Cylinder, Notes);
    TestFalse(TEXT("A simple solid primitive satisfies the tree proxy requirement"),
        Statuses.Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));

    const UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    FDiscGolfEnvironmentAssetSlot TreeSlot;
    TreeSlot.Category = EDiscGolfEnvironmentAssetCategory::TreeConiferYoung;
    TreeSlot.CollisionMode = EDiscGolfEnvironmentCollisionMode::TrunkOrBranchBlocking;
    FDiscGolfEnvironmentMeshVariant TreeVariant;
    TreeVariant.VisualMesh = const_cast<UStaticMesh*>(Cube);
    TreeVariant.CollisionProxyMesh = const_cast<UStaticMesh*>(Cylinder);
    FString VariantNotes;
    TestFalse(TEXT("A dedicated simple trunk proxy satisfies blocking slot policy"),
        UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
            TreeSlot, TreeVariant, VariantNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));

    TreeVariant.CollisionProxyMesh.Reset();
    TestTrue(TEXT("Blocking slot policy rejects a missing dedicated proxy"),
        UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
            TreeSlot, TreeVariant, VariantNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));

    FDiscGolfEnvironmentAssetSlot ShrubSlot;
    ShrubSlot.Category = EDiscGolfEnvironmentAssetCategory::Shrub;
    ShrubSlot.CollisionMode = EDiscGolfEnvironmentCollisionMode::ShrubOverlap;
    FDiscGolfEnvironmentMeshVariant ShrubVariant;
    ShrubVariant.VisualMesh = NewObject<UStaticMesh>();
    ShrubVariant.InteractionProxyMesh = const_cast<UStaticMesh*>(Cylinder);
    TestFalse(TEXT("A dedicated interaction proxy satisfies overlap slot policy"),
        UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
            ShrubSlot, ShrubVariant, VariantNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));
    ShrubVariant.InteractionProxyMesh.Reset();
    TestTrue(TEXT("Overlap slot policy rejects a missing interaction proxy"),
        UDiscGolfEnvironmentAssetBinder::ValidateVariantForSlot(
            ShrubSlot, ShrubVariant, VariantNotes)
        .Contains(EDiscGolfEnvironmentBindingStatus::NeedsCollision));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderSafetyTest,
    "DiscGolfTour.Environment.AssetBinder.ScanDoesNotMutate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderSafetyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfEnvironmentAssetSet* AssetSet = NewObject<UDiscGolfEnvironmentAssetSet>();
    const int32 InitialSlotCount = AssetSet->Slots.Num();
    TArray<int32> InitialVariants;
    for (const FDiscGolfEnvironmentAssetSlot& Slot : AssetSet->Slots)
    {
        InitialVariants.Add(Slot.Variants.Num());
    }
    const FDiscGolfEnvironmentBindingScan Scan =
        UDiscGolfEnvironmentAssetBinder::ProposeBindingsToReport(
            { TEXT("/Game/DefinitelyMissingVendorRoot") }, AssetSet,
            TEXT("Automation/EnvironmentAssetBindingReport_ScanDoesNotMutate.json"));
    TestEqual(TEXT("One proposal is emitted for every slot"), Scan.Proposals.Num(), 16);
    TestEqual(TEXT("Scanning preserves slot count"), AssetSet->Slots.Num(), InitialSlotCount);
    for (int32 Index = 0; Index < AssetSet->Slots.Num(); ++Index)
    {
        TestEqual(TEXT("Scanning preserves existing bindings"),
            AssetSet->Slots[Index].Variants.Num(), InitialVariants[Index]);
    }
    TestTrue(TEXT("Candidate report path is produced"), !Scan.ReportPath.IsEmpty());
    TestTrue(TEXT("Automation scan uses its isolated report path"),
        Scan.ReportPath.Contains(
            TEXT("Saved/Automation/EnvironmentAssetBindingReport_ScanDoesNotMutate.json")));

    FString ProposalBeforeValidation;
    TestTrue(TEXT("Candidate proposal report can be read"),
        FFileHelper::LoadFileToString(ProposalBeforeValidation, *Scan.ReportPath));
    const FDiscGolfEnvironmentValidationResult Validation =
        UDiscGolfEnvironmentAssetBinder::ValidateEnvironmentAssetReadinessToReport(
            AssetSet,
            TEXT("Automation/EnvironmentAssetBindingReport_SeparateReadiness.json"));
    FString ProposalAfterValidation;
    TestTrue(TEXT("Candidate proposal report remains readable after readiness validation"),
        FFileHelper::LoadFileToString(ProposalAfterValidation, *Scan.ReportPath));
    TestEqual(TEXT("Readiness validation preserves the separate proposal artifact"),
        ProposalAfterValidation, ProposalBeforeValidation);
    TestNotEqual(TEXT("Proposal and readiness paths are separate"),
        Scan.ReportPath, Validation.ReportPath);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderRootClassificationTest,
    "DiscGolfTour.Environment.AssetBinder.RootNamesDoNotClassifyCandidates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderRootClassificationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TArray<FString> SpruceRoots = { TEXT("/Game/PN_interactiveSpruceForest") };
    const TArray<FString> StumpRoots = { TEXT("/Game/Stump_Scanned") };

    TestEqual(TEXT("A vendor-root spruce token does not classify an unrelated mesh"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/PN_interactiveSpruceForest/Meshes/SM_UtilityPlane.SM_UtilityPlane"),
            SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        0.0f);
    TestEqual(TEXT("A vendor-root stump token does not classify an unrelated mesh"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/SM_UtilityPlane.SM_UtilityPlane"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::Stump),
        0.0f);
    TestEqual(TEXT("Mannequin subtrees are excluded even when an asset name contains spruce"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/PN_interactiveSpruceForest/UE4_Mannequin/Mannequin/SM_SpruceHat.SM_SpruceHat"),
            SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        0.0f);
    TestEqual(TEXT("Example/demo subtrees are excluded even when an asset name contains spruce"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/PN_interactiveSpruceForest/ExampleContent/Winter/Meshes/spruce_full_01.spruce_full_01"),
            SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        0.0f);
    const TArray<FString> NestedExampleRoots = {
        TEXT("/Game/PN_interactiveSpruceForest/ExampleContent")
    };
    TestEqual(TEXT("Selecting an excluded subtree as the vendor root cannot bypass exclusion"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/PN_interactiveSpruceForest/ExampleContent/Meshes/spruce_full_01.spruce_full_01"),
            NestedExampleRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        0.0f);
    TestEqual(TEXT("A logo mesh cannot enter the log slot through substring matching"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/SM_Logo_Plane.SM_Logo_Plane"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::Log),
        0.0f);
    TestTrue(TEXT("The intentional groundcover compound token remains classifiable"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/groundcover_01.groundcover_01"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::GroundCover) > 0.0f);
    TestTrue(TEXT("The intentional deadwood compound token remains classifiable"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/deadwood_01.deadwood_01"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::Log) > 0.0f);
    TestEqual(TEXT("A small rock cannot enter a young-conifer slot on size alone"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/rock_small.rock_small"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferYoung),
        0.0f);
    TestEqual(TEXT("A large utility mesh cannot enter a large-conifer slot on size alone"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/SM_Large_Plane.SM_Large_Plane"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        0.0f);
    TestEqual(TEXT("A small spruce cannot enter a small-rock slot on size alone"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/PN_interactiveSpruceForest/Meshes/small/spruce_small_01.spruce_small_01"),
            SpruceRoots, EDiscGolfEnvironmentAssetCategory::RockSmall),
        0.0f);
    TestTrue(TEXT("A real mesh retains meaningful relative-path classification"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/PN_interactiveSpruceForest/Meshes/full/high/spruce_full_01.spruce_full_01"),
            SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge) > 0.0f);
    TestTrue(TEXT("A real stump retains meaningful asset-name classification"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            TEXT("/Game/Stump_Scanned/Meshes/Stump_1_mesh.Stump_1_mesh"),
            StumpRoots, EDiscGolfEnvironmentAssetCategory::Stump) > 0.0f);

    const FString FullSpruce =
        TEXT("/Game/PN_interactiveSpruceForest/Meshes/full/high/spruce_full_01.spruce_full_01");
    const FString HalfSpruce =
        TEXT("/Game/PN_interactiveSpruceForest/Meshes/half/high/spruce_half_01.spruce_half_01");
    const FString SmallSpruce =
        TEXT("/Game/PN_interactiveSpruceForest/Meshes/small/spruce_small_01.spruce_small_01");
    TestEqual(TEXT("Generic full form tokens are neutral across mature conifer slots"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            FullSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            FullSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferMedium));
    TestEqual(TEXT("Generic half form tokens are neutral across mature conifer slots"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            HalfSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferMedium),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            HalfSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge));
    const FString HighSpruce =
        TEXT("/Game/PN_interactiveSpruceForest/Meshes/high/spruce_01.spruce_01");
    const FString LowSpruce =
        TEXT("/Game/PN_interactiveSpruceForest/Meshes/low/spruce_01.spruce_01");
    TestEqual(TEXT("High and low quality tokens do not change classification"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            HighSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            LowSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge));
    TestTrue(TEXT("Generic small/sapling tokens rank a conifer toward the young slot"),
        UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            SmallSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferYoung)
        > UDiscGolfEnvironmentAssetBinder::ScoreAssetPathForCategory(
            SmallSpruce, SpruceRoots, EDiscGolfEnvironmentAssetCategory::TreeConiferMedium));

    const auto BoundsFitness = [](EDiscGolfEnvironmentAssetCategory Category, float HeightCm)
    {
        return UDiscGolfEnvironmentAssetBinder::ScoreTreeBoundsForCategory(
            Category, FVector(400.0f, 400.0f, HeightCm));
    };
    TestEqual(TEXT("A 0.56 metre regeneration mesh does not rank as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 56.45f), 0.0f);
    TestEqual(TEXT("A 1.21 metre regeneration mesh does not rank as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 120.58f), 0.0f);
    TestEqual(TEXT("A 1.78 metre regeneration mesh does not rank as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 177.70f), 0.0f);
    TestEqual(TEXT("A 2.56 metre regeneration mesh does not rank as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 255.54f), 0.0f);
    TestTrue(TEXT("A 4.53 metre conifer ranks as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 452.93f) > 0.0f);
    TestTrue(TEXT("A 6.35 metre conifer ranks as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 634.64f) > 0.0f);
    TestTrue(TEXT("A 7.74 metre conifer ranks as a young tree"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 773.96f) > 0.0f);
    TestTrue(TEXT("An 8.5 metre conifer remains eligible in the young-medium overlap"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferYoung, 850.0f) > 0.0f
        && BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferMedium, 850.0f) > 0.0f);
    TestTrue(TEXT("An 11 metre conifer ranks medium rather than large"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferMedium, 1100.0f)
        > BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferLarge, 1100.0f));
    TestTrue(TEXT("A 14 metre conifer remains eligible in the medium-large overlap"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferMedium, 1400.0f) > 0.0f
        && BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferLarge, 1400.0f) > 0.0f);
    TestTrue(TEXT("A 16 metre conifer ranks large rather than medium"),
        BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferLarge, 1600.0f)
        > BoundsFitness(EDiscGolfEnvironmentAssetCategory::TreeConiferMedium, 1600.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderReadinessResultTest,
    "DiscGolfTour.Environment.AssetBinder.ReadinessIsDistinctFromReportGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderReadinessResultTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfEnvironmentAssetSet* AssetSet = NewObject<UDiscGolfEnvironmentAssetSet>();
    const FDiscGolfEnvironmentValidationResult Result =
        UDiscGolfEnvironmentAssetBinder::ValidateEnvironmentAssetReadinessToReport(
            AssetSet,
            TEXT("Automation/EnvironmentAssetBindingReport_Readiness.json"));
    TestTrue(TEXT("The validation report was generated"), Result.bReportGenerated);
    TestTrue(TEXT("The default data asset contains all sixteen categories"),
        Result.bStructurallyComplete);
    TestFalse(TEXT("Empty slots are not production ready"), Result.bProductionReady);
    TestTrue(TEXT("Readiness automation uses its isolated report path"),
        Result.ReportPath.Contains(
            TEXT("Saved/Automation/EnvironmentAssetBindingReport_Readiness.json")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderApprovalGateTest,
    "DiscGolfTour.Environment.AssetBinder.InvalidApprovalIsAtomic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderApprovalGateTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfEnvironmentAssetSet* AssetSet = NewObject<UDiscGolfEnvironmentAssetSet>();
    const int32 InitialCount = AssetSet->Slots[0].Variants.Num();
    FString Error;
    TestFalse(TEXT("A non-mesh approval is rejected"),
        UDiscGolfEnvironmentAssetBinder::ApplyApprovedBindings(
            AssetSet, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge,
            { FSoftObjectPath(TEXT("/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial")) }, Error));
    TestEqual(TEXT("Rejected approval leaves the slot unchanged"),
        AssetSet->Slots[0].Variants.Num(), InitialCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnvironmentBinderApprovalMetadataTest,
    "DiscGolfTour.Environment.AssetBinder.ReapprovalPreservesVariantMetadata",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnvironmentBinderApprovalMetadataTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfEnvironmentAssetSet* AssetSet = NewObject<UDiscGolfEnvironmentAssetSet>();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    TestNotNull(TEXT("Visual fixture is available"), Cube);
    TestNotNull(TEXT("Proxy fixture is available"), Cylinder);

    FDiscGolfEnvironmentMeshVariant Existing;
    Existing.VisualMesh = Cube;
    Existing.CollisionProxyMesh = Cylinder;
    Existing.InteractionProxyMesh = Cylinder;
    Existing.Weight = 2.75f;
    Existing.UniformScaleRange = FVector2D(0.82f, 1.18f);
    Existing.bNaniteSuitable = true;
    AssetSet->Slots[0].Variants = { Existing };

    FString Error;
    TestTrue(TEXT("Explicit reapproval succeeds"),
        UDiscGolfEnvironmentAssetBinder::ApplyApprovedBindings(
            AssetSet, EDiscGolfEnvironmentAssetCategory::TreeConiferLarge,
            { FSoftObjectPath(Cube) }, Error));
    TestEqual(TEXT("Reapproval keeps one selected variant"),
        AssetSet->Slots[0].Variants.Num(), 1);
    if (AssetSet->Slots[0].Variants.Num() == 1)
    {
        const FDiscGolfEnvironmentMeshVariant& Actual = AssetSet->Slots[0].Variants[0];
        TestEqual(TEXT("Weight is preserved"), Actual.Weight, Existing.Weight);
        TestEqual(TEXT("Scale range is preserved"),
            Actual.UniformScaleRange, Existing.UniformScaleRange);
        TestEqual(TEXT("Collision proxy is preserved"),
            Actual.CollisionProxyMesh.ToSoftObjectPath(),
            Existing.CollisionProxyMesh.ToSoftObjectPath());
        TestEqual(TEXT("Interaction proxy is preserved"),
            Actual.InteractionProxyMesh.ToSoftObjectPath(),
            Existing.InteractionProxyMesh.ToSoftObjectPath());
        TestEqual(TEXT("Reviewed Nanite flag is preserved"),
            Actual.bNaniteSuitable, Existing.bNaniteSuitable);
    }
    return true;
}

#endif
