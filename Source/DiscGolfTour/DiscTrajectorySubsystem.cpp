#include "DiscTrajectorySubsystem.h"

#include "DiscGolfTour.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 TrajectorySchemaVersion = 3;

FString PlasticName(EDiscPlastic Plastic)
{
    switch (Plastic)
    {
        case EDiscPlastic::Base: return TEXT("Base");
        case EDiscPlastic::Crystal: return TEXT("Crystal");
        case EDiscPlastic::Tour:
        default: return TEXT("Tour");
    }
}

FString ThrowStyleName(EThrowStyle Style)
{
    return Style == EThrowStyle::Forehand ? TEXT("Forehand") : TEXT("Backhand");
}

FString ShotContextName(EDiscShotContext Context)
{
    switch (Context)
    {
        case EDiscShotContext::Circle1Putt: return TEXT("Circle1Putt");
        case EDiscShotContext::Circle2Putt: return TEXT("Circle2Putt");
        case EDiscShotContext::Drive:
        default: return TEXT("Drive");
    }
}

EDiscShotContext ParseShotContext(const FString& Value)
{
    if (Value.Equals(TEXT("Circle1Putt"), ESearchCase::IgnoreCase)) return EDiscShotContext::Circle1Putt;
    if (Value.Equals(TEXT("Circle2Putt"), ESearchCase::IgnoreCase)) return EDiscShotContext::Circle2Putt;
    return EDiscShotContext::Drive;
}

FString BasketContactName(EBasketContactResult Result)
{
    switch (Result)
    {
        case EBasketContactResult::Caught: return TEXT("Caught");
        case EBasketContactResult::ChainDeflection: return TEXT("ChainDeflection");
        case EBasketContactResult::BandRejection: return TEXT("BandRejection");
        case EBasketContactResult::TrayRejection: return TEXT("TrayRejection");
        case EBasketContactResult::None:
        default: return TEXT("None");
    }
}

FString FixtureTypeName(EDiscGolfFixtureType Type)
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

EBasketContactResult ParseBasketContact(const FString& Value)
{
    if (Value.Equals(TEXT("Caught"), ESearchCase::IgnoreCase)) return EBasketContactResult::Caught;
    if (Value.Equals(TEXT("ChainDeflection"), ESearchCase::IgnoreCase)) return EBasketContactResult::ChainDeflection;
    if (Value.Equals(TEXT("BandRejection"), ESearchCase::IgnoreCase)) return EBasketContactResult::BandRejection;
    if (Value.Equals(TEXT("TrayRejection"), ESearchCase::IgnoreCase)) return EBasketContactResult::TrayRejection;
    return EBasketContactResult::None;
}

FString GroundStateName(EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Impact: return TEXT("Impact");
        case EDiscGroundState::Skipping: return TEXT("Skipping");
        case EDiscGroundState::Sliding: return TEXT("Sliding");
        case EDiscGroundState::EdgeRolling: return TEXT("EdgeRolling");
        case EDiscGroundState::Settled: return TEXT("Settled");
        case EDiscGroundState::Airborne:
        default: return TEXT("Airborne");
    }
}

FString GroundSurfaceName(EGroundSurfaceType Surface)
{
    switch (Surface)
    {
        case EGroundSurfaceType::Rough: return TEXT("Rough");
        case EGroundSurfaceType::Dirt: return TEXT("Dirt");
        case EGroundSurfaceType::Rock: return TEXT("Rock");
        case EGroundSurfaceType::TeePad: return TEXT("TeePad");
        case EGroundSurfaceType::Fairway:
        default: return TEXT("Fairway");
    }
}

FString CourseSurfaceName(ECourseSurfaceType Surface)
{
    switch (Surface)
    {
        case ECourseSurfaceType::TeePad: return TEXT("TeePad");
        case ECourseSurfaceType::LightRough: return TEXT("LightRough");
        case ECourseSurfaceType::DeepRough: return TEXT("DeepRough");
        case ECourseSurfaceType::Dirt: return TEXT("Dirt");
        case ECourseSurfaceType::Rock: return TEXT("Rock");
        case ECourseSurfaceType::OutOfBounds: return TEXT("OutOfBounds");
        case ECourseSurfaceType::Hazard: return TEXT("Hazard");
        case ECourseSurfaceType::Fairway:
        default: return TEXT("Fairway");
    }
}

FString LieName(ELieType Lie)
{
    switch (Lie)
    {
        case ELieType::Tee: return TEXT("Tee");
        case ELieType::LightRough: return TEXT("LightRough");
        case ELieType::DeepRough: return TEXT("DeepRough");
        case ELieType::Circle2: return TEXT("Circle2");
        case ELieType::Circle1: return TEXT("Circle1");
        case ELieType::Hazard: return TEXT("Hazard");
        case ELieType::Fairway:
        default: return TEXT("Fairway");
    }
}

FString PenaltyName(EDiscGolfPenaltyType Penalty)
{
    switch (Penalty)
    {
        case EDiscGolfPenaltyType::OutOfBounds: return TEXT("OutOfBounds");
        case EDiscGolfPenaltyType::Hazard: return TEXT("Hazard");
        case EDiscGolfPenaltyType::None:
        default: return TEXT("None");
    }
}

FString ReliefName(EDiscGolfReliefRule Relief)
{
    return Relief == EDiscGolfReliefRule::LastInBounds ? TEXT("LastInBounds") : TEXT("PlayFromResult");
}

EDiscGroundState ParseGroundState(const FString& Value)
{
    if (Value.Equals(TEXT("Impact"), ESearchCase::IgnoreCase)) return EDiscGroundState::Impact;
    if (Value.Equals(TEXT("Skipping"), ESearchCase::IgnoreCase)) return EDiscGroundState::Skipping;
    if (Value.Equals(TEXT("Sliding"), ESearchCase::IgnoreCase)) return EDiscGroundState::Sliding;
    if (Value.Equals(TEXT("EdgeRolling"), ESearchCase::IgnoreCase)) return EDiscGroundState::EdgeRolling;
    if (Value.Equals(TEXT("Settled"), ESearchCase::IgnoreCase)) return EDiscGroundState::Settled;
    return EDiscGroundState::Airborne;
}

TSharedRef<FJsonObject> VectorObject(const FVector& Vector)
{
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Vector.X);
    Object->SetNumberField(TEXT("y"), Vector.Y);
    Object->SetNumberField(TEXT("z"), Vector.Z);
    return Object;
}

FVector ReadVector(const TSharedPtr<FJsonObject>& Object)
{
    if (!Object.IsValid()) return FVector::ZeroVector;
    return FVector(
        Object->GetNumberField(TEXT("x")),
        Object->GetNumberField(TEXT("y")),
        Object->GetNumberField(TEXT("z")));
}

bool ReadFloatRange(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutMin, float& OutMax)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 2) return false;
    OutMin = static_cast<float>((*Values)[0]->AsNumber());
    OutMax = static_cast<float>((*Values)[1]->AsNumber());
    return FMath::IsFinite(OutMin) && FMath::IsFinite(OutMax) && OutMin <= OutMax;
}

bool ReadIntRange(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& OutMin, int32& OutMax)
{
    float Min = 0.0f;
    float Max = 0.0f;
    if (!ReadFloatRange(Object, Field, Min, Max)) return false;
    OutMin = FMath::RoundToInt(Min);
    OutMax = FMath::RoundToInt(Max);
    return OutMin <= OutMax;
}

FString CsvFloat(float Value)
{
    return FString::Printf(TEXT("%.6f"), Value);
}

void AddFailure(FDiscTrajectorySummary& Summary, const TCHAR* Label, float Value, float Minimum, float Maximum)
{
    if (Value < Minimum || Value > Maximum)
    {
        Summary.RegressionFailures.Add(FString::Printf(
            TEXT("%s %.3f outside [%.3f, %.3f]"), Label, Value, Minimum, Maximum));
    }
}

TSharedRef<FJsonObject> SummaryObject(const FDiscTrajectorySummary& Summary)
{
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("capture_id"), Summary.CaptureId);
    Object->SetStringField(TEXT("captured_utc"), Summary.CapturedUtc);
    Object->SetStringField(TEXT("preset_id"), Summary.PresetId.ToString());
    Object->SetNumberField(TEXT("render_fps"), Summary.RenderFps);
    Object->SetNumberField(TEXT("sample_count"), Summary.SampleCount);
    Object->SetNumberField(TEXT("ground_transition_count"), Summary.GroundTransitionCount);
    Object->SetNumberField(TEXT("duration_s"), Summary.DurationSeconds);
    Object->SetNumberField(TEXT("air_time_s"), Summary.AirTimeSeconds);
    Object->SetNumberField(TEXT("air_carry_m"), Summary.AirCarryMeters);
    Object->SetNumberField(TEXT("final_carry_m"), Summary.FinalCarryMeters);
    Object->SetNumberField(TEXT("apex_m"), Summary.ApexMeters);
    Object->SetNumberField(TEXT("lateral_m"), Summary.LateralMeters);
    Object->SetObjectField(TEXT("start_world_location_cm"), VectorObject(Summary.StartWorldLocationCm));
    Object->SetObjectField(TEXT("final_world_location_cm"), VectorObject(Summary.FinalWorldLocationCm));
    Object->SetStringField(TEXT("final_ground_state"), GroundStateName(Summary.FinalGroundState));
    Object->SetStringField(TEXT("final_ground_surface"), GroundSurfaceName(Summary.FinalGroundSurface));
    Object->SetNumberField(TEXT("ground_contact_count"), Summary.GroundContactCount);
    Object->SetNumberField(TEXT("ground_distance_m"), Summary.GroundDistanceMeters);
    Object->SetNumberField(TEXT("basket_contact_count"), Summary.BasketContactCount);
    Object->SetStringField(TEXT("last_basket_contact"), BasketContactName(Summary.LastBasketContact));
    Object->SetBoolField(TEXT("holed_out"), Summary.bHoledOut);
    Object->SetStringField(TEXT("surface_at_rest"), CourseSurfaceName(Summary.SurfaceAtRest));
    Object->SetStringField(TEXT("playing_surface"), CourseSurfaceName(Summary.PlayingSurface));
    Object->SetStringField(TEXT("resulting_lie_type"), LieName(Summary.ResultingLieType));
    Object->SetStringField(TEXT("penalty_type"), PenaltyName(Summary.PenaltyType));
    Object->SetStringField(TEXT("relief_rule"), ReliefName(Summary.ReliefRule));
    Object->SetNumberField(TEXT("penalty_strokes"), Summary.PenaltyStrokes);
    Object->SetObjectField(TEXT("resulting_lie_location_cm"), VectorObject(Summary.ResultingLieLocationCm));
    Object->SetBoolField(TEXT("was_regression"), Summary.bWasRegression);
    Object->SetBoolField(TEXT("regression_passed"), Summary.bRegressionPassed);
    TArray<TSharedPtr<FJsonValue>> Failures;
    for (const FString& Failure : Summary.RegressionFailures)
    {
        Failures.Add(MakeShared<FJsonValueString>(Failure));
    }
    Object->SetArrayField(TEXT("regression_failures"), Failures);
    return Object;
}
}

void UDiscTrajectorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ExportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TrajectoryExports"));
    IFileManager::Get().MakeDirectory(*ExportDirectory, true);

    FString Error;
    if (!LoadRegressionPresets(Error))
    {
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Physics regression presets could not be loaded: %s. Using built-in fallback."), *Error);
        BuildFallbackPresets();
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("Trajectory export ready at %s with %d regression preset(s)."),
        *ExportDirectory, RegressionPresets.Num());
}

bool UDiscTrajectorySubsystem::LoadRegressionPresets(FString& OutError)
{
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/PhysicsRegressionPresets.json"));
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        OutError = FString::Printf(TEXT("Cannot read %s"), *Path);
        return false;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Preset JSON is malformed");
        return false;
    }
    if (Root->GetIntegerField(TEXT("schema_version")) != 2)
    {
        OutError = TEXT("Unsupported preset schema version");
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Entries = nullptr;
    if (!Root->TryGetArrayField(TEXT("presets"), Entries) || !Entries || Entries->IsEmpty())
    {
        OutError = TEXT("Preset list is empty");
        return false;
    }

    TSet<FName> SeenIds;
    TArray<FPhysicsRegressionPreset> Parsed;
    for (const TSharedPtr<FJsonValue>& Entry : *Entries)
    {
        const TSharedPtr<FJsonObject> Object = Entry.IsValid() ? Entry->AsObject() : nullptr;
        if (!Object.IsValid())
        {
            OutError = TEXT("Preset entry is not an object");
            return false;
        }

        FPhysicsRegressionPreset Preset;
        Preset.PresetId = FName(Object->GetStringField(TEXT("id")));
        Preset.DisplayName = FText::FromString(Object->GetStringField(TEXT("name")));
        Preset.Description = Object->GetStringField(TEXT("description"));
        Preset.MoldId = FName(Object->GetStringField(TEXT("mold_id")));
        const FString Plastic = Object->GetStringField(TEXT("plastic"));
        Preset.Plastic = Plastic.Equals(TEXT("Base"), ESearchCase::IgnoreCase)
            ? EDiscPlastic::Base
            : Plastic.Equals(TEXT("Crystal"), ESearchCase::IgnoreCase)
                ? EDiscPlastic::Crystal
                : EDiscPlastic::Tour;
        Preset.ThrowStyle = Object->GetStringField(TEXT("throw_style")).Equals(TEXT("Forehand"), ESearchCase::IgnoreCase)
            ? EThrowStyle::Forehand
            : EThrowStyle::Backhand;
        Preset.ShotContext = ParseShotContext(Object->GetStringField(TEXT("shot_context")));
        Preset.StartDistanceMeters = static_cast<float>(Object->GetNumberField(TEXT("start_distance_m")));
        Preset.AimOffsetDeg = static_cast<float>(Object->GetNumberField(TEXT("aim_offset_deg")));
        Preset.Power01 = static_cast<float>(Object->GetNumberField(TEXT("power")));
        Preset.HyzerDeg = static_cast<float>(Object->GetNumberField(TEXT("hyzer_deg")));
        Preset.NoseAngleDeg = static_cast<float>(Object->GetNumberField(TEXT("nose_deg")));
        Preset.LaunchAngleDeg = static_cast<float>(Object->GetNumberField(TEXT("launch_deg")));
        Preset.TimingError = static_cast<float>(Object->GetNumberField(TEXT("timing_error")));
        Preset.WindMps = ReadVector(Object->GetObjectField(TEXT("wind_mps")));
        Preset.RenderFps = Object->GetIntegerField(TEXT("render_fps"));

        const TSharedPtr<FJsonObject> Expected = Object->GetObjectField(TEXT("expected"));
        FString FinalState;
        FString BasketContact;
        bool bExpectedHoledOut = false;
        if (Preset.PresetId.IsNone() || SeenIds.Contains(Preset.PresetId)
            || Preset.RenderFps < 15 || Preset.RenderFps > 240
            || !ReadFloatRange(Expected, TEXT("air_carry_m"), Preset.Expected.MinAirCarryMeters, Preset.Expected.MaxAirCarryMeters)
            || !ReadFloatRange(Expected, TEXT("final_carry_m"), Preset.Expected.MinFinalCarryMeters, Preset.Expected.MaxFinalCarryMeters)
            || !ReadFloatRange(Expected, TEXT("apex_m"), Preset.Expected.MinApexMeters, Preset.Expected.MaxApexMeters)
            || !ReadFloatRange(Expected, TEXT("air_time_s"), Preset.Expected.MinAirTimeSeconds, Preset.Expected.MaxAirTimeSeconds)
            || !ReadFloatRange(Expected, TEXT("lateral_m"), Preset.Expected.MinLateralMeters, Preset.Expected.MaxLateralMeters)
            || !ReadIntRange(Expected, TEXT("ground_contacts"), Preset.Expected.MinGroundContacts, Preset.Expected.MaxGroundContacts)
            || !Expected->TryGetStringField(TEXT("final_ground_state"), FinalState)
            || !Expected->TryGetBoolField(TEXT("holed_out"), bExpectedHoledOut)
            || !Expected->TryGetStringField(TEXT("basket_contact"), BasketContact)
            || !FMath::IsFinite(Preset.StartDistanceMeters) || Preset.StartDistanceMeters < 0.0f || Preset.StartDistanceMeters > 25.0f
            || !FMath::IsFinite(Preset.AimOffsetDeg) || FMath::Abs(Preset.AimOffsetDeg) > 45.0f)
        {
            OutError = FString::Printf(TEXT("Preset %s is incomplete, duplicated, or outside safe bounds"), *Preset.PresetId.ToString());
            return false;
        }
        Preset.Expected.ExpectedFinalGroundState = ParseGroundState(FinalState);
        Preset.Expected.ExpectedHoledOut = bExpectedHoledOut ? 1 : 0;
        Preset.Expected.bRequireBasketContact = true;
        Preset.Expected.ExpectedBasketContact = ParseBasketContact(BasketContact);
        SeenIds.Add(Preset.PresetId);
        Parsed.Add(Preset);
    }

    RegressionPresets = MoveTemp(Parsed);
    OutError.Reset();
    return true;
}

void UDiscTrajectorySubsystem::BuildFallbackPresets()
{
    RegressionPresets.Reset();
    for (const int32 Fps : {30, 60, 120})
    {
        FPhysicsRegressionPreset Preset;
        Preset.PresetId = FName(*FString::Printf(TEXT("ApexCalm%d"), Fps));
        Preset.DisplayName = FText::FromString(FString::Printf(TEXT("Apex Calm Baseline - %d FPS"), Fps));
        Preset.Description = TEXT("Built-in calm-air frame-rate regression fallback.");
        Preset.RenderFps = Fps;
        Preset.Expected.MinAirCarryMeters = 65.0f;
        Preset.Expected.MaxAirCarryMeters = 105.0f;
        Preset.Expected.MinFinalCarryMeters = 70.0f;
        Preset.Expected.MaxFinalCarryMeters = 135.0f;
        Preset.Expected.MinApexMeters = 3.0f;
        Preset.Expected.MaxApexMeters = 18.0f;
        Preset.Expected.MinAirTimeSeconds = 4.0f;
        Preset.Expected.MaxAirTimeSeconds = 12.0f;
        Preset.Expected.MinLateralMeters = -45.0f;
        Preset.Expected.MaxLateralMeters = 20.0f;
        Preset.Expected.MinGroundContacts = 1;
        Preset.Expected.MaxGroundContacts = 8;
        Preset.Expected.ExpectedHoledOut = 0;
        Preset.Expected.bRequireBasketContact = true;
        Preset.Expected.ExpectedBasketContact = EBasketContactResult::None;
        RegressionPresets.Add(Preset);
    }
}

FDiscTrajectorySummary UDiscTrajectorySubsystem::BuildSummary(
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& Telemetry,
    const FThrowRelease& Release,
    const FDiscGolfLieState& ResultingLie,
    bool bHoledOut,
    FName PresetId,
    int32 RenderFps)
{
    FDiscTrajectorySummary Summary;
    Summary.PresetId = PresetId;
    Summary.RenderFps = RenderFps;
    Summary.SampleCount = Samples.Num();
    Summary.GroundTransitionCount = Transitions.Num();
    Summary.DurationSeconds = Telemetry.FlightTimeSeconds;
    Summary.FinalGroundState = Telemetry.GroundState;
    Summary.FinalGroundSurface = Telemetry.GroundSurface;
    Summary.GroundContactCount = Telemetry.GroundContactCount;
    Summary.GroundDistanceMeters = Telemetry.GroundDistanceMeters;
    Summary.BasketContactCount = Telemetry.BasketContactCount;
    Summary.LastBasketContact = Telemetry.LastBasketContact;
    Summary.bHoledOut = bHoledOut;
    Summary.SurfaceAtRest = ResultingLie.SurfaceAtRest;
    Summary.PlayingSurface = ResultingLie.PlayingSurface;
    Summary.ResultingLieType = ResultingLie.LieType;
    Summary.PenaltyType = ResultingLie.PenaltyType;
    Summary.ReliefRule = ResultingLie.ReliefRule;
    Summary.PenaltyStrokes = ResultingLie.PenaltyStrokes;
    Summary.ResultingLieLocationCm = ResultingLie.LieLocationCm;
    Summary.bWasRegression = !PresetId.IsNone();

    if (Samples.IsEmpty()) return Summary;

    Summary.StartWorldLocationCm = Samples[0].WorldLocationCm;
    Summary.FinalWorldLocationCm = Samples.Last().WorldLocationCm;
    const FVector FlatForward = FVector(Release.Direction.X, Release.Direction.Y, 0.0f).GetSafeNormal(SMALL_NUMBER, FVector::ForwardVector);
    const FVector Right = FVector::CrossProduct(FVector::UpVector, FlatForward).GetSafeNormal(SMALL_NUMBER, FVector::RightVector);

    float MaximumZCm = Summary.StartWorldLocationCm.Z;
    const FDiscTrajectorySample* FirstGroundSample = nullptr;
    for (const FDiscTrajectorySample& Sample : Samples)
    {
        MaximumZCm = FMath::Max(MaximumZCm, static_cast<float>(Sample.WorldLocationCm.Z));
        if (!FirstGroundSample && (Sample.GroundContactCount > 0 || Sample.GroundState != EDiscGroundState::Airborne))
        {
            FirstGroundSample = &Sample;
        }
    }

    const FVector FinalOffsetMeters = (Summary.FinalWorldLocationCm - Summary.StartWorldLocationCm) / 100.0f;
    const FVector AirOffsetMeters = ((FirstGroundSample ? FirstGroundSample->WorldLocationCm : Summary.FinalWorldLocationCm)
        - Summary.StartWorldLocationCm) / 100.0f;
    Summary.AirTimeSeconds = FirstGroundSample ? FirstGroundSample->TimeSeconds : Telemetry.FlightTimeSeconds;
    Summary.AirCarryMeters = FVector::DotProduct(AirOffsetMeters, FlatForward);
    Summary.FinalCarryMeters = FVector::DotProduct(FinalOffsetMeters, FlatForward);
    Summary.LateralMeters = FVector::DotProduct(FinalOffsetMeters, Right);
    Summary.ApexMeters = (MaximumZCm - Summary.StartWorldLocationCm.Z) / 100.0f;
    return Summary;
}

bool UDiscTrajectorySubsystem::EvaluateRegression(
    const FPhysicsRegressionPreset& Preset,
    FDiscTrajectorySummary& InOutSummary)
{
    InOutSummary.RegressionFailures.Reset();
    AddFailure(InOutSummary, TEXT("air_carry_m"), InOutSummary.AirCarryMeters,
        Preset.Expected.MinAirCarryMeters, Preset.Expected.MaxAirCarryMeters);
    AddFailure(InOutSummary, TEXT("final_carry_m"), InOutSummary.FinalCarryMeters,
        Preset.Expected.MinFinalCarryMeters, Preset.Expected.MaxFinalCarryMeters);
    AddFailure(InOutSummary, TEXT("apex_m"), InOutSummary.ApexMeters,
        Preset.Expected.MinApexMeters, Preset.Expected.MaxApexMeters);
    AddFailure(InOutSummary, TEXT("air_time_s"), InOutSummary.AirTimeSeconds,
        Preset.Expected.MinAirTimeSeconds, Preset.Expected.MaxAirTimeSeconds);
    AddFailure(InOutSummary, TEXT("lateral_m"), InOutSummary.LateralMeters,
        Preset.Expected.MinLateralMeters, Preset.Expected.MaxLateralMeters);
    if (InOutSummary.GroundContactCount < Preset.Expected.MinGroundContacts
        || InOutSummary.GroundContactCount > Preset.Expected.MaxGroundContacts)
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("ground_contacts %d outside [%d, %d]"),
            InOutSummary.GroundContactCount, Preset.Expected.MinGroundContacts, Preset.Expected.MaxGroundContacts));
    }
    if (Preset.Expected.bRequireFinalGroundState
        && InOutSummary.FinalGroundState != Preset.Expected.ExpectedFinalGroundState)
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("final_ground_state %s expected %s"),
            *GroundStateName(InOutSummary.FinalGroundState), *GroundStateName(Preset.Expected.ExpectedFinalGroundState)));
    }
    if (Preset.Expected.ExpectedHoledOut >= 0
        && InOutSummary.bHoledOut != (Preset.Expected.ExpectedHoledOut == 1))
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("holed_out %s expected %s"),
            InOutSummary.bHoledOut ? TEXT("true") : TEXT("false"),
            Preset.Expected.ExpectedHoledOut == 1 ? TEXT("true") : TEXT("false")));
    }
    if (Preset.Expected.bRequireBasketContact
        && InOutSummary.LastBasketContact != Preset.Expected.ExpectedBasketContact)
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("basket_contact %s expected %s"),
            *BasketContactName(InOutSummary.LastBasketContact), *BasketContactName(Preset.Expected.ExpectedBasketContact)));
    }
    if (Preset.Expected.bRequireNoRulesPenalty && InOutSummary.PenaltyStrokes != 0)
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("rules_penalty_strokes %d expected 0"),
            InOutSummary.PenaltyStrokes));
    }
    InOutSummary.bRegressionPassed = InOutSummary.RegressionFailures.IsEmpty();
    return InOutSummary.bRegressionPassed;
}

FString UDiscTrajectorySubsystem::BuildCsv(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscTrajectorySummary& Summary)
{
    TArray<FString> Lines;
    Lines.Add(TEXT("# schema=disc_golf_trajectory"));
    Lines.Add(FString::Printf(TEXT("# schema_version=%d"), TrajectorySchemaVersion));
    Lines.Add(FString::Printf(TEXT("# capture_id=%s"), *Summary.CaptureId));
    Lines.Add(FString::Printf(TEXT("# captured_utc=%s"), *Summary.CapturedUtc));
    Lines.Add(FString::Printf(TEXT("# preset_id=%s"), *Summary.PresetId.ToString()));
    Lines.Add(FString::Printf(TEXT("# render_fps=%d"), Summary.RenderFps));
    Lines.Add(FString::Printf(TEXT("# mold_id=%s"), *Disc.MoldId.ToString()));
    Lines.Add(FString::Printf(TEXT("# plastic=%s"), *PlasticName(Disc.Plastic)));
    Lines.Add(FString::Printf(TEXT("# shot_context=%s"), *ShotContextName(Release.ShotContext)));
    Lines.Add(FString::Printf(TEXT("# holed_out=%s"), Summary.bHoledOut ? TEXT("true") : TEXT("false")));
    Lines.Add(FString::Printf(TEXT("# basket_contact=%s"), *BasketContactName(Summary.LastBasketContact)));
    Lines.Add(FString::Printf(TEXT("# course_surface_at_rest=%s"), *CourseSurfaceName(Summary.SurfaceAtRest)));
    Lines.Add(FString::Printf(TEXT("# resulting_lie=%s"), *LieName(Summary.ResultingLieType)));
    Lines.Add(FString::Printf(TEXT("# penalty_type=%s"), *PenaltyName(Summary.PenaltyType)));
    Lines.Add(FString::Printf(TEXT("# penalty_strokes=%d"), Summary.PenaltyStrokes));
    Lines.Add(TEXT("# position_m is relative to the release sample; world_location_cm is Unreal world space"));
    Lines.Add(TEXT("row_type,index,time_s,pos_x_m,pos_y_m,pos_z_m,world_x_cm,world_y_cm,world_z_cm,velocity_x_mps,velocity_y_mps,velocity_z_mps,normal_x,normal_y,normal_z,wind_x_mps,wind_y_mps,wind_z_mps,spin_rpm,aoa_deg,ground_state,surface,course_surface,ground_contacts,transition_from,transition_to,impact_speed_mps,incidence_deg,edge_angle_deg"));

    const FVector Start = Samples.IsEmpty() ? FVector::ZeroVector : Samples[0].WorldLocationCm;
    for (int32 Index = 0; Index < Samples.Num(); ++Index)
    {
        const FDiscTrajectorySample& Sample = Samples[Index];
        const FVector RelativeMeters = (Sample.WorldLocationCm - Start) / 100.0f;
        const TArray<FString> Columns = {
            TEXT("sample"), FString::FromInt(Index), CsvFloat(Sample.TimeSeconds),
            CsvFloat(RelativeMeters.X), CsvFloat(RelativeMeters.Y), CsvFloat(RelativeMeters.Z),
            CsvFloat(Sample.WorldLocationCm.X), CsvFloat(Sample.WorldLocationCm.Y), CsvFloat(Sample.WorldLocationCm.Z),
            CsvFloat(Sample.VelocityMps.X), CsvFloat(Sample.VelocityMps.Y), CsvFloat(Sample.VelocityMps.Z),
            CsvFloat(Sample.DiscNormalWorld.X), CsvFloat(Sample.DiscNormalWorld.Y), CsvFloat(Sample.DiscNormalWorld.Z),
            CsvFloat(Sample.WindMps.X), CsvFloat(Sample.WindMps.Y), CsvFloat(Sample.WindMps.Z),
            CsvFloat(Sample.SpinRpm), CsvFloat(Sample.AngleOfAttackDeg), GroundStateName(Sample.GroundState),
            GroundSurfaceName(Sample.GroundSurface), CourseSurfaceName(Sample.CourseSurface), FString::FromInt(Sample.GroundContactCount),
            TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT("")};
        Lines.Add(FString::Join(Columns, TEXT(",")));
    }
    for (int32 Index = 0; Index < Transitions.Num(); ++Index)
    {
        const FDiscGroundTransition& Transition = Transitions[Index];
        const FVector RelativeMeters = (Transition.WorldLocationCm - Start) / 100.0f;
        const TArray<FString> Columns = {
            TEXT("transition"), FString::FromInt(Index), CsvFloat(Transition.TimeSeconds),
            CsvFloat(RelativeMeters.X), CsvFloat(RelativeMeters.Y), CsvFloat(RelativeMeters.Z),
            CsvFloat(Transition.WorldLocationCm.X), CsvFloat(Transition.WorldLocationCm.Y), CsvFloat(Transition.WorldLocationCm.Z),
            TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""),
            TEXT(""), TEXT(""), GroundStateName(Transition.ToState), GroundSurfaceName(Transition.Surface),
            CourseSurfaceName(Transition.CourseSurface), FString::FromInt(Transition.GroundContactCount),
            GroundStateName(Transition.FromState), GroundStateName(Transition.ToState),
            CsvFloat(Transition.ImpactSpeedMps), CsvFloat(Transition.IncidenceAngleDeg), CsvFloat(Transition.DiscEdgeAngleDeg)};
        Lines.Add(FString::Join(Columns, TEXT(",")));
    }
    return FString::Join(Lines, TEXT("\n")) + TEXT("\n");
}

FString UDiscTrajectorySubsystem::BuildJson(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& Telemetry,
    const FDiscTrajectorySummary& Summary)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_trajectory"));
    Root->SetNumberField(TEXT("schema_version"), TrajectorySchemaVersion);
    Root->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("coordinate_contract"), TEXT("Unreal world centimeters; relative position and velocity/wind in SI units"));
    Root->SetObjectField(TEXT("summary"), SummaryObject(Summary));

    TSharedRef<FJsonObject> DiscObject = MakeShared<FJsonObject>();
    DiscObject->SetStringField(TEXT("mold_id"), Disc.MoldId.ToString());
    DiscObject->SetStringField(TEXT("display_name"), Disc.DisplayName.ToString());
    DiscObject->SetStringField(TEXT("plastic"), PlasticName(Disc.Plastic));
    DiscObject->SetNumberField(TEXT("speed"), Disc.Speed);
    DiscObject->SetNumberField(TEXT("glide"), Disc.Glide);
    DiscObject->SetNumberField(TEXT("turn"), Disc.Turn);
    DiscObject->SetNumberField(TEXT("fade"), Disc.Fade);
    DiscObject->SetNumberField(TEXT("mass_kg"), Disc.Aero.MassKg);
    DiscObject->SetNumberField(TEXT("diameter_m"), Disc.Aero.DiameterM);
    DiscObject->SetNumberField(TEXT("area_m2"), Disc.Aero.AreaM2);
    DiscObject->SetNumberField(TEXT("cl0"), Disc.Aero.CL0);
    DiscObject->SetNumberField(TEXT("cla"), Disc.Aero.CLa);
    DiscObject->SetNumberField(TEXT("cd0"), Disc.Aero.CD0);
    DiscObject->SetNumberField(TEXT("cda"), Disc.Aero.CDa);
    DiscObject->SetNumberField(TEXT("ground_restitution"), Disc.Aero.GroundRestitution);
    DiscObject->SetNumberField(TEXT("ground_friction"), Disc.Aero.GroundFriction);
    Root->SetObjectField(TEXT("disc"), DiscObject);

    TSharedRef<FJsonObject> ReleaseObject = MakeShared<FJsonObject>();
    ReleaseObject->SetStringField(TEXT("throw_style"), ThrowStyleName(Release.ThrowStyle));
    ReleaseObject->SetStringField(TEXT("shot_context"), ShotContextName(Release.ShotContext));
    ReleaseObject->SetObjectField(TEXT("direction"), VectorObject(Release.Direction));
    ReleaseObject->SetNumberField(TEXT("timing_error"), Release.TimingError);
    ReleaseObject->SetNumberField(TEXT("quality"), Release.Quality01);
    ReleaseObject->SetNumberField(TEXT("release_speed_mps"), Release.ReleaseSpeedMps);
    ReleaseObject->SetNumberField(TEXT("spin_rpm"), Release.SpinRpm);
    ReleaseObject->SetNumberField(TEXT("aim_offset_deg"), Release.AimOffsetDeg);
    ReleaseObject->SetNumberField(TEXT("effective_hyzer_deg"), Release.EffectiveHyzerDeg);
    ReleaseObject->SetNumberField(TEXT("effective_nose_deg"), Release.EffectiveNoseAngleDeg);
    ReleaseObject->SetNumberField(TEXT("effective_launch_deg"), Release.EffectiveLaunchAngleDeg);
    ReleaseObject->SetNumberField(TEXT("lie_power_multiplier"), Release.LiePowerMultiplier);
    ReleaseObject->SetNumberField(TEXT("lie_timing_error_multiplier"), Release.LieTimingErrorMultiplier);
    Root->SetObjectField(TEXT("release"), ReleaseObject);

    TArray<TSharedPtr<FJsonValue>> SampleValues;
    const FVector Start = Samples.IsEmpty() ? FVector::ZeroVector : Samples[0].WorldLocationCm;
    for (const FDiscTrajectorySample& Sample : Samples)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("time_s"), Sample.TimeSeconds);
        Object->SetObjectField(TEXT("relative_position_m"), VectorObject((Sample.WorldLocationCm - Start) / 100.0f));
        Object->SetObjectField(TEXT("world_location_cm"), VectorObject(Sample.WorldLocationCm));
        Object->SetObjectField(TEXT("velocity_mps"), VectorObject(Sample.VelocityMps));
        Object->SetObjectField(TEXT("disc_normal_world"), VectorObject(Sample.DiscNormalWorld));
        Object->SetObjectField(TEXT("wind_mps"), VectorObject(Sample.WindMps));
        Object->SetNumberField(TEXT("spin_rpm"), Sample.SpinRpm);
        Object->SetNumberField(TEXT("angle_of_attack_deg"), Sample.AngleOfAttackDeg);
        Object->SetStringField(TEXT("ground_state"), GroundStateName(Sample.GroundState));
        Object->SetStringField(TEXT("surface"), GroundSurfaceName(Sample.GroundSurface));
        Object->SetStringField(TEXT("course_surface"), CourseSurfaceName(Sample.CourseSurface));
        Object->SetNumberField(TEXT("ground_contact_count"), Sample.GroundContactCount);
        SampleValues.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("samples"), SampleValues);

    TArray<TSharedPtr<FJsonValue>> TransitionValues;
    for (const FDiscGroundTransition& Transition : Transitions)
    {
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("time_s"), Transition.TimeSeconds);
        Object->SetObjectField(TEXT("relative_position_m"), VectorObject((Transition.WorldLocationCm - Start) / 100.0f));
        Object->SetStringField(TEXT("from"), GroundStateName(Transition.FromState));
        Object->SetStringField(TEXT("to"), GroundStateName(Transition.ToState));
        Object->SetStringField(TEXT("surface"), GroundSurfaceName(Transition.Surface));
        Object->SetStringField(TEXT("course_surface"), CourseSurfaceName(Transition.CourseSurface));
        Object->SetNumberField(TEXT("ground_contact_count"), Transition.GroundContactCount);
        Object->SetNumberField(TEXT("impact_speed_mps"), Transition.ImpactSpeedMps);
        Object->SetNumberField(TEXT("incidence_deg"), Transition.IncidenceAngleDeg);
        Object->SetNumberField(TEXT("disc_edge_angle_deg"), Transition.DiscEdgeAngleDeg);
        TransitionValues.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetArrayField(TEXT("ground_transitions"), TransitionValues);

    TSharedRef<FJsonObject> FinalTelemetry = MakeShared<FJsonObject>();
    FinalTelemetry->SetNumberField(TEXT("flight_time_s"), Telemetry.FlightTimeSeconds);
    FinalTelemetry->SetNumberField(TEXT("carry_m"), Telemetry.CarryMeters);
    FinalTelemetry->SetNumberField(TEXT("ground_time_s"), Telemetry.GroundPlayTimeSeconds);
    FinalTelemetry->SetNumberField(TEXT("ground_distance_m"), Telemetry.GroundDistanceMeters);
    FinalTelemetry->SetNumberField(TEXT("last_impact_speed_mps"), Telemetry.LastImpactSpeedMps);
    FinalTelemetry->SetNumberField(TEXT("last_incidence_deg"), Telemetry.LastImpactIncidenceDeg);
    FinalTelemetry->SetNumberField(TEXT("last_edge_angle_deg"), Telemetry.LastDiscEdgeAngleDeg);
    FinalTelemetry->SetNumberField(TEXT("basket_contact_count"), Telemetry.BasketContactCount);
    FinalTelemetry->SetStringField(TEXT("last_basket_contact"), BasketContactName(Telemetry.LastBasketContact));
    FinalTelemetry->SetNumberField(TEXT("fixture_contact_count"), Telemetry.FixtureContactCount);
    FinalTelemetry->SetStringField(TEXT("last_fixture_type"), FixtureTypeName(Telemetry.LastFixtureType));
    FinalTelemetry->SetNumberField(TEXT("last_fixture_impact_speed_mps"), Telemetry.LastFixtureImpactSpeedMps);
    FinalTelemetry->SetObjectField(TEXT("last_fixture_entry_velocity_mps"),
        VectorObject(Telemetry.LastFixtureEntryVelocityMps));
    FinalTelemetry->SetObjectField(TEXT("last_fixture_exit_velocity_mps"),
        VectorObject(Telemetry.LastFixtureExitVelocityMps));
    FinalTelemetry->SetObjectField(TEXT("last_fixture_impact_normal"),
        VectorObject(Telemetry.LastFixtureImpactNormal));
    FinalTelemetry->SetNumberField(TEXT("last_fixture_entry_spin_rpm"), Telemetry.LastFixtureEntrySpinRpm);
    FinalTelemetry->SetNumberField(TEXT("last_fixture_exit_spin_rpm"), Telemetry.LastFixtureExitSpinRpm);
    FinalTelemetry->SetStringField(TEXT("course_surface"), CourseSurfaceName(Telemetry.CourseSurface));
    Root->SetObjectField(TEXT("final_telemetry"), FinalTelemetry);

    FString Output;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Output);
    FJsonSerializer::Serialize(Root, Writer);
    return Output;
}

bool UDiscTrajectorySubsystem::CompleteCapture(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& Telemetry,
    const FDiscGolfLieState& ResultingLie,
    bool bHoledOut,
    FName PresetId,
    int32 RenderFps,
    FDiscTrajectorySummary& OutSummary,
    FString& OutError)
{
    if (Samples.IsEmpty())
    {
        OutError = TEXT("Flight produced no trajectory samples");
        return false;
    }

    OutSummary = BuildSummary(Samples, Transitions, Telemetry, Release, ResultingLie, bHoledOut, PresetId, RenderFps);
    const FDateTime Now = FDateTime::UtcNow();
    OutSummary.CapturedUtc = Now.ToIso8601();
    FString Prefix = PresetId.IsNone() ? Disc.MoldId.ToString() : PresetId.ToString();
    Prefix.ReplaceInline(TEXT(" "), TEXT("_"));
    OutSummary.CaptureId = FString::Printf(TEXT("%s_%s%03dZ"),
        *Prefix, *Now.ToString(TEXT("%Y%m%dT%H%M%S")), Now.GetMillisecond());

    if (!PresetId.IsNone())
    {
        const FPhysicsRegressionPreset* Preset = RegressionPresets.FindByPredicate([PresetId](const FPhysicsRegressionPreset& Entry)
        {
            return Entry.PresetId == PresetId;
        });
        if (!Preset)
        {
            OutError = FString::Printf(TEXT("Unknown regression preset %s"), *PresetId.ToString());
            return false;
        }
        EvaluateRegression(*Preset, OutSummary);
    }

    const FString Json = BuildJson(Disc, Release, Samples, Transitions, Telemetry, OutSummary);
    const FString Csv = BuildCsv(Disc, Release, Samples, Transitions, OutSummary);
    LastExportJsonPath = FPaths::Combine(ExportDirectory, OutSummary.CaptureId + TEXT(".json"));
    LastExportCsvPath = FPaths::Combine(ExportDirectory, OutSummary.CaptureId + TEXT(".csv"));
    const FString LatestJsonPath = FPaths::Combine(ExportDirectory, TEXT("LatestTrajectory.json"));
    const FString LatestCsvPath = FPaths::Combine(ExportDirectory, TEXT("LatestTrajectory.csv"));
    if (!FFileHelper::SaveStringToFile(Json, *LastExportJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Csv, *LastExportCsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Json, *LatestJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Csv, *LatestCsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("Could not write trajectory exports to %s"), *ExportDirectory);
        return false;
    }

    LastSummary = OutSummary;
    bHasLastCapture = true;
    OutError.Reset();
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Trajectory export %s: %d samples, %.2f m air / %.2f m final carry, %.2f m apex -> %s"),
        *OutSummary.CaptureId, OutSummary.SampleCount, OutSummary.AirCarryMeters,
        OutSummary.FinalCarryMeters, OutSummary.ApexMeters, *LastExportJsonPath);
    return true;
}

bool UDiscTrajectorySubsystem::WriteRegressionSuiteReport(
    const TArray<FDiscTrajectorySummary>& Summaries,
    FString& OutReportPath,
    bool& OutPassed,
    FString& OutError) const
{
    if (Summaries.IsEmpty())
    {
        OutError = TEXT("Regression suite has no completed captures");
        OutPassed = false;
        return false;
    }

    OutPassed = true;
    TArray<FString> ComparisonFailures;
    const FDiscTrajectorySummary* Reference = Summaries.FindByPredicate([](const FDiscTrajectorySummary& Summary)
    {
        return Summary.PresetId == FName(TEXT("ApexCalm60"));
    });
    for (const FDiscTrajectorySummary& Summary : Summaries)
    {
        OutPassed &= Summary.bRegressionPassed;
        if (Reference && Summary.PresetId.ToString().StartsWith(TEXT("ApexCalm")) && &Summary != Reference)
        {
            const float CarryDelta = FMath::Abs(Summary.FinalCarryMeters - Reference->FinalCarryMeters);
            const float ApexDelta = FMath::Abs(Summary.ApexMeters - Reference->ApexMeters);
            const float LateralDelta = FMath::Abs(Summary.LateralMeters - Reference->LateralMeters);
            const float GroundDelta = FMath::Abs(Summary.GroundDistanceMeters - Reference->GroundDistanceMeters);
            if (CarryDelta > 0.35f || ApexDelta > 0.10f || LateralDelta > 0.25f || GroundDelta > 0.50f
                || Summary.GroundContactCount != Reference->GroundContactCount)
            {
                ComparisonFailures.Add(FString::Printf(
                    TEXT("%s differs from ApexCalm60: carry %.3f m, apex %.3f m, lateral %.3f m, ground %.3f m, contacts %d/%d"),
                    *Summary.PresetId.ToString(), CarryDelta, ApexDelta, LateralDelta, GroundDelta,
                    Summary.GroundContactCount, Reference->GroundContactCount));
            }
        }
    }
    if (!Reference)
    {
        ComparisonFailures.Add(TEXT("ApexCalm60 reference capture is missing"));
    }
    OutPassed &= ComparisonFailures.IsEmpty();

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_physics_regression_report"));
    Root->SetNumberField(TEXT("schema_version"), 2);
    Root->SetStringField(TEXT("completed_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetBoolField(TEXT("passed"), OutPassed);
    Root->SetStringField(TEXT("frame_rate_reference"), TEXT("ApexCalm60"));
    Root->SetNumberField(TEXT("max_final_carry_delta_m"), 0.35);
    Root->SetNumberField(TEXT("max_apex_delta_m"), 0.10);
    Root->SetNumberField(TEXT("max_lateral_delta_m"), 0.25);
    Root->SetNumberField(TEXT("max_ground_distance_delta_m"), 0.50);
    TArray<TSharedPtr<FJsonValue>> Results;
    for (const FDiscTrajectorySummary& Summary : Summaries)
    {
        Results.Add(MakeShared<FJsonValueObject>(SummaryObject(Summary)));
    }
    Root->SetArrayField(TEXT("results"), Results);
    TArray<TSharedPtr<FJsonValue>> Failures;
    for (const FString& Failure : ComparisonFailures)
    {
        Failures.Add(MakeShared<FJsonValueString>(Failure));
    }
    Root->SetArrayField(TEXT("comparison_failures"), Failures);

    FString Json;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);

    const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PhysicsRegressionReports"));
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FDateTime ReportTime = FDateTime::UtcNow();
    const FString Stamp = FString::Printf(TEXT("%s%03dZ"),
        *ReportTime.ToString(TEXT("%Y%m%dT%H%M%S")), ReportTime.GetMillisecond());
    OutReportPath = FPaths::Combine(ReportDirectory, FString::Printf(TEXT("PhysicsRegression_%s.json"), *Stamp));
    const FString LatestPath = FPaths::Combine(ReportDirectory, TEXT("LatestPhysicsRegression.json"));
    if (!FFileHelper::SaveStringToFile(Json, *OutReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Json, *LatestPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("Could not write regression report to %s"), *ReportDirectory);
        return false;
    }
    OutError.Reset();
    return true;
}
