#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DiscGolfAITypes.h"
#include "DiscGolfAIGolferProfile.generated.h"

UCLASS(BlueprintType)
class DISCGOLFTOURDEVELOPER_API UDiscGolfAIGolferProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GolferId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGAISkillProfile Skill;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGDiscInstance> BagTemplate;
};
