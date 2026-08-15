#include "DiscGolfFixturePresentationActor.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DiscGolfCourseDefinition.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Crc.h"

namespace
{
void ConfigurePresentationPrimitive(UPrimitiveComponent* Component)
{
    if (!Component) return;
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCollisionResponseToAllChannels(ECR_Ignore);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
}

bool IsCollisionFree(const UPrimitiveComponent* Component)
{
    return Component
        && Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision
        && !Component->GetGenerateOverlapEvents()
        && !Component->CanEverAffectNavigation();
}

FVector ComponentwiseScale(const FVector& Value, const FVector& Scale)
{
    return FVector(Value.X * Scale.X, Value.Y * Scale.Y, Value.Z * Scale.Z);
}
}

ADiscGolfFixturePresentationActor::ADiscGolfFixturePresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    BoulderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoulderMesh"));
    BoulderMesh->SetupAttachment(SceneRoot);
    BrushInstances = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("BrushInstances"));
    BrushInstances->SetupAttachment(SceneRoot);
    SignBoard = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignBoard"));
    SignBoard->SetupAttachment(SceneRoot);
    SignPostA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignPostA"));
    SignPostA->SetupAttachment(SceneRoot);
    SignPostB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SignPostB"));
    SignPostB->SetupAttachment(SceneRoot);
    SignText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignText"));
    SignText->SetupAttachment(SceneRoot);
    SignTextBack = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SignTextBack"));
    SignTextBack->SetupAttachment(SceneRoot);

    ConfigurePresentationPrimitive(BoulderMesh);
    ConfigurePresentationPrimitive(BrushInstances);
    ConfigurePresentationPrimitive(SignBoard);
    ConfigurePresentationPrimitive(SignPostA);
    ConfigurePresentationPrimitive(SignPostB);
    ConfigurePresentationPrimitive(SignText);
    ConfigurePresentationPrimitive(SignTextBack);
    HideAllVisuals();
}

void ADiscGolfFixturePresentationActor::HideAllVisuals()
{
    BoulderMesh->SetVisibility(false);
    BrushInstances->SetVisibility(false);
    SignBoard->SetVisibility(false);
    SignPostA->SetVisibility(false);
    SignPostB->SetVisibility(false);
    SignText->SetVisibility(false);
    SignTextBack->SetVisibility(false);
}

bool ADiscGolfFixturePresentationActor::Configure(
    const FDiscGolfCollisionFixtureDefinition& Definition,
    float GrassDensityScale,
    float CullDistanceScale)
{
    FixtureId = Definition.FixtureId;
    FixtureType = Definition.FixtureType;
    SetActorLocationAndRotation(Definition.LocationCm, Definition.Rotation);
    SetActorScale3D(FVector::OneVector);
    Tags.AddUnique(TEXT("Presentation.Fixture"));
    HideAllVisuals();

    switch (Definition.FixtureType)
    {
        case EDiscGolfFixtureType::Rock:
            return ConfigureBoulder(Definition);
        case EDiscGolfFixtureType::DenseGrass:
            return ConfigureBrush(Definition, GrassDensityScale, CullDistanceScale);
        case EDiscGolfFixtureType::Sign:
            return ConfigureSign(Definition);
        case EDiscGolfFixtureType::Tree:
        case EDiscGolfFixtureType::Unknown:
        default:
            return false;
    }
}

bool ADiscGolfFixturePresentationActor::ConfigureBoulder(
    const FDiscGolfCollisionFixtureDefinition& Definition)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Boulder01/boulder_01_1k.boulder_01_1k"));
    if (!Mesh) return false;

    BoulderMesh->SetStaticMesh(Mesh);
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const FVector DesiredExtent = Definition.Scale.GetAbs() * 50.0f * 0.92f;
    const FVector SafeExtent(
        FMath::Max(Bounds.BoxExtent.X, 1.0f),
        FMath::Max(Bounds.BoxExtent.Y, 1.0f),
        FMath::Max(Bounds.BoxExtent.Z, 1.0f));
    const FVector VisualScale(
        DesiredExtent.X / SafeExtent.X,
        DesiredExtent.Y / SafeExtent.Y,
        DesiredExtent.Z / SafeExtent.Z);
    BoulderMesh->SetRelativeScale3D(VisualScale);
    BoulderMesh->SetRelativeLocation(-ComponentwiseScale(Bounds.Origin, VisualScale));
    BoulderMesh->SetVisibility(true);
    return true;
}

bool ADiscGolfFixturePresentationActor::ConfigureBrush(
    const FDiscGolfCollisionFixtureDefinition& Definition,
    float GrassDensityScale,
    float CullDistanceScale)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Fixtures/PolyHaven/Shrub04/shrub_04_1k.shrub_04_1k"));
    if (!Mesh) return false;

    BrushInstances->ClearInstances();
    BrushInstances->SetStaticMesh(Mesh);
    BrushInstances->SetCullDistances(
        FMath::RoundToInt(4200.0f * FMath::Max(CullDistanceScale, 0.1f)),
        FMath::RoundToInt(11500.0f * FMath::Max(CullDistanceScale, 0.1f)));
    BrushInstances->SetVisibility(true);

    const float SafeDensity = FMath::Clamp(GrassDensityScale, 0.0f, 1.5f);
    const int32 TargetCount = SafeDensity <= 0.0f
        ? 0 : FMath::Clamp(FMath::RoundToInt(12.0f * SafeDensity), 3, 18);
    const FVector HalfExtent = Definition.Scale.GetAbs() * 50.0f;
    const FBoxSphereBounds Bounds = Mesh->GetBounds();
    const float MeshHeight = FMath::Max(Bounds.BoxExtent.Z * 2.0f, 1.0f);
    const FVector LocalMeshBase(
        Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z - Bounds.BoxExtent.Z);
    FRandomStream Random(static_cast<int32>(FCrc::StrCrc32(*Definition.FixtureId.ToString())));
    for (int32 Index = 0; Index < TargetCount; ++Index)
    {
        const float TargetHeightCm = HalfExtent.Z * 2.0f * Random.FRandRange(0.58f, 0.90f);
        const float UniformScale = TargetHeightCm / MeshHeight;
        const FRotator Rotation(0.0f, Random.FRandRange(-180.0f, 180.0f), 0.0f);
        const FVector DesiredBase(
            Random.FRandRange(-HalfExtent.X * 0.82f, HalfExtent.X * 0.82f),
            Random.FRandRange(-HalfExtent.Y * 0.82f, HalfExtent.Y * 0.82f),
            -HalfExtent.Z);
        const FVector Location = DesiredBase - Rotation.RotateVector(LocalMeshBase * UniformScale);
        BrushInstances->AddInstance(
            FTransform(Rotation, Location, FVector(UniformScale)), false);
    }
    return TargetCount > 0;
}

bool ADiscGolfFixturePresentationActor::ConfigureSign(
    const FDiscGolfCollisionFixtureDefinition& Definition)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Wood = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Presentation/Course/PineRidge/Materials/MI_PineRidgeSignWood.MI_PineRidgeSignWood"));
    if (!Cube || !Wood) return false;

    SignBoard->SetStaticMesh(Cube);
    SignBoard->SetRelativeScale3D(Definition.Scale);
    SignBoard->SetMaterial(0, Wood);
    SignBoard->SetVisibility(true);

    const float HalfWidthCm = Definition.Scale.Y * 50.0f;
    const float HalfHeightCm = Definition.Scale.Z * 50.0f;
    constexpr float PostHeightCm = 100.0f;
    constexpr float PostWidthCm = 8.0f;
    const float PostCenterZ = -HalfHeightCm + 10.0f - PostHeightCm * 0.5f;
    const float PostOffsetY = HalfWidthCm * 0.64f;
    for (UStaticMeshComponent* Post : { SignPostA, SignPostB })
    {
        Post->SetStaticMesh(Cube);
        Post->SetRelativeScale3D(FVector(PostWidthCm / 100.0f,
            PostWidthCm / 100.0f, PostHeightCm / 100.0f));
        Post->SetMaterial(0, Wood);
        Post->SetVisibility(true);
    }
    SignPostA->SetRelativeLocation(FVector(0.0f, -PostOffsetY, PostCenterZ));
    SignPostB->SetRelativeLocation(FVector(0.0f, PostOffsetY, PostCenterZ));

    const float TextOffsetX = Definition.Scale.X * 50.0f + 1.0f;
    for (UTextRenderComponent* Text : { SignText, SignTextBack })
    {
        const FString SignCopy = Definition.FixtureId == TEXT("TeeSponsorSign")
            ? TEXT("HOLE 1   PAR 3\n362 FT")
            : TEXT("PINE RIDGE\nCHAMPIONSHIP");
        Text->SetText(FText::FromString(SignCopy));
        Text->SetTextRenderColor(FColor(238, 230, 199));
        Text->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        Text->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
        Text->SetWorldSize(FMath::Clamp(HalfHeightCm * 0.25f, 14.0f, 26.0f));
        Text->SetVisibility(true);
    }
    SignText->SetRelativeLocation(FVector(TextOffsetX, 0.0f, 0.0f));
    SignTextBack->SetRelativeLocation(FVector(-TextOffsetX, 0.0f, 0.0f));
    SignTextBack->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
    return true;
}

bool ADiscGolfFixturePresentationActor::IsCollisionInvariant() const
{
    const UPrimitiveComponent* Components[] = {
        BoulderMesh, BrushInstances, SignBoard, SignPostA, SignPostB, SignText, SignTextBack
    };
    for (const UPrimitiveComponent* Component : Components)
    {
        if (!IsCollisionFree(Component)) return false;
    }
    return FixtureType != EDiscGolfFixtureType::Unknown && !FixtureId.IsNone();
}

int32 ADiscGolfFixturePresentationActor::GetBrushInstanceCount() const
{
    return BrushInstances ? BrushInstances->GetInstanceCount() : 0;
}
