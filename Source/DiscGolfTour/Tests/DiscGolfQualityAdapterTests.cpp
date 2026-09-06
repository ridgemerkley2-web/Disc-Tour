#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCoursePresentationDefinition.h"
#include "../DiscGolfEnvironmentDataAssets.h"
#include "../DiscGolfQualityAdapter.h"

namespace
{
struct FExpectedQualityMapping
{
    EDiscGolfRuntimeQualityProfile Profile;
    FName ProfileId;
    FName CourseTierId;
    EDiscGolfEnvironmentQuality EnvironmentQuality;
};

const FExpectedQualityMapping ExpectedPlayerMappings[] = {
    { EDiscGolfRuntimeQualityProfile::Performance, TEXT("Performance"), TEXT("Low"),
        EDiscGolfEnvironmentQuality::Performance },
    { EDiscGolfRuntimeQualityProfile::Medium, TEXT("Medium"), TEXT("Medium"),
        EDiscGolfEnvironmentQuality::Performance },
    { EDiscGolfRuntimeQualityProfile::High, TEXT("High"), TEXT("High"),
        EDiscGolfEnvironmentQuality::High },
    { EDiscGolfRuntimeQualityProfile::Cinematic, TEXT("Cinematic"), TEXT("High"),
        EDiscGolfEnvironmentQuality::Cinematic }
};

bool HasUniformEngineGroupQuality(
    const Scalability::FQualityLevels& Quality,
    int32 Expected)
{
    return Quality.ViewDistanceQuality == Expected
        && Quality.AntiAliasingQuality == Expected
        && Quality.ShadowQuality == Expected
        && Quality.GlobalIlluminationQuality == Expected
        && Quality.ReflectionQuality == Expected
        && Quality.PostProcessQuality == Expected
        && Quality.TextureQuality == Expected
        && Quality.EffectsQuality == Expected
        && Quality.FoliageQuality == Expected
        && Quality.ShadingQuality == Expected
        && Quality.LandscapeQuality == Expected;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfQualityPlayerPresetMappingTest,
    "DiscGolfTour.Quality.Adapter.PlayerPresetMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfQualityPlayerPresetMappingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (int32 Quality = 0; Quality < UE_ARRAY_COUNT(ExpectedPlayerMappings); ++Quality)
    {
        const FDiscGolfResolvedQualityProfile Result =
            DiscGolfQualityAdapter::ResolvePlayerPreset(Quality);
        const FExpectedQualityMapping& Expected = ExpectedPlayerMappings[Quality];
        TestEqual(TEXT("Player preset has the expected stable profile"),
            static_cast<int32>(Result.Profile), static_cast<int32>(Expected.Profile));
        TestEqual(TEXT("Player preset has the expected stable profile ID"),
            Result.ProfileId, Expected.ProfileId);
        TestEqual(TEXT("Player preset has the expected course tier"),
            Result.CourseVisualTierId, Expected.CourseTierId);
        TestEqual(TEXT("Player preset has the expected environment tier"),
            static_cast<int32>(Result.EnvironmentQuality),
            static_cast<int32>(Expected.EnvironmentQuality));
        TestTrue(TEXT("Player preset sets every engine group uniformly"),
            HasUniformEngineGroupQuality(Result.EngineQuality, Quality));
    }

    const FDiscGolfResolvedQualityProfile BelowRange =
        DiscGolfQualityAdapter::ResolvePlayerPreset(-100);
    const FDiscGolfResolvedQualityProfile AboveRange =
        DiscGolfQualityAdapter::ResolvePlayerPreset(100);
    TestEqual(TEXT("Player preset clamps values below Performance"),
        static_cast<int32>(BelowRange.Profile),
        static_cast<int32>(EDiscGolfRuntimeQualityProfile::Performance));
    TestEqual(TEXT("Player preset clamps values above Cinematic"),
        static_cast<int32>(AboveRange.Profile),
        static_cast<int32>(EDiscGolfRuntimeQualityProfile::Cinematic));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfQualityOmenCaptureContractTest,
    "DiscGolfTour.Quality.Adapter.OmenCaptureContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfQualityOmenCaptureContractTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    Scalability::FQualityLevels Baseline;
    Baseline.SetFromSingleQualityLevel(1);
    const int32 BaselineLandscapeQuality = Baseline.LandscapeQuality;
    const FDiscGolfResolvedQualityProfile Result =
        DiscGolfQualityAdapter::MakeOmenCaptureProfile(Baseline);
    const Scalability::FQualityLevels& Quality = Result.EngineQuality;

    TestEqual(TEXT("Omen capture keeps its stable profile ID"), Result.ProfileId,
        FName(TEXT("OmenGameplay1080pHighFoliageV1")));
    TestTrue(TEXT("Omen capture uses 100 percent resolution"),
        FMath::IsNearlyEqual(Quality.ResolutionQuality, 100.0f));
    TestTrue(TEXT("Omen capture preserves the accepted quality-2 groups"),
        Quality.ViewDistanceQuality == 2
        && Quality.AntiAliasingQuality == 2
        && Quality.ShadowQuality == 2
        && Quality.GlobalIlluminationQuality == 2
        && Quality.ReflectionQuality == 2
        && Quality.PostProcessQuality == 2
        && Quality.TextureQuality == 2
        && Quality.EffectsQuality == 2
        && Quality.ShadingQuality == 2);
    TestEqual(TEXT("Omen capture preserves the accepted foliage-3 group"),
        Quality.FoliageQuality, 3);
    TestEqual(TEXT("Uncontracted landscape quality remains at its baseline"),
        Quality.LandscapeQuality, BaselineLandscapeQuality);
    TestEqual(TEXT("Omen capture uses authored High course presentation"),
        Result.CourseVisualTierId, FName(TEXT("High")));
    TestEqual(TEXT("Omen capture does not promote environment to Cinematic"),
        static_cast<int32>(Result.EnvironmentQuality),
        static_cast<int32>(EDiscGolfEnvironmentQuality::High));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfQualityCurrentProfileResolutionTest,
    "DiscGolfTour.Quality.Adapter.CurrentProfileResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfQualityCurrentProfileResolutionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (int32 Quality = 0; Quality < UE_ARRAY_COUNT(ExpectedPlayerMappings); ++Quality)
    {
        const FDiscGolfResolvedQualityProfile PlayerProfile =
            DiscGolfQualityAdapter::ResolvePlayerPreset(Quality);
        const FDiscGolfResolvedQualityProfile Current =
            DiscGolfQualityAdapter::ResolveCurrent(PlayerProfile.EngineQuality);
        TestEqual(TEXT("Current engine state resolves the player-facing profile"),
            static_cast<int32>(Current.Profile),
            static_cast<int32>(ExpectedPlayerMappings[Quality].Profile));
        TestEqual(TEXT("Current engine state retains the supplied quality snapshot"),
            Current.EngineQuality.FoliageQuality,
            PlayerProfile.EngineQuality.FoliageQuality);
    }

    Scalability::FQualityLevels Baseline;
    Baseline.SetFromSingleQualityLevel(0);
    const FDiscGolfResolvedQualityProfile Omen =
        DiscGolfQualityAdapter::MakeOmenCaptureProfile(Baseline);
    const FDiscGolfResolvedQualityProfile ResolvedOmen =
        DiscGolfQualityAdapter::ResolveCurrent(Omen.EngineQuality);
    TestEqual(TEXT("Exact Omen shape remains distinguishable from Cinematic"),
        static_cast<int32>(ResolvedOmen.Profile),
        static_cast<int32>(EDiscGolfRuntimeQualityProfile::OmenGameplay1080pHighFoliageV1));
    TestEqual(TEXT("Resolved Omen environment remains High"),
        static_cast<int32>(ResolvedOmen.EnvironmentQuality),
        static_cast<int32>(EDiscGolfEnvironmentQuality::High));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfQualityPureResolutionTest,
    "DiscGolfTour.Quality.Adapter.PureResolutionNoGlobalMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfQualityPureResolutionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const Scalability::FQualityLevels Before = Scalability::GetQualityLevels();
    Scalability::FQualityLevels Synthetic;
    Synthetic.SetFromSingleQualityLevel(0);
    const FDiscGolfResolvedQualityProfile Player =
        DiscGolfQualityAdapter::ResolvePlayerPreset(3);
    const FDiscGolfResolvedQualityProfile Current =
        DiscGolfQualityAdapter::ResolveCurrent(Synthetic);
    const FDiscGolfResolvedQualityProfile Omen =
        DiscGolfQualityAdapter::MakeOmenCaptureProfile(Synthetic);
    (void)Player;
    (void)Current;
    (void)Omen;
    const Scalability::FQualityLevels After = Scalability::GetQualityLevels();
    TestTrue(TEXT("Pure quality resolution does not mutate global scalability"),
        Before == After);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfQualityCollisionAuthorityInvariantTest,
    "DiscGolfTour.Quality.Adapter.CollisionAuthorityInvariant",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfQualityCollisionAuthorityInvariantTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfCoursePresentationDefinition Presentation;
    FString Source;
    FString Error;
    if (!TestTrue(TEXT("Pine Ridge presentation loads for quality validation"),
        DiscGolfCoursePresentation::LoadPineRidge(Presentation, Source, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("Presentation declares collision invariant across quality"),
        Presentation.bCollisionInvariantAcrossQuality);

    TArray<FDiscGolfResolvedQualityProfile> Profiles;
    for (int32 Quality = 0; Quality < UE_ARRAY_COUNT(ExpectedPlayerMappings); ++Quality)
    {
        Profiles.Add(DiscGolfQualityAdapter::ResolvePlayerPreset(Quality));
    }
    Scalability::FQualityLevels Baseline;
    Baseline.SetFromSingleQualityLevel(2);
    Profiles.Add(DiscGolfQualityAdapter::MakeOmenCaptureProfile(Baseline));

    for (const FDiscGolfResolvedQualityProfile& Profile : Profiles)
    {
        const FDiscGolfCourseVisualQualityTier* Tier =
            Presentation.QualityTiers.FindByPredicate(
                [&Profile](const FDiscGolfCourseVisualQualityTier& Candidate)
                {
                    return Candidate.TierId == Profile.CourseVisualTierId;
                });
        TestNotNull(TEXT("Every resolved profile selects an authored course tier"), Tier);
        if (Tier)
        {
            TestFalse(TEXT("Resolved presentation tier cannot affect collision"),
                Tier->bAffectsCollision);
            TestEqual(TEXT("Resolved presentation tier retains competitive collision profile"),
                Tier->CollisionProfileId, Presentation.CollisionProfileId);
        }
    }

    const UDiscGolfForestPreset* ForestPreset = GetDefault<UDiscGolfForestPreset>();
    TestTrue(TEXT("Environment quality never disables canopy interaction authority"),
        ForestPreset->Performance.bEnableCanopyInteractionVolumes
        && ForestPreset->High.bEnableCanopyInteractionVolumes
        && ForestPreset->Cinematic.bEnableCanopyInteractionVolumes);
    return true;
}

#endif
