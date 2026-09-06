#pragma once

#include "CoreMinimal.h"

namespace DiscGolfRuntimeCheckpointJournal
{
inline constexpr const TCHAR* Schema =
    TEXT("DiscGolfTour.Session19ThreeHoleRuntimeJournal.v1");
inline constexpr const TCHAR* UserDirRelativePath =
    TEXT("Saved/TechnicalEvidence/three-hole-runtime-journal-v1.jsonl");

struct FCaptureBinding
{
    FString CandidateId;
    FString UserDirToken;
    FString ExecutableSha256;
    FString ArchiveManifestSha256;
    FString CaptureNonce;
};

struct FScoreRow
{
    int32 HoleNumber = 0;
    int32 Par = 0;
    int32 Strokes = 0;
    int32 Penalties = 0;
};

struct FCheckpointEvent
{
    int32 Sequence = 0;
    int64 MonotonicMs = 0;
    FString Event;
    FString RoundId;
    TOptional<int32> HoleNumber;
    int32 CompletedHoles = 0;
    int32 TotalStrokes = 0;
    int32 TotalPenalties = 0;
    TArray<FScoreRow> FinalScoreRows;
};

/**
 * Generates a lowercase RFC 4122 UUIDv4 string. FGuid::NewGuid stores the
 * native Windows GUID fields in FGuid's uint32 layout, so formatting that
 * value directly does not reliably leave the RFC version and variant nibbles
 * at string offsets 14 and 19.
 */
DISCGOLFTOUR_API FString NewCanonicalUuidV4();

/**
 * Atomically creates a new runtime-journal file and returns its open writer.
 * Existing paths are never opened or truncated. The Windows Shipping capture
 * uses CREATE_NEW because UE 5.8's generic FILEWRITE_NoReplaceExisting flag
 * is not forwarded to the platform OpenWrite call.
 */
DISCGOLFTOUR_API bool TryCreateExclusiveJournalWriter(
    const FString& AbsolutePath,
    TUniquePtr<FArchive>& OutWriter,
    FString& OutError);

/**
 * Validates the complete opt-in binding and proves that the redirected UserDir
 * basename is the same canonical UUID supplied by the external runner.
 */
DISCGOLFTOUR_API bool TryValidateCaptureBinding(
    const FCaptureBinding& Binding,
    const FString& RedirectedUserDir,
    FString& OutError);

/** Canonical, condensed, ASCII-only JSONL header (including its trailing LF). */
DISCGOLFTOUR_API bool TrySerializeHeaderLine(
    const FCaptureBinding& Binding,
    const FString& RoundId,
    const FString& StartedUtc,
    FString& OutLine,
    FString& OutError);

/** Canonical, condensed, ASCII-only JSONL event (including its trailing LF). */
DISCGOLFTOUR_API bool TrySerializeEventLine(
    const FCheckpointEvent& Event,
    FString& OutLine,
    FString& OutError);
}
