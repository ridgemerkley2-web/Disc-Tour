#pragma once

#include "CoreMinimal.h"
#include "DiscGolfEnvironmentTypes.h"
#include "Scalability.h"

/** Stable runtime quality identities used to resolve presentation-only policy. */
enum class EDiscGolfRuntimeQualityProfile : uint8
{
    Performance,
    Medium,
    High,
    Cinematic,
    OmenGameplay1080pHighFoliageV1
};

/**
 * Immutable quality selection for one course build.
 *
 * This structure owns presentation density/cull policy only. It deliberately
 * contains no collision, vegetation-interaction, MetaHuman, or save policy.
 */
struct DISCGOLFTOUR_API FDiscGolfResolvedQualityProfile
{
    EDiscGolfRuntimeQualityProfile Profile = EDiscGolfRuntimeQualityProfile::High;
    FName ProfileId = TEXT("High");
    Scalability::FQualityLevels EngineQuality;
    FName CourseVisualTierId = TEXT("High");
    EDiscGolfEnvironmentQuality EnvironmentQuality = EDiscGolfEnvironmentQuality::High;
};

namespace DiscGolfQualityAdapter
{
    /** Resolves the normalized 0..3 player-facing preset without changing global state. */
    DISCGOLFTOUR_API FDiscGolfResolvedQualityProfile ResolvePlayerPreset(
        int32 GraphicsQuality);

    /** Resolves active engine quality into one immutable course-build profile. */
    DISCGOLFTOUR_API FDiscGolfResolvedQualityProfile ResolveCurrent(
        const Scalability::FQualityLevels& Quality);

    /** Builds the accepted Omen capture contract without changing global state. */
    DISCGOLFTOUR_API FDiscGolfResolvedQualityProfile MakeOmenCaptureProfile(
        const Scalability::FQualityLevels& Baseline);
}
