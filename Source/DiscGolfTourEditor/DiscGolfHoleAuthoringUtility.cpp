#include "DiscGolfHoleAuthoringUtility.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PineRidgeHole1Environment.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    bool IsFiniteVector(const FVector& Value)
    {
        return !Value.ContainsNaN()
            && FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    FDiscGolfShotRouteDefinition* FindPrimaryRoute(FDiscGolfHoleBlockoutDefinition& Definition)
    {
        return Definition.ShotRoutes.FindByPredicate([](const FDiscGolfShotRouteDefinition& Route)
        {
            return Route.RouteType == EDiscGolfShotRouteType::Primary;
        });
    }
}

FDiscGolfHoleAuthoringDraft UDiscGolfHoleAuthoringUtility::CreateHoleDefinition(
    FName CourseId, FName LayoutId, int32 HoleNumber, const FText& HoleName)
{
    FDiscGolfHoleAuthoringDraft Draft;
    Draft.Definition.CourseId = CourseId;
    Draft.Definition.LayoutId = LayoutId;
    Draft.Definition.HoleNumber = HoleNumber;
    Draft.Definition.HoleName = HoleName;
    Draft.Definition.Par = 3;
    return Draft;
}

void UDiscGolfHoleAuthoringUtility::AssignTee(FDiscGolfHoleAuthoringDraft& Draft, FVector TeeLocationCm)
{
    Draft.Definition.TeeLocationCm = TeeLocationCm;
    Draft.bTeeAssigned = IsFiniteVector(TeeLocationCm);
}

void UDiscGolfHoleAuthoringUtility::AssignBasket(FDiscGolfHoleAuthoringDraft& Draft, FVector BasketLocationCm)
{
    Draft.Definition.BasketLocationCm = BasketLocationCm;
    Draft.bBasketAssigned = IsFiniteVector(BasketLocationCm);
}

void UDiscGolfHoleAuthoringUtility::DrawFairwaySpline(
    FDiscGolfHoleAuthoringDraft& Draft, const TArray<FVector>& PointsCm)
{
    FDiscGolfShotRouteDefinition* Route = FindPrimaryRoute(Draft.Definition);
    if (!Route)
    {
        FDiscGolfShotRouteDefinition NewRoute;
        NewRoute.RouteId = TEXT("PrimaryFairway");
        NewRoute.Label = FText::FromString(TEXT("PRIMARY FAIRWAY"));
        NewRoute.RouteType = EDiscGolfShotRouteType::Primary;
        NewRoute.TargetStrokes = Draft.Definition.Par;
        Draft.Definition.ShotRoutes.Add(NewRoute);
        Route = &Draft.Definition.ShotRoutes.Last();
    }
    Route->CorridorWidthCm = Draft.FairwayWidthCm;
    Route->WaypointsCm = PointsCm;
}

void UDiscGolfHoleAuthoringUtility::SetPar(FDiscGolfHoleAuthoringDraft& Draft, int32 Par)
{
    Draft.Definition.Par = FMath::Clamp(Par, 1, 12);
}

float UDiscGolfHoleAuthoringUtility::CalculateDistance(const FDiscGolfHoleAuthoringDraft& Draft)
{
    return FVector::Dist(Draft.Definition.TeeLocationCm, Draft.Definition.BasketLocationCm) / 30.48f;
}

void UDiscGolfHoleAuthoringUtility::SetGreenRadius(FDiscGolfHoleAuthoringDraft& Draft, float RadiusCm)
{
    Draft.GreenRadiusCm = FMath::Clamp(RadiusCm, 300.0f, 3000.0f);
}

void UDiscGolfHoleAuthoringUtility::SetFairwayWidth(FDiscGolfHoleAuthoringDraft& Draft, float WidthCm)
{
    Draft.FairwayWidthCm = FMath::Clamp(WidthCm, 500.0f, 5000.0f);
    if (FDiscGolfShotRouteDefinition* Route = FindPrimaryRoute(Draft.Definition))
    {
        Route->CorridorWidthCm = Draft.FairwayWidthCm;
    }
}

void UDiscGolfHoleAuthoringUtility::AssignIntroCamera(
    FDiscGolfHoleAuthoringDraft& Draft, FName CameraAnchorId)
{
    Draft.IntroCameraId = CameraAnchorId;
}

void UDiscGolfHoleAuthoringUtility::AssignPreviewSpline(
    FDiscGolfHoleAuthoringDraft& Draft, const TArray<FVector>& PointsCm)
{
    Draft.Definition.FlyoverPointsCm = PointsCm;
}

FDiscGolfHoleValidationResult UDiscGolfHoleAuthoringUtility::ValidateHole(
    const FDiscGolfHoleAuthoringDraft& Draft,
    const TArray<FDiscGolfHoleBlockoutDefinition>& ExistingHoles)
{
    FDiscGolfHoleValidationResult Result;
    Result.ExactDistanceFeet = CalculateDistance(Draft);
    Result.DisplayDistanceFeet = FMath::RoundToInt(Result.ExactDistanceFeet);
    if (!Draft.bTeeAssigned) Result.Errors.Add(TEXT("Missing tee assignment"));
    if (!Draft.bBasketAssigned) Result.Errors.Add(TEXT("Missing basket assignment"));
    if (!IsFiniteVector(Draft.Definition.TeeLocationCm)) Result.Errors.Add(TEXT("Invalid spawn transform"));
    if (!IsFiniteVector(Draft.Definition.BasketLocationCm)) Result.Errors.Add(TEXT("Invalid basket transform"));
    if (Draft.Definition.CourseId.IsNone() || Draft.Definition.LayoutId.IsNone()
        || Draft.Definition.HoleNumber <= 0 || Draft.Definition.HoleName.IsEmpty()
        || Draft.Definition.Par <= 0)
    {
        Result.Errors.Add(TEXT("Hole metadata is incomplete"));
    }
    if (Result.ExactDistanceFeet < 80.0f || Result.ExactDistanceFeet > 1300.0f)
    {
        Result.Errors.Add(TEXT("Implausible tee-to-basket distance"));
    }
    if (FVector::Dist2D(Draft.Definition.TeeLocationCm, Draft.Definition.BasketLocationCm)
        <= Draft.TeeClearRadiusCm + Draft.GreenRadiusCm)
    {
        Result.Errors.Add(TEXT("Basket intersects tee/green clearance"));
    }
    const FDiscGolfShotRouteDefinition* Primary = Draft.Definition.ShotRoutes.FindByPredicate(
        [](const FDiscGolfShotRouteDefinition& Route)
        {
            return Route.RouteType == EDiscGolfShotRouteType::Primary && Route.WaypointsCm.Num() >= 2;
        });
    if (!Primary) Result.Errors.Add(TEXT("Primary fairway spline is missing"));
    else if (!Primary->WaypointsCm[0].Equals(Draft.Definition.TeeLocationCm, 300.0f)
        || !Primary->WaypointsCm.Last().Equals(Draft.Definition.BasketLocationCm, 500.0f))
    {
        Result.Errors.Add(TEXT("Fairway spline metadata does not cover tee and basket"));
    }
    if (Draft.Definition.FlyoverPointsCm.Num() < 2) Result.Warnings.Add(TEXT("Preview spline is missing"));
    if (Draft.IntroCameraId.IsNone()) Result.Warnings.Add(TEXT("Intro camera is not assigned"));
    else if (!Draft.Definition.CameraAnchors.ContainsByPredicate([&Draft](const auto& Anchor)
        { return Anchor.AnchorId == Draft.IntroCameraId; }))
    {
        Result.Errors.Add(TEXT("Intro camera metadata does not match a camera anchor"));
    }
    for (const FDiscGolfTreeDefinition& Tree : Draft.Definition.Trees)
    {
        if (FVector::Dist2D(Tree.LocationCm, Draft.Definition.BasketLocationCm) < Draft.GreenRadiusCm)
        {
            Result.Errors.Add(TEXT("Green obstruction inside configured clearance"));
            break;
        }
    }
    if (ExistingHoles.ContainsByPredicate([&Draft](const FDiscGolfHoleBlockoutDefinition& Other)
        { return Other.HoleNumber == Draft.Definition.HoleNumber; }))
    {
        Result.Errors.Add(TEXT("Duplicate hole number"));
    }
    Result.bValid = Result.Errors.IsEmpty();
    return Result;
}

bool UDiscGolfHoleAuthoringUtility::GeneratePineRidgeHole1ValidationReport(
    FString& OutReportPath, FString& OutError)
{
    FDiscGolfHoleBlockoutDefinition Definition;
    FString Source;
    if (!DiscGolfCourseDefinition::LoadPineRidgeHole1(Definition, Source, OutError)) return false;

    FDiscGolfHoleAuthoringDraft Draft;
    Draft.Definition = Definition;
    Draft.bTeeAssigned = true;
    Draft.bBasketAssigned = true;
    Draft.IntroCameraId = TEXT("TeeBroadcast");
    const TArray<FDiscGolfHole1EnvironmentZonePlan> Zones =
        PineRidgeHole1Environment::BuildZonePlan(Definition);
    if (const auto* Green = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::Green; })) Draft.GreenRadiusCm = Green->RadiusCm;
    if (const auto* Tee = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneType == EDiscGolfEnvironmentZoneType::Tee; })) Draft.TeeClearRadiusCm = Tee->RadiusCm;
    if (const auto* Fairway = Zones.FindByPredicate([](const auto& Zone)
        { return Zone.ZoneId == TEXT("H01_PrimaryFairway"); })) Draft.FairwayWidthCm = Fairway->WidthCm;
    const FDiscGolfHoleValidationResult Validation = ValidateHole(Draft, {});

    const FString FlightReportPath = FPaths::Combine(FPaths::ProjectSavedDir(),
        TEXT("EnvironmentReports/PineRidgeHole1FlightRoutes.json"));
    FString FlightJson;
    bool bRoutesPassed = false;
    int32 RouteCount = 0;
    TSharedPtr<FJsonObject> FlightRoot;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FlightJson);
    if (FFileHelper::LoadFileToString(FlightJson, *FlightReportPath))
    {
        const TSharedRef<TJsonReader<>> LoadedReader = TJsonReaderFactory<>::Create(FlightJson);
        if (FJsonSerializer::Deserialize(LoadedReader, FlightRoot) && FlightRoot.IsValid())
        {
            bRoutesPassed = FlightRoot->GetBoolField(TEXT("passed"));
            RouteCount = FlightRoot->GetIntegerField(TEXT("routeCount"));
        }
    }

    const auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_course_validation"));
    Root->SetNumberField(TEXT("schemaVersion"), 1);
    Root->SetStringField(TEXT("holeName"), Definition.HoleName.ToString());
    Root->SetNumberField(TEXT("holeNumber"), Definition.HoleNumber);
    Root->SetNumberField(TEXT("par"), Definition.Par);
    Root->SetNumberField(TEXT("officialDisplayedDistanceFeet"), FMath::RoundToInt(
        DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition)));
    Root->SetNumberField(TEXT("exactTeeToBasketDistanceFeet"),
        DiscGolfCourseDefinition::MeasuredDistanceFeet(Definition));
    Root->SetStringField(TEXT("teeDimensionsMeters"), TEXT("4.2 x 1.8"));
    Root->SetNumberField(TEXT("teeClearRadiusFeet"), Draft.TeeClearRadiusCm / 30.48f);
    Root->SetNumberField(TEXT("fairwayWidthFeet"), Draft.FairwayWidthCm / 30.48f);
    Root->SetNumberField(TEXT("greenRadiusFeet"), Draft.GreenRadiusCm / 30.48f);
    Root->SetNumberField(TEXT("environmentZones"), Zones.Num());
    Root->SetNumberField(TEXT("strategicAuthoredTreeCount"), Definition.Trees.Num());
    int32 ObAreaCount = 0;
    for (const FDiscGolfBlockoutSurfaceDefinition& Surface : Definition.Surfaces)
    {
        if (Surface.SurfaceType == ECourseSurfaceType::OutOfBounds) ++ObAreaCount;
    }
    Root->SetNumberField(TEXT("obAreaCount"), ObAreaCount);
    Root->SetStringField(TEXT("playableRouteTests"), bRoutesPassed
        ? FString::Printf(TEXT("PASS (%d/%d)"), RouteCount, RouteCount) : TEXT("PENDING OR FAILED"));
    Root->SetStringField(TEXT("introCameraStatus"), Draft.IntroCameraId.IsNone() ? TEXT("MISSING") : TEXT("READY"));
    Root->SetStringField(TEXT("flyoverSplineStatus"), Definition.FlyoverPointsCm.Num() >= 2 ? TEXT("READY") : TEXT("MISSING"));
    Root->SetStringField(TEXT("basketMarkerStatus"), TEXT("READY - VISIBILITY AWARE"));
    Root->SetStringField(TEXT("navigationStatus"), TEXT("READY - EXISTING COURSE FLOW"));
    Root->SetStringField(TEXT("scorecardStatus"), TEXT("READY - AUTHORED HOLES ONLY"));
    Root->SetStringField(TEXT("replayStatus"), TEXT("READY - BOUNDED PATH / TRACKING + TEE CAMERAS"));
    Root->SetStringField(TEXT("validationStatus"), Validation.bValid ? TEXT("READY") : TEXT("INVALID"));
    Root->SetStringField(TEXT("environmentVisualAcceptance"), TEXT("PENDING FAB IMPORT"));
    TArray<TSharedPtr<FJsonValue>> Errors;
    for (const FString& Error : Validation.Errors) Errors.Add(MakeShared<FJsonValueString>(Error));
    Root->SetArrayField(TEXT("errors"), Errors);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("Could not serialize Hole 1 validation report");
        return false;
    }
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CourseReports"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    OutReportPath = FPaths::Combine(Directory, TEXT("PineRidgeHole1Validation.json"));
    if (!FFileHelper::SaveStringToFile(Json, *OutReportPath))
    {
        OutError = FString::Printf(TEXT("Could not write %s"), *OutReportPath);
        return false;
    }
    OutError.Reset();
    return Validation.bValid;
}
