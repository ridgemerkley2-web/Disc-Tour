#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.h"
#include "DiscGolfEnvironmentTypes.h"

/**
 * Fixed decorative-environment policy for one gameplay-authored zone type.
 * The adapter has no quality input: hard exclusions therefore cannot be weakened by a visual preset.
 */
struct DISCGOLFTOUREDITOR_API FDiscGolfCourseAuthoringPcgZonePolicy
{
    EDiscGolfEnvironmentZoneType EnvironmentZoneType = EDiscGolfEnvironmentZoneType::Fairway;
    int32 Priority = 0;
    bool bHardExclusion = false;
    /** Tree-scattering exclusion only; grass/ground-cover decoration remains eligible. */
    bool bExcludeTreesFromFlightLine = false;
};

/**
 * Value-only handoff from authoritative gameplay authoring to decorative PCG tooling.
 * Deliberately absent are lie, penalty, collision, wind, route, and spawn-instance fields.
 */
struct DISCGOLFTOUREDITOR_API FDiscGolfCourseAuthoringPcgZonePlanEntry
{
    FName StableZoneId = NAME_None;
    EDGCourseZoneType SourceZoneType = EDGCourseZoneType::Rough;
    EDiscGolfEnvironmentZoneType EnvironmentZoneType = EDiscGolfEnvironmentZoneType::Fairway;
    TArray<FVector> PolygonPointsCm;
    int32 Priority = 0;
    bool bApplyToDecorativePcg = true;
    bool bHardExclusion = false;
    /** Fairway flight-line protection for tree categories, not an all-decoration exclusion. */
    bool bExcludeTreesFromFlightLine = false;
};

namespace DiscGolfCourseAuthoringPcgAdapter
{
    /** Exact, closed mapping for every supported EDGCourseZoneType value. */
    DISCGOLFTOUREDITOR_API bool TryGetZonePolicy(
        EDGCourseZoneType SourceZoneType,
        FDiscGolfCourseAuthoringPcgZonePolicy& OutPolicy,
        FString& OutError);

    /**
     * Builds a deterministic plan sorted by explicit stable ID. The source definitions are read-only.
     * Any invalid/ambiguous input fails closed and leaves OutPlan empty; no partial plan is returned.
     */
    DISCGOLFTOUREDITOR_API bool BuildPlan(
        const TArray<FDGCourseZoneDefinition>& AuthoritativeGameplayZones,
        TArray<FDiscGolfCourseAuthoringPcgZonePlanEntry>& OutPlan,
        FString& OutError);
}
