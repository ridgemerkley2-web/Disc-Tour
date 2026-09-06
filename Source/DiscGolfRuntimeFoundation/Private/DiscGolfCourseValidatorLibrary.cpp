#include "DiscGolfCourseValidatorLibrary.h"

#include "DGFrameworkCourseDefinition.h"

namespace
{
    void AddError(
        TArray<FDGCourseValidationIssue>& Issues,
        FName Code,
        int32 HoleNumber,
        const FString& Message)
    {
        FDGCourseValidationIssue& Issue = Issues.AddDefaulted_GetRef();
        Issue.Severity = EDGValidationSeverity::Error;
        Issue.Code = Code;
        Issue.HoleNumber = HoleNumber;
        Issue.Message = Message;
    }
}

TArray<FDGCourseValidationIssue> UDiscGolfCourseValidatorLibrary::ValidateCourse(
    const UDiscGolfCourseDefinition* Course)
{
    TArray<FDGCourseValidationIssue> Issues;
    if (!IsValid(Course))
    {
        AddError(Issues, TEXT("COURSE_INVALID"), 0, TEXT("Course definition is null or invalid."));
        return Issues;
    }
    if (Course->CourseId.IsNone())
    {
        AddError(Issues, TEXT("COURSE_ID_MISSING"), 0, TEXT("Course requires an explicit stable ID."));
    }
    if (Course->Holes.IsEmpty())
    {
        AddError(Issues, TEXT("COURSE_HOLES_MISSING"), 0, TEXT("Course requires at least one hole."));
        return Issues;
    }

    TSet<FName> HoleIds;
    TSet<int32> HoleNumbers;
    for (const FDGHoleDefinition& Hole : Course->Holes)
    {
        if (Hole.HoleId.IsNone())
        {
            AddError(Issues, TEXT("HOLE_ID_MISSING"), Hole.HoleNumber,
                TEXT("Hole requires an explicit stable ID."));
        }
        else if (HoleIds.Contains(Hole.HoleId))
        {
            AddError(Issues, TEXT("HOLE_ID_DUPLICATE"), Hole.HoleNumber,
                TEXT("Course contains a duplicate hole ID."));
        }
        HoleIds.Add(Hole.HoleId);

        if (Hole.HoleNumber < 1)
        {
            AddError(Issues, TEXT("HOLE_NUMBER_INVALID"), Hole.HoleNumber,
                TEXT("Hole number must be positive."));
        }
        else if (HoleNumbers.Contains(Hole.HoleNumber))
        {
            AddError(Issues, TEXT("HOLE_NUMBER_DUPLICATE"), Hole.HoleNumber,
                TEXT("Course contains a duplicate hole number."));
        }
        HoleNumbers.Add(Hole.HoleNumber);
    }
    return Issues;
}

bool UDiscGolfCourseValidatorLibrary::HasErrors(
    const TArray<FDGCourseValidationIssue>& Issues)
{
    return Issues.ContainsByPredicate([](const FDGCourseValidationIssue& Issue)
    {
        return Issue.Severity == EDGValidationSeverity::Error;
    });
}
