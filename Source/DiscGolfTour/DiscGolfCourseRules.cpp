#include "DiscGolfCourseRules.h"

#include "DiscGolfCourseSurfaceActor.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
bool TrySurfaceFromName(const FString& Name, ECourseSurfaceType& OutSurface)
{
    if (Name.Contains(TEXT("OutOfBounds"), ESearchCase::IgnoreCase)
        || Name.Contains(TEXT("Out_Of_Bounds"), ESearchCase::IgnoreCase)
        || Name.Equals(TEXT("OB"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::OutOfBounds;
        return true;
    }
    if (Name.Contains(TEXT("Hazard"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::Hazard;
        return true;
    }
    if (Name.Contains(TEXT("DeepRough"), ESearchCase::IgnoreCase)
        || Name.Contains(TEXT("Deep_Rough"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::DeepRough;
        return true;
    }
    if (Name.Contains(TEXT("LightRough"), ESearchCase::IgnoreCase)
        || Name.Contains(TEXT("Light_Rough"), ESearchCase::IgnoreCase)
        || Name.Contains(TEXT("Rough"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::LightRough;
        return true;
    }
    if (Name.Contains(TEXT("TeePad"), ESearchCase::IgnoreCase)
        || Name.Contains(TEXT("Tee_Pad"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::TeePad;
        return true;
    }
    if (Name.Contains(TEXT("Dirt"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::Dirt;
        return true;
    }
    if (Name.Contains(TEXT("Rock"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::Rock;
        return true;
    }
    if (Name.Contains(TEXT("Fairway"), ESearchCase::IgnoreCase))
    {
        OutSurface = ECourseSurfaceType::Fairway;
        return true;
    }
    return false;
}
}

bool DiscGolfCourseRules::HasSurfaceIdentity(
    const AActor* SurfaceActor,
    const UPhysicalMaterial* PhysicalMaterial)
{
    if (Cast<ADiscGolfCourseSurfaceActor>(SurfaceActor)) return true;
    if (SurfaceActor && (SurfaceActor->ActorHasTag(TagZoneOutOfBounds)
        || SurfaceActor->ActorHasTag(TagZoneHazard)
        || SurfaceActor->ActorHasTag(TagSurfaceDeepRough)
        || SurfaceActor->ActorHasTag(TagSurfaceLightRough)
        || SurfaceActor->ActorHasTag(TagSurfaceRough)
        || SurfaceActor->ActorHasTag(TagSurfaceDirt)
        || SurfaceActor->ActorHasTag(TagSurfaceRock)
        || SurfaceActor->ActorHasTag(TagSurfaceTeePad)
        || SurfaceActor->ActorHasTag(TagSurfaceFairway))) return true;

    ECourseSurfaceType Ignored = ECourseSurfaceType::Fairway;
    return PhysicalMaterial && TrySurfaceFromName(PhysicalMaterial->GetName(), Ignored);
}

ECourseSurfaceType DiscGolfCourseRules::ResolveSurface(
    const AActor* SurfaceActor,
    const UPhysicalMaterial* PhysicalMaterial)
{
    if (const ADiscGolfCourseSurfaceActor* CourseSurface = Cast<ADiscGolfCourseSurfaceActor>(SurfaceActor))
    {
        return CourseSurface->GetCourseSurfaceType();
    }
    if (SurfaceActor)
    {
        if (SurfaceActor->ActorHasTag(TagZoneOutOfBounds)) return ECourseSurfaceType::OutOfBounds;
        if (SurfaceActor->ActorHasTag(TagZoneHazard)) return ECourseSurfaceType::Hazard;
        if (SurfaceActor->ActorHasTag(TagSurfaceDeepRough)) return ECourseSurfaceType::DeepRough;
        if (SurfaceActor->ActorHasTag(TagSurfaceLightRough)
            || SurfaceActor->ActorHasTag(TagSurfaceRough)) return ECourseSurfaceType::LightRough;
        if (SurfaceActor->ActorHasTag(TagSurfaceDirt)) return ECourseSurfaceType::Dirt;
        if (SurfaceActor->ActorHasTag(TagSurfaceRock)) return ECourseSurfaceType::Rock;
        if (SurfaceActor->ActorHasTag(TagSurfaceTeePad)) return ECourseSurfaceType::TeePad;
        if (SurfaceActor->ActorHasTag(TagSurfaceFairway)) return ECourseSurfaceType::Fairway;
    }

    ECourseSurfaceType Resolved = ECourseSurfaceType::Fairway;
    return PhysicalMaterial && TrySurfaceFromName(PhysicalMaterial->GetName(), Resolved)
        ? Resolved
        : ECourseSurfaceType::Fairway;
}

EGroundSurfaceType DiscGolfCourseRules::GroundResponseSurface(ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::TeePad: return EGroundSurfaceType::TeePad;
        case ECourseSurfaceType::LightRough:
        case ECourseSurfaceType::DeepRough:
        case ECourseSurfaceType::Hazard: return EGroundSurfaceType::Rough;
        case ECourseSurfaceType::Dirt: return EGroundSurfaceType::Dirt;
        case ECourseSurfaceType::Rock: return EGroundSurfaceType::Rock;
        case ECourseSurfaceType::OutOfBounds:
        case ECourseSurfaceType::Fairway:
        default: return EGroundSurfaceType::Fairway;
    }
}

FLieEffectProfile DiscGolfCourseRules::LieEffects(ECourseSurfaceType Surface)
{
    FLieEffectProfile Effects;
    switch (Surface)
    {
        case ECourseSurfaceType::LightRough:
            Effects.ProfileId = TEXT("LightRough");
            Effects.PowerMultiplier = 0.96f;
            Effects.TimingErrorMultiplier = 1.10f;
            break;
        case ECourseSurfaceType::DeepRough:
            Effects.ProfileId = TEXT("DeepRough");
            Effects.PowerMultiplier = 0.88f;
            Effects.TimingErrorMultiplier = 1.25f;
            break;
        case ECourseSurfaceType::Hazard:
            Effects.ProfileId = TEXT("Hazard");
            Effects.PowerMultiplier = 0.92f;
            Effects.TimingErrorMultiplier = 1.15f;
            break;
        default:
            Effects.ProfileId = TEXT("Clean");
            break;
    }
    return Effects;
}

ELieType DiscGolfCourseRules::LieTypeForSurfaceAndDistance(
    ECourseSurfaceType Surface,
    float DistanceToBasketMeters,
    bool bIsTee)
{
    if (bIsTee) return ELieType::Tee;
    if (Surface == ECourseSurfaceType::Hazard) return ELieType::Hazard;
    if (DistanceToBasketMeters <= 10.0f) return ELieType::Circle1;
    if (DistanceToBasketMeters <= 20.0f) return ELieType::Circle2;
    if (Surface == ECourseSurfaceType::DeepRough) return ELieType::DeepRough;
    if (Surface == ECourseSurfaceType::LightRough) return ELieType::LightRough;
    return ELieType::Fairway;
}

FThrowCommand DiscGolfCourseRules::ApplyLieEffects(
    const FThrowCommand& Command,
    const FLieEffectProfile& Effects)
{
    FThrowCommand Result = Command;
    Result.Power01 = FMath::Clamp(Command.Power01 * FMath::Max(Effects.PowerMultiplier, 0.0f), 0.0f, 1.0f);
    Result.TimingError = FMath::Clamp(
        Command.TimingError * FMath::Max(Effects.TimingErrorMultiplier, 0.0f), -1.0f, 1.0f);
    return Result;
}

FDiscGolfLieState DiscGolfCourseRules::ResolveLie(
    ECourseSurfaceType SurfaceAtRest,
    const FVector& RawDiscLocationCm,
    const FVector& LastInBoundsLocationCm,
    const FVector& BasketLocationCm,
    bool bIsTee)
{
    FDiscGolfLieState Result;
    Result.SurfaceAtRest = SurfaceAtRest;
    Result.RawDiscLocationCm = RawDiscLocationCm;
    Result.LieLocationCm = RawDiscLocationCm;
    Result.PlayingSurface = SurfaceAtRest;

    if (SurfaceAtRest == ECourseSurfaceType::OutOfBounds)
    {
        Result.PenaltyType = EDiscGolfPenaltyType::OutOfBounds;
        Result.ReliefRule = EDiscGolfReliefRule::LastInBounds;
        Result.PenaltyStrokes = 1;
        Result.LieLocationCm = LastInBoundsLocationCm;
        Result.PlayingSurface = ECourseSurfaceType::Fairway;
    }
    else if (SurfaceAtRest == ECourseSurfaceType::Hazard)
    {
        Result.PenaltyType = EDiscGolfPenaltyType::Hazard;
        Result.ReliefRule = EDiscGolfReliefRule::PlayFromResult;
        Result.PenaltyStrokes = 1;
    }

    Result.DistanceToBasketMeters = FVector::Dist2D(Result.LieLocationCm, BasketLocationCm) / 100.0f;
    Result.LieType = LieTypeForSurfaceAndDistance(Result.PlayingSurface, Result.DistanceToBasketMeters, bIsTee);
    Result.ShotContext = Result.DistanceToBasketMeters <= 10.0f
        ? EDiscShotContext::Circle1Putt
        : Result.DistanceToBasketMeters <= 20.0f
            ? EDiscShotContext::Circle2Putt
            : EDiscShotContext::Drive;
    if (bIsTee) Result.ShotContext = EDiscShotContext::Drive;
    Result.Effects = LieEffects(Result.PlayingSurface);
    return Result;
}

FVector DiscGolfCourseRules::ReliefPointInsideBoundary(
    const FVector& InsideBoundaryCm,
    const FVector& OutsideBoundaryCm,
    float ReliefDistanceCm)
{
    const FVector Inward = FVector(
        InsideBoundaryCm.X - OutsideBoundaryCm.X,
        InsideBoundaryCm.Y - OutsideBoundaryCm.Y,
        0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    return InsideBoundaryCm + Inward * FMath::Max(ReliefDistanceCm, 0.0f);
}

bool DiscGolfCourseRules::IsPenaltySurface(ECourseSurfaceType Surface)
{
    return Surface == ECourseSurfaceType::OutOfBounds || Surface == ECourseSurfaceType::Hazard;
}

FString DiscGolfCourseRules::SurfaceName(ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::TeePad: return TEXT("TEE PAD");
        case ECourseSurfaceType::LightRough: return TEXT("LIGHT ROUGH");
        case ECourseSurfaceType::DeepRough: return TEXT("DEEP ROUGH");
        case ECourseSurfaceType::Dirt: return TEXT("DIRT");
        case ECourseSurfaceType::Rock: return TEXT("ROCK");
        case ECourseSurfaceType::OutOfBounds: return TEXT("OUT OF BOUNDS");
        case ECourseSurfaceType::Hazard: return TEXT("HAZARD");
        case ECourseSurfaceType::Fairway:
        default: return TEXT("FAIRWAY");
    }
}

FString DiscGolfCourseRules::PenaltyName(EDiscGolfPenaltyType Penalty)
{
    switch (Penalty)
    {
        case EDiscGolfPenaltyType::OutOfBounds: return TEXT("OUT OF BOUNDS");
        case EDiscGolfPenaltyType::Hazard: return TEXT("HAZARD");
        case EDiscGolfPenaltyType::None:
        default: return TEXT("NO PENALTY");
    }
}

FString DiscGolfCourseRules::LieEffectsText(const FLieEffectProfile& Effects)
{
    if (Effects.ProfileId == FName(TEXT("Clean"))) return TEXT("CLEAN STANCE - 100% POWER / NORMAL TIMING");
    return FString::Printf(TEXT("%s - %.0f%% POWER / %.0f%% TIMING ERROR"),
        *Effects.ProfileId.ToString().ToUpper(),
        Effects.PowerMultiplier * 100.0f,
        Effects.TimingErrorMultiplier * 100.0f);
}
