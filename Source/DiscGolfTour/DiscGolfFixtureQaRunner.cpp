#include "DiscGolfFixtureQaRunner.h"

#include "DiscActor.h"
#include "DiscCatalogSubsystem.h"
#include "DiscFlightComponent.h"
#include "DiscGolfCourseDefinition.h"
#include "DiscGolfMath.h"
#include "DiscGolfTour.h"
#include "DiscGolfWorldFixtureActor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

namespace
{
const TCHAR* FixtureTypeName(EDiscGolfFixtureType Type)
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

const TCHAR* SpeedClassName(float SpeedMps)
{
    if (SpeedMps < 12.0f) return TEXT("Low");
    if (SpeedMps < 20.0f) return TEXT("Medium");
    return TEXT("Drive");
}

TSharedRef<FJsonObject> VectorObject(const FVector& Value)
{
    TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("x"), Value.X);
    Object->SetNumberField(TEXT("y"), Value.Y);
    Object->SetNumberField(TEXT("z"), Value.Z);
    return Object;
}
}

ADiscGolfFixtureQaRunner::ADiscGolfFixtureQaRunner()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ADiscGolfFixtureQaRunner::Start()
{
    UDiscCatalogSubsystem* Catalog = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UDiscCatalogSubsystem>() : nullptr;
    if (!Catalog || !Catalog->ResolveDisc(TEXT("Apex"), EDiscPlastic::Tour, QaDiscDefinition))
    {
        UE_LOG(LogDiscGolfTour, Error, TEXT("FIXTURE COLLISION SMOKE FAIL: Apex/Tour QA disc unavailable."));
        FPlatformMisc::RequestExitWithStatus(false, 1);
        return;
    }

    constexpr float SpeedsMps[] = { 8.0f, 16.0f, 24.0f };
    constexpr EDiscGolfFixtureType FixtureTypes[] = {
        EDiscGolfFixtureType::Tree,
        EDiscGolfFixtureType::Rock,
        EDiscGolfFixtureType::Sign,
        EDiscGolfFixtureType::DenseGrass
    };
    for (EDiscGolfFixtureType FixtureType : FixtureTypes)
    {
        for (float SpeedMps : SpeedsMps)
        {
            for (bool bGlancing : { false, true })
            {
                FDiscGolfFixtureQaScenario Scenario;
                Scenario.FixtureType = FixtureType;
                Scenario.CommandSpeedMps = SpeedMps;
                Scenario.bGlancing = bGlancing;
                Scenario.ScenarioId = FName(*FString::Printf(TEXT("%s_%s_%s"),
                    FixtureTypeName(FixtureType), SpeedClassName(SpeedMps),
                    bGlancing ? TEXT("Glancing") : TEXT("HeadOn")));
                Scenarios.Add(Scenario);
            }
        }
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Fixture collision QA started: %d live disc/fixture scenarios."), Scenarios.Num());
    StartNextScenario();
}

void ADiscGolfFixtureQaRunner::StartNextScenario()
{
    if (ActiveQaDisc) ActiveQaDisc->Destroy();
    if (ActiveQaFixture) ActiveQaFixture->Destroy();
    ActiveQaDisc = nullptr;
    ActiveQaFixture = nullptr;

    if (!Scenarios.IsValidIndex(ScenarioIndex))
    {
        Finish();
        return;
    }

    const FDiscGolfFixtureQaScenario& Scenario = Scenarios[ScenarioIndex];
    const FVector FixtureCenter(5000.0f, -20000.0f - ScenarioIndex * 1000.0f, 8000.0f);
    FDiscGolfCollisionFixtureDefinition Definition;
    Definition.FixtureId = Scenario.ScenarioId;
    Definition.FixtureType = Scenario.FixtureType;
    Definition.LocationCm = FixtureCenter;

    const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
    switch (Scenario.FixtureType)
    {
        case EDiscGolfFixtureType::Tree:
            Definition.Shape = EDiscGolfPrimitiveShape::Cylinder;
            Definition.Scale = FVector(0.42f, 0.42f, 5.0f);
            MeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
            break;
        case EDiscGolfFixtureType::Rock:
            Definition.Shape = EDiscGolfPrimitiveShape::Sphere;
            Definition.Scale = FVector::OneVector;
            MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
            break;
        case EDiscGolfFixtureType::Sign:
            Definition.Shape = EDiscGolfPrimitiveShape::Box;
            Definition.Scale = FVector(0.12f, 2.0f, 1.35f);
            break;
        case EDiscGolfFixtureType::DenseGrass:
            Definition.Shape = EDiscGolfPrimitiveShape::Box;
            Definition.Scale = FVector(5.5f, 2.8f, 1.8f);
            break;
        case EDiscGolfFixtureType::Unknown:
        default:
            break;
    }

    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
    ActiveQaFixture = GetWorld()->SpawnActor<ADiscGolfWorldFixtureActor>();
    if (!Mesh || !ActiveQaFixture)
    {
        FDiscGolfFixtureQaResult Result;
        Result.Scenario = Scenario;
        Result.FailureReason = TEXT("fixture mesh or actor could not spawn");
        Results.Add(Result);
        ++ScenarioIndex;
        StartNextScenario();
        return;
    }
    ActiveQaFixture->Configure(Definition, Mesh);

    FVector Direction = FVector::ForwardVector;
    float LateralOffsetCm = 0.0f;
    if (Scenario.bGlancing)
    {
        if (Scenario.FixtureType == EDiscGolfFixtureType::Tree) LateralOffsetCm = 28.0f;
        else if (Scenario.FixtureType == EDiscGolfFixtureType::Rock) LateralOffsetCm = 48.0f;
        else Direction = FVector(1.0f, 0.42f, 0.0f).GetSafeNormal();
    }
    const float ApproachDistanceCm = Scenario.FixtureType == EDiscGolfFixtureType::DenseGrass
        ? 420.0f : 160.0f;
    const FVector DiscStart = FixtureCenter - Direction * ApproachDistanceCm
        + FVector(0.0f, LateralOffsetCm, 0.0f);
    ActiveQaDisc = GetWorld()->SpawnActor<ADiscActor>(DiscStart, Direction.Rotation());
    if (!ActiveQaDisc)
    {
        FDiscGolfFixtureQaResult Result;
        Result.Scenario = Scenario;
        Result.FailureReason = TEXT("QA disc could not spawn");
        Results.Add(Result);
        ++ScenarioIndex;
        StartNextScenario();
        return;
    }

    ActiveQaDisc->InitializeDisc(QaDiscDefinition, nullptr);
    FThrowRelease Release;
    Release.Grade = EReleaseGrade::Perfect;
    Release.Timing = EReleaseTiming::OnTime;
    Release.Quality01 = 1.0f;
    Release.ReleaseSpeedMps = Scenario.CommandSpeedMps;
    Release.SpinRpm = 500.0f + Scenario.CommandSpeedMps * 30.0f;
    Release.Direction = Direction;
    Release.ThrowStyle = EThrowStyle::Backhand;
    Release.ShotContext = EDiscShotContext::Drive;
    ActiveQaDisc->Throw(Release);

    GetWorldTimerManager().SetTimer(EvaluationTimer, this,
        &ADiscGolfFixtureQaRunner::EvaluateCurrentScenario, 0.65f, false);
}

void ADiscGolfFixtureQaRunner::EvaluateCurrentScenario()
{
    const FDiscGolfFixtureQaScenario& Scenario = Scenarios[ScenarioIndex];
    FDiscGolfFixtureQaResult Result;
    Result.Scenario = Scenario;
    if (!ActiveQaDisc || !ActiveQaDisc->GetFlightComponent())
    {
        Result.FailureReason = TEXT("QA disc disappeared before evaluation");
    }
    else
    {
        Result.Telemetry = ActiveQaDisc->GetFlightComponent()->GetTelemetry();
        const FVector Entry = Result.Telemetry.LastFixtureEntryVelocityMps;
        const FVector Normal = Result.Telemetry.LastFixtureImpactNormal;
        if (Scenario.FixtureType == EDiscGolfFixtureType::DenseGrass)
        {
            Result.ExpectedExitVelocityMps = DiscGolfMath::ResolveFixtureOverlap(
                Entry, Scenario.FixtureType).VelocityMps;
        }
        else
        {
            Result.ExpectedExitVelocityMps = DiscGolfMath::ResolveFixtureImpact(
                Entry, Normal, Scenario.FixtureType).VelocityMps;
            const FVector EntryDirection = Entry.GetSafeNormal();
            const float NormalFraction = FMath::Clamp(
                -FVector::DotProduct(EntryDirection, Normal.GetSafeNormal()), 0.0f, 1.0f);
            Result.TangentialEntryRatio = FMath::Sqrt(FMath::Max(1.0f - NormalFraction * NormalFraction, 0.0f));
        }

        const bool bContact = Result.Telemetry.FixtureContactCount == 1
            && Result.Telemetry.LastFixtureType == Scenario.FixtureType;
        const bool bResponseMatches = Result.Telemetry.LastFixtureExitVelocityMps.Equals(
            Result.ExpectedExitVelocityMps, 0.02f);
        const bool bEnergyReduced = Result.Telemetry.LastFixtureExitVelocityMps.Size()
            <= Result.Telemetry.LastFixtureImpactSpeedMps + 0.01f;
        const bool bSpinReduced = Result.Telemetry.LastFixtureExitSpinRpm
            <= Result.Telemetry.LastFixtureEntrySpinRpm + 0.1f;
        const bool bAngleClassValid = Scenario.FixtureType == EDiscGolfFixtureType::DenseGrass
            || (Scenario.bGlancing ? Result.TangentialEntryRatio > 0.30f
                                   : Result.TangentialEntryRatio < 0.50f);
        const bool bDirectionValid = Scenario.FixtureType != EDiscGolfFixtureType::DenseGrass
            || FVector::DotProduct(Entry.GetSafeNormal(),
                Result.Telemetry.LastFixtureExitVelocityMps.GetSafeNormal()) > 0.999f;
        Result.bPassed = bContact && bResponseMatches && bEnergyReduced
            && bSpinReduced && bAngleClassValid && bDirectionValid;
        if (!Result.bPassed)
        {
            Result.FailureReason = FString::Printf(
                TEXT("contact=%d type=%d response=%d energy=%d spin=%d angle=%d direction=%d"),
                Result.Telemetry.FixtureContactCount,
                static_cast<int32>(Result.Telemetry.LastFixtureType),
                bResponseMatches ? 1 : 0, bEnergyReduced ? 1 : 0,
                bSpinReduced ? 1 : 0, bAngleClassValid ? 1 : 0,
                bDirectionValid ? 1 : 0);
        }
    }

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("FIXTURE QA %s %s: entry %.2f m/s, exit %.2f m/s, spin %.0f -> %.0f rpm%s%s"),
        *Scenario.ScenarioId.ToString(), Result.bPassed ? TEXT("PASS") : TEXT("FAIL"),
        Result.Telemetry.LastFixtureImpactSpeedMps,
        Result.Telemetry.LastFixtureExitVelocityMps.Size(),
        Result.Telemetry.LastFixtureEntrySpinRpm,
        Result.Telemetry.LastFixtureExitSpinRpm,
        Result.FailureReason.IsEmpty() ? TEXT("") : TEXT(" | "), *Result.FailureReason);
    Results.Add(Result);
    ++ScenarioIndex;
    StartNextScenario();
}

bool ADiscGolfFixtureQaRunner::WriteReport(bool bPassed, FString& OutPath, FString& OutError) const
{
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FixtureQaReports"));
    if (!IFileManager::Get().MakeDirectory(*Directory, true) && !IFileManager::Get().DirectoryExists(*Directory))
    {
        OutError = TEXT("could not create FixtureQaReports directory");
        return false;
    }

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("disc_golf_fixture_qa"));
    Root->SetNumberField(TEXT("schema_version"), 1);
    Root->SetStringField(TEXT("collision_profile_id"), TEXT("PineRidgeCompetitiveV2_Fixtures"));
    Root->SetStringField(TEXT("captured_utc"), FDateTime::UtcNow().ToIso8601());
    Root->SetBoolField(TEXT("passed"), bPassed);
    Root->SetNumberField(TEXT("scenario_count"), Results.Num());

    int32 PassedCount = 0;
    TArray<TSharedPtr<FJsonValue>> ResultValues;
    for (const FDiscGolfFixtureQaResult& Result : Results)
    {
        PassedCount += Result.bPassed ? 1 : 0;
        TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetStringField(TEXT("scenario_id"), Result.Scenario.ScenarioId.ToString());
        Object->SetStringField(TEXT("fixture_type"), FixtureTypeName(Result.Scenario.FixtureType));
        Object->SetStringField(TEXT("speed_class"), SpeedClassName(Result.Scenario.CommandSpeedMps));
        Object->SetStringField(TEXT("contact_angle"), Result.Scenario.bGlancing ? TEXT("Glancing") : TEXT("HeadOn"));
        Object->SetNumberField(TEXT("command_speed_mps"), Result.Scenario.CommandSpeedMps);
        Object->SetNumberField(TEXT("entry_speed_mps"), Result.Telemetry.LastFixtureImpactSpeedMps);
        Object->SetNumberField(TEXT("exit_speed_mps"), Result.Telemetry.LastFixtureExitVelocityMps.Size());
        Object->SetNumberField(TEXT("speed_retention"), Result.Telemetry.LastFixtureImpactSpeedMps > SMALL_NUMBER
            ? Result.Telemetry.LastFixtureExitVelocityMps.Size() / Result.Telemetry.LastFixtureImpactSpeedMps : 0.0f);
        Object->SetObjectField(TEXT("entry_velocity_mps"), VectorObject(Result.Telemetry.LastFixtureEntryVelocityMps));
        Object->SetObjectField(TEXT("exit_velocity_mps"), VectorObject(Result.Telemetry.LastFixtureExitVelocityMps));
        Object->SetObjectField(TEXT("expected_exit_velocity_mps"), VectorObject(Result.ExpectedExitVelocityMps));
        Object->SetObjectField(TEXT("impact_normal"), VectorObject(Result.Telemetry.LastFixtureImpactNormal));
        Object->SetNumberField(TEXT("entry_spin_rpm"), Result.Telemetry.LastFixtureEntrySpinRpm);
        Object->SetNumberField(TEXT("exit_spin_rpm"), Result.Telemetry.LastFixtureExitSpinRpm);
        Object->SetNumberField(TEXT("spin_retention"), Result.Telemetry.LastFixtureEntrySpinRpm > SMALL_NUMBER
            ? Result.Telemetry.LastFixtureExitSpinRpm / Result.Telemetry.LastFixtureEntrySpinRpm : 0.0f);
        Object->SetNumberField(TEXT("tangential_entry_ratio"), Result.TangentialEntryRatio);
        Object->SetNumberField(TEXT("fixture_contact_count"), Result.Telemetry.FixtureContactCount);
        Object->SetBoolField(TEXT("passed"), Result.bPassed);
        Object->SetStringField(TEXT("failure_reason"), Result.FailureReason);
        ResultValues.Add(MakeShared<FJsonValueObject>(Object));
    }
    Root->SetNumberField(TEXT("passed_count"), PassedCount);
    Root->SetArrayField(TEXT("results"), ResultValues);

    FString Json;
    const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
    FJsonSerializer::Serialize(Root, Writer);
    const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
    OutPath = FPaths::Combine(Directory, FString::Printf(TEXT("FixtureQa_%s.json"), *Timestamp));
    const FString LatestPath = FPaths::Combine(Directory, TEXT("LatestFixtureQa.json"));
    if (!FFileHelper::SaveStringToFile(Json, *OutPath)
        || !FFileHelper::SaveStringToFile(Json, *LatestPath))
    {
        OutError = TEXT("could not write fixture QA report");
        return false;
    }
    OutError.Reset();
    return true;
}

void ADiscGolfFixtureQaRunner::Finish()
{
    if (ActiveQaDisc) ActiveQaDisc->Destroy();
    if (ActiveQaFixture) ActiveQaFixture->Destroy();
    ActiveQaDisc = nullptr;
    ActiveQaFixture = nullptr;

    int32 PassedCount = 0;
    for (const FDiscGolfFixtureQaResult& Result : Results)
    {
        PassedCount += Result.bPassed ? 1 : 0;
    }
    bool bPassed = Results.Num() == 24 && PassedCount == Results.Num();
    FString ReportPath;
    FString Error;
    if (!WriteReport(bPassed, ReportPath, Error)) bPassed = false;

    if (bPassed)
    {
        UE_LOG(LogDiscGolfTour, Display,
            TEXT("FIXTURE COLLISION SMOKE PASS: %d/%d live scenarios; report %s"),
            PassedCount, Results.Num(), *ReportPath);
    }
    else
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("FIXTURE COLLISION SMOKE FAIL: %d/%d scenarios passed; %s%s%s"),
            PassedCount, Results.Num(), *Error,
            Error.IsEmpty() ? TEXT("") : TEXT(" | "), *ReportPath);
    }
    FPlatformMisc::RequestExitWithStatus(false, bPassed ? 0 : 1);
}
