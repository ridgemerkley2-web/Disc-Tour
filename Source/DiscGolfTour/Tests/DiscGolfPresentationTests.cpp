#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfPresentationMath.h"
#include "../DiscGolfPerformanceBudget.h"
#include "../DiscGolferPresentationComponent.h"
#include "../DiscGolfFoliagePresentationActor.h"
#include "../DiscGolfTerrainPresentationActor.h"
#include "../DiscGolfWaterPresentationActor.h"
#include "../DiscGolfLevelDesignReviewActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"

namespace
{
TArray<FDiscTrajectorySample> MakePresentationSamples()
{
    TArray<FDiscTrajectorySample> Samples;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        FDiscTrajectorySample Sample;
        Sample.TimeSeconds = 2.0f + Index * 0.25f;
        Sample.WorldLocationCm = FVector(Index * 100.0f, Index * 20.0f, 120.0f + Index * 10.0f);
        Sample.VelocityMps = FVector(10.0f - Index, 2.0f, 1.0f);
        Sample.DiscNormalWorld = FVector(0.0f, 0.0f, 1.0f);
        Sample.SpinRpm = 600.0f - Index * 50.0f;
        Sample.AngleOfAttackDeg = static_cast<float>(Index);
        if (Index >= 3)
        {
            Sample.GroundState = Index == 3 ? EDiscGroundState::Sliding : EDiscGroundState::Settled;
            Sample.GroundContactCount = 1;
        }
        Samples.Add(Sample);
    }
    return Samples;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReplayInterpolationTest,
    "DiscGolfTour.Presentation.ReplayInterpolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReplayInterpolationTest::RunTest(const FString& Parameters)
{
    const TArray<FDiscTrajectorySample> Samples = MakePresentationSamples();
    FDiscReplayFrame Frame;
    TestTrue(TEXT("Recorded samples can produce a replay frame"),
        DiscGolfPresentationMath::EvaluateReplayFrame(Samples, 0.375f, Frame));
    TestEqual(TEXT("Binary search finds the lower segment"), Frame.LowerSampleIndex, 1);
    TestTrue(TEXT("Segment interpolation is centered"), FMath::IsNearlyEqual(Frame.SegmentAlpha, 0.5f));
    TestTrue(TEXT("Position interpolates from authoritative samples"),
        Frame.WorldLocationCm.Equals(FVector(150.0f, 30.0f, 135.0f), 0.001f));
    TestTrue(TEXT("Velocity interpolates without resimulation"),
        Frame.VelocityMps.Equals(FVector(8.5f, 2.0f, 1.0f), 0.001f));
    TestTrue(TEXT("Spin interpolates without resimulation"), FMath::IsNearlyEqual(Frame.SpinRpm, 525.0f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReplayClampTest,
    "DiscGolfTour.Presentation.ReplayClampsToCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReplayClampTest::RunTest(const FString& Parameters)
{
    const TArray<FDiscTrajectorySample> Samples = MakePresentationSamples();
    FDiscReplayFrame Before;
    FDiscReplayFrame After;
    TestTrue(TEXT("Negative playback time clamps to first sample"),
        DiscGolfPresentationMath::EvaluateReplayFrame(Samples, -10.0f, Before));
    TestTrue(TEXT("Excess playback time clamps to last sample"),
        DiscGolfPresentationMath::EvaluateReplayFrame(Samples, 100.0f, After));
    TestTrue(TEXT("Start position is exact"), Before.WorldLocationCm.Equals(Samples[0].WorldLocationCm));
    TestTrue(TEXT("Final position is exact"), After.WorldLocationCm.Equals(Samples.Last().WorldLocationCm));
    TestEqual(TEXT("Final state is retained"), After.GroundState, EDiscGroundState::Settled);
    TestEqual(TEXT("Final contact count is retained"), After.GroundContactCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfReplayOrientationTest,
    "DiscGolfTour.Presentation.ReplayOrientation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReplayOrientationTest::RunTest(const FString& Parameters)
{
    const FVector Velocity(8.0f, 3.0f, -1.0f);
    const FVector Normal(0.12f, -0.04f, 0.992f);
    const FQuat Rotation = DiscGolfPresentationMath::ReplayRotation(Velocity, Normal);
    TestTrue(TEXT("Replay rotation is normalized"), Rotation.IsNormalized());
    TestTrue(TEXT("Replay up axis preserves the recorded disc normal"),
        FVector::DotProduct(Rotation.GetUpVector(), Normal.GetSafeNormal()) > 0.999f);
    const FVector PlaneVelocity = (Velocity - Rotation.GetUpVector()
        * FVector::DotProduct(Velocity, Rotation.GetUpVector())).GetSafeNormal();
    TestTrue(TEXT("Replay forward follows recorded velocity in the disc plane"),
        FVector::DotProduct(Rotation.GetForwardVector(), PlaneVelocity) > 0.999f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfTracerDecimationTest,
    "DiscGolfTour.Presentation.TracerDecimation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfTracerDecimationTest::RunTest(const FString& Parameters)
{
    TArray<FDiscTrajectorySample> Samples;
    Samples.Reserve(1000);
    for (int32 Index = 0; Index < 1000; ++Index)
    {
        FDiscTrajectorySample Sample;
        Sample.TimeSeconds = Index / 240.0f;
        Sample.WorldLocationCm = FVector(Index * 2.0f, FMath::Sin(Index * 0.01f) * 30.0f, 100.0f);
        if (Index >= 700)
        {
            Sample.GroundState = EDiscGroundState::Sliding;
            Sample.GroundContactCount = 1;
        }
        Samples.Add(Sample);
    }

    TArray<int32> Indices;
    DiscGolfPresentationMath::BuildTracerSampleIndices(Samples, Indices, 75.0f, 0.075f, 80);
    TestTrue(TEXT("Tracer is bounded for long solver captures"), Indices.Num() <= 80);
    TestEqual(TEXT("Tracer preserves the release endpoint"), Indices[0], 0);
    TestEqual(TEXT("Tracer preserves the final endpoint"), Indices.Last(), Samples.Num() - 1);
    TestTrue(TEXT("Tracer retains the air-to-ground state boundary"), Indices.Contains(700));
    for (int32 Index = 1; Index < Indices.Num(); ++Index)
    {
        TestTrue(TEXT("Tracer indices remain strictly ordered"), Indices[Index] > Indices[Index - 1]);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPerformanceBudgetTest,
    "DiscGolfTour.Presentation.PerformanceBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPerformanceBudgetTest::RunTest(const FString& Parameters)
{
    FDiscGolfPerformanceBudget Budget;
    Budget.MinimumSampleCount = 5;
    Budget.WindowSampleCount = 10;
    FDiscGolfPerformanceTracker Tracker(Budget);

    for (int32 Index = 0; Index < 10; ++Index) Tracker.AddFrame(1.0f / 60.0f);
    const FDiscGolfPerformanceSummary Healthy = Tracker.Summarize(1024ull * 1024ull * 1024ull);
    TestEqual(TEXT("The frame window reaches its fixed capacity"), Healthy.SampleCount, 10);
    TestTrue(TEXT("A stable 60 FPS sample reports approximately 60 FPS"),
        FMath::IsNearlyEqual(Healthy.AverageFps, 60.0f, 0.1f));
    TestEqual(TEXT("Healthy frame and memory telemetry passes"),
        DiscGolfPerformance::Evaluate(Healthy, Budget), EDiscGolfPerformanceBudgetState::Pass);

    Tracker.Reset();
    for (int32 Index = 0; Index < 25; ++Index) Tracker.AddFrame(0.040f);
    const FDiscGolfPerformanceSummary Slow = Tracker.Summarize(1024ull * 1024ull * 1024ull);
    TestEqual(TEXT("Long captures remain bounded"), Slow.SampleCount, 10);
    TestTrue(TEXT("Slow frames produce a failing P95"), Slow.P95FrameTimeMs >= 40.0f);
    TestEqual(TEXT("A failing P95 fails the budget"),
        DiscGolfPerformance::Evaluate(Slow, Budget), EDiscGolfPerformanceBudgetState::Fail);

    Tracker.Reset();
    for (int32 Index = 0; Index < 5; ++Index) Tracker.AddFrame(1.0f / 60.0f);
    const FDiscGolfPerformanceSummary HighMemory = Tracker.Summarize(5ull * 1024ull * 1024ull * 1024ull);
    TestEqual(TEXT("Excess process memory fails an otherwise healthy sample"),
        DiscGolfPerformance::Evaluate(HighMemory, Budget), EDiscGolfPerformanceBudgetState::Fail);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolferAnimationContractTest,
    "DiscGolfTour.Presentation.GolferAnimationContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolferAnimationContractTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Long drive context selects drive animation"),
        DiscGolferPresentation::ResolveFamily(EDiscShotContext::Drive, 90.0f),
        EGolferAnimationFamily::Drive);
    TestEqual(TEXT("Short clean context selects approach animation"),
        DiscGolferPresentation::ResolveFamily(EDiscShotContext::Drive, 40.0f),
        EGolferAnimationFamily::Approach);
    TestEqual(TEXT("Circle context selects putting animation"),
        DiscGolferPresentation::ResolveFamily(EDiscShotContext::Circle1Putt, 8.0f),
        EGolferAnimationFamily::Putt);

    UDiscGolferPresentationComponent* Presentation = NewObject<UDiscGolferPresentationComponent>();
    TestFalse(TEXT("The contract test intentionally uses a detached component"),
        Presentation->IsRegistered());
    Presentation->SetShotContext(EDiscShotContext::Drive, 40.0f);
    Presentation->BeginTiming(EThrowStyle::Forehand);
    TestEqual(TEXT("First throw press enters windup"), Presentation->GetAnimationPhase(),
        EGolferAnimationPhase::Windup);

    FThrowRelease Release;
    Release.ShotContext = EDiscShotContext::Drive;
    Release.ThrowStyle = EThrowStyle::Forehand;
    Release.Grade = EReleaseGrade::Good;
    Release.Timing = EReleaseTiming::Late;
    Presentation->CommitAuthoritativeRelease(Release);
    TestEqual(TEXT("Authoritative launch enters visual release"), Presentation->GetAnimationPhase(),
        EGolferAnimationPhase::Release);
    TestEqual(TEXT("Release feedback is available to an animation blueprint"),
        Presentation->GetPresentationReleaseGrade(), EReleaseGrade::Good);
    Presentation->AdvancePresentation(0.25f);
    TestEqual(TEXT("Release advances to follow-through"), Presentation->GetAnimationPhase(),
        EGolferAnimationPhase::FollowThrough);
    Presentation->AdvancePresentation(1.0f);
    TestEqual(TEXT("Follow-through returns to setup"), Presentation->GetAnimationPhase(),
        EGolferAnimationPhase::Setup);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfPineRidgeEnvironmentAssetsTest,
    "DiscGolfTour.Presentation.PineRidgeEnvironmentAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfPineRidgeEnvironmentAssetsTest::RunTest(const FString& Parameters)
{
    TestNotNull(TEXT("Pine Ridge terrain master is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeTerrain.M_PineRidgeTerrain")));
    TestNotNull(TEXT("Pine Ridge fairway material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeFairway.MI_PineRidgeFairway")));
    TestNotNull(TEXT("Pine Ridge two-sided PBR grass material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeGrassBlade.M_PineRidgeGrassBlade")));
    TestNotNull(TEXT("Pine Ridge feathered fairway PBR material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeFairwayBlend.M_PineRidgeFairwayBlend")));
    TestNotNull(TEXT("Pine Ridge feathered trail and shoreline PBR material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeTrailBlend.M_PineRidgeTrailBlend")));
    TestNotNull(TEXT("Pine Ridge forest-floor litter material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_PineRidgeLeafLitter.M_PineRidgeLeafLitter")));
    TestNotNull(TEXT("Pine Ridge trail-wear material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeTrailWear.MI_PineRidgeTrailWear")));
    TestNotNull(TEXT("Pine Ridge shore-reed material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeShoreReeds.MI_PineRidgeShoreReeds")));
    TestNotNull(TEXT("Pine Ridge grass-card alpha mask is cooked and loadable"),
        LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_grass_card_alpha.pine_ridge_grass_card_alpha")));
    TestNotNull(TEXT("Pine Ridge litter-card alpha mask is cooked and loadable"),
        LoadObject<UTexture2D>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Textures/Generated/pine_ridge_litter_card_alpha.pine_ridge_litter_card_alpha")));
    TestNotNull(TEXT("Gallery Lake animated water material is cooked and loadable"),
        LoadObject<UMaterialInterface>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Materials/M_GalleryLakeWater.M_GalleryLakeWater")));
    TestNotNull(TEXT("CC0 fir presentation mesh is cooked and loadable"),
        LoadObject<UStaticMesh>(nullptr,
            TEXT("/Game/Presentation/Course/PineRidge/Foliage/PolyHaven/FirSapling/fir_sapling_a.fir_sapling_a")));
    const ADiscGolfFoliagePresentationActor* Defaults = GetDefault<ADiscGolfFoliagePresentationActor>();
    TestTrue(TEXT("Foliage components can never change competitive collision"),
        Defaults && Defaults->IsCollisionInvariant());
    const ADiscGolfTerrainPresentationActor* TerrainDefaults = GetDefault<ADiscGolfTerrainPresentationActor>();
    TestTrue(TEXT("Terrain relief can never change competitive collision"),
        TerrainDefaults && TerrainDefaults->IsCollisionInvariant());
    const ADiscGolfWaterPresentationActor* WaterDefaults = GetDefault<ADiscGolfWaterPresentationActor>();
    TestTrue(TEXT("Water presentation can never change competitive collision"),
        WaterDefaults && WaterDefaults->IsCollisionInvariant());
    const ADiscGolfLevelDesignReviewActor* ReviewDefaults = GetDefault<ADiscGolfLevelDesignReviewActor>();
    TestTrue(TEXT("Level-design review can never change competitive collision"),
        ReviewDefaults && ReviewDefaults->IsCollisionInvariant());
    return true;
}

#endif
