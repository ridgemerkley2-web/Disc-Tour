#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfCourseTypes.h"
#include "DiscGolfCourseValidatorLibrary.generated.h"

class UDiscGolfCourseDefinition;

UCLASS()
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfCourseValidatorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Course Validation")
    static TArray<FDGCourseValidationIssue> ValidateCourse(
        const UDiscGolfCourseDefinition* Course
    );

    UFUNCTION(BlueprintPure, Category="Disc Golf|Course Validation")
    static bool HasErrors(const TArray<FDGCourseValidationIssue>& Issues);
};
