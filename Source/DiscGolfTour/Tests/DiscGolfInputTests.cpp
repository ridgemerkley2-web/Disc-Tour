#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfGameplayGate.h"
#include "../DiscGolfInputConfig.h"
#include "../DiscGolfInputRoutePolicy.h"
#include "../ThrowControllerComponent.h"
#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

namespace
{
    const FEnhancedActionKeyMapping* FindMapping(
        const UInputMappingContext* Context,
        const UInputAction* Action,
        const FKey& Key)
    {
        for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
        {
            if (Mapping.Action == Action && Mapping.Key == Key)
            {
                return &Mapping;
            }
        }
        return nullptr;
    }

    template<typename TModifier>
    bool HasModifier(const FEnhancedActionKeyMapping* Mapping)
    {
        if (!Mapping) return false;
        for (const UInputModifier* Modifier : Mapping->Modifiers)
        {
            if (Modifier && Modifier->IsA<TModifier>()) return true;
        }
        return false;
    }

    float GetDeadZoneThreshold(const FEnhancedActionKeyMapping* Mapping)
    {
        if (!Mapping) return -1.0f;
        for (const UInputModifier* Modifier : Mapping->Modifiers)
        {
            if (const UInputModifierDeadZone* DeadZone = Cast<UInputModifierDeadZone>(Modifier))
            {
                return DeadZone->LowerThreshold;
            }
        }
        return -1.0f;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHoleIntroConfirmIsolationTest,
    "DiscGolfTour.Session19.Input.HoleIntroConfirmIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHoleIntroConfirmIsolationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using EDisposition = EDiscGolfPresentationDismissInputDisposition;
    FDiscGolfPresentationDismissInputBarrier Barrier;

    TestEqual(TEXT("Space press dismisses and arms its release barrier"),
        Barrier.Route(EKeys::SpaceBar, IE_Pressed, true, true),
        EDisposition::DismissAndConsume);
    TestTrue(TEXT("Space barrier is pending"), Barrier.IsPending(EKeys::SpaceBar));
    TestEqual(TEXT("Held Space repeat remains consumed after the intro closes"),
        Barrier.Route(EKeys::SpaceBar, IE_Repeat, true, false),
        EDisposition::ConsumeHeldEdge);
    Barrier.Arm(EKeys::Gamepad_FaceButton_Bottom);
    TestTrue(TEXT("A throw key already down before dismissal joins the snapshot"),
        Barrier.IsPending(EKeys::Gamepad_FaceButton_Bottom));
    TestEqual(TEXT("A new remapped throw edge also joins while pending"),
        Barrier.Route(EKeys::LeftMouseButton, IE_Pressed, true, false),
        EDisposition::ConsumeHeldEdge);
    TestTrue(TEXT("Remapped throw key joins the transaction"),
        Barrier.IsPending(EKeys::LeftMouseButton));
    TestEqual(TEXT("Space release is forwarded while still consumed"),
        Barrier.Route(EKeys::SpaceBar, IE_Released, true, false),
        EDisposition::ForwardReleaseAndConsume);
    TestTrue(TEXT("Barrier remains fail-closed through release evaluation"),
        Barrier.IsPending(EKeys::SpaceBar));
    const uint32 SpaceReleaseGeneration =
        Barrier.GetReleaseGeneration(EKeys::SpaceBar);
    TestTrue(TEXT("Space release has a completion generation"),
        SpaceReleaseGeneration != 0);
    TestEqual(TEXT("A release-queued Space repeat remains consumed"),
        Barrier.Route(EKeys::SpaceBar, IE_Repeat, true, false),
        EDisposition::ConsumeHeldEdge);
    TestEqual(TEXT("A repeat cannot invalidate the queued release generation"),
        Barrier.GetReleaseGeneration(EKeys::SpaceBar),
        SpaceReleaseGeneration);
    TestTrue(TEXT("Matching Space completion clears that key"),
        Barrier.CompleteRelease(EKeys::SpaceBar, SpaceReleaseGeneration));
    TestTrue(TEXT("Other throw keys keep the transaction closed after Space clears"),
        Barrier.IsPending());
    TestEqual(TEXT("South release is forwarded while still consumed"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Released, true, false),
        EDisposition::ForwardReleaseAndConsume);
    const uint32 SouthReleaseGeneration =
        Barrier.GetReleaseGeneration(EKeys::Gamepad_FaceButton_Bottom);
    TestTrue(TEXT("Matching South completion clears that key"),
        Barrier.CompleteRelease(
            EKeys::Gamepad_FaceButton_Bottom, SouthReleaseGeneration));
    TestTrue(TEXT("Remapped throw key still keeps the barrier pending"),
        Barrier.IsPending());
    TestEqual(TEXT("Remapped throw release is forwarded while consumed"),
        Barrier.Route(EKeys::LeftMouseButton, IE_Released, true, false),
        EDisposition::ForwardReleaseAndConsume);
    TestTrue(TEXT("Matching remapped-key completion clears the final key"),
        Barrier.CompleteRelease(
            EKeys::LeftMouseButton,
            Barrier.GetReleaseGeneration(EKeys::LeftMouseButton)));
    TestFalse(TEXT("Barrier clears only after every throw key releases"),
        Barrier.IsPending());
    TestEqual(TEXT("A late reconciled Space repeat remains quarantined"),
        Barrier.Route(EKeys::SpaceBar, IE_Repeat, true, false),
        EDisposition::ConsumeHeldEdge);
    TestEqual(TEXT("A duplicate Space release remains quarantined"),
        Barrier.Route(EKeys::SpaceBar, IE_Released, true, false),
        EDisposition::ConsumeHeldEdge);
    TestEqual(TEXT("Next deliberate Space press passes through"),
        Barrier.Route(EKeys::SpaceBar, IE_Pressed, true, false),
        EDisposition::PassThrough);
    TestEqual(TEXT("A deliberate South press exits its prior quarantine"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, true, false),
        EDisposition::PassThrough);
    TestEqual(TEXT("The deliberate South release passes normally"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Released, true, false),
        EDisposition::PassThrough);

    TestEqual(TEXT("A first-seen Repeat still dismisses and arms safely"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Repeat, true, true),
        EDisposition::DismissAndConsume);
    TestEqual(TEXT("South held edge remains consumed"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Repeat, true, false),
        EDisposition::ConsumeHeldEdge);
    TestEqual(TEXT("South release is forwarded while consumed"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Released, true, false),
        EDisposition::ForwardReleaseAndConsume);
    TestTrue(TEXT("South repeat transaction completes by matching generation"),
        Barrier.CompleteRelease(
            EKeys::Gamepad_FaceButton_Bottom,
            Barrier.GetReleaseGeneration(EKeys::Gamepad_FaceButton_Bottom)));
    TestFalse(TEXT("South barrier clears after release evaluation"), Barrier.IsPending());
    TestEqual(TEXT("South reconciled repeat remains quarantined after completion"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Repeat, true, false),
        EDisposition::ConsumeHeldEdge);
    TestEqual(TEXT("A fresh South press exits quarantine"),
        Barrier.Route(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, true, false),
        EDisposition::PassThrough);

    TestEqual(TEXT("Release/re-press race starts from the dismissal press"),
        Barrier.Route(EKeys::SpaceBar, IE_Pressed, true, true),
        EDisposition::DismissAndConsume);
    TestEqual(TEXT("First release queues a generation"),
        Barrier.Route(EKeys::SpaceBar, IE_Released, true, false),
        EDisposition::ForwardReleaseAndConsume);
    const uint32 StaleGeneration =
        Barrier.GetReleaseGeneration(EKeys::SpaceBar);
    TestEqual(TEXT("Re-press before next-tick completion remains fail-closed"),
        Barrier.Route(EKeys::SpaceBar, IE_Pressed, true, false),
        EDisposition::ConsumeHeldEdge);
    TestFalse(TEXT("Stale completion cannot clear a newly down key"),
        Barrier.CompleteRelease(EKeys::SpaceBar, StaleGeneration));
    TestTrue(TEXT("Re-pressed key remains pending"), Barrier.IsPending());
    TestEqual(TEXT("New release queues the replacement generation"),
        Barrier.Route(EKeys::SpaceBar, IE_Released, true, false),
        EDisposition::ForwardReleaseAndConsume);
    const uint32 ReplacementGeneration =
        Barrier.GetReleaseGeneration(EKeys::SpaceBar);
    TestTrue(TEXT("Re-press advances the release generation"),
        ReplacementGeneration != 0 && ReplacementGeneration != StaleGeneration);
    TestTrue(TEXT("Replacement completion clears the barrier"),
        Barrier.CompleteRelease(EKeys::SpaceBar, ReplacementGeneration));
    TestFalse(TEXT("Release/re-press transaction ends neutral"), Barrier.IsPending());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfHoleIntroThrowTimingLifecycleTest,
    "DiscGolfTour.Session19.Input.HoleIntroThrowTimingLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfHoleIntroThrowTimingLifecycleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using EDisposition = EDiscGolfPresentationDismissInputDisposition;

    const auto CanBeginPlayerThrow = [](
        const FDiscGolfPresentationDismissInputBarrier& Barrier,
        bool bFreshThrowDownRequired)
    {
        return DiscGolfGameplayGate::CanCommitPlayerRelease(
            true,  // base launch state is otherwise legal
            false, // main menu
            false, // regression
            false, // lie transition
            true,  // local player controller
            Barrier.IsPending(),
            bFreshThrowDownRequired,
            false, // controls menu
            false, // character creator
            false);// world pause
    };

    const auto BeginTiming = [this](
        const FString& CaseLabel,
        UThrowControllerComponent* ThrowController)
    {
        FThrowCommand Command;
        TestFalse(FString::Printf(TEXT("%s first legal throw press starts rather than commits timing"),
            *CaseLabel), ThrowController->HandleThrowPress(
                TEXT("Apex"), EDiscPlastic::Tour,
                FVector::ForwardVector, FVector::ForwardVector, Command));
        TestTrue(FString::Printf(TEXT("%s first legal throw press owns release timing"),
            *CaseLabel), ThrowController->IsTimingActive());
    };

    struct FSharedConfirmCase
    {
        FKey Key;
        const TCHAR* Label;
    };
    const FSharedConfirmCase SharedConfirmCases[] = {
        {EKeys::SpaceBar, TEXT("Space")},
        {EKeys::Gamepad_FaceButton_Bottom, TEXT("Gamepad South")}
    };

    for (const FSharedConfirmCase& ConfirmCase : SharedConfirmCases)
    {
        const FString Label(ConfirmCase.Label);
        FDiscGolfPresentationDismissInputBarrier Barrier;
        UThrowControllerComponent* ThrowController =
            NewObject<UThrowControllerComponent>();
        bool bFreshThrowDownRequired = false;

        const EDisposition DismissDisposition = Barrier.Route(
            ConfirmCase.Key, IE_Pressed, true, true);
        TestEqual(FString::Printf(TEXT("%s active-intro press dismisses and consumes"),
            *Label), DismissDisposition, EDisposition::DismissAndConsume);
        if (DismissDisposition == EDisposition::DismissAndConsume)
        {
            bFreshThrowDownRequired = true;
        }
        TestFalse(FString::Printf(TEXT("%s dismissal cannot enter timing"), *Label),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        TestFalse(FString::Printf(TEXT("%s dismissal leaves timing inactive"), *Label),
            ThrowController->IsTimingActive());

        TestEqual(FString::Printf(TEXT("%s release remains consumed"), *Label),
            Barrier.Route(ConfirmCase.Key, IE_Released, true, false),
            EDisposition::ForwardReleaseAndConsume);
        const uint32 ReleaseGeneration =
            Barrier.GetReleaseGeneration(ConfirmCase.Key);
        TestTrue(FString::Printf(TEXT("%s release queues a completion generation"),
            *Label), ReleaseGeneration != 0);
        TestFalse(FString::Printf(TEXT("%s release frame remains fail-closed"), *Label),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        TestFalse(FString::Printf(TEXT("%s release frame cannot start timing"), *Label),
            ThrowController->IsTimingActive());

        // This models the controller's first later neutral PostProcessInput
        // frame: the physical key is up, so its matching generation may clear.
        TestTrue(FString::Printf(TEXT("%s first neutral frame completes release"),
            *Label), Barrier.CompleteRelease(
                ConfirmCase.Key, ReleaseGeneration));
        TestFalse(FString::Printf(
            TEXT("%s first neutral frame still requires new throw intent"), *Label),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        TestFalse(FString::Printf(TEXT("%s first neutral frame keeps timing inactive"),
            *Label), ThrowController->IsTimingActive());

        // A second neutral frame must remain stable. Mapping-context rebuilds
        // and late reconciliation have no authority to clear the fresh latch.
        TestFalse(FString::Printf(
            TEXT("%s second neutral frame remains fail-closed"), *Label),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        TestFalse(FString::Printf(TEXT("%s second neutral frame keeps timing inactive"),
            *Label), ThrowController->IsTimingActive());

        const EDisposition FreshDisposition = Barrier.Route(
            ConfirmCase.Key, IE_Pressed, true, false);
        TestEqual(FString::Printf(TEXT("%s second distinct press is fresh"), *Label),
            FreshDisposition, EDisposition::PassThrough);
        if (FreshDisposition == EDisposition::PassThrough
            && !Barrier.IsPending())
        {
            bFreshThrowDownRequired = false;
        }
        TestTrue(FString::Printf(TEXT("%s second distinct press opens the throw gate"),
            *Label), CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        BeginTiming(Label, ThrowController);
    }

    // A naturally expired intro no longer owns shared confirm. The first
    // explicit down edge after the authored 3.25 second window is therefore
    // fresh gameplay intent, even when a prior UI handoff left the latch armed.
    constexpr float AuthoredHoleIntroDurationSeconds = 3.25f;
    constexpr float PostExpiryElapsedSeconds =
        AuthoredHoleIntroDurationSeconds + 0.01f;
    const bool bIntroVisibleAfterExpiry =
        PostExpiryElapsedSeconds < AuthoredHoleIntroDurationSeconds;
    TestFalse(TEXT("The deterministic post-3.25s case has naturally expired"),
        bIntroVisibleAfterExpiry);
    for (const FSharedConfirmCase& ConfirmCase : SharedConfirmCases)
    {
        const FString Label = FString::Printf(TEXT("Expired intro %s"),
            ConfirmCase.Label);
        FDiscGolfPresentationDismissInputBarrier Barrier;
        UThrowControllerComponent* ThrowController =
            NewObject<UThrowControllerComponent>();
        bool bFreshThrowDownRequired = true;
        const EDisposition FirstPostExpiryDisposition = Barrier.Route(
            ConfirmCase.Key, IE_Pressed, true, bIntroVisibleAfterExpiry);
        TestEqual(FString::Printf(TEXT("%s first press passes through"), *Label),
            FirstPostExpiryDisposition, EDisposition::PassThrough);
        if (FirstPostExpiryDisposition == EDisposition::PassThrough
            && !Barrier.IsPending())
        {
            bFreshThrowDownRequired = false;
        }
        TestTrue(FString::Printf(TEXT("%s first press opens the throw gate"), *Label),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        BeginTiming(Label, ThrowController);
    }

    // A remapped throw key that is physically down during dismissal joins the
    // same release transaction. It cannot start timing until it releases,
    // survives two neutral frames, and produces a later distinct down edge.
    {
        FDiscGolfPresentationDismissInputBarrier Barrier;
        UThrowControllerComponent* ThrowController =
            NewObject<UThrowControllerComponent>();
        bool bFreshThrowDownRequired = false;
        const EDisposition DismissDisposition = Barrier.Route(
            EKeys::SpaceBar, IE_Pressed, true, true);
        if (DismissDisposition == EDisposition::DismissAndConsume)
        {
            bFreshThrowDownRequired = true;
        }
        TestEqual(TEXT("Remapped lifecycle begins with a consumed Space dismissal"),
            DismissDisposition, EDisposition::DismissAndConsume);
        TestEqual(TEXT("Held remapped mouse press joins the pending transaction"),
            Barrier.Route(EKeys::LeftMouseButton, IE_Pressed, true, false),
            EDisposition::ConsumeHeldEdge);

        TestEqual(TEXT("Space release remains consumed in remapped transaction"),
            Barrier.Route(EKeys::SpaceBar, IE_Released, true, false),
            EDisposition::ForwardReleaseAndConsume);
        TestTrue(TEXT("Space neutral completion succeeds in remapped transaction"),
            Barrier.CompleteRelease(EKeys::SpaceBar,
                Barrier.GetReleaseGeneration(EKeys::SpaceBar)));
        TestTrue(TEXT("Held remapped key keeps the transaction closed"),
            Barrier.IsPending());
        TestFalse(TEXT("Held remapped key cannot open the throw gate"),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));

        TestEqual(TEXT("Remapped mouse release remains consumed"),
            Barrier.Route(EKeys::LeftMouseButton, IE_Released, true, false),
            EDisposition::ForwardReleaseAndConsume);
        TestTrue(TEXT("Remapped mouse first neutral frame completes release"),
            Barrier.CompleteRelease(EKeys::LeftMouseButton,
                Barrier.GetReleaseGeneration(EKeys::LeftMouseButton)));
        TestFalse(TEXT("Remapped mouse first neutral frame requires fresh intent"),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        TestFalse(TEXT("Remapped mouse second neutral frame remains fail-closed"),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        TestFalse(TEXT("Remapped mouse neutral frames keep timing inactive"),
            ThrowController->IsTimingActive());

        const EDisposition FreshRemappedDisposition = Barrier.Route(
            EKeys::LeftMouseButton, IE_Pressed, true, false);
        TestEqual(TEXT("A later remapped mouse press is distinct gameplay intent"),
            FreshRemappedDisposition, EDisposition::PassThrough);
        if (FreshRemappedDisposition == EDisposition::PassThrough
            && !Barrier.IsPending())
        {
            bFreshThrowDownRequired = false;
        }
        TestTrue(TEXT("Later remapped mouse press opens the throw gate"),
            CanBeginPlayerThrow(Barrier, bFreshThrowDownRequired));
        BeginTiming(TEXT("Remapped mouse"), ThrowController);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnhancedInputFallbackTest,
    "DiscGolfTour.Input.EnhancedInputFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnhancedInputFallbackTest::RunTest(const FString& Parameters)
{
    UDiscGolfInputConfig* Config = UDiscGolfInputConfig::BuildRuntimeFallback(GetTransientPackage());
    TestNotNull(TEXT("Fallback config is created"), Config);
    if (!Config) return false;

    FString MissingField;
    TestTrue(TEXT("Fallback config supplies every required action and context"), Config->IsComplete(MissingField));
    TestEqual(TEXT("Fallback exposes all keyboard, mouse, controller, round, presentation, and developer defaults"), Config->GameplayMappingContext->GetMappings().Num(), 47);
    TestTrue(TEXT("Aim action is one-dimensional"), Config->AimAction->ValueType == EInputActionValueType::Axis1D);
    TestTrue(TEXT("Throw action is boolean"), Config->ThrowAction->ValueType == EInputActionValueType::Boolean);

    TArray<const UInputAction*> OrderedActions;
    Config->GetOrderedActions(OrderedActions);
    TestEqual(TEXT("Controls menu exposes every gameplay, course, round, presentation, and developer action"), OrderedActions.Num(), 22);

    TestTrue(TEXT("A retains negative aim"), HasModifier<UInputModifierNegate>(FindMapping(Config->GameplayMappingContext, Config->AimAction, EKeys::A)));
    TestNotNull(TEXT("D retains positive aim"), FindMapping(Config->GameplayMappingContext, Config->AimAction, EKeys::D));
    TestTrue(TEXT("Left stick aim has an explicit dead zone"), HasModifier<UInputModifierDeadZone>(FindMapping(Config->GameplayMappingContext, Config->AimAction, EKeys::Gamepad_LeftX)));
    TestNotNull(TEXT("Mouse can throw"), FindMapping(Config->GameplayMappingContext, Config->ThrowAction, EKeys::LeftMouseButton));
    TestNotNull(TEXT("Controller can throw"), FindMapping(Config->GameplayMappingContext, Config->ThrowAction, EKeys::Gamepad_FaceButton_Bottom));
    TestNotNull(TEXT("C cycles physics presets"), FindMapping(Config->GameplayMappingContext, Config->CycleRegressionPresetAction, EKeys::C));
    TestNotNull(TEXT("G runs the selected physics preset"), FindMapping(Config->GameplayMappingContext, Config->RunRegressionPresetAction, EKeys::G));
    TestNotNull(TEXT("H runs the physics suite"), FindMapping(Config->GameplayMappingContext, Config->RunRegressionSuiteAction, EKeys::H));
    TestNotNull(TEXT("T toggles the shot tracer"), FindMapping(Config->GameplayMappingContext, Config->ToggleShotTracerAction, EKeys::T));
    TestNotNull(TEXT("V starts or cancels instant replay"), FindMapping(Config->GameplayMappingContext, Config->InstantReplayAction, EKeys::V));
    TestNotNull(TEXT("K toggles the playable course"), FindMapping(Config->GameplayMappingContext, Config->ToggleCourseAction, EKeys::K));
    TestNotNull(TEXT("L starts the authored course flyover"), FindMapping(Config->GameplayMappingContext, Config->CourseFlyoverAction, EKeys::L));
    TestNotNull(TEXT("N advances the authored round"), FindMapping(Config->GameplayMappingContext, Config->NextHoleAction, EKeys::N));
    TestNotNull(TEXT("Tab toggles the scorecard"), FindMapping(Config->GameplayMappingContext, Config->ScorecardAction, EKeys::Tab));
    TestNotNull(TEXT("Right trigger advances the authored round"), FindMapping(Config->GameplayMappingContext, Config->NextHoleAction, EKeys::Gamepad_RightTrigger));
    TestNotNull(TEXT("Left trigger toggles the scorecard"), FindMapping(Config->GameplayMappingContext, Config->ScorecardAction, EKeys::Gamepad_LeftTrigger));
    TestNotNull(TEXT("Left shoulder toggles the shot tracer"), FindMapping(Config->GameplayMappingContext, Config->ToggleShotTracerAction, EKeys::Gamepad_LeftShoulder));
    TestNotNull(TEXT("Right shoulder starts or cancels replay"), FindMapping(Config->GameplayMappingContext, Config->InstantReplayAction, EKeys::Gamepad_RightShoulder));

    for (const FEnhancedActionKeyMapping& Mapping : Config->GameplayMappingContext->GetMappings())
    {
        TestTrue(FString::Printf(TEXT("%s is player mappable"), *Mapping.Key.ToString()), Mapping.IsPlayerMappable());
        TestFalse(FString::Printf(TEXT("%s has a remapping name"), *Mapping.Key.ToString()), Mapping.GetMappingName().IsNone());
    }

    FString CompatibilityReason;
    TestTrue(TEXT("Keyboard button slots accept other keyboard buttons"),
        UDiscGolfInputConfig::IsRemapCandidateCompatible(EKeys::A, EKeys::K, CompatibilityReason));
    TestTrue(TEXT("Keyboard button slots accept mouse buttons"),
        UDiscGolfInputConfig::IsRemapCandidateCompatible(EKeys::SpaceBar, EKeys::LeftMouseButton, CompatibilityReason));
    TestFalse(TEXT("Keyboard slots reject controller inputs"),
        UDiscGolfInputConfig::IsRemapCandidateCompatible(EKeys::A, EKeys::Gamepad_FaceButton_Bottom, CompatibilityReason));
    TestTrue(TEXT("Controller axis slots accept controller axes"),
        UDiscGolfInputConfig::IsRemapCandidateCompatible(EKeys::Gamepad_LeftX, EKeys::Gamepad_RightX, CompatibilityReason));
    TestFalse(TEXT("Controller axis slots reject controller buttons"),
        UDiscGolfInputConfig::IsRemapCandidateCompatible(EKeys::Gamepad_LeftX, EKeys::Gamepad_FaceButton_Bottom, CompatibilityReason));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfEnhancedInputSouthpawFallbackTest,
    "DiscGolfTour.Session12.Input.SouthpawAndDeadZoneFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfEnhancedInputSouthpawFallbackTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UDiscGolfInputConfig* Southpaw = UDiscGolfInputConfig::BuildRuntimeFallback(
        GetTransientPackage(), true, 0.40f);
    TestNotNull(TEXT("Southpaw fallback is created"), Southpaw);
    if (!Southpaw) return false;

    TestNotNull(TEXT("Southpaw aim moves to right stick"), FindMapping(
        Southpaw->GameplayMappingContext, Southpaw->AimAction, EKeys::Gamepad_RightX));
    TestNotNull(TEXT("Southpaw power moves to right stick"), FindMapping(
        Southpaw->GameplayMappingContext, Southpaw->PowerAction, EKeys::Gamepad_RightY));
    TestNotNull(TEXT("Southpaw hyzer moves to left stick"), FindMapping(
        Southpaw->GameplayMappingContext, Southpaw->HyzerAction, EKeys::Gamepad_LeftX));
    TestNotNull(TEXT("Southpaw nose moves to left stick"), FindMapping(
        Southpaw->GameplayMappingContext, Southpaw->NoseAction, EKeys::Gamepad_LeftY));
    TestFalse(TEXT("Default left-stick aim is absent in Southpaw"), FindMapping(
        Southpaw->GameplayMappingContext, Southpaw->AimAction, EKeys::Gamepad_LeftX) != nullptr);
    TestTrue(TEXT("Configured dead zone reaches the remapped axis"), FMath::IsNearlyEqual(
        GetDeadZoneThreshold(FindMapping(
            Southpaw->GameplayMappingContext, Southpaw->AimAction, EKeys::Gamepad_RightX)), 0.40f));
    TestEqual(TEXT("Southpaw preserves the complete fallback mapping count"),
        Southpaw->GameplayMappingContext->GetMappings().Num(), 47);
    return true;
}

#endif
