#include "DiscGolfAIShotPlannerComponent.h"

bool UDiscGolfAIShotPlannerComponent::SelectBestCandidate(
    const TArray<FDGAIShotCandidate>& Candidates,
    FDGAIShotCandidate& OutCandidate) const
{
    if (Candidates.IsEmpty())
    {
        return false;
    }
    int32 BestIndex = 0;
    float BestUtility = -TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        const FDGAIShotCandidate& Candidate = Candidates[Index];
        const float Utility = Candidate.ExpectedProgressM
            + Candidate.SuccessProbability * 50.0f
            - Candidate.OutOfBoundsProbability * (50.0f + Skill.CourseManagement * 50.0f)
            - Candidate.ObstacleHitProbability * 35.0f
            - Candidate.LandingErrorM * (0.5f + Skill.Accuracy);
        if (FMath::IsFinite(Utility) && Utility > BestUtility)
        {
            BestUtility = Utility;
            BestIndex = Index;
        }
    }
    OutCandidate = Candidates[BestIndex];
    OutCandidate.UtilityScore = BestUtility;
    return FMath::IsFinite(BestUtility);
}
