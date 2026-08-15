#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"

/**
 * Semantic presentation domains. This vocabulary deliberately describes what happened,
 * not which sound asset should play. A future audio director can map EventId to assets.
 */
enum class EDiscGolfPresentationAudioCategory : uint8
{
    Invalid,
    ThrowRelease,
    AirborneFlight,
    GroundContact,
    GroundState,
    BasketOutcome,
    Penalty,
    HoleStart,
    HoleCompletion,
    HoleTransition,
    RoundCompletion,
    Replay,
    Flyover
};

/** Immutable lifecycle context attached by the presentation boundary. */
struct DISCGOLFTOUR_API FDiscGolfPresentationAudioContext
{
    int32 ShotSequence = 0;
    int32 HoleNumber = 0;
    FVector WorldLocationCm = FVector::ZeroVector;
    float EventTimeSeconds = 0.0f;
    bool bReplayPresentation = false;
    ECourseSurfaceType CourseSurface = ECourseSurfaceType::Fairway;
    EDiscGroundState GroundState = EDiscGroundState::Airborne;
    EBasketContactResult BasketResult = EBasketContactResult::None;
    EDiscGolfPenaltyType Penalty = EDiscGolfPenaltyType::None;

    bool IsValid() const;
};

/**
 * Immutable value returned by the presentation-audio resolver.
 * It owns no UObject, world, score, collision, or simulation state.
 */
struct DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent
{
    int32 ContractVersion = 1;
    EDiscGolfPresentationAudioCategory Category = EDiscGolfPresentationAudioCategory::Invalid;
    FName EventId = NAME_None;
    FString Label;
    float Intensity01 = 0.0f;
    float PitchMultiplier = 1.0f;
    bool bHasContext = false;
    FDiscGolfPresentationAudioContext Context;

    bool IsValid() const;
    FString DedupeKey() const;
};

/**
 * Pure, deterministic conversion from existing gameplay results to presentation intent.
 * Callers remain responsible for deciding when to request and play an event.
 */
namespace DiscGolfPresentationAudio
{
    inline constexpr int32 CurrentContractVersion = 1;
    inline constexpr int32 MaximumTraceEvents = 64;
    inline constexpr float MinimumPitchMultiplier = 0.80f;
    inline constexpr float MaximumPitchMultiplier = 1.20f;

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveThrowRelease(
        EReleaseGrade Grade,
        EReleaseTiming Timing,
        float Quality01,
        float ReleaseSpeedMps);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveAirborneFlight(
        float SpeedMps,
        float SpinRpm,
        float FlightProgress01);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveGroundContact(
        EGroundSurfaceType Surface,
        float ImpactSpeedMps,
        float IncidenceAngleDeg);

    /** Rich course identity keeps hazard/water and rough presentation distinct. */
    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveCourseSurfaceContact(
        EGroundSurfaceType PhysicalSurface,
        ECourseSurfaceType CourseSurface,
        float ImpactSpeedMps,
        float IncidenceAngleDeg,
        FName PresentationMaterialId = NAME_None);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveGroundState(
        EDiscGroundState State,
        EGroundSurfaceType Surface,
        float SpeedMps);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveBasketOutcome(
        EBasketContactResult Result,
        float IncomingSpeedMps);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolvePenalty(
        EDiscGolfPenaltyType Penalty,
        int32 PenaltyStrokes);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveHoleStart(
        int32 HoleNumber,
        int32 TotalHoleCount,
        int32 Par);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveHoleCompletion(
        int32 HoleNumber,
        int32 Strokes,
        int32 Par);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveHoleTransition(
        int32 FromHoleNumber,
        int32 ToHoleNumber,
        int32 TotalHoleCount);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveRoundCompletion(
        int32 CompletedHoleCount,
        int32 TotalHoleCount,
        int32 ScoreToPar);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveReplay(bool bStarting);
    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent ResolveFlyover(bool bStarting);

    DISCGOLFTOUR_API FDiscGolfPresentationAudioEvent WithContext(
        const FDiscGolfPresentationAudioEvent& Event,
        const FDiscGolfPresentationAudioContext& Context);

    DISCGOLFTOUR_API bool ShouldEmit(
        const FDiscGolfPresentationAudioEvent& Event,
        bool bRegressionActive);

    DISCGOLFTOUR_API bool AppendBoundedTrace(
        TArray<FDiscGolfPresentationAudioEvent>& Trace,
        const FDiscGolfPresentationAudioEvent& Event,
        int32 MaximumEvents = MaximumTraceEvents);
}
