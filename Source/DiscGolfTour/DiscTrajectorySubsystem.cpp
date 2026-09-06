#include "DiscTrajectorySubsystem.h"

#include "DiscGolfMath.h"
#include "DiscGolfTour.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
constexpr int32 TrajectorySchemaVersion = 5;
constexpr int32 RegressionReportSchemaVersion = 3;
constexpr const TCHAR* CanonicalRegressionPresetFileSha1 =
    TEXT("193C48DBCEDDBC629FA5873892F061C30BA4FF21");

struct FCanonicalRegressionScenario
{
    const TCHAR* PresetId;
    int32 RenderFps;
};

constexpr FCanonicalRegressionScenario CanonicalRegressionScenarios[] = {
    {TEXT("ApexCalm30"), 30},
    {TEXT("ApexCalm60"), 60},
    {TEXT("ApexCalm120"), 120},
    {TEXT("ApexForehandCalm60"), 60},
    {TEXT("TouchCircle1Center"), 60},
    {TEXT("TouchCircle2Center"), 60}};

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

FString HandednessName(EDGHandedness Handedness)
{
    switch (Handedness)
    {
        case EDGHandedness::Right: return TEXT("Right");
        case EDGHandedness::Left: return TEXT("Left");
        default: return TEXT("Unknown");
    }
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

bool TryParsePlastic(const FString& Value, EDiscPlastic& OutPlastic)
{
    if (Value.Equals(TEXT("Base"), ESearchCase::IgnoreCase))
    {
        OutPlastic = EDiscPlastic::Base;
        return true;
    }
    if (Value.Equals(TEXT("Tour"), ESearchCase::IgnoreCase))
    {
        OutPlastic = EDiscPlastic::Tour;
        return true;
    }
    if (Value.Equals(TEXT("Crystal"), ESearchCase::IgnoreCase))
    {
        OutPlastic = EDiscPlastic::Crystal;
        return true;
    }
    return false;
}

bool TryParseThrowStyle(const FString& Value, EThrowStyle& OutStyle)
{
    if (Value.Equals(TEXT("Backhand"), ESearchCase::IgnoreCase))
    {
        OutStyle = EThrowStyle::Backhand;
        return true;
    }
    if (Value.Equals(TEXT("Forehand"), ESearchCase::IgnoreCase))
    {
        OutStyle = EThrowStyle::Forehand;
        return true;
    }
    return false;
}

bool TryParseShotContext(const FString& Value, EDiscShotContext& OutContext)
{
    if (Value.Equals(TEXT("Drive"), ESearchCase::IgnoreCase))
    {
        OutContext = EDiscShotContext::Drive;
        return true;
    }
    if (Value.Equals(TEXT("Circle1Putt"), ESearchCase::IgnoreCase))
    {
        OutContext = EDiscShotContext::Circle1Putt;
        return true;
    }
    if (Value.Equals(TEXT("Circle2Putt"), ESearchCase::IgnoreCase))
    {
        OutContext = EDiscShotContext::Circle2Putt;
        return true;
    }
    return false;
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

bool TryParseBasketContact(const FString& Value, EBasketContactResult& OutResult)
{
    if (Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
    {
        OutResult = EBasketContactResult::None;
        return true;
    }
    if (Value.Equals(TEXT("Caught"), ESearchCase::IgnoreCase))
    {
        OutResult = EBasketContactResult::Caught;
        return true;
    }
    if (Value.Equals(TEXT("ChainDeflection"), ESearchCase::IgnoreCase))
    {
        OutResult = EBasketContactResult::ChainDeflection;
        return true;
    }
    if (Value.Equals(TEXT("BandRejection"), ESearchCase::IgnoreCase))
    {
        OutResult = EBasketContactResult::BandRejection;
        return true;
    }
    if (Value.Equals(TEXT("TrayRejection"), ESearchCase::IgnoreCase))
    {
        OutResult = EBasketContactResult::TrayRejection;
        return true;
    }
    return false;
}

bool IsKnownBasketContact(const EBasketContactResult Result)
{
    switch (Result)
    {
        case EBasketContactResult::None:
        case EBasketContactResult::Caught:
        case EBasketContactResult::ChainDeflection:
        case EBasketContactResult::BandRejection:
        case EBasketContactResult::TrayRejection:
            return true;
        default:
            return false;
    }
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

bool TryParseGroundState(const FString& Value, EDiscGroundState& OutState)
{
    if (Value.Equals(TEXT("Airborne"), ESearchCase::IgnoreCase))
    {
        OutState = EDiscGroundState::Airborne;
        return true;
    }
    if (Value.Equals(TEXT("Impact"), ESearchCase::IgnoreCase))
    {
        OutState = EDiscGroundState::Impact;
        return true;
    }
    if (Value.Equals(TEXT("Skipping"), ESearchCase::IgnoreCase))
    {
        OutState = EDiscGroundState::Skipping;
        return true;
    }
    if (Value.Equals(TEXT("Sliding"), ESearchCase::IgnoreCase))
    {
        OutState = EDiscGroundState::Sliding;
        return true;
    }
    if (Value.Equals(TEXT("EdgeRolling"), ESearchCase::IgnoreCase))
    {
        OutState = EDiscGroundState::EdgeRolling;
        return true;
    }
    if (Value.Equals(TEXT("Settled"), ESearchCase::IgnoreCase))
    {
        OutState = EDiscGroundState::Settled;
        return true;
    }
    return false;
}

bool IsKnownGroundState(const EDiscGroundState State)
{
    switch (State)
    {
        case EDiscGroundState::Airborne:
        case EDiscGroundState::Impact:
        case EDiscGroundState::Skipping:
        case EDiscGroundState::Sliding:
        case EDiscGroundState::EdgeRolling:
        case EDiscGroundState::Settled:
            return true;
        default:
            return false;
    }
}

TSharedRef<FJsonObject> VectorObject(const FVector& Vector)
{
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Vector.X);
    Object->SetNumberField(TEXT("y"), Vector.Y);
    Object->SetNumberField(TEXT("z"), Vector.Z);
    return Object;
}

bool ReadFiniteFloatField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutValue)
{
    double Value = 0.0;
    if (!Object.IsValid() || !Object->TryGetNumberField(Field, Value)
        || !FMath::IsFinite(Value)
        || FMath::Abs(Value) > static_cast<double>(TNumericLimits<float>::Max()))
    {
        return false;
    }
    OutValue = static_cast<float>(Value);
    return FMath::IsFinite(OutValue);
}

bool ReadFiniteVectorField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, FVector& OutValue)
{
    const TSharedPtr<FJsonObject>* Vector = nullptr;
    if (!Object.IsValid() || !Object->TryGetObjectField(Field, Vector) || !Vector || !Vector->IsValid())
    {
        return false;
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!(*Vector)->TryGetNumberField(TEXT("x"), X)
        || !(*Vector)->TryGetNumberField(TEXT("y"), Y)
        || !(*Vector)->TryGetNumberField(TEXT("z"), Z)
        || !FMath::IsFinite(X) || !FMath::IsFinite(Y) || !FMath::IsFinite(Z))
    {
        return false;
    }

    OutValue = FVector(X, Y, Z);
    return true;
}

bool ReadFloatRange(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, float& OutMin, float& OutMax)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 2) return false;
    double Minimum = 0.0;
    double Maximum = 0.0;
    if (!(*Values)[0].IsValid() || !(*Values)[1].IsValid()
        || !(*Values)[0]->TryGetNumber(Minimum) || !(*Values)[1]->TryGetNumber(Maximum)
        || !FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum)
        || FMath::Abs(Minimum) > static_cast<double>(TNumericLimits<float>::Max())
        || FMath::Abs(Maximum) > static_cast<double>(TNumericLimits<float>::Max())
        || Minimum > Maximum)
    {
        return false;
    }
    OutMin = static_cast<float>(Minimum);
    OutMax = static_cast<float>(Maximum);
    return FMath::IsFinite(OutMin) && FMath::IsFinite(OutMax);
}

bool ReadIntRange(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, int32& OutMin, int32& OutMax)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values) || !Values || Values->Num() != 2) return false;
    double Minimum = 0.0;
    double Maximum = 0.0;
    if (!(*Values)[0].IsValid() || !(*Values)[1].IsValid()
        || !(*Values)[0]->TryGetNumber(Minimum) || !(*Values)[1]->TryGetNumber(Maximum)
        || !FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum)
        || FMath::TruncToDouble(Minimum) != Minimum || FMath::TruncToDouble(Maximum) != Maximum
        || Minimum < static_cast<double>(MIN_int32) || Minimum > static_cast<double>(MAX_int32)
        || Maximum < static_cast<double>(MIN_int32) || Maximum > static_cast<double>(MAX_int32)
        || Minimum > Maximum)
    {
        return false;
    }
    OutMin = static_cast<int32>(Minimum);
    OutMax = static_cast<int32>(Maximum);
    return true;
}

FString CsvFloat(float Value)
{
    return FString::Printf(TEXT("%.6f"), Value);
}

void AddFailure(FDiscTrajectorySummary& Summary, const TCHAR* Label, float Value, float Minimum, float Maximum)
{
    if (!FMath::IsFinite(Minimum) || !FMath::IsFinite(Maximum) || Minimum > Maximum)
    {
        Summary.RegressionFailures.Add(FString::Printf(TEXT("%s envelope is non-finite or reversed"), Label));
        return;
    }
    if (!FMath::IsFinite(Value))
    {
        Summary.RegressionFailures.Add(FString::Printf(TEXT("%s result is non-finite"), Label));
        return;
    }
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
    Object->SetStringField(TEXT("handedness"), HandednessName(Summary.Handedness));
    Object->SetNumberField(TEXT("wind_phase_origin_s"), Summary.WindPhaseOriginSeconds);
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

#if DG_WITH_DEVELOPMENT_CONTENT
bool BuildRegressionReportJson(
    const TArray<FDiscTrajectorySummary>& Summaries,
    const FString& RunState,
    bool bPassed,
    bool bAuthoritativePresetsLoaded,
    bool bPresentationTraceUnchanged,
    const FString& PresetFileSha1,
    const TArray<FString>& ComparisonFailures,
    FString& OutJson)
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_physics_regression_report"));
    Root->SetNumberField(TEXT("schema_version"), RegressionReportSchemaVersion);
    Root->SetStringField(TEXT("run_state"), RunState);
    Root->SetStringField(TEXT("completed_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetBoolField(TEXT("passed"), bPassed);
    Root->SetBoolField(TEXT("authoritative_presets_loaded"), bAuthoritativePresetsLoaded);
    Root->SetBoolField(TEXT("presentation_trace_unchanged"), bPresentationTraceUnchanged);
    Root->SetStringField(TEXT("preset_file_sha1"), PresetFileSha1);
    Root->SetStringField(TEXT("frame_rate_reference"), TEXT("ApexCalm60"));
    Root->SetNumberField(TEXT("max_final_carry_delta_m"), 0.35);
    Root->SetNumberField(TEXT("max_apex_delta_m"), 0.10);
    Root->SetNumberField(TEXT("max_lateral_delta_m"), 0.25);
    Root->SetNumberField(TEXT("max_ground_distance_delta_m"), 0.50);

    TArray<TSharedPtr<FJsonValue>> Results;
    Results.Reserve(Summaries.Num());
    for (const FDiscTrajectorySummary& Summary : Summaries)
    {
        Results.Add(MakeShared<FJsonValueObject>(SummaryObject(Summary)));
    }
    Root->SetArrayField(TEXT("results"), Results);

    TArray<TSharedPtr<FJsonValue>> Failures;
    Failures.Reserve(ComparisonFailures.Num());
    for (const FString& Failure : ComparisonFailures)
    {
        Failures.Add(MakeShared<FJsonValueString>(Failure));
    }
    Root->SetArrayField(TEXT("comparison_failures"), Failures);

    OutJson.Reset();
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutJson);
    return FJsonSerializer::Serialize(Root, Writer);
}

bool InvalidateLatestRegressionReport()
{
    IFileManager& FileManager = IFileManager::Get();
    const FString LatestPath = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("PhysicsRegressionReports/LatestPhysicsRegression.json"));
    if (FileManager.FileExists(*LatestPath))
    {
        FileManager.Delete(*LatestPath, false, true, true);
    }
    return !FileManager.FileExists(*LatestPath);
}

bool CommitLatestRegressionReport(
    const FString& Json,
    bool bWriteArchive,
    FString& OutReportPath,
    FString& OutError)
{
    OutReportPath.Reset();
    OutError.Reset();

    IFileManager& FileManager = IFileManager::Get();
    const FString ReportDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(), TEXT("PhysicsRegressionReports"));
    const FString LatestPath = FPaths::Combine(
        ReportDirectory, TEXT("LatestPhysicsRegression.json"));
    if (!FileManager.MakeDirectory(*ReportDirectory, true)
        && !FileManager.DirectoryExists(*ReportDirectory))
    {
        OutError = FString::Printf(
            TEXT("Could not create regression report directory %s"), *ReportDirectory);
        return false;
    }

    const FDateTime ReportTime = FDateTime::UtcNow();
    const FString Stamp = FString::Printf(TEXT("%s%03dZ"),
        *ReportTime.ToString(TEXT("%Y%m%dT%H%M%S")), ReportTime.GetMillisecond());
    const FString StagedLatestPath = FPaths::Combine(
        ReportDirectory,
        FString::Printf(TEXT("LatestPhysicsRegression.%s.tmp"), *Stamp));

    if (!FFileHelper::SaveStringToFile(
            Json, *StagedLatestPath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        FileManager.Delete(*StagedLatestPath, false, true, true);
        const bool bInvalidated = InvalidateLatestRegressionReport();
        OutError = FString::Printf(
            TEXT("Could not stage regression report in %s%s"),
            *ReportDirectory,
            bInvalidated ? TEXT("") : TEXT("; prior Latest report could not be invalidated"));
        return false;
    }

    if (!FileManager.Move(
            *LatestPath, *StagedLatestPath,
            true, true, false, true))
    {
        FileManager.Delete(*StagedLatestPath, false, true, true);
        const bool bInvalidated = InvalidateLatestRegressionReport();
        OutError = FString::Printf(
            TEXT("Could not atomically publish regression report to %s%s"),
            *LatestPath,
            bInvalidated ? TEXT("") : TEXT("; prior Latest report could not be invalidated"));
        return false;
    }

    OutReportPath = LatestPath;
    if (bWriteArchive)
    {
        const FString ArchivePath = FPaths::Combine(
            ReportDirectory,
            FString::Printf(TEXT("PhysicsRegression_%s.json"), *Stamp));
        if (FFileHelper::SaveStringToFile(
                Json, *ArchivePath,
                FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            UE_LOG(LogDiscGolfTour, Verbose,
                TEXT("Archived committed regression report to %s."), *ArchivePath);
        }
        else
        {
            UE_LOG(LogDiscGolfTour, Warning,
                TEXT("Latest regression report committed, but archival copy could not be written to %s."),
                *ArchivePath);
        }
    }
    return true;
}
#endif
}

void UDiscTrajectorySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if DG_RELEASE_V05_SCOPE
    ExportDirectory.Reset();
    LastExportJsonPath.Reset();
    LastExportCsvPath.Reset();
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("In-memory trajectory summaries ready; diagnostic file export is disabled."));
#else
    ExportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TrajectoryExports"));
    IFileManager::Get().MakeDirectory(*ExportDirectory, true);

#if DG_WITH_DEVELOPMENT_CONTENT
    FString Error;
    if (!LoadRegressionPresets(Error))
    {
        UE_LOG(LogDiscGolfTour, Warning, TEXT("Physics regression presets could not be loaded: %s. Using built-in fallback."), *Error);
        BuildFallbackPresets();
    }
    UE_LOG(LogDiscGolfTour, Display, TEXT("Trajectory export ready at %s with %d regression preset(s)."),
        *ExportDirectory, RegressionPresets.Num());
#else
    UE_LOG(LogDiscGolfTour, Display, TEXT("Trajectory export ready at %s."), *ExportDirectory);
#endif
#endif
}

FString UDiscTrajectorySubsystem::ComputeRegressionPresetFileSha1(
    const TArray<uint8>& RawBytes)
{
    uint8 Digest[FSHA1::DigestSize];
    const uint8 EmptyByte = 0;
    FSHA1::HashBuffer(
        RawBytes.IsEmpty() ? static_cast<const void*>(&EmptyByte)
                           : static_cast<const void*>(RawBytes.GetData()),
        static_cast<uint64>(RawBytes.Num()),
        Digest);
    return BytesToHex(Digest, FSHA1::DigestSize).ToUpper();
}

bool UDiscTrajectorySubsystem::IsRegressionPresetDigestAuthoritative(
    const FString& Digest)
{
    return Digest.Equals(
        CanonicalRegressionPresetFileSha1,
        ESearchCase::CaseSensitive);
}

bool UDiscTrajectorySubsystem::IsRegressionPresetCommandValid(
    const FPhysicsRegressionPreset& Preset)
{
    // Regression direction is supplied by the golfer at runtime. A canonical
    // horizontal direction lets the loader validate every preset-authored field
    // through the same fail-closed contract used by LaunchThrow.
    return DiscGolfMath::IsThrowCommandValid(Preset.MakeCommand(FVector::ForwardVector));
}

FDiscTrajectorySummary UDiscTrajectorySubsystem::MakeRegressionLaunchFailureSummary(
    const FPhysicsRegressionPreset& Preset,
    EDGHandedness Handedness,
    const FString& Failure)
{
    FDiscTrajectorySummary Summary;
    Summary.PresetId = Preset.PresetId;
    Summary.RenderFps = Preset.RenderFps;
    Summary.Handedness = Handedness;
    Summary.bWasRegression = true;
    Summary.bRegressionPassed = false;
    Summary.RegressionFailures.Add(Failure.IsEmpty()
        ? TEXT("Authoritative throw boundary rejected the regression launch")
        : Failure);
    return Summary;
}

bool UDiscTrajectorySubsystem::LoadRegressionPresets(FString& OutError)
{
    RegressionPresets.Reset();
    bRegressionPresetsAuthoritative = false;
    RegressionPresetFileSha1.Reset();
#if !DG_WITH_DEVELOPMENT_CONTENT
    OutError = TEXT("Regression presets are unavailable in this build");
    return false;
#else
    const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/PhysicsRegressionPresets.json"));
    TArray<uint8> RawBytes;
    if (!FFileHelper::LoadFileToArray(RawBytes, *Path))
    {
        OutError = FString::Printf(TEXT("Cannot read raw bytes from %s"), *Path);
        return false;
    }
    const FString ParsedFileSha1 = ComputeRegressionPresetFileSha1(RawBytes);
    if (!IsRegressionPresetDigestAuthoritative(ParsedFileSha1))
    {
        OutError = FString::Printf(
            TEXT("Preset file SHA-1 %s does not match canonical contract %s"),
            *ParsedFileSha1, CanonicalRegressionPresetFileSha1);
        return false;
    }

    FString Text;
    FFileHelper::BufferToString(Text, RawBytes.GetData(), RawBytes.Num());

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("Preset JSON is malformed");
        return false;
    }
    FString Schema;
    double SchemaVersion = 0.0;
    if (!Root->TryGetStringField(TEXT("schema"), Schema)
        || Schema != TEXT("disc_golf_physics_regression_presets")
        || !Root->TryGetNumberField(TEXT("schema_version"), SchemaVersion)
        || !FMath::IsFinite(SchemaVersion)
        || SchemaVersion != 2.0)
    {
        OutError = TEXT("Unsupported preset schema or version");
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
        FString PresetId;
        FString DisplayName;
        FString MoldId;
        FString Plastic;
        FString ThrowStyle;
        FString ShotContext;
        FString FinalState;
        FString BasketContact;
        bool bExpectedHoledOut = false;
        double RenderFps = 0.0;
        const TSharedPtr<FJsonObject>* ExpectedValue = nullptr;
        const bool bHasRequiredFields =
            Object->TryGetStringField(TEXT("id"), PresetId)
            && Object->TryGetStringField(TEXT("name"), DisplayName)
            && Object->TryGetStringField(TEXT("description"), Preset.Description)
            && Object->TryGetStringField(TEXT("mold_id"), MoldId)
            && Object->TryGetStringField(TEXT("plastic"), Plastic)
            && Object->TryGetStringField(TEXT("throw_style"), ThrowStyle)
            && Object->TryGetStringField(TEXT("shot_context"), ShotContext)
            && TryParsePlastic(Plastic, Preset.Plastic)
            && TryParseThrowStyle(ThrowStyle, Preset.ThrowStyle)
            && TryParseShotContext(ShotContext, Preset.ShotContext)
            && ReadFiniteFloatField(Object, TEXT("start_distance_m"), Preset.StartDistanceMeters)
            && ReadFiniteFloatField(Object, TEXT("aim_offset_deg"), Preset.AimOffsetDeg)
            && ReadFiniteFloatField(Object, TEXT("power"), Preset.Power01)
            && ReadFiniteFloatField(Object, TEXT("hyzer_deg"), Preset.HyzerDeg)
            && ReadFiniteFloatField(Object, TEXT("nose_deg"), Preset.NoseAngleDeg)
            && ReadFiniteFloatField(Object, TEXT("launch_deg"), Preset.LaunchAngleDeg)
            && ReadFiniteFloatField(Object, TEXT("timing_error"), Preset.TimingError)
            && ReadFiniteVectorField(Object, TEXT("wind_mps"), Preset.WindMps)
            && Object->TryGetNumberField(TEXT("render_fps"), RenderFps)
            && FMath::IsFinite(RenderFps)
            && FMath::TruncToDouble(RenderFps) == RenderFps
            && RenderFps >= static_cast<double>(MIN_int32)
            && RenderFps <= static_cast<double>(MAX_int32)
            && Object->TryGetObjectField(TEXT("expected"), ExpectedValue)
            && ExpectedValue && ExpectedValue->IsValid();

        if (bHasRequiredFields)
        {
            Preset.PresetId = FName(*PresetId);
            Preset.DisplayName = FText::FromString(DisplayName);
            Preset.MoldId = FName(*MoldId);
            Preset.RenderFps = static_cast<int32>(RenderFps);
        }
        const TSharedPtr<FJsonObject> Expected = bHasRequiredFields ? *ExpectedValue : nullptr;
        if (!bHasRequiredFields
            || Preset.PresetId.IsNone() || Preset.MoldId.IsNone() || SeenIds.Contains(Preset.PresetId)
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
            || !TryParseGroundState(FinalState, Preset.Expected.ExpectedFinalGroundState)
            || !TryParseBasketContact(BasketContact, Preset.Expected.ExpectedBasketContact)
            || Preset.StartDistanceMeters < 0.0f || Preset.StartDistanceMeters > 25.0f
            || FMath::Abs(Preset.AimOffsetDeg) > 45.0f
            || !IsRegressionPresetCommandValid(Preset))
        {
            const FString ErrorId = PresetId.IsEmpty() ? TEXT("<unknown>") : PresetId;
            OutError = FString::Printf(
                TEXT("Preset %s is incomplete, duplicated, has unknown enum values, or is outside safe bounds"),
                *ErrorId);
            return false;
        }
        Preset.Expected.ExpectedHoledOut = bExpectedHoledOut ? 1 : 0;
        Preset.Expected.bRequireBasketContact = true;
        SeenIds.Add(Preset.PresetId);
        Parsed.Add(Preset);
    }

    RegressionPresets = MoveTemp(Parsed);
    RegressionPresetFileSha1 = ParsedFileSha1;
    bRegressionPresetsAuthoritative = true;
    OutError.Reset();
    return true;
#endif
}

void UDiscTrajectorySubsystem::BuildFallbackPresets()
{
    bRegressionPresetsAuthoritative = false;
    RegressionPresetFileSha1.Reset();
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
    Summary.Handedness = Release.Handedness;
    Summary.WindPhaseOriginSeconds = Release.WindPhaseOriginSeconds;
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
    if (Preset.Expected.MinGroundContacts > Preset.Expected.MaxGroundContacts)
    {
        InOutSummary.RegressionFailures.Add(TEXT("ground_contacts envelope is reversed"));
    }
    else if (InOutSummary.GroundContactCount < Preset.Expected.MinGroundContacts
        || InOutSummary.GroundContactCount > Preset.Expected.MaxGroundContacts)
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("ground_contacts %d outside [%d, %d]"),
            InOutSummary.GroundContactCount, Preset.Expected.MinGroundContacts, Preset.Expected.MaxGroundContacts));
    }
    const bool bExpectedGroundStateKnown = IsKnownGroundState(Preset.Expected.ExpectedFinalGroundState);
    const bool bFinalGroundStateKnown = IsKnownGroundState(InOutSummary.FinalGroundState);
    if (!bExpectedGroundStateKnown)
    {
        InOutSummary.RegressionFailures.Add(TEXT("expected final_ground_state enum is unknown"));
    }
    if (!bFinalGroundStateKnown)
    {
        InOutSummary.RegressionFailures.Add(TEXT("final_ground_state result enum is unknown"));
    }
    if (Preset.Expected.bRequireFinalGroundState && bExpectedGroundStateKnown && bFinalGroundStateKnown
        && InOutSummary.FinalGroundState != Preset.Expected.ExpectedFinalGroundState)
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("final_ground_state %s expected %s"),
            *GroundStateName(InOutSummary.FinalGroundState), *GroundStateName(Preset.Expected.ExpectedFinalGroundState)));
    }
    if (Preset.Expected.ExpectedHoledOut < -1 || Preset.Expected.ExpectedHoledOut > 1)
    {
        InOutSummary.RegressionFailures.Add(TEXT("holed_out envelope must be -1, 0, or 1"));
    }
    else if (Preset.Expected.ExpectedHoledOut >= 0
        && InOutSummary.bHoledOut != (Preset.Expected.ExpectedHoledOut == 1))
    {
        InOutSummary.RegressionFailures.Add(FString::Printf(TEXT("holed_out %s expected %s"),
            InOutSummary.bHoledOut ? TEXT("true") : TEXT("false"),
            Preset.Expected.ExpectedHoledOut == 1 ? TEXT("true") : TEXT("false")));
    }
    const bool bExpectedBasketContactKnown = IsKnownBasketContact(Preset.Expected.ExpectedBasketContact);
    const bool bLastBasketContactKnown = IsKnownBasketContact(InOutSummary.LastBasketContact);
    if (!bExpectedBasketContactKnown)
    {
        InOutSummary.RegressionFailures.Add(TEXT("expected basket_contact enum is unknown"));
    }
    if (!bLastBasketContactKnown)
    {
        InOutSummary.RegressionFailures.Add(TEXT("basket_contact result enum is unknown"));
    }
    if (Preset.Expected.bRequireBasketContact && bExpectedBasketContactKnown && bLastBasketContactKnown
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

bool UDiscTrajectorySubsystem::EvaluateRegressionSuite(
    const TArray<FDiscTrajectorySummary>& Summaries,
    TArray<FString>& OutFailures)
{
    OutFailures.Reset();
    if (Summaries.Num() != UE_ARRAY_COUNT(CanonicalRegressionScenarios))
    {
        OutFailures.Add(FString::Printf(
            TEXT("Canonical regression suite requires exactly %d scenarios; received %d"),
            UE_ARRAY_COUNT(CanonicalRegressionScenarios), Summaries.Num()));
    }

    TMap<FName, const FDiscTrajectorySummary*> CanonicalSummaries;
    TSet<float> WindPhaseOrigins;
    for (const FDiscTrajectorySummary& Summary : Summaries)
    {
        const FCanonicalRegressionScenario* Canonical = nullptr;
        for (const FCanonicalRegressionScenario& Candidate : CanonicalRegressionScenarios)
        {
            if (Summary.PresetId == FName(Candidate.PresetId))
            {
                Canonical = &Candidate;
                break;
            }
        }

        if (!Canonical)
        {
            OutFailures.Add(FString::Printf(
                TEXT("Unknown regression scenario identity %s"), *Summary.PresetId.ToString()));
            continue;
        }
        if (CanonicalSummaries.Contains(Summary.PresetId))
        {
            OutFailures.Add(FString::Printf(
                TEXT("Duplicate regression scenario identity %s"), *Summary.PresetId.ToString()));
            continue;
        }
        CanonicalSummaries.Add(Summary.PresetId, &Summary);

        if (Summary.RenderFps != Canonical->RenderFps)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s render_fps %d expected %d"),
                Canonical->PresetId, Summary.RenderFps, Canonical->RenderFps));
        }
        if (Summary.Handedness != EDGHandedness::Right)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s handedness %s expected Right for the canonical baseline"),
                Canonical->PresetId, *HandednessName(Summary.Handedness)));
        }
        if (!Summary.bWasRegression)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s was not captured as a regression scenario"), Canonical->PresetId));
        }
        if (!Summary.bRegressionPassed)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s failed its acceptance envelope"), Canonical->PresetId));
        }
        else if (!Summary.RegressionFailures.IsEmpty())
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s claims acceptance while retaining failure details"), Canonical->PresetId));
        }
        if (Summary.SampleCount <= 1)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s sample_count %d must be greater than 1"),
                Canonical->PresetId, Summary.SampleCount));
        }
        const bool bCanonicalTouchPutt =
            Summary.PresetId == FName(TEXT("TouchCircle1Center"))
            || Summary.PresetId == FName(TEXT("TouchCircle2Center"));
        if (bCanonicalTouchPutt && !Summary.bHoledOut)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s did not hole out"), Canonical->PresetId));
        }
        if (bCanonicalTouchPutt
            && Summary.LastBasketContact != EBasketContactResult::Caught)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s basket_contact %s expected Caught"),
                Canonical->PresetId,
                *BasketContactName(Summary.LastBasketContact)));
        }
        if (!IsKnownGroundState(Summary.FinalGroundState))
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s final_ground_state result enum is unknown"), Canonical->PresetId));
        }
        if (!IsKnownBasketContact(Summary.LastBasketContact))
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s basket_contact result enum is unknown"), Canonical->PresetId));
        }

        const auto RequireFinite = [&OutFailures, Canonical](const TCHAR* Label, const float Value)
        {
            if (!FMath::IsFinite(Value))
            {
                OutFailures.Add(FString::Printf(
                    TEXT("%s %s result is non-finite"), Canonical->PresetId, Label));
            }
        };
        RequireFinite(TEXT("duration_s"), Summary.DurationSeconds);
        RequireFinite(TEXT("air_carry_m"), Summary.AirCarryMeters);
        RequireFinite(TEXT("final_carry_m"), Summary.FinalCarryMeters);
        RequireFinite(TEXT("apex_m"), Summary.ApexMeters);
        RequireFinite(TEXT("air_time_s"), Summary.AirTimeSeconds);
        RequireFinite(TEXT("lateral_m"), Summary.LateralMeters);
        RequireFinite(TEXT("ground_distance_m"), Summary.GroundDistanceMeters);
        if (!FMath::IsFinite(Summary.WindPhaseOriginSeconds)
            || Summary.WindPhaseOriginSeconds < 0.0f
            || Summary.WindPhaseOriginSeconds >= 4096.0f)
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s wind_phase_origin_s is outside the deterministic phase envelope"),
                Canonical->PresetId));
        }
        else if (WindPhaseOrigins.Contains(Summary.WindPhaseOriginSeconds))
        {
            OutFailures.Add(FString::Printf(
                TEXT("%s reuses deterministic wind phase %.3f within the suite"),
                Canonical->PresetId, Summary.WindPhaseOriginSeconds));
        }
        else
        {
            WindPhaseOrigins.Add(Summary.WindPhaseOriginSeconds);
        }
    }

    for (const FCanonicalRegressionScenario& Canonical : CanonicalRegressionScenarios)
    {
        if (!CanonicalSummaries.Contains(FName(Canonical.PresetId)))
        {
            OutFailures.Add(FString::Printf(
                TEXT("Canonical regression scenario %s is missing"), Canonical.PresetId));
        }
    }

    const FDiscTrajectorySummary* Reference = nullptr;
    if (const FDiscTrajectorySummary* const* Found = CanonicalSummaries.Find(FName(TEXT("ApexCalm60"))))
    {
        Reference = *Found;
    }
    if (Reference)
    {
        for (const TCHAR* ComparedId : {TEXT("ApexCalm30"), TEXT("ApexCalm120")})
        {
            const FDiscTrajectorySummary* const* Found = CanonicalSummaries.Find(FName(ComparedId));
            if (!Found) continue;
            const FDiscTrajectorySummary& Summary = **Found;
            if (!FMath::IsFinite(Summary.FinalCarryMeters) || !FMath::IsFinite(Reference->FinalCarryMeters)
                || !FMath::IsFinite(Summary.ApexMeters) || !FMath::IsFinite(Reference->ApexMeters)
                || !FMath::IsFinite(Summary.LateralMeters) || !FMath::IsFinite(Reference->LateralMeters)
                || !FMath::IsFinite(Summary.GroundDistanceMeters) || !FMath::IsFinite(Reference->GroundDistanceMeters))
            {
                continue;
            }

            const float CarryDelta = FMath::Abs(Summary.FinalCarryMeters - Reference->FinalCarryMeters);
            const float ApexDelta = FMath::Abs(Summary.ApexMeters - Reference->ApexMeters);
            const float LateralDelta = FMath::Abs(Summary.LateralMeters - Reference->LateralMeters);
            const float GroundDelta = FMath::Abs(Summary.GroundDistanceMeters - Reference->GroundDistanceMeters);
            if (CarryDelta > 0.35f || ApexDelta > 0.10f || LateralDelta > 0.25f || GroundDelta > 0.50f
                || Summary.GroundContactCount != Reference->GroundContactCount)
            {
                OutFailures.Add(FString::Printf(
                    TEXT("%s differs from ApexCalm60: carry %.3f m, apex %.3f m, lateral %.3f m, ground %.3f m, contacts %d/%d"),
                    ComparedId, CarryDelta, ApexDelta, LateralDelta, GroundDelta,
                    Summary.GroundContactCount, Reference->GroundContactCount));
            }
        }
    }

    return OutFailures.IsEmpty();
}

bool UDiscTrajectorySubsystem::EvaluateRegressionReportAcceptance(
    const TArray<FDiscTrajectorySummary>& Summaries,
    bool bAuthoritativePresetsLoaded,
    bool bPresentationTraceUnchanged,
    TArray<FString>& OutFailures)
{
    bool bPassed = EvaluateRegressionSuite(Summaries, OutFailures);
    if (!bAuthoritativePresetsLoaded)
    {
        OutFailures.Add(
            TEXT("Canonical preset file was not loaded; built-in fallback captures are diagnostic-only"));
        bPassed = false;
    }
    if (!bPresentationTraceUnchanged)
    {
        OutFailures.Add(TEXT("Regression emitted presentation audio events"));
        bPassed = false;
    }
    return bPassed;
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
    Lines.Add(FString::Printf(TEXT("# handedness=%s"), *HandednessName(Release.Handedness)));
    Lines.Add(FString::Printf(TEXT("# wind_phase_origin_s=%s"),
        *CsvFloat(Release.WindPhaseOriginSeconds)));
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
    ReleaseObject->SetStringField(TEXT("handedness"), HandednessName(Release.Handedness));
    ReleaseObject->SetNumberField(TEXT("wind_phase_origin_s"), Release.WindPhaseOriginSeconds);
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
    FinalTelemetry->SetNumberField(
        TEXT("wind_phase_origin_s"), Telemetry.WindPhaseOriginSeconds);
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

bool UDiscTrajectorySubsystem::DeferNextCaptureExport(FString& OutError)
{
#if DG_RELEASE_V05_SCOPE
    OutError = TEXT("Trajectory file export is unavailable in this release build");
    return false;
#else
    if (bHasPendingDeferredCaptureExport)
    {
        OutError = TEXT("A deferred trajectory capture is already pending; flush or discard it first");
        return false;
    }
    if (bCaptureExportDeferralArmed)
    {
        OutError = TEXT("Trajectory export deferral is already armed");
        return false;
    }

    bCaptureExportDeferralArmed = true;
    OutError.Reset();
    return true;
#endif
}

bool UDiscTrajectorySubsystem::FlushDeferredCaptureExport(
    FDiscTrajectorySummary& OutSummary,
    FString& OutError)
{
    OutSummary = FDiscTrajectorySummary();
#if DG_RELEASE_V05_SCOPE
    OutError = TEXT("Trajectory file export is unavailable in this release build");
    return false;
#else
    if (!bHasPendingDeferredCaptureExport)
    {
        OutError = bCaptureExportDeferralArmed
            ? TEXT("Trajectory export deferral is armed, but no completed capture is pending")
            : TEXT("No deferred trajectory capture is pending");
        return false;
    }

    if (!WriteCaptureExport(
        PendingDeferredDisc,
        PendingDeferredRelease,
        PendingDeferredSamples,
        PendingDeferredTransitions,
        PendingDeferredTelemetry,
        PendingDeferredSummary,
        OutError))
    {
        // Retain the complete value snapshot so a transient I/O failure can be retried.
        return false;
    }

    OutSummary = PendingDeferredSummary;
    ResetDeferredCapturePayload();
    OutError.Reset();
    return true;
#endif
}

bool UDiscTrajectorySubsystem::DiscardDeferredCaptureExport()
{
    const bool bHadDeferredState = bCaptureExportDeferralArmed || bHasPendingDeferredCaptureExport;
    bCaptureExportDeferralArmed = false;
    ResetDeferredCapturePayload();
    return bHadDeferredState;
}

bool UDiscTrajectorySubsystem::GetPendingDeferredCaptureSummary(
    FDiscTrajectorySummary& OutSummary) const
{
    OutSummary = bHasPendingDeferredCaptureExport
        ? PendingDeferredSummary
        : FDiscTrajectorySummary();
    return bHasPendingDeferredCaptureExport;
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
    if (!FMath::IsFinite(Release.WindPhaseOriginSeconds)
        || Release.WindPhaseOriginSeconds < 0.0f
        || Release.WindPhaseOriginSeconds >= 4096.0f
        || !FMath::IsFinite(Telemetry.WindPhaseOriginSeconds)
        || !FMath::IsNearlyEqual(
            Telemetry.WindPhaseOriginSeconds,
            Release.WindPhaseOriginSeconds,
            KINDA_SMALL_NUMBER)
        || !FMath::IsNearlyEqual(
            Telemetry.Release.WindPhaseOriginSeconds,
            Release.WindPhaseOriginSeconds,
            KINDA_SMALL_NUMBER))
    {
        OutError = TEXT("Trajectory release/telemetry wind phase provenance is invalid or inconsistent");
        return false;
    }
    if (bHasPendingDeferredCaptureExport)
    {
        OutError = TEXT("A deferred trajectory capture is already pending; refusing to replace it");
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

#if DG_RELEASE_V05_SCOPE
    // The summary remains gameplay-observable for HUD, replay and round-flow
    // consumers, but normal Shipping play never serializes diagnostic samples.
    LastSummary = OutSummary;
    bHasLastCapture = true;
    LastExportJsonPath.Reset();
    LastExportCsvPath.Reset();
    OutError.Reset();
    return true;
#else
    if (bCaptureExportDeferralArmed)
    {
        // These are deliberate value copies. The flight actor and component are
        // free to finish their normal lifetime before the deferred export flushes.
        PendingDeferredDisc = Disc;
        PendingDeferredRelease = Release;
        PendingDeferredSamples = Samples;
        PendingDeferredTransitions = Transitions;
        PendingDeferredTelemetry = Telemetry;
        PendingDeferredSummary = OutSummary;
        bCaptureExportDeferralArmed = false;
        bHasPendingDeferredCaptureExport = true;

        // The completed flight remains immediately observable even though no disk
        // serialization is performed on this frame.
        LastSummary = OutSummary;
        bHasLastCapture = true;
        OutError.Reset();
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("Trajectory export deferred for capture %s: %d samples, %.2f m air / %.2f m final carry, %.2f m apex"),
            *OutSummary.CaptureId, OutSummary.SampleCount, OutSummary.AirCarryMeters,
            OutSummary.FinalCarryMeters, OutSummary.ApexMeters);
        return true;
    }

    return WriteCaptureExport(Disc, Release, Samples, Transitions, Telemetry, OutSummary, OutError);
#endif
}

bool UDiscTrajectorySubsystem::WriteCaptureExport(
    const FResolvedDiscDefinition& Disc,
    const FThrowRelease& Release,
    const TArray<FDiscTrajectorySample>& Samples,
    const TArray<FDiscGroundTransition>& Transitions,
    const FDiscFlightTelemetry& Telemetry,
    const FDiscTrajectorySummary& Summary,
    FString& OutError)
{
#if DG_RELEASE_V05_SCOPE
    (void)Disc;
    (void)Release;
    (void)Samples;
    (void)Transitions;
    (void)Telemetry;
    (void)Summary;
    OutError = TEXT("Trajectory file export is unavailable in this release build");
    return false;
#else
    const FString Json = BuildJson(Disc, Release, Samples, Transitions, Telemetry, Summary);
    const FString Csv = BuildCsv(Disc, Release, Samples, Transitions, Summary);
    const FString ExportJsonPath = FPaths::Combine(ExportDirectory, Summary.CaptureId + TEXT(".json"));
    const FString ExportCsvPath = FPaths::Combine(ExportDirectory, Summary.CaptureId + TEXT(".csv"));
    const FString LatestJsonPath = FPaths::Combine(ExportDirectory, TEXT("LatestTrajectory.json"));
    const FString LatestCsvPath = FPaths::Combine(ExportDirectory, TEXT("LatestTrajectory.csv"));
    if (!FFileHelper::SaveStringToFile(Json, *ExportJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Csv, *ExportCsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Json, *LatestJsonPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        || !FFileHelper::SaveStringToFile(Csv, *LatestCsvPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FString::Printf(TEXT("Could not write trajectory exports to %s"), *ExportDirectory);
        return false;
    }

    LastExportJsonPath = ExportJsonPath;
    LastExportCsvPath = ExportCsvPath;
    LastSummary = Summary;
    bHasLastCapture = true;
    OutError.Reset();
    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Trajectory export %s: %d samples, %.2f m air / %.2f m final carry, %.2f m apex -> %s"),
        *Summary.CaptureId, Summary.SampleCount, Summary.AirCarryMeters,
        Summary.FinalCarryMeters, Summary.ApexMeters, *LastExportJsonPath);
    return true;
#endif
}

void UDiscTrajectorySubsystem::ResetDeferredCapturePayload()
{
    bHasPendingDeferredCaptureExport = false;
    PendingDeferredDisc = FResolvedDiscDefinition();
    PendingDeferredRelease = FThrowRelease();
    PendingDeferredSamples.Empty();
    PendingDeferredTransitions.Empty();
    PendingDeferredTelemetry = FDiscFlightTelemetry();
    PendingDeferredSummary = FDiscTrajectorySummary();
}

bool UDiscTrajectorySubsystem::WriteRegressionSuiteReport(
    const TArray<FDiscTrajectorySummary>& Summaries,
    bool bPresentationTraceUnchanged,
    FString& OutReportPath,
    bool& OutPassed,
    FString& OutError) const
{
    OutReportPath.Reset();
    OutPassed = false;
    OutError.Reset();
#if !DG_WITH_DEVELOPMENT_CONTENT
    (void)Summaries;
    (void)bPresentationTraceUnchanged;
    OutError = TEXT("Regression reporting is unavailable in this build");
    return false;
#else
    TArray<FString> ComparisonFailures;
    OutPassed = EvaluateRegressionReportAcceptance(
        Summaries, bRegressionPresetsAuthoritative,
        bPresentationTraceUnchanged, ComparisonFailures);

    FString Json;
    if (!BuildRegressionReportJson(
            Summaries,
            TEXT("completed"),
            OutPassed,
            bRegressionPresetsAuthoritative,
            bPresentationTraceUnchanged,
            RegressionPresetFileSha1,
            ComparisonFailures,
            Json))
    {
        InvalidateLatestRegressionReport();
        OutError = TEXT("Could not serialize regression report");
        OutPassed = false;
        return false;
    }

    if (!CommitLatestRegressionReport(Json, true, OutReportPath, OutError))
    {
        OutPassed = false;
        return false;
    }
    return true;
#endif
}

bool UDiscTrajectorySubsystem::BeginRegressionSuiteReport(
    FString& OutReportPath,
    FString& OutError) const
{
    static const TArray<FDiscTrajectorySummary> NoSummaries;
    return WriteRegressionSuiteStateReport(
        NoSummaries,
        TEXT("in_progress"),
        TEXT("Regression suite has not completed"),
        false,
        OutReportPath,
        OutError);
}

bool UDiscTrajectorySubsystem::WriteRegressionSuiteFailureReport(
    const TArray<FDiscTrajectorySummary>& Summaries,
    const FString& Failure,
    FString& OutReportPath,
    FString& OutError) const
{
    return WriteRegressionSuiteStateReport(
        Summaries,
        TEXT("failed"),
        Failure.IsEmpty() ? TEXT("Regression suite failed without a reason") : Failure,
        true,
        OutReportPath,
        OutError);
}

bool UDiscTrajectorySubsystem::WriteRegressionSuiteStateReport(
    const TArray<FDiscTrajectorySummary>& Summaries,
    const FString& RunState,
    const FString& Failure,
    bool bWriteArchive,
    FString& OutReportPath,
    FString& OutError) const
{
    OutReportPath.Reset();
    OutError.Reset();
#if !DG_WITH_DEVELOPMENT_CONTENT
    (void)Summaries;
    (void)RunState;
    (void)Failure;
    (void)bWriteArchive;
    OutError = TEXT("Regression reporting is unavailable in this build");
    return false;
#else
    TArray<FString> Failures;
    Failures.Add(Failure.IsEmpty()
        ? TEXT("Regression suite state is not accepted")
        : Failure);

    FString Json;
    if (!BuildRegressionReportJson(
            Summaries,
            RunState,
            false,
            bRegressionPresetsAuthoritative,
            false,
            RegressionPresetFileSha1,
            Failures,
            Json))
    {
        InvalidateLatestRegressionReport();
        OutError = TEXT("Could not serialize regression state report");
        return false;
    }
    return CommitLatestRegressionReport(
        Json, bWriteArchive, OutReportPath, OutError);
#endif
}
