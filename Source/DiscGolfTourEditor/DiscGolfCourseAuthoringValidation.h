#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfCourseAuthoringValidation.generated.h"

class UDiscGolfCourseDefinition;

/**
 * Strict editor-only validation layered over the framework's portable course validator.
 * This reports authoring defects and never changes or repairs the supplied data asset.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfCourseAuthoringValidation : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Course Authoring|Validation")
    static TArray<FDGCourseValidationIssue> ValidateCourse(
        const UDiscGolfCourseDefinition* Course);

    UFUNCTION(BlueprintPure, Category="Disc Golf|Course Authoring|Validation")
    static bool HasErrors(const TArray<FDGCourseValidationIssue>& Issues);
};
