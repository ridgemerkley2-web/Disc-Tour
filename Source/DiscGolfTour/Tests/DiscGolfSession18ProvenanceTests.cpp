#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
TSharedPtr<FJsonObject> LoadJsonObject(
    FAutomationTestBase& Test,
    const FString& RelativePath)
{
    FString Text;
    const FString AbsolutePath = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir() / RelativePath);
    if (!Test.TestTrue(
            FString::Printf(TEXT("%s exists"), *RelativePath),
            FFileHelper::LoadFileToString(Text, *AbsolutePath)))
    {
        return nullptr;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!Test.TestTrue(
            FString::Printf(TEXT("%s parses"), *RelativePath),
            FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()))
    {
        return nullptr;
    }
    return Root;
}

TArray<FString> StringArray(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
{
    TArray<FString> Result;
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (Object.IsValid() && Object->TryGetArrayField(Field, Values) && Values)
    {
        for (const TSharedPtr<FJsonValue>& Value : *Values)
        {
            FString Text;
            if (Value.IsValid() && Value->TryGetString(Text))
            {
                Result.Add(Text);
            }
        }
    }
    return Result;
}

const TArray<FString>& HistoricalBlockers()
{
    static const TArray<FString> Values = {
        TEXT("FINAL_PUBLIC_TITLE_NOT_LOCKED_OR_CLEARED"),
        TEXT("EQUIPMENT_AND_COURSE_DISPLAY_NAMES_PENDING_CLEARANCE"),
        TEXT("CHARACTER_FRAMEWORK_SOURCE_DISTRIBUTION_RIGHTS_UNRESOLVED"),
        TEXT("DEVELOPMENT_DO_NOT_SHIP_CONTENT_PRESENT_IN_RUNTIME_CLOSURE"),
        TEXT("QUARANTINED_IMPORT_RECEIPTS_PENDING"),
        TEXT("POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE"),
        TEXT("ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED"),
        TEXT("MANUAL_VISUAL_LOGO_AND_TRADE_DRESS_REVIEW_REQUIRED"),
        TEXT("SESSION10_ENVIRONMENT_BINDINGS_NOT_PRODUCTION_READY"),
        TEXT("SESSION11_EQUIPMENT_PHYSICS_AND_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION12_PRESENTATION_INPUT_AUDIO_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION13_COURSE_AUTHORING_PCG_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION14_CAREER_AI_PRODUCTION_READINESS_PENDING"),
        TEXT("SESSION15_VERTICAL_SLICE_PRODUCTION_READINESS_PENDING"),
    };
    return Values;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession18ProvenanceLedgerTest,
    "DiscGolfTour.Session18.Provenance.OneBlockerResolution",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession18ProvenanceLedgerTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Contract = LoadJsonObject(
        *this, TEXT("Config/DG_Session18PolyHavenProvenanceContract.json"));
    if (!Contract.IsValid())
    {
        return false;
    }

    const TArray<FString> Historical = StringArray(Contract, TEXT("historicalReleaseBlockers"));
    const TArray<FString> Resolved = StringArray(Contract, TEXT("resolvedReleaseBlockers"));
    const TArray<FString> Remaining = StringArray(Contract, TEXT("remainingReleaseBlockers"));
    TestEqual(TEXT("Historical ledger retains fourteen blockers"), Historical.Num(), 14);
    TestTrue(TEXT("Historical blocker order remains exact"), Historical == HistoricalBlockers());
    TestEqual(TEXT("Exactly one blocker is resolved"), Resolved.Num(), 1);
    if (Resolved.Num() == 1)
    {
        TestEqual(TEXT("Only the Poly Haven derived-runtime receipt blocker resolves"),
            Resolved[0], FString(TEXT("POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE")));
    }
    TestEqual(TEXT("Exactly thirteen blockers remain"), Remaining.Num(), 13);
    TestFalse(TEXT("The resolved blocker is absent from the remaining ledger"),
        Remaining.Contains(TEXT("POLY_HAVEN_DERIVED_RUNTIME_PROVENANCE_RECEIPT_INCOMPLETE")));
    TestTrue(TEXT("Whole-package provenance remains explicitly blocked"),
        Remaining.Contains(TEXT("ACTUAL_STAGED_PACKAGE_PROVENANCE_CLOSURE_NOT_IMPLEMENTED")));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession18ProvenancePartitionTest,
    "DiscGolfTour.Session18.Provenance.ExactRuntimePartition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession18ProvenancePartitionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Receipt = LoadJsonObject(
        *this, TEXT("SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json"));
    if (!Receipt.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Sources = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Derived = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Excluded = nullptr;
    TestTrue(TEXT("Source array exists"), Receipt->TryGetArrayField(TEXT("sourceFiles"), Sources));
    TestTrue(TEXT("Derived array exists"), Receipt->TryGetArrayField(TEXT("derivedRuntimeArtifacts"), Derived));
    TestTrue(TEXT("Exclusion array exists"), Receipt->TryGetArrayField(
        TEXT("excludedProjectOriginalRuntimeArtifacts"), Excluded));
    if (!Sources || !Derived || !Excluded)
    {
        return false;
    }
    TestEqual(TEXT("Exactly 35 source files are bound"), Sources->Num(), 35);
    TestEqual(TEXT("Exactly 55 Poly Haven-derived runtime artifacts are bound"), Derived->Num(), 55);
    TestEqual(TEXT("Exactly five project-original artifacts are excluded"), Excluded->Num(), 5);

    TSet<FString> RuntimePaths;
    TSet<FString> MappedSources;
    auto ConsumeRuntime = [this, &RuntimePaths, &MappedSources](
        const TArray<TSharedPtr<FJsonValue>>& Values,
        bool bRequirePolySources)
    {
        for (const TSharedPtr<FJsonValue>& Value : Values)
        {
            const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!Item.IsValid())
            {
                AddError(TEXT("Runtime receipt entry is not an object"));
                continue;
            }
            FString RelativePath;
            if (!Item->TryGetStringField(TEXT("relativePath"), RelativePath))
            {
                AddError(TEXT("Runtime receipt entry has no relativePath"));
                continue;
            }
            TestTrue(TEXT("Runtime path stays under Pine Ridge"),
                RelativePath.StartsWith(TEXT("Content/Presentation/Course/PineRidge/")) &&
                RelativePath.EndsWith(TEXT(".uasset")) && !RelativePath.Contains(TEXT("..")));
            TestFalse(TEXT("Runtime paths are unique"), RuntimePaths.Contains(RelativePath));
            RuntimePaths.Add(RelativePath);
            TestTrue(TEXT("Every declared runtime artifact exists"),
                FPaths::FileExists(FPaths::ConvertRelativePathToFull(
                    FPaths::ProjectDir() / RelativePath)));
            if (bRequirePolySources)
            {
                const TArray<TSharedPtr<FJsonValue>>* Transitive = nullptr;
                if (!Item->TryGetArrayField(TEXT("transitiveSourceRelativePaths"), Transitive) ||
                    !Transitive || Transitive->IsEmpty())
                {
                    AddError(FString::Printf(TEXT("Derived artifact has no source closure: %s"),
                        *RelativePath));
                    continue;
                }
                for (const TSharedPtr<FJsonValue>& Source : *Transitive)
                {
                    MappedSources.Add(Source->AsString());
                }
            }
        }
    };
    ConsumeRuntime(*Derived, true);
    ConsumeRuntime(*Excluded, false);
    TestEqual(TEXT("Runtime partition contains exactly 60 unique files"), RuntimePaths.Num(), 60);
    TestEqual(TEXT("Every one of the 35 Poly Haven source files participates"), MappedSources.Num(), 35);
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfSession18ProvenanceBoundaryTest,
    "DiscGolfTour.Session18.Provenance.ReleaseBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession18ProvenanceBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const TSharedPtr<FJsonObject> Contract = LoadJsonObject(
        *this, TEXT("Config/DG_Session18PolyHavenProvenanceContract.json"));
    const TSharedPtr<FJsonObject> Receipt = LoadJsonObject(
        *this, TEXT("SourceArt/PineRidge/PolyHaven/derived_runtime_receipt.json"));
    if (!Contract.IsValid() || !Receipt.IsValid())
    {
        return false;
    }
    const TSharedPtr<FJsonObject> ExitGate = Contract->GetObjectField(TEXT("exitGate"));
    const TSharedPtr<FJsonObject> Evidence = Contract->GetObjectField(TEXT("closureEvidence"));
    const TSharedPtr<FJsonObject> Assertions = Receipt->GetObjectField(TEXT("closureAssertions"));
    TestTrue(TEXT("Derived source-to-runtime provenance is complete"),
        ExitGate->GetBoolField(TEXT("derivedRuntimeProvenanceComplete")));
    TestFalse(TEXT("Public release remains blocked"),
        ExitGate->GetBoolField(TEXT("publicReleaseReady")));
    TestEqual(TEXT("Thirteen release blockers remain"),
        static_cast<int32>(ExitGate->GetNumberField(TEXT("remainingReleaseBlockerCount"))), 13);
    TestFalse(TEXT("Session 18 does not claim whole-package provenance"),
        Evidence->GetBoolField(TEXT("actualStagedPackageProvenanceClosed")));
    TestFalse(TEXT("Receipt does not claim whole-package provenance"),
        Assertions->GetBoolField(TEXT("actualStagedPackageProvenanceClosed")));
    return !HasAnyErrors();
}

#endif
