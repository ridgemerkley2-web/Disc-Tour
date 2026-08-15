#include "DiscGolfWorldFixtureActor.h"

#include "DiscGolfCourseDefinition.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

namespace
{
FName FixtureTag(EDiscGolfFixtureType Type)
{
    switch (Type)
    {
        case EDiscGolfFixtureType::Tree: return TEXT("CourseFixture.Tree");
        case EDiscGolfFixtureType::DenseGrass: return TEXT("CourseFixture.DenseGrass");
        case EDiscGolfFixtureType::Rock: return TEXT("CourseFixture.Rock");
        case EDiscGolfFixtureType::Sign: return TEXT("CourseFixture.Sign");
        case EDiscGolfFixtureType::Unknown:
        default: return TEXT("CourseFixture.Unknown");
    }
}
}

ADiscGolfWorldFixtureActor::ADiscGolfWorldFixtureActor()
{
    PrimaryActorTick.bCanEverTick = false;
    GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
    OverlapVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("OverlapVolume"));
    OverlapVolume->SetupAttachment(GetStaticMeshComponent());
    OverlapVolume->SetBoxExtent(FVector(50.0f));
    OverlapVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    OverlapVolume->SetGenerateOverlapEvents(false);
}

void ADiscGolfWorldFixtureActor::Configure(
    const FDiscGolfCollisionFixtureDefinition& Definition,
    UStaticMesh* Mesh)
{
    FixtureId = Definition.FixtureId;
    FixtureType = Definition.FixtureType;
    SetActorLocationAndRotation(Definition.LocationCm, Definition.Rotation);
    SetActorScale3D(Definition.Scale);
    GetStaticMeshComponent()->SetStaticMesh(Mesh);
    GetStaticMeshComponent()->SetCollisionObjectType(ECC_WorldStatic);
    GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Block);

    if (IsOverlapFixture())
    {
        // Keep the display mesh out of gameplay collision. Basic-shape meshes can asynchronously
        // restore their body setup, so a dedicated primitive owns the invariant overlap contract.
        GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
        GetStaticMeshComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
        GetStaticMeshComponent()->SetGenerateOverlapEvents(false);
        GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        OverlapVolume->SetCollisionObjectType(ECC_WorldStatic);
        OverlapVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
        OverlapVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
        OverlapVolume->SetGenerateOverlapEvents(true);
        OverlapVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    }
    else
    {
        OverlapVolume->SetGenerateOverlapEvents(false);
        OverlapVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        GetStaticMeshComponent()->SetGenerateOverlapEvents(false);
    }

    Tags.AddUnique(TEXT("CourseFixture"));
    Tags.AddUnique(FixtureTag(FixtureType));
}

void ADiscGolfWorldFixtureActor::ConfigureTree(
    FName InFixtureId,
    const FVector& LocationCm,
    float HeightScale,
    UStaticMesh* Mesh)
{
    FDiscGolfCollisionFixtureDefinition Definition;
    Definition.FixtureId = InFixtureId;
    Definition.FixtureType = EDiscGolfFixtureType::Tree;
    Definition.Shape = EDiscGolfPrimitiveShape::Cylinder;
    Definition.LocationCm = LocationCm + FVector(0.0f, 0.0f, 250.0f * HeightScale);
    Definition.Scale = FVector(0.42f, 0.42f, 5.0f * HeightScale);
    Configure(Definition, Mesh);
}

bool ADiscGolfWorldFixtureActor::IsOverlapFixture() const
{
    return FixtureType == EDiscGolfFixtureType::DenseGrass;
}

bool ADiscGolfWorldFixtureActor::HasValidCollisionContract() const
{
    const UStaticMeshComponent* Mesh = GetStaticMeshComponent();
    if (!Mesh || !OverlapVolume || FixtureId.IsNone() || FixtureType == EDiscGolfFixtureType::Unknown) return false;
    if (IsOverlapFixture())
    {
        return Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Ignore
            && !Mesh->GetGenerateOverlapEvents()
            && OverlapVolume->GetCollisionEnabled() == ECollisionEnabled::QueryOnly
            && OverlapVolume->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Overlap
            && OverlapVolume->GetGenerateOverlapEvents();
    }
    return Mesh->GetCollisionEnabled() == ECollisionEnabled::QueryAndPhysics
        && Mesh->GetCollisionResponseToChannel(ECC_WorldDynamic) == ECR_Block;
}
