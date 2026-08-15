#include "DiscGolfCourseValidatorLibrary.h"
#include "DGFrameworkCourseDefinition.h"

static void DGAddCourseIssue(
    TArray<FDGCourseValidationIssue>& Issues,
    EDGValidationSeverity Severity,
    FName Code,
    int32 HoleNumber,
    const FString& Message)
{
    FDGCourseValidationIssue Issue;
    Issue.Severity = Severity;
    Issue.Code = Code;
    Issue.HoleNumber = HoleNumber;
    Issue.Message = Message;
    Issues.Add(Issue);
}

TArray<FDGCourseValidationIssue> UDiscGolfCourseValidatorLibrary::ValidateCourse(
    const UDiscGolfCourseDefinition* Course)
{
    TArray<FDGCourseValidationIssue> Issues;

    if (!Course)
    {
        DGAddCourseIssue(
            Issues,
            EDGValidationSeverity::Error,
            TEXT("COURSE_NULL"),
            0,
            TEXT("Course definition is null.")
        );
        return Issues;
    }

    if (Course->CourseId.IsNone())
    {
        DGAddCourseIssue(
            Issues,
            EDGValidationSeverity::Error,
            TEXT("COURSE_ID_MISSING"),
            0,
            TEXT("CourseId is required.")
        );
    }

    if (Course->Holes.Num() <= 0)
    {
        DGAddCourseIssue(
            Issues,
            EDGValidationSeverity::Error,
            TEXT("COURSE_HAS_NO_HOLES"),
            0,
            TEXT("Course contains no holes.")
        );
        return Issues;
    }

    TSet<int32> HoleNumbers;
    TSet<FName> HoleIds;

    for (const FDGHoleDefinition& Hole : Course->Holes)
    {
        if (Hole.HoleId.IsNone())
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Error,
                TEXT("HOLE_ID_MISSING"),
                Hole.HoleNumber,
                TEXT("HoleId is required.")
            );
        }
        else if (HoleIds.Contains(Hole.HoleId))
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Error,
                TEXT("HOLE_ID_DUPLICATE"),
                Hole.HoleNumber,
                FString::Printf(TEXT("Duplicate HoleId: %s"), *Hole.HoleId.ToString())
            );
        }
        HoleIds.Add(Hole.HoleId);

        if (Hole.HoleNumber <= 0)
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Error,
                TEXT("HOLE_NUMBER_INVALID"),
                Hole.HoleNumber,
                TEXT("HoleNumber must be greater than zero.")
            );
        }
        else if (HoleNumbers.Contains(Hole.HoleNumber))
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Error,
                TEXT("HOLE_NUMBER_DUPLICATE"),
                Hole.HoleNumber,
                TEXT("HoleNumber must be unique.")
            );
        }
        HoleNumbers.Add(Hole.HoleNumber);

        if (Hole.Par < 2 || Hole.Par > 6)
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Warning,
                TEXT("PAR_UNUSUAL"),
                Hole.HoleNumber,
                TEXT("Par is outside the recommended 2-6 range.")
            );
        }

        const float TeeBasketDistanceM = FVector::Dist2D(
            Hole.TeeTransform.GetLocation(),
            Hole.BasketTransform.GetLocation()
        ) / 100.0f;

        if (TeeBasketDistanceM < 5.0f)
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Error,
                TEXT("TEE_BASKET_TOO_CLOSE"),
                Hole.HoleNumber,
                TEXT("Tee and basket are too close or not authored.")
            );
        }

        if (Hole.PublishedDistanceM > 0.0f &&
            FMath::Abs(Hole.PublishedDistanceM - TeeBasketDistanceM) > 20.0f)
        {
            DGAddCourseIssue(
                Issues,
                EDGValidationSeverity::Warning,
                TEXT("PUBLISHED_DISTANCE_MISMATCH"),
                Hole.HoleNumber,
                TEXT("Published distance differs materially from tee-to-basket planar distance.")
            );
        }

        TSet<FName> ZoneIds;
        for (const FDGCourseZoneDefinition& Zone : Hole.Zones)
        {
            if (Zone.ZoneId.IsNone())
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("ZONE_ID_MISSING"),
                    Hole.HoleNumber,
                    TEXT("Every course zone requires a ZoneId.")
                );
            }
            else if (ZoneIds.Contains(Zone.ZoneId))
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("ZONE_ID_DUPLICATE"),
                    Hole.HoleNumber,
                    FString::Printf(TEXT("Duplicate ZoneId: %s"), *Zone.ZoneId.ToString())
                );
            }
            ZoneIds.Add(Zone.ZoneId);

            if (Zone.PolygonPointsCm.Num() < 3)
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("ZONE_POLYGON_INVALID"),
                    Hole.HoleNumber,
                    TEXT("Zone polygon requires at least three points.")
                );
            }
        }

        TSet<FName> MandoIds;
        for (const FDGMandoDefinition& Mando : Hole.Mandos)
        {
            if (Mando.MandoId.IsNone())
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("MANDO_ID_MISSING"),
                    Hole.HoleNumber,
                    TEXT("Every mando requires a MandoId.")
                );
            }
            else if (MandoIds.Contains(Mando.MandoId))
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("MANDO_ID_DUPLICATE"),
                    Hole.HoleNumber,
                    FString::Printf(TEXT("Duplicate MandoId: %s"), *Mando.MandoId.ToString())
                );
            }
            MandoIds.Add(Mando.MandoId);

            if (FVector::DistSquared(Mando.GatePointACm, Mando.GatePointBCm) < FMath::Square(50.0f))
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("MANDO_GATE_TOO_SMALL"),
                    Hole.HoleNumber,
                    TEXT("Mando gate points are too close.")
                );
            }

            if (Mando.DropZoneIndex != INDEX_NONE &&
                !Hole.DropZoneTransforms.IsValidIndex(Mando.DropZoneIndex))
            {
                DGAddCourseIssue(
                    Issues,
                    EDGValidationSeverity::Error,
                    TEXT("MANDO_DROPZONE_INVALID"),
                    Hole.HoleNumber,
                    TEXT("Mando references an invalid drop-zone index.")
                );
            }
        }
    }

    return Issues;
}

bool UDiscGolfCourseValidatorLibrary::HasErrors(
    const TArray<FDGCourseValidationIssue>& Issues)
{
    for (const FDGCourseValidationIssue& Issue : Issues)
    {
        if (Issue.Severity == EDGValidationSeverity::Error)
        {
            return true;
        }
    }

    return false;
}
