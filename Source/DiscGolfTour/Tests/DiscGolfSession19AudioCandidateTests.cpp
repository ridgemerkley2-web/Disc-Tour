#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfPresentationAudioRouterComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession19OriginalAudioCandidateBindingTest,
    "DiscGolfTour.Session19.AudioCandidate.CategoryBindings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession19OriginalAudioCandidateBindingTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfPresentationAudioRouterComponent* Router =
        NewObject<UDiscGolfPresentationAudioRouterComponent>();
    TestNotNull(TEXT("Audio router constructs"), Router);
    if (!Router)
    {
        return false;
    }

    TestEqual(TEXT("Exact user-authored override table remains empty by default"),
        Router->FallbackSounds.Num(), 0);
    TestEqual(TEXT("One generated candidate binding exists per semantic category"),
        Router->GetGeneratedCandidateBindingCount(), 12);

    const EDiscGolfPresentationAudioCategory Categories[] = {
        EDiscGolfPresentationAudioCategory::ThrowRelease,
        EDiscGolfPresentationAudioCategory::AirborneFlight,
        EDiscGolfPresentationAudioCategory::GroundContact,
        EDiscGolfPresentationAudioCategory::GroundState,
        EDiscGolfPresentationAudioCategory::BasketOutcome,
        EDiscGolfPresentationAudioCategory::Penalty,
        EDiscGolfPresentationAudioCategory::HoleStart,
        EDiscGolfPresentationAudioCategory::HoleCompletion,
        EDiscGolfPresentationAudioCategory::HoleTransition,
        EDiscGolfPresentationAudioCategory::RoundCompletion,
        EDiscGolfPresentationAudioCategory::Replay,
        EDiscGolfPresentationAudioCategory::Flyover,
    };
    for (EDiscGolfPresentationAudioCategory Category : Categories)
    {
        TestTrue(TEXT("Generated category binding retains a valid soft object path"),
            Router->HasGeneratedCandidateBinding(Category));
    }
    TestFalse(TEXT("Invalid category never receives a candidate binding"),
        Router->HasGeneratedCandidateBinding(EDiscGolfPresentationAudioCategory::Invalid));
    TestFalse(TEXT("Router remains event driven"), Router->PrimaryComponentTick.bCanEverTick);
    return true;
}

#endif
