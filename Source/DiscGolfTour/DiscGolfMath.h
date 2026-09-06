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

    constexpr float ThrowCommandMinimumPower01 = 0.0f;
    constexpr float ThrowCommandMaximumPower01 = 1.0f;
    constexpr float ThrowCommandMinimumHyzerDeg = -34.0f;
    constexpr float ThrowCommandMaximumHyzerDeg = 34.0f;
    constexpr float ThrowCommandMinimumNoseAngleDeg = -7.0f;
    constexpr float ThrowCommandMaximumNoseAngleDeg = 11.0f;
    constexpr float ThrowCommandMinimumLaunchAngleDeg = -5.0f;
    constexpr float ThrowCommandMaximumLaunchAngleDeg = 35.0f;
    constexpr float ThrowCommandMinimumTimingError = -1.0f;
    constexpr float ThrowCommandMaximumTimingError = 1.0f;

    // Shared numerical safety ceilings. DiscCatalogSubsystem applies these same
    // bounds before resolution; the runtime seam repeats them so a direct caller
    // cannot bypass the catalog and feed pathological values to the solver.
    constexpr float DiscAeroMaximumGeometryOrInertia = 1.0f;
    constexpr float DiscAeroMaximumCoefficient = 10.0f;
    constexpr float DiscAeroMaximumStabilityMomentNm = 1.0f;
    constexpr float DiscAeroMaximumSpeedThresholdMps = 100.0f;
    constexpr float DiscAeroMaximumSpinDecayPerSecond = 100.0f;

    // /Engine/BasicShapes/Cylinder is 100 cm in diameter and height. This root
    // scale is therefore the authoritative 21 cm x 1.5 cm disc collision body.
    constexpr float CanonicalDiscCollisionScaleXY = 0.21f;
    constexpr float CanonicalDiscCollisionScaleZ = 0.015f;
    inline FVector CanonicalDiscCollisionScale()
    {
        return FVector(
            CanonicalDiscCollisionScaleXY,
            CanonicalDiscCollisionScaleXY,
            CanonicalDiscCollisionScaleZ);
    }

    constexpr int32 ResolvedDiscMinimumSpeed = 1;
    constexpr int32 ResolvedDiscMaximumSpeed = 15;
    constexpr int32 ResolvedDiscMinimumGlide = 1;
    constexpr int32 ResolvedDiscMaximumGlide = 7;
    constexpr float ResolvedDiscMinimumMassGrams = 130.0f;
    constexpr float ResolvedDiscMaximumMassGrams = 200.0f;
    constexpr int32 ResolvedDiscMaximumNicknameCharacters = 64;

    // Release angle limits match ResolveThrowRelease. Speed and spin ceilings
    // are deliberately wider than current gameplay and fixture-QA output while
    // still excluding finite values capable of destabilizing world integration.
    constexpr float ThrowReleaseMaximumSpeedMps = 100.0f;
    constexpr float ThrowReleaseMaximumSpinRpm = 5000.0f;
    constexpr float ThrowReleaseMaximumAimOffsetDeg = 6.0f;
    constexpr float ThrowReleaseMaximumHyzerOffsetDeg = 4.0f;
    constexpr float ThrowReleaseMaximumNoseOffsetDeg = 3.0f;
    constexpr float ThrowReleaseMaximumLaunchOffsetDeg = 2.0f;
    constexpr float ThrowReleaseMaximumWindPhaseOriginSeconds = 4096.0f;
    constexpr float ThrowReleaseMaximumLieTimingErrorMultiplier = 2.0f;

    inline bool IsValidDiscPlastic(EDiscPlastic Plastic)
    {
        switch (Plastic)
        {
            case EDiscPlastic::Base:
            case EDiscPlastic::Tour:
            case EDiscPlastic::Crystal:
                return true;
            default:
                return false;
        }
    }

    inline bool IsValidThrowStyle(EThrowStyle ThrowStyle)
    {
        switch (ThrowStyle)
        {
            case EThrowStyle::Backhand:
            case EThrowStyle::Forehand:
                return true;
            default:
                return false;
        }
    }

    inline bool IsValidHandedness(EDGHandedness Handedness)
    {
        switch (Handedness)
        {
            case EDGHandedness::Right:
            case EDGHandedness::Left:
                return true;
            default:
                return false;
        }
    }

    /** Sign for lateral release error, authored hyzer bank, and axial spin. */
    inline float ThrowRotationSign(EThrowStyle ThrowStyle, EDGHandedness Handedness)
    {
        const float StyleSign = ThrowStyle == EThrowStyle::Backhand ? 1.0f : -1.0f;
        const float HandednessSign = Handedness == EDGHandedness::Right ? 1.0f : -1.0f;
        return StyleSign * HandednessSign;
    }

    inline bool IsValidDiscShotContext(EDiscShotContext ShotContext)
    {
        switch (ShotContext)
        {
            case EDiscShotContext::Drive:
            case EDiscShotContext::Circle2Putt:
            case EDiscShotContext::Circle1Putt:
                return true;
            default:
                return false;
        }
    }

    inline bool IsValidReleaseGrade(EReleaseGrade Grade)
    {
        switch (Grade)
        {
            case EReleaseGrade::Perfect:
            case EReleaseGrade::Great:
            case EReleaseGrade::Good:
            case EReleaseGrade::Poor:
                return true;
            default:
                return false;
        }
    }

    inline bool IsValidReleaseTiming(EReleaseTiming Timing)
    {
        switch (Timing)
        {
            case EReleaseTiming::Early:
            case EReleaseTiming::OnTime:
            case EReleaseTiming::Late:
                return true;
            default:
                return false;
        }
    }

    /** Catalog-aligned fail-closed contract for every aerodynamic solver input. */
    inline bool IsDiscAeroProfileValid(
        const FDiscAeroProfile& Aero,
        FString* OutError = nullptr)
    {
        const auto Reject = [OutError](const FString& Error)
        {
            if (OutError) *OutError = Error;
            return false;
        };
        struct FNamedAeroValue
        {
            const TCHAR* Name;
            float Value;
            float Maximum;
        };

        const FNamedAeroValue PositiveValues[] = {
            { TEXT("MassKg"), Aero.MassKg, DiscAeroMaximumGeometryOrInertia },
            { TEXT("DiameterM"), Aero.DiameterM, DiscAeroMaximumGeometryOrInertia },
            { TEXT("AreaM2"), Aero.AreaM2, DiscAeroMaximumGeometryOrInertia },
            { TEXT("InertiaAxialKgM2"), Aero.InertiaAxialKgM2, DiscAeroMaximumGeometryOrInertia },
            { TEXT("InertiaPlanarKgM2"), Aero.InertiaPlanarKgM2, DiscAeroMaximumGeometryOrInertia },
            { TEXT("CLa"), Aero.CLa, DiscAeroMaximumCoefficient },
            { TEXT("CD0"), Aero.CD0, DiscAeroMaximumCoefficient },
            { TEXT("CDa"), Aero.CDa, DiscAeroMaximumCoefficient },
            { TEXT("TurnStartsAboveMps"), Aero.TurnStartsAboveMps, DiscAeroMaximumSpeedThresholdMps },
            { TEXT("FadeStartsBelowMps"), Aero.FadeStartsBelowMps, DiscAeroMaximumSpeedThresholdMps }
        };
        for (const FNamedAeroValue& Field : PositiveValues)
        {
            if (!FMath::IsFinite(Field.Value)
                || Field.Value <= 0.0f
                || Field.Value > Field.Maximum)
            {
                return Reject(FString::Printf(
                    TEXT("Aero.%s must be finite and in (0, %.3g]"),
                    Field.Name, Field.Maximum));
            }
        }

        const FNamedAeroValue SignedValues[] = {
            { TEXT("CL0"), Aero.CL0, DiscAeroMaximumCoefficient },
            { TEXT("CM0"), Aero.CM0, DiscAeroMaximumCoefficient },
            { TEXT("CMa"), Aero.CMa, DiscAeroMaximumCoefficient }
        };
        for (const FNamedAeroValue& Field : SignedValues)
        {
            if (!FMath::IsFinite(Field.Value)
                || FMath::Abs(Field.Value) > Field.Maximum)
            {
                return Reject(FString::Printf(
                    TEXT("Aero.%s must be finite with absolute value <= %.3g"),
                    Field.Name, Field.Maximum));
            }
        }

        const FNamedAeroValue NonNegativeValues[] = {
            { TEXT("HighSpeedTurnMomentNm"), Aero.HighSpeedTurnMomentNm,
                DiscAeroMaximumStabilityMomentNm },
            { TEXT("LowSpeedFadeMomentNm"), Aero.LowSpeedFadeMomentNm,
                DiscAeroMaximumStabilityMomentNm },
            { TEXT("SpinDecayPerSecond"), Aero.SpinDecayPerSecond,
                DiscAeroMaximumSpinDecayPerSecond }
        };
        for (const FNamedAeroValue& Field : NonNegativeValues)
        {
            if (!FMath::IsFinite(Field.Value)
                || Field.Value < 0.0f
                || Field.Value > Field.Maximum)
            {
                return Reject(FString::Printf(
                    TEXT("Aero.%s must be finite and in [0, %.3g]"),
                    Field.Name, Field.Maximum));
            }
        }

        if (!FMath::IsFinite(Aero.GroundRestitution)
            || Aero.GroundRestitution <= 0.0f
            || Aero.GroundRestitution > 1.0f)
        {
            return Reject(TEXT("Aero.GroundRestitution must be finite and in (0, 1]"));
        }
        if (!FMath::IsFinite(Aero.GroundFriction)
            || Aero.GroundFriction <= 0.0f
            || Aero.GroundFriction > 1.0f)
        {
            return Reject(TEXT("Aero.GroundFriction must be finite and in (0, 1]"));
        }

        if (OutError) OutError->Reset();
        return true;
    }

    /** Complete runtime contract for catalog-only and player-instance snapshots. */
    inline bool IsResolvedDiscDefinitionValid(
        const FResolvedDiscDefinition& Disc,
        FString* OutError = nullptr)
    {
        const auto Reject = [OutError](const FString& Error)
        {
            if (OutError) *OutError = Error;
            return false;
        };
        const bool bColorFinite = FMath::IsFinite(Disc.DiscColor.R)
            && FMath::IsFinite(Disc.DiscColor.G)
            && FMath::IsFinite(Disc.DiscColor.B)
            && FMath::IsFinite(Disc.DiscColor.A);

        // DiscInstanceId is intentionally optional: regression/catalog throws
        // have no player-owned instance, while equipment throws carry one.
        if (Disc.MoldId.IsNone()) return Reject(TEXT("Resolved disc requires a MoldId"));
        if (Disc.DisplayName.IsEmpty()) return Reject(TEXT("Resolved disc requires a DisplayName"));
        if (Disc.Speed < ResolvedDiscMinimumSpeed || Disc.Speed > ResolvedDiscMaximumSpeed)
            return Reject(TEXT("Resolved disc Speed must be in [1, 15]"));
        if (Disc.Glide < ResolvedDiscMinimumGlide || Disc.Glide > ResolvedDiscMaximumGlide)
            return Reject(TEXT("Resolved disc Glide must be in [1, 7]"));
        if (!FMath::IsFinite(Disc.Turn)) return Reject(TEXT("Resolved disc Turn must be finite"));
        if (!FMath::IsFinite(Disc.Fade)) return Reject(TEXT("Resolved disc Fade must be finite"));
        if (!IsValidDiscPlastic(Disc.Plastic))
            return Reject(TEXT("Resolved disc Plastic enum is invalid"));
        if (!FMath::IsFinite(Disc.DiscMassGrams)
            || Disc.DiscMassGrams < ResolvedDiscMinimumMassGrams
            || Disc.DiscMassGrams > ResolvedDiscMaximumMassGrams)
        {
            return Reject(TEXT("Resolved disc mass metadata must be finite and in [130, 200] grams"));
        }
        const float AeroMassGrams = Disc.Aero.MassKg * 1000.0f;
        if (!FMath::IsFinite(AeroMassGrams)
            || !FMath::IsNearlyEqual(AeroMassGrams, Disc.DiscMassGrams, 0.01f))
        {
            return Reject(TEXT("Resolved disc mass metadata must match Aero.MassKg"));
        }
        if (!FMath::IsFinite(Disc.DiscWear01)
            || Disc.DiscWear01 < 0.0f
            || Disc.DiscWear01 > 1.0f)
        {
            return Reject(TEXT("Resolved disc wear metadata must be finite and in [0, 1]"));
        }
        if (!bColorFinite) return Reject(TEXT("Resolved disc color channels must be finite"));
        if (Disc.DiscStampId != FName(TEXT("dg_generic_default")))
            return Reject(TEXT("Resolved disc requires the generic development stamp"));
        if (Disc.DiscNickname.Len() > ResolvedDiscMaximumNicknameCharacters)
            return Reject(TEXT("Resolved disc nickname exceeds 64 characters"));
        if (Disc.bWearAffectsPhysics)
            return Reject(TEXT("Resolved disc wear physics is not calibrated"));

        FString AeroError;
        if (!IsDiscAeroProfileValid(Disc.Aero, &AeroError)) return Reject(AeroError);
        if (OutError) OutError->Reset();
        return true;
    }

    /** Fail-closed contract for commands entering the authoritative gameplay boundary. */
    inline bool IsThrowCommandValid(const FThrowCommand& Command)
    {
        const bool bDirectionFinite = FMath::IsFinite(Command.Direction.X)
            && FMath::IsFinite(Command.Direction.Y)
            && FMath::IsFinite(Command.Direction.Z)
            && FMath::IsFinite(Command.Direction.SizeSquared());
        const bool bScalarsFinite = FMath::IsFinite(Command.Power01)
            && FMath::IsFinite(Command.HyzerDeg)
            && FMath::IsFinite(Command.NoseAngleDeg)
            && FMath::IsFinite(Command.LaunchAngleDeg)
            && FMath::IsFinite(Command.TimingError);
        const FVector HorizontalDirection(
            Command.Direction.X, Command.Direction.Y, 0.0f);
        if (!bDirectionFinite
            || Command.Direction.IsNearlyZero()
            || HorizontalDirection.IsNearlyZero()
            || !bScalarsFinite)
        {
            return false;
        }

        return !Command.MoldId.IsNone()
            && IsValidDiscPlastic(Command.Plastic)
            && IsValidThrowStyle(Command.ThrowStyle)
            && IsValidHandedness(Command.Handedness)
            && IsValidDiscShotContext(Command.ShotContext)
            && Command.Power01 >= ThrowCommandMinimumPower01
            && Command.Power01 <= ThrowCommandMaximumPower01
            && Command.HyzerDeg >= ThrowCommandMinimumHyzerDeg
            && Command.HyzerDeg <= ThrowCommandMaximumHyzerDeg
            && Command.NoseAngleDeg >= ThrowCommandMinimumNoseAngleDeg
            && Command.NoseAngleDeg <= ThrowCommandMaximumNoseAngleDeg
            && Command.LaunchAngleDeg >= ThrowCommandMinimumLaunchAngleDeg
            && Command.LaunchAngleDeg <= ThrowCommandMaximumLaunchAngleDeg
            && Command.TimingError >= ThrowCommandMinimumTimingError
            && Command.TimingError <= ThrowCommandMaximumTimingError;
    }

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

    /**
     * Fail-closed contract for immutable release snapshots entering the solver.
     * Besides finite/range checks, timing-derived phase fields must agree with
     * ResolveThrowRelease so telemetry cannot describe a different release than
     * the launch state being simulated.
     */
    inline bool IsThrowReleaseValid(const FThrowRelease& Release, FString* OutError = nullptr)
    {
        const auto Reject = [OutError](const FString& Error)
        {
            if (OutError)
            {
                *OutError = Error;
            }
            return false;
        };

        if (!IsValidReleaseGrade(Release.Grade))
        {
            return Reject(TEXT("Release.Grade enum is invalid"));
        }
        if (!IsValidReleaseTiming(Release.Timing))
        {
            return Reject(TEXT("Release.Timing enum is invalid"));
        }
        if (!IsValidThrowStyle(Release.ThrowStyle))
        {
            return Reject(TEXT("Release.ThrowStyle enum is invalid"));
        }
        if (!IsValidHandedness(Release.Handedness))
        {
            return Reject(TEXT("Release.Handedness enum is invalid"));
        }
        if (!IsValidDiscShotContext(Release.ShotContext))
        {
            return Reject(TEXT("Release.ShotContext enum is invalid"));
        }

        struct FNamedReleaseValue
        {
            const TCHAR* Name;
            float Value;
        };
        const FNamedReleaseValue ScalarFields[] = {
            { TEXT("TimingError"), Release.TimingError },
            { TEXT("Quality01"), Release.Quality01 },
            { TEXT("SpeedMultiplier"), Release.SpeedMultiplier },
            { TEXT("SpinMultiplier"), Release.SpinMultiplier },
            { TEXT("ReleaseSpeedMps"), Release.ReleaseSpeedMps },
            { TEXT("SpinRpm"), Release.SpinRpm },
            { TEXT("AimOffsetDeg"), Release.AimOffsetDeg },
            { TEXT("HyzerOffsetDeg"), Release.HyzerOffsetDeg },
            { TEXT("NoseOffsetDeg"), Release.NoseOffsetDeg },
            { TEXT("LaunchOffsetDeg"), Release.LaunchOffsetDeg },
            { TEXT("EffectiveHyzerDeg"), Release.EffectiveHyzerDeg },
            { TEXT("EffectiveNoseAngleDeg"), Release.EffectiveNoseAngleDeg },
            { TEXT("EffectiveLaunchAngleDeg"), Release.EffectiveLaunchAngleDeg },
            { TEXT("WindPhaseOriginSeconds"), Release.WindPhaseOriginSeconds },
            { TEXT("LiePowerMultiplier"), Release.LiePowerMultiplier },
            { TEXT("LieTimingErrorMultiplier"), Release.LieTimingErrorMultiplier }
        };
        for (const FNamedReleaseValue& Field : ScalarFields)
        {
            if (!FMath::IsFinite(Field.Value))
            {
                return Reject(FString::Printf(
                    TEXT("Release.%s must be finite"), Field.Name));
            }
        }

        if (!FMath::IsFinite(Release.Direction.X)
            || !FMath::IsFinite(Release.Direction.Y)
            || !FMath::IsFinite(Release.Direction.Z))
        {
            return Reject(TEXT("Release.Direction must be finite"));
        }
        if (Release.TimingError < ThrowCommandMinimumTimingError
            || Release.TimingError > ThrowCommandMaximumTimingError)
        {
            return Reject(TEXT("Release.TimingError must be in [-1, 1]"));
        }
        if (Release.Quality01 < 0.0f || Release.Quality01 > 1.0f)
        {
            return Reject(TEXT("Release.Quality01 must be in [0, 1]"));
        }
        if (Release.SpeedMultiplier <= 0.0f || Release.SpeedMultiplier > 1.0f)
        {
            return Reject(TEXT("Release.SpeedMultiplier must be in (0, 1]"));
        }
        if (Release.SpinMultiplier <= 0.0f || Release.SpinMultiplier > 1.0f)
        {
            return Reject(TEXT("Release.SpinMultiplier must be in (0, 1]"));
        }
        if (Release.ReleaseSpeedMps <= 0.0f
            || Release.ReleaseSpeedMps > ThrowReleaseMaximumSpeedMps)
        {
            return Reject(FString::Printf(
                TEXT("Release.ReleaseSpeedMps must be in (0, %.0f]"),
                ThrowReleaseMaximumSpeedMps));
        }
        if (Release.SpinRpm <= 0.0f || Release.SpinRpm > ThrowReleaseMaximumSpinRpm)
        {
            return Reject(FString::Printf(
                TEXT("Release.SpinRpm must be in (0, %.0f]"),
                ThrowReleaseMaximumSpinRpm));
        }
        if (FMath::Abs(Release.AimOffsetDeg) > ThrowReleaseMaximumAimOffsetDeg)
        {
            return Reject(TEXT("Release.AimOffsetDeg is outside [-6, 6]"));
        }
        if (FMath::Abs(Release.HyzerOffsetDeg) > ThrowReleaseMaximumHyzerOffsetDeg)
        {
            return Reject(TEXT("Release.HyzerOffsetDeg is outside [-4, 4]"));
        }
        if (FMath::Abs(Release.NoseOffsetDeg) > ThrowReleaseMaximumNoseOffsetDeg)
        {
            return Reject(TEXT("Release.NoseOffsetDeg is outside [-3, 3]"));
        }
        if (FMath::Abs(Release.LaunchOffsetDeg) > ThrowReleaseMaximumLaunchOffsetDeg)
        {
            return Reject(TEXT("Release.LaunchOffsetDeg is outside [-2, 2]"));
        }
        if (Release.EffectiveHyzerDeg < ThrowCommandMinimumHyzerDeg
            || Release.EffectiveHyzerDeg > ThrowCommandMaximumHyzerDeg)
        {
            return Reject(TEXT("Release.EffectiveHyzerDeg must be in [-34, 34]"));
        }
        if (Release.EffectiveNoseAngleDeg < ThrowCommandMinimumNoseAngleDeg
            || Release.EffectiveNoseAngleDeg > ThrowCommandMaximumNoseAngleDeg)
        {
            return Reject(TEXT("Release.EffectiveNoseAngleDeg must be in [-7, 11]"));
        }
        if (Release.EffectiveLaunchAngleDeg < ThrowCommandMinimumLaunchAngleDeg
            || Release.EffectiveLaunchAngleDeg > ThrowCommandMaximumLaunchAngleDeg)
        {
            return Reject(TEXT("Release.EffectiveLaunchAngleDeg must be in [-5, 35]"));
        }
        if (Release.WindPhaseOriginSeconds < 0.0f
            || Release.WindPhaseOriginSeconds >= ThrowReleaseMaximumWindPhaseOriginSeconds)
        {
            return Reject(TEXT("Release.WindPhaseOriginSeconds must be in [0, 4096)"));
        }
        if (Release.LiePowerMultiplier <= 0.0f || Release.LiePowerMultiplier > 1.0f)
        {
            return Reject(TEXT("Release.LiePowerMultiplier must be in (0, 1]"));
        }
        if (Release.LieTimingErrorMultiplier < 1.0f
            || Release.LieTimingErrorMultiplier > ThrowReleaseMaximumLieTimingErrorMultiplier)
        {
            return Reject(TEXT("Release.LieTimingErrorMultiplier must be in [1, 2]"));
        }

        const float HorizontalDirectionSizeSquared =
            Release.Direction.X * Release.Direction.X
            + Release.Direction.Y * Release.Direction.Y;
        if (!FMath::IsFinite(HorizontalDirectionSizeSquared))
        {
            return Reject(TEXT("Release.Direction horizontal magnitude must be finite"));
        }
        if (!FMath::IsNearlyEqual(HorizontalDirectionSizeSquared, 1.0f, 1.0e-3f))
        {
            return Reject(TEXT("Release.Direction must have unit horizontal magnitude"));
        }
        if (FMath::Abs(Release.Direction.Z) > KINDA_SMALL_NUMBER)
        {
            return Reject(TEXT("Release.Direction.Z must be zero"));
        }

        const EReleaseGrade ExpectedGrade = ReleaseGrade(Release.TimingError);
        const EReleaseTiming ExpectedTiming = ExpectedGrade == EReleaseGrade::Perfect
            ? EReleaseTiming::OnTime
            : (Release.TimingError < 0.0f ? EReleaseTiming::Early : EReleaseTiming::Late);
        const float Severity01 = FMath::Clamp(
            (FMath::Abs(Release.TimingError) - ReleasePerfectError)
                / (1.0f - ReleasePerfectError),
            0.0f, 1.0f);
        const float SignedSeverity = FMath::Sign(Release.TimingError) * Severity01;
        const bool bPutting = Release.ShotContext != EDiscShotContext::Drive;
        const float RotationSign = ThrowRotationSign(
            Release.ThrowStyle, Release.Handedness);
        const float ExpectedQuality01 = 1.0f - Severity01;
        const float ExpectedSpeedMultiplier =
            1.0f - (bPutting ? 0.10f : 0.16f) * FMath::Pow(Severity01, 1.25f);
        const float ExpectedSpinMultiplier =
            1.0f - (bPutting ? 0.15f : 0.22f) * FMath::Pow(Severity01, 1.15f);
        const float ExpectedAimOffsetDeg =
            RotationSign * SignedSeverity * (bPutting ? 2.8f : 6.0f);
        const float ExpectedHyzerOffsetDeg =
            SignedSeverity * (bPutting ? 1.5f : 4.0f);
        const float ExpectedNoseOffsetDeg =
            SignedSeverity * (bPutting ? 1.2f : 3.0f);
        const float ExpectedLaunchOffsetDeg =
            SignedSeverity * (bPutting ? 1.0f : 2.0f);
        constexpr float PhaseTolerance = 1.0e-4f;
        if (Release.Grade != ExpectedGrade)
        {
            return Reject(TEXT("Release.Grade does not match TimingError"));
        }
        if (Release.Timing != ExpectedTiming)
        {
            return Reject(TEXT("Release.Timing does not match TimingError"));
        }
        if (!FMath::IsNearlyEqual(Release.Quality01, ExpectedQuality01, PhaseTolerance))
        {
            return Reject(TEXT("Release.Quality01 does not match TimingError"));
        }
        if (!FMath::IsNearlyEqual(
            Release.SpeedMultiplier, ExpectedSpeedMultiplier, PhaseTolerance))
        {
            return Reject(TEXT("Release.SpeedMultiplier does not match TimingError/ShotContext"));
        }
        if (!FMath::IsNearlyEqual(
            Release.SpinMultiplier, ExpectedSpinMultiplier, PhaseTolerance))
        {
            return Reject(TEXT("Release.SpinMultiplier does not match TimingError/ShotContext"));
        }
        if (!FMath::IsNearlyEqual(
            Release.AimOffsetDeg, ExpectedAimOffsetDeg, PhaseTolerance))
        {
            return Reject(TEXT("Release.AimOffsetDeg does not match timing/style/handedness"));
        }
        if (!FMath::IsNearlyEqual(
            Release.HyzerOffsetDeg, ExpectedHyzerOffsetDeg, PhaseTolerance))
        {
            return Reject(TEXT("Release.HyzerOffsetDeg does not match TimingError/ShotContext"));
        }
        if (!FMath::IsNearlyEqual(
            Release.NoseOffsetDeg, ExpectedNoseOffsetDeg, PhaseTolerance))
        {
            return Reject(TEXT("Release.NoseOffsetDeg does not match TimingError/ShotContext"));
        }
        if (!FMath::IsNearlyEqual(
            Release.LaunchOffsetDeg, ExpectedLaunchOffsetDeg, PhaseTolerance))
        {
            return Reject(TEXT("Release.LaunchOffsetDeg does not match TimingError/ShotContext"));
        }

        if (OutError)
        {
            OutError->Reset();
        }
        return true;
    }

    /**
     * Rebase a horizontal release direction from the golfer aim-line origin to
     * an animated grip origin without changing the post-timing aim point.
     * The caller's output is unchanged on every rejected input.
     */
    inline bool TryRebaseReleaseDirectionToPreserveAimPoint(
        const FVector& AimLineOriginCm,
        const FVector& ReleaseOriginCm,
        float AimReferenceDistanceCm,
        const FVector& BaseDirection,
        float AimOffsetDeg,
        FVector& OutBaseDirection,
        FString& OutError)
    {
        const auto Reject = [&OutError](const TCHAR* Error)
        {
            OutError = Error;
            return false;
        };
        const auto IsFiniteVector = [](const FVector& Value)
        {
            return FMath::IsFinite(Value.X)
                && FMath::IsFinite(Value.Y)
                && FMath::IsFinite(Value.Z);
        };

        if (!IsFiniteVector(AimLineOriginCm))
        {
            return Reject(TEXT("Aim-line origin must be finite"));
        }
        if (!IsFiniteVector(ReleaseOriginCm))
        {
            return Reject(TEXT("Release origin must be finite"));
        }
        if (!FMath::IsFinite(AimReferenceDistanceCm)
            || AimReferenceDistanceCm <= SMALL_NUMBER)
        {
            return Reject(TEXT("Aim reference distance must be finite and positive"));
        }
        if (!IsFiniteVector(BaseDirection))
        {
            return Reject(TEXT("Base direction must be finite"));
        }
        if (!FMath::IsFinite(AimOffsetDeg)
            || FMath::Abs(AimOffsetDeg) > ThrowReleaseMaximumAimOffsetDeg)
        {
            return Reject(TEXT("Aim offset must be finite and inside the release contract"));
        }

        const FVector FlatBaseDirection(BaseDirection.X, BaseDirection.Y, 0.0f);
        const double BaseDirectionSizeSquared = FlatBaseDirection.SizeSquared();
        if (!FMath::IsFinite(BaseDirectionSizeSquared)
            || BaseDirectionSizeSquared <= SMALL_NUMBER)
        {
            return Reject(TEXT("Base direction must have a usable horizontal magnitude"));
        }
        const FVector NormalizedBaseDirection =
            FlatBaseDirection / FMath::Sqrt(BaseDirectionSizeSquared);
        if (!IsFiniteVector(NormalizedBaseDirection))
        {
            return Reject(TEXT("Normalized base direction must be finite"));
        }

        // DiscFlightComponent applies this same world-up rotation after it
        // normalizes Release.Direction, so preserve the target after that phase.
        const FQuat AimError(
            FVector::UpVector, FMath::DegreesToRadians(AimOffsetDeg));
        const FVector RotatedBaseDirection =
            AimError.RotateVector(NormalizedBaseDirection);
        const FVector OriginalPostTimingDirection(
            RotatedBaseDirection.X, RotatedBaseDirection.Y, 0.0f);
        const double OriginalPostTimingSizeSquared =
            OriginalPostTimingDirection.SizeSquared();
        if (!IsFiniteVector(OriginalPostTimingDirection)
            || !FMath::IsFinite(OriginalPostTimingSizeSquared)
            || OriginalPostTimingSizeSquared <= SMALL_NUMBER)
        {
            return Reject(TEXT("Post-timing direction could not be resolved"));
        }
        const FVector NormalizedPostTimingDirection =
            OriginalPostTimingDirection
            / FMath::Sqrt(OriginalPostTimingSizeSquared);
        const FVector AimPointCm = AimLineOriginCm
            + NormalizedPostTimingDirection * AimReferenceDistanceCm;
        if (!IsFiniteVector(AimPointCm))
        {
            return Reject(TEXT("Aim point must remain finite"));
        }

        const FVector ReleaseToAimPoint(
            AimPointCm.X - ReleaseOriginCm.X,
            AimPointCm.Y - ReleaseOriginCm.Y,
            0.0f);
        const double ReleaseToAimPointSizeSquared =
            ReleaseToAimPoint.SizeSquared();
        if (!IsFiniteVector(ReleaseToAimPoint)
            || !FMath::IsFinite(ReleaseToAimPointSizeSquared)
            || ReleaseToAimPointSizeSquared <= SMALL_NUMBER)
        {
            return Reject(TEXT("Release origin must have a usable line to the aim point"));
        }
        const FVector DesiredPostTimingDirection =
            ReleaseToAimPoint / FMath::Sqrt(ReleaseToAimPointSizeSquared);
        const FVector RotatedCandidateDirection =
            AimError.Inverse().RotateVector(DesiredPostTimingDirection);
        const FVector FlatCandidateDirection(
            RotatedCandidateDirection.X, RotatedCandidateDirection.Y, 0.0f);
        const double CandidateDirectionSizeSquared =
            FlatCandidateDirection.SizeSquared();
        if (!IsFiniteVector(FlatCandidateDirection)
            || !FMath::IsFinite(CandidateDirectionSizeSquared)
            || CandidateDirectionSizeSquared <= SMALL_NUMBER)
        {
            return Reject(TEXT("Rebased base direction could not be resolved"));
        }

        const FVector CandidateBaseDirection =
            FlatCandidateDirection / FMath::Sqrt(CandidateDirectionSizeSquared);
        if (!IsFiniteVector(CandidateBaseDirection))
        {
            return Reject(TEXT("Rebased base direction must remain finite"));
        }

        OutBaseDirection = CandidateBaseDirection;
        OutError.Reset();
        return true;
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

    inline float EffectiveFlightTimeoutSeconds(
        EDiscShotContext ShotContext,
        int32 DiscSpeed,
        float ConfiguredMaximumSeconds)
    {
        float TimeoutSeconds = FMath::Max(ConfiguredMaximumSeconds, 1.0f);
        if (ShotContext != EDiscShotContext::Drive)
        {
            TimeoutSeconds = FMath::Min(TimeoutSeconds, 8.0f);
        }
        if (DiscSpeed <= 3)
        {
            // A low-speed disc selected for a full-power fairway shot can
            // otherwise remain aloft until the global watchdog. Keep the
            // unusual player choice valid while bounding camera lockout.
            TimeoutSeconds = FMath::Min(TimeoutSeconds, 12.0f);
        }
        return TimeoutSeconds;
    }

    inline bool IsFlightStateWithinSafetyEnvelope(
        const FVector& WorldLocationCm,
        const FVector& VelocityMps,
        const FVector& LaunchWorldLocationCm,
        float MaximumDropBelowLaunchCm = 1000.0f)
    {
        const bool bFinite = FMath::IsFinite(WorldLocationCm.X)
            && FMath::IsFinite(WorldLocationCm.Y)
            && FMath::IsFinite(WorldLocationCm.Z)
            && FMath::IsFinite(VelocityMps.X)
            && FMath::IsFinite(VelocityMps.Y)
            && FMath::IsFinite(VelocityMps.Z)
            && FMath::IsFinite(LaunchWorldLocationCm.Z);
        return bFinite && WorldLocationCm.Z >= LaunchWorldLocationCm.Z
            - FMath::Max(MaximumDropBelowLaunchCm, 0.0f);
    }

    inline float RecommendedPuttPower01(float DistanceMeters)
    {
        return FMath::Clamp(0.34f + FMath::Max(DistanceMeters, 0.0f) * 0.029f, 0.38f, 0.94f);
    }

    inline float RecommendedPuttLaunchAngleDeg(
        EDiscShotContext ShotContext,
        float DistanceMeters)
    {
        if (ShotContext == EDiscShotContext::Circle2Putt)
        {
            return 12.0f;
        }
        if (ShotContext != EDiscShotContext::Circle1Putt)
        {
            return 7.0f;
        }

        // Tap-ins need a flatter release to enter the chain window instead of
        // clipping the top band. Restore the established 14-degree shape only
        // after the lie is far enough away to need the additional height.
        const float Blend01 = FMath::Clamp(
            (FMath::Max(DistanceMeters, 0.0f) - 2.0f) / 5.0f,
            0.0f,
            1.0f);
        return FMath::Lerp(10.0f, 14.0f, Blend01);
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
        const auto IsFiniteVector = [](const FVector& Value)
        {
            return FMath::IsFinite(Value.X)
                && FMath::IsFinite(Value.Y)
                && FMath::IsFinite(Value.Z);
        };
        const auto InvalidEvaluation = []()
        {
            // Basket contact is optional. A malformed sample must degrade to no
            // event instead of leaking partial telemetry or awarding a catch.
            return FBasketContactEvaluation();
        };
        if (!IsFiniteVector(RelativeLocationCm) || !IsFiniteVector(VelocityMps))
        {
            return InvalidEvaluation();
        }

        FBasketContactEvaluation Evaluation;
        Evaluation.IncomingSpeedMps = VelocityMps.Size();
        if (!FMath::IsFinite(Evaluation.IncomingSpeedMps))
        {
            return InvalidEvaluation();
        }
        if (Evaluation.IncomingSpeedMps < 0.25f) return Evaluation;

        const FVector HorizontalPosition(RelativeLocationCm.X, RelativeLocationCm.Y, 0.0f);
        const FVector HorizontalVelocityCm(VelocityMps.X * 100.0f, VelocityMps.Y * 100.0f, 0.0f);
        if (!IsFiniteVector(HorizontalVelocityCm))
        {
            return InvalidEvaluation();
        }
        const float HorizontalSpeedSq = HorizontalVelocityCm.SizeSquared2D();
        if (!FMath::IsFinite(HorizontalSpeedSq))
        {
            return InvalidEvaluation();
        }
        if (HorizontalSpeedSq < 1.0f) return Evaluation;

        const float ClosestApproachDot = static_cast<float>(FVector::DotProduct(
            HorizontalPosition, HorizontalVelocityCm));
        if (!FMath::IsFinite(ClosestApproachDot))
        {
            return InvalidEvaluation();
        }
        const float ClosestTime = FMath::Clamp(
            -ClosestApproachDot / HorizontalSpeedSq,
            0.0f, 0.18f);
        if (!FMath::IsFinite(ClosestTime))
        {
            return InvalidEvaluation();
        }
        const FVector Predicted = RelativeLocationCm + VelocityMps * (ClosestTime * 100.0f)
            + FVector(0.0f, 0.0f, -0.5f * 980.665f * ClosestTime * ClosestTime);
        if (!IsFiniteVector(Predicted))
        {
            return InvalidEvaluation();
        }
        Evaluation.PredictedRadialCm = FVector(Predicted.X, Predicted.Y, 0.0f).Size();
        Evaluation.PredictedHeightCm = Predicted.Z;
        if (!FMath::IsFinite(Evaluation.PredictedRadialCm)
            || !FMath::IsFinite(Evaluation.PredictedHeightCm))
        {
            return InvalidEvaluation();
        }

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
            if (!IsFiniteVector(Evaluation.DeflectedVelocityMps))
            {
                return InvalidEvaluation();
            }
            return Evaluation;
        }
        if (Evaluation.PredictedHeightCm <= 88.0f)
        {
            Evaluation.Result = EBasketContactResult::TrayRejection;
            Evaluation.DeflectedVelocityMps = FVector(
                VelocityMps.X * 0.42f,
                VelocityMps.Y * 0.42f,
                FMath::Max(FMath::Abs(VelocityMps.Z) * 0.30f, 1.35f));
            if (!IsFiniteVector(Evaluation.DeflectedVelocityMps))
            {
                return InvalidEvaluation();
            }
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
        if (!IsFiniteVector(Evaluation.DeflectedVelocityMps))
        {
            return InvalidEvaluation();
        }
        return Evaluation;
    }

    inline FThrowRelease ResolveThrowRelease(const FThrowCommand& Command)
    {
        FThrowRelease Release;
        Release.TimingError = FMath::Clamp(
            Command.TimingError, ThrowCommandMinimumTimingError, ThrowCommandMaximumTimingError);
        Release.Grade = ReleaseGrade(Release.TimingError);
        Release.Timing = Release.Grade == EReleaseGrade::Perfect
            ? EReleaseTiming::OnTime
            : (Release.TimingError < 0.0f ? EReleaseTiming::Early : EReleaseTiming::Late);

        const float AbsError = FMath::Abs(Release.TimingError);
        const float Severity01 = FMath::Clamp(
            (AbsError - ReleasePerfectError) / (1.0f - ReleasePerfectError), 0.0f, 1.0f);
        const float SignedSeverity = FMath::Sign(Release.TimingError) * Severity01;
        const float RotationSign = ThrowRotationSign(Command.ThrowStyle, Command.Handedness);

        Release.Quality01 = 1.0f - Severity01;
        const bool bPutting = Command.ShotContext != EDiscShotContext::Drive;
        Release.SpeedMultiplier = 1.0f - (bPutting ? 0.10f : 0.16f) * FMath::Pow(Severity01, 1.25f);
        Release.SpinMultiplier = 1.0f - (bPutting ? 0.15f : 0.22f) * FMath::Pow(Severity01, 1.15f);
        Release.AimOffsetDeg = RotationSign * SignedSeverity * (bPutting ? 2.8f : 6.0f);
        Release.HyzerOffsetDeg = SignedSeverity * (bPutting ? 1.5f : 4.0f);
        Release.NoseOffsetDeg = SignedSeverity * (bPutting ? 1.2f : 3.0f);
        Release.LaunchOffsetDeg = SignedSeverity * (bPutting ? 1.0f : 2.0f);

        const float Power01 = FMath::Clamp(
            Command.Power01, ThrowCommandMinimumPower01, ThrowCommandMaximumPower01);
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

        Release.EffectiveHyzerDeg = FMath::Clamp(Command.HyzerDeg + Release.HyzerOffsetDeg,
            ThrowCommandMinimumHyzerDeg, ThrowCommandMaximumHyzerDeg);
        Release.EffectiveNoseAngleDeg = FMath::Clamp(Command.NoseAngleDeg + Release.NoseOffsetDeg,
            ThrowCommandMinimumNoseAngleDeg, ThrowCommandMaximumNoseAngleDeg);
        Release.EffectiveLaunchAngleDeg = FMath::Clamp(Command.LaunchAngleDeg + Release.LaunchOffsetDeg,
            ThrowCommandMinimumLaunchAngleDeg, ThrowCommandMaximumLaunchAngleDeg);
        Release.ThrowStyle = Command.ThrowStyle;
        Release.Handedness = Command.Handedness;
        Release.ShotContext = Command.ShotContext;
        // Direction is the horizontal aim authority. Launch elevation lives
        // exclusively in EffectiveLaunchAngleDeg, so preserve in the immutable
        // release exactly the direction that DiscFlight will simulate.
        Release.Direction = FVector(
            Command.Direction.X, Command.Direction.Y, 0.0f).GetSafeNormal();
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
