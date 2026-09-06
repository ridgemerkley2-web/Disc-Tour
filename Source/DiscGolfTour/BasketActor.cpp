#include "BasketActor.h"
#include "DiscActor.h"
#include "DiscFlightComponent.h"
#include "DiscGolfMath.h"
#include "DiscGolfTour.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ABasketActor::ABasketActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    Pole = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pole"));
    Pole->SetupAttachment(Root);
    Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Pole->SetRelativeLocation(FVector(0, 0, 105));
    Pole->SetRelativeScale3D(FVector(0.04f, 0.04f, 1.8f));

    Tray = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tray"));
    Tray->SetupAttachment(Root);
    Tray->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tray->SetRelativeLocation(FVector(0, 0, 82));
    Tray->SetRelativeScale3D(FVector(0.70f, 0.70f, 0.05f));

    TopBand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TopBand"));
    TopBand->SetupAttachment(Root);
    TopBand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TopBand->SetRelativeLocation(FVector(0, 0, 145));
    TopBand->SetRelativeScale3D(FVector(0.56f, 0.56f, 0.06f));

    if (CylinderMesh.Succeeded())
    {
        Pole->SetStaticMesh(CylinderMesh.Object);
        Tray->SetStaticMesh(CylinderMesh.Object);
        TopBand->SetStaticMesh(CylinderMesh.Object);
    }

    for (int32 ChainIndex = 0; ChainIndex < 12; ++ChainIndex)
    {
        UStaticMeshComponent* Chain = CreateDefaultSubobject<UStaticMeshComponent>(
            *FString::Printf(TEXT("Chain%02d"), ChainIndex));
        Chain->SetupAttachment(Root);
        Chain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const float AngleRad = 2.0f * PI * static_cast<float>(ChainIndex) / 12.0f;
        Chain->SetRelativeLocation(FVector(FMath::Cos(AngleRad) * 23.0f, FMath::Sin(AngleRad) * 23.0f, 114.0f));
        Chain->SetRelativeScale3D(FVector(0.018f, 0.018f, 0.57f));
        if (CylinderMesh.Succeeded()) Chain->SetStaticMesh(CylinderMesh.Object);
        ChainStrands.Add(Chain);
    }

    CatchVolume = CreateDefaultSubobject<USphereComponent>(TEXT("CatchVolume"));
    CatchVolume->SetupAttachment(Root);
    CatchVolume->SetSphereRadius(52.0f);
    CatchVolume->SetRelativeLocation(FVector(0, 0, 114));
    CatchVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CatchVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
    CatchVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
    CatchVolume->OnComponentBeginOverlap.AddDynamic(this, &ABasketActor::OnCatchVolumeBeginOverlap);
}

void ABasketActor::OnCatchVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    ADiscActor* Disc = Cast<ADiscActor>(OtherActor);
    if (!Disc || !Disc->GetFlightComponent()) return;

    if (!Disc->GetFlightComponent()->IsFlying())
    {
        // Spawn-inside overlaps can arrive before LaunchThrow applies velocity.
        // GameMode rechecks that exact overlap immediately after its launch
        // transaction commits; a render-tick retry would be frame dependent.
        return;
    }

    EvaluateDiscContact(Disc);
}

bool ABasketActor::EvaluateOverlappingDiscContact(ADiscActor* Disc)
{
    return Disc
        && CatchVolume
        && CatchVolume->IsOverlappingActor(Disc)
        && EvaluateDiscContact(Disc);
}

bool ABasketActor::EvaluateDiscContact(ADiscActor* Disc)
{
    if (!Disc || !Disc->GetFlightComponent() || !Disc->GetFlightComponent()->IsFlying())
    {
        return false;
    }

    const FVector BasketBase = GetActorLocation();
    const FVector RelativeLocation = Disc->GetActorLocation() - BasketBase;
    const FBasketContactEvaluation Evaluation = DiscGolfMath::EvaluateBasketContact(
        RelativeLocation, Disc->GetFlightComponent()->GetVelocityMps());
    if (Evaluation.Result == EBasketContactResult::None) return false;

    UE_LOG(LogDiscGolfTour, Display,
        TEXT("Basket contact result %d: speed %.2f m/s, predicted radius %.1f cm, height %.1f cm."),
        static_cast<int32>(Evaluation.Result), Evaluation.IncomingSpeedMps,
        Evaluation.PredictedRadialCm, Evaluation.PredictedHeightCm);
    Disc->ResolveBasketContact(Evaluation, BasketBase + FVector(0.0f, 0.0f, 101.0f));
    return true;
}
