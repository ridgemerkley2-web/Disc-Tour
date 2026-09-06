#pragma once

#include "CoreMinimal.h"

namespace DiscGolfTerrainPresentationMath
{
    /**
     * Removes the linearly varying endpoint clearance from a terrain path and
     * pins both endpoints to their authored ground anchors. Invalid or
     * degenerate input returns false without changing OutNormalizedPoints.
     */
    DISCGOLFTOUR_API bool TryNormalizePathToGroundAnchors(
        const TArray<FVector>& SourcePoints,
        const FVector& GroundStartCm,
        const FVector& GroundEndCm,
        TArray<FVector>& OutNormalizedPoints);
}
