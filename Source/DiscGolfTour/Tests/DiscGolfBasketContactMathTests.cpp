#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfMath.h"

#include <limits>

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X)
            && FMath::IsFinite(Value.Y)
            && FMath::IsFinite(Value.Z);
    }

    bool IsFiniteDefaultContact(const FBasketContactEvaluation& Evaluation)
    {
        return Evaluation.Result == EBasketContactResult::None
            && FMath::IsFinite(Evaluation.PredictedRadialCm)
            && FMath::IsNearlyZero(Evaluation.PredictedRadialCm)
            && FMath::IsFinite(Evaluation.PredictedHeightCm)
            && FMath::IsNearlyZero(Evaluation.PredictedHeightCm)
            && FMath::IsFinite(Evaluation.IncomingSpeedMps)
            && FMath::IsNearlyZero(Evaluation.IncomingSpeedMps)
            && IsFiniteVector(Evaluation.DeflectedVelocityMps)
            && Evaluation.DeflectedVelocityMps.IsNearlyZero();
    }

    void SetVectorComponent(FVector& Value, int32 ComponentIndex, double ComponentValue)
    {
        switch (ComponentIndex)
        {
            case 0: Value.X = ComponentValue; break;
            case 1: Value.Y = ComponentValue; break;
            default: Value.Z = ComponentValue; break;
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBasketContactNonFiniteInputTest,
    "DiscGolfTour.Physics.ContactMath.BasketRejectsNonFiniteInput",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBasketContactNonFiniteInputTest::RunTest(const FString& Parameters)
{
    const double InvalidValues[] = {
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    const TCHAR* InvalidNames[] = { TEXT("NaN"), TEXT("+Inf"), TEXT("-Inf") };
    const TCHAR* ComponentNames[] = { TEXT("X"), TEXT("Y"), TEXT("Z") };

    const FVector ValidLocation(-52.0, 0.0, 114.0);
    const FVector ValidVelocity(8.0, 0.0, 0.0);
    for (int32 ValueIndex = 0; ValueIndex < UE_ARRAY_COUNT(InvalidValues); ++ValueIndex)
    {
        for (int32 ComponentIndex = 0; ComponentIndex < 3; ++ComponentIndex)
        {
            FVector InvalidLocation = ValidLocation;
            SetVectorComponent(
                InvalidLocation, ComponentIndex, InvalidValues[ValueIndex]);
            const FBasketContactEvaluation LocationEvaluation =
                DiscGolfMath::EvaluateBasketContact(InvalidLocation, ValidVelocity);
            TestTrue(*FString::Printf(
                TEXT("RelativeLocation.%s %s returns a finite default evaluation"),
                ComponentNames[ComponentIndex], InvalidNames[ValueIndex]),
                IsFiniteDefaultContact(LocationEvaluation));

            FVector InvalidVelocity = ValidVelocity;
            SetVectorComponent(
                InvalidVelocity, ComponentIndex, InvalidValues[ValueIndex]);
            const FBasketContactEvaluation VelocityEvaluation =
                DiscGolfMath::EvaluateBasketContact(ValidLocation, InvalidVelocity);
            TestTrue(*FString::Printf(
                TEXT("Velocity.%s %s returns a finite default evaluation"),
                ComponentNames[ComponentIndex], InvalidNames[ValueIndex]),
                IsFiniteDefaultContact(VelocityEvaluation));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBasketContactDerivedOverflowTest,
    "DiscGolfTour.Physics.ContactMath.BasketRejectsDerivedOverflow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBasketContactDerivedOverflowTest::RunTest(const FString& Parameters)
{
    const double MaximumFinite = std::numeric_limits<double>::max();

    const FBasketContactEvaluation SpeedOverflow =
        DiscGolfMath::EvaluateBasketContact(
            FVector(-52.0, 0.0, 114.0),
            FVector(MaximumFinite, 0.0, 0.0));
    TestTrue(TEXT("Finite velocity whose derived speed overflows returns the default"),
        IsFiniteDefaultContact(SpeedOverflow));

    const FBasketContactEvaluation DotOverflow =
        DiscGolfMath::EvaluateBasketContact(
            FVector(MaximumFinite, 0.0, 114.0),
            FVector(-8.0, 0.0, 0.0));
    TestTrue(TEXT("Finite inputs whose closest-approach dot product overflows return the default"),
        IsFiniteDefaultContact(DotOverflow));

    const FBasketContactEvaluation RadiusOverflow =
        DiscGolfMath::EvaluateBasketContact(
            FVector(-52.0, MaximumFinite, 114.0),
            FVector(8.0, 0.0, 0.0));
    TestTrue(TEXT("Finite inputs whose predicted radial magnitude overflows return the default"),
        IsFiniteDefaultContact(RadiusOverflow));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfBasketContactValidBehaviorTest,
    "DiscGolfTour.Physics.ContactMath.BasketValidBehaviorUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfBasketContactValidBehaviorTest::RunTest(const FString& Parameters)
{
    const FBasketContactEvaluation Caught = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0, 0.0, 114.0), FVector(8.0, 0.0, 0.0));
    TestEqual(TEXT("Valid center-chain contact remains caught"),
        Caught.Result, EBasketContactResult::Caught);
    TestTrue(TEXT("Caught evaluation remains finite"),
        FMath::IsFinite(Caught.PredictedRadialCm)
        && FMath::IsFinite(Caught.PredictedHeightCm)
        && FMath::IsFinite(Caught.IncomingSpeedMps)
        && IsFiniteVector(Caught.DeflectedVelocityMps));

    const FBasketContactEvaluation Deflected = DiscGolfMath::EvaluateBasketContact(
        FVector(-52.0, 36.0, 114.0), FVector(8.0, 0.0, 0.0));
    TestEqual(TEXT("Valid weak-side contact remains a chain deflection"),
        Deflected.Result, EBasketContactResult::ChainDeflection);
    TestTrue(TEXT("Valid weak-side deflection retains its established speed loss"),
        Deflected.DeflectedVelocityMps.Size() < 3.0);
    TestTrue(TEXT("Deflection evaluation remains finite"),
        FMath::IsFinite(Deflected.PredictedRadialCm)
        && FMath::IsFinite(Deflected.PredictedHeightCm)
        && FMath::IsFinite(Deflected.IncomingSpeedMps)
        && IsFiniteVector(Deflected.DeflectedVelocityMps));
    return true;
}

#endif
