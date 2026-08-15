#include "DiscGolfAIShotPlannerComponent.h"

UDiscGolfAIShotPlannerComponent::UDiscGolfAIShotPlannerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

float UDiscGolfAIShotPlannerComponent::ScoreCandidate(
    const FDGAIShotCandidate& Candidate) const
{
    const float RiskAversion = 1.0f - FMath::Clamp(Skill.Aggression, 0.0f, 1.0f);
    const float Management = FMath::Clamp(Skill.CourseManagement, 0.0f, 1.0f);
    const float Accuracy = FMath::Clamp(Skill.Accuracy, 0.0f, 1.0f);

    const float ProgressScore = Candidate.ExpectedProgressM * 0.40f;
    const float SuccessScore = Candidate.SuccessProbability * 80.0f;
    const float OBPenalty = Candidate.OutOfBoundsProbability * (70.0f + 100.0f * RiskAversion);
    const float ObstaclePenalty = Candidate.ObstacleHitProbability * (35.0f + 60.0f * Management);
    const float ErrorPenalty = Candidate.LandingErrorM * FMath::Lerp(0.65f, 0.20f, Accuracy);

    return ProgressScore + SuccessScore - OBPenalty - ObstaclePenalty - ErrorPenalty;
}

bool UDiscGolfAIShotPlannerComponent::SelectBestCandidate(
    const TArray<FDGAIShotCandidate>& Candidates,
    FDGAIShotCandidate& OutCandidate) const
{
    if (Candidates.Num() <= 0)
    {
        return false;
    }

    float BestScore = -BIG_NUMBER;
    int32 BestIndex = INDEX_NONE;

    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        const float Score = ScoreCandidate(Candidates[Index]);
        if (Score > BestScore)
        {
            BestScore = Score;
            BestIndex = Index;
        }
    }

    if (!Candidates.IsValidIndex(BestIndex))
    {
        return false;
    }

    OutCandidate = Candidates[BestIndex];
    OutCandidate.UtilityScore = BestScore;
    return true;
}
