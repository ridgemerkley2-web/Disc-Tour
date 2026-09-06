#include "DiscGolfCareerSubsystem.h"

bool UDiscGolfCareerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    return DG_WITH_CAREER_AI != 0 && Super::ShouldCreateSubsystem(Outer);
}

#include "DiscGolfRoundState.h"
#include "DiscGolfScoringLibrary.h"
#include "Kismet/GameplayStatics.h"

namespace
{
bool IsStableId(const FName Id)
{
    if (Id.IsNone())
    {
        return false;
    }
    const FString Text = Id.ToString();
    if (Text.IsEmpty() || Text.Len() > 64)
    {
        return false;
    }
    for (const TCHAR Character : Text)
    {
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-'))
        {
            return false;
        }
    }
    return true;
}

bool ValidateHistoryScorecard(const FDGRoundScorecard& Scorecard, FString& OutError)
{
    if (!IsStableId(Scorecard.EventId) || !IsStableId(Scorecard.CourseId)
        || Scorecard.RoundNumber < 1 || Scorecard.RoundNumber > 64
        || Scorecard.Holes.IsEmpty() || Scorecard.Holes.Num() > 36)
    {
        OutError = TEXT("career scorecard identity, round number or hole count is invalid");
        return false;
    }
    for (int32 Index = 0; Index < Scorecard.Holes.Num(); ++Index)
    {
        const FDGScorecardHole& Hole = Scorecard.Holes[Index];
        if (Hole.HoleNumber != Index + 1 || Hole.Par < 1 || Hole.Par > 9
            || Hole.Strokes < 0 || Hole.Strokes > 99
            || Hole.PenaltyStrokes < 0 || Hole.PenaltyStrokes > 99
            || Hole.Strokes + Hole.PenaltyStrokes <= 0
            || Hole.Strokes + Hole.PenaltyStrokes > 99 || !Hole.bComplete)
        {
            OutError = TEXT("career scorecard contains an invalid or incomplete hole");
            return false;
        }
    }
    if (!UDiscGolfScoringLibrary::IsRoundComplete(Scorecard))
    {
        OutError = TEXT("career scorecard is incomplete");
        return false;
    }
    return true;
}
}

void UDiscGolfCareerSubsystem::ResetToDefaults()
{
    Progress = FDGCareerProgress();
}

bool UDiscGolfCareerSubsystem::ValidateProgress(
    const FDGCareerProgress& Candidate,
    FString& OutError)
{
    if (Candidate.SeasonNumber < 1 || Candidate.SeasonNumber > 1000
        || Candidate.CareerCurrency < 0 || Candidate.CareerCurrency > 1000000000
        || !FMath::IsFinite(Candidate.PlayerRating)
        || Candidate.PlayerRating < 0.0f || Candidate.PlayerRating > 3000.0f
        || Candidate.WorldRank < 0 || Candidate.WorldRank > 10000000)
    {
        OutError = TEXT("career scalar progress is invalid");
        return false;
    }
    if (Candidate.CompletedEventIds.Num() > MaxCompletedEvents
        || Candidate.RoundHistory.Num() > MaxRoundHistory
        || Candidate.Sponsorships.Num() > MaxSponsorships
        || Candidate.CompletedEventIds.Num() != Candidate.RoundHistory.Num())
    {
        OutError = TEXT("career collections exceed bounds or event/result counts disagree");
        return false;
    }

    TSet<FName> EventIds;
    for (const FName EventId : Candidate.CompletedEventIds)
    {
        if (!IsStableId(EventId) || EventIds.Contains(EventId))
        {
            OutError = TEXT("career contains an invalid or duplicate completed event");
            return false;
        }
        EventIds.Add(EventId);
    }

    TSet<FName> ResultIds;
    TSet<FName> ResultEventIds;
    for (const FDGRoundScorecard& Scorecard : Candidate.RoundHistory)
    {
        if (!ValidateHistoryScorecard(Scorecard, OutError))
        {
            return false;
        }
        const FName ResultId = DiscGolfCompetitionRuntime::MakeResultId(Scorecard);
        if (ResultId.IsNone() || ResultIds.Contains(ResultId))
        {
            OutError = TEXT("career contains an invalid or duplicate round result");
            return false;
        }
        if (!EventIds.Contains(Scorecard.EventId) || ResultEventIds.Contains(Scorecard.EventId))
        {
            OutError = TEXT("career round history does not map one-to-one to completed events");
            return false;
        }
        ResultIds.Add(ResultId);
        ResultEventIds.Add(Scorecard.EventId);
    }
    if (ResultEventIds.Num() != EventIds.Num())
    {
        OutError = TEXT("career completed event is missing its round result");
        return false;
    }

    TSet<FName> SponsorshipBrands;
    for (const FDGSponsorshipProgress& Sponsorship : Candidate.Sponsorships)
    {
        if (!IsStableId(Sponsorship.BrandId)
            || SponsorshipBrands.Contains(Sponsorship.BrandId)
            || Sponsorship.Tier < 0 || Sponsorship.Tier > 100
            || !FMath::IsFinite(Sponsorship.Reputation)
            || Sponsorship.Reputation < 0.0f || Sponsorship.Reputation > 1000000000.0f)
        {
            OutError = TEXT("career sponsorship progress is invalid or duplicated");
            return false;
        }
        SponsorshipBrands.Add(Sponsorship.BrandId);
    }

    OutError.Reset();
    return true;
}

bool UDiscGolfCareerSubsystem::CommitCompletedRound(
    const FDiscGolfCompetitionRuntimeDefinition& Event,
    const FDiscGolfRoundState& Round,
    FString& OutError)
{
    if (!ValidateProgress(Progress, OutError))
    {
        return false;
    }

    FDGRoundScorecard Scorecard;
    if (!DiscGolfCompetitionRuntime::BuildCompletedScorecard(
            Event, Round, Scorecard, OutError))
    {
        return false;
    }
    const FName ResultId = DiscGolfCompetitionRuntime::MakeResultId(Scorecard);
    if (Progress.CompletedEventIds.Contains(Event.EventId))
    {
        OutError = TEXT("career event has already been completed");
        return false;
    }
    if (Progress.RoundHistory.ContainsByPredicate(
            [ResultId](const FDGRoundScorecard& Existing)
            {
                return DiscGolfCompetitionRuntime::MakeResultId(Existing) == ResultId;
            }))
    {
        OutError = TEXT("career round result has already been recorded");
        return false;
    }
    if (Progress.CompletedEventIds.Num() >= MaxCompletedEvents
        || Progress.RoundHistory.Num() >= MaxRoundHistory)
    {
        OutError = TEXT("career history is full");
        return false;
    }

    FDGCareerProgress Candidate = Progress;
    Candidate.CompletedEventIds.Add(Event.EventId);
    Candidate.RoundHistory.Add(MoveTemp(Scorecard));
    if (!ValidateProgress(Candidate, OutError))
    {
        return false;
    }

    Progress = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

FString UDiscGolfCareerSubsystem::DefaultSaveSlot()
{
    return TEXT("DGT_Career_Dev_v1");
}

bool UDiscGolfCareerSubsystem::SaveCareer(FString& OutError) const
{
    return SaveCareerToSlot(DefaultSaveSlot(), 0, OutError);
}

bool UDiscGolfCareerSubsystem::LoadCareer(FString& OutError)
{
    return LoadCareerFromSlot(DefaultSaveSlot(), 0, OutError);
}

bool UDiscGolfCareerSubsystem::SaveCareerToSlot(
    const FString& SlotName,
    const int32 UserIndex,
    FString& OutError) const
{
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        OutError = TEXT("career save slot and user index are invalid");
        return false;
    }
    if (!ValidateProgress(Progress, OutError))
    {
        return false;
    }

    UDiscGolfCareerProgressSaveGame* Save = Cast<UDiscGolfCareerProgressSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UDiscGolfCareerProgressSaveGame::StaticClass()));
    if (!Save)
    {
        OutError = TEXT("career could not allocate its schema-v1 save container");
        return false;
    }
    Save->Progress = Progress;
    if (!UGameplayStatics::SaveGameToSlot(Save, SlotName, UserIndex))
    {
        OutError = FString::Printf(TEXT("career failed to write save slot %s"), *SlotName);
        return false;
    }

    OutError.Reset();
    return true;
}

bool UDiscGolfCareerSubsystem::LoadCareerFromSaveGame(
    const UDiscGolfCareerProgressSaveGame* Save,
    FString& OutError)
{
    if (!Save)
    {
        OutError = TEXT("career save is missing");
        return false;
    }
    if (Save->SchemaVersion != UDiscGolfCareerProgressSaveGame::CurrentSchemaVersion)
    {
        OutError = TEXT("career save is not schema v1");
        return false;
    }

    FDGCareerProgress Candidate = Save->Progress;
    if (!ValidateProgress(Candidate, OutError))
    {
        return false;
    }
    Progress = MoveTemp(Candidate);
    OutError.Reset();
    return true;
}

bool UDiscGolfCareerSubsystem::LoadCareerFromSlot(
    const FString& SlotName,
    const int32 UserIndex,
    FString& OutError)
{
    if (SlotName.IsEmpty() || UserIndex < 0)
    {
        OutError = TEXT("career save slot and user index are invalid");
        return false;
    }
    const UDiscGolfCareerProgressSaveGame* Save = Cast<UDiscGolfCareerProgressSaveGame>(
        UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
    return LoadCareerFromSaveGame(Save, OutError);
}
