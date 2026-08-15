#include "DiscGolfInputConfig.h"

#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "PlayerMappableKeySettings.h"

namespace
{
    UDiscGolfRuntimeInputAction* MakeAction(
        UDiscGolfInputConfig* Config,
        const TCHAR* ObjectName,
        EInputActionValueType ValueType,
        const TCHAR* MappingName,
        const TCHAR* DisplayName)
    {
        UDiscGolfRuntimeInputAction* Action = NewObject<UDiscGolfRuntimeInputAction>(Config, ObjectName);
        Action->ValueType = ValueType;
        Action->ActionDescription = FText::FromString(DisplayName);
        Action->ConfigurePlayerMapping(FName(MappingName), FText::FromString(DisplayName));

        if (ValueType != EInputActionValueType::Boolean)
        {
            // Legacy axis mappings accumulated opposite digital keys, so A+D,
            // W+S, and the equivalent pairs continue to cancel each other.
            Action->AccumulationBehavior = EInputActionAccumulationBehavior::Cumulative;
        }
        return Action;
    }

    void MapAxisKey(
        UInputMappingContext* Context,
        const UInputAction* Action,
        const FKey& Key,
        bool bNegate = false,
        bool bDeadZone = false)
    {
        FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
        if (bNegate)
        {
            Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
        }
        if (bDeadZone)
        {
            UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Context);
            DeadZone->LowerThreshold = 0.25f;
            DeadZone->UpperThreshold = 1.0f;
            DeadZone->Type = EDeadZoneType::Axial;
            Mapping.Modifiers.Add(DeadZone);
        }
    }

    void MapButton(UInputMappingContext* Context, const UInputAction* Action, const FKey& Key)
    {
        Context->MapKey(Action, Key);
    }
}

void UDiscGolfRuntimeInputAction::ConfigurePlayerMapping(FName MappingName, const FText& DisplayName)
{
    UPlayerMappableKeySettings* Settings = NewObject<UPlayerMappableKeySettings>(this, TEXT("PlayerMapping"));
    Settings->Name = MappingName;
    Settings->DisplayName = DisplayName;
    Settings->DisplayCategory = FText::FromString(TEXT("Gameplay"));
    PlayerMappableKeySettings = Settings;
}

bool UDiscGolfInputConfig::IsComplete(FString& OutMissingField) const
{
#define REQUIRE_INPUT_FIELD(Field) \
    if (!Field) \
    { \
        OutMissingField = TEXT(#Field); \
        return false; \
    }

    REQUIRE_INPUT_FIELD(GameplayMappingContext);
    REQUIRE_INPUT_FIELD(AimAction);
    REQUIRE_INPUT_FIELD(PowerAction);
    REQUIRE_INPUT_FIELD(HyzerAction);
    REQUIRE_INPUT_FIELD(NoseAction);
    REQUIRE_INPUT_FIELD(ThrowAction);
    REQUIRE_INPUT_FIELD(ToggleThrowStyleAction);
    REQUIRE_INPUT_FIELD(ResetHoleAction);
    REQUIRE_INPUT_FIELD(CycleRegressionPresetAction);
    REQUIRE_INPUT_FIELD(RunRegressionPresetAction);
    REQUIRE_INPUT_FIELD(RunRegressionSuiteAction);
    REQUIRE_INPUT_FIELD(ToggleShotTracerAction);
    REQUIRE_INPUT_FIELD(InstantReplayAction);
    REQUIRE_INPUT_FIELD(ToggleCourseAction);
    REQUIRE_INPUT_FIELD(CourseFlyoverAction);
    REQUIRE_INPUT_FIELD(NextHoleAction);
    REQUIRE_INPUT_FIELD(ScorecardAction);
    REQUIRE_INPUT_FIELD(CyclePlasticAction);
    REQUIRE_INPUT_FIELD(Disc1Action);
    REQUIRE_INPUT_FIELD(Disc2Action);
    REQUIRE_INPUT_FIELD(Disc3Action);
    REQUIRE_INPUT_FIELD(Disc4Action);
    REQUIRE_INPUT_FIELD(Disc5Action);

#undef REQUIRE_INPUT_FIELD

    OutMissingField.Reset();
    return true;
}

void UDiscGolfInputConfig::GetOrderedActions(TArray<const UInputAction*>& OutActions) const
{
    OutActions.Reset(22);
    OutActions.Append({
        AimAction,
        PowerAction,
        HyzerAction,
        NoseAction,
        ThrowAction,
        ToggleThrowStyleAction,
        CyclePlasticAction,
        Disc1Action,
        Disc2Action,
        Disc3Action,
        Disc4Action,
        Disc5Action,
        ResetHoleAction,
        ToggleShotTracerAction,
        InstantReplayAction,
        ToggleCourseAction,
        CourseFlyoverAction,
        NextHoleAction,
        ScorecardAction,
        CycleRegressionPresetAction,
        RunRegressionPresetAction,
        RunRegressionSuiteAction
    });
}

bool UDiscGolfInputConfig::IsRemapCandidateCompatible(
    const FKey& ReferenceKey,
    const FKey& CandidateKey,
    FString& OutReason)
{
    if (!ReferenceKey.IsValid() || !CandidateKey.IsValid() || CandidateKey == EKeys::AnyKey)
    {
        OutReason = TEXT("That input cannot be assigned.");
        return false;
    }

    if (ReferenceKey.IsGamepadKey() != CandidateKey.IsGamepadKey())
    {
        OutReason = ReferenceKey.IsGamepadKey()
            ? TEXT("Choose a controller input for this slot.")
            : TEXT("Choose a keyboard or mouse input for this slot.");
        return false;
    }

    const bool bReferenceAxis = ReferenceKey.IsAxis1D() || ReferenceKey.IsAxis2D() || ReferenceKey.IsAxis3D();
    const bool bCandidateAxis = CandidateKey.IsAxis1D() || CandidateKey.IsAxis2D() || CandidateKey.IsAxis3D();
    if (bReferenceAxis != bCandidateAxis)
    {
        OutReason = bReferenceAxis
            ? TEXT("Move an analog axis for this slot.")
            : TEXT("Press a button or key for this slot.");
        return false;
    }

    if (ReferenceKey.IsAxis1D() != CandidateKey.IsAxis1D()
        || ReferenceKey.IsAxis2D() != CandidateKey.IsAxis2D()
        || ReferenceKey.IsAxis3D() != CandidateKey.IsAxis3D())
    {
        OutReason = TEXT("Choose an input with the same axis type.");
        return false;
    }

    OutReason.Reset();
    return true;
}

UDiscGolfInputConfig* UDiscGolfInputConfig::BuildRuntimeFallback(UObject* Outer)
{
    UObject* SafeOuter = Outer ? Outer : GetTransientPackage();
    UDiscGolfInputConfig* Config = NewObject<UDiscGolfInputConfig>(SafeOuter, TEXT("DiscGolfRuntimeInputConfig"));
    Config->GameplayMappingContext = NewObject<UInputMappingContext>(Config, TEXT("IMC_Gameplay_Runtime"));

    Config->AimAction = MakeAction(Config, TEXT("IA_Aim_Runtime"), EInputActionValueType::Axis1D, TEXT("Aim"), TEXT("Aim"));
    Config->PowerAction = MakeAction(Config, TEXT("IA_Power_Runtime"), EInputActionValueType::Axis1D, TEXT("Power"), TEXT("Power"));
    Config->HyzerAction = MakeAction(Config, TEXT("IA_Hyzer_Runtime"), EInputActionValueType::Axis1D, TEXT("Hyzer"), TEXT("Hyzer / Anhyzer"));
    Config->NoseAction = MakeAction(Config, TEXT("IA_Nose_Runtime"), EInputActionValueType::Axis1D, TEXT("Nose"), TEXT("Nose Angle"));
    Config->ThrowAction = MakeAction(Config, TEXT("IA_Throw_Runtime"), EInputActionValueType::Boolean, TEXT("Throw"), TEXT("Throw / Release"));
    Config->ToggleThrowStyleAction = MakeAction(Config, TEXT("IA_ToggleThrowStyle_Runtime"), EInputActionValueType::Boolean, TEXT("ToggleThrowStyle"), TEXT("Toggle Throw Style"));
    Config->ResetHoleAction = MakeAction(Config, TEXT("IA_ResetHole_Runtime"), EInputActionValueType::Boolean, TEXT("ResetHole"), TEXT("Reset Hole"));
    Config->CycleRegressionPresetAction = MakeAction(Config, TEXT("IA_CycleRegressionPreset_Runtime"), EInputActionValueType::Boolean, TEXT("CycleRegressionPreset"), TEXT("Cycle Physics Preset"));
    Config->RunRegressionPresetAction = MakeAction(Config, TEXT("IA_RunRegressionPreset_Runtime"), EInputActionValueType::Boolean, TEXT("RunRegressionPreset"), TEXT("Run Physics Preset"));
    Config->RunRegressionSuiteAction = MakeAction(Config, TEXT("IA_RunRegressionSuite_Runtime"), EInputActionValueType::Boolean, TEXT("RunRegressionSuite"), TEXT("Run Physics Suite"));
    Config->ToggleShotTracerAction = MakeAction(Config, TEXT("IA_ToggleShotTracer_Runtime"), EInputActionValueType::Boolean, TEXT("ToggleShotTracer"), TEXT("Toggle Shot Tracer"));
    Config->InstantReplayAction = MakeAction(Config, TEXT("IA_InstantReplay_Runtime"), EInputActionValueType::Boolean, TEXT("InstantReplay"), TEXT("Instant Replay / Cancel"));
    Config->ToggleCourseAction = MakeAction(Config, TEXT("IA_ToggleCourse_Runtime"), EInputActionValueType::Boolean, TEXT("ToggleCourse"), TEXT("Toggle Course"));
    Config->CourseFlyoverAction = MakeAction(Config, TEXT("IA_CourseFlyover_Runtime"), EInputActionValueType::Boolean, TEXT("CourseFlyover"), TEXT("Course Flyover"));
    Config->NextHoleAction = MakeAction(Config, TEXT("IA_NextHole_Runtime"), EInputActionValueType::Boolean, TEXT("NextHole"), TEXT("Next Hole / Restart Round"));
    Config->ScorecardAction = MakeAction(Config, TEXT("IA_Scorecard_Runtime"), EInputActionValueType::Boolean, TEXT("Scorecard"), TEXT("Toggle Scorecard"));
    Config->CyclePlasticAction = MakeAction(Config, TEXT("IA_CyclePlastic_Runtime"), EInputActionValueType::Boolean, TEXT("CyclePlastic"), TEXT("Cycle Plastic"));
    Config->Disc1Action = MakeAction(Config, TEXT("IA_Disc1_Runtime"), EInputActionValueType::Boolean, TEXT("Disc1"), TEXT("Select Disc 1"));
    Config->Disc2Action = MakeAction(Config, TEXT("IA_Disc2_Runtime"), EInputActionValueType::Boolean, TEXT("Disc2"), TEXT("Select Disc 2"));
    Config->Disc3Action = MakeAction(Config, TEXT("IA_Disc3_Runtime"), EInputActionValueType::Boolean, TEXT("Disc3"), TEXT("Select Disc 3"));
    Config->Disc4Action = MakeAction(Config, TEXT("IA_Disc4_Runtime"), EInputActionValueType::Boolean, TEXT("Disc4"), TEXT("Select Disc 4"));
    Config->Disc5Action = MakeAction(Config, TEXT("IA_Disc5_Runtime"), EInputActionValueType::Boolean, TEXT("Disc5"), TEXT("Select Disc 5"));

    // Preserve every existing keyboard mapping.
    MapAxisKey(Config->GameplayMappingContext, Config->AimAction, EKeys::A, true);
    MapAxisKey(Config->GameplayMappingContext, Config->AimAction, EKeys::D);
    MapAxisKey(Config->GameplayMappingContext, Config->PowerAction, EKeys::W);
    MapAxisKey(Config->GameplayMappingContext, Config->PowerAction, EKeys::S, true);
    MapAxisKey(Config->GameplayMappingContext, Config->HyzerAction, EKeys::Q, true);
    MapAxisKey(Config->GameplayMappingContext, Config->HyzerAction, EKeys::E);
    MapAxisKey(Config->GameplayMappingContext, Config->NoseAction, EKeys::Z, true);
    MapAxisKey(Config->GameplayMappingContext, Config->NoseAction, EKeys::X);

    MapButton(Config->GameplayMappingContext, Config->ThrowAction, EKeys::SpaceBar);
    MapButton(Config->GameplayMappingContext, Config->ToggleThrowStyleAction, EKeys::F);
    MapButton(Config->GameplayMappingContext, Config->ResetHoleAction, EKeys::R);
    MapButton(Config->GameplayMappingContext, Config->CycleRegressionPresetAction, EKeys::C);
    MapButton(Config->GameplayMappingContext, Config->RunRegressionPresetAction, EKeys::G);
    MapButton(Config->GameplayMappingContext, Config->RunRegressionSuiteAction, EKeys::H);
    MapButton(Config->GameplayMappingContext, Config->ToggleShotTracerAction, EKeys::T);
    MapButton(Config->GameplayMappingContext, Config->InstantReplayAction, EKeys::V);
    MapButton(Config->GameplayMappingContext, Config->ToggleCourseAction, EKeys::K);
    MapButton(Config->GameplayMappingContext, Config->CourseFlyoverAction, EKeys::L);
    MapButton(Config->GameplayMappingContext, Config->NextHoleAction, EKeys::N);
    MapButton(Config->GameplayMappingContext, Config->ScorecardAction, EKeys::Tab);
    MapButton(Config->GameplayMappingContext, Config->CyclePlasticAction, EKeys::P);
    MapButton(Config->GameplayMappingContext, Config->Disc1Action, EKeys::One);
    MapButton(Config->GameplayMappingContext, Config->Disc2Action, EKeys::Two);
    MapButton(Config->GameplayMappingContext, Config->Disc3Action, EKeys::Three);
    MapButton(Config->GameplayMappingContext, Config->Disc4Action, EKeys::Four);
    MapButton(Config->GameplayMappingContext, Config->Disc5Action, EKeys::Five);

    // Mouse buttons supplement the keyboard without changing shot setup feel.
    MapButton(Config->GameplayMappingContext, Config->ThrowAction, EKeys::LeftMouseButton);
    MapButton(Config->GameplayMappingContext, Config->ToggleThrowStyleAction, EKeys::RightMouseButton);

    // Controller layout: sticks configure the shot; face/D-pad buttons handle
    // discrete actions. Explicit dead zones avoid dependence on legacy axes.
    MapAxisKey(Config->GameplayMappingContext, Config->AimAction, EKeys::Gamepad_LeftX, false, true);
    MapAxisKey(Config->GameplayMappingContext, Config->PowerAction, EKeys::Gamepad_LeftY, false, true);
    MapAxisKey(Config->GameplayMappingContext, Config->HyzerAction, EKeys::Gamepad_RightX, false, true);
    MapAxisKey(Config->GameplayMappingContext, Config->NoseAction, EKeys::Gamepad_RightY, false, true);
    MapButton(Config->GameplayMappingContext, Config->ThrowAction, EKeys::Gamepad_FaceButton_Bottom);
    MapButton(Config->GameplayMappingContext, Config->ToggleThrowStyleAction, EKeys::Gamepad_FaceButton_Left);
    MapButton(Config->GameplayMappingContext, Config->CyclePlasticAction, EKeys::Gamepad_FaceButton_Right);
    MapButton(Config->GameplayMappingContext, Config->ResetHoleAction, EKeys::Gamepad_Special_Right);
    MapButton(Config->GameplayMappingContext, Config->ToggleShotTracerAction, EKeys::Gamepad_LeftShoulder);
    MapButton(Config->GameplayMappingContext, Config->InstantReplayAction, EKeys::Gamepad_RightShoulder);
    MapButton(Config->GameplayMappingContext, Config->ToggleCourseAction, EKeys::Gamepad_LeftThumbstick);
    MapButton(Config->GameplayMappingContext, Config->CourseFlyoverAction, EKeys::Gamepad_RightThumbstick);
    MapButton(Config->GameplayMappingContext, Config->NextHoleAction, EKeys::Gamepad_RightTrigger);
    MapButton(Config->GameplayMappingContext, Config->ScorecardAction, EKeys::Gamepad_LeftTrigger);
    MapButton(Config->GameplayMappingContext, Config->Disc1Action, EKeys::Gamepad_DPad_Up);
    MapButton(Config->GameplayMappingContext, Config->Disc2Action, EKeys::Gamepad_DPad_Right);
    MapButton(Config->GameplayMappingContext, Config->Disc3Action, EKeys::Gamepad_DPad_Down);
    MapButton(Config->GameplayMappingContext, Config->Disc4Action, EKeys::Gamepad_DPad_Left);
    MapButton(Config->GameplayMappingContext, Config->Disc5Action, EKeys::Gamepad_FaceButton_Top);

    return Config;
}
