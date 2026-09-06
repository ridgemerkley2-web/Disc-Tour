#include "DiscActor.h"
#include "DiscFlightComponent.h"
#include "DiscGolfMath.h"
#include "DiscGolfTour.h"
#include "WindDirector.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

ADiscActor::ADiscActor()
{
    PrimaryActorTick.bCanEverTick = false;

    DiscMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DiscMesh"));
    SetRootComponent(DiscMesh);
    DiscMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    DiscMesh->SetCollisionObjectType(ECC_WorldDynamic);
    DiscMesh->SetCollisionResponseToAllChannels(ECR_Block);
    DiscMesh->SetGenerateOverlapEvents(true);
    DiscMesh->SetSimulatePhysics(false);
    DiscMesh->bReturnMaterialOnMove = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (CylinderMesh.Succeeded())
    {
        DiscMesh->SetStaticMesh(CylinderMesh.Object);
        DiscMesh->SetRelativeScale3D(DiscGolfMath::CanonicalDiscCollisionScale());
    }

    FlightComponent = CreateDefaultSubobject<UDiscFlightComponent>(TEXT("FlightComponent"));

    ChaseArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm"));
    ChaseArm->SetupAttachment(DiscMesh);
    ChaseArm->TargetArmLength = 420.0f;
    ChaseArm->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
    ChaseArm->bDoCollisionTest = false;

    ChaseCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ChaseCamera"));
    ChaseCamera->SetupAttachment(ChaseArm);
}

void ADiscActor::BeginPlay()
{
    Super::BeginPlay();
    FlightComponent->OnFlightSettled.AddDynamic(this, &ADiscActor::HandleFlightSettled);
    DiscMesh->OnComponentBeginOverlap.AddDynamic(this, &ADiscActor::HandleDiscOverlap);
}

bool ADiscActor::InitializeDisc(const FResolvedDiscDefinition& InDisc, AWindDirector* InWindDirector)
{
    FString Error;
    if (!DiscGolfMath::IsResolvedDiscDefinitionValid(InDisc, &Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc actor initialization rejected: %s"), *Error);
        return false;
    }
    if (InWindDirector && !InWindDirector->ValidatePhysicsWindConfiguration(Error))
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc actor initialization rejected by wind configuration: %s"), *Error);
        return false;
    }
    if (!FlightComponent->ConfigureDisc(InDisc)
        || !FlightComponent->SetWindDirector(InWindDirector))
    {
        return false;
    }

    ResolvedDisc = InDisc;
    bDiscInitialized = true;
    return true;
}

bool ADiscActor::ConfigureLaunchingActorCollisionExclusion(AActor* LaunchingActor)
{
    if (!DiscMesh || !IsValid(LaunchingActor) || LaunchingActor == this
        || FlightComponent->IsFlying())
    {
        return false;
    }

    const auto& ExistingExclusions = DiscMesh->GetMoveIgnoreActors();
    if (!ExistingExclusions.IsEmpty()
        && (ExistingExclusions.Num() != 1
            || !ExistingExclusions.Contains(LaunchingActor)))
    {
        return false;
    }
    DiscMesh->IgnoreActorWhenMoving(LaunchingActor, true);
    return IsLaunchingActorCollisionExcluded(LaunchingActor);
}

bool ADiscActor::IsLaunchingActorCollisionExcluded(
    const AActor* LaunchingActor) const
{
    if (!DiscMesh || !IsValid(LaunchingActor))
    {
        return false;
    }
    const auto& Exclusions = DiscMesh->GetMoveIgnoreActors();
    return Exclusions.Num() == 1 && Exclusions.Contains(LaunchingActor);
}

bool ADiscActor::Throw(const FThrowRelease& Release)
{
    if (!bDiscInitialized)
    {
        UE_LOG(LogDiscGolfTour, Error,
            TEXT("Disc actor throw rejected because initialization never completed."));
        return false;
    }
    return FlightComponent->Launch(Release);
}

void ADiscActor::HoleOut()
{
    if (!FlightComponent->IsFlying()) return;
    FlightComponent->StopFlight(false);
    OnDiscHoledOut.Broadcast(this);
}

void ADiscActor::ResolveBasketContact(
    const FBasketContactEvaluation& Evaluation,
    const FVector& CaptureWorldLocationCm)
{
    if (!FlightComponent->IsFlying() || Evaluation.Result == EBasketContactResult::None) return;
    if (FlightComponent->ApplyBasketContact(Evaluation, CaptureWorldLocationCm)
        && Evaluation.Result == EBasketContactResult::Caught)
    {
        HoleOut();
    }
}

void ADiscActor::HandleFlightSettled(FDiscFlightTelemetry Telemetry)
{
    OnDiscSettled.Broadcast(this, GetActorLocation());
}

void ADiscActor::HandleDiscOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComponent,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    (void)OverlappedComponent;
    (void)OtherComponent;
    (void)OtherBodyIndex;
    (void)bFromSweep;
    (void)SweepResult;
    FlightComponent->ApplyFixtureOverlap(OtherActor);
}
