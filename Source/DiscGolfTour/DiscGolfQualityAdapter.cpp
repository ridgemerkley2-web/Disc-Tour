#include "DiscGolfQualityAdapter.h"

namespace
{
constexpr int32 MinPlayerQuality = 0;
constexpr int32 MaxPlayerQuality = 3;

bool HasOmenCaptureShape(const Scalability::FQualityLevels& Quality)
{
    return FMath::IsNearlyEqual(Quality.ResolutionQuality, 100.0f)
        && Quality.ViewDistanceQuality == 2
        && Quality.AntiAliasingQuality == 2
        && Quality.ShadowQuality == 2
        && Quality.GlobalIlluminationQuality == 2
        && Quality.ReflectionQuality == 2
        && Quality.PostProcessQuality == 2
        && Quality.TextureQuality == 2
        && Quality.EffectsQuality == 2
        && Quality.FoliageQuality == 3
        && Quality.ShadingQuality == 2;
}

FDiscGolfResolvedQualityProfile MakePlayerProfile(
    int32 GraphicsQuality,
    const Scalability::FQualityLevels& EngineQuality)
{
    FDiscGolfResolvedQualityProfile Result;
    Result.EngineQuality = EngineQuality;

    switch (FMath::Clamp(GraphicsQuality, MinPlayerQuality, MaxPlayerQuality))
    {
        case 0:
            Result.Profile = EDiscGolfRuntimeQualityProfile::Performance;
            Result.ProfileId = TEXT("Performance");
            Result.CourseVisualTierId = TEXT("Low");
            Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::Performance;
            break;
        case 1:
            Result.Profile = EDiscGolfRuntimeQualityProfile::Medium;
            Result.ProfileId = TEXT("Medium");
            Result.CourseVisualTierId = TEXT("Medium");
            // The environment preset intentionally has no serialized Medium enum.
            Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::Performance;
            break;
        case 3:
            Result.Profile = EDiscGolfRuntimeQualityProfile::Cinematic;
            Result.ProfileId = TEXT("Cinematic");
            // The course presentation contract tops out at its collision-invariant High tier.
            Result.CourseVisualTierId = TEXT("High");
            Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::Cinematic;
            break;
        case 2:
        default:
            Result.Profile = EDiscGolfRuntimeQualityProfile::High;
            Result.ProfileId = TEXT("High");
            Result.CourseVisualTierId = TEXT("High");
            Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::High;
            break;
    }

    return Result;
}
}

FDiscGolfResolvedQualityProfile DiscGolfQualityAdapter::ResolvePlayerPreset(
    int32 GraphicsQuality)
{
    const int32 NormalizedQuality = FMath::Clamp(
        GraphicsQuality, MinPlayerQuality, MaxPlayerQuality);
    Scalability::FQualityLevels EngineQuality;
    EngineQuality.SetFromSingleQualityLevel(NormalizedQuality);
    return MakePlayerProfile(NormalizedQuality, EngineQuality);
}

FDiscGolfResolvedQualityProfile DiscGolfQualityAdapter::ResolveCurrent(
    const Scalability::FQualityLevels& Quality)
{
    if (HasOmenCaptureShape(Quality))
    {
        FDiscGolfResolvedQualityProfile Result = MakePlayerProfile(2, Quality);
        Result.Profile = EDiscGolfRuntimeQualityProfile::OmenGameplay1080pHighFoliageV1;
        Result.ProfileId = TEXT("OmenGameplay1080pHighFoliageV1");
        // Foliage 3 preserves the authored High population; it must not silently
        // promote the separate environment controller to Cinematic.
        Result.CourseVisualTierId = TEXT("High");
        Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::High;
        return Result;
    }

    // Existing course presentation selected from sg.FoliageQuality. Preserve
    // that player-facing behavior while centralizing it into one build snapshot.
    return MakePlayerProfile(Quality.FoliageQuality, Quality);
}

FDiscGolfResolvedQualityProfile DiscGolfQualityAdapter::MakeOmenCaptureProfile(
    const Scalability::FQualityLevels& Baseline)
{
    Scalability::FQualityLevels Quality = Baseline;
    Quality.ResolutionQuality = 100.0f;
    Quality.ViewDistanceQuality = 2;
    Quality.AntiAliasingQuality = 2;
    Quality.ShadowQuality = 2;
    Quality.GlobalIlluminationQuality = 2;
    Quality.ReflectionQuality = 2;
    Quality.PostProcessQuality = 2;
    Quality.TextureQuality = 2;
    Quality.EffectsQuality = 2;
    Quality.FoliageQuality = 3;
    Quality.ShadingQuality = 2;

    FDiscGolfResolvedQualityProfile Result = MakePlayerProfile(2, Quality);
    Result.Profile = EDiscGolfRuntimeQualityProfile::OmenGameplay1080pHighFoliageV1;
    Result.ProfileId = TEXT("OmenGameplay1080pHighFoliageV1");
    Result.CourseVisualTierId = TEXT("High");
    Result.EnvironmentQuality = EDiscGolfEnvironmentQuality::High;
    return Result;
}
