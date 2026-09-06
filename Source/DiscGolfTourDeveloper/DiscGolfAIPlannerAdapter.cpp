#include "DiscGolfAIPlannerAdapter.h"

#include "DiscBagComponent.h"
#include "DiscGolfAIShotPlannerComponent.h"

namespace
{
constexpr int32 MaxMeasuredCandidateCount = 256;
constexpr float PositionAgreementToleranceCm = 1.0f;

bool IsFiniteVector(const FVector& Value)
{
    return FMath::IsFinite(Value.X)
        && FMath::IsFinite(Value.Y)
        && FMath::IsFinite(Value.Z);
}

bool IsProbability(const float Value)
{
    return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
}

bool IsValidProjectPlastic(const EDiscPlastic Plastic)
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

bool IsValidProjectThrowStyle(const EThrowStyle ThrowStyle)
{
    return ThrowStyle == EThrowStyle::Backhand
        || ThrowStyle == EThrowStyle::Forehand;
}

bool IsValidShotContext(const EDiscShotContext ShotContext)
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

bool IsValidGroundState(const EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Airborne:
        case EDiscGroundState::Impact:
        case EDiscGroundState::Skipping:
        case EDiscGroundState::Sliding:
        case EDiscGroundState::EdgeRolling:
        case EDiscGroundState::Settled:
            return true;
        default:
            return false;
    }
}

bool IsValidGroundSurface(const EGroundSurfaceType Surface)
{
    switch (Surface)
    {
        case EGroundSurfaceType::Fairway:
        case EGroundSurfaceType::Rough:
        case EGroundSurfaceType::Dirt:
        case EGroundSurfaceType::Rock:
        case EGroundSurfaceType::TeePad:
            return true;
        default:
            return false;
    }
}

bool IsValidCourseSurface(const ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::Fairway:
        case ECourseSurfaceType::TeePad:
        case ECourseSurfaceType::LightRough:
        case ECourseSurfaceType::DeepRough:
        case ECourseSurfaceType::Dirt:
        case ECourseSurfaceType::Rock:
        case ECourseSurfaceType::OutOfBounds:
        case ECourseSurfaceType::Hazard:
            return true;
        default:
            return false;
    }
}

bool IsValidFixtureType(const EDiscGolfFixtureType Type)
{
    switch (Type)
    {
        case EDiscGolfFixtureType::Unknown:
        case EDiscGolfFixtureType::Tree:
        case EDiscGolfFixtureType::DenseGrass:
        case EDiscGolfFixtureType::Rock:
        case EDiscGolfFixtureType::Sign:
            return true;
        default:
            return false;
    }
}

bool IsValidBasketResult(const EBasketContactResult Result)
{
    switch (Result)
    {
        case EBasketContactResult::None:
        case EBasketContactResult::Caught:
        case EBasketContactResult::ChainDeflection:
        case EBasketContactResult::BandRejection:
        case EBasketContactResult::TrayRejection:
            return true;
        default:
            return false;
    }
}

FName PlasticIdFromProjectEnum(const EDiscPlastic Plastic)
{
    switch (Plastic)
    {
        case EDiscPlastic::Base: return TEXT("Base");
        case EDiscPlastic::Tour: return TEXT("Tour");
        case EDiscPlastic::Crystal: return TEXT("Crystal");
        default: return NAME_None;
    }
}

bool TryProjectPlasticFromId(const FName PlasticId, EDiscPlastic& OutPlastic)
{
    if (PlasticId == TEXT("Base"))
    {
        OutPlastic = EDiscPlastic::Base;
        return true;
    }
    if (PlasticId == TEXT("Tour"))
    {
        OutPlastic = EDiscPlastic::Tour;
        return true;
    }
    if (PlasticId == TEXT("Crystal"))
    {
        OutPlastic = EDiscPlastic::Crystal;
        return true;
    }
    return false;
}

EDGThrowType PluginThrowTypeFromProjectEnum(const EThrowStyle ThrowStyle)
{
    return ThrowStyle == EThrowStyle::Forehand
        ? EDGThrowType::Forehand
        : EDGThrowType::Backhand;
}

bool TryProjectThrowStyleFromPluginEnum(
    const EDGThrowType ThrowType,
    EThrowStyle& OutThrowStyle)
{
    if (ThrowType == EDGThrowType::Backhand)
    {
        OutThrowStyle = EThrowStyle::Backhand;
        return true;
    }
    if (ThrowType == EDGThrowType::Forehand)
    {
        OutThrowStyle = EThrowStyle::Forehand;
        return true;
    }
    return false;
}

bool ValidateSkill(const float Value, const TCHAR* Label, FString& OutError)
{
    if (!IsProbability(Value))
    {
        OutError = FString::Printf(
            TEXT("AI skill %s must be finite and in [0,1]."), Label);
        return false;
    }
    return true;
}

bool ValidateCommand(
    const UDiscGolfAIGolferProfile& Profile,
    const FThrowCommand& Command,
    FString& OutError)
{
    if (!Command.DiscInstanceId.IsValid() || Command.MoldId.IsNone())
    {
        OutError = TEXT("Measured command requires stable disc instance and mold IDs.");
        return false;
    }
    if (!IsValidProjectPlastic(Command.Plastic)
        || !IsValidProjectThrowStyle(Command.ThrowStyle)
        || !IsValidShotContext(Command.ShotContext))
    {
        OutError = TEXT("Measured command contains an unsupported project enum.");
        return false;
    }
    if (!IsFiniteVector(Command.Direction)
        || Command.Direction.SizeSquared() <= SMALL_NUMBER)
    {
        OutError = TEXT("Measured command direction must be finite and non-zero.");
        return false;
    }
    if (!IsProbability(Command.Power01)
        || !FMath::IsFinite(Command.HyzerDeg)
        || Command.HyzerDeg < -34.0f || Command.HyzerDeg > 34.0f
        || !FMath::IsFinite(Command.NoseAngleDeg)
        || Command.NoseAngleDeg < -7.0f || Command.NoseAngleDeg > 11.0f
        || !FMath::IsFinite(Command.LaunchAngleDeg)
        || Command.LaunchAngleDeg < -5.0f || Command.LaunchAngleDeg > 35.0f
        || !FMath::IsFinite(Command.TimingError)
        || Command.TimingError < -1.0f || Command.TimingError > 1.0f)
    {
        OutError = TEXT("Measured command contains an invalid power, angle, or timing value.");
        return false;
    }

    const FName PlasticId = PlasticIdFromProjectEnum(Command.Plastic);
    const FDGDiscInstance* ProfileDisc = Profile.BagTemplate.FindByPredicate(
        [&Command](const FDGDiscInstance& Disc)
        {
            return Disc.InstanceId == Command.DiscInstanceId;
        });
    if (!ProfileDisc
        || ProfileDisc->DiscDefinitionId != Command.MoldId
        || ProfileDisc->PlasticId != PlasticId)
    {
        OutError = TEXT("Measured command disc identity is not an exact profile-bag member.");
        return false;
    }
    return true;
}

bool ValidateFinalTelemetry(
    const FDiscFlightTelemetry& Telemetry,
    FString& OutError)
{
    if (Telemetry.State != EDiscFlightState::Settled
        && Telemetry.State != EDiscFlightState::HoledOut)
    {
        OutError = TEXT("Measured outcome requires completed flight telemetry.");
        return false;
    }
    if (!IsFiniteVector(Telemetry.VelocityMps)
        || !IsFiniteVector(Telemetry.LastFixtureEntryVelocityMps)
        || !IsFiniteVector(Telemetry.LastFixtureExitVelocityMps)
        || !IsFiniteVector(Telemetry.LastFixtureImpactNormal))
    {
        OutError = TEXT("Measured telemetry vectors must be finite.");
        return false;
    }
    const float NonNegativeValues[] = {
        Telemetry.SpeedMps,
        Telemetry.SpinRpm,
        Telemetry.FlightTimeSeconds,
        Telemetry.CarryMeters,
        Telemetry.GroundPlayTimeSeconds,
        Telemetry.GroundDistanceMeters,
        Telemetry.LastImpactSpeedMps,
        Telemetry.LastFixtureImpactSpeedMps,
        Telemetry.LastFixtureEntrySpinRpm,
        Telemetry.LastFixtureExitSpinRpm,
        Telemetry.Release.ReleaseSpeedMps,
        Telemetry.Release.SpinRpm,
        Telemetry.Release.Quality01,
        Telemetry.Release.SpeedMultiplier,
        Telemetry.Release.SpinMultiplier,
        Telemetry.Release.LiePowerMultiplier,
        Telemetry.Release.LieTimingErrorMultiplier
    };
    for (const float Value : NonNegativeValues)
    {
        if (!FMath::IsFinite(Value) || Value < 0.0f)
        {
            OutError = TEXT("Measured telemetry metrics must be finite and non-negative.");
            return false;
        }
    }
    const float FiniteValues[] = {
        Telemetry.AngleOfAttackDeg,
        Telemetry.LastImpactIncidenceDeg,
        Telemetry.LastDiscEdgeAngleDeg,
        Telemetry.Release.TimingError,
        Telemetry.Release.AimOffsetDeg,
        Telemetry.Release.HyzerOffsetDeg,
        Telemetry.Release.NoseOffsetDeg,
        Telemetry.Release.LaunchOffsetDeg,
        Telemetry.Release.EffectiveHyzerDeg,
        Telemetry.Release.EffectiveNoseAngleDeg,
        Telemetry.Release.EffectiveLaunchAngleDeg
    };
    for (const float Value : FiniteValues)
    {
        if (!FMath::IsFinite(Value))
        {
            OutError = TEXT("Measured telemetry angles must be finite.");
            return false;
        }
    }
    if (Telemetry.GroundContactCount < 0
        || Telemetry.FixtureContactCount < 0
        || Telemetry.BasketContactCount < 0
        || !IsValidGroundState(Telemetry.GroundState)
        || !IsValidGroundSurface(Telemetry.GroundSurface)
        || !IsValidCourseSurface(Telemetry.CourseSurface)
        || !IsValidFixtureType(Telemetry.LastFixtureType)
        || !IsValidBasketResult(Telemetry.LastBasketContact)
        || !IsValidProjectThrowStyle(Telemetry.Release.ThrowStyle)
        || !IsValidShotContext(Telemetry.Release.ShotContext)
        || !IsFiniteVector(Telemetry.Release.Direction)
        || Telemetry.Release.Direction.SizeSquared() <= SMALL_NUMBER)
    {
        OutError = TEXT("Measured telemetry contains invalid counts, enums, or release direction.");
        return false;
    }
    return true;
}

bool DiscMatchesProfileMember(
    const UDiscGolfAIGolferProfile& Profile,
    const FDGDiscInstance& CandidateDisc)
{
    const FDGDiscInstance* ProfileDisc = Profile.BagTemplate.FindByPredicate(
        [&CandidateDisc](const FDGDiscInstance& Disc)
        {
            return Disc.InstanceId == CandidateDisc.InstanceId;
        });
    return ProfileDisc
        && ProfileDisc->DiscDefinitionId == CandidateDisc.DiscDefinitionId
        && ProfileDisc->PlasticId == CandidateDisc.PlasticId
        && ProfileDisc->StampId == CandidateDisc.StampId
        && FMath::IsNearlyEqual(ProfileDisc->MassGrams, CandidateDisc.MassGrams)
        && FMath::IsNearlyEqual(ProfileDisc->Wear01, CandidateDisc.Wear01);
}

int32 CompareGuid(const FGuid& Left, const FGuid& Right)
{
    if (Left.A != Right.A) return Left.A < Right.A ? -1 : 1;
    if (Left.B != Right.B) return Left.B < Right.B ? -1 : 1;
    if (Left.C != Right.C) return Left.C < Right.C ? -1 : 1;
    if (Left.D != Right.D) return Left.D < Right.D ? -1 : 1;
    return 0;
}

int32 CompareName(const FName Left, const FName Right)
{
    return Left.ToString().Compare(Right.ToString(), ESearchCase::CaseSensitive);
}

template <typename T>
int32 CompareScalar(const T Left, const T Right)
{
    if (Left < Right) return -1;
    if (Right < Left) return 1;
    return 0;
}

struct FPreparedMeasuredCandidate
{
    FDGAIShotCandidate Candidate;
    FThrowCommand SourceCommand;
    FDiscGolfAIMeasuredShotOutcome SourceOutcome;
};

bool StableCandidateLess(
    const FPreparedMeasuredCandidate& Left,
    const FPreparedMeasuredCandidate& Right)
{
    int32 Result = CompareGuid(
        Left.Candidate.Disc.InstanceId,
        Right.Candidate.Disc.InstanceId);
    if (Result != 0) return Result < 0;
    Result = CompareName(
        Left.Candidate.Disc.DiscDefinitionId,
        Right.Candidate.Disc.DiscDefinitionId);
    if (Result != 0) return Result < 0;
    Result = CompareName(
        Left.Candidate.Disc.PlasticId,
        Right.Candidate.Disc.PlasticId);
    if (Result != 0) return Result < 0;

#define DG_COMPARE_FIELD(Field) \
    Result = CompareScalar(Left.Field, Right.Field); \
    if (Result != 0) return Result < 0

    DG_COMPARE_FIELD(Candidate.ThrowType);
    DG_COMPARE_FIELD(SourceCommand.ShotContext);
    DG_COMPARE_FIELD(Candidate.Power01);
    DG_COMPARE_FIELD(Candidate.HyzerDegrees);
    DG_COMPARE_FIELD(Candidate.NoseDegrees);
    DG_COMPARE_FIELD(SourceCommand.LaunchAngleDeg);
    DG_COMPARE_FIELD(SourceCommand.TimingError);
    DG_COMPARE_FIELD(SourceCommand.Direction.X);
    DG_COMPARE_FIELD(SourceCommand.Direction.Y);
    DG_COMPARE_FIELD(SourceCommand.Direction.Z);
    DG_COMPARE_FIELD(Candidate.PredictedLandingLocationCm.X);
    DG_COMPARE_FIELD(Candidate.PredictedLandingLocationCm.Y);
    DG_COMPARE_FIELD(Candidate.PredictedLandingLocationCm.Z);
    DG_COMPARE_FIELD(Candidate.ExpectedProgressM);
    DG_COMPARE_FIELD(Candidate.SuccessProbability);
    DG_COMPARE_FIELD(Candidate.OutOfBoundsProbability);
    DG_COMPARE_FIELD(Candidate.ObstacleHitProbability);
    DG_COMPARE_FIELD(Candidate.LandingErrorM);
    DG_COMPARE_FIELD(SourceOutcome.FinalTelemetry.FlightTimeSeconds);
    DG_COMPARE_FIELD(SourceOutcome.FinalTelemetry.CarryMeters);
    DG_COMPARE_FIELD(SourceOutcome.FinalTelemetry.GroundContactCount);
    DG_COMPARE_FIELD(SourceOutcome.FinalTelemetry.FixtureContactCount);
    DG_COMPARE_FIELD(SourceOutcome.FinalTelemetry.BasketContactCount);

#undef DG_COMPARE_FIELD
    return false;
}

bool CandidateIdentityMatches(
    const FDGAIShotCandidate& Left,
    const FDGAIShotCandidate& Right)
{
    return Left.Disc.InstanceId == Right.Disc.InstanceId
        && Left.Disc.DiscDefinitionId == Right.Disc.DiscDefinitionId
        && Left.Disc.PlasticId == Right.Disc.PlasticId
        && Left.ThrowType == Right.ThrowType
        && Left.Power01 == Right.Power01
        && Left.HyzerDegrees == Right.HyzerDegrees
        && Left.NoseDegrees == Right.NoseDegrees
        && Left.PredictedLandingLocationCm == Right.PredictedLandingLocationCm
        && Left.ExpectedProgressM == Right.ExpectedProgressM
        && Left.SuccessProbability == Right.SuccessProbability
        && Left.OutOfBoundsProbability == Right.OutOfBoundsProbability
        && Left.ObstacleHitProbability == Right.ObstacleHitProbability
        && Left.LandingErrorM == Right.LandingErrorM;
}
}

bool DiscGolfAIPlannerAdapter::BuildGenericProofProfile(
    UObject* Outer,
    UDiscGolfAIGolferProfile*& OutProfile,
    FString& OutError)
{
    OutError.Reset();
    UDiscGolfAIGolferProfile* Candidate = NewObject<UDiscGolfAIGolferProfile>(
        Outer ? Outer : GetTransientPackage());
    if (!Candidate)
    {
        OutError = TEXT("Generic AI proof profile could not be allocated.");
        return false;
    }

    Candidate->GolferId = TEXT("dg_generic_ai_proof");
    Candidate->DisplayName = FText::FromString(TEXT("Generic AI Proof Golfer"));
    Candidate->Skill.Power = 0.74f;
    Candidate->Skill.Accuracy = 0.72f;
    Candidate->Skill.Putting = 0.68f;
    Candidate->Skill.Consistency = 0.71f;
    Candidate->Skill.Aggression = 0.48f;
    Candidate->Skill.CourseManagement = 0.76f;

    const FDGDiscBagLoadout DefaultLoadout = UDiscBagComponent::BuildDefaultLoadout();
    Candidate->BagTemplate = DefaultLoadout.Discs;
    if (!ValidateProfile(*Candidate, OutError))
    {
        return false;
    }

    OutProfile = Candidate;
    return true;
}

bool DiscGolfAIPlannerAdapter::ValidateProfile(
    const UDiscGolfAIGolferProfile& Profile,
    FString& OutError)
{
    OutError.Reset();
    if (Profile.GolferId.IsNone()
        || !Profile.GolferId.ToString().StartsWith(TEXT("dg_generic_")))
    {
        OutError = TEXT("AI profile requires a stable generic-only GolferId.");
        return false;
    }
    if (Profile.DisplayName.IsEmpty())
    {
        OutError = TEXT("AI profile requires a display name.");
        return false;
    }
    if (!ValidateSkill(Profile.Skill.Power, TEXT("Power"), OutError)
        || !ValidateSkill(Profile.Skill.Accuracy, TEXT("Accuracy"), OutError)
        || !ValidateSkill(Profile.Skill.Putting, TEXT("Putting"), OutError)
        || !ValidateSkill(Profile.Skill.Consistency, TEXT("Consistency"), OutError)
        || !ValidateSkill(Profile.Skill.Aggression, TEXT("Aggression"), OutError)
        || !ValidateSkill(Profile.Skill.CourseManagement, TEXT("CourseManagement"), OutError))
    {
        return false;
    }
    if (Profile.BagTemplate.IsEmpty() || Profile.BagTemplate.Num() > 24)
    {
        OutError = TEXT("AI profile bag must contain between one and 24 discs.");
        return false;
    }

    FDGDiscBagLoadout Loadout;
    Loadout.BagEquipmentId = TEXT("dg_generic_bag_default");
    Loadout.Capacity = 24;
    Loadout.Discs = Profile.BagTemplate;
    Loadout.SelectedDiscInstanceId = Profile.BagTemplate[0].InstanceId;
    if (!UDiscBagComponent::ValidateLoadout(Loadout, OutError))
    {
        return false;
    }
    return true;
}

bool DiscGolfAIPlannerAdapter::ValidateContext(
    const FDGAIShotContext& Context,
    FString& OutError)
{
    OutError.Reset();
    if (!IsFiniteVector(Context.LieLocationCm)
        || !IsFiniteVector(Context.TargetLocationCm)
        || !IsFiniteVector(Context.WindVelocityMps))
    {
        OutError = TEXT("AI shot context vectors must be finite.");
        return false;
    }
    if (FVector::DistSquared(Context.LieLocationCm, Context.TargetLocationCm)
        <= FMath::Square(PositionAgreementToleranceCm))
    {
        OutError = TEXT("AI shot context requires distinct lie and target locations.");
        return false;
    }
    if (Context.StrokesTaken < 0 || Context.Par < 1 || Context.Par > 20)
    {
        OutError = TEXT("AI shot context contains invalid strokes or par.");
        return false;
    }
    return true;
}

bool DiscGolfAIPlannerAdapter::ValidateCandidate(
    const UDiscGolfAIGolferProfile& Profile,
    const FDGAIShotCandidate& Candidate,
    FString& OutError)
{
    OutError.Reset();
    if (!ValidateProfile(Profile, OutError))
    {
        return false;
    }
    if (!UDiscBagComponent::ValidateDiscInstance(Candidate.Disc, OutError)
        || !DiscMatchesProfileMember(Profile, Candidate.Disc))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("AI candidate disc is not an exact profile-bag member.");
        }
        return false;
    }
    EThrowStyle ProjectThrowStyle = EThrowStyle::Backhand;
    if (!TryProjectThrowStyleFromPluginEnum(Candidate.ThrowType, ProjectThrowStyle))
    {
        OutError = TEXT("AI candidate throw type cannot map to a project throw style.");
        return false;
    }
    if (!IsProbability(Candidate.Power01)
        || !FMath::IsFinite(Candidate.HyzerDegrees)
        || Candidate.HyzerDegrees < -34.0f || Candidate.HyzerDegrees > 34.0f
        || !FMath::IsFinite(Candidate.NoseDegrees)
        || Candidate.NoseDegrees < -7.0f || Candidate.NoseDegrees > 11.0f
        || !IsFiniteVector(Candidate.PredictedLandingLocationCm)
        || !FMath::IsFinite(Candidate.ExpectedProgressM)
        || Candidate.ExpectedProgressM < 0.0f
        || !IsProbability(Candidate.SuccessProbability)
        || !IsProbability(Candidate.OutOfBoundsProbability)
        || !IsProbability(Candidate.ObstacleHitProbability)
        || !FMath::IsFinite(Candidate.LandingErrorM)
        || Candidate.LandingErrorM < 0.0f
        || !FMath::IsFinite(Candidate.UtilityScore))
    {
        OutError = TEXT("AI candidate contains invalid finite, bounded, or non-negative values.");
        return false;
    }
    return true;
}

bool DiscGolfAIPlannerAdapter::BuildCandidateFromMeasuredOutcome(
    const UDiscGolfAIGolferProfile& Profile,
    const FDGAIShotContext& Context,
    const FDiscGolfAIMeasuredShotOutcome& Outcome,
    FDGAIShotCandidate& OutCandidate,
    FString& OutError)
{
    OutError.Reset();
    if (!ValidateProfile(Profile, OutError)
        || !ValidateContext(Context, OutError)
        || !ValidateCommand(Profile, Outcome.Command, OutError)
        || !ValidateFinalTelemetry(Outcome.FinalTelemetry, OutError))
    {
        return false;
    }
    if (!IsFiniteVector(Outcome.StartWorldLocationCm)
        || !IsFiniteVector(Outcome.FinalWorldLocationCm))
    {
        OutError = TEXT("Measured outcome locations must be finite.");
        return false;
    }
    if (!Outcome.StartWorldLocationCm.Equals(
            Context.LieLocationCm, PositionAgreementToleranceCm))
    {
        OutError = TEXT("Measured outcome start does not match the shot-context lie.");
        return false;
    }
    const bool bCommandIsPutting =
        Outcome.Command.ShotContext != EDiscShotContext::Drive;
    if (bCommandIsPutting != Context.bPutting
        || Outcome.FinalTelemetry.Release.ThrowStyle != Outcome.Command.ThrowStyle
        || Outcome.FinalTelemetry.Release.ShotContext != Outcome.Command.ShotContext)
    {
        OutError = TEXT("Measured outcome command, context, and release identities disagree.");
        return false;
    }

    const double StartDistanceCm = FVector::Dist(
        Outcome.StartWorldLocationCm, Context.TargetLocationCm);
    const double FinalDistanceCm = FVector::Dist(
        Outcome.FinalWorldLocationCm, Context.TargetLocationCm);
    const double ProgressM = (StartDistanceCm - FinalDistanceCm) / 100.0;
    const double LandingErrorM = FinalDistanceCm / 100.0;
    if (!FMath::IsFinite(StartDistanceCm)
        || !FMath::IsFinite(FinalDistanceCm)
        || !FMath::IsFinite(ProgressM)
        || !FMath::IsFinite(LandingErrorM)
        || ProgressM < 0.0
        || ProgressM > static_cast<double>(TNumericLimits<float>::Max())
        || LandingErrorM > static_cast<double>(TNumericLimits<float>::Max()))
    {
        OutError = TEXT("Measured outcome does not provide finite non-negative progress.");
        return false;
    }

    const FDGDiscInstance* Disc = Profile.BagTemplate.FindByPredicate(
        [&Outcome](const FDGDiscInstance& Entry)
        {
            return Entry.InstanceId == Outcome.Command.DiscInstanceId;
        });
    if (!Disc)
    {
        OutError = TEXT("Measured outcome disc disappeared from the validated profile.");
        return false;
    }

    FDGAIShotCandidate Candidate;
    Candidate.Disc = *Disc;
    Candidate.ThrowType = PluginThrowTypeFromProjectEnum(Outcome.Command.ThrowStyle);
    Candidate.Power01 = Outcome.Command.Power01;
    Candidate.HyzerDegrees = Outcome.Command.HyzerDeg;
    Candidate.NoseDegrees = Outcome.Command.NoseAngleDeg;
    Candidate.PredictedLandingLocationCm = Outcome.FinalWorldLocationCm;
    Candidate.ExpectedProgressM = static_cast<float>(ProgressM);
    Candidate.SuccessProbability =
        Outcome.FinalTelemetry.State == EDiscFlightState::HoledOut
        || FinalDistanceCm < StartDistanceCm ? 1.0f : 0.0f;
    Candidate.OutOfBoundsProbability =
        Outcome.FinalTelemetry.CourseSurface == ECourseSurfaceType::OutOfBounds
        ? 1.0f : 0.0f;
    Candidate.ObstacleHitProbability =
        Outcome.FinalTelemetry.FixtureContactCount > 0 ? 1.0f : 0.0f;
    Candidate.LandingErrorM = static_cast<float>(LandingErrorM);
    Candidate.UtilityScore = 0.0f;
    if (!ValidateCandidate(Profile, Candidate, OutError))
    {
        return false;
    }

    OutCandidate = MoveTemp(Candidate);
    return true;
}

bool DiscGolfAIPlannerAdapter::SelectMeasuredShot(
    const UDiscGolfAIGolferProfile& Profile,
    const FDGAIShotContext& Context,
    const TArray<FDiscGolfAIMeasuredShotOutcome>& Outcomes,
    FThrowCommand& OutCommand,
    FDGAIShotCandidate& OutCandidate,
    FString& OutError)
{
    OutError.Reset();
    if (!ValidateProfile(Profile, OutError)
        || !ValidateContext(Context, OutError))
    {
        return false;
    }
    if (Outcomes.IsEmpty() || Outcomes.Num() > MaxMeasuredCandidateCount)
    {
        OutError = TEXT("AI selection requires between one and 256 measured outcomes.");
        return false;
    }

    TArray<FPreparedMeasuredCandidate> Prepared;
    Prepared.Reserve(Outcomes.Num());
    for (const FDiscGolfAIMeasuredShotOutcome& Outcome : Outcomes)
    {
        FDGAIShotCandidate Candidate;
        if (!BuildCandidateFromMeasuredOutcome(
                Profile, Context, Outcome, Candidate, OutError))
        {
            return false;
        }
        FPreparedMeasuredCandidate& Entry = Prepared.AddDefaulted_GetRef();
        Entry.Candidate = MoveTemp(Candidate);
        Entry.SourceCommand = Outcome.Command;
        Entry.SourceOutcome = Outcome;
    }
    Prepared.StableSort(StableCandidateLess);

    TArray<FDGAIShotCandidate> PlannerCandidates;
    PlannerCandidates.Reserve(Prepared.Num());
    for (const FPreparedMeasuredCandidate& Entry : Prepared)
    {
        PlannerCandidates.Add(Entry.Candidate);
    }

    UDiscGolfAIShotPlannerComponent* Planner =
        NewObject<UDiscGolfAIShotPlannerComponent>(GetTransientPackage());
    if (!Planner)
    {
        OutError = TEXT("AI shot planner component could not be allocated.");
        return false;
    }
    Planner->Skill = Profile.Skill;

    FDGAIShotCandidate Selected;
    if (!Planner->SelectBestCandidate(PlannerCandidates, Selected)
        || !ValidateCandidate(Profile, Selected, OutError))
    {
        if (OutError.IsEmpty())
        {
            OutError = TEXT("AI shot planner rejected the measured candidate set.");
        }
        return false;
    }

    const int32 SelectedIndex = Prepared.IndexOfByPredicate(
        [&Selected](const FPreparedMeasuredCandidate& Entry)
        {
            return CandidateIdentityMatches(Entry.Candidate, Selected);
        });
    if (!Prepared.IsValidIndex(SelectedIndex))
    {
        OutError = TEXT("AI planner result could not map back to its measured command.");
        return false;
    }

    const FPreparedMeasuredCandidate& Source = Prepared[SelectedIndex];
    FThrowCommand Command = Source.SourceCommand;
    EDiscPlastic Plastic = EDiscPlastic::Tour;
    EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    if (!TryProjectPlasticFromId(Selected.Disc.PlasticId, Plastic)
        || !TryProjectThrowStyleFromPluginEnum(Selected.ThrowType, ThrowStyle))
    {
        OutError = TEXT("Selected plugin candidate cannot map to project enums.");
        return false;
    }
    Command.DiscInstanceId = Selected.Disc.InstanceId;
    Command.MoldId = Selected.Disc.DiscDefinitionId;
    Command.Plastic = Plastic;
    Command.ThrowStyle = ThrowStyle;
    Command.Power01 = Selected.Power01;
    Command.HyzerDeg = Selected.HyzerDegrees;
    Command.NoseAngleDeg = Selected.NoseDegrees;
    if (!ValidateCommand(Profile, Command, OutError))
    {
        return false;
    }

    OutCommand = MoveTemp(Command);
    OutCandidate = MoveTemp(Selected);
    return true;
}
