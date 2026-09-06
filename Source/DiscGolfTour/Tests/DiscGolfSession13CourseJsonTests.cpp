#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "../DiscGolfCourseDefinition.h"

namespace
{
    bool LoadCanonicalHoleJson(FString& OutJson)
    {
        return FFileHelper::LoadFileToString(
            OutJson,
            *FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/PineRidgeHole1.json")));
    }

    bool ParseMustReject(
        FAutomationTestBase& Test,
        const FString& Json,
        const TCHAR* ExpectedErrorFragment)
    {
        FDiscGolfHoleBlockoutDefinition Definition;
        Definition.HoleNumber = 777;
        Definition.CourseId = TEXT("UnchangedOnFailure");
        FString Error;
        const bool bParsed = DiscGolfCourseDefinition::ParseJson(Json, Definition, Error);
        Test.TestFalse(TEXT("Malformed course JSON is rejected"), bParsed);
        Test.TestEqual(TEXT("A rejected parse does not mutate the caller's hole number"),
            Definition.HoleNumber, 777);
        Test.TestEqual(TEXT("A rejected parse does not mutate the caller's course identity"),
            Definition.CourseId, FName(TEXT("UnchangedOnFailure")));
        Test.TestTrue(
            FString::Printf(TEXT("Rejection identifies '%s'"), ExpectedErrorFragment),
            Error.Contains(ExpectedErrorFragment, ESearchCase::IgnoreCase));
        return !bParsed;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13CourseJsonUnknownEnumTest,
    "DiscGolfTour.Session13.CourseJson.RejectsUnknownEnum",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13CourseJsonUnknownEnumTest::RunTest(const FString& Parameters)
{
    FString Json;
    if (!TestTrue(TEXT("Canonical Hole 1 JSON is present"), LoadCanonicalHoleJson(Json))) return false;
    TestTrue(TEXT("Test mutation finds the canonical surface token"),
        Json.ReplaceInline(TEXT("\"surface\": \"Fairway\""), TEXT("\"surface\": \"UnknownFairway\""),
            ESearchCase::CaseSensitive) > 0);
    return ParseMustReject(*this, Json, TEXT("unknown"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13CourseJsonBooleanIntegerTest,
    "DiscGolfTour.Session13.CourseJson.RejectsBooleanInteger",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13CourseJsonBooleanIntegerTest::RunTest(const FString& Parameters)
{
    FString Json;
    if (!TestTrue(TEXT("Canonical Hole 1 JSON is present"), LoadCanonicalHoleJson(Json))) return false;
    TestTrue(TEXT("Test mutation finds par"),
        Json.ReplaceInline(TEXT("\"par\": 3"), TEXT("\"par\": true"), ESearchCase::CaseSensitive) == 1);
    return ParseMustReject(*this, Json, TEXT("number"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13CourseJsonNonFiniteNumberTest,
    "DiscGolfTour.Session13.CourseJson.RejectsNonFiniteNumber",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13CourseJsonNonFiniteNumberTest::RunTest(const FString& Parameters)
{
    FString Json;
    if (!TestTrue(TEXT("Canonical Hole 1 JSON is present"), LoadCanonicalHoleJson(Json))) return false;
    TestTrue(TEXT("Test mutation finds a tree height"),
        Json.ReplaceInline(TEXT("\"heightScale\": 0.92"), TEXT("\"heightScale\": 1e9999"),
            ESearchCase::CaseSensitive) > 0);
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Error;
    TestFalse(TEXT("Overflowing JSON numbers are rejected"),
        DiscGolfCourseDefinition::ParseJson(Json, Definition, Error));
    TestTrue(TEXT("Overflow rejection is a parse or finite-number failure"),
        Error.Contains(TEXT("finite"), ESearchCase::IgnoreCase)
        || Error.Contains(TEXT("invalid JSON"), ESearchCase::IgnoreCase));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13CourseJsonUnknownFieldTest,
    "DiscGolfTour.Session13.CourseJson.RejectsUnknownField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13CourseJsonUnknownFieldTest::RunTest(const FString& Parameters)
{
    FString Json;
    if (!TestTrue(TEXT("Canonical Hole 1 JSON is present"), LoadCanonicalHoleJson(Json))) return false;
    TestTrue(TEXT("Test mutation adds one root field"),
        Json.ReplaceInline(TEXT("\"par\": 3,"), TEXT("\"par\": 3, \"unexpectedPolicy\": 7,"),
            ESearchCase::CaseSensitive) == 1);
    return ParseMustReject(*this, Json, TEXT("unknown"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13CourseJsonDuplicateFieldTest,
    "DiscGolfTour.Session13.CourseJson.RejectsDuplicateField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13CourseJsonDuplicateFieldTest::RunTest(const FString& Parameters)
{
    FString Json;
    if (!TestTrue(TEXT("Canonical Hole 1 JSON is present"), LoadCanonicalHoleJson(Json))) return false;
    TestTrue(TEXT("Test mutation adds one duplicate root field"),
        Json.ReplaceInline(TEXT("\"par\": 3,"), TEXT("\"par\": 3, \"par\": 4,"),
            ESearchCase::CaseSensitive) == 1);
    return ParseMustReject(*this, Json, TEXT("duplicate"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscGolfSession13CourseJsonCanonicalParityTest,
    "DiscGolfTour.Session13.CourseJson.CanonicalPineRidgeParity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfSession13CourseJsonCanonicalParityTest::RunTest(const FString& Parameters)
{
    FString Json;
    if (!TestTrue(TEXT("Canonical Hole 1 JSON is present"), LoadCanonicalHoleJson(Json))) return false;

    FDiscGolfHoleBlockoutDefinition Parsed;
    FString Error;
    if (!TestTrue(TEXT("Strict parsing accepts canonical Pine Ridge Hole 1"),
        DiscGolfCourseDefinition::ParseJson(Json, Parsed, Error)))
    {
        AddError(Error);
        return false;
    }

    FDiscGolfHoleBlockoutDefinition Loaded;
    FString Source;
    TestTrue(TEXT("The existing runtime loader still accepts Hole 1"),
        DiscGolfCourseDefinition::LoadPineRidgeHole1(Loaded, Source, Error));
    TestEqual(TEXT("The authored runtime source remains authoritative"), Source, FString(TEXT("AUTHORED JSON")));
    TestEqual(TEXT("Hole identity is unchanged"), Parsed.HoleNumber, Loaded.HoleNumber);
    TestEqual(TEXT("Surface coverage is unchanged"), Parsed.Surfaces.Num(), Loaded.Surfaces.Num());
    TestEqual(TEXT("Collision fixture coverage is unchanged"), Parsed.CollisionFixtures.Num(), Loaded.CollisionFixtures.Num());
    TestEqual(TEXT("Strategy route coverage is unchanged"), Parsed.ShotRoutes.Num(), Loaded.ShotRoutes.Num());
    TestTrue(TEXT("Measured distance is unchanged"), FMath::IsNearlyEqual(
        DiscGolfCourseDefinition::MeasuredDistanceFeet(Parsed),
        DiscGolfCourseDefinition::MeasuredDistanceFeet(Loaded), 0.001f));
    return true;
}

#endif
