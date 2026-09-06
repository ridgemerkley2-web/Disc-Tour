#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"

class AActor;
class UPhysicalMaterial;

namespace DiscGolfCourseRules
{
    inline const FName TagSurfaceFairway(TEXT("Surface.Fairway"));
    inline const FName TagSurfaceTeePad(TEXT("Surface.TeePad"));
    inline const FName TagSurfaceLightRough(TEXT("Surface.LightRough"));
    inline const FName TagSurfaceRough(TEXT("Surface.Rough"));
    inline const FName TagSurfaceDeepRough(TEXT("Surface.DeepRough"));
    inline const FName TagSurfaceDirt(TEXT("Surface.Dirt"));
    inline const FName TagSurfaceRock(TEXT("Surface.Rock"));
    inline const FName TagZoneOutOfBounds(TEXT("Zone.OutOfBounds"));
    inline const FName TagZoneHazard(TEXT("Zone.Hazard"));

    DISCGOLFTOUR_API bool HasSurfaceIdentity(
        const AActor* SurfaceActor,
        const UPhysicalMaterial* PhysicalMaterial);

    DISCGOLFTOUR_API ECourseSurfaceType ResolveSurface(
        const AActor* SurfaceActor,
        const UPhysicalMaterial* PhysicalMaterial);

    DISCGOLFTOUR_API EGroundSurfaceType GroundResponseSurface(ECourseSurfaceType Surface);
    DISCGOLFTOUR_API FLieEffectProfile LieEffects(ECourseSurfaceType Surface);
    DISCGOLFTOUR_API ELieType LieTypeForSurfaceAndDistance(
        ECourseSurfaceType Surface,
        float DistanceToBasketMeters,
        bool bIsTee = false);

    DISCGOLFTOUR_API FThrowCommand ApplyLieEffects(
        const FThrowCommand& Command,
        const FLieEffectProfile& Effects);

    DISCGOLFTOUR_API FDiscGolfLieState ResolveLie(
        ECourseSurfaceType SurfaceAtRest,
        const FVector& RawDiscLocationCm,
        const FVector& LastInBoundsLocationCm,
        const FVector& BasketLocationCm,
        bool bIsTee = false);

    DISCGOLFTOUR_API FVector ReliefPointInsideBoundary(
        const FVector& InsideBoundaryCm,
        const FVector& OutsideBoundaryCm,
        float ReliefDistanceCm = 100.0f);

    /** A typed vertical probe must also support the raw point near ground. */
    DISCGOLFTOUR_API bool IsSupportedSettledSurfaceProbe(
        bool bSurfaceTraceHit,
        const FVector& RawLocationCm,
        const FVector& GroundLocationCm,
        float MaximumVerticalSeparationCm = 100.0f);

    /**
     * Project only a restored playable lie's height onto supported course
     * ground. Raw impact, relief, penalty, surface, and derived provenance are
     * retained. Terminal hole checkpoints bypass this helper entirely.
     * OutLieState is unchanged when the supporting probe is rejected.
     */
    DISCGOLFTOUR_API bool TryProjectRestoredLieToSupportedGround(
        const FDiscGolfLieState& SavedLieState,
        bool bSurfaceTraceHit,
        const FVector& GroundLocationCm,
        FDiscGolfLieState& OutLieState,
        float MaximumVerticalSeparationCm = 100.0f);

    /** A finite traced course projection is in bounds when it is not a penalty surface. */
    DISCGOLFTOUR_API bool IsInBoundsSurfaceProbe(
        bool bSurfaceTraceHit,
        ECourseSurfaceType Surface,
        const FVector& RawLocationCm,
        const FVector& GroundLocationCm);

    DISCGOLFTOUR_API bool IsPenaltySurface(ECourseSurfaceType Surface);
    DISCGOLFTOUR_API FString SurfaceName(ECourseSurfaceType Surface);
    DISCGOLFTOUR_API FString PenaltyName(EDiscGolfPenaltyType Penalty);
    DISCGOLFTOUR_API FString LieEffectsText(const FLieEffectProfile& Effects);
}
