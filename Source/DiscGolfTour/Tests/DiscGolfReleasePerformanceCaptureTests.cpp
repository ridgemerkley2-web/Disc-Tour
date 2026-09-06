#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../DiscGolfTourGameMode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfReleasePerformanceCaptureLaunchTest,
    "DiscGolfTour.Session19.ReleasePerformanceCapture.LaunchGuards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfReleasePerformanceCaptureLaunchTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FString Token = TEXT("11111111-2222-4333-8444-555555555555");
    const FString ExternalUserDir = TEXT("C:/DGTourExternal/") + Token;
    const FString ProjectDirectory = TEXT("C:/DGTour");
    const FString RootDirectory = TEXT("C:/DGTour_Packages/Candidate/Windows");
    const FString ValidCommand = FString::Printf(
        TEXT("-Course=PineRidge -Hole=2 -PerformanceCaptureSeconds=30 ")
        TEXT("-ResX=1920 -ResY=1080 -ForceRes -RenderOffscreen -dx12 ")
        TEXT("-unattended -NoLoadExistingSave -DGNoProfileWrites -UserDir=%s"),
        *ExternalUserDir);

    const auto Validate = [&](const FString& CommandLine, bool bUnattended = true,
                              const FString& Resolved = FString(),
                              bool bCaptureDirectoryExists = false,
                              bool bSaveGameDirectoryExists = false)
    {
        return ADiscGolfTourGameMode::ValidateReleasePerformanceCaptureLaunch(
            CommandLine,
            bUnattended,
            Resolved.IsEmpty() ? ExternalUserDir : Resolved,
            ProjectDirectory,
            RootDirectory,
            bCaptureDirectoryExists,
            bSaveGameDirectoryExists);
    };

    const FDiscGolfReleasePerformanceCaptureLaunchValidation Accepted = Validate(ValidCommand);
    TestTrue(TEXT("exact isolated Shipping capture launch is accepted"), Accepted.bAttempted);
    TestTrue(TEXT("exact isolated Shipping capture guards pass"), Accepted.bAccepted);
    TestEqual(TEXT("requested hole is preserved"), Accepted.HoleNumber, 2);
    TestEqual(TEXT("duration is fixed at thirty seconds"), Accepted.DurationSeconds, 30.0f);
    TestEqual(TEXT("external UUID token is preserved"), Accepted.UserDirToken, Token);

    const FDiscGolfReleasePerformanceCaptureLaunchValidation NormalLaunch = Validate(
        TEXT("-Course=PineRidge -Hole=1 -ResX=1920 -ResY=1080"));
    TestFalse(TEXT("normal launch is not a capture attempt"), NormalLaunch.bAttempted);
    TestFalse(TEXT("normal launch is never promoted to capture"), NormalLaunch.bAccepted);
    TestTrue(TEXT("Development preserves public performance telemetry mutation"),
        ADiscGolfTourGameMode::IsPublicPerformanceCaptureMutationAllowed(true));
    TestFalse(TEXT("Shipping rejects public performance telemetry mutation"),
        ADiscGolfTourGameMode::IsPublicPerformanceCaptureMutationAllowed(false));

    TestFalse(TEXT("FApp unattended state is independently required"),
        Validate(ValidCommand, false).bAccepted);

    const TArray<FString> RequiredArguments = {
        TEXT("-Course=PineRidge"),
        TEXT("-Hole=2"),
        TEXT("-PerformanceCaptureSeconds=30"),
        TEXT("-ResX=1920"),
        TEXT("-ResY=1080"),
        TEXT("-ForceRes"),
        TEXT("-RenderOffscreen"),
        TEXT("-dx12"),
        TEXT("-unattended"),
        TEXT("-NoLoadExistingSave"),
        TEXT("-DGNoProfileWrites"),
        TEXT("-UserDir=") + ExternalUserDir,
    };
    for (const FString& RequiredArgument : RequiredArguments)
    {
        FString Missing = ValidCommand;
        Missing.ReplaceInline(*RequiredArgument, TEXT(""), ESearchCase::CaseSensitive);
        TestFalse(
            FString::Printf(TEXT("missing required argument is rejected: %s"), *RequiredArgument),
            Validate(Missing).bAccepted);
        TestFalse(
            FString::Printf(TEXT("duplicate singleton argument is rejected: %s"), *RequiredArgument),
            Validate(ValidCommand + TEXT(" ") + RequiredArgument).bAccepted);
    }

    const TArray<FString> BiasedOrUnknownArguments = {
        TEXT("-UseFixedTimeStep"),
        TEXT("-FixedSeed=123"),
        TEXT("-Benchmark"),
        TEXT("-NoVSync"),
        TEXT("-ExecCmds=stat_unit"),
        TEXT("-windowed"),
        TEXT("-d3d12"),
        TEXT("-log"),
    };
    for (const FString& BiasedOrUnknownArgument : BiasedOrUnknownArguments)
    {
        TestFalse(
            FString::Printf(TEXT("biased or unknown argument is rejected: %s"),
                *BiasedOrUnknownArgument),
            Validate(ValidCommand + TEXT(" ") + BiasedOrUnknownArgument).bAccepted);
    }

    const TArray<TPair<FString, FString>> Mutations = {
        {TEXT("-Course=PineRidge"), TEXT("-Course=Regression")},
        {TEXT("-Hole=2"), TEXT("-Hole=4")},
        {TEXT("-PerformanceCaptureSeconds=30"), TEXT("-PerformanceCaptureSeconds=29")},
        {TEXT("-ResX=1920"), TEXT("-ResX=1280")},
        {TEXT("-ResY=1080"), TEXT("-ResY=720")},
        {TEXT("-dx12"), TEXT("-dx11")},
    };
    for (const TPair<FString, FString>& Mutation : Mutations)
    {
        FString Mutated = ValidCommand;
        Mutated.ReplaceInline(*Mutation.Key, *Mutation.Value, ESearchCase::CaseSensitive);
        TestFalse(
            FString::Printf(TEXT("wrong required value is rejected: %s"), *Mutation.Value),
            Validate(Mutated).bAccepted);
    }

    TestFalse(TEXT("unknown NullRHI flag is rejected"),
        Validate(ValidCommand + TEXT(" -nullrhi")).bAccepted);
    TestFalse(TEXT("pre-existing capture namespace is rejected"),
        Validate(ValidCommand, true, FString(), true, false).bAccepted);
    TestFalse(TEXT("pre-existing save namespace is rejected"),
        Validate(ValidCommand, true, FString(), false, true).bAccepted);
    TestFalse(TEXT("resolved UserDir mismatch is rejected"),
        Validate(ValidCommand, true,
            TEXT("C:/DGTourExternal/aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee")).bAccepted);

    FString RelativeUserDir = ValidCommand;
    RelativeUserDir.ReplaceInline(*ExternalUserDir, TEXT("relative/11111111-2222-4333-8444-555555555555"));
    TestFalse(TEXT("relative UserDir is rejected"), Validate(RelativeUserDir).bAccepted);

    const FString ProjectUserDir = ProjectDirectory + TEXT("/11111111-2222-4333-8444-555555555555");
    FString ProjectLocal = ValidCommand;
    ProjectLocal.ReplaceInline(*ExternalUserDir, *ProjectUserDir);
    TestFalse(TEXT("project-local UserDir is rejected"),
        Validate(ProjectLocal, true, ProjectUserDir).bAccepted);

    const FString RootUserDir = RootDirectory + TEXT("/11111111-2222-4333-8444-555555555555");
    FString ArchiveLocal = ValidCommand;
    ArchiveLocal.ReplaceInline(*ExternalUserDir, *RootUserDir);
    TestFalse(TEXT("archive-local UserDir is rejected"),
        Validate(ArchiveLocal, true, RootUserDir).bAccepted);

    const FString NonV4UserDir = TEXT("C:/DGTourExternal/11111111-2222-3333-8444-555555555555");
    FString NonV4 = ValidCommand;
    NonV4.ReplaceInline(*ExternalUserDir, *NonV4UserDir);
    TestFalse(TEXT("non-v4 UserDir token is rejected"),
        Validate(NonV4, true, NonV4UserDir).bAccepted);

    return true;
}

#endif
