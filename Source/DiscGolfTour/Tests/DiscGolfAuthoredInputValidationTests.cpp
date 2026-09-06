#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfCourseDefinition.h"
#include "../DiscGolfVegetationInteractionActor.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfCollisionFixtureRotationValidationTest,
    "DiscGolfTour.CourseDefinition.CollisionFixtureRotationValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfCollisionFixtureRotationValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    FDiscGolfHoleBlockoutDefinition Definition =
        DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    FString Error;
    TestTrue(TEXT("The Pine Ridge fallback starts with valid fixture rotations"),
        DiscGolfCourseDefinition::Validate(Definition, Error));
    TestFalse(TEXT("The fallback contains collision fixtures"),
        Definition.CollisionFixtures.IsEmpty());
    if (Definition.CollisionFixtures.IsEmpty())
    {
        return false;
    }

    Definition.CollisionFixtures[0].Rotation.Pitch =
        std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("A NaN collision-fixture rotation is rejected"),
        DiscGolfCourseDefinition::Validate(Definition, Error));
    TestTrue(TEXT("The NaN rejection identifies the collision fixture"),
        Error.Contains(TEXT("collision fixture")));

    Definition = DiscGolfCourseDefinition::PineRidgeHole1Fallback();
    Definition.CollisionFixtures[0].Rotation.Yaw =
        std::numeric_limits<float>::infinity();
    TestFalse(TEXT("An infinite collision-fixture rotation is rejected"),
        DiscGolfCourseDefinition::Validate(Definition, Error));
    TestTrue(TEXT("The infinity rejection identifies the collision fixture"),
        Error.Contains(TEXT("collision fixture")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfVegetationCooldownValidationTest,
    "DiscGolfTour.Environment.VegetationCooldownValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfVegetationCooldownValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;

    ADiscGolfVegetationInteractionActor* Interaction =
        NewObject<ADiscGolfVegetationInteractionActor>();
    TestTrue(TEXT("The default vegetation interaction contract is valid"),
        Interaction->HasValidInteractionContract());

    Interaction->Profile.ReentryCooldownSeconds = 0.0f;
    TestTrue(TEXT("A zero-second cooldown is valid"),
        Interaction->HasValidInteractionContract());
    Interaction->Profile.ReentryCooldownSeconds = 2.0f;
    TestTrue(TEXT("The authored two-second cooldown maximum is valid"),
        Interaction->HasValidInteractionContract());

    Interaction->Profile.ReentryCooldownSeconds = -0.01f;
    TestFalse(TEXT("A negative cooldown is rejected"),
        Interaction->HasValidInteractionContract());
    Interaction->Profile.ReentryCooldownSeconds = 2.01f;
    TestFalse(TEXT("A cooldown above the authored maximum is rejected"),
        Interaction->HasValidInteractionContract());
    Interaction->Profile.ReentryCooldownSeconds =
        std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("A NaN cooldown is rejected"),
        Interaction->HasValidInteractionContract());
    Interaction->Profile.ReentryCooldownSeconds =
        std::numeric_limits<float>::infinity();
    TestFalse(TEXT("An infinite cooldown is rejected"),
        Interaction->HasValidInteractionContract());
    return true;
}

#endif
