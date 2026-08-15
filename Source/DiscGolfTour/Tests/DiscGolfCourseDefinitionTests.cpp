#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseDefinition.h"
#include "../DiscGolfCoursePresentationDefinition.h"
#include "../DiscGolfFoliagePresentationActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeManifestTest,
    "DiscGolfTour.CourseDefinition.CourseManifest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeManifestTest::RunTest(const FString& Parameters)
{
    FDiscGolfCourseManifestDefinition Manifest;
    FString Source;
    FString Error;
    TestTrue(TEXT("Three-hole course manifest loads"),
        DiscGolfCourseDefinition::LoadPineRidgeCourseManifest(Manifest, Source, Error));
    TestEqual(TEXT("Development uses the authored manifest"), Source, FString(TEXT("AUTHORED JSON")));
    TestEqual(TEXT("Manifest has three contiguous holes"), Manifest.Holes.Num(), 3);
    TestEqual(TEXT("Final manifest entry is hole three"), Manifest.Holes.Last().HoleNumber, 3);
    TestTrue(TEXT("Hole origins define one spatially distributed course"),
        FVector::Dist2D(Manifest.Holes[0].WorldOriginCm, Manifest.Holes[1].WorldOriginCm) > 5000.0f
        && FVector::Dist2D(Manifest.Holes[1].WorldOriginCm, Manifest.Holes[2].WorldOriginCm) > 5000.0f);

    FDiscGolfHoleBlockoutDefinition LocalHole2;
    FString HoleSource;
    TestTrue(TEXT("Local Hole 2 loads for persistent placement"),
        DiscGolfCourseDefinition::LoadPineRidgeHole(2, LocalHole2, HoleSource, Error));
    FDiscGolfHoleBlockoutDefinition WorldHole2;
    TestTrue(TEXT("Hole 2 places into the shared course coordinate system"),
        DiscGolfCourseDefinition::PlaceHoleInCourse(
            LocalHole2, Manifest.Holes[1], WorldHole2, Error));
    TestTrue(TEXT("Placed Hole 2 tee uses its manifest world origin"),
        WorldHole2.TeeLocationCm.Equals(FVector(14500.0f, 4000.0f, 100.0f), 0.1f));
    TestTrue(TEXT("Placement preserves local tee-to-basket distance"),
        FMath::IsNearlyEqual(FVector::Dist(LocalHole2.TeeLocationCm, LocalHole2.BasketLocationCm),
            FVector::Dist(WorldHole2.TeeLocationCm, WorldHole2.BasketLocationCm), 0.1f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeFallbackDefinitionTest,
    "DiscGolfTour.CourseDefinition.PineRidgeFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeFallbackDefinitionTest::RunTest(const FString& Parameters)
{
    const FDiscGolfHoleBlockoutDefinition Definition = DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    FString Error;
    TestTrue(TEXT("Source fallback passes the production validator"),
        DiscGolfCourseDefinition::Validate(Definition, Error));
    TestEqual(TEXT("Fallback course identity is stable"), Definition.CourseId,
        FName(TEXT("PineRidgeChampionship")));
    TestEqual(TEXT("Fallback provides eleven typed surfaces"), Definition.Surfaces.Num(), 11);
    TestEqual(TEXT("Fallback provides three discrete non-tree fixtures"), Definition.CollisionFixtures.Num(), 3);
    TestEqual(TEXT("Fallback provides two landing zones"), Definition.LandingZones.Num(), 2);
    TestEqual(TEXT("Fallback provides three broadcast anchors"), Definition.CameraAnchors.Num(), 3);
    TestEqual(TEXT("Fallback provides two local wind zones"), Definition.WindZones.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeAuthoredJsonTest,
    "DiscGolfTour.CourseDefinition.AuthoredJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeAuthoredJsonTest::RunTest(const FString& Parameters)
{
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    FString Error;
    TestTrue(TEXT("Pine Ridge loads from external data or its validated fallback"),
        DiscGolfCourseDefinition::LoadPineRidgeHole1(Definition, Source, Error));
    TestEqual(TEXT("Checked-in authored JSON is preferred in the development project"),
        Source, FString(TEXT("AUTHORED JSON")));
    TestTrue(TEXT("Loaded data retains a six-point flyover"), Definition.FlyoverPointsCm.Num() == 6);
    TestEqual(TEXT("Loaded data retains dense grass, rock, and sign fixtures"),
        Definition.CollisionFixtures.Num(), 3);
    const FDiscGolfBlockoutSurfaceDefinition* Tee = Definition.Surfaces.FindByPredicate(
        [](const FDiscGolfBlockoutSurfaceDefinition& Surface)
        { return Surface.SurfaceId == TEXT("Tee"); });
    TestTrue(TEXT("Hole 1 uses a professional rectangular tee pad"), Tee
        && Tee->Scale.X >= 4.0f && Tee->Scale.Y >= 1.7f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeDistanceTest,
    "DiscGolfTour.CourseDefinition.Distance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeDistanceTest::RunTest(const FString& Parameters)
{
    const FDiscGolfHoleBlockoutDefinition Definition = DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    const float DistanceFeet = DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition);
    TestTrue(TEXT("Authored hole is approximately 362 feet"), FMath::IsNearlyEqual(DistanceFeet, 361.9f, 0.2f));
    TestTrue(TEXT("Pin has authored elevation"), Definition.BasketLocationCm.Z > Definition.TeeLocationCm.Z);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfNeedleGateDefinitionTest,
    "DiscGolfTour.CourseDefinition.NeedleGate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfNeedleGateDefinitionTest::RunTest(const FString& Parameters)
{
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    FString Error;
    TestTrue(TEXT("Needle Gate loads and validates"),
        DiscGolfCourseDefinition::LoadPineRidgeHole(2, Definition, Source, Error));
    TestEqual(TEXT("Needle Gate is hole two"), Definition.HoleNumber, 2);
    TestEqual(TEXT("Needle Gate is par four"), Definition.Par, 4);
    TestTrue(TEXT("Needle Gate is approximately 640 feet"),
        FMath::IsNearlyEqual(DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition), 640.0f, 0.5f));
    TestTrue(TEXT("Needle Gate has a dense tree corridor"), Definition.Trees.Num() >= 18);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfGalleryLakeDefinitionTest,
    "DiscGolfTour.CourseDefinition.GalleryLake",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfGalleryLakeDefinitionTest::RunTest(const FString& Parameters)
{
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    FString Error;
    TestTrue(TEXT("Gallery Lake loads and validates"),
        DiscGolfCourseDefinition::LoadPineRidgeHole(3, Definition, Source, Error));
    TestEqual(TEXT("Gallery Lake is hole three"), Definition.HoleNumber, 3);
    TestEqual(TEXT("Gallery Lake is par four"), Definition.Par, 4);
    TestTrue(TEXT("Gallery Lake is approximately 725 feet"),
        FMath::IsNearlyEqual(DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition), 725.0f, 0.5f));
    TestTrue(TEXT("Gallery Lake finishes downhill"), Definition.BasketLocationCm.Z < Definition.TeeLocationCm.Z);
    TestTrue(TEXT("Gallery Lake includes its water hazard"), Definition.Surfaces.ContainsByPredicate(
        [](const FDiscGolfBlockoutSurfaceDefinition& Surface)
        {
            return Surface.SurfaceId == TEXT("GalleryLakeWater") && Surface.SurfaceType == ECourseSurfaceType::Hazard;
        }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeSourceFallbackCoverageTest,
    "DiscGolfTour.CourseDefinition.SourceFallbackCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeSourceFallbackCoverageTest::RunTest(const FString& Parameters)
{
    const FDiscGolfHoleBlockoutDefinition NeedleGate = DiscGolfCourseDefinition::PineRidgeHole2Fallback();
    const FDiscGolfHoleBlockoutDefinition GalleryLake = DiscGolfCourseDefinition::PineRidgeHole3Fallback();
    FString Error;
    TestTrue(TEXT("Needle Gate source fallback validates"), DiscGolfCourseDefinition::Validate(NeedleGate, Error));
    TestTrue(TEXT("Gallery Lake source fallback validates"), DiscGolfCourseDefinition::Validate(GalleryLake, Error));
    TestEqual(TEXT("Needle Gate fallback retains twelve surfaces"), NeedleGate.Surfaces.Num(), 12);
    TestEqual(TEXT("Needle Gate fallback retains eighteen trees"), NeedleGate.Trees.Num(), 18);
    TestEqual(TEXT("Needle Gate fallback retains three discrete fixtures"), NeedleGate.CollisionFixtures.Num(), 3);
    TestEqual(TEXT("Gallery Lake fallback retains twelve surfaces"), GalleryLake.Surfaces.Num(), 12);
    TestEqual(TEXT("Gallery Lake fallback retains fourteen trees"), GalleryLake.Trees.Num(), 14);
    TestEqual(TEXT("Gallery Lake fallback retains three discrete fixtures"), GalleryLake.CollisionFixtures.Num(), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeStrategyRoutesTest,
    "DiscGolfTour.CourseDefinition.StrategyRoutes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeStrategyRoutesTest::RunTest(const FString& Parameters)
{
    for (int32 HoleNumber = 1; HoleNumber <= 3; ++HoleNumber)
    {
        FDiscGolfHoleBlockoutDefinition Definition;
        FString Source;
        FString Error;
        TestTrue(FString::Printf(TEXT("Hole %d strategy data loads"), HoleNumber),
            DiscGolfCourseDefinition::LoadPineRidgeHole(HoleNumber, Definition, Source, Error));
        TestEqual(FString::Printf(TEXT("Hole %d has three distinct scoring plans"), HoleNumber),
            Definition.ShotRoutes.Num(), 3);
        TestTrue(FString::Printf(TEXT("Hole %d has a primary route"), HoleNumber),
            Definition.ShotRoutes.ContainsByPredicate([](const FDiscGolfShotRouteDefinition& Route)
            { return Route.RouteType == EDiscGolfShotRouteType::Primary; }));
        TestTrue(FString::Printf(TEXT("Hole %d has a risk/reward route"), HoleNumber),
            Definition.ShotRoutes.ContainsByPredicate([](const FDiscGolfShotRouteDefinition& Route)
            { return Route.RouteType == EDiscGolfShotRouteType::RiskReward; }));
        TestTrue(FString::Printf(TEXT("Hole %d has a bailout route"), HoleNumber),
            Definition.ShotRoutes.ContainsByPredicate([](const FDiscGolfShotRouteDefinition& Route)
            { return Route.RouteType == EDiscGolfShotRouteType::Bailout; }));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseDuplicateIdentityValidationTest,
    "DiscGolfTour.CourseDefinition.RejectsDuplicateIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseDuplicateIdentityValidationTest::RunTest(const FString& Parameters)
{
    FDiscGolfHoleBlockoutDefinition Definition = DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    Definition.Surfaces[1].SurfaceId = Definition.Surfaces[0].SurfaceId;
    FString Error;
    TestFalse(TEXT("Duplicate feature IDs are rejected"), DiscGolfCourseDefinition::Validate(Definition, Error));
    TestTrue(TEXT("Validation reports the duplicate identity"), Error.Contains(TEXT("duplicate")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCourseCameraCoverageValidationTest,
    "DiscGolfTour.CourseDefinition.RequiresCameraCoverage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCourseCameraCoverageValidationTest::RunTest(const FString& Parameters)
{
    FDiscGolfHoleBlockoutDefinition Definition = DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    Definition.CameraAnchors.Last().Mode = EDiscGolfCameraAnchorMode::Launch;
    FString Error;
    TestFalse(TEXT("Missing finish coverage is rejected"), DiscGolfCourseDefinition::Validate(Definition, Error));
    TestTrue(TEXT("Validation explains camera coverage"), Error.Contains(TEXT("camera coverage")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCoursePresentationContractTest,
    "DiscGolfTour.CourseDefinition.PresentationCollisionInvariant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCoursePresentationContractTest::RunTest(const FString& Parameters)
{
    FDiscGolfCoursePresentationDefinition Presentation;
    FString Source;
    FString Error;
    TestTrue(TEXT("Pine Ridge presentation contract loads and validates"),
        DiscGolfCoursePresentation::LoadPineRidge(Presentation, Source, Error));
    TestEqual(TEXT("Development uses authored presentation JSON"), Source, FString(TEXT("AUTHORED JSON")));
    TestEqual(TEXT("All three quality tiers are present"), Presentation.QualityTiers.Num(), 3);
    TestEqual(TEXT("All three holes have visual plans"), Presentation.Holes.Num(), 3);
    TestEqual(TEXT("Active collision revision names the fixture pass"), Presentation.CollisionProfileId,
        FName(TEXT("PineRidgeCompetitiveV2_Fixtures")));
    TestFalse(TEXT("Contract honestly reports that production assets are pending"), Presentation.bAssetsReady);
    TestEqual(TEXT("Opening uses the Brewster-style tree-line reference"),
        Presentation.Holes[0].ForestReferenceId, FName(TEXT("BrewsterRidgeTreeLine")));
    TestTrue(TEXT("Opening tee and green clear approximately 35 feet"),
        FMath::IsNearlyEqual(Presentation.Holes[0].TeeClearingRadiusCm, 1067.0f, 1.0f)
        && FMath::IsNearlyEqual(Presentation.Holes[0].GreenClearingRadiusCm, 1067.0f, 1.0f));
    TestEqual(TEXT("Needle Gate uses the Northwood-style compression reference"),
        Presentation.Holes[1].ForestReferenceId, FName(TEXT("NorthwoodBlackCompression")));
    TestEqual(TEXT("Gallery Lake uses the Idlewild-style lake-frame reference"),
        Presentation.Holes[2].ForestReferenceId, FName(TEXT("IdlewildLakeFrame")));
    TestEqual(TEXT("Normal-tier Opening has a dense decorative target"),
        DiscGolfFoliagePresentation::ResolveDecorativeInstanceTarget(Presentation.Holes[0], 0.85f), 352);
    TestEqual(TEXT("Normal-tier Needle Gate is the densest forest"),
        DiscGolfFoliagePresentation::ResolveDecorativeInstanceTarget(Presentation.Holes[1], 0.85f), 428);
    TestEqual(TEXT("Normal-tier Gallery Lake retains dense wooded framing"),
        DiscGolfFoliagePresentation::ResolveDecorativeInstanceTarget(Presentation.Holes[2], 0.85f), 383);

    TArray<FDiscGolfHoleBlockoutDefinition> Holes;
    Holes.Add(DiscGolfCourseDefinition::PineRidgeHole1Fallback());
    Holes.Add(DiscGolfCourseDefinition::PineRidgeHole2Fallback());
    Holes.Add(DiscGolfCourseDefinition::PineRidgeHole3Fallback());
    uint32 LowSignature = 0;
    uint32 HighSignature = 0;
    TestTrue(TEXT("Low quality resolves competitive collision"),
        DiscGolfCoursePresentation::CompetitiveCollisionSignatureForQuality(
            Holes, Presentation, TEXT("Low"), LowSignature, Error));
    TestTrue(TEXT("High quality resolves competitive collision"),
        DiscGolfCoursePresentation::CompetitiveCollisionSignatureForQuality(
            Holes, Presentation, TEXT("High"), HighSignature, Error));
    TestEqual(TEXT("Visual quality cannot change surface/tree/fixture collision"), LowSignature, HighSignature);

    Presentation.QualityTiers.Last().bAffectsCollision = true;
    TestFalse(TEXT("A quality tier that affects collision is rejected"),
        DiscGolfCoursePresentation::Validate(Presentation, Error));
    TestTrue(TEXT("Collision rejection is explicit"), Error.Contains(TEXT("collision")));
    Presentation = DiscGolfCoursePresentation::PineRidgeFallback();
    Presentation.Holes[1].ForestReferenceId = NAME_None;
    TestFalse(TEXT("A hole without a forest-design reference is rejected"),
        DiscGolfCoursePresentation::Validate(Presentation, Error));
    return true;
}

#endif
