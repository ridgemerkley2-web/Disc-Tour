#include "DiscActor.h"
#include "DiscFlightComponent.h"
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
        DiscMesh->SetRelativeScale3D(FVector(0.21f, 0.21f, 0.015f));
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

void ADiscActor::InitializeDisc(const FResolvedDiscDefinition& InDisc, AWindDirector* InWindDirector)
{
    ResolvedDisc = InDisc;
    FlightComponent->ConfigureDisc(InDisc);
    FlightComponent->SetWindDirector(InWindDirector);
}

void ADiscActor::Throw(const FThrowRelease& Release)
{
    FlightComponent->Launch(Release);
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
    FlightComponent->ApplyBasketContact(Evaluation, CaptureWorldLocationCm);
    if (Evaluation.Result == EBasketContactResult::Caught)
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
