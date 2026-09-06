#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

#include "../BasketActor.h"
#include "../DiscActor.h"
#include "../DiscFlightComponent.h"
#include "../DiscGolfMath.h"
#include "../DiscGolfVegetationInteractionActor.h"
#include "../WindDirector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Components/BoxComponent.h"
#include "GameFramework/WorldSettings.h"

#include <limits>

namespace
{
FResolvedDiscDefinition MakeBoundaryDisc()
{
    FResolvedDiscDefinition Disc;
    Disc.MoldId = TEXT("BoundaryDisc");
    Disc.DisplayName = FText::FromString(TEXT("Solver Boundary Disc"));
    return Disc;
}

FThrowRelease MakeBoundaryRelease(float WindPhaseOriginSeconds = 211.0f)
{
    FThrowCommand Command;
    Command.MoldId = TEXT("BoundaryDisc");
    Command.Direction = FVector::ForwardVector;
    Command.Power01 = 0.74f;
    Command.HyzerDeg = 4.0f;
    Command.NoseAngleDeg = 1.0f;
    Command.LaunchAngleDeg = 9.0f;
    Command.TimingError = 0.0f;
    FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);
    Release.WindPhaseOriginSeconds = WindPhaseOriginSeconds;
    return Release;
}

UWorld* MakeBoundaryWorld(const TCHAR* BaseName)
{
    const FName WorldName = MakeUniqueObjectName(
        GetTransientPackage(), UWorld::StaticClass(), FName(BaseName));
    return UWorld::CreateWorld(
        EWorldType::Game, false, WorldName, GetTransientPackage());
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightTransactionalBoundaryTest,
    "DiscGolfTour.Physics.SolverBoundary.TransactionalLaunch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightTransactionalBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("TransactionalBoundaryWorld"));
    if (!TestNotNull(TEXT("Boundary test world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    const FResolvedDiscDefinition ValidDisc = MakeBoundaryDisc();
    const FThrowRelease ValidRelease = MakeBoundaryRelease();
    ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 0.0f, 2000.0f), FRotator(0.0f, 17.0f, 0.0f));
    if (!TestNotNull(TEXT("Boundary disc actor exists"), DiscActor)) return false;
    UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();
    if (!TestNotNull(TEXT("Boundary flight component exists"), Flight)) return false;
    const FTransform InitialTransform = DiscActor->GetActorTransform();
    TestTrue(TEXT("Spawned disc uses the canonical authoritative collision transform"),
        UDiscFlightComponent::IsOwnerTransformValidForLaunch(InitialTransform));

    AddExpectedError(
        TEXT("Disc actor initialization rejected"),
        EAutomationExpectedErrorFlags::Contains, 2);
    AddExpectedError(
        TEXT("Disc actor throw rejected because initialization never completed"),
        EAutomationExpectedErrorFlags::Contains, 1);
    AddExpectedError(
        TEXT("Disc launch rejected because its release is invalid"),
        EAutomationExpectedErrorFlags::Contains, 2);

    FResolvedDiscDefinition InvalidDisc = ValidDisc;
    InvalidDisc.Aero.MassKg = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite resolved disc initialization is rejected"),
        DiscActor->InitializeDisc(InvalidDisc, nullptr));
    TestFalse(TEXT("Rejected initialization is not marked successful"),
        DiscActor->IsDiscInitialized());
    TestFalse(TEXT("An uninitialized actor cannot forward a release"),
        DiscActor->Throw(ValidRelease));
    TestEqual(TEXT("Rejected disc remains Idle"),
        Flight->GetFlightState(), EDiscFlightState::Idle);
    TestTrue(TEXT("Rejected disc creates no samples"),
        Flight->GetTrajectorySamples().IsEmpty());
    TestTrue(TEXT("Rejected disc leaves the actor transform unchanged"),
        DiscActor->GetActorTransform().Equals(InitialTransform));

    TestTrue(TEXT("Valid resolved disc initializes transactionally"),
        DiscActor->InitializeDisc(ValidDisc, nullptr));
    TestTrue(TEXT("Successful initialization is tracked"),
        DiscActor->IsDiscInitialized() && Flight->IsDiscConfigured());
    TestFalse(TEXT("Invalid reconfiguration is rejected"),
        DiscActor->InitializeDisc(InvalidDisc, nullptr));
    TestEqual(TEXT("Invalid reconfiguration preserves the accepted disc"),
        DiscActor->GetResolvedDisc().MoldId, ValidDisc.MoldId);
    TestTrue(TEXT("Invalid reconfiguration preserves prior initialization"),
        DiscActor->IsDiscInitialized());
    AddExpectedError(
        TEXT("Disc configuration rejected"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Component-level invalid configuration is transactional"),
        Flight->ConfigureDisc(InvalidDisc));
    TestEqual(TEXT("Component-level rejection preserves the accepted disc"),
        Flight->GetDisc().MoldId, ValidDisc.MoldId);
    TestTrue(TEXT("Component remains configured after rejected replacement"),
        Flight->IsDiscConfigured());

    FThrowRelease InvalidRelease = ValidRelease;
    InvalidRelease.ReleaseSpeedMps = std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("Non-finite release is rejected"), DiscActor->Throw(InvalidRelease));
    InvalidRelease = ValidRelease;
    InvalidRelease.ThrowStyle = static_cast<EThrowStyle>(255);
    TestFalse(TEXT("Unknown release enum is rejected"), DiscActor->Throw(InvalidRelease));
    TestEqual(TEXT("Rejected releases remain Idle"),
        Flight->GetFlightState(), EDiscFlightState::Idle);
    TestTrue(TEXT("Rejected releases create no samples"),
        Flight->GetTrajectorySamples().IsEmpty());
    TestTrue(TEXT("Rejected releases leave the actor transform unchanged"),
        DiscActor->GetActorTransform().Equals(InitialTransform));

    struct FConfigMutation
    {
        const TCHAR* Name;
        float UDiscFlightComponent::* Member;
        float OutOfRangeValue;
    };
    const FConfigMutation ConfigMutations[] = {
        { TEXT("AirDensityKgM3"), &UDiscFlightComponent::AirDensityKgM3, 0.0f },
        { TEXT("FixedStepSeconds"), &UDiscFlightComponent::FixedStepSeconds, 0.1f },
        { TEXT("MaxFlightSeconds"), &UDiscFlightComponent::MaxFlightSeconds, 121.0f },
        { TEXT("MaxGroundPlaySeconds"), &UDiscFlightComponent::MaxGroundPlaySeconds, 61.0f },
        { TEXT("MaxPrecessionRateRadPerSec"), &UDiscFlightComponent::MaxPrecessionRateRadPerSec, 21.0f },
        { TEXT("GroundContactBiasMps"), &UDiscFlightComponent::GroundContactBiasMps, -0.1f },
        { TEXT("GroundProbeDepthCm"), &UDiscFlightComponent::GroundProbeDepthCm, 0.0f },
        { TEXT("TrajectorySampleHz"), &UDiscFlightComponent::TrajectorySampleHz, 1001.0f }
    };
    AddExpectedError(
        TEXT("Disc launch rejected because its simulation configuration is invalid"),
        EAutomationExpectedErrorFlags::Contains,
        static_cast<int32>(UE_ARRAY_COUNT(ConfigMutations)) * 2);
    for (const FConfigMutation& Mutation : ConfigMutations)
    {
        const float AcceptedValue = Flight->*(Mutation.Member);
        const float RejectedValues[] = {
            std::numeric_limits<float>::quiet_NaN(),
            Mutation.OutOfRangeValue
        };
        for (const float RejectedValue : RejectedValues)
        {
            Flight->*(Mutation.Member) = RejectedValue;
            TestFalse(*FString::Printf(TEXT("%s rejects invalid launch configuration"), Mutation.Name),
                DiscActor->Throw(ValidRelease));
            TestEqual(*FString::Printf(TEXT("%s rejection remains Idle"), Mutation.Name),
                Flight->GetFlightState(), EDiscFlightState::Idle);
            TestTrue(*FString::Printf(TEXT("%s rejection creates no samples"), Mutation.Name),
                Flight->GetTrajectorySamples().IsEmpty());
            TestTrue(*FString::Printf(TEXT("%s rejection preserves transform"), Mutation.Name),
                DiscActor->GetActorTransform().Equals(InitialTransform));
        }
        Flight->*(Mutation.Member) = AcceptedValue;
    }

    FString ConfigError;
    const float AcceptedFixedStepSeconds = Flight->FixedStepSeconds;
    const float AcceptedTrajectorySampleHz = Flight->TrajectorySampleHz;
    Flight->FixedStepSeconds = 1.0f / 30.0f;
    Flight->TrajectorySampleHz = 30.0f;
    TestTrue(TEXT("Trajectory sampling at the fixed-step rate is accepted"),
        Flight->ValidateSimulationConfiguration(ConfigError));
    Flight->TrajectorySampleHz = 30.01f;
    TestFalse(TEXT("Trajectory sampling faster than solver integration is rejected"),
        Flight->ValidateSimulationConfiguration(ConfigError));
    TestTrue(TEXT("Cross-field sampling rejection identifies the rate relationship"),
        ConfigError.Contains(TEXT("cannot exceed the fixed-step simulation rate")));
    Flight->FixedStepSeconds = AcceptedFixedStepSeconds;
    Flight->TrajectorySampleHz = AcceptedTrajectorySampleHz;
    TestTrue(TEXT("Restored simulation configuration validates"),
        Flight->ValidateSimulationConfiguration(ConfigError));
    TestTrue(TEXT("Valid release enters authoritative flight"),
        DiscActor->Throw(ValidRelease));
    TestEqual(TEXT("Valid release enters Flying"),
        Flight->GetFlightState(), EDiscFlightState::Flying);
    TestEqual(TEXT("Valid launch records exactly one initial sample"),
        Flight->GetTrajectorySamples().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightLauncherCollisionExclusionTest,
    "DiscGolfTour.Physics.SolverBoundary.LauncherCollisionExclusion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightLauncherCollisionExclusionTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("LauncherCollisionExclusionWorld"));
    if (!TestNotNull(TEXT("Launcher exclusion world exists"), World)
        || !TestNotNull(TEXT("Launcher exclusion engine exists"), GEngine))
    {
        if (World) World->DestroyWorld(false);
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        if (World->HasBegunPlay())
        {
            World->EndPlay(EEndPlayReason::Quit);
        }
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };

    World->InitializeActorsForPlay(FURL());
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!TestNotNull(TEXT("Launcher exclusion world settings exist"), WorldSettings))
    {
        return false;
    }
    WorldSettings->NotifyBeginPlay();

    const FVector LaunchLocation(0.0f, 0.0f, 2000.0f);
    AActor* Launcher = World->SpawnActor<AActor>(
        LaunchLocation, FRotator::ZeroRotator);
    AActor* UnrelatedActor = World->SpawnActor<AActor>(
        LaunchLocation + FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
    ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
        LaunchLocation, FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Launcher actor exists"), Launcher)
        || !TestNotNull(TEXT("Unrelated actor exists"), UnrelatedActor)
        || !TestNotNull(TEXT("Launcher exclusion disc exists"), DiscActor))
    {
        return false;
    }

    UBoxComponent* LauncherCollision = NewObject<UBoxComponent>(Launcher);
    if (!TestNotNull(TEXT("Launcher blocking component exists"), LauncherCollision))
    {
        return false;
    }
    Launcher->SetRootComponent(LauncherCollision);
    LauncherCollision->SetBoxExtent(FVector(90.0f));
    LauncherCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    LauncherCollision->SetCollisionObjectType(ECC_Pawn);
    LauncherCollision->SetCollisionResponseToAllChannels(ECR_Block);
    LauncherCollision->RegisterComponent();
    Launcher->UpdateOverlaps(false);
    DiscActor->UpdateOverlaps(false);

    TestFalse(TEXT("Null launchers cannot broaden collision exclusions"),
        DiscActor->ConfigureLaunchingActorCollisionExclusion(nullptr));
    TestTrue(TEXT("The exact launcher is excluded from swept disc movement"),
        DiscActor->ConfigureLaunchingActorCollisionExclusion(Launcher));
    TestTrue(TEXT("The launcher exclusion is queryable"),
        DiscActor->IsLaunchingActorCollisionExcluded(Launcher));
    TestFalse(TEXT("Unrelated actors retain authoritative collision"),
        DiscActor->IsLaunchingActorCollisionExcluded(UnrelatedActor));
    TestFalse(TEXT("The exclusion cannot be broadened to a second actor"),
        DiscActor->ConfigureLaunchingActorCollisionExclusion(UnrelatedActor));
    TestTrue(TEXT("A rejected broadening attempt preserves only the launcher"),
        DiscActor->IsLaunchingActorCollisionExcluded(Launcher));

    if (!TestTrue(TEXT("Launcher exclusion disc initializes"),
            DiscActor->InitializeDisc(MakeBoundaryDisc(), nullptr))
        || !TestTrue(TEXT("Launcher exclusion flight launches"),
            DiscActor->Throw(MakeBoundaryRelease(223.0f))))
    {
        return false;
    }
    UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();
    if (!TestNotNull(TEXT("Launcher exclusion flight exists"), Flight))
    {
        return false;
    }

    Flight->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    const FDiscFlightTelemetry& Telemetry = Flight->GetTelemetry();
    TestTrue(TEXT("Launch remains authoritative after clearing the launcher"),
        Flight->IsFlying());
    TestEqual(TEXT("The launcher cannot become a time-zero ground contact"),
        Telemetry.GroundContactCount, 0);
    TestEqual(TEXT("The launch remains airborne"),
        Telemetry.GroundState, EDiscGroundState::Airborne);
    TestTrue(TEXT("The disc advances away from its exact grip origin"),
        FVector::Dist(DiscActor->GetActorLocation(), LaunchLocation) > 1.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightRuntimeSafetyBoundaryTest,
    "DiscGolfTour.Physics.SolverBoundary.RuntimeSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightRuntimeSafetyBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("RuntimeSafetyBoundaryWorld"));
    if (!TestNotNull(TEXT("Runtime safety world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    const FResolvedDiscDefinition ValidDisc = MakeBoundaryDisc();
    const FThrowRelease ValidRelease = MakeBoundaryRelease(307.0f);

    AWindDirector* AcceptedWind = World->SpawnActor<AWindDirector>();
    AWindDirector* RejectedWind = World->SpawnActor<AWindDirector>();
    ADiscActor* WindTransactionDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, -3000.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Accepted wind exists"), AcceptedWind)
        || !TestNotNull(TEXT("Rejected wind exists"), RejectedWind)
        || !TestNotNull(TEXT("Wind transaction disc exists"), WindTransactionDisc))
    {
        return false;
    }
    RejectedWind->BaseWindMps.X = std::numeric_limits<float>::quiet_NaN();
    ADiscActor* WindInitDisc = World->SpawnActor<ADiscActor>(
        FVector(3000.0f, -3000.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Wind initialization transaction disc exists"), WindInitDisc))
    {
        return false;
    }
    AddExpectedError(
        TEXT("Disc actor initialization rejected by wind configuration"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Invalid initial wind rejects the complete actor transaction"),
        WindInitDisc->InitializeDisc(ValidDisc, RejectedWind));
    TestFalse(TEXT("Rejected initial wind does not mark actor initialized"),
        WindInitDisc->IsDiscInitialized());
    TestFalse(TEXT("Rejected initial wind does not partially configure the component"),
        WindInitDisc->GetFlightComponent()->IsDiscConfigured());
    TestTrue(TEXT("Rejected initial wind preserves the unresolved actor snapshot"),
        WindInitDisc->GetResolvedDisc().MoldId.IsNone());
    TestTrue(TEXT("Disc initializes with accepted wind"),
        WindTransactionDisc->InitializeDisc(ValidDisc, AcceptedWind));
    AddExpectedError(
        TEXT("Disc flight rejected an invalid wind director"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Invalid wind reassignment is rejected"),
        WindTransactionDisc->GetFlightComponent()->SetWindDirector(RejectedWind));
    TestTrue(TEXT("Rejected wind reassignment preserves the accepted wind"),
        WindTransactionDisc->Throw(ValidRelease));
    WindTransactionDisc->GetFlightComponent()->StopFlight(false);

    AWindDirector* CorruptedAfterInitWind = World->SpawnActor<AWindDirector>();
    ADiscActor* WindPreflightDisc = World->SpawnActor<ADiscActor>(
        FVector(-3000.0f, -3000.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Post-initialization wind exists"), CorruptedAfterInitWind)
        || !TestNotNull(TEXT("Wind preflight disc exists"), WindPreflightDisc))
    {
        return false;
    }
    TestTrue(TEXT("Wind preflight disc initializes before corruption"),
        WindPreflightDisc->InitializeDisc(ValidDisc, CorruptedAfterInitWind));
    const FTransform WindPreflightTransform = WindPreflightDisc->GetActorTransform();
    CorruptedAfterInitWind->GustAmplitudeMps =
        std::numeric_limits<float>::infinity();
    AddExpectedError(
        TEXT("Disc launch rejected because its physics wind configuration could not be frozen"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("Post-initialization wind corruption rejects launch"),
        WindPreflightDisc->Throw(ValidRelease));
    TestEqual(TEXT("Wind-rejected launch remains Idle"),
        WindPreflightDisc->GetFlightComponent()->GetFlightState(),
        EDiscFlightState::Idle);
    TestTrue(TEXT("Wind-rejected launch records no samples"),
        WindPreflightDisc->GetFlightComponent()->GetTrajectorySamples().IsEmpty());
    TestTrue(TEXT("Wind-rejected launch preserves transform"),
        WindPreflightDisc->GetActorTransform().Equals(WindPreflightTransform));

    ADiscActor* BacklogDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 0.0f, 4000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Backlog disc exists"), BacklogDisc)) return false;
    TestTrue(TEXT("Backlog disc initializes"),
        BacklogDisc->InitializeDisc(ValidDisc, nullptr));
    UDiscFlightComponent* BacklogFlight = BacklogDisc->GetFlightComponent();
    BacklogFlight->FixedStepSeconds = 0.001f;
    TestTrue(TEXT("Backlog flight launches"), BacklogDisc->Throw(ValidRelease));
    TestFalse(TEXT("A newly accepted flight clears invalid-flight diagnostics"),
        BacklogFlight->DidLastFlightTerminateInvalidly());
    BacklogFlight->TickComponent(1.0f, LEVELTICK_All, nullptr);
    TestTrue(TEXT("Accepted 100 ms tick drains at the 1 ms fixed-step boundary"),
        FMath::IsNearlyEqual(
            BacklogFlight->GetTelemetry().FlightTimeSeconds, 0.1f, 0.0011f));
    TestTrue(TEXT("Backlog remains bounded below one pending fixed step"),
        BacklogFlight->GetPendingSimulationTimeSeconds()
            < static_cast<double>(BacklogFlight->FixedStepSeconds) + UE_DOUBLE_SMALL_NUMBER);
    AddExpectedError(
        TEXT("Authoritative flight terminated at tick configuration"),
        EAutomationExpectedErrorFlags::Contains, 1);
    BacklogFlight->AirDensityKgM3 = std::numeric_limits<float>::quiet_NaN();
    BacklogFlight->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Mid-flight invalid configuration terminates flight"),
        BacklogFlight->IsFlying());
    TestEqual(TEXT("Invalid runtime configuration settles fail-closed"),
        BacklogFlight->GetFlightState(), EDiscFlightState::Settled);
    TestTrue(TEXT("Invalid runtime configuration latches an evidence-visible diagnostic"),
        BacklogFlight->DidLastFlightTerminateInvalidly());
    TestEqual(TEXT("Invalid runtime configuration preserves its failure context"),
        BacklogFlight->GetLastFlightValidationFailureContext(),
        FString(TEXT("tick configuration")));
    TestFalse(TEXT("Invalid runtime configuration preserves a non-empty failure reason"),
        BacklogFlight->GetLastFlightValidationFailure().IsEmpty());

    BacklogFlight->AirDensityKgM3 = 1.225f;
    TestTrue(TEXT("A valid relaunch succeeds after diagnostic termination"),
        BacklogDisc->Throw(ValidRelease));
    TestFalse(TEXT("A valid relaunch clears the prior invalid-flight latch"),
        BacklogFlight->DidLastFlightTerminateInvalidly());
    TestTrue(TEXT("A valid relaunch clears the prior invalid-flight context"),
        BacklogFlight->GetLastFlightValidationFailureContext().IsEmpty());
    TestTrue(TEXT("A valid relaunch clears the prior invalid-flight reason"),
        BacklogFlight->GetLastFlightValidationFailure().IsEmpty());
    BacklogFlight->StopFlight(false);

    ADiscActor* DeltaDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 3000.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Delta guard disc exists"), DeltaDisc)) return false;
    TestTrue(TEXT("Delta guard disc initializes"),
        DeltaDisc->InitializeDisc(ValidDisc, nullptr));
    TestTrue(TEXT("Delta guard flight launches"), DeltaDisc->Throw(ValidRelease));
    AddExpectedError(
        TEXT("Authoritative flight terminated at tick delta"),
        EAutomationExpectedErrorFlags::Contains, 1);
    DeltaDisc->GetFlightComponent()->TickComponent(
        std::numeric_limits<float>::quiet_NaN(), LEVELTICK_All, nullptr);
    TestFalse(TEXT("Non-finite tick delta terminates flight"),
        DeltaDisc->GetFlightComponent()->IsFlying());
    TestTrue(TEXT("Non-finite tick delta latches an evidence-visible diagnostic"),
        DeltaDisc->GetFlightComponent()->DidLastFlightTerminateInvalidly());
    TestEqual(TEXT("Non-finite tick delta preserves its failure context"),
        DeltaDisc->GetFlightComponent()->GetLastFlightValidationFailureContext(),
        FString(TEXT("tick delta")));

    FTransform InvalidTransform = FTransform::Identity;
    InvalidTransform.SetScale3D(FVector(
        std::numeric_limits<float>::quiet_NaN(), 1.0f, 1.0f));
    TestFalse(TEXT("Non-finite launch transform is rejected by the shared seam"),
        UDiscFlightComponent::IsOwnerTransformValidForLaunch(InvalidTransform));
    TestFalse(TEXT("Non-finite spawn transform is rejected by the shared seam"),
        UDiscFlightComponent::IsSpawnTransformValidForLaunch(InvalidTransform));
    TestFalse(TEXT("Noncanonical finite launch scale is rejected by the shared seam"),
        UDiscFlightComponent::IsOwnerTransformValidForLaunch(FTransform::Identity));
    TestTrue(TEXT("Unit-scale finite spawn transform is accepted by the shared seam"),
        UDiscFlightComponent::IsSpawnTransformValidForLaunch(FTransform::Identity));
    FTransform CanonicalTransform = FTransform::Identity;
    CanonicalTransform.SetScale3D(DiscGolfMath::CanonicalDiscCollisionScale());
    TestTrue(TEXT("Canonical finite launch transform is accepted by the shared seam"),
        UDiscFlightComponent::IsOwnerTransformValidForLaunch(CanonicalTransform));
    TestFalse(TEXT("Canonical collision scale is not a pre-spawn transform"),
        UDiscFlightComponent::IsSpawnTransformValidForLaunch(CanonicalTransform));
    TestFalse(TEXT("Non-finite hit normal is rejected"),
        UDiscFlightComponent::IsContactNormalValid(FVector(
            std::numeric_limits<float>::infinity(), 0.0f, 1.0f)));
    TestFalse(TEXT("Zero hit normal is rejected"),
        UDiscFlightComponent::IsContactNormalValid(FVector::ZeroVector));
    TestTrue(TEXT("Finite nonzero hit normal is accepted"),
        UDiscFlightComponent::IsContactNormalValid(FVector::UpVector));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightGroundSupportLossTelemetrySyncTest,
    "DiscGolfTour.Physics.SolverBoundary.GroundSupportLossTelemetrySync",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightGroundSupportLossTelemetrySyncTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("GroundSupportLossTelemetryWorld"));
    if (!TestNotNull(TEXT("Ground-support-loss world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 0.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Ground-support-loss disc exists"), DiscActor)
        || !TestTrue(TEXT("Ground-support-loss disc initializes"),
            DiscActor->InitializeDisc(MakeBoundaryDisc(), nullptr))
        || !TestTrue(TEXT("Ground-support-loss flight launches"),
            DiscActor->Throw(MakeBoundaryRelease(337.0f))))
    {
        return false;
    }

    UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();
    if (!TestNotNull(TEXT("Ground-support-loss flight component exists"), Flight))
    {
        return false;
    }
    const int32 InitialTransitionCount = Flight->GetGroundTransitions().Num();
    FString TransitionError;
    TestTrue(TEXT("Production support-loss transition validates at its immediate boundary"),
        Flight->TriggerGroundSupportLossForTesting(
            Flight->FixedStepSeconds, TransitionError));
    TestTrue(TEXT("Support-loss transition reports no validation error"),
        TransitionError.IsEmpty());
    TestEqual(TEXT("Support-loss solver state returns to Flying"),
        Flight->GetFlightState(), EDiscFlightState::Flying);
    TestEqual(TEXT("Support-loss telemetry state returns to Flying atomically"),
        Flight->GetTelemetry().State, EDiscFlightState::Flying);
    TestEqual(TEXT("Support-loss telemetry ground state returns to Airborne atomically"),
        Flight->GetTelemetry().GroundState, EDiscGroundState::Airborne);
    TestFalse(TEXT("A coherent support-loss transition does not terminate invalidly"),
        Flight->DidLastFlightTerminateInvalidly());
    TestEqual(TEXT("Support loss records exactly one ground transition"),
        Flight->GetGroundTransitions().Num(), InitialTransitionCount + 1);
    if (Flight->GetGroundTransitions().Num() > InitialTransitionCount)
    {
        const FDiscGroundTransition& Transition = Flight->GetGroundTransitions().Last();
        TestEqual(TEXT("Support loss records Sliding as the previous ground state"),
            Transition.FromState, EDiscGroundState::Sliding);
        TestEqual(TEXT("Support loss records Airborne as the resulting ground state"),
            Transition.ToState, EDiscGroundState::Airborne);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightExternalOwnerTransformMutationTest,
    "DiscGolfTour.Physics.SolverBoundary.ExternalOwnerTransformMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightExternalOwnerTransformMutationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("ExternalOwnerTransformWorld"));
    if (!TestNotNull(TEXT("External-transform test world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    const FResolvedDiscDefinition Disc = MakeBoundaryDisc();
    const FThrowRelease Release = MakeBoundaryRelease(359.0f);
    ADiscActor* TeleportDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, -500.0f, 2000.0f), FRotator::ZeroRotator);
    ADiscActor* RotationDisc = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 500.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("External-teleport disc exists"), TeleportDisc)
        || !TestNotNull(TEXT("External-rotation disc exists"), RotationDisc)
        || !TestTrue(TEXT("External-teleport disc initializes"),
            TeleportDisc->InitializeDisc(Disc, nullptr))
        || !TestTrue(TEXT("External-rotation disc initializes"),
            RotationDisc->InitializeDisc(Disc, nullptr))
        || !TestTrue(TEXT("External-teleport flight launches"),
            TeleportDisc->Throw(Release))
        || !TestTrue(TEXT("External-rotation flight launches"),
            RotationDisc->Throw(Release)))
    {
        return false;
    }

    UDiscFlightComponent* TeleportFlight = TeleportDisc->GetFlightComponent();
    UDiscFlightComponent* RotationFlight = RotationDisc->GetFlightComponent();
    if (!TestNotNull(TEXT("External-teleport flight exists"), TeleportFlight)
        || !TestNotNull(TEXT("External-rotation flight exists"), RotationFlight))
    {
        return false;
    }
    const FTransform AcceptedTeleportTransform = TeleportDisc->GetActorTransform();
    const FTransform AcceptedRotationTransform = RotationDisc->GetActorTransform();

    AddExpectedError(
        TEXT("owner transform changed outside authoritative flight integration"),
        EAutomationExpectedErrorFlags::Contains, 2);
    TestTrue(TEXT("Finite external teleport applies before solver validation"),
        TeleportDisc->SetActorLocation(
            AcceptedTeleportTransform.GetLocation() + FVector(125.0f, 0.0f, 0.0f),
            false, nullptr, ETeleportType::TeleportPhysics));
    TeleportFlight->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("External teleport terminates authoritative flight"),
        TeleportFlight->IsFlying());
    TestEqual(TEXT("External teleport settles fail-closed flight"),
        TeleportFlight->GetFlightState(), EDiscFlightState::Settled);
    TestTrue(TEXT("External teleport restores the last authoritative transform"),
        TeleportDisc->GetActorTransform().Equals(AcceptedTeleportTransform));
    TestEqual(TEXT("External teleport advances no solver time"),
        TeleportFlight->GetTelemetry().FlightTimeSeconds, 0.0f);

    const FQuat ExternalRotation = FQuat(FVector::UpVector, FMath::DegreesToRadians(30.0f))
        * AcceptedRotationTransform.GetRotation();
    TestTrue(TEXT("Finite external rotation applies before solver validation"),
        RotationDisc->SetActorRotation(
            ExternalRotation, ETeleportType::TeleportPhysics));
    RotationFlight->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("External rotation terminates authoritative flight"),
        RotationFlight->IsFlying());
    TestEqual(TEXT("External rotation settles fail-closed flight"),
        RotationFlight->GetFlightState(), EDiscFlightState::Settled);
    TestTrue(TEXT("External rotation restores the last authoritative transform"),
        RotationDisc->GetActorTransform().Equals(AcceptedRotationTransform));
    TestEqual(TEXT("External rotation advances no solver time"),
        RotationFlight->GetTelemetry().FlightTimeSeconds, 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightDeferredOverlapTransformLatchTest,
    "DiscGolfTour.Physics.SolverBoundary.DeferredOverlapTransformLatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightDeferredOverlapTransformLatchTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("DeferredOverlapTransformWorld"));
    if (!TestNotNull(TEXT("Deferred-overlap test world exists"), World)) return false;
    if (!TestNotNull(TEXT("Deferred-overlap test engine exists"), GEngine))
    {
        World->DestroyWorld(false);
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        if (World->HasBegunPlay())
        {
            World->EndPlay(EEndPlayReason::Quit);
        }
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };

    World->InitializeActorsForPlay(FURL());
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!TestNotNull(TEXT("Deferred-overlap world settings exist"), WorldSettings))
    {
        return false;
    }
    WorldSettings->NotifyBeginPlay();

    // The default vegetation box begins at X=20 cm. The launched disc starts
    // outside it and crosses that boundary during its first authoritative
    // swept step, forcing the production BeginOverlap callback path.
    ADiscGolfVegetationInteractionActor* Interaction =
        World->SpawnActor<ADiscGolfVegetationInteractionActor>(
            FVector(260.0f, 0.0f, 2000.0f), FRotator::ZeroRotator);
    ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 0.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Deferred-overlap vegetation exists"), Interaction)
        || !TestNotNull(TEXT("Deferred-overlap disc exists"), DiscActor))
    {
        return false;
    }
    Interaction->UpdateOverlaps(false);
    DiscActor->UpdateOverlaps(false);
    TestFalse(TEXT("Disc starts outside the vegetation interaction volume"),
        Interaction->IsOverlappingActor(DiscActor));
    if (!TestTrue(TEXT("Deferred-overlap disc initializes"),
            DiscActor->InitializeDisc(MakeBoundaryDisc(), nullptr))
        || !TestTrue(TEXT("Deferred-overlap flight launches"),
            DiscActor->Throw(MakeBoundaryRelease(433.0f))))
    {
        return false;
    }

    UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();
    if (!TestNotNull(TEXT("Deferred-overlap flight component exists"), Flight))
    {
        return false;
    }
    const float PreContactSpeedMps = Flight->GetVelocityMps().Size();
    for (int32 Frame = 0;
        Frame < 4 && Flight->IsFlying()
            && Flight->GetTelemetry().FixtureContactCount == 0;
        ++Frame)
    {
        Flight->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    }

    TestTrue(TEXT("Deferred overlap callback does not terminate authoritative flight"),
        Flight->IsFlying());
    TestEqual(TEXT("Crossing the interaction boundary records exactly one fixture contact"),
        Flight->GetTelemetry().FixtureContactCount, 1);
    TestTrue(TEXT("The deferred vegetation callback applies its authored drag"),
        Flight->GetVelocityMps().Size() < PreContactSpeedMps);

    // Deferring engine callbacks must not loosen the regular between-step
    // equality guard: an unrelated finite teleport still fails closed and is
    // restored to the last solver-owned transform.
    const FTransform AcceptedTransform = DiscActor->GetActorTransform();
    AddExpectedError(
        TEXT("owner transform changed outside authoritative flight integration"),
        EAutomationExpectedErrorFlags::Contains, 1);
    TestTrue(TEXT("External teleport still applies before boundary validation"),
        DiscActor->SetActorLocation(
            AcceptedTransform.GetLocation() + FVector(125.0f, 0.0f, 0.0f),
            false, nullptr, ETeleportType::TeleportPhysics));
    Flight->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("External teleport still terminates the flight"), Flight->IsFlying());
    TestTrue(TEXT("External teleport restores the last solver-owned transform"),
        DiscActor->GetActorTransform().Equals(AcceptedTransform));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightBasketBoundaryTest,
    "DiscGolfTour.Physics.SolverBoundary.BasketContact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightBasketBoundaryTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("BasketBoundaryWorld"));
    if (!TestNotNull(TEXT("Basket boundary world exists"), World)) return false;
    ON_SCOPE_EXIT
    {
        World->DestroyWorld(false);
    };

    ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 0.0f, 2000.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Basket boundary disc exists"), DiscActor)) return false;
    TestTrue(TEXT("Basket boundary disc initializes"),
        DiscActor->InitializeDisc(MakeBoundaryDisc(), nullptr));
    TestTrue(TEXT("Basket boundary flight launches"),
        DiscActor->Throw(MakeBoundaryRelease(401.0f)));
    UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();

    const FTransform AcceptedTransform = DiscActor->GetActorTransform();
    const FVector AcceptedVelocity = Flight->GetVelocityMps();
    const int32 AcceptedSampleCount = Flight->GetTrajectorySamples().Num();
    const int32 AcceptedContactCount = Flight->GetTelemetry().BasketContactCount;
    const auto TestUnchanged = [this, DiscActor, Flight, AcceptedTransform,
        AcceptedVelocity, AcceptedSampleCount, AcceptedContactCount](const TCHAR* Context)
    {
        TestTrue(*FString::Printf(TEXT("%s preserves transform"), Context),
            DiscActor->GetActorTransform().Equals(AcceptedTransform));
        TestTrue(*FString::Printf(TEXT("%s preserves velocity"), Context),
            Flight->GetVelocityMps().Equals(AcceptedVelocity));
        TestEqual(*FString::Printf(TEXT("%s preserves samples"), Context),
            Flight->GetTrajectorySamples().Num(), AcceptedSampleCount);
        TestEqual(*FString::Printf(TEXT("%s preserves contact telemetry"), Context),
            Flight->GetTelemetry().BasketContactCount, AcceptedContactCount);
    };

    FBasketContactEvaluation Evaluation;
    Evaluation.Result = static_cast<EBasketContactResult>(255);
    TestFalse(TEXT("Unknown basket result is rejected"),
        Flight->ApplyBasketContact(Evaluation, FVector::ZeroVector));
    TestUnchanged(TEXT("Unknown basket result"));

    AddExpectedError(
        TEXT("Basket contact rejected because its evaluation/capture boundary is invalid"),
        EAutomationExpectedErrorFlags::Contains, 4);
    Evaluation = FBasketContactEvaluation();
    Evaluation.Result = EBasketContactResult::ChainDeflection;
    Evaluation.PredictedRadialCm = 20.0f;
    Evaluation.PredictedHeightCm = 110.0f;
    Evaluation.IncomingSpeedMps = std::numeric_limits<float>::quiet_NaN();
    Evaluation.DeflectedVelocityMps = FVector(1.0f, 0.0f, -0.5f);
    TestFalse(TEXT("Non-finite basket scalar is rejected"),
        Flight->ApplyBasketContact(Evaluation, FVector::ZeroVector));
    TestUnchanged(TEXT("Non-finite basket scalar"));

    Evaluation.IncomingSpeedMps = AcceptedVelocity.Size();
    Evaluation.DeflectedVelocityMps.X = std::numeric_limits<float>::infinity();
    TestFalse(TEXT("Non-finite basket deflection is rejected"),
        Flight->ApplyBasketContact(Evaluation, FVector::ZeroVector));
    TestUnchanged(TEXT("Non-finite basket deflection"));

    Evaluation.Result = EBasketContactResult::Caught;
    Evaluation.DeflectedVelocityMps = FVector::ZeroVector;
    const FVector InvalidCapture(
        std::numeric_limits<float>::quiet_NaN(), 0.0f, 100.0f);
    TestFalse(TEXT("Non-finite basket capture is rejected"),
        Flight->ApplyBasketContact(Evaluation, InvalidCapture));
    TestUnchanged(TEXT("Non-finite basket capture"));

    const FVector ExcessiveCapture = DiscActor->GetActorLocation()
        + FVector(251.0f, 0.0f, 0.0f);
    TestFalse(TEXT("Finite basket capture beyond the snap envelope is rejected"),
        Flight->ApplyBasketContact(Evaluation, ExcessiveCapture));
    TestUnchanged(TEXT("Out-of-envelope basket capture"));

    Evaluation.Result = EBasketContactResult::ChainDeflection;
    Evaluation.DeflectedVelocityMps = FVector(1.0f, 0.25f, -0.5f);
    TestTrue(TEXT("Finite basket deflection is accepted"),
        Flight->ApplyBasketContact(Evaluation, FVector::ZeroVector));
    TestTrue(TEXT("Accepted basket deflection commits velocity"),
        Flight->GetVelocityMps().Equals(Evaluation.DeflectedVelocityMps));
    TestEqual(TEXT("Accepted basket deflection increments telemetry once"),
        Flight->GetTelemetry().BasketContactCount, AcceptedContactCount + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDiscFlightSpawnInsideBasketAtLaunchTest,
    "DiscGolfTour.Physics.SolverBoundary.SpawnInsideBasketAtLaunch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDiscFlightSpawnInsideBasketAtLaunchTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    UWorld* World = MakeBoundaryWorld(TEXT("SpawnInsideBasketWorld"));
    if (!TestNotNull(TEXT("Spawn-inside basket world exists"), World)) return false;
    if (!TestNotNull(TEXT("Spawn-inside basket engine exists"), GEngine))
    {
        World->DestroyWorld(false);
        return false;
    }
    FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
    WorldContext.SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        if (World->HasBegunPlay())
        {
            World->EndPlay(EEndPlayReason::Quit);
        }
        World->DestroyWorld(false);
        GEngine->DestroyWorldContext(World);
    };

    // Dynamic actors only receive their production initial-overlap pass after
    // the world has entered play. A bare transient UWorld skips that lifecycle
    // seam and cannot represent a disc spawned inside an existing catch volume.
    World->InitializeActorsForPlay(FURL());
    AWorldSettings* WorldSettings = World->GetWorldSettings();
    if (!TestNotNull(TEXT("Spawn-inside basket world settings exist"), WorldSettings))
    {
        return false;
    }
    WorldSettings->NotifyBeginPlay();

    ABasketActor* Basket = World->SpawnActor<ABasketActor>(
        FVector::ZeroVector, FRotator::ZeroRotator);
    ADiscActor* DiscActor = World->SpawnActor<ADiscActor>(
        FVector(0.0f, 0.0f, 114.0f), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("Spawn-inside basket exists"), Basket)
        || !TestNotNull(TEXT("Spawn-inside disc exists"), DiscActor))
    {
        return false;
    }

    Basket->UpdateOverlaps(false);
    DiscActor->UpdateOverlaps(false);
    TestTrue(TEXT("Disc is cached inside the basket catch volume before launch"),
        Basket->IsOverlappingActor(DiscActor));
    TestTrue(TEXT("Spawn-inside disc initializes"),
        DiscActor->InitializeDisc(MakeBoundaryDisc(), nullptr));
    TestFalse(TEXT("An overlapping idle disc cannot resolve contact"),
        Basket->EvaluateOverlappingDiscContact(DiscActor));

    FThrowCommand Command;
    Command.MoldId = TEXT("BoundaryDisc");
    Command.Direction = FVector::ForwardVector;
    Command.Power01 = 0.40f;
    Command.LaunchAngleDeg = 0.0f;
    Command.ShotContext = EDiscShotContext::Circle1Putt;
    const FThrowRelease Release = DiscGolfMath::ResolveThrowRelease(Command);
    TestTrue(TEXT("Spawn-inside committed release launches"),
        DiscActor->Throw(Release));

    UDiscFlightComponent* Flight = DiscActor->GetFlightComponent();
    if (!TestNotNull(TEXT("Spawn-inside flight component exists"), Flight)) return false;
    TestTrue(TEXT("Disc is flying before the post-commit overlap seam"), Flight->IsFlying());
    TestEqual(TEXT("No basket contact precedes the post-commit overlap seam"),
        Flight->GetTelemetry().BasketContactCount, 0);
    TestEqual(TEXT("Launch remains at solver time zero"),
        Flight->GetTelemetry().FlightTimeSeconds, 0.0f);

    TestTrue(TEXT("Committed launch immediately evaluates its cached basket overlap"),
        Basket->EvaluateOverlappingDiscContact(DiscActor));
    const FDiscFlightTelemetry& Telemetry = Flight->GetTelemetry();
    TestEqual(TEXT("Basket contact remains at solver time zero"),
        Telemetry.FlightTimeSeconds, 0.0f);
    TestEqual(TEXT("Exactly one basket contact is recorded"),
        Telemetry.BasketContactCount, 1);
    TestEqual(TEXT("Center overlap is caught"),
        Telemetry.LastBasketContact, EBasketContactResult::Caught);
    TestEqual(TEXT("Catch resolves flight"),
        Flight->GetFlightState(), EDiscFlightState::Settled);
    TestFalse(TEXT("Caught disc is no longer flying"), Flight->IsFlying());
    TestTrue(TEXT("Disc snaps to the authoritative capture point"),
        DiscActor->GetActorLocation().Equals(FVector(0.0f, 0.0f, 101.0f)));
    for (const FDiscTrajectorySample& Sample : Flight->GetTrajectorySamples())
    {
        TestEqual(TEXT("Every immediate-catch sample stays at solver time zero"),
            Sample.TimeSeconds, 0.0f);
    }

    TestFalse(TEXT("A settled caught disc cannot be evaluated twice"),
        Basket->EvaluateOverlappingDiscContact(DiscActor));
    TestEqual(TEXT("Repeated overlap evaluation does not duplicate contact telemetry"),
        Flight->GetTelemetry().BasketContactCount, 1);
    return true;
}

#endif
