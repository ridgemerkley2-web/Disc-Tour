#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfFixtureQaRunner.generated.h"

class ADiscActor;
class ADiscGolfWorldFixtureActor;

struct FDiscGolfFixtureQaScenario
{
    FName ScenarioId = NAME_None;
    EDiscGolfFixtureType FixtureType = EDiscGolfFixtureType::Unknown;
    float CommandSpeedMps = 0.0f;
    bool bGlancing = false;
};

struct FDiscGolfFixtureQaResult
{
    FDiscGolfFixtureQaScenario Scenario;
    FDiscFlightTelemetry Telemetry;
    FVector ExpectedExitVelocityMps = FVector::ZeroVector;
    float TangentialEntryRatio = 0.0f;
    bool bPassed = false;
    FString FailureReason;
};

/** Runs real disc/fixture component contacts outside the course and writes a deterministic QA matrix. */
UCLASS()
class DISCGOLFTOUR_API ADiscGolfFixtureQaRunner : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfFixtureQaRunner();
    void Start();

private:
    UPROPERTY() TObjectPtr<ADiscActor> ActiveQaDisc;
    UPROPERTY() TObjectPtr<ADiscGolfWorldFixtureActor> ActiveQaFixture;
    TArray<FDiscGolfFixtureQaScenario> Scenarios;
    TArray<FDiscGolfFixtureQaResult> Results;
    FResolvedDiscDefinition QaDiscDefinition;
    int32 ScenarioIndex = 0;
    FTimerHandle EvaluationTimer;

    void StartNextScenario();
    void EvaluateCurrentScenario();
    void Finish();
    bool WriteReport(bool bPassed, FString& OutPath, FString& OutError) const;
};
