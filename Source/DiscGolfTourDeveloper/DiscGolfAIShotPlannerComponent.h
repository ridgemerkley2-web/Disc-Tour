#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfAITypes.h"
#include "DiscGolfAIShotPlannerComponent.generated.h"

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOURDEVELOPER_API UDiscGolfAIShotPlannerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGAISkillProfile Skill;
    UFUNCTION(BlueprintCallable) bool SelectBestCandidate(const TArray<FDGAIShotCandidate>& Candidates, FDGAIShotCandidate& OutCandidate) const;
};
