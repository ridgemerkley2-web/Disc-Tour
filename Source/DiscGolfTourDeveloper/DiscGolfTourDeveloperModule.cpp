#include "DiscGolfSession3SmokeRunner.h"
#include "DiscGolfSession3VisualCaptureRunner.h"
#include "DiscGolfSession4VisualCaptureRunner.h"
#include "DiscGolfSession5MocapSmokeRunner.h"
#include "DiscGolfSession5MocapVisualCaptureRunner.h"
#include "DiscGolfSession6OutfitSmokeRunner.h"
#include "DiscGolfSession6OutfitVisualCaptureRunner.h"
#include "DiscGolfSession7FullCharacterSmokeRunner.h"
#include "DiscGolfSession7FullCharacterVisualCaptureRunner.h"
#include "DiscGolfSession8CookClosureRunner.h"
#include "DiscGolfSession8BMetaHumanPackagedRunner.h"
#include "DiscGolfSession14AISmokeRunner.h"
#include "DiscGolfSession15VerticalSliceRunner.h"
#include "DiscGolfLevelDesignReviewActor.h"
#include "DiscGolfTourGameMode.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Modules/ModuleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDiscGolfTourDeveloper, Log, All);

namespace DiscGolfTourDeveloper
{
    enum class ERunner : uint8
    {
        None,
        Session3Smoke,
        Session3Visual,
        Session4Visual,
        Session5MocapSmoke,
        Session5MocapVisual,
        Session6OutfitSmoke,
        Session6OutfitVisual,
        Session7CharacterSmoke,
        Session7CharacterVisual,
        Session8CookClosure,
        Session8BPackaged,
        Session14AI,
        Session15VerticalSlice
    };

    enum class EDeveloperCommand : uint8
    {
        RunSelectedRegression,
        RunRegressionSuite,
        RunRulesFixture,
        LoadCourse,
        ToggleCourse,
        LoadHole,
        ToggleTracer,
        CapturePerformance,
        ResetPerformance,
        ToggleDeveloperHud,
        ToggleLevelDesignReview,
        StartNeedleGateTelemetry,
        ResetNeedleGateTelemetry,
        SelectTelemetryRoute,
        TelemetryTradeoffUnderstood,
        TelemetryNextShotClear,
        SaveRouteTelemetry,
        StopRouteTelemetry
    };

    static ERunner ParseCommandLine()
    {
        const TCHAR* CommandLine = FCommandLine::Get();
        if (FParse::Param(CommandLine, TEXT("Session15VerticalSliceSmokeTest")))
        {
            return ERunner::Session15VerticalSlice;
        }
        if (FParse::Param(CommandLine, TEXT("Session14AIGolferSmokeTest")))
        {
            return ERunner::Session14AI;
        }
        if (FParse::Param(CommandLine, TEXT("Session8BMetaHumanPackagedAcceptance")))
        {
            return ERunner::Session8BPackaged;
        }
        if (FParse::Param(CommandLine, TEXT("Session8CookClosureSmokeTest")))
        {
            return ERunner::Session8CookClosure;
        }
        if (FParse::Param(CommandLine, TEXT("Session7FullCharacterVisualCapture")))
        {
            return ERunner::Session7CharacterVisual;
        }
        if (FParse::Param(CommandLine, TEXT("Session7FullCharacterThrowSmokeTest")))
        {
            return ERunner::Session7CharacterSmoke;
        }
        if (FParse::Param(CommandLine, TEXT("Session6OutfitVisualCapture")))
        {
            return ERunner::Session6OutfitVisual;
        }
        if (FParse::Param(CommandLine, TEXT("Session6OutfitThrowSmokeTest")))
        {
            return ERunner::Session6OutfitSmoke;
        }
        if (FParse::Param(CommandLine, TEXT("Session5MocapVisualCapture")))
        {
            return ERunner::Session5MocapVisual;
        }
        if (FParse::Param(CommandLine, TEXT("Session5MocapPipelineSmokeTest")))
        {
            return ERunner::Session5MocapSmoke;
        }
        if (FParse::Param(CommandLine, TEXT("Session4VisualCapture")))
        {
            return ERunner::Session4Visual;
        }
        if (FParse::Param(CommandLine, TEXT("Session3VisualCapture")))
        {
            return ERunner::Session3Visual;
        }
        if (FParse::Param(CommandLine, TEXT("Session3OneThrowSmokeTest")))
        {
            return ERunner::Session3Smoke;
        }
        return ERunner::None;
    }

    static bool ParseConsoleRunner(const FString& Name, ERunner& OutRunner)
    {
        const TPair<const TCHAR*, ERunner> Values[] =
        {
            { TEXT("session3-smoke"), ERunner::Session3Smoke },
            { TEXT("session3-visual"), ERunner::Session3Visual },
            { TEXT("session4-visual"), ERunner::Session4Visual },
            { TEXT("session5-mocap-smoke"), ERunner::Session5MocapSmoke },
            { TEXT("session5-mocap-visual"), ERunner::Session5MocapVisual },
            { TEXT("session6-outfit-smoke"), ERunner::Session6OutfitSmoke },
            { TEXT("session6-outfit-visual"), ERunner::Session6OutfitVisual },
            { TEXT("session7-character-smoke"), ERunner::Session7CharacterSmoke },
            { TEXT("session7-character-visual"), ERunner::Session7CharacterVisual },
            { TEXT("session8-cook-closure"), ERunner::Session8CookClosure },
            { TEXT("session8b-packaged"), ERunner::Session8BPackaged },
            { TEXT("session14-ai"), ERunner::Session14AI },
            { TEXT("session15-vertical-slice"), ERunner::Session15VerticalSlice }
        };
        for (const TPair<const TCHAR*, ERunner>& Value : Values)
        {
            if (Name.Equals(Value.Key, ESearchCase::IgnoreCase))
            {
                OutRunner = Value.Value;
                return true;
            }
        }
        return false;
    }

    template <typename TRunner>
    static void SpawnAndStart(UWorld& World)
    {
        if (TRunner* Runner = World.SpawnActor<TRunner>())
        {
            Runner->Start();
            return;
        }
        UE_LOG(LogDiscGolfTourDeveloper, Error,
            TEXT("Developer validation runner could not spawn."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
    }

    static void Run(UWorld& World, ERunner Runner)
    {
        switch (Runner)
        {
        case ERunner::Session3Smoke:
            SpawnAndStart<ADiscGolfSession3SmokeRunner>(World);
            break;
        case ERunner::Session3Visual:
            SpawnAndStart<ADiscGolfSession3VisualCaptureRunner>(World);
            break;
        case ERunner::Session4Visual:
            SpawnAndStart<ADiscGolfSession4VisualCaptureRunner>(World);
            break;
        case ERunner::Session5MocapSmoke:
            SpawnAndStart<ADiscGolfSession5MocapSmokeRunner>(World);
            break;
        case ERunner::Session5MocapVisual:
            SpawnAndStart<ADiscGolfSession5MocapVisualCaptureRunner>(World);
            break;
        case ERunner::Session6OutfitSmoke:
            SpawnAndStart<ADiscGolfSession6OutfitSmokeRunner>(World);
            break;
        case ERunner::Session6OutfitVisual:
            SpawnAndStart<ADiscGolfSession6OutfitVisualCaptureRunner>(World);
            break;
        case ERunner::Session7CharacterSmoke:
            SpawnAndStart<ADiscGolfSession7FullCharacterSmokeRunner>(World);
            break;
        case ERunner::Session7CharacterVisual:
            SpawnAndStart<ADiscGolfSession7FullCharacterVisualCaptureRunner>(World);
            break;
        case ERunner::Session8CookClosure:
            SpawnAndStart<ADiscGolfSession8CookClosureRunner>(World);
            break;
        case ERunner::Session8BPackaged:
            SpawnAndStart<ADiscGolfSession8BMetaHumanPackagedRunner>(World);
            break;
        case ERunner::Session14AI:
            SpawnAndStart<ADiscGolfSession14AISmokeRunner>(World);
            break;
        case ERunner::Session15VerticalSlice:
            SpawnAndStart<ADiscGolfSession15VerticalSliceRunner>(World);
            break;
        default:
            break;
        }
    }

    static bool UsesPineRidge(ERunner Runner)
    {
        return Runner != ERunner::None && Runner != ERunner::Session8CookClosure;
    }

    static bool UsesPerformanceProfile(ERunner Runner)
    {
        return Runner == ERunner::Session8BPackaged
            || Runner == ERunner::Session15VerticalSlice;
    }

    static bool SuppressesSave(ERunner Runner)
    {
        return Runner == ERunner::Session6OutfitSmoke
            || Runner == ERunner::Session6OutfitVisual
            || Runner == ERunner::Session7CharacterSmoke
            || Runner == ERunner::Session7CharacterVisual
            || Runner == ERunner::Session8CookClosure
            || Runner == ERunner::Session8BPackaged;
    }

    static void AppendRuntimeModeFlags(ERunner Runner)
    {
        if (Runner == ERunner::None)
        {
            return;
        }
        FCommandLine::Append(TEXT(" -DGDeveloperToolAutomation"));
        if (UsesPineRidge(Runner))
        {
            FCommandLine::Append(TEXT(" -DGDeveloperToolPineRidge"));
        }
        if (UsesPerformanceProfile(Runner))
        {
            FCommandLine::Append(TEXT(" -DGDeveloperToolPerformanceProfile"));
        }
        if (Runner == ERunner::Session8BPackaged)
        {
            FCommandLine::Append(TEXT(" -DGDeveloperToolExclusiveCapture"));
        }
        if (SuppressesSave(Runner))
        {
            FCommandLine::Append(TEXT(" -DGDeveloperToolNoSave"));
        }
    }
}

class FDiscGolfTourDeveloperModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        using namespace DiscGolfTourDeveloper;
        RequestedRunner = ParseCommandLine();
        AppendRuntimeModeFlags(RequestedRunner);
        WorldInitializedHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(
            this, &FDiscGolfTourDeveloperModule::HandleWorldInitialized);
        ConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
            TEXT("DGT.Developer.Run"),
            TEXT("Run a non-Shipping validation runner. Argument: session3-smoke, "
                "session3-visual, session4-visual, session5-mocap-smoke, "
                "session5-mocap-visual, session6-outfit-smoke, session6-outfit-visual, "
                "session7-character-smoke, session7-character-visual, "
                "session8-cook-closure, session8b-packaged, session14-ai, "
                "or session15-vertical-slice."),
            FConsoleCommandWithWorldAndArgsDelegate::CreateRaw(
                this, &FDiscGolfTourDeveloperModule::HandleConsoleCommand),
            ECVF_Cheat);
        RegisterDeveloperCommands();
    }

    virtual void ShutdownModule() override
    {
        if (WorldInitializedHandle.IsValid())
        {
            FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitializedHandle);
            WorldInitializedHandle.Reset();
        }
        if (ConsoleCommand)
        {
            IConsoleManager::Get().UnregisterConsoleObject(ConsoleCommand);
            ConsoleCommand = nullptr;
        }
        for (IConsoleObject* DeveloperCommand : DeveloperCommands)
        {
            if (DeveloperCommand)
            {
                IConsoleManager::Get().UnregisterConsoleObject(DeveloperCommand);
            }
        }
        DeveloperCommands.Reset();
    }

private:
    void HandleWorldInitialized(UWorld* World, const UWorld::InitializationValues)
    {
        using namespace DiscGolfTourDeveloper;
        if (RequestedRunner == ERunner::None || !World || !World->IsGameWorld())
        {
            return;
        }
        Schedule(*World, RequestedRunner, 0.5f);
    }

    void HandleConsoleCommand(const TArray<FString>& Args, UWorld* World)
    {
        using namespace DiscGolfTourDeveloper;
        ERunner Runner = ERunner::None;
        if (!World || Args.Num() != 1 || !ParseConsoleRunner(Args[0], Runner))
        {
            UE_LOG(LogDiscGolfTourDeveloper, Error,
                TEXT("DGT.Developer.Run requires one recognized runner argument in a game world."));
            return;
        }
        Schedule(*World, Runner, 0.01f);
    }

    void RegisterDeveloperCommands()
    {
        using DiscGolfTourDeveloper::EDeveloperCommand;
        RegisterDeveloperCommand(TEXT("DGT_RunSelectedRegression"),
            TEXT("Run the selected development physics regression."),
            EDeveloperCommand::RunSelectedRegression);
        RegisterDeveloperCommand(TEXT("DGT_RunRegressionSuite"),
            TEXT("Run the development physics regression suite."),
            EDeveloperCommand::RunRegressionSuite);
        RegisterDeveloperCommand(TEXT("DGT_RunRulesFixture"),
            TEXT("Run one named development rules fixture."),
            EDeveloperCommand::RunRulesFixture);
        RegisterDeveloperCommand(TEXT("DGT_LoadCourse"),
            TEXT("Load one named development course."),
            EDeveloperCommand::LoadCourse);
        RegisterDeveloperCommand(TEXT("DGT_ToggleCourse"),
            TEXT("Toggle the development course."),
            EDeveloperCommand::ToggleCourse);
        RegisterDeveloperCommand(TEXT("DGT_LoadHole"),
            TEXT("Load one numbered development hole."),
            EDeveloperCommand::LoadHole);
        RegisterDeveloperCommand(TEXT("DGT_ToggleTracer"),
            TEXT("Toggle the development shot tracer."),
            EDeveloperCommand::ToggleTracer);
        RegisterDeveloperCommand(TEXT("DGT_CapturePerformance"),
            TEXT("Capture a development performance snapshot."),
            EDeveloperCommand::CapturePerformance);
        RegisterDeveloperCommand(TEXT("DGT_ResetPerformanceTelemetry"),
            TEXT("Reset development performance telemetry."),
            EDeveloperCommand::ResetPerformance);
        RegisterDeveloperCommand(TEXT("DGT_ToggleDeveloperHud"),
            TEXT("Toggle the development HUD."),
            EDeveloperCommand::ToggleDeveloperHud);
        RegisterDeveloperCommand(TEXT("DGT_ToggleLevelDesignReview"),
            TEXT("Toggle the level-design review overlay."),
            EDeveloperCommand::ToggleLevelDesignReview);
        RegisterDeveloperCommand(TEXT("DGT_StartNeedleGateTelemetry"),
            TEXT("Start Needle Gate route telemetry."),
            EDeveloperCommand::StartNeedleGateTelemetry);
        RegisterDeveloperCommand(TEXT("DGT_ResetNeedleGateTelemetry"),
            TEXT("Reset and start Needle Gate route telemetry."),
            EDeveloperCommand::ResetNeedleGateTelemetry);
        RegisterDeveloperCommand(TEXT("DGT_SelectTelemetryRoute"),
            TEXT("Select one named route telemetry path."),
            EDeveloperCommand::SelectTelemetryRoute);
        RegisterDeveloperCommand(TEXT("DGT_TelemetryTradeoffUnderstood"),
            TEXT("Set route-tradeoff acknowledgement to 0 or 1."),
            EDeveloperCommand::TelemetryTradeoffUnderstood);
        RegisterDeveloperCommand(TEXT("DGT_TelemetryNextShotClear"),
            TEXT("Set next-shot clarity to 0 or 1."),
            EDeveloperCommand::TelemetryNextShotClear);
        RegisterDeveloperCommand(TEXT("DGT_SaveRouteTelemetry"),
            TEXT("Save the active route telemetry record."),
            EDeveloperCommand::SaveRouteTelemetry);
        RegisterDeveloperCommand(TEXT("DGT_StopRouteTelemetry"),
            TEXT("Stop route telemetry."),
            EDeveloperCommand::StopRouteTelemetry);
    }

    void RegisterDeveloperCommand(
        const TCHAR* Name,
        const TCHAR* Help,
        DiscGolfTourDeveloper::EDeveloperCommand Command)
    {
        DeveloperCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
            Name,
            Help,
            FConsoleCommandWithWorldAndArgsDelegate::CreateLambda(
                [this, Command](const TArray<FString>& Args, UWorld* World)
                {
                    HandleDeveloperCommand(Command, Args, World);
                }),
            ECVF_Cheat));
    }

    void HandleDeveloperCommand(
        DiscGolfTourDeveloper::EDeveloperCommand Command,
        const TArray<FString>& Args,
        UWorld* World)
    {
        using DiscGolfTourDeveloper::EDeveloperCommand;
        ADiscGolfTourGameMode* GameMode = World
            ? World->GetAuthGameMode<ADiscGolfTourGameMode>() : nullptr;
        if (!GameMode)
        {
            UE_LOG(LogDiscGolfTourDeveloper, Error,
                TEXT("Developer command requires the authoritative DiscGolfTour game mode."));
            return;
        }
        const auto RequireNoArgs = [&Args]() { return Args.IsEmpty(); };
        const auto RequireText = [&Args](FString& OutValue)
        {
            if (Args.IsEmpty()) return false;
            OutValue = FString::Join(Args, TEXT(" "));
            return !OutValue.IsEmpty();
        };
        const auto RequireInt = [&Args](int32& OutValue)
        {
            return Args.Num() == 1 && LexTryParseString(OutValue, *Args[0]);
        };

        bool bArgumentsValid = true;
        switch (Command)
        {
        case EDeveloperCommand::RunSelectedRegression:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->RunSelectedPhysicsRegression();
            break;
        case EDeveloperCommand::RunRegressionSuite:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->RunPhysicsRegressionSuite();
            break;
        case EDeveloperCommand::RunRulesFixture:
        {
            FString Value;
            bArgumentsValid = RequireText(Value);
            if (bArgumentsValid) GameMode->RunCourseRulesFixture(Value);
            break;
        }
        case EDeveloperCommand::LoadCourse:
        {
            FString Value;
            bArgumentsValid = RequireText(Value);
            if (bArgumentsValid) GameMode->LoadCourse(Value);
            break;
        }
        case EDeveloperCommand::ToggleCourse:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->ToggleCourse();
            break;
        case EDeveloperCommand::LoadHole:
        {
            int32 Value = 0;
            bArgumentsValid = RequireInt(Value);
            if (bArgumentsValid) GameMode->LoadRoundHole(Value);
            break;
        }
        case EDeveloperCommand::ToggleTracer:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->ToggleShotTracer();
            break;
        case EDeveloperCommand::CapturePerformance:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->CapturePerformanceSnapshot();
            break;
        case EDeveloperCommand::ResetPerformance:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->ResetPerformanceTelemetry();
            break;
        case EDeveloperCommand::ToggleDeveloperHud:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->ToggleDeveloperHud();
            break;
        case EDeveloperCommand::ToggleLevelDesignReview:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid)
            {
                ADiscGolfLevelDesignReviewActor* FirstReviewActor = nullptr;
                for (TActorIterator<ADiscGolfLevelDesignReviewActor> It(World); It; ++It)
                {
                    if (!FirstReviewActor)
                    {
                        FirstReviewActor = *It;
                    }
                }
                if (!FirstReviewActor)
                {
                    UE_LOG(LogDiscGolfTourDeveloper, Warning,
                        TEXT("DGT_ToggleLevelDesignReview requires a developer review actor in the world."));
                    break;
                }
                const bool bVisible = !FirstReviewActor->IsReviewVisible();
                for (TActorIterator<ADiscGolfLevelDesignReviewActor> It(World); It; ++It)
                {
                    It->SetReviewVisible(bVisible);
                }
            }
            break;
        case EDeveloperCommand::StartNeedleGateTelemetry:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->StartNeedleGateRouteTelemetry(false);
            break;
        case EDeveloperCommand::ResetNeedleGateTelemetry:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->StartNeedleGateRouteTelemetry(true);
            break;
        case EDeveloperCommand::SelectTelemetryRoute:
        {
            FString Value;
            bArgumentsValid = RequireText(Value);
            if (bArgumentsValid) GameMode->SelectRouteTelemetry(Value);
            break;
        }
        case EDeveloperCommand::TelemetryTradeoffUnderstood:
        {
            int32 Value = 0;
            bArgumentsValid = RequireInt(Value);
            if (bArgumentsValid) GameMode->SetRouteTelemetryTradeoffUnderstood(Value);
            break;
        }
        case EDeveloperCommand::TelemetryNextShotClear:
        {
            int32 Value = 0;
            bArgumentsValid = RequireInt(Value);
            if (bArgumentsValid) GameMode->SetRouteTelemetryNextShotClear(Value);
            break;
        }
        case EDeveloperCommand::SaveRouteTelemetry:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->SaveRouteTelemetry();
            break;
        case EDeveloperCommand::StopRouteTelemetry:
            bArgumentsValid = RequireNoArgs();
            if (bArgumentsValid) GameMode->StopRouteTelemetry();
            break;
        default:
            bArgumentsValid = false;
            break;
        }
        if (!bArgumentsValid)
        {
            UE_LOG(LogDiscGolfTourDeveloper, Error,
                TEXT("Developer command arguments are invalid."));
        }
    }

    static void Schedule(UWorld& World, DiscGolfTourDeveloper::ERunner Runner, float DelaySeconds)
    {
        const TWeakObjectPtr<UWorld> WeakWorld(&World);
        FTimerHandle TimerHandle;
        World.GetTimerManager().SetTimer(
            TimerHandle,
            FTimerDelegate::CreateLambda([WeakWorld, Runner]()
            {
                if (UWorld* ResolvedWorld = WeakWorld.Get())
                {
                    DiscGolfTourDeveloper::Run(*ResolvedWorld, Runner);
                }
            }),
            DelaySeconds,
            false);
    }

    DiscGolfTourDeveloper::ERunner RequestedRunner = DiscGolfTourDeveloper::ERunner::None;
    FDelegateHandle WorldInitializedHandle;
    IConsoleObject* ConsoleCommand = nullptr;
    TArray<IConsoleObject*> DeveloperCommands;
};

IMPLEMENT_MODULE(FDiscGolfTourDeveloperModule, DiscGolfTourDeveloper)
