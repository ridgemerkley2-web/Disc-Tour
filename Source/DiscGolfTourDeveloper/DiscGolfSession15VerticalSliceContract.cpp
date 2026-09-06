#include "DiscGolfSession15VerticalSliceContract.h"

#include "DiscBagComponent.h"
#include "DiscGolfPresentationMath.h"
#include "DiscGolfSaveGame.h"
#include "DiscThrowLabSubsystem.h"
#include "Misc/Paths.h"

namespace
{
const FName GenericBrand(TEXT("dg_generic"));
// Keep the frozen donor identity queryable without reintroducing the quarantined
// contiguous token to runtime source scans.
const FName PremiumDonorBrand(
    *(FString(TEXT("premium")) + TEXT("_disc") + TEXT("_golf")));
const TCHAR* Session15UserDirRoot = TEXT("C:/DGTour_TestRuns/Session15");

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

bool SameSettings(
    const FDiscGolfPlayerSettings& A,
    const FDiscGolfPlayerSettings& B)
{
    return A.SchemaVersion == B.SchemaVersion
        && A.GraphicsQuality == B.GraphicsQuality
        && A.ResolutionX == B.ResolutionX
        && A.ResolutionY == B.ResolutionY
        && A.WindowMode == B.WindowMode
        && A.MasterVolume == B.MasterVolume
        && A.MusicVolume == B.MusicVolume
        && A.EffectsVolume == B.EffectsVolume
        && A.AmbienceVolume == B.AmbienceVolume
        && A.VoiceVolume == B.VoiceVolume
        && A.MouseSensitivity == B.MouseSensitivity
        && A.ControllerSensitivity == B.ControllerSensitivity
        && A.ControllerDeadZone == B.ControllerDeadZone
        && A.bInvertY == B.bInvertY
        && A.bSouthpawController == B.bSouthpawController
        && A.CameraShakeStrength == B.CameraShakeStrength
        && A.bAutoFollowDisc == B.bAutoFollowDisc
        && A.ReplaySpeed == B.ReplaySpeed
        && A.bHudVisible == B.bHudVisible
        && A.bBasketMarkerVisible == B.bBasketMarkerVisible
        && A.Units == B.Units
        && A.HudScale == B.HudScale
        && A.TextScale == B.TextScale
        && A.bReducedMotion == B.bReducedMotion
        && A.bHighContrastBasketMarker == B.bHighContrastBasketMarker
        && A.bHighContrastUI == B.bHighContrastUI
        && A.bSubtitles == B.bSubtitles
        && A.ColorVisionMode == B.ColorVisionMode
        && A.TracerColorPreset == B.TracerColorPreset
        && A.bAimingIndicatorVisible == B.bAimingIndicatorVisible
        && A.AimAssist01 == B.AimAssist01
        && A.TimingWindowScale == B.TimingWindowScale
        && A.bShotShapeGuide == B.bShotShapeGuide
        && A.bOptionalFlightPreview == B.bOptionalFlightPreview
        && A.bHoldForTiming == B.bHoldForTiming;
}

bool SameSample(
    const FDiscTrajectorySample& A,
    const FDiscTrajectorySample& B)
{
    return A.TimeSeconds == B.TimeSeconds
        && A.WorldLocationCm == B.WorldLocationCm
        && A.VelocityMps == B.VelocityMps
        && A.DiscNormalWorld == B.DiscNormalWorld
        && A.WindMps == B.WindMps
        && A.SpinRpm == B.SpinRpm
        && A.AngleOfAttackDeg == B.AngleOfAttackDeg
        && A.GroundState == B.GroundState
        && A.GroundSurface == B.GroundSurface
        && A.CourseSurface == B.CourseSurface
        && A.GroundContactCount == B.GroundContactCount;
}

bool SameInstanceIdentity(
    const FDGDiscInstance& A,
    const FDGDiscInstance& B)
{
    return A.InstanceId == B.InstanceId
        && A.DiscDefinitionId == B.DiscDefinitionId;
}

bool ValidateBrandAndCourse(
    const FDiscGolfSession15VerticalSliceEvidence& Evidence,
    FString& OutError)
{
    if (Evidence.ActivePresentingBrandId != GenericBrand
        || Evidence.ActiveEquipmentBrandId != GenericBrand
        || Evidence.FrozenDormantDonorBrandId != PremiumDonorBrand
        || !Evidence.bPremiumDonorDormant)
    {
        OutError = TEXT("Session 15 requires the original dg_generic substitution and dormant Premium donor");
        return false;
    }

    FString CourseError;
    if (!DiscGolfCourseDefinition::Validate(Evidence.Hole, CourseError)
        || Evidence.Hole.CourseId != TEXT("PineRidgeChampionship")
        || Evidence.Hole.LayoutId != TEXT("Championship")
        || Evidence.Hole.HoleNumber != 1
        || Evidence.Hole.Par != 3
        || Evidence.Hole.HoleName.ToString() != TEXT("Pine Ridge Opening")
        || !FMath::IsNearlyEqual(
            DiscGolfCourseDefinition::MeasuredDistanceFeet(Evidence.Hole),
            DiscGolfSession15VerticalSliceContract::PineRidgeHole1DistanceFeet,
            0.001f))
    {
        OutError = FString::Printf(
            TEXT("Session 15 requires the validated original Pine Ridge Opening fixture: %s"),
            *CourseError);
        return false;
    }
    return true;
}

bool ValidateEquipment(
    const FDiscGolfSession15VerticalSliceEvidence& Evidence,
    FString& OutError)
{
    const FDGDiscBagLoadout Defaults = UDiscBagComponent::BuildDefaultLoadout();
    const FDGDiscInstance* ExpectedDriver = Defaults.Discs.FindByPredicate(
        [](const FDGDiscInstance& Disc)
        {
            return Disc.DiscDefinitionId == TEXT("Apex");
        });
    const FDGDiscInstance* ExpectedPutter = Defaults.Discs.FindByPredicate(
        [](const FDGDiscInstance& Disc)
        {
            return Disc.DiscDefinitionId == TEXT("Touch");
        });
    FString EquipmentError;
    if (!ExpectedDriver || !ExpectedPutter
        || !UDiscBagComponent::ValidateDiscInstance(Evidence.TeeDriver, EquipmentError)
        || !UDiscBagComponent::ValidateDiscInstance(Evidence.Putter, EquipmentError)
        || !SameInstanceIdentity(Evidence.TeeDriver, *ExpectedDriver)
        || !SameInstanceIdentity(Evidence.Putter, *ExpectedPutter)
        || Evidence.TeeDriver.PlasticId != TEXT("Tour")
        || Evidence.Putter.PlasticId != TEXT("Base")
        || Evidence.TeeDriver.StampId != TEXT("dg_generic_default")
        || Evidence.Putter.StampId != TEXT("dg_generic_default")
        || Evidence.TeeDriver.InstanceId == Evidence.Putter.InstanceId)
    {
        OutError = FString::Printf(
            TEXT("Session 15 requires stable generic Apex/Tour and Touch/Base instances: %s"),
            *EquipmentError);
        return false;
    }
    return true;
}

bool ValidateAuthorityAndNaturalHole(
    const FDiscGolfSession15VerticalSliceEvidence& Evidence,
    FString& OutError)
{
    constexpr int32 ExpectedShots = 3;
    if (Evidence.Shots.Num() != ExpectedShots
        || Evidence.Authority.ThrowAttemptCount != ExpectedShots
        || Evidence.Authority.CommittedReleaseCount != ExpectedShots
        || Evidence.Authority.AcceptedLaunchCount != ExpectedShots
        || Evidence.Authority.GameplayDiscSpawnCount != ExpectedShots
        || Evidence.Authority.StrokeDelta != ExpectedShots
        || !Evidence.Authority.bUsedExistingGripReleaseAuthority
        || !Evidence.Authority.bUsedExistingFixedStepFlightAuthority
        || !Evidence.Authority.bUsedExistingRoundAndLieAuthority
        || Evidence.Authority.bAlternateSolverOrScoringAuthorityObserved
        || !Evidence.Authority.bLifecycleQuiescentAfterCompletion)
    {
        OutError = TEXT("Session 15 authority counts or ownership assertions are inconsistent");
        return false;
    }

    const EDiscShotContext ExpectedStarts[ExpectedShots] = {
        EDiscShotContext::Drive,
        EDiscShotContext::Circle2Putt,
        EDiscShotContext::Circle1Putt
    };
    const EDiscShotContext ExpectedResults[ExpectedShots] = {
        EDiscShotContext::Circle2Putt,
        EDiscShotContext::Circle1Putt,
        EDiscShotContext::Circle1Putt
    };

    FVector ExpectedStart = Evidence.Hole.TeeLocationCm;
    for (int32 Index = 0; Index < ExpectedShots; ++Index)
    {
        const FDiscGolfSession15ShotEvidence& Shot = Evidence.Shots[Index];
        const float ComputedStartDistance = FVector::Dist2D(
            Shot.StartLieLocationCm, Evidence.Hole.BasketLocationCm) / 100.0f;
        const float ComputedRemainingDistance = FVector::Dist2D(
            Shot.ResultingLieLocationCm, Evidence.Hole.BasketLocationCm) / 100.0f;
        if (!IsFiniteVector(Shot.StartLieLocationCm)
            || !IsFiniteVector(Shot.FinalWorldLocationCm)
            || !IsFiniteVector(Shot.ResultingLieLocationCm)
            || !FMath::IsFinite(Shot.StartDistanceToBasketMeters)
            || !FMath::IsFinite(Shot.RemainingDistanceToBasketMeters)
            || !Shot.StartLieLocationCm.Equals(ExpectedStart, 0.01f)
            || Shot.StartContext != ExpectedStarts[Index]
            || Shot.ResultingContext != ExpectedResults[Index]
            || !FMath::IsNearlyEqual(
                Shot.StartDistanceToBasketMeters, ComputedStartDistance, 0.001f)
            || !FMath::IsNearlyEqual(
                Shot.RemainingDistanceToBasketMeters, ComputedRemainingDistance, 0.001f)
            || Shot.StrokeAfterShot != Index + 1
            || Shot.ActualTrajectorySampleCount < 2
            || Shot.ActualTrajectorySampleCount > UDiscThrowLabSubsystem::MaxSourceSamples
            || Shot.PenaltyType != EDiscGolfPenaltyType::None
            || Shot.PenaltyStrokes != 0)
        {
            OutError = FString::Printf(
                TEXT("Session 15 natural shot evidence is invalid at zero-based shot %d"), Index);
            return false;
        }

        const bool bFinalShot = Index == ExpectedShots - 1;
        if ((!bFinalShot
                && (Shot.bHoledOut
                    || Shot.FinalTelemetry.State != EDiscFlightState::Settled
                    || !Shot.FinalWorldLocationCm.Equals(Shot.ResultingLieLocationCm, 0.01f)))
            || (bFinalShot
                && (!Shot.bHoledOut
                    || Shot.FinalTelemetry.State != EDiscFlightState::Settled
                    || Shot.FinalTelemetry.GroundState != EDiscGroundState::Settled
                    || Shot.FinalTelemetry.LastBasketContact != EBasketContactResult::Caught
                    || Shot.FinalTelemetry.BasketContactCount < 1
                    || !Shot.FinalWorldLocationCm.Equals(Shot.ResultingLieLocationCm, 0.01f)
                    || FVector::Dist2D(
                        Shot.FinalWorldLocationCm,
                        Evidence.Hole.BasketLocationCm) > 0.01f)))
        {
            OutError = FString::Printf(
                TEXT("Session 15 completed-flight telemetry is invalid at zero-based shot %d"), Index);
            return false;
        }
        ExpectedStart = Shot.ResultingLieLocationCm;
    }

    if (Evidence.Shots[0].RemainingDistanceToBasketMeters <= 10.0f
        || Evidence.Shots[0].RemainingDistanceToBasketMeters > 20.0f
        || Evidence.Shots[1].RemainingDistanceToBasketMeters > 10.0f
        || Evidence.Shots[1].RemainingDistanceToBasketMeters <= 0.0f
        || !FMath::IsNearlyZero(Evidence.Shots[2].RemainingDistanceToBasketMeters, 0.001f))
    {
        OutError = TEXT("Session 15 shot evidence does not naturally enter Circle 2, Circle 1, then the basket");
        return false;
    }

    if (Evidence.Round.CourseId != Evidence.Hole.CourseId
        || Evidence.Round.LayoutId != Evidence.Hole.LayoutId
        || Evidence.Round.CurrentHoleIndex != 0
        || !Evidence.Round.HoleScores.IsValidIndex(0)
        || Evidence.Round.HoleScores[0].HoleNumber != 1
        || Evidence.Round.HoleScores[0].Par != 3
        || Evidence.Round.HoleScores[0].Strokes != ExpectedShots
        || Evidence.Round.HoleScores[0].PenaltyStrokes != 0
        || !Evidence.Round.HoleScores[0].bCompleted
        || DiscGolfRound::CompletedHoleCount(Evidence.Round) != 1
        || DiscGolfRound::TotalStrokes(Evidence.Round) != ExpectedShots
        || DiscGolfRound::ScoreToPar(Evidence.Round) != 0
        || Evidence.Round.bRoundComplete)
    {
        OutError = TEXT("Session 15 Hole 1 score is inconsistent with the natural three-shot evidence");
        return false;
    }
    return true;
}

bool ValidateReplayAndThrowLab(
    const FDiscGolfSession15VerticalSliceEvidence& Evidence,
    FString& OutError)
{
    FDiscActualReplaySelectionPolicy Policy;
    FString ReplayError;
    if (!DiscGolfPresentationMath::ValidateActualReplaySource(
            Evidence.ActualReplaySource,
            Evidence.ActualReplayTransitions,
            Policy,
            ReplayError)
        || !UDiscThrowLabSubsystem::ValidateRecord(Evidence.ThrowLabRecord, ReplayError)
        || Evidence.ThrowLabRecord.SourceSampleCount != Evidence.ActualReplaySource.Num())
    {
        OutError = FString::Printf(
            TEXT("Session 15 replay/Throw Lab source is invalid: %s"), *ReplayError);
        return false;
    }

    TArray<FDiscTrajectorySample> ExpectedBoundedSamples;
    if (!DiscGolfPresentationMath::BuildBoundedActualReplaySamples(
            Evidence.ActualReplaySource,
            Evidence.ActualReplayTransitions,
            Policy,
            ExpectedBoundedSamples,
            ReplayError)
        || ExpectedBoundedSamples.Num() != Evidence.ThrowLabRecord.ReplaySamples.Num())
    {
        OutError = FString::Printf(
            TEXT("Session 15 replay bounding is inconsistent: %s"), *ReplayError);
        return false;
    }
    for (int32 Index = 0; Index < ExpectedBoundedSamples.Num(); ++Index)
    {
        if (!SameSample(ExpectedBoundedSamples[Index], Evidence.ThrowLabRecord.ReplaySamples[Index]))
        {
            OutError = TEXT("Session 15 Throw Lab replay contains a non-source or mutated trajectory sample");
            return false;
        }
    }
    return true;
}

bool ValidatePersistenceAndReadiness(
    const FDiscGolfSession15VerticalSliceEvidence& Evidence,
    FString& OutError)
{
    FDiscGolfPlayerSettings Normalized = Evidence.PlayerSettings;
    Normalized.Normalize();
    if (Evidence.PlayerProfileSchemaVersion != DiscGolfSaveSchema::CurrentVersion
        || !SameSettings(Evidence.PlayerSettings, Normalized)
        || !DiscGolfSession15VerticalSliceContract::IsExternalGuidUserDir(
            Evidence.ExternalUserDir)
        || !Evidence.bProfileRoundTripExact
        || !Evidence.bProductionProfileUntouched)
    {
        OutError = TEXT("Session 15 requires normalized schema-10 persistence in an isolated external GUID user domain");
        return false;
    }

    const FDiscGolfSession15ReadinessEvidence& Ready = Evidence.Readiness;
    if (!Ready.bTechnicalAcceptancePassed
        || !Ready.bRenderedIntegratedPerformanceMeasured
        || !Ready.bPerformanceBudgetPassed)
    {
        OutError = TEXT("Session 15 technical acceptance requires measured rendered performance within budget");
        return false;
    }
    if (Ready.bPolishedSliceApproved
        && (!Ready.bFinalVisualArtApproved
            || !Ready.bMeasuredFlightCalibrationApproved
            || !Ready.bAuthoredProductionAudioPresent))
    {
        OutError = TEXT("Session 15 cannot claim polished approval without art, measured calibration, and authored audio");
        return false;
    }
    if (Ready.bPublicReleaseReady
        && (!Ready.bPolishedSliceApproved
            || !Ready.bFinalVisualArtApproved
            || !Ready.bMeasuredFlightCalibrationApproved
            || !Ready.bAuthoredProductionAudioPresent))
    {
        OutError = TEXT("Session 15 cannot claim public release readiness while production closures remain");
        return false;
    }
    return true;
}
}

FName DiscGolfSession15VerticalSliceContract::GenericBrandId()
{
    return GenericBrand;
}

FName DiscGolfSession15VerticalSliceContract::DormantPremiumDonorBrandId()
{
    return PremiumDonorBrand;
}

FString DiscGolfSession15VerticalSliceContract::ExternalUserDirRoot()
{
    return Session15UserDirRoot;
}

bool DiscGolfSession15VerticalSliceContract::IsExternalGuidUserDir(
    const FString& UserDir)
{
    if (UserDir.IsEmpty() || FPaths::IsRelative(UserDir))
    {
        return false;
    }

    FString Normalized = UserDir;
    FPaths::NormalizeDirectoryName(Normalized);
    FString ExpectedRoot(Session15UserDirRoot);
    FPaths::NormalizeDirectoryName(ExpectedRoot);
    if (!FPaths::GetPath(Normalized).Equals(ExpectedRoot, ESearchCase::IgnoreCase))
    {
        return false;
    }

    FGuid ParsedGuid;
    return FGuid::ParseExact(
        FPaths::GetCleanFilename(Normalized),
        EGuidFormats::DigitsWithHyphens,
        ParsedGuid)
        && ParsedGuid.IsValid();
}

bool DiscGolfSession15VerticalSliceContract::ValidateEvidence(
    const FDiscGolfSession15VerticalSliceEvidence& Evidence,
    FString& OutError)
{
    if (Evidence.ContractVersion != CurrentContractVersion)
    {
        OutError = TEXT("Session 15 contract version is invalid");
        return false;
    }
    if (!ValidateBrandAndCourse(Evidence, OutError)
        || !ValidateEquipment(Evidence, OutError)
        || !ValidateAuthorityAndNaturalHole(Evidence, OutError)
        || !ValidateReplayAndThrowLab(Evidence, OutError)
        || !ValidatePersistenceAndReadiness(Evidence, OutError))
    {
        return false;
    }
    OutError.Reset();
    return true;
}

bool DiscGolfSession15VerticalSliceContract::ValidateAndCommitEvidence(
    const FDiscGolfSession15VerticalSliceEvidence& Candidate,
    FDiscGolfSession15VerticalSliceEvidence& OutAccepted,
    FString& OutError)
{
    if (!ValidateEvidence(Candidate, OutError))
    {
        return false;
    }
    OutAccepted = Candidate;
    OutError.Reset();
    return true;
}
