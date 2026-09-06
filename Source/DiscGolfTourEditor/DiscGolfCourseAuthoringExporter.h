#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DiscGolfCourseAuthoringExporter.generated.h"

class ADiscGolfHoleAuthoringActor;
class UDiscGolfCourseDefinition;

/**
 * Editor-only bridge from placed project authoring actors to the framework course DTO.
 * The resulting Data Asset is supplemental authoring data; project runtime course loading
 * remains owned by the DiscGolfTour runtime module.
 */
UCLASS()
class DISCGOLFTOUREDITOR_API UDiscGolfCourseAuthoringExporter : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Builds and strictly validates a transient candidate before touching TargetCourse.
     * Any conversion or validation error leaves every target field and its package unchanged.
     */
    UFUNCTION(BlueprintCallable, Category="Disc Golf|Course Authoring|Export")
    static bool UpdateCourseDefinition(
        UDiscGolfCourseDefinition* TargetCourse,
        FName CourseId,
        FText DisplayName,
        FName BrandId,
        FName BiomeId,
        const TArray<ADiscGolfHoleAuthoringActor*>& HoleRoots,
        TArray<FDGCourseValidationIssue>& OutIssues,
        FString& OutError);
};

/** Value-only seam used by the actor bridge and deterministic editor automation. */
namespace DiscGolfCourseAuthoringExporter
{
    DISCGOLFTOUREDITOR_API bool UpdateCourseDefinitionFromHoles(
        UDiscGolfCourseDefinition* TargetCourse,
        FName CourseId,
        const FText& DisplayName,
        FName BrandId,
        FName BiomeId,
        const TArray<FDGHoleDefinition>& SourceHoles,
        TArray<FDGCourseValidationIssue>& OutIssues,
        FString& OutError);
}
