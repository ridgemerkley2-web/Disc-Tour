#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscCatalogSubsystem.h"
#include "../DiscGolfMath.h"

#include <limits>

namespace
{
    FThrowCommand MakeValidSolverCommand(
        float TimingError = 0.75f,
        EThrowStyle Style = EThrowStyle::Backhand,
        EDGHandedness Handedness = EDGHandedness::Right,
        EDiscShotContext ShotContext = EDiscShotContext::Drive)
    {
        FThrowCommand Command;
        Command.MoldId = TEXT("Apex");
        Command.Plastic = EDiscPlastic::Tour;
        Command.ThrowStyle = Style;
        Command.Handedness = Handedness;
        Command.ShotContext = ShotContext;
        Command.Direction = FVector::ForwardVector;
        Command.Power01 = 0.82f;
        Command.HyzerDeg = 3.0f;
        Command.NoseAngleDeg = 1.0f;
        Command.LaunchAngleDeg = 7.0f;
        Command.TimingError = TimingError;
        return Command;
    }

    FResolvedDiscDefinition MakeValidResolvedDisc()
    {
        TArray<FDiscMoldDefinition> Molds;
        TArray<FDiscPlasticDefinition> Plastics;
        UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);

        FResolvedDiscDefinition Disc;
        UDiscCatalogSubsystem::ResolveFromDefinitions(
            Molds, Plastics, TEXT("Apex"), EDiscPlastic::Tour, Disc);
        return Disc;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfThrowReleaseSolverInputValidationTest,
    "DiscGolfTour.Physics.SolverInput.ReleaseValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfThrowReleaseSolverInputValidationTest::RunTest(const FString& Parameters)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const FThrowRelease Valid = DiscGolfMath::ResolveThrowRelease(MakeValidSolverCommand());

    FString Error = TEXT("stale");
    TestTrue(TEXT("Resolver output satisfies the solver release contract"),
        DiscGolfMath::IsThrowReleaseValid(Valid, &Error));
    TestTrue(TEXT("Successful release validation clears an old diagnostic"), Error.IsEmpty());

    const EThrowStyle Styles[] = { EThrowStyle::Backhand, EThrowStyle::Forehand };
    const EDGHandedness Hands[] = { EDGHandedness::Right, EDGHandedness::Left };
    const EDiscShotContext Contexts[] = {
        EDiscShotContext::Drive,
        EDiscShotContext::Circle2Putt,
        EDiscShotContext::Circle1Putt
    };
    const float TimingEndpoints[] = { -1.0f, 1.0f };
    for (EThrowStyle Style : Styles)
    {
        for (EDGHandedness Hand : Hands)
        {
            for (EDiscShotContext Context : Contexts)
            {
                for (float Timing : TimingEndpoints)
                {
                    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(
                        MakeValidSolverCommand(Timing, Style, Hand, Context));
                    TestTrue(TEXT("All style/hand/context combinations and timing endpoints validate"),
                        DiscGolfMath::IsThrowReleaseValid(Release));
                }
            }
        }
    }

    FThrowRelease Boundary = Valid;
    Boundary.ReleaseSpeedMps = 100.0f;
    Boundary.SpinRpm = 5000.0f;
    Boundary.EffectiveHyzerDeg = 34.0f;
    Boundary.EffectiveNoseAngleDeg = 11.0f;
    Boundary.EffectiveLaunchAngleDeg = 35.0f;
    Boundary.WindPhaseOriginSeconds = 4095.999f;
    Boundary.LiePowerMultiplier = 0.001f;
    Boundary.LieTimingErrorMultiplier = 2.0f;
    TestTrue(TEXT("Inclusive upper release bounds and a positive lie multiplier validate"),
        DiscGolfMath::IsThrowReleaseValid(Boundary));
    Boundary.EffectiveHyzerDeg = -34.0f;
    Boundary.EffectiveNoseAngleDeg = -7.0f;
    Boundary.EffectiveLaunchAngleDeg = -5.0f;
    Boundary.WindPhaseOriginSeconds = 0.0f;
    TestTrue(TEXT("Inclusive lower angle and wind-phase bounds validate"),
        DiscGolfMath::IsThrowReleaseValid(Boundary));

    {
        FThrowRelease Candidate = Valid;
        Candidate.Grade = static_cast<EReleaseGrade>(255);
        TestFalse(TEXT("Invalid release grade is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Timing = static_cast<EReleaseTiming>(255);
        TestFalse(TEXT("Invalid release timing is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.ThrowStyle = static_cast<EThrowStyle>(255);
        TestFalse(TEXT("Invalid throw style is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Handedness = static_cast<EDGHandedness>(255);
        TestFalse(TEXT("Invalid handedness is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.ShotContext = static_cast<EDiscShotContext>(255);
        TestFalse(TEXT("Invalid shot context is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
    }

    struct FReleaseFloatMutation
    {
        const TCHAR* Name;
        float FThrowRelease::* Field;
        float Value;
    };
    const FReleaseFloatMutation NonFiniteCases[] = {
        { TEXT("TimingError"), &FThrowRelease::TimingError, NaN },
        { TEXT("Quality01"), &FThrowRelease::Quality01, Infinity },
        { TEXT("SpeedMultiplier"), &FThrowRelease::SpeedMultiplier, NaN },
        { TEXT("SpinMultiplier"), &FThrowRelease::SpinMultiplier, Infinity },
        { TEXT("ReleaseSpeedMps"), &FThrowRelease::ReleaseSpeedMps, NaN },
        { TEXT("SpinRpm"), &FThrowRelease::SpinRpm, Infinity },
        { TEXT("AimOffsetDeg"), &FThrowRelease::AimOffsetDeg, NaN },
        { TEXT("HyzerOffsetDeg"), &FThrowRelease::HyzerOffsetDeg, Infinity },
        { TEXT("NoseOffsetDeg"), &FThrowRelease::NoseOffsetDeg, NaN },
        { TEXT("LaunchOffsetDeg"), &FThrowRelease::LaunchOffsetDeg, Infinity },
        { TEXT("EffectiveHyzerDeg"), &FThrowRelease::EffectiveHyzerDeg, NaN },
        { TEXT("EffectiveNoseAngleDeg"), &FThrowRelease::EffectiveNoseAngleDeg, Infinity },
        { TEXT("EffectiveLaunchAngleDeg"), &FThrowRelease::EffectiveLaunchAngleDeg, NaN },
        { TEXT("WindPhaseOriginSeconds"), &FThrowRelease::WindPhaseOriginSeconds, Infinity },
        { TEXT("LiePowerMultiplier"), &FThrowRelease::LiePowerMultiplier, NaN },
        { TEXT("LieTimingErrorMultiplier"), &FThrowRelease::LieTimingErrorMultiplier, Infinity }
    };
    for (const FReleaseFloatMutation& Mutation : NonFiniteCases)
    {
        FThrowRelease Candidate = Valid;
        Candidate.*(Mutation.Field) = Mutation.Value;
        Error.Reset();
        TestFalse(*FString::Printf(TEXT("Non-finite %s is rejected"), Mutation.Name),
            DiscGolfMath::IsThrowReleaseValid(Candidate, &Error));
        TestTrue(*FString::Printf(TEXT("%s diagnostic names the field"), Mutation.Name),
            Error.Contains(Mutation.Name));
    }

    const FReleaseFloatMutation RangeCases[] = {
        { TEXT("TimingError below"), &FThrowRelease::TimingError, -1.01f },
        { TEXT("TimingError above"), &FThrowRelease::TimingError, 1.01f },
        { TEXT("Quality01 below"), &FThrowRelease::Quality01, -0.01f },
        { TEXT("Quality01 above"), &FThrowRelease::Quality01, 1.01f },
        { TEXT("SpeedMultiplier zero"), &FThrowRelease::SpeedMultiplier, 0.0f },
        { TEXT("SpeedMultiplier above"), &FThrowRelease::SpeedMultiplier, 1.01f },
        { TEXT("SpinMultiplier zero"), &FThrowRelease::SpinMultiplier, 0.0f },
        { TEXT("SpinMultiplier above"), &FThrowRelease::SpinMultiplier, 1.01f },
        { TEXT("ReleaseSpeedMps zero"), &FThrowRelease::ReleaseSpeedMps, 0.0f },
        { TEXT("ReleaseSpeedMps above"), &FThrowRelease::ReleaseSpeedMps, 100.01f },
        { TEXT("SpinRpm zero"), &FThrowRelease::SpinRpm, 0.0f },
        { TEXT("SpinRpm above"), &FThrowRelease::SpinRpm, 5000.01f },
        { TEXT("AimOffsetDeg below"), &FThrowRelease::AimOffsetDeg, -6.01f },
        { TEXT("AimOffsetDeg above"), &FThrowRelease::AimOffsetDeg, 6.01f },
        { TEXT("HyzerOffsetDeg below"), &FThrowRelease::HyzerOffsetDeg, -4.01f },
        { TEXT("HyzerOffsetDeg above"), &FThrowRelease::HyzerOffsetDeg, 4.01f },
        { TEXT("NoseOffsetDeg below"), &FThrowRelease::NoseOffsetDeg, -3.01f },
        { TEXT("NoseOffsetDeg above"), &FThrowRelease::NoseOffsetDeg, 3.01f },
        { TEXT("LaunchOffsetDeg below"), &FThrowRelease::LaunchOffsetDeg, -2.01f },
        { TEXT("LaunchOffsetDeg above"), &FThrowRelease::LaunchOffsetDeg, 2.01f },
        { TEXT("EffectiveHyzerDeg below"), &FThrowRelease::EffectiveHyzerDeg, -34.01f },
        { TEXT("EffectiveHyzerDeg above"), &FThrowRelease::EffectiveHyzerDeg, 34.01f },
        { TEXT("EffectiveNoseAngleDeg below"), &FThrowRelease::EffectiveNoseAngleDeg, -7.01f },
        { TEXT("EffectiveNoseAngleDeg above"), &FThrowRelease::EffectiveNoseAngleDeg, 11.01f },
        { TEXT("EffectiveLaunchAngleDeg below"), &FThrowRelease::EffectiveLaunchAngleDeg, -5.01f },
        { TEXT("EffectiveLaunchAngleDeg above"), &FThrowRelease::EffectiveLaunchAngleDeg, 35.01f },
        { TEXT("WindPhaseOriginSeconds below"), &FThrowRelease::WindPhaseOriginSeconds, -0.01f },
        { TEXT("WindPhaseOriginSeconds above"), &FThrowRelease::WindPhaseOriginSeconds, 4096.0f },
        { TEXT("LiePowerMultiplier zero"), &FThrowRelease::LiePowerMultiplier, 0.0f },
        { TEXT("LiePowerMultiplier above"), &FThrowRelease::LiePowerMultiplier, 1.01f },
        { TEXT("LieTimingErrorMultiplier below"), &FThrowRelease::LieTimingErrorMultiplier, 0.99f },
        { TEXT("LieTimingErrorMultiplier above"), &FThrowRelease::LieTimingErrorMultiplier, 2.01f }
    };
    for (const FReleaseFloatMutation& Mutation : RangeCases)
    {
        FThrowRelease Candidate = Valid;
        Candidate.*(Mutation.Field) = Mutation.Value;
        TestFalse(*FString::Printf(TEXT("Out-of-range %s is rejected"), Mutation.Name),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
    }

    {
        FThrowRelease Candidate = Valid;
        Candidate.Direction.X = NaN;
        TestFalse(TEXT("Non-finite direction is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Direction = FVector::ZeroVector;
        TestFalse(TEXT("Zero direction is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Direction = FVector::UpVector;
        TestFalse(TEXT("Vertical direction is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Direction = FVector(1.0f, 0.0f, 0.01f);
        TestFalse(TEXT("Direction with elevation is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Direction = FVector(0.5f, 0.0f, 0.0f);
        TestFalse(TEXT("Non-unit horizontal direction is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Direction = FVector(
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            0.0f);
        TestFalse(TEXT("Direction whose squared magnitude overflows is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
    }

    const FReleaseFloatMutation PhaseCases[] = {
        { TEXT("Quality01"), &FThrowRelease::Quality01, Valid.Quality01 + 0.01f },
        { TEXT("SpeedMultiplier"), &FThrowRelease::SpeedMultiplier, Valid.SpeedMultiplier + 0.01f },
        { TEXT("SpinMultiplier"), &FThrowRelease::SpinMultiplier, Valid.SpinMultiplier + 0.01f },
        { TEXT("AimOffsetDeg"), &FThrowRelease::AimOffsetDeg, Valid.AimOffsetDeg + 0.01f },
        { TEXT("HyzerOffsetDeg"), &FThrowRelease::HyzerOffsetDeg, Valid.HyzerOffsetDeg + 0.01f },
        { TEXT("NoseOffsetDeg"), &FThrowRelease::NoseOffsetDeg, Valid.NoseOffsetDeg + 0.01f },
        { TEXT("LaunchOffsetDeg"), &FThrowRelease::LaunchOffsetDeg, Valid.LaunchOffsetDeg + 0.01f }
    };
    for (const FReleaseFloatMutation& Mutation : PhaseCases)
    {
        FThrowRelease Candidate = Valid;
        Candidate.*(Mutation.Field) = Mutation.Value;
        Error.Reset();
        TestFalse(*FString::Printf(TEXT("Timing phase mismatch in %s is rejected"), Mutation.Name),
            DiscGolfMath::IsThrowReleaseValid(Candidate, &Error));
        TestTrue(*FString::Printf(TEXT("Phase diagnostic names %s"), Mutation.Name),
            Error.Contains(Mutation.Name));
    }
    {
        FThrowRelease Candidate = Valid;
        Candidate.Grade = EReleaseGrade::Perfect;
        TestFalse(TEXT("Valid but timing-inconsistent grade is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
        Candidate = Valid;
        Candidate.Timing = EReleaseTiming::Early;
        TestFalse(TEXT("Valid but timing-inconsistent phase label is rejected"),
            DiscGolfMath::IsThrowReleaseValid(Candidate));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfResolvedDiscSolverInputValidationTest,
    "DiscGolfTour.Physics.SolverInput.ResolvedDiscValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfResolvedDiscSolverInputValidationTest::RunTest(const FString& Parameters)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const FResolvedDiscDefinition Valid = MakeValidResolvedDisc();
    FString Error = TEXT("stale");
    TestTrue(TEXT("Fallback post-plastic resolved disc satisfies the runtime contract"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(Valid, &Error));
    TestTrue(TEXT("Successful resolved-disc validation clears an old diagnostic"), Error.IsEmpty());

    const auto ExpectInvalid = [this, &Valid](
        const TCHAR* Label, const TFunction<void(FResolvedDiscDefinition&)>& Mutate)
    {
        FResolvedDiscDefinition Candidate = Valid;
        Mutate(Candidate);
        TestFalse(Label, DiscGolfMath::IsResolvedDiscDefinitionValid(Candidate));
    };

    ExpectInvalid(TEXT("Missing mold identity is rejected"),
        [](FResolvedDiscDefinition& D) { D.MoldId = NAME_None; });
    ExpectInvalid(TEXT("Missing display name is rejected"),
        [](FResolvedDiscDefinition& D) { D.DisplayName = FText::GetEmpty(); });
    ExpectInvalid(TEXT("Speed below one is rejected"),
        [](FResolvedDiscDefinition& D) { D.Speed = 0; });
    ExpectInvalid(TEXT("Speed above fifteen is rejected"),
        [](FResolvedDiscDefinition& D) { D.Speed = 16; });
    ExpectInvalid(TEXT("Glide below one is rejected"),
        [](FResolvedDiscDefinition& D) { D.Glide = 0; });
    ExpectInvalid(TEXT("Glide above seven is rejected"),
        [](FResolvedDiscDefinition& D) { D.Glide = 8; });
    ExpectInvalid(TEXT("Non-finite turn rating is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.Turn = NaN; });
    ExpectInvalid(TEXT("Non-finite fade rating is rejected"),
        [Infinity](FResolvedDiscDefinition& D) { D.Fade = Infinity; });
    ExpectInvalid(TEXT("Invalid plastic enum is rejected"),
        [](FResolvedDiscDefinition& D) { D.Plastic = static_cast<EDiscPlastic>(255); });
    ExpectInvalid(TEXT("Non-finite mass metadata is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.DiscMassGrams = NaN; });
    ExpectInvalid(TEXT("Mass below 130 grams is rejected"),
        [](FResolvedDiscDefinition& D) { D.DiscMassGrams = 129.99f; });
    ExpectInvalid(TEXT("Mass above 200 grams is rejected"),
        [](FResolvedDiscDefinition& D) { D.DiscMassGrams = 200.01f; });
    ExpectInvalid(TEXT("Mass metadata/aerodynamic mass mismatch is rejected"),
        [](FResolvedDiscDefinition& D) { D.Aero.MassKg = 0.174f; });
    ExpectInvalid(TEXT("Non-finite wear metadata is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.DiscWear01 = NaN; });
    ExpectInvalid(TEXT("Wear below zero is rejected"),
        [](FResolvedDiscDefinition& D) { D.DiscWear01 = -0.01f; });
    ExpectInvalid(TEXT("Wear above one is rejected"),
        [](FResolvedDiscDefinition& D) { D.DiscWear01 = 1.01f; });
    ExpectInvalid(TEXT("Non-finite red color is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.DiscColor.R = NaN; });
    ExpectInvalid(TEXT("Non-finite green color is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.DiscColor.G = NaN; });
    ExpectInvalid(TEXT("Non-finite blue color is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.DiscColor.B = NaN; });
    ExpectInvalid(TEXT("Non-finite alpha color is rejected"),
        [NaN](FResolvedDiscDefinition& D) { D.DiscColor.A = NaN; });
    ExpectInvalid(TEXT("Non-generic development stamp is rejected"),
        [](FResolvedDiscDefinition& D) { D.DiscStampId = TEXT("custom_stamp"); });
    ExpectInvalid(TEXT("Nickname longer than 64 characters is rejected"),
        [](FResolvedDiscDefinition& D) { D.DiscNickname = FString::ChrN(65, TEXT('N')); });
    ExpectInvalid(TEXT("Uncalibrated wear physics is rejected"),
        [](FResolvedDiscDefinition& D) { D.bWearAffectsPhysics = true; });

    FResolvedDiscDefinition Boundary = Valid;
    Boundary.Speed = 1;
    Boundary.Glide = 1;
    Boundary.DiscMassGrams = 130.0f;
    Boundary.Aero.MassKg = 0.130f;
    Boundary.DiscWear01 = 0.0f;
    Boundary.DiscNickname = FString::ChrN(64, TEXT('N'));
    TestTrue(TEXT("Lower metadata bounds and 64-character nickname validate"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(Boundary));
    Boundary.Speed = 15;
    Boundary.Glide = 7;
    Boundary.DiscMassGrams = 200.0f;
    Boundary.Aero.MassKg = 0.200f;
    Boundary.DiscWear01 = 1.0f;
    TestTrue(TEXT("Upper metadata bounds validate"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(Boundary));

    Boundary = Valid;
    Boundary.DiscInstanceId.Invalidate();
    Boundary.bDiscFavorite = false;
    TestTrue(TEXT("Catalog/regression identity and favorite false validate"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(Boundary));
    Boundary.DiscInstanceId = FGuid(1, 2, 3, 4);
    Boundary.bDiscFavorite = true;
    TestTrue(TEXT("Player identity and favorite true validate"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(Boundary));

    Boundary = Valid;
    Boundary.DiscMassGrams = NaN;
    Error.Reset();
    TestFalse(TEXT("Invalid metadata supplies a diagnostic"),
        DiscGolfMath::IsResolvedDiscDefinitionValid(Boundary, &Error));
    TestTrue(TEXT("Resolved-disc diagnostic identifies mass"), Error.Contains(TEXT("mass")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfAeroSolverInputValidationTest,
    "DiscGolfTour.Physics.SolverInput.AeroValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfAeroSolverInputValidationTest::RunTest(const FString& Parameters)
{
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const float Infinity = std::numeric_limits<float>::infinity();
    const FResolvedDiscDefinition ValidDisc = MakeValidResolvedDisc();
    const FDiscAeroProfile Valid = ValidDisc.Aero;
    FString Error = TEXT("stale");
    TestTrue(TEXT("Post-plastic fallback aero satisfies the solver contract"),
        DiscGolfMath::IsDiscAeroProfileValid(Valid, &Error));
    TestTrue(TEXT("Successful aero validation clears an old diagnostic"), Error.IsEmpty());

    FDiscAeroProfile Boundary;
    Boundary.MassKg = 1.0f;
    Boundary.DiameterM = 1.0f;
    Boundary.AreaM2 = 1.0f;
    Boundary.InertiaAxialKgM2 = 1.0f;
    Boundary.InertiaPlanarKgM2 = 1.0f;
    Boundary.CL0 = -10.0f;
    Boundary.CLa = 10.0f;
    Boundary.CD0 = 10.0f;
    Boundary.CDa = 10.0f;
    Boundary.CM0 = 10.0f;
    Boundary.CMa = -10.0f;
    Boundary.HighSpeedTurnMomentNm = 1.0f;
    Boundary.LowSpeedFadeMomentNm = 1.0f;
    Boundary.TurnStartsAboveMps = 100.0f;
    Boundary.FadeStartsBelowMps = 100.0f;
    Boundary.SpinDecayPerSecond = 100.0f;
    Boundary.GroundRestitution = 1.0f;
    Boundary.GroundFriction = 1.0f;
    TestTrue(TEXT("All catalog-aligned upper/sign boundaries validate"),
        DiscGolfMath::IsDiscAeroProfileValid(Boundary));
    Boundary.HighSpeedTurnMomentNm = 0.0f;
    Boundary.LowSpeedFadeMomentNm = 0.0f;
    Boundary.SpinDecayPerSecond = 0.0f;
    TestTrue(TEXT("Zero stability moments and spin decay validate"),
        DiscGolfMath::IsDiscAeroProfileValid(Boundary));

    struct FAeroMutation
    {
        const TCHAR* Name;
        float FDiscAeroProfile::* Field;
        float Value;
    };
    const FAeroMutation InvalidCases[] = {
        { TEXT("MassKg"), &FDiscAeroProfile::MassKg, 0.0f },
        { TEXT("DiameterM"), &FDiscAeroProfile::DiameterM, 1.01f },
        { TEXT("AreaM2"), &FDiscAeroProfile::AreaM2, NaN },
        { TEXT("InertiaAxialKgM2"), &FDiscAeroProfile::InertiaAxialKgM2, 0.0f },
        { TEXT("InertiaPlanarKgM2"), &FDiscAeroProfile::InertiaPlanarKgM2, Infinity },
        { TEXT("CL0"), &FDiscAeroProfile::CL0, 10.01f },
        { TEXT("CLa"), &FDiscAeroProfile::CLa, 0.0f },
        { TEXT("CD0"), &FDiscAeroProfile::CD0, 10.01f },
        { TEXT("CDa"), &FDiscAeroProfile::CDa, NaN },
        { TEXT("CM0"), &FDiscAeroProfile::CM0, Infinity },
        { TEXT("CMa"), &FDiscAeroProfile::CMa, -10.01f },
        { TEXT("HighSpeedTurnMomentNm"), &FDiscAeroProfile::HighSpeedTurnMomentNm, -0.01f },
        { TEXT("LowSpeedFadeMomentNm"), &FDiscAeroProfile::LowSpeedFadeMomentNm, 1.01f },
        { TEXT("TurnStartsAboveMps"), &FDiscAeroProfile::TurnStartsAboveMps, 0.0f },
        { TEXT("FadeStartsBelowMps"), &FDiscAeroProfile::FadeStartsBelowMps, 100.01f },
        { TEXT("SpinDecayPerSecond"), &FDiscAeroProfile::SpinDecayPerSecond, 100.01f },
        { TEXT("GroundRestitution"), &FDiscAeroProfile::GroundRestitution, 0.0f },
        { TEXT("GroundFriction"), &FDiscAeroProfile::GroundFriction, 1.01f }
    };
    for (const FAeroMutation& Mutation : InvalidCases)
    {
        FDiscAeroProfile Candidate = Valid;
        Candidate.*(Mutation.Field) = Mutation.Value;
        Error.Reset();
        TestFalse(*FString::Printf(TEXT("Invalid Aero.%s is rejected"), Mutation.Name),
            DiscGolfMath::IsDiscAeroProfileValid(Candidate, &Error));
        TestTrue(*FString::Printf(TEXT("Aero.%s diagnostic names the field"), Mutation.Name),
            Error.Contains(Mutation.Name));

        FResolvedDiscDefinition ResolvedCandidate = ValidDisc;
        ResolvedCandidate.Aero = Candidate;
        TestFalse(*FString::Printf(TEXT("Resolved disc composes Aero.%s validation"), Mutation.Name),
            DiscGolfMath::IsResolvedDiscDefinitionValid(ResolvedCandidate));
    }

    FDiscAeroProfile SpinDecayNonFinite = Valid;
    SpinDecayNonFinite.SpinDecayPerSecond = NaN;
    TestFalse(TEXT("Non-finite spin decay is rejected"),
        DiscGolfMath::IsDiscAeroProfileValid(SpinDecayNonFinite));
    return true;
}

#endif
