#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "DiscBagComponent.h"
#include "DiscCatalogSubsystem.h"
#include "DiscGolfCareerProgressSaveGame.h"
#include "DiscGolfCareerSubsystem.h"
#include "DiscGolfCompetitionRuntime.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfRoundState.h"
#include "DiscGolfScoringLibrary.h"

namespace
{
TArray<FDiscGolfHoleBlockoutDefinition> BuildFallbackHoles()
{
    return {
        DiscGolfCourseDefinition::PineRidgeHole1Fallback(),
        DiscGolfCourseDefinition::PineRidgeHole2Fallback(),
        DiscGolfCourseDefinition::PineRidgeHole3Fallback()
    };
}

bool BuildCompletedRound(FDiscGolfRoundState& OutRound, FString& OutError)
{
    if (!DiscGolfRound::Initialize(
        OutRound,
        TEXT("PineRidgeChampionship"),
        TEXT("Championship"),
        FText::FromString(TEXT("Pine Ridge Championship")),
        BuildFallbackHoles(),
        OutError))
    {
        return false;
    }

    // Project hole strokes already include their separately recorded penalties.
    return DiscGolfRound::RecordCurrentHole(OutRound, 3, 1, OutError)
        && DiscGolfRound::Advance(OutRound, OutError)
        && DiscGolfRound::RecordCurrentHole(OutRound, 4, 0, OutError)
        && DiscGolfRound::Advance(OutRound, OutError)
        && DiscGolfRound::RecordCurrentHole(OutRound, 5, 2, OutError);
}

bool FindPlastic(
    const TArray<FDiscPlasticDefinition>& Plastics,
    const FName PlasticId,
    EDiscPlastic& OutPlastic)
{
    const FDiscPlasticDefinition* Found = Plastics.FindByPredicate(
        [PlasticId](const FDiscPlasticDefinition& Entry)
        {
            return Entry.PlasticId == PlasticId;
        });
    if (!Found)
    {
        return false;
    }
    OutPlastic = Found->Plastic;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14SmokeCatalogIdentityTest,
    "DiscGolfTour.Session14.Smoke.CatalogIdentityAndValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14SmokeCatalogIdentityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDiscMoldDefinition> Molds;
    TArray<FDiscPlasticDefinition> Plastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);

    FString Error;
    TestTrue(TEXT("Fallback catalog passes the existing strict validator"),
        UDiscCatalogSubsystem::ValidateDefinitions(Molds, Plastics, Error));
    TestTrue(TEXT("Fallback mold catalog is populated"), !Molds.IsEmpty());
    TestTrue(TEXT("Fallback plastic catalog is populated"), !Plastics.IsEmpty());

    TSet<FName> MoldIds;
    for (const FDiscMoldDefinition& Mold : Molds)
    {
        TestFalse(TEXT("Fallback mold ID is not duplicated"), MoldIds.Contains(Mold.MoldId));
        MoldIds.Add(Mold.MoldId);
    }
    TSet<FName> PlasticIds;
    for (const FDiscPlasticDefinition& Plastic : Plastics)
    {
        TestFalse(TEXT("Fallback plastic ID is not duplicated"), PlasticIds.Contains(Plastic.PlasticId));
        PlasticIds.Add(Plastic.PlasticId);
    }
    TestEqual(TEXT("Every fallback mold has one unique ID"), MoldIds.Num(), Molds.Num());
    TestEqual(TEXT("Every fallback plastic has one unique ID"), PlasticIds.Num(), Plastics.Num());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14SmokeStarterBagResolutionTest,
    "DiscGolfTour.Session14.Smoke.StarterBagStableResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14SmokeStarterBagResolutionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TArray<FDiscMoldDefinition> Molds;
    TArray<FDiscPlasticDefinition> Plastics;
    UDiscCatalogSubsystem::BuildFallbackDefinitions(Molds, Plastics);

    const FDGDiscBagLoadout First = UDiscBagComponent::BuildDefaultLoadout();
    const FDGDiscBagLoadout Second = UDiscBagComponent::BuildDefaultLoadout();
    FString Error;
    TestTrue(TEXT("Starter bag passes the existing strict loadout validator"),
        UDiscBagComponent::ValidateLoadout(First, Error));
    TestEqual(TEXT("Starter bag population is deterministic"), First.Discs.Num(), Second.Discs.Num());
    TestEqual(TEXT("Starter bag selection identity is stable"),
        First.SelectedDiscInstanceId, Second.SelectedDiscInstanceId);

    for (int32 Index = 0; Index < First.Discs.Num(); ++Index)
    {
        const FDGDiscInstance& Instance = First.Discs[Index];
        TestEqual(TEXT("Every starter disc retains an exact stable instance identity"),
            Instance.InstanceId, Second.Discs[Index].InstanceId);
        TestEqual(TEXT("Every starter disc retains an exact stable mold identity"),
            Instance.DiscDefinitionId, Second.Discs[Index].DiscDefinitionId);
        TestEqual(TEXT("Every starter disc retains an exact stable plastic identity"),
            Instance.PlasticId, Second.Discs[Index].PlasticId);

        EDiscPlastic Plastic = EDiscPlastic::Tour;
        const bool bPlasticFound = FindPlastic(Plastics, Instance.PlasticId, Plastic);
        TestTrue(TEXT("Starter disc plastic resolves in the fallback catalog"), bPlasticFound);

        FResolvedDiscDefinition CatalogDisc;
        const bool bCatalogResolved = bPlasticFound
            && UDiscCatalogSubsystem::ResolveFromDefinitions(
                Molds, Plastics, Instance.DiscDefinitionId, Plastic, CatalogDisc);
        TestTrue(TEXT("Starter disc mold resolves in the fallback catalog"), bCatalogResolved);

        FResolvedDiscDefinition ResolvedInstance;
        const bool bInstanceResolved = bCatalogResolved
            && UDiscBagComponent::ResolveDiscInstance(
                Instance, CatalogDisc, ResolvedInstance, Error);
        TestTrue(TEXT("Starter disc resolves through the player-instance seam"), bInstanceResolved);
        if (bInstanceResolved)
        {
            TestEqual(TEXT("Resolved disc preserves exact instance identity"),
                ResolvedInstance.DiscInstanceId, Instance.InstanceId);
            TestEqual(TEXT("Resolved disc preserves exact mold identity"),
                ResolvedInstance.MoldId, Instance.DiscDefinitionId);
            TestEqual(TEXT("Resolved disc preserves exact mass"),
                ResolvedInstance.DiscMassGrams, Instance.MassGrams);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14SmokeScoringParityTest,
    "DiscGolfTour.Session14.Smoke.ScoringPenaltyParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14SmokeScoringParityTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    FDGRoundScorecard Scorecard;
    Scorecard.EventId = TEXT("PineRidgeChampionship");
    Scorecard.CourseId = TEXT("PineRidgeChampionship");
    Scorecard.RoundNumber = 1;

    const int32 Pars[] = { 3, 4, 4 };
    const int32 BaseStrokes[] = { 2, 4, 3 };
    const int32 Penalties[] = { 1, 0, 2 };
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FDGScorecardHole& Hole = Scorecard.Holes.AddDefaulted_GetRef();
        Hole.HoleNumber = Index + 1;
        Hole.Par = Pars[Index];
        Hole.Strokes = BaseStrokes[Index];
        Hole.PenaltyStrokes = Penalties[Index];
        Hole.bComplete = true;
    }

    TestTrue(TEXT("Framework scorecard reports complete"),
        UDiscGolfScoringLibrary::IsRoundComplete(Scorecard));
    TestEqual(TEXT("Framework total includes separate penalties exactly once"),
        UDiscGolfScoringLibrary::CalculateTotalStrokes(Scorecard), 12);
    TestEqual(TEXT("Framework to-par includes separate penalties exactly once"),
        UDiscGolfScoringLibrary::CalculateToPar(Scorecard), 1);

    FDiscGolfRoundState ProjectRound;
    FString Error;
    TestTrue(TEXT("Equivalent project round completes"), BuildCompletedRound(ProjectRound, Error));
    TestTrue(TEXT("Equivalent project round reports complete"), ProjectRound.bRoundComplete);
    TestEqual(TEXT("Project strokes-including-penalties match framework total"),
        DiscGolfRound::TotalStrokes(ProjectRound),
        UDiscGolfScoringLibrary::CalculateTotalStrokes(Scorecard));
    TestEqual(TEXT("Project to-par matches framework to-par"),
        DiscGolfRound::ScoreToPar(ProjectRound),
        UDiscGolfScoringLibrary::CalculateToPar(Scorecard));
    TestEqual(TEXT("Separate project penalty metadata is preserved"),
        DiscGolfRound::TotalPenaltyStrokes(ProjectRound), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14SmokeCourseValidationTest,
    "DiscGolfTour.Session14.Smoke.PineRidgeFallbackValidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14SmokeCourseValidationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfCourseManifestDefinition Manifest =
        DiscGolfCourseDefinition::PineRidgeCourseFallback();
    FString Error;
    TestTrue(TEXT("Fallback course manifest passes the existing project validator"),
        DiscGolfCourseDefinition::ValidateManifest(Manifest, Error));
    TestEqual(TEXT("Fallback manifest contains all three holes"), Manifest.Holes.Num(), 3);

    const TArray<FDiscGolfHoleBlockoutDefinition> Holes = BuildFallbackHoles();
    TestEqual(TEXT("All three fallback hole definitions are present"), Holes.Num(), 3);
    for (int32 Index = 0; Index < Holes.Num(); ++Index)
    {
        TestTrue(TEXT("Fallback hole passes the existing project validator"),
            DiscGolfCourseDefinition::Validate(Holes[Index], Error));
        TestEqual(TEXT("Fallback hole numbering is contiguous"), Holes[Index].HoleNumber, Index + 1);
        TestEqual(TEXT("Manifest numbering matches its fallback hole"),
            Manifest.Holes[Index].HoleNumber, Holes[Index].HoleNumber);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession14SmokeCareerArchiveTest,
    "DiscGolfTour.Session14.Smoke.CareerSchemaV1ArchiveRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession14SmokeCareerArchiveTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FDiscGolfCompetitionRuntimeDefinition Event =
        DiscGolfCompetitionRuntime::BuildSourceFallbackEvent();
    FDiscGolfRoundState Round;
    FString Error;
    TestTrue(TEXT("Completed source round is available for career serialization"),
        BuildCompletedRound(Round, Error));

    FDGRoundScorecard Scorecard;
    TestTrue(TEXT("Completed source round converts to a valid event result"),
        DiscGolfCompetitionRuntime::BuildCompletedScorecard(Event, Round, Scorecard, Error));

    UDiscGolfCareerProgressSaveGame* Source = NewObject<UDiscGolfCareerProgressSaveGame>();
    Source->SchemaVersion = UDiscGolfCareerProgressSaveGame::CurrentSchemaVersion;
    Source->Progress.CompletedEventIds.Add(Event.EventId);
    Source->Progress.RoundHistory.Add(Scorecard);
    TestTrue(TEXT("Populated schema-v1 career passes strict validation before serialization"),
        UDiscGolfCareerSubsystem::ValidateProgress(Source->Progress, Error));

    TArray<uint8> Bytes;
    {
        FMemoryWriter Writer(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Writer, false);
        Archive.ArIsSaveGame = true;
        Source->Serialize(Archive);
    }
    TestTrue(TEXT("SaveGame archive emits career bytes"), !Bytes.IsEmpty());

    UDiscGolfCareerProgressSaveGame* Restored = NewObject<UDiscGolfCareerProgressSaveGame>();
    {
        FMemoryReader Reader(Bytes, true);
        FObjectAndNameAsStringProxyArchive Archive(Reader, false);
        Archive.ArIsSaveGame = true;
        Restored->Serialize(Archive);
    }

    TestEqual(TEXT("Career schema version survives SaveGame serialization"),
        Restored->SchemaVersion, UDiscGolfCareerProgressSaveGame::CurrentSchemaVersion);
    TestEqual(TEXT("Exactly one completed event survives SaveGame serialization"),
        Restored->Progress.CompletedEventIds.Num(), 1);
    TestEqual(TEXT("Exactly one round result survives SaveGame serialization"),
        Restored->Progress.RoundHistory.Num(), 1);
    TestTrue(TEXT("Restored schema-v1 career passes strict validation"),
        UDiscGolfCareerSubsystem::ValidateProgress(Restored->Progress, Error));
    if (Restored->Progress.RoundHistory.Num() == 1)
    {
        TestEqual(TEXT("Restored result retains the exact score total"),
            UDiscGolfScoringLibrary::CalculateTotalStrokes(Restored->Progress.RoundHistory[0]), 12);
        TestEqual(TEXT("Restored result retains the exact score to par"),
            UDiscGolfScoringLibrary::CalculateToPar(Restored->Progress.RoundHistory[0]), 1);
    }
    return true;
}

#endif
