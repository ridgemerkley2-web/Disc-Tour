#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"

namespace DiscGolfMath
{
    inline FDiscGolfFixtureImpactProfile FixtureImpactProfile(EDiscGolfFixtureType FixtureType)
    {
        FDiscGolfFixtureImpactProfile Profile;
        Profile.FixtureType = FixtureType;
        switch (FixtureType)
        {
            case EDiscGolfFixtureType::Tree:
                Profile.NormalRestitution = 0.22f;
                Profile.TangentialRetention = 0.46f;
                Profile.SpinRetention = 0.56f;
                break;
            case EDiscGolfFixtureType::DenseGrass:
                Profile.NormalRestitution = 0.0f;
                Profile.TangentialRetention = 1.0f;
                Profile.SpinRetention = 0.64f;
                Profile.PassThroughSpeedRetention = 0.56f;
                Profile.bOverlapVolume = true;
                break;
            case EDiscGolfFixtureType::Rock:
                Profile.NormalRestitution = 0.46f;
                Profile.TangentialRetention = 0.72f;
                Profile.SpinRetention = 0.82f;
                break;
            case EDiscGolfFixtureType::Sign:
                Profile.NormalRestitution = 0.31f;
                Profile.TangentialRetention = 0.55f;
                Profile.SpinRetention = 0.68f;
                break;
            case EDiscGolfFixtureType::Unknown:
            default:
                break;
        }
        return Profile;
    }

    inline FDiscGolfFixtureImpactResult ResolveFixtureImpact(
        const FVector& IncomingVelocityMps,
        const FVector& ImpactNormal,
        EDiscGolfFixtureType FixtureType)
    {
        const FDiscGolfFixtureImpactProfile Profile = FixtureImpactProfile(FixtureType);
        FDiscGolfFixtureImpactResult Result;
        Result.FixtureType = FixtureType;
        Result.ImpactSpeedMps = IncomingVelocityMps.Size();
        Result.SpinMultiplier = Profile.SpinRetention;

        const FVector Normal = ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        const float IntoSurface = FVector::DotProduct(IncomingVelocityMps, Normal);
        Result.NormalSpeedMps = FMath::Max(-IntoSurface, 0.0f);
        const FVector TangentVelocity = IncomingVelocityMps - IntoSurface * Normal;
        const FVector ReflectedNormal = IntoSurface < 0.0f
            ? -IntoSurface * Profile.NormalRestitution * Normal
            : IntoSurface * Normal;
        Result.VelocityMps = TangentVelocity * Profile.TangentialRetention + ReflectedNormal;
        return Result;
    }

    inline FDiscGolfFixtureImpactResult ResolveFixtureOverlap(
        const FVector& IncomingVelocityMps,
        EDiscGolfFixtureType FixtureType)
    {
        const FDiscGolfFixtureImpactProfile Profile = FixtureImpactProfile(FixtureType);
        FDiscGolfFixtureImpactResult Result;
        Result.FixtureType = FixtureType;
        Result.ImpactSpeedMps = IncomingVelocityMps.Size();
        Result.SpinMultiplier = Profile.SpinRetention;
        Result.bPassThrough = Profile.bOverlapVolume;
        Result.VelocityMps = IncomingVelocityMps * Profile.PassThroughSpeedRetention;
        return Result;
    }

    constexpr float ReleasePerfectError = 0.12f;
    constexpr float ReleaseGreatError = 0.34f;
    constexpr float ReleaseGoodError = 0.62f;

    inline float EffectiveHoleDistanceFeet(const FVector& TeeLocationCm, const FVector& BasketLocationCm)
    {
        const float MeasuredFeet = FVector::Dist(TeeLocationCm, BasketLocationCm) / 30.48f;
        const float ElevationChangeFeet = (BasketLocationCm.Z - TeeLocationCm.Z) / 30.48f;
        return MeasuredFeet + ElevationChangeFeet * 3.0f;
    }

    inline float DiscStabilityMomentNm(float SpeedMps, float HighSpeedTurnMomentNm, float LowSpeedFadeMomentNm,
        float TurnStartsAboveMps, float FadeStartsBelowMps)
    {
        const float TurnWeight = FMath::Clamp((SpeedMps - TurnStartsAboveMps) / 8.0f, 0.0f, 1.0f);
        const float FadeWeight = FMath::Clamp((FadeStartsBelowMps - SpeedMps) / 8.0f, 0.0f, 1.0f);
        return HighSpeedTurnMomentNm * TurnWeight - LowSpeedFadeMomentNm * FadeWeight;
    }

    inline float NormalizeTimingError(float TimingNeedle01, float IdealTiming01, float TimingMissSpan01)
    {
        const float SafeSpan = FMath::Max(TimingMissSpan01, 0.01f);
        return FMath::Clamp((TimingNeedle01 - IdealTiming01) / SafeSpan, -1.0f, 1.0f);
    }

    inline EReleaseGrade ReleaseGrade(float TimingError)
    {
        const float AbsError = FMath::Abs(FMath::Clamp(TimingError, -1.0f, 1.0f));
        if (AbsError <= ReleasePerfectError) return EReleaseGrade::Perfect;
        if (AbsError <= ReleaseGreatError) return EReleaseGrade::Great;
        if (AbsError <= ReleaseGoodError) return EReleaseGrade::Good;
        return EReleaseGrade::Poor;
    }

    /** Unreal's positive pitch rotates +X toward -Z, so positive gameplay launch must negate the quaternion angle. */
    inline FVector LaunchDirectionFromFlat(const FVector& FlatDirection, float LaunchAngleDeg)
    {
        const FVector Flat = FVector(FlatDirection.X, FlatDirection.Y, 0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Flat).GetSafeNormal(SMALL_NUMBER, FVector::RightVector);
        return FQuat(Right, FMath::DegreesToRadians(-LaunchAngleDeg)).RotateVector(Flat).GetSafeNormal();
    }

    inline EDiscShotContext ShotContextForLie(ELieType Lie)
    {
        if (Lie == ELieType::Circle1) return EDiscShotContext::Circle1Putt;
        if (Lie == ELieType::Circle2) return EDiscShotContext::Circle2Putt;
        return EDiscShotContext::Drive;
    }

    inline float RecommendedPuttPower01(float DistanceMeters)
    {
        return FMath::Clamp(0.34f + FMath::Max(DistanceMeters, 0.0f) * 0.029f, 0.38f, 0.94f);
    }

    inline float EstimatedPuttRangeMeters(float Power01)
    {
        return FMath::Max((FMath::Clamp(Power01, 0.0f, 1.0f) - 0.34f) / 0.029f, 0.0f);
    }

    inline float SignedAimErrorDeg(const FVector& PlayerForward, const FVector& ToBasket)
    {
        const FVector Forward = FVector(PlayerForward.X, PlayerForward.Y, 0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
        const FVector Target = FVector(ToBasket.X, ToBasket.Y, 0.0f).GetSafeNormal(SMALL_NUMBER, Forward);
        const float CrossZ = FVector::CrossProduct(Forward, Target).Z;
        const float Dot = FMath::Clamp(FVector::DotProduct(Forward, Target), -1.0f, 1.0f);
        return FMath::RadiansToDegrees(FMath::Atan2(CrossZ, Dot));
    }

    inline FBasketContactEvaluation EvaluateBasketContact(
        const FVector& RelativeLocationCm,
        const FVector& VelocityMps)
    {
        FBasketContactEvaluation Evaluation;
        Evaluation.IncomingSpeedMps = VelocityMps.Size();
        if (Evaluation.IncomingSpeedMps < 0.25f) return Evaluation;

        const FVector HorizontalPosition(RelativeLocationCm.X, RelativeLocationCm.Y, 0.0f);
        const FVector HorizontalVelocityCm(VelocityMps.X * 100.0f, VelocityMps.Y * 100.0f, 0.0f);
        const float HorizontalSpeedSq = HorizontalVelocityCm.SizeSquared2D();
        if (HorizontalSpeedSq < 1.0f) return Evaluation;

        const float ClosestTime = FMath::Clamp(
            -FVector::DotProduct(HorizontalPosition, HorizontalVelocityCm) / HorizontalSpeedSq,
            0.0f, 0.18f);
        const FVector Predicted = RelativeLocationCm + VelocityMps * (ClosestTime * 100.0f)
            + FVector(0.0f, 0.0f, -0.5f * 980.665f * ClosestTime * ClosestTime);
        Evaluation.PredictedRadialCm = FVector(Predicted.X, Predicted.Y, 0.0f).Size();
        Evaluation.PredictedHeightCm = Predicted.Z;

        // The overlap starts outside the chains. Ignore a disc whose closest approach is now behind it.
        if (ClosestTime <= KINDA_SMALL_NUMBER && Evaluation.PredictedRadialCm > 38.0f) return Evaluation;
        if (Evaluation.PredictedRadialCm > 48.0f) return Evaluation;

        if (Evaluation.PredictedHeightCm >= 139.0f)
        {
            Evaluation.Result = EBasketContactResult::BandRejection;
            Evaluation.DeflectedVelocityMps = FVector(
                VelocityMps.X * 0.36f,
                VelocityMps.Y * 0.36f,
                -FMath::Max(FMath::Abs(VelocityMps.Z) * 0.35f, 1.15f));
            return Evaluation;
        }
        if (Evaluation.PredictedHeightCm <= 88.0f)
        {
            Evaluation.Result = EBasketContactResult::TrayRejection;
            Evaluation.DeflectedVelocityMps = FVector(
                VelocityMps.X * 0.42f,
                VelocityMps.Y * 0.42f,
                FMath::Max(FMath::Abs(VelocityMps.Z) * 0.30f, 1.35f));
            return Evaluation;
        }

        const bool bCenterChains = Evaluation.PredictedRadialCm <= 31.0f
            && Evaluation.PredictedHeightCm >= 94.0f
            && Evaluation.PredictedHeightCm <= 136.0f
            && Evaluation.IncomingSpeedMps >= 2.0f
            && Evaluation.IncomingSpeedMps <= 13.5f
            && FMath::Abs(VelocityMps.Z) <= 6.0f;
        if (bCenterChains)
        {
            Evaluation.Result = EBasketContactResult::Caught;
            return Evaluation;
        }

        Evaluation.Result = EBasketContactResult::ChainDeflection;
        FVector Outward(Predicted.X, Predicted.Y, 0.0f);
        if (!Outward.Normalize())
        {
            Outward = -HorizontalVelocityCm.GetSafeNormal(SMALL_NUMBER, FVector::BackwardVector);
        }
        Evaluation.DeflectedVelocityMps = Outward * FMath::Max(1.25f, Evaluation.IncomingSpeedMps * 0.24f)
            + FVector(0.0f, 0.0f, -0.85f);
        return Evaluation;
    }

    inline FThrowRelease ResolveThrowRelease(const FThrowCommand& Command)
    {
        FThrowRelease Release;
        Release.TimingError = FMath::Clamp(Command.TimingError, -1.0f, 1.0f);
        Release.Grade = ReleaseGrade(Release.TimingError);
        Release.Timing = Release.Grade == EReleaseGrade::Perfect
            ? EReleaseTiming::OnTime
            : (Release.TimingError < 0.0f ? EReleaseTiming::Early : EReleaseTiming::Late);

        const float AbsError = FMath::Abs(Release.TimingError);
        const float Severity01 = FMath::Clamp(
            (AbsError - ReleasePerfectError) / (1.0f - ReleasePerfectError), 0.0f, 1.0f);
        const float SignedSeverity = FMath::Sign(Release.TimingError) * Severity01;
        const float HandSign = Command.ThrowStyle == EThrowStyle::Backhand ? 1.0f : -1.0f;

        Release.Quality01 = 1.0f - Severity01;
        const bool bPutting = Command.ShotContext != EDiscShotContext::Drive;
        Release.SpeedMultiplier = 1.0f - (bPutting ? 0.10f : 0.16f) * FMath::Pow(Severity01, 1.25f);
        Release.SpinMultiplier = 1.0f - (bPutting ? 0.15f : 0.22f) * FMath::Pow(Severity01, 1.15f);
        Release.AimOffsetDeg = HandSign * SignedSeverity * (bPutting ? 2.8f : 6.0f);
        Release.HyzerOffsetDeg = SignedSeverity * (bPutting ? 1.5f : 4.0f);
        Release.NoseOffsetDeg = SignedSeverity * (bPutting ? 1.2f : 3.0f);
        Release.LaunchOffsetDeg = SignedSeverity * (bPutting ? 1.0f : 2.0f);

        const float Power01 = FMath::Clamp(Command.Power01, 0.0f, 1.0f);
        if (bPutting)
        {
            const float MaxReleaseMps = Command.ShotContext == EDiscShotContext::Circle1Putt ? 13.0f : 13.7f;
            Release.ReleaseSpeedMps = FMath::Lerp(3.8f, MaxReleaseMps, Power01) * Release.SpeedMultiplier;
            Release.SpinRpm = FMath::Lerp(220.0f, 620.0f, Power01) * Release.SpinMultiplier;
        }
        else
        {
            const float MinReleaseMps = Command.ThrowStyle == EThrowStyle::Backhand ? 8.5f : 8.0f;
            const float MaxReleaseMps = Command.ThrowStyle == EThrowStyle::Backhand ? 30.5f : 27.5f;
            const float MaxSpinRpm = Command.ThrowStyle == EThrowStyle::Backhand ? 1050.0f : 900.0f;
            Release.ReleaseSpeedMps = FMath::Lerp(MinReleaseMps, MaxReleaseMps, Power01) * Release.SpeedMultiplier;
            Release.SpinRpm = FMath::Lerp(300.0f, MaxSpinRpm, Power01) * Release.SpinMultiplier;
        }

        Release.EffectiveHyzerDeg = FMath::Clamp(Command.HyzerDeg + Release.HyzerOffsetDeg, -34.0f, 34.0f);
        Release.EffectiveNoseAngleDeg = FMath::Clamp(Command.NoseAngleDeg + Release.NoseOffsetDeg, -7.0f, 11.0f);
        Release.EffectiveLaunchAngleDeg = FMath::Clamp(Command.LaunchAngleDeg + Release.LaunchOffsetDeg, -5.0f, 35.0f);
        Release.ThrowStyle = Command.ThrowStyle;
        Release.ShotContext = Command.ShotContext;
        Release.Direction = Command.Direction.GetSafeNormal();
        if (Release.Direction.IsNearlyZero()) Release.Direction = FVector::ForwardVector;
        return Release;
    }

    inline FGroundSurfaceProfile GroundSurfaceProfile(EGroundSurfaceType Surface)
    {
        FGroundSurfaceProfile Profile;
        Profile.Surface = Surface;
        switch (Surface)
        {
            case EGroundSurfaceType::TeePad:
                Profile.RestitutionScale = 1.10f;
                Profile.FrictionScale = 0.72f;
                Profile.ImpactSpinRetention = 0.92f;
                Profile.SkipMinSpeedMps = 6.5f;
                Profile.SkipMaxIncidenceDeg = 21.0f;
                Profile.MaxConsecutiveSkips = 4;
                Profile.EdgeRollMinAngleDeg = 54.0f;
                Profile.EdgeRollMinSpeedMps = 2.8f;
                Profile.EdgeRollMinSpinRpm = 140.0f;
                Profile.SlideDecelerationMps2 = 1.5f;
                Profile.RollDecelerationMps2 = 0.8f;
                Profile.GroundSpinDecayPerSecond = 1.05f;
                Profile.SettleSpeedMps = 1.0f;
                break;
            case EGroundSurfaceType::Rough:
                Profile.RestitutionScale = 0.55f;
                Profile.FrictionScale = 1.65f;
                Profile.ImpactSpinRetention = 0.60f;
                Profile.SkipMinSpeedMps = 12.0f;
                Profile.SkipMaxIncidenceDeg = 10.0f;
                Profile.MaxConsecutiveSkips = 1;
                Profile.EdgeRollMinAngleDeg = 60.0f;
                Profile.EdgeRollMinSpeedMps = 4.5f;
                Profile.EdgeRollMinSpinRpm = 220.0f;
                Profile.SlideDecelerationMps2 = 5.5f;
                Profile.RollDecelerationMps2 = 3.4f;
                Profile.GroundSpinDecayPerSecond = 2.8f;
                Profile.SettleSpeedMps = 1.5f;
                break;
            case EGroundSurfaceType::Dirt:
                Profile.RestitutionScale = 0.82f;
                Profile.FrictionScale = 0.90f;
                Profile.ImpactSpinRetention = 0.78f;
                Profile.SkipMinSpeedMps = 9.0f;
                Profile.SkipMaxIncidenceDeg = 16.0f;
                Profile.MaxConsecutiveSkips = 2;
                Profile.EdgeRollMinAngleDeg = 57.0f;
                Profile.EdgeRollMinSpeedMps = 3.5f;
                Profile.EdgeRollMinSpinRpm = 170.0f;
                Profile.SlideDecelerationMps2 = 2.8f;
                Profile.RollDecelerationMps2 = 1.5f;
                Profile.GroundSpinDecayPerSecond = 1.65f;
                Profile.SettleSpeedMps = 1.2f;
                break;
            case EGroundSurfaceType::Rock:
                Profile.RestitutionScale = 1.55f;
                Profile.FrictionScale = 0.55f;
                Profile.ImpactSpinRetention = 0.93f;
                Profile.SkipMinSpeedMps = 5.5f;
                Profile.SkipMaxIncidenceDeg = 26.0f;
                Profile.MaxConsecutiveSkips = 5;
                Profile.EdgeRollMinAngleDeg = 50.0f;
                Profile.EdgeRollMinSpeedMps = 2.5f;
                Profile.EdgeRollMinSpinRpm = 110.0f;
                Profile.SlideDecelerationMps2 = 1.2f;
                Profile.RollDecelerationMps2 = 0.65f;
                Profile.GroundSpinDecayPerSecond = 0.85f;
                Profile.SettleSpeedMps = 0.9f;
                break;
            case EGroundSurfaceType::Fairway:
            default:
                break;
        }
        return Profile;
    }

    inline FGroundImpactResult ResolveGroundImpact(
        const FVector& VelocityMps,
        const FVector& SurfaceNormal,
        const FVector& DiscNormal,
        float SpinRpm,
        float BaseRestitution,
        float BaseFriction,
        EGroundSurfaceType Surface,
        int32 PriorConsecutiveSkips = 0)
    {
        FGroundImpactResult Result;
        Result.Surface = Surface;
        const FGroundSurfaceProfile Profile = GroundSurfaceProfile(Surface);
        const FVector Normal = SurfaceNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        const FVector SafeDiscNormal = DiscNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
        const float SignedNormalSpeed = FVector::DotProduct(VelocityMps, Normal);
        const FVector TangentialVelocity = VelocityMps - SignedNormalSpeed * Normal;
        Result.ImpactSpeedMps = VelocityMps.Size();
        Result.ApproachSpeedMps = FMath::Max(-SignedNormalSpeed, 0.0f);
        Result.TangentialSpeedMps = TangentialVelocity.Size();
        Result.IncidenceAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(
            Result.ApproachSpeedMps, FMath::Max(Result.TangentialSpeedMps, 1.0e-4f)));
        Result.DiscEdgeAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(
            FMath::Abs(FVector::DotProduct(SafeDiscNormal, Normal)), 0.0f, 1.0f)));
        Result.EffectiveRestitution = FMath::Clamp(BaseRestitution * Profile.RestitutionScale, 0.02f, 0.72f);
        Result.EffectiveFriction = FMath::Clamp(BaseFriction * Profile.FrictionScale, 0.05f, 0.95f);

        if (Result.ImpactSpeedMps <= Profile.SettleSpeedMps ||
            Result.TangentialSpeedMps <= Profile.SettleSpeedMps * 0.55f)
        {
            Result.State = EDiscGroundState::Settled;
            Result.VelocityMps = FVector::ZeroVector;
            Result.SpinMultiplier = 0.0f;
            return Result;
        }

        const FVector TangentDirection = TangentialVelocity.GetSafeNormal();
        const bool bCanEdgeRoll = Result.DiscEdgeAngleDeg >= Profile.EdgeRollMinAngleDeg &&
            Result.TangentialSpeedMps >= Profile.EdgeRollMinSpeedMps &&
            FMath::Abs(SpinRpm) >= Profile.EdgeRollMinSpinRpm;
        if (bCanEdgeRoll)
        {
            Result.State = EDiscGroundState::EdgeRolling;
            const float Retention = FMath::Clamp(1.0f - Result.EffectiveFriction * 0.10f, 0.72f, 0.98f);
            Result.VelocityMps = TangentDirection * Result.TangentialSpeedMps * Retention;
            Result.SpinMultiplier = Profile.ImpactSpinRetention * 0.90f;
            return Result;
        }

        const float RestitutionBias = FMath::Clamp((Result.EffectiveRestitution - 0.08f) / 0.20f, 0.0f, 1.0f);
        const float PlasticAdjustedSkipMin = Profile.SkipMinSpeedMps * FMath::Lerp(1.12f, 0.88f, RestitutionBias);
        const bool bHasSkipGeometry = Result.ApproachSpeedMps >= 0.35f &&
            Result.TangentialSpeedMps >= PlasticAdjustedSkipMin &&
            Result.IncidenceAngleDeg <= Profile.SkipMaxIncidenceDeg;
        const bool bCanSkip = bHasSkipGeometry && PriorConsecutiveSkips < Profile.MaxConsecutiveSkips;
        if (bCanSkip)
        {
            Result.State = EDiscGroundState::Skipping;
            const float ShallowWeight = 1.0f - FMath::Clamp(
                Result.IncidenceAngleDeg / FMath::Max(Profile.SkipMaxIncidenceDeg, 1.0f), 0.0f, 1.0f);
            const float TangentRetention = FMath::Clamp(
                1.0f - Result.EffectiveFriction * (0.09f + 0.04f * (1.0f - ShallowWeight)), 0.68f, 0.98f);
            const float SkipLiftMps = FMath::Clamp(
                Result.ApproachSpeedMps * Result.EffectiveRestitution +
                Result.TangentialSpeedMps * 0.075f * ShallowWeight,
                0.45f, 3.8f);
            Result.VelocityMps = TangentDirection * Result.TangentialSpeedMps * TangentRetention + Normal * SkipLiftMps;
            Result.SpinMultiplier = Profile.ImpactSpinRetention * 0.96f;
            return Result;
        }

        Result.State = EDiscGroundState::Sliding;
        float SlideRetention = FMath::Clamp(1.0f - Result.EffectiveFriction * 0.22f, 0.55f, 0.96f);
        if (bHasSkipGeometry && PriorConsecutiveSkips >= Profile.MaxConsecutiveSkips)
        {
            // A capped skip train is the point where the rim/flight plate loses its
            // clean rebound and starts scrubbing across the surface. Absorb that
            // transition energy so a high-speed low hop does not become a 50 m skid.
            SlideRetention *= 0.65f;
        }
        Result.VelocityMps = TangentDirection * Result.TangentialSpeedMps * SlideRetention;
        Result.SpinMultiplier = Profile.ImpactSpinRetention * 0.82f;
        return Result;
    }

    inline FVector ApplyGroundDeceleration(const FVector& TangentialVelocityMps, float DecelerationMps2, float Dt)
    {
        const float Speed = TangentialVelocityMps.Size();
        if (Speed <= KINDA_SMALL_NUMBER || Dt <= 0.0f) return FVector::ZeroVector;
        const float NewSpeed = FMath::Max(0.0f, Speed - FMath::Max(DecelerationMps2, 0.0f) * Dt);
        return TangentialVelocityMps * (NewSpeed / Speed);
    }
}
