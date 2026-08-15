#include "DiscGolfRouteTelemetry.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    float DistanceToSegment2D(const FVector& Point, const FVector& A, const FVector& B)
    {
        const FVector2D Point2(Point.X, Point.Y);
        const FVector2D A2(A.X, A.Y);
        const FVector2D B2(B.X, B.Y);
        const FVector2D Delta = B2 - A2;
        const float LengthSquared = Delta.SizeSquared();
        if (LengthSquared <= KINDA_SMALL_NUMBER) return FVector2D::Distance(Point2, A2);
        const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point2 - A2, Delta) / LengthSquared, 0.0f, 1.0f);
        return FVector2D::Distance(Point2, A2 + Delta * Alpha);
    }

    float DistanceToRoute2D(const FVector& Point, const TArray<FVector>& Waypoints)
    {
        if (Waypoints.IsEmpty()) return TNumericLimits<float>::Max();
        if (Waypoints.Num() == 1) return FVector::Dist2D(Point, Waypoints[0]);
        float Closest = TNumericLimits<float>::Max();
        for (int32 Index = 1; Index < Waypoints.Num(); ++Index)
        {
            Closest = FMath::Min(Closest, DistanceToSegment2D(Point, Waypoints[Index - 1], Waypoints[Index]));
        }
        return Closest;
    }

    FString ClassifyLandingMiss(const FDiscGolfLandingZoneDefinition& Zone, const FVector& Location)
    {
        const FVector Local = Zone.Rotation.UnrotateVector(Location - Zone.LocationCm);
        const float XExcess = FMath::Abs(Local.X) - Zone.ExtentCm.X;
        const float YExcess = FMath::Abs(Local.Y) - Zone.ExtentCm.Y;
        if (XExcess <= 0.0f && YExcess <= 0.0f) return TEXT("Inside");
        const float XNormalized = XExcess / FMath::Max(Zone.ExtentCm.X, 1.0f);
        const float YNormalized = YExcess / FMath::Max(Zone.ExtentCm.Y, 1.0f);
        if (XNormalized >= YNormalized) return Local.X < 0.0f ? TEXT("Short") : TEXT("Long");
        return Local.Y < 0.0f ? TEXT("Left") : TEXT("Right");
    }

    TSharedRef<FJsonObject> VectorJson(const FVector& Value)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("x"), Value.X);
        Json->SetNumberField(TEXT("y"), Value.Y);
        Json->SetNumberField(TEXT("z"), Value.Z);
        return Json;
    }

    FVector JsonVector(const TSharedPtr<FJsonObject>& Json)
    {
        return Json.IsValid()
            ? FVector(Json->GetNumberField(TEXT("x")), Json->GetNumberField(TEXT("y")), Json->GetNumberField(TEXT("z")))
            : FVector::ZeroVector;
    }

    FString PenaltyName(EDiscGolfPenaltyType Type)
    {
        switch (Type)
        {
            case EDiscGolfPenaltyType::OutOfBounds: return TEXT("OutOfBounds");
            case EDiscGolfPenaltyType::Hazard: return TEXT("Hazard");
            case EDiscGolfPenaltyType::None:
            default: return TEXT("None");
        }
    }

    EDiscGolfPenaltyType ParsePenalty(const FString& Value)
    {
        if (Value.Equals(TEXT("OutOfBounds"), ESearchCase::IgnoreCase)) return EDiscGolfPenaltyType::OutOfBounds;
        if (Value.Equals(TEXT("Hazard"), ESearchCase::IgnoreCase)) return EDiscGolfPenaltyType::Hazard;
        return EDiscGolfPenaltyType::None;
    }

    EDiscGolfFixtureType ParseFixture(const FString& Value)
    {
        if (Value.Equals(TEXT("Tree"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::Tree;
        if (Value.Equals(TEXT("DenseGrass"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::DenseGrass;
        if (Value.Equals(TEXT("Rock"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::Rock;
        if (Value.Equals(TEXT("Sign"), ESearchCase::IgnoreCase)) return EDiscGolfFixtureType::Sign;
        return EDiscGolfFixtureType::Unknown;
    }

    FString PlasticName(EDiscPlastic Plastic)
    {
        switch (Plastic)
        {
            case EDiscPlastic::Tour: return TEXT("Tour");
            case EDiscPlastic::Crystal: return TEXT("Crystal");
            case EDiscPlastic::Base:
            default: return TEXT("Base");
        }
    }

    EDiscPlastic ParsePlastic(const FString& Value)
    {
        if (Value.Equals(TEXT("Tour"), ESearchCase::IgnoreCase)) return EDiscPlastic::Tour;
        if (Value.Equals(TEXT("Crystal"), ESearchCase::IgnoreCase)) return EDiscPlastic::Crystal;
        return EDiscPlastic::Base;
    }

    FString ThrowStyleName(EThrowStyle Style)
    {
        return Style == EThrowStyle::Forehand ? TEXT("Forehand") : TEXT("Backhand");
    }

    EThrowStyle ParseThrowStyle(const FString& Value)
    {
        return Value.Equals(TEXT("Forehand"), ESearchCase::IgnoreCase)
            ? EThrowStyle::Forehand : EThrowStyle::Backhand;
    }
}

FString DiscGolfRouteTelemetry::RouteTypeName(EDiscGolfShotRouteType Type)
{
    switch (Type)
    {
        case EDiscGolfShotRouteType::RiskReward: return TEXT("RiskReward");
        case EDiscGolfShotRouteType::Bailout: return TEXT("Bailout");
        case EDiscGolfShotRouteType::Primary:
        default: return TEXT("Primary");
    }
}

FString DiscGolfRouteTelemetry::FixtureTypeName(EDiscGolfFixtureType Type)
{
    switch (Type)
    {
        case EDiscGolfFixtureType::Tree: return TEXT("Tree");
        case EDiscGolfFixtureType::DenseGrass: return TEXT("DenseGrass");
        case EDiscGolfFixtureType::Rock: return TEXT("Rock");
        case EDiscGolfFixtureType::Sign: return TEXT("Sign");
        case EDiscGolfFixtureType::Unknown:
        default: return TEXT("Unknown");
    }
}

FDiscGolfRouteTelemetryEvaluation DiscGolfRouteTelemetry::EvaluateRouteShot(
    const FDiscGolfShotRouteDefinition& Route,
    const FDiscGolfLandingZoneDefinition& LandingZone,
    const TArray<FDiscTrajectorySample>& Samples,
    const FVector& RawFinalLocationCm)
{
    FDiscGolfRouteTelemetryEvaluation Result;
    Result.MissSide = ClassifyLandingMiss(LandingZone, RawFinalLocationCm);
    Result.bLandingZoneHit = Result.MissSide == TEXT("Inside");
    Result.CorridorSampleCount = Samples.Num();
    const float HalfWidth = FMath::Max(Route.CorridorWidthCm * 0.5f, 1.0f);
    for (const FDiscTrajectorySample& Sample : Samples)
    {
        const float Distance = DistanceToRoute2D(Sample.WorldLocationCm, Route.WaypointsCm);
        if (Distance <= HalfWidth + KINDA_SMALL_NUMBER) ++Result.CorridorSamplesInside;
        Result.MaximumCorridorDeviationMeters = FMath::Max(
            Result.MaximumCorridorDeviationMeters, FMath::Max(0.0f, Distance - HalfWidth) / 100.0f);
    }
    Result.CorridorAdherencePercent = Result.CorridorSampleCount > 0
        ? 100.0f * static_cast<float>(Result.CorridorSamplesInside) / static_cast<float>(Result.CorridorSampleCount)
        : 0.0f;
    return Result;
}

int32 DiscGolfRouteTelemetry::CountAttempts(const FDiscGolfRouteTelemetrySession& Session, FName RouteId)
{
    int32 Count = 0;
    for (const FDiscGolfRouteTelemetryAttempt& Attempt : Session.Attempts)
    {
        Count += Attempt.RouteId == RouteId ? 1 : 0;
    }
    return Count;
}

bool DiscGolfRouteTelemetry::IsComplete(const FDiscGolfRouteTelemetrySession& Session)
{
    if (Session.Routes.Num() != 3 || Session.TargetAttemptsPerRoute <= 0) return false;
    return Session.Routes.ContainsByPredicate([&Session](const FDiscGolfShotRouteDefinition& Route)
    {
        return CountAttempts(Session, Route.RouteId) < Session.TargetAttemptsPerRoute;
    }) == false;
}

bool DiscGolfRouteTelemetry::IsCompatible(
    const FDiscGolfRouteTelemetrySession& Session,
    FName CourseId,
    FName LayoutId,
    FName CollisionProfileId,
    int32 HoleNumber)
{
    return Session.SchemaVersion == 1
        && Session.CourseId == CourseId
        && Session.LayoutId == LayoutId
        && Session.CollisionProfileId == CollisionProfileId
        && Session.HoleNumber == HoleNumber;
}

bool DiscGolfRouteTelemetry::SerializeSession(
    const FDiscGolfRouteTelemetrySession& Session,
    FString& OutJson,
    FString& OutError)
{
    if (Session.SchemaVersion != 1 || Session.SessionId.IsEmpty() || Session.Routes.Num() != 3)
    {
        OutError = TEXT("route telemetry session identity or route coverage is invalid");
        return false;
    }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_route_telemetry"));
    Root->SetNumberField(TEXT("schema_version"), Session.SchemaVersion);
    Root->SetStringField(TEXT("session_id"), Session.SessionId);
    Root->SetStringField(TEXT("started_utc"), Session.StartedUtc);
    Root->SetStringField(TEXT("updated_utc"), Session.UpdatedUtc);
    Root->SetStringField(TEXT("course_id"), Session.CourseId.ToString());
    Root->SetStringField(TEXT("layout_id"), Session.LayoutId.ToString());
    Root->SetStringField(TEXT("collision_profile_id"), Session.CollisionProfileId.ToString());
    Root->SetNumberField(TEXT("hole_number"), Session.HoleNumber);
    Root->SetNumberField(TEXT("target_attempts_per_route"), Session.TargetAttemptsPerRoute);
    Root->SetStringField(TEXT("active_route_id"), Session.ActiveRouteId.ToString());
    Root->SetBoolField(TEXT("complete"), IsComplete(Session));

    TArray<TSharedPtr<FJsonValue>> RouteValues;
    for (const FDiscGolfShotRouteDefinition& Route : Session.Routes)
    {
        TSharedRef<FJsonObject> RouteJson = MakeShared<FJsonObject>();
        RouteJson->SetStringField(TEXT("route_id"), Route.RouteId.ToString());
        RouteJson->SetStringField(TEXT("label"), Route.Label.ToString());
        RouteJson->SetStringField(TEXT("route_type"), RouteTypeName(Route.RouteType));
        RouteJson->SetStringField(TEXT("landing_zone_id"), Route.LandingZoneId.ToString());
        RouteJson->SetNumberField(TEXT("target_strokes"), Route.TargetStrokes);
        RouteJson->SetNumberField(TEXT("risk_rating"), Route.RiskRating);
        RouteJson->SetNumberField(TEXT("reward_rating"), Route.RewardRating);
        RouteJson->SetNumberField(TEXT("corridor_width_meters"), Route.CorridorWidthCm / 100.0f);

        const TArray<FDiscGolfRouteTelemetryAttempt> RouteAttempts = Session.Attempts.FilterByPredicate(
            [&Route](const FDiscGolfRouteTelemetryAttempt& Attempt) { return Attempt.RouteId == Route.RouteId; });
        int32 LandingHits = 0;
        int32 PenaltyAttempts = 0;
        int32 VisibleAttempts = 0;
        int32 UnderstoodRated = 0;
        int32 UnderstoodYes = 0;
        int32 ClearRated = 0;
        int32 ClearYes = 0;
        int32 FinalScores = 0;
        float AdherenceTotal = 0.0f;
        float RemainingTotal = 0.0f;
        float ScoreTotal = 0.0f;
        for (const FDiscGolfRouteTelemetryAttempt& Attempt : RouteAttempts)
        {
            LandingHits += Attempt.bLandingZoneHit ? 1 : 0;
            PenaltyAttempts += Attempt.PenaltyStrokes > 0 ? 1 : 0;
            VisibleAttempts += Attempt.bBasketVisible ? 1 : 0;
            AdherenceTotal += Attempt.CorridorAdherencePercent;
            RemainingTotal += Attempt.RemainingDistanceMeters;
            if (Attempt.TradeoffUnderstood >= 0)
            {
                ++UnderstoodRated;
                UnderstoodYes += Attempt.TradeoffUnderstood > 0 ? 1 : 0;
            }
            if (Attempt.NextShotClear >= 0)
            {
                ++ClearRated;
                ClearYes += Attempt.NextShotClear > 0 ? 1 : 0;
            }
            if (Attempt.FinalHoleScore >= 0)
            {
                ++FinalScores;
                ScoreTotal += Attempt.FinalHoleScore;
            }
        }
        const float Count = static_cast<float>(RouteAttempts.Num());
        TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
        Summary->SetNumberField(TEXT("attempts"), RouteAttempts.Num());
        Summary->SetNumberField(TEXT("target_attempts"), Session.TargetAttemptsPerRoute);
        Summary->SetNumberField(TEXT("landing_zone_hits"), LandingHits);
        Summary->SetNumberField(TEXT("landing_zone_hit_rate_percent"), Count > 0.0f ? LandingHits * 100.0f / Count : 0.0f);
        Summary->SetNumberField(TEXT("penalty_attempts"), PenaltyAttempts);
        Summary->SetNumberField(TEXT("penalty_rate_percent"), Count > 0.0f ? PenaltyAttempts * 100.0f / Count : 0.0f);
        Summary->SetNumberField(TEXT("mean_corridor_adherence_percent"), Count > 0.0f ? AdherenceTotal / Count : 0.0f);
        Summary->SetNumberField(TEXT("mean_remaining_distance_meters"), Count > 0.0f ? RemainingTotal / Count : 0.0f);
        Summary->SetNumberField(TEXT("basket_visible_rate_percent"), Count > 0.0f ? VisibleAttempts * 100.0f / Count : 0.0f);
        Summary->SetNumberField(TEXT("tradeoff_understood_rated"), UnderstoodRated);
        Summary->SetNumberField(TEXT("tradeoff_understood_yes"), UnderstoodYes);
        Summary->SetNumberField(TEXT("next_shot_clear_rated"), ClearRated);
        Summary->SetNumberField(TEXT("next_shot_clear_yes"), ClearYes);
        Summary->SetNumberField(TEXT("final_scores_recorded"), FinalScores);
        Summary->SetNumberField(TEXT("mean_final_score"), FinalScores > 0 ? ScoreTotal / static_cast<float>(FinalScores) : 0.0f);
        RouteJson->SetObjectField(TEXT("summary"), Summary);
        RouteValues.Add(MakeShared<FJsonValueObject>(RouteJson));
    }
    Root->SetArrayField(TEXT("routes"), RouteValues);

    TArray<TSharedPtr<FJsonValue>> AttemptValues;
    for (const FDiscGolfRouteTelemetryAttempt& Attempt : Session.Attempts)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetNumberField(TEXT("attempt_number"), Attempt.AttemptNumber);
        Json->SetStringField(TEXT("route_id"), Attempt.RouteId.ToString());
        Json->SetStringField(TEXT("recorded_utc"), Attempt.RecordedUtc);
        Json->SetObjectField(TEXT("release_location_cm"), VectorJson(Attempt.ReleaseLocationCm));
        Json->SetObjectField(TEXT("raw_final_location_cm"), VectorJson(Attempt.RawFinalLocationCm));
        Json->SetObjectField(TEXT("lie_location_cm"), VectorJson(Attempt.LieLocationCm));
        Json->SetBoolField(TEXT("landing_zone_hit"), Attempt.bLandingZoneHit);
        Json->SetStringField(TEXT("miss_side"), Attempt.MissSide);
        Json->SetNumberField(TEXT("corridor_sample_count"), Attempt.CorridorSampleCount);
        Json->SetNumberField(TEXT("corridor_samples_inside"), Attempt.CorridorSamplesInside);
        Json->SetNumberField(TEXT("corridor_adherence_percent"), Attempt.CorridorAdherencePercent);
        Json->SetNumberField(TEXT("maximum_corridor_deviation_meters"), Attempt.MaximumCorridorDeviationMeters);
        Json->SetStringField(TEXT("penalty_type"), PenaltyName(Attempt.PenaltyType));
        Json->SetNumberField(TEXT("penalty_strokes"), Attempt.PenaltyStrokes);
        Json->SetNumberField(TEXT("remaining_distance_meters"), Attempt.RemainingDistanceMeters);
        Json->SetBoolField(TEXT("basket_visible"), Attempt.bBasketVisible);
        Json->SetNumberField(TEXT("tradeoff_understood"), Attempt.TradeoffUnderstood);
        Json->SetNumberField(TEXT("next_shot_clear"), Attempt.NextShotClear);
        Json->SetNumberField(TEXT("final_hole_score"), Attempt.FinalHoleScore);
        Json->SetBoolField(TEXT("holed_out_on_route_shot"), Attempt.bHoledOutOnRouteShot);
        Json->SetNumberField(TEXT("fixture_contact_count"), Attempt.FixtureContactCount);
        Json->SetStringField(TEXT("last_fixture_type"), FixtureTypeName(Attempt.LastFixtureType));
        Json->SetNumberField(TEXT("flight_time_seconds"), Attempt.FlightTimeSeconds);
        Json->SetNumberField(TEXT("final_carry_meters"), Attempt.FinalCarryMeters);
        Json->SetStringField(TEXT("mold_id"), Attempt.MoldId.ToString());
        Json->SetStringField(TEXT("plastic"), PlasticName(Attempt.Plastic));
        Json->SetStringField(TEXT("throw_style"), ThrowStyleName(Attempt.ThrowStyle));
        Json->SetNumberField(TEXT("release_speed_mps"), Attempt.ReleaseSpeedMps);
        Json->SetNumberField(TEXT("release_spin_rpm"), Attempt.ReleaseSpinRpm);
        Json->SetNumberField(TEXT("release_hyzer_deg"), Attempt.ReleaseHyzerDeg);
        Json->SetNumberField(TEXT("release_nose_deg"), Attempt.ReleaseNoseDeg);
        Json->SetNumberField(TEXT("release_launch_deg"), Attempt.ReleaseLaunchDeg);
        Json->SetNumberField(TEXT("release_quality_01"), Attempt.ReleaseQuality01);
        AttemptValues.Add(MakeShared<FJsonValueObject>(Json));
    }
    Root->SetArrayField(TEXT("attempts"), AttemptValues);

    OutJson.Reset();
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("could not serialize route telemetry JSON");
        return false;
    }
    OutError.Reset();
    return true;
}

bool DiscGolfRouteTelemetry::DeserializeSession(
    const FString& Json,
    FDiscGolfRouteTelemetrySession& OutSession,
    FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
        || Root->GetStringField(TEXT("schema")) != TEXT("disc_golf_route_telemetry")
        || static_cast<int32>(Root->GetNumberField(TEXT("schema_version"))) != 1)
    {
        OutError = TEXT("route telemetry JSON identity/version is invalid");
        return false;
    }

    FDiscGolfRouteTelemetrySession Session;
    Session.SessionId = Root->GetStringField(TEXT("session_id"));
    Session.StartedUtc = Root->GetStringField(TEXT("started_utc"));
    Session.UpdatedUtc = Root->GetStringField(TEXT("updated_utc"));
    Session.CourseId = FName(*Root->GetStringField(TEXT("course_id")));
    Session.LayoutId = FName(*Root->GetStringField(TEXT("layout_id")));
    Session.CollisionProfileId = FName(*Root->GetStringField(TEXT("collision_profile_id")));
    Session.HoleNumber = static_cast<int32>(Root->GetNumberField(TEXT("hole_number")));
    Session.TargetAttemptsPerRoute = static_cast<int32>(Root->GetNumberField(TEXT("target_attempts_per_route")));
    Session.ActiveRouteId = FName(*Root->GetStringField(TEXT("active_route_id")));

    const TArray<TSharedPtr<FJsonValue>>* Attempts = nullptr;
    if (!Root->TryGetArrayField(TEXT("attempts"), Attempts) || !Attempts)
    {
        OutError = TEXT("route telemetry attempts array is missing");
        return false;
    }
    for (const TSharedPtr<FJsonValue>& Value : *Attempts)
    {
        const TSharedPtr<FJsonObject> A = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!A.IsValid())
        {
            OutError = TEXT("route telemetry attempt is not an object");
            return false;
        }
        FDiscGolfRouteTelemetryAttempt Attempt;
        Attempt.AttemptNumber = static_cast<int32>(A->GetNumberField(TEXT("attempt_number")));
        Attempt.RouteId = FName(*A->GetStringField(TEXT("route_id")));
        Attempt.RecordedUtc = A->GetStringField(TEXT("recorded_utc"));
        Attempt.ReleaseLocationCm = JsonVector(A->GetObjectField(TEXT("release_location_cm")));
        Attempt.RawFinalLocationCm = JsonVector(A->GetObjectField(TEXT("raw_final_location_cm")));
        Attempt.LieLocationCm = JsonVector(A->GetObjectField(TEXT("lie_location_cm")));
        Attempt.bLandingZoneHit = A->GetBoolField(TEXT("landing_zone_hit"));
        Attempt.MissSide = A->GetStringField(TEXT("miss_side"));
        Attempt.CorridorSampleCount = static_cast<int32>(A->GetNumberField(TEXT("corridor_sample_count")));
        Attempt.CorridorSamplesInside = static_cast<int32>(A->GetNumberField(TEXT("corridor_samples_inside")));
        Attempt.CorridorAdherencePercent = A->GetNumberField(TEXT("corridor_adherence_percent"));
        Attempt.MaximumCorridorDeviationMeters = A->GetNumberField(TEXT("maximum_corridor_deviation_meters"));
        Attempt.PenaltyType = ParsePenalty(A->GetStringField(TEXT("penalty_type")));
        Attempt.PenaltyStrokes = static_cast<int32>(A->GetNumberField(TEXT("penalty_strokes")));
        Attempt.RemainingDistanceMeters = A->GetNumberField(TEXT("remaining_distance_meters"));
        Attempt.bBasketVisible = A->GetBoolField(TEXT("basket_visible"));
        Attempt.TradeoffUnderstood = static_cast<int32>(A->GetNumberField(TEXT("tradeoff_understood")));
        Attempt.NextShotClear = static_cast<int32>(A->GetNumberField(TEXT("next_shot_clear")));
        Attempt.FinalHoleScore = static_cast<int32>(A->GetNumberField(TEXT("final_hole_score")));
        Attempt.bHoledOutOnRouteShot = A->GetBoolField(TEXT("holed_out_on_route_shot"));
        Attempt.FixtureContactCount = static_cast<int32>(A->GetNumberField(TEXT("fixture_contact_count")));
        Attempt.LastFixtureType = ParseFixture(A->GetStringField(TEXT("last_fixture_type")));
        Attempt.FlightTimeSeconds = A->GetNumberField(TEXT("flight_time_seconds"));
        Attempt.FinalCarryMeters = A->GetNumberField(TEXT("final_carry_meters"));
        Attempt.MoldId = FName(*A->GetStringField(TEXT("mold_id")));
        Attempt.Plastic = ParsePlastic(A->GetStringField(TEXT("plastic")));
        Attempt.ThrowStyle = ParseThrowStyle(A->GetStringField(TEXT("throw_style")));
        Attempt.ReleaseSpeedMps = A->GetNumberField(TEXT("release_speed_mps"));
        Attempt.ReleaseSpinRpm = A->GetNumberField(TEXT("release_spin_rpm"));
        Attempt.ReleaseHyzerDeg = A->GetNumberField(TEXT("release_hyzer_deg"));
        Attempt.ReleaseNoseDeg = A->GetNumberField(TEXT("release_nose_deg"));
        Attempt.ReleaseLaunchDeg = A->GetNumberField(TEXT("release_launch_deg"));
        Attempt.ReleaseQuality01 = A->GetNumberField(TEXT("release_quality_01"));
        Session.Attempts.Add(MoveTemp(Attempt));
    }
    if (Session.SessionId.IsEmpty() || Session.TargetAttemptsPerRoute <= 0)
    {
        OutError = TEXT("route telemetry session metadata is invalid");
        return false;
    }
    OutSession = MoveTemp(Session);
    OutError.Reset();
    return true;
}

bool DiscGolfRouteTelemetry::SaveSession(
    FDiscGolfRouteTelemetrySession& Session,
    const FString& Directory,
    FString& OutPath,
    FString& OutError)
{
    Session.UpdatedUtc = FDateTime::UtcNow().ToIso8601();
    FString Json;
    if (!SerializeSession(Session, Json, OutError)) return false;
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString SessionPath = FPaths::Combine(Directory,
        FString::Printf(TEXT("RouteTelemetry_%s.json"), *Session.SessionId));
    const FString LatestPath = FPaths::Combine(Directory, TEXT("LatestRouteTelemetry.json"));
    if (!FFileHelper::SaveStringToFile(Json, *SessionPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Json, *LatestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("could not write route telemetry under %s"), *Directory);
        return false;
    }
    OutPath = SessionPath;
    OutError.Reset();
    return true;
}

bool DiscGolfRouteTelemetry::LoadSession(
    const FString& Path,
    FDiscGolfRouteTelemetrySession& OutSession,
    FString& OutError)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *Path))
    {
        OutError = FString::Printf(TEXT("could not read %s"), *Path);
        return false;
    }
    return DeserializeSession(Json, OutSession, OutError);
}
