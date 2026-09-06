#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfInputRoutePolicy.h"
#include "../DiscGolfPlayerExperience.h"
#include "../ThrowControllerComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12InputRouteIsolationTest,
    "DiscGolfTour.Session12.Input.RouteIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12InputRouteIsolationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace DiscGolfInputRoutePolicy;

    TestTrue(TEXT("Gameplay accepts gameplay"), AllowsAction(
        EDiscGolfInputRoute::Gameplay, EDiscGolfInputRoute::Gameplay));
    TestTrue(TEXT("Gameplay may enter aim/throw"), AllowsAction(
        EDiscGolfInputRoute::Gameplay, EDiscGolfInputRoute::AimThrow));
    TestFalse(TEXT("Timing transaction blocks generic gameplay"), AllowsAction(
        EDiscGolfInputRoute::AimThrow, EDiscGolfInputRoute::Gameplay));
    TestFalse(TEXT("UI never passes gameplay through"), AllowsAction(
        EDiscGolfInputRoute::UI, EDiscGolfInputRoute::Gameplay));
    TestFalse(TEXT("Replay never passes gameplay through"), AllowsAction(
        EDiscGolfInputRoute::Replay, EDiscGolfInputRoute::Gameplay));
    TestFalse(TEXT("Creator never passes gameplay through"), AllowsAction(
        EDiscGolfInputRoute::CharacterCreator, EDiscGolfInputRoute::Gameplay));

    FDiscGolfInputRouteContext Context;
    Context.bAimThrowActive = true;
    TestTrue(TEXT("Timing selects aim/throw"), Resolve(Context) == EDiscGolfInputRoute::AimThrow);
    Context.bReplayActive = true;
    TestTrue(TEXT("Replay takes precedence over timing"), Resolve(Context) == EDiscGolfInputRoute::Replay);
    Context.bUiOpen = true;
    TestTrue(TEXT("UI takes precedence over replay"), Resolve(Context) == EDiscGolfInputRoute::UI);
    Context.bCharacterCreatorOpen = true;
    TestTrue(TEXT("Creator is the top route"), Resolve(Context) == EDiscGolfInputRoute::CharacterCreator);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AccessibilityNormalizationTest,
    "DiscGolfTour.Session12.Accessibility.SettingsNormalizationAndColor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AccessibilityNormalizationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDiscGolfPlayerSettings Settings;
    Settings.AmbienceVolume = -2.0f;
    Settings.VoiceVolume = 5.0f;
    Settings.ControllerDeadZone = 2.0f;
    Settings.ReplaySpeed = 10.0f;
    Settings.AimAssist01 = -1.0f;
    Settings.TimingWindowScale = 9.0f;
    Settings.ColorVisionMode = static_cast<EDiscGolfColorVisionMode>(99);
    Settings.TracerColorPreset = static_cast<EDiscGolfTracerColorPreset>(99);
    Settings.Normalize();

    TestEqual(TEXT("Ambience clamps"), Settings.AmbienceVolume, 0.0f);
    TestEqual(TEXT("Voice clamps"), Settings.VoiceVolume, 1.0f);
    TestEqual(TEXT("Dead zone clamps"), Settings.ControllerDeadZone, 0.95f);
    TestEqual(TEXT("Replay speed clamps"), Settings.ReplaySpeed, 2.0f);
    TestEqual(TEXT("Aim assist clamps"), Settings.AimAssist01, 0.0f);
    TestEqual(TEXT("Timing scale clamps"), Settings.TimingWindowScale, 2.0f);
    TestTrue(TEXT("Invalid color-vision mode normalizes"),
        Settings.ColorVisionMode == EDiscGolfColorVisionMode::Tritanopia);
    TestTrue(TEXT("Invalid tracer preset normalizes"),
        Settings.TracerColorPreset == EDiscGolfTracerColorPreset::PaperWhite);
    TestTrue(TEXT("Accessible lime resolves to an opaque bright color"),
        DiscGolfPlayerExperience::ResolveTracerColor(
            EDiscGolfTracerColorPreset::AccessibleLime).A == 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession12AssistCommandCaptureTest,
    "DiscGolfTour.Session12.Accessibility.AssistCommandCaptureParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession12AssistCommandCaptureTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FVector RawDirection = FVector::ForwardVector;
    const FVector TargetDirection = RawDirection.RotateAngleAxis(10.0f, FVector::UpVector);
    TestEqual(TEXT("Zero aim assist preserves the accepted direction exactly"),
        UThrowControllerComponent::ApplyAimAssistToDirection(
            RawDirection, TargetDirection, 0.0f), RawDirection);

    const FVector Assisted = UThrowControllerComponent::ApplyAimAssistToDirection(
        RawDirection, TargetDirection, 0.5f);
    const float AssistedYaw = Assisted.Rotation().Yaw;
    TestTrue(TEXT("Assist corrects toward the target"), AssistedYaw > 0.0f);
    TestTrue(TEXT("Assist remains bounded to three degrees at half scale"), AssistedYaw <= 3.01f);

    const float BaseTimingError = UThrowControllerComponent::NormalizeTimingErrorForAccessibility(
        0.70f, 0.82f, 0.18f, 1.0f);
    const float WiderTimingError = UThrowControllerComponent::NormalizeTimingErrorForAccessibility(
        0.70f, 0.82f, 0.18f, 2.0f);
    TestTrue(TEXT("A wider timing window reduces normalized miss severity"),
        FMath::Abs(WiderTimingError) < FMath::Abs(BaseTimingError));

    UThrowControllerComponent* Controller = NewObject<UThrowControllerComponent>();
    Controller->SetAccessibilityAssist(0.0f, 1.0f);
    FThrowCommand Command;
    TestFalse(TEXT("First press starts timing"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, RawDirection, TargetDirection, Command));
    TestTrue(TEXT("Second press captures one command"), Controller->HandleThrowPress(
        TEXT("Apex"), EDiscPlastic::Tour, RawDirection, TargetDirection, Command));
    TestEqual(TEXT("Assist never changes equipment mold"), Command.MoldId, FName(TEXT("Apex")));
    TestTrue(TEXT("Assist never changes equipment plastic"), Command.Plastic == EDiscPlastic::Tour);
    TestEqual(TEXT("Default command capture retains exact direction"), Command.Direction, RawDirection);
    return true;
}

#endif
