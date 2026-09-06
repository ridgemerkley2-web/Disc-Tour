#include "DiscGolfCourseAuthoringExporter.h"

#include "DGFrameworkCourseDefinition.h"
#include "DiscGolfCourseAuthoringActors.h"
#include "DiscGolfCourseAuthoringValidation.h"
#include "UObject/Package.h"

namespace
{
    void AddExportIssue(
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

    void SortIssues(TArray<FDGCourseValidationIssue>& Issues)
    {
        Issues.StableSort([](const FDGCourseValidationIssue& A, const FDGCourseValidationIssue& B)
        {
            if (A.HoleNumber != B.HoleNumber) return A.HoleNumber < B.HoleNumber;
            if (A.Severity != B.Severity)
            {
                return static_cast<uint8>(A.Severity) > static_cast<uint8>(B.Severity);
            }
            return A.Code.LexicalLess(B.Code);
        });
    }

    void SortHoles(TArray<FDGHoleDefinition>& Holes)
    {
        Holes.Sort([](const FDGHoleDefinition& A, const FDGHoleDefinition& B)
        {
            if (A.HoleNumber != B.HoleNumber) return A.HoleNumber < B.HoleNumber;
            return A.HoleId.LexicalLess(B.HoleId);
        });
    }

    bool ReportDuplicateHoleIdentity(
        const TArray<FDGHoleDefinition>& Holes,
        TArray<FDGCourseValidationIssue>& OutIssues)
    {
        TSet<int32> HoleNumbers;
        TSet<FName> HoleIds;
        for (const FDGHoleDefinition& Hole : Holes)
        {
            if (HoleNumbers.Contains(Hole.HoleNumber))
            {
                AddExportIssue(OutIssues, TEXT("HOLE_NUMBER_DUPLICATE"), Hole.HoleNumber,
                    FString::Printf(TEXT("Duplicate authored hole number: %d."), Hole.HoleNumber));
            }
            HoleNumbers.Add(Hole.HoleNumber);

            if (!Hole.HoleId.IsNone() && HoleIds.Contains(Hole.HoleId))
            {
                AddExportIssue(OutIssues, TEXT("HOLE_ID_DUPLICATE"), Hole.HoleNumber,
                    FString::Printf(TEXT("Duplicate authored HoleId: %s."), *Hole.HoleId.ToString()));
            }
            HoleIds.Add(Hole.HoleId);
        }
        return !OutIssues.IsEmpty();
    }

    FString FirstErrorMessage(const TArray<FDGCourseValidationIssue>& Issues)
    {
        const FDGCourseValidationIssue* FirstError = Issues.FindByPredicate(
            [](const FDGCourseValidationIssue& Issue)
            {
                return Issue.Severity == EDGValidationSeverity::Error;
            });
        return FirstError ? FirstError->Message : FString{};
    }
}

bool DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
    UDiscGolfCourseDefinition* TargetCourse,
    FName CourseId,
    const FText& DisplayName,
    FName BrandId,
    FName BiomeId,
    const TArray<FDGHoleDefinition>& SourceHoles,
    TArray<FDGCourseValidationIssue>& OutIssues,
    FString& OutError)
{
    OutIssues.Reset();
    OutError.Reset();
    if (!IsValid(TargetCourse))
    {
        OutError = TEXT("Target course definition is null or invalid.");
        AddExportIssue(OutIssues, TEXT("COURSE_TARGET_INVALID"), 0, OutError);
        return false;
    }

    TArray<FDGHoleDefinition> SortedHoles = SourceHoles;
    SortHoles(SortedHoles);
    if (ReportDuplicateHoleIdentity(SortedHoles, OutIssues))
    {
        SortIssues(OutIssues);
        OutError = FirstErrorMessage(OutIssues);
        return false;
    }

    UDiscGolfCourseDefinition* Candidate =
        NewObject<UDiscGolfCourseDefinition>(GetTransientPackage());
    Candidate->CourseId = CourseId;
    Candidate->DisplayName = DisplayName;
    Candidate->BrandId = BrandId;
    Candidate->BiomeId = BiomeId;
    Candidate->Holes = MoveTemp(SortedHoles);

    OutIssues = UDiscGolfCourseAuthoringValidation::ValidateCourse(Candidate);
    if (UDiscGolfCourseAuthoringValidation::HasErrors(OutIssues))
    {
        OutError = FirstErrorMessage(OutIssues);
        return false;
    }

    // This is the commit boundary. No target mutation, transaction, or dirty flag occurs above it.
    TargetCourse->Modify();
    TargetCourse->CourseId = Candidate->CourseId;
    TargetCourse->DisplayName = Candidate->DisplayName;
    TargetCourse->BrandId = Candidate->BrandId;
    TargetCourse->BiomeId = Candidate->BiomeId;
    TargetCourse->Holes = Candidate->Holes;
    TargetCourse->MarkPackageDirty();
    return true;
}

bool UDiscGolfCourseAuthoringExporter::UpdateCourseDefinition(
    UDiscGolfCourseDefinition* TargetCourse,
    FName CourseId,
    FText DisplayName,
    FName BrandId,
    FName BiomeId,
    const TArray<ADiscGolfHoleAuthoringActor*>& HoleRoots,
    TArray<FDGCourseValidationIssue>& OutIssues,
    FString& OutError)
{
    OutIssues.Reset();
    OutError.Reset();

    TArray<const ADiscGolfHoleAuthoringActor*> SortedRoots;
    SortedRoots.Reserve(HoleRoots.Num());
    for (const ADiscGolfHoleAuthoringActor* Root : HoleRoots)
    {
        if (!IsValid(Root))
        {
            OutError = TEXT("Course export contains a null or invalid hole root.");
            AddExportIssue(OutIssues, TEXT("AUTHORING_ROOT_INVALID"), 0, OutError);
            return false;
        }
        SortedRoots.Add(Root);
    }
    SortedRoots.Sort([](
        const ADiscGolfHoleAuthoringActor& A,
        const ADiscGolfHoleAuthoringActor& B)
    {
        if (A.HoleNumber != B.HoleNumber) return A.HoleNumber < B.HoleNumber;
        return A.HoleId.LexicalLess(B.HoleId);
    });

    TArray<FDGHoleDefinition> Holes;
    Holes.Reserve(SortedRoots.Num());
    for (const ADiscGolfHoleAuthoringActor* Root : SortedRoots)
    {
        FDGHoleDefinition Hole;
        FString ConversionError;
        if (!Root->BuildHoleDefinition(Hole, ConversionError))
        {
            OutError = FString::Printf(TEXT("Hole %d (%s) conversion failed: %s"),
                Root->HoleNumber, *Root->HoleId.ToString(), *ConversionError);
            AddExportIssue(OutIssues, TEXT("HOLE_CONVERSION_FAILED"), Root->HoleNumber, OutError);
            return false;
        }
        Holes.Add(MoveTemp(Hole));
    }

    return DiscGolfCourseAuthoringExporter::UpdateCourseDefinitionFromHoles(
        TargetCourse, CourseId, DisplayName, BrandId, BiomeId,
        Holes, OutIssues, OutError);
}
