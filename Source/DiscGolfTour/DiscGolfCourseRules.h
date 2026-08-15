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

    DISCGOLFTOUR_API bool IsPenaltySurface(ECourseSurfaceType Surface);
    DISCGOLFTOUR_API FString SurfaceName(ECourseSurfaceType Surface);
    DISCGOLFTOUR_API FString PenaltyName(EDiscGolfPenaltyType Penalty);
    DISCGOLFTOUR_API FString LieEffectsText(const FLieEffectProfile& Effects);
}
