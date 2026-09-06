#include "Misc/AutomationTest.h"

#include "../DiscGolfProductionMotion.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfProductionMotionFamilyPathsTest,
    "DiscGolfTour.Character.ProductionMotion.FamilyPaths",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfProductionMotionFamilyPathsTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TestEqual(TEXT("Drive family path"),
        FString(DiscGolfProductionMotion::MontageForFamily(EGolferAnimationFamily::Drive)),
        FString(DiscGolfProductionMotion::DriveMontage));
    TestEqual(TEXT("Approach family path"),
        FString(DiscGolfProductionMotion::MontageForFamily(EGolferAnimationFamily::Approach)),
        FString(DiscGolfProductionMotion::ApproachMontage));
    TestEqual(TEXT("Putt family path"),
        FString(DiscGolfProductionMotion::MontageForFamily(EGolferAnimationFamily::Putt)),
        FString(DiscGolfProductionMotion::PuttMontage));

    TestTrue(TEXT("Drive context accepts the drive family"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Drive, EGolferAnimationFamily::Drive));
    TestTrue(TEXT("Drive context accepts the distance-dependent approach family"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Drive, EGolferAnimationFamily::Approach));
    TestFalse(TEXT("Drive context rejects stale putt presentation state"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Drive, EGolferAnimationFamily::Putt));
    TestTrue(TEXT("Circle 1 accepts only the putt family"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Circle1Putt, EGolferAnimationFamily::Putt));
    TestTrue(TEXT("Circle 2 accepts only the putt family"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Circle2Putt, EGolferAnimationFamily::Putt));
    TestFalse(TEXT("Circle 1 rejects stale drive presentation state"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Circle1Putt, EGolferAnimationFamily::Drive));
    TestFalse(TEXT("Circle 2 rejects stale approach presentation state"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Circle2Putt, EGolferAnimationFamily::Approach));
    TestFalse(TEXT("Invalid authoritative context rejects every family"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            static_cast<EDiscShotContext>(255), EGolferAnimationFamily::Putt));
    TestFalse(TEXT("Invalid presentation family is rejected"),
        DiscGolfProductionMotion::IsFamilyCompatibleWithShotContext(
            EDiscShotContext::Drive, static_cast<EGolferAnimationFamily>(255)));

    TestEqual(TEXT("Runtime selects recipe v6"),
        FString(DiscGolfProductionMotion::ActiveVersion), FString(TEXT("v6")));
    TestEqual(TEXT("Runtime selects asset revision v006"),
        FString(DiscGolfProductionMotion::ActiveAssetRevision), FString(TEXT("v006")));
    TestEqual(TEXT("v005 mixed-space contract is explicit"),
        FString(DiscGolfProductionMotion::V5::RotationSpace),
        FString(TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE")));
    TestEqual(TEXT("v005 root remains translation-only"),
        FString(DiscGolfProductionMotion::V5::RootTrackPolicy),
        FString(TEXT("TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE")));
    TestEqual(TEXT("v005 axial component partition count"),
        DiscGolfProductionMotion::V5::ComponentRotationBoneCount, 7);
    TestEqual(TEXT("v005 appendicular local partition count"),
        DiscGolfProductionMotion::V5::LocalRotationBoneCount, 31);
    TestEqual(TEXT("v006 mixed-space contract is explicit"),
        FString(DiscGolfProductionMotion::V6::RotationSpace),
        FString(TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE")));
    TestEqual(TEXT("v006 root remains translation-only"),
        FString(DiscGolfProductionMotion::V6::RootTrackPolicy),
        FString(TEXT("TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE")));
    TestEqual(TEXT("v006 axial component partition count"),
        DiscGolfProductionMotion::V6::ComponentRotationBoneCount, 7);
    TestEqual(TEXT("v006 appendicular local partition count"),
        DiscGolfProductionMotion::V6::LocalRotationBoneCount, 31);
    TestEqual(TEXT("v007 staged candidate revision is explicit"),
        FString(DiscGolfProductionMotion::V7::AssetRevision),
        FString(TEXT("v007")));
    TestEqual(TEXT("v007 mixed-space contract is explicit"),
        FString(DiscGolfProductionMotion::V7::RotationSpace),
        FString(TEXT("MIXED_AXIAL_COMPONENT_APPENDICULAR_LOCAL_ADDITIVE_TO_REFERENCE")));
    TestEqual(TEXT("v007 root remains translation-only"),
        FString(DiscGolfProductionMotion::V7::RootTrackPolicy),
        FString(TEXT("TRANSLATION_ONLY_REFERENCE_ROTATION_SCALE")));
    TestNotEqual(TEXT("Staged v007 does not silently become runtime-active"),
        FString(DiscGolfProductionMotion::ActiveVersion),
        FString(DiscGolfProductionMotion::V7::Version));
    TestEqual(TEXT("Runtime drive montage remains the accepted v006 asset"),
        FString(DiscGolfProductionMotion::DriveMontage),
        FString(DiscGolfProductionMotion::V6::DriveMontage));
    TestEqual(TEXT("Runtime production library remains the accepted v006 asset"),
        FString(DiscGolfProductionMotion::Library),
        FString(DiscGolfProductionMotion::V6::Library));
    TestNotEqual(TEXT("Staged v007 drive montage is not the runtime alias"),
        FString(DiscGolfProductionMotion::DriveMontage),
        FString(DiscGolfProductionMotion::V7::DriveMontage));
    TestNotEqual(TEXT("Staged v007 library is not the runtime alias"),
        FString(DiscGolfProductionMotion::Library),
        FString(DiscGolfProductionMotion::V7::Library));

    const TArray<FString> V1Paths = {
        DiscGolfProductionMotion::V1::DriveSequence,
        DiscGolfProductionMotion::V1::DriveMontage,
        DiscGolfProductionMotion::V1::ApproachSequence,
        DiscGolfProductionMotion::V1::ApproachMontage,
        DiscGolfProductionMotion::V1::PuttSequence,
        DiscGolfProductionMotion::V1::PuttMontage,
        DiscGolfProductionMotion::V1::Library,
    };
    const TArray<FString> V2Paths = {
        DiscGolfProductionMotion::V2::DriveSequence,
        DiscGolfProductionMotion::V2::DriveMontage,
        DiscGolfProductionMotion::V2::ApproachSequence,
        DiscGolfProductionMotion::V2::ApproachMontage,
        DiscGolfProductionMotion::V2::PuttSequence,
        DiscGolfProductionMotion::V2::PuttMontage,
        DiscGolfProductionMotion::V2::Library,
    };
    const TArray<FString> V3Paths = {
        DiscGolfProductionMotion::V3::DriveSequence,
        DiscGolfProductionMotion::V3::DriveMontage,
        DiscGolfProductionMotion::V3::ApproachSequence,
        DiscGolfProductionMotion::V3::ApproachMontage,
        DiscGolfProductionMotion::V3::PuttSequence,
        DiscGolfProductionMotion::V3::PuttMontage,
        DiscGolfProductionMotion::V3::Library,
    };
    const TArray<FString> V4Paths = {
        DiscGolfProductionMotion::V4::DriveSequence,
        DiscGolfProductionMotion::V4::DriveMontage,
        DiscGolfProductionMotion::V4::ApproachSequence,
        DiscGolfProductionMotion::V4::ApproachMontage,
        DiscGolfProductionMotion::V4::PuttSequence,
        DiscGolfProductionMotion::V4::PuttMontage,
        DiscGolfProductionMotion::V4::Library,
    };
    const TArray<FString> V5Paths = {
        DiscGolfProductionMotion::V5::DriveSequence,
        DiscGolfProductionMotion::V5::DriveMontage,
        DiscGolfProductionMotion::V5::ApproachSequence,
        DiscGolfProductionMotion::V5::ApproachMontage,
        DiscGolfProductionMotion::V5::PuttSequence,
        DiscGolfProductionMotion::V5::PuttMontage,
        DiscGolfProductionMotion::V5::Library,
    };
    const TArray<FString> V6Paths = {
        DiscGolfProductionMotion::V6::DriveSequence,
        DiscGolfProductionMotion::V6::DriveMontage,
        DiscGolfProductionMotion::V6::ApproachSequence,
        DiscGolfProductionMotion::V6::ApproachMontage,
        DiscGolfProductionMotion::V6::PuttSequence,
        DiscGolfProductionMotion::V6::PuttMontage,
        DiscGolfProductionMotion::V6::Library,
    };
    const TArray<FString> V7Paths = {
        DiscGolfProductionMotion::V7::DriveSequence,
        DiscGolfProductionMotion::V7::DriveMontage,
        DiscGolfProductionMotion::V7::ApproachSequence,
        DiscGolfProductionMotion::V7::ApproachMontage,
        DiscGolfProductionMotion::V7::PuttSequence,
        DiscGolfProductionMotion::V7::PuttMontage,
        DiscGolfProductionMotion::V7::Library,
    };
    TSet<FString> UniquePaths;
    for (const FString& Path : V1Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v001 candidate retains its immutable revision"),
            Path.Contains(TEXT("_v001"))
                || Path.EndsWith(TEXT("DA_DG_ProductionMotionLibrary.DA_DG_ProductionMotionLibrary")));
    }
    for (const FString& Path : V2Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v002 candidate uses its versioned revision"),
            Path.Contains(TEXT("_v002")));
    }
    for (const FString& Path : V3Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v003 candidate uses its versioned revision"),
            Path.Contains(TEXT("_v003")));
    }
    for (const FString& Path : V4Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v004 candidate uses its versioned revision"),
            Path.Contains(TEXT("_v004")));
    }
    for (const FString& Path : V5Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v005 candidate uses its versioned revision"),
            Path.Contains(TEXT("_v005")));
    }
    for (const FString& Path : V6Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v006 candidate uses its versioned revision"),
            Path.Contains(TEXT("_v006")));
    }
    for (const FString& Path : V7Paths)
    {
        UniquePaths.Add(Path);
        TestTrue(TEXT("v007 candidate uses its isolated revision"),
            Path.Contains(TEXT("_v007")));
    }
    TestEqual(TEXT("All immutable seven-asset generations are distinct"),
        UniquePaths.Num(), 49);
    for (const FString& Path : V7Paths)
    {
        TestTrue(TEXT("Candidate remains in the production-motion root"),
            Path.StartsWith(DiscGolfProductionMotion::TargetRoot));
        TestFalse(TEXT("Candidate is not in the synthetic mocap root"),
            Path.Contains(TEXT("/Animation/Mocap/")));
        TestFalse(TEXT("Candidate is not in the prototype throw root"),
            Path.Contains(TEXT("/Animation/Throws/")));
    }
    return true;
}

#endif
