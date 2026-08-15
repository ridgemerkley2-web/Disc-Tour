#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfInputConfig.h"
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

#endif
