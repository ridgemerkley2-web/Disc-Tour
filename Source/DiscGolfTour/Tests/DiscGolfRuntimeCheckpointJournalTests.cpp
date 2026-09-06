#if WITH_DEV_AUTOMATION_TESTS

#include "../DiscGolfRuntimeCheckpointJournal.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDiscGolfRuntimeCheckpointJournalTest,
    "DiscGolfTour.Release.RuntimeCheckpointJournal.CanonicalContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscGolfRuntimeCheckpointJournalTest::RunTest(const FString& Parameters)
{
    using namespace DiscGolfRuntimeCheckpointJournal;

    FCaptureBinding Binding;
    Binding.CandidateId = TEXT("S19_WindowsShipping_20990101T000000Z_abcdef123456");
    Binding.UserDirToken = TEXT("11111111-2222-4333-8444-555555555555");
    Binding.ExecutableSha256 = FString::ChrN(64, TEXT('A'));
    Binding.ArchiveManifestSha256 = FString::ChrN(64, TEXT('B'));
    Binding.CaptureNonce = TEXT("aaaaaaaa-bbbb-4ccc-8ddd-eeeeeeeeeeee");

    FString Error;
    TestTrue(TEXT("valid capture binding"), TryValidateCaptureBinding(
        Binding,
        TEXT("C:/External/11111111-2222-4333-8444-555555555555"),
        Error));

    FString Header;
    TestTrue(TEXT("canonical header serializes"), TrySerializeHeaderLine(
        Binding,
        TEXT("12345678-1234-4234-8234-123456789abc"),
        TEXT("2099-01-01T00:00:00.000Z"),
        Header,
        Error));
    TestTrue(TEXT("header has one LF and no presentation whitespace"),
        Header.EndsWith(TEXT("\n")) && !Header.Contains(TEXT("\r"))
        && !Header.LeftChop(1).Contains(TEXT("\n")));
    TestTrue(TEXT("header keys are canonical"), Header.StartsWith(
        TEXT("{\"archiveManifestSha256\":\"")));

    // CoCreateGuid writes GUID::Data3 (the version-bearing word) after Data2
    // in memory. Interpreting those bytes as FGuid places Data3 in B's high
    // word, which FGuid prints as the second rather than the third group.
    const FGuid NativeWindowsLayoutUuidV4(
        0x01234567, 0x4abc1234, 0x6745238d, 0x89abcdef);
    const FString RawNativeWindowsLayout = NativeWindowsLayoutUuidV4.ToString(
        EGuidFormats::DigitsWithHyphensLower);
    TestEqual(TEXT("native UUID version is reordered by FGuid formatting"),
        RawNativeWindowsLayout[9], TEXT('4'));
    TestEqual(TEXT("native UUID variant is reordered by FGuid formatting"),
        RawNativeWindowsLayout[26], TEXT('8'));
    FString ReorderedHeader = TEXT("sentinel-native-layout");
    TestFalse(TEXT("raw native-layout FGuid is not an RFC string UUIDv4"),
        TrySerializeHeaderLine(
            Binding,
            RawNativeWindowsLayout,
            TEXT("2099-01-01T00:00:00.000Z"),
            ReorderedHeader,
            Error));
    TestEqual(TEXT("reordered round serialization is transactional"),
        ReorderedHeader, TEXT("sentinel-native-layout"));

    // Exercise the production generator rather than another handwritten test
    // UUID. On Windows, formatting FGuid::NewGuid directly can move the native
    // GUID version/variant fields away from the RFC string offsets.
    for (int32 Index = 0; Index < 64; ++Index)
    {
        const FString GeneratedRoundId = NewCanonicalUuidV4();
        TestEqual(TEXT("generated round UUID length"), GeneratedRoundId.Len(), 36);
        TestEqual(TEXT("generated round UUID version"), GeneratedRoundId[14], TEXT('4'));
        TestTrue(TEXT("generated round UUID variant"),
            GeneratedRoundId[19] == TEXT('8') || GeneratedRoundId[19] == TEXT('9')
            || GeneratedRoundId[19] == TEXT('a') || GeneratedRoundId[19] == TEXT('b'));

        FString GeneratedHeader;
        TestTrue(TEXT("real generated round UUID serializes"), TrySerializeHeaderLine(
            Binding,
            GeneratedRoundId,
            TEXT("2099-01-01T00:00:00.000Z"),
            GeneratedHeader,
            Error));
    }

    FCheckpointEvent Event;
    Event.Sequence = 12;
    Event.MonotonicMs = 1200;
    Event.Event = TEXT("FINAL_SCORECARD");
    Event.RoundId = TEXT("12345678-1234-4234-8234-123456789abc");
    Event.CompletedHoles = 3;
    Event.TotalStrokes = 11;
    Event.TotalPenalties = 0;
    Event.FinalScoreRows = {
        {1, 3, 3, 0},
        {2, 4, 4, 0},
        {3, 4, 4, 0},
    };
    FString EventLine;
    TestTrue(TEXT("canonical final event serializes"),
        TrySerializeEventLine(Event, EventLine, Error));
    TestTrue(TEXT("final event begins in sorted key order"), EventLine.StartsWith(
        TEXT("{\"completedHoles\":3,\"event\":\"FINAL_SCORECARD\",\"finalScore\":")));
    TestTrue(TEXT("score rows use sorted keys"), EventLine.Contains(
        TEXT("{\"holeNumber\":1,\"par\":3,\"penalties\":0,\"strokes\":3}")));

    FCaptureBinding ReusedNonce = Binding;
    ReusedNonce.CaptureNonce = ReusedNonce.UserDirToken;
    TestFalse(TEXT("capture nonce cannot reuse UserDir token"),
        TryValidateCaptureBinding(
            ReusedNonce,
            TEXT("C:/External/11111111-2222-4333-8444-555555555555"),
            Error));
    FString ReusedRoundOutput = TEXT("sentinel-round");
    TestFalse(TEXT("round ID cannot reuse capture nonce"), TrySerializeHeaderLine(
        Binding,
        Binding.CaptureNonce,
        TEXT("2099-01-01T00:00:00.000Z"),
        ReusedRoundOutput,
        Error));
    TestEqual(TEXT("reused round serialization is transactional"),
        ReusedRoundOutput, TEXT("sentinel-round"));

    FCaptureBinding Mutated = Binding;
    Mutated.ExecutableSha256[0] = TEXT('a');
    FString Unchanged = TEXT("sentinel");
    TestFalse(TEXT("lowercase digest rejected"), TrySerializeHeaderLine(
        Mutated,
        TEXT("12345678-1234-4234-8234-123456789abc"),
        TEXT("2099-01-01T00:00:00.000Z"),
        Unchanged,
        Error));
    TestEqual(TEXT("failed serialization is transactional"), Unchanged, TEXT("sentinel"));

    const FString WriterTestDirectory = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("Automation/RuntimeCheckpointJournal"),
            NewCanonicalUuidV4()));
    IFileManager& FileManager = IFileManager::Get();
    TestTrue(TEXT("exclusive-writer test directory created"),
        FileManager.MakeDirectory(*WriterTestDirectory, true));
    const FString WriterTestPath = FPaths::Combine(
        WriterTestDirectory, TEXT("exclusive-create.jsonl"));

    TUniquePtr<FArchive> FirstWriter;
    FString WriterError;
    const bool bFirstWriterCreated = TryCreateExclusiveJournalWriter(
        WriterTestPath, FirstWriter, WriterError);
    TestTrue(FString::Printf(
        TEXT("first atomic writer owns a new path (%s)"), *WriterError),
        bFirstWriterCreated);
    if (FirstWriter)
    {
        static const ANSICHAR FirstBytes[] = "first\n";
        FirstWriter->Serialize(
            const_cast<ANSICHAR*>(FirstBytes), UE_ARRAY_COUNT(FirstBytes) - 1);
        FirstWriter->Flush();
        TestFalse(TEXT("first write and durable flush succeeded"), FirstWriter->IsError());

        TArray<uint8> FlushedBytes;
        TestTrue(TEXT("flushed bytes are readable while writer remains open"),
            FFileHelper::LoadFileToArray(
                FlushedBytes, *WriterTestPath, FILEREAD_AllowWrite));
        TestEqual(TEXT("first flushed byte count"),
            FlushedBytes.Num(), static_cast<int32>(UE_ARRAY_COUNT(FirstBytes) - 1));
        TestTrue(TEXT("first flushed bytes match"),
            FlushedBytes.Num() == UE_ARRAY_COUNT(FirstBytes) - 1
            && FMemory::Memcmp(
                FlushedBytes.GetData(), FirstBytes, FlushedBytes.Num()) == 0);

        TUniquePtr<FArchive> CollisionWriter;
        FString CollisionError;
        TestFalse(TEXT("atomic collision is rejected"),
            TryCreateExclusiveJournalWriter(
                WriterTestPath, CollisionWriter, CollisionError));
        TestFalse(TEXT("collision returns no writer"), CollisionWriter.IsValid());
        TestTrue(TEXT("collision reports existing path"),
            CollisionError.Contains(TEXT("already exists")));

        TArray<uint8> CollisionPreservedBytes;
        TestTrue(TEXT("collision leaves original readable"),
            FFileHelper::LoadFileToArray(
                CollisionPreservedBytes, *WriterTestPath, FILEREAD_AllowWrite));
        TestTrue(TEXT("collision preserves original bytes"),
            CollisionPreservedBytes == FlushedBytes);

        static const ANSICHAR SecondBytes[] = "second\n";
        FirstWriter->Serialize(
            const_cast<ANSICHAR*>(SecondBytes), UE_ARRAY_COUNT(SecondBytes) - 1);
        FirstWriter->Flush();
        TestFalse(TEXT("original writer remains usable after collision"),
            FirstWriter->IsError());
        TestTrue(TEXT("original writer closes cleanly"), FirstWriter->Close());
        FirstWriter.Reset();

        TArray<uint8> FinalBytes;
        TestTrue(TEXT("closed journal bytes are readable"),
            FFileHelper::LoadFileToArray(FinalBytes, *WriterTestPath));
        TArray<uint8> ExpectedFinalBytes;
        ExpectedFinalBytes.Append(
            reinterpret_cast<const uint8*>(FirstBytes), UE_ARRAY_COUNT(FirstBytes) - 1);
        ExpectedFinalBytes.Append(
            reinterpret_cast<const uint8*>(SecondBytes), UE_ARRAY_COUNT(SecondBytes) - 1);
        TestTrue(TEXT("write, flush, and close preserve exact bytes"),
            FinalBytes == ExpectedFinalBytes);

        TUniquePtr<FArchive> ClosedCollisionWriter;
        FString ClosedCollisionError;
        TestFalse(TEXT("closed existing path still cannot be replaced"),
            TryCreateExclusiveJournalWriter(
                WriterTestPath, ClosedCollisionWriter, ClosedCollisionError));
        TArray<uint8> ClosedCollisionBytes;
        TestTrue(TEXT("closed-path collision leaves bytes readable"),
            FFileHelper::LoadFileToArray(
                ClosedCollisionBytes, *WriterTestPath));
        TestTrue(TEXT("closed-path collision preserves exact bytes"),
            ClosedCollisionBytes == ExpectedFinalBytes);
    }
    TestTrue(TEXT("exclusive-writer test directory cleaned"),
        FileManager.DeleteDirectory(*WriterTestDirectory, false, true));
    return true;
}

#endif
