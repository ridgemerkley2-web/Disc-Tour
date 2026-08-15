#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfAITypes.h"
#include "DiscGolfAIGolferProfile.generated.h"

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAIGolferProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FName GolferId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    FDGAIGolferSkillProfile Skill;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AI")
    TArray<FDGDiscInstance> BagTemplate;
};
