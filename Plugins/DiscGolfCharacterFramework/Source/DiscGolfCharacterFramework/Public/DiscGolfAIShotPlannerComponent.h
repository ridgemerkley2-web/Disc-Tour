#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAITypes.h"
#include "DiscGolfAIShotPlannerComponent.generated.h"

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAIShotPlannerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfAIShotPlannerComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|AI")
    FDGAIGolferSkillProfile Skill;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|AI")
    bool SelectBestCandidate(
        const TArray<FDGAIShotCandidate>& Candidates,
        FDGAIShotCandidate& OutCandidate
    ) const;

    UFUNCTION(BlueprintPure, Category="Disc Golf|AI")
    float ScoreCandidate(const FDGAIShotCandidate& Candidate) const;

    // Build candidates from the project's actual flight predictor/simulator.
    UFUNCTION(BlueprintImplementableEvent, Category="Disc Golf|AI")
    void BuildShotCandidates(
        const FDGAIShotContext& Context,
        TArray<FDGAIShotCandidate>& OutCandidates
    );
};
