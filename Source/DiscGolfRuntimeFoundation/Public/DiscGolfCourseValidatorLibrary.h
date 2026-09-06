#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfCourseValidatorLibrary.generated.h"

class UDiscGolfCourseDefinition;

UCLASS()
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfCourseValidatorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Disc Golf|Course|Validation")
    static TArray<FDGCourseValidationIssue> ValidateCourse(
        const UDiscGolfCourseDefinition* Course);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Course|Validation")
    static bool HasErrors(const TArray<FDGCourseValidationIssue>& Issues);
};
