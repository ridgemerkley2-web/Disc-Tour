#include "DiscGolfRuntimeCheckpointJournal.h"

#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
bool IsCanonicalUuid(const FString& Value, bool bRequireVersionFour)
{
    FGuid Parsed;
    if (!FGuid::ParseExact(Value, EGuidFormats::DigitsWithHyphensLower, Parsed)
        || !Parsed.IsValid()
        || Parsed.ToString(EGuidFormats::DigitsWithHyphensLower) != Value)
    {
        return false;
    }
    if (!bRequireVersionFour)
    {
        return true;
    }
    return Value.Len() == 36
        && Value[14] == TEXT('4')
        && (Value[19] == TEXT('8') || Value[19] == TEXT('9')
            || Value[19] == TEXT('a') || Value[19] == TEXT('b'));
}

bool IsUpperSha256(const FString& Value)
{
    if (Value.Len() != 64)
    {
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (!((Character >= TEXT('0') && Character <= TEXT('9'))
            || (Character >= TEXT('A') && Character <= TEXT('F'))))
        {
            return false;
        }
    }
    return true;
}

bool IsCandidateId(const FString& Value)
{
    static const FString Prefix = TEXT("S19_WindowsShipping_");
    if (!Value.StartsWith(Prefix, ESearchCase::CaseSensitive))
    {
        return false;
    }
    const int32 SuffixLength = Value.Len() - Prefix.Len();
    if (SuffixLength < 1 || SuffixLength > 96)
    {
        return false;
    }
    for (int32 Index = Prefix.Len(); Index < Value.Len(); ++Index)
    {
        const TCHAR Character = Value[Index];
        if (!FChar::IsAlnum(Character)
            && Character != TEXT('_') && Character != TEXT('-'))
        {
            return false;
        }
    }
    return true;
}

bool IsValidUtcText(const FString& Value)
{
    FDateTime Parsed;
    return Value.EndsWith(TEXT("Z"), ESearchCase::CaseSensitive)
        && FDateTime::ParseIso8601(*Value, Parsed);
}

template <typename WriterType>
bool CloseJsonLine(
    const TSharedRef<WriterType>& Writer,
    FString& InOutLine,
    FString& OutError)
{
    if (!Writer->Close())
    {
        OutError = TEXT("canonical JSON writer could not close");
        InOutLine.Reset();
        return false;
    }
    InOutLine += TEXT("\n");
    OutError.Reset();
    return true;
}

#if PLATFORM_WINDOWS
class FExclusiveJournalArchive final : public FArchive
{
public:
    FExclusiveJournalArchive(HANDLE InHandle, FString InFilename)
        : Handle(InHandle)
        , Filename(MoveTemp(InFilename))
    {
        SetIsSaving(true);
        SetIsPersistent(true);
    }

    virtual ~FExclusiveJournalArchive() override
    {
        Close();
    }

    virtual void Serialize(void* Data, int64 Length) override
    {
        if (Handle == INVALID_HANDLE_VALUE || Length < 0
            || (Length > 0 && Data == nullptr))
        {
            SetError();
            return;
        }

        uint8* Cursor = static_cast<uint8*>(Data);
        int64 Remaining = Length;
        while (Remaining > 0)
        {
            const DWORD ChunkSize = static_cast<DWORD>(FMath::Min<int64>(
                Remaining, static_cast<int64>(MAXDWORD)));
            DWORD BytesWritten = 0;
            if (!::WriteFile(Handle, Cursor, ChunkSize, &BytesWritten, nullptr)
                || BytesWritten != ChunkSize)
            {
                Position += BytesWritten;
                SetError();
                return;
            }
            Position += BytesWritten;
            Cursor += BytesWritten;
            Remaining -= BytesWritten;
        }
    }

    virtual void Flush() override
    {
        if (Handle != INVALID_HANDLE_VALUE && !::FlushFileBuffers(Handle))
        {
            SetError();
        }
    }

    virtual bool Close() override
    {
        if (Handle != INVALID_HANDLE_VALUE)
        {
            Flush();
            if (!::CloseHandle(Handle))
            {
                SetError();
            }
            Handle = INVALID_HANDLE_VALUE;
        }
        return !IsError();
    }

    virtual void Seek(int64 NewPosition) override
    {
        if (Handle == INVALID_HANDLE_VALUE || NewPosition < 0)
        {
            SetError();
            return;
        }
        LARGE_INTEGER Distance;
        Distance.QuadPart = NewPosition;
        LARGE_INTEGER Result;
        if (!::SetFilePointerEx(Handle, Distance, &Result, FILE_BEGIN)
            || Result.QuadPart != NewPosition)
        {
            SetError();
            return;
        }
        Position = NewPosition;
    }

    virtual int64 Tell() override
    {
        return Position;
    }

    virtual int64 TotalSize() override
    {
        return Position;
    }

    virtual FString GetArchiveName() const override
    {
        return Filename;
    }

private:
    HANDLE Handle = INVALID_HANDLE_VALUE;
    FString Filename;
    int64 Position = 0;
};
#endif
}

FString DiscGolfRuntimeCheckpointJournal::NewCanonicalUuidV4()
{
    FString Value = FGuid::NewGuid().ToString(
        EGuidFormats::DigitsWithHyphensLower);
    if (Value.Len() != 36)
    {
        return FString();
    }

    // FGuid's formatted field order is not the native GUID memory order used
    // by Windows CoCreateGuid. Put the RFC 4122 version and variant markers in
    // their canonical string positions while retaining the variant's two
    // random low bits.
    Value[14] = TEXT('4');
    static constexpr TCHAR VariantCharacters[] = TEXT("89ab");
    const TCHAR SourceVariant = Value[19];
    int32 SourceNibble = INDEX_NONE;
    if (SourceVariant >= TEXT('0') && SourceVariant <= TEXT('9'))
    {
        SourceNibble = SourceVariant - TEXT('0');
    }
    else if (SourceVariant >= TEXT('a') && SourceVariant <= TEXT('f'))
    {
        SourceNibble = SourceVariant - TEXT('a') + 10;
    }
    if (SourceNibble == INDEX_NONE)
    {
        return FString();
    }
    Value[19] = VariantCharacters[SourceNibble & 0x3];
    return Value;
}

bool DiscGolfRuntimeCheckpointJournal::TryCreateExclusiveJournalWriter(
    const FString& AbsolutePath,
    TUniquePtr<FArchive>& OutWriter,
    FString& OutError)
{
    OutWriter.Reset();
    if (AbsolutePath.IsEmpty() || FPaths::IsRelative(AbsolutePath))
    {
        OutError = TEXT("journal path must be absolute");
        return false;
    }

#if PLATFORM_WINDOWS
    FString NativePath = FPaths::ConvertRelativePathToFull(AbsolutePath);
    FPaths::MakePlatformFilename(NativePath);
    HANDLE Handle = ::CreateFileW(
        *NativePath,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (Handle == INVALID_HANDLE_VALUE)
    {
        const DWORD ErrorCode = ::GetLastError();
        OutError = ErrorCode == ERROR_FILE_EXISTS || ErrorCode == ERROR_ALREADY_EXISTS
            ? TEXT("journal path already exists")
            : FString::Printf(
                TEXT("journal atomic create failed with platform error %u"),
                static_cast<uint32>(ErrorCode));
        return false;
    }
    OutWriter = MakeUnique<FExclusiveJournalArchive>(Handle, MoveTemp(NativePath));
    OutError.Reset();
    return true;
#else
    OutError = TEXT("atomic runtime-journal creation is supported only on Windows");
    return false;
#endif
}

bool DiscGolfRuntimeCheckpointJournal::TryValidateCaptureBinding(
    const FCaptureBinding& Binding,
    const FString& RedirectedUserDir,
    FString& OutError)
{
    if (!IsCandidateId(Binding.CandidateId))
    {
        OutError = TEXT("candidate ID is invalid");
        return false;
    }
    if (!IsCanonicalUuid(Binding.UserDirToken, false))
    {
        OutError = TEXT("UserDir token is not a canonical UUID");
        return false;
    }
    if (!IsUpperSha256(Binding.ExecutableSha256)
        || !IsUpperSha256(Binding.ArchiveManifestSha256))
    {
        OutError = TEXT("candidate hashes are not uppercase SHA-256 values");
        return false;
    }
    if (!IsCanonicalUuid(Binding.CaptureNonce, true))
    {
        OutError = TEXT("capture nonce is not a canonical UUIDv4");
        return false;
    }
    if (Binding.CaptureNonce == Binding.UserDirToken)
    {
        OutError = TEXT("capture nonce must be independent from the UserDir token");
        return false;
    }

    FString NormalizedUserDir = FPaths::ConvertRelativePathToFull(RedirectedUserDir);
    FPaths::NormalizeDirectoryName(NormalizedUserDir);
    if (FPaths::GetCleanFilename(NormalizedUserDir) != Binding.UserDirToken)
    {
        OutError = TEXT("redirected UserDir basename does not match its capture token");
        return false;
    }
    OutError.Reset();
    return true;
}

bool DiscGolfRuntimeCheckpointJournal::TrySerializeHeaderLine(
    const FCaptureBinding& Binding,
    const FString& RoundId,
    const FString& StartedUtc,
    FString& OutLine,
    FString& OutError)
{
    FString BindingError;
    if (!IsCandidateId(Binding.CandidateId)
        || !IsCanonicalUuid(Binding.UserDirToken, false)
        || !IsUpperSha256(Binding.ExecutableSha256)
        || !IsUpperSha256(Binding.ArchiveManifestSha256)
        || !IsCanonicalUuid(Binding.CaptureNonce, true))
    {
        OutError = TEXT("header capture binding is invalid");
        return false;
    }
    if (!IsCanonicalUuid(RoundId, true) || !IsValidUtcText(StartedUtc)
        || Binding.CaptureNonce == Binding.UserDirToken
        || RoundId == Binding.CaptureNonce || RoundId == Binding.UserDirToken)
    {
        OutError = TEXT("header round ID, nonce independence, or UTC timestamp is invalid");
        return false;
    }

    OutLine.Reset();
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutLine);
    Writer->WriteObjectStart();
    // Alphabetical key order is part of the canonical raw-byte contract.
    Writer->WriteValue(TEXT("archiveManifestSha256"), Binding.ArchiveManifestSha256);
    Writer->WriteValue(TEXT("candidateId"), Binding.CandidateId);
    Writer->WriteValue(TEXT("captureNonce"), Binding.CaptureNonce);
    Writer->WriteValue(TEXT("executableSha256"), Binding.ExecutableSha256);
    Writer->WriteValue(TEXT("recordType"), TEXT("HEADER"));
    Writer->WriteValue(TEXT("roundId"), RoundId);
    Writer->WriteValue(TEXT("schema"), Schema);
    Writer->WriteValue(TEXT("schemaVersion"), 1);
    Writer->WriteValue(TEXT("session"), 19);
    Writer->WriteValue(TEXT("startedUtc"), StartedUtc);
    Writer->WriteValue(TEXT("userDirToken"), Binding.UserDirToken);
    Writer->WriteObjectEnd();
    return CloseJsonLine(Writer, OutLine, OutError);
}

bool DiscGolfRuntimeCheckpointJournal::TrySerializeEventLine(
    const FCheckpointEvent& Event,
    FString& OutLine,
    FString& OutError)
{
    const bool bFinalScorecard = Event.Event == TEXT("FINAL_SCORECARD");
    if (Event.Sequence < 1 || Event.MonotonicMs < 0
        || Event.Event.IsEmpty() || !IsCanonicalUuid(Event.RoundId, true)
        || (Event.HoleNumber.IsSet()
            && (Event.HoleNumber.GetValue() < 1 || Event.HoleNumber.GetValue() > 3))
        || Event.CompletedHoles < 0 || Event.CompletedHoles > 3
        || Event.TotalStrokes < 0 || Event.TotalPenalties < 0
        || Event.TotalPenalties > Event.TotalStrokes
        || (bFinalScorecard != (Event.FinalScoreRows.Num() == 3)))
    {
        OutError = TEXT("event checkpoint fields are invalid");
        return false;
    }
    int32 FinalPar = 0;
    int32 FinalStrokes = 0;
    int32 FinalPenalties = 0;
    for (int32 Index = 0; Index < Event.FinalScoreRows.Num(); ++Index)
    {
        const FScoreRow& Row = Event.FinalScoreRows[Index];
        if (Row.HoleNumber != Index + 1 || Row.Par < 1 || Row.Strokes < 1
            || Row.Penalties < 0 || Row.Penalties > Row.Strokes)
        {
            OutError = TEXT("final scorecard row is invalid");
            return false;
        }
        FinalPar += Row.Par;
        FinalStrokes += Row.Strokes;
        FinalPenalties += Row.Penalties;
    }
    if (bFinalScorecard
        && (Event.CompletedHoles != 3
            || FinalStrokes != Event.TotalStrokes
            || FinalPenalties != Event.TotalPenalties))
    {
        OutError = TEXT("final scorecard totals do not reconcile");
        return false;
    }

    OutLine.Reset();
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&OutLine);
    Writer->WriteObjectStart();
    // Alphabetical key order is part of the canonical raw-byte contract.
    Writer->WriteValue(TEXT("completedHoles"), Event.CompletedHoles);
    Writer->WriteValue(TEXT("event"), Event.Event);
    if (bFinalScorecard)
    {
        Writer->WriteObjectStart(TEXT("finalScore"));
        Writer->WriteValue(TEXT("completedHoles"), Event.CompletedHoles);
        Writer->WriteArrayStart(TEXT("holeRows"));
        for (const FScoreRow& Row : Event.FinalScoreRows)
        {
            Writer->WriteObjectStart();
            Writer->WriteValue(TEXT("holeNumber"), Row.HoleNumber);
            Writer->WriteValue(TEXT("par"), Row.Par);
            Writer->WriteValue(TEXT("penalties"), Row.Penalties);
            Writer->WriteValue(TEXT("strokes"), Row.Strokes);
            Writer->WriteObjectEnd();
        }
        Writer->WriteArrayEnd();
        Writer->WriteValue(TEXT("parTotal"), FinalPar);
        Writer->WriteValue(TEXT("totalHoles"), Event.FinalScoreRows.Num());
        Writer->WriteValue(TEXT("totalPenalties"), FinalPenalties);
        Writer->WriteValue(TEXT("totalStrokes"), FinalStrokes);
        Writer->WriteObjectEnd();
    }
    if (Event.HoleNumber.IsSet())
    {
        Writer->WriteValue(TEXT("holeNumber"), Event.HoleNumber.GetValue());
    }
    else
    {
        Writer->WriteNull(TEXT("holeNumber"));
    }
    Writer->WriteValue(TEXT("monotonicMs"), Event.MonotonicMs);
    Writer->WriteValue(TEXT("recordType"), TEXT("EVENT"));
    Writer->WriteValue(TEXT("roundId"), Event.RoundId);
    Writer->WriteValue(TEXT("sequence"), Event.Sequence);
    Writer->WriteValue(TEXT("totalPenalties"), Event.TotalPenalties);
    Writer->WriteValue(TEXT("totalStrokes"), Event.TotalStrokes);
    Writer->WriteObjectEnd();
    return CloseJsonLine(Writer, OutLine, OutError);
}
