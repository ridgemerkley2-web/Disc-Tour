#include "DiscGolfWaterPresentationActor.h"

#include "DiscGolfCourseDefinition.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
    FString ToObjectPath(const FString& AssetPath)
    {
        const int32 SlashIndex = AssetPath.Find(TEXT("/"), ESearchCase::CaseSensitive,
            ESearchDir::FromEnd);
        const FString AssetName = SlashIndex == INDEX_NONE
            ? AssetPath : AssetPath.Mid(SlashIndex + 1);
        return AssetName.Contains(TEXT("."), ESearchCase::CaseSensitive)
            ? AssetPath : FString::Printf(TEXT("%s.%s"), *AssetPath, *AssetName);
    }
}

ADiscGolfWaterPresentationActor::ADiscGolfWaterPresentationActor()
{
    PrimaryActorTick.bCanEverTick = false;

    WaterMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WaterMesh"));
    SetRootComponent(WaterMesh);
    WaterMesh->SetMobility(EComponentMobility::Movable);
    WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterMesh->SetGenerateOverlapEvents(false);
    WaterMesh->SetCanEverAffectNavigation(false);
    WaterMesh->SetCastShadow(false);
    Tags.AddUnique(TEXT("Presentation.Surface.Water.Visual"));
}

bool ADiscGolfWaterPresentationActor::Configure(
    const FDiscGolfBlockoutSurfaceDefinition& Surface,
    const FString& WaterMaterialPath)
{
    bReady = false;
    TriangleCount = 0;
    if (!WaterMesh || Surface.Shape != EDiscGolfPrimitiveShape::Cylinder)
    {
        return false;
    }

    WaterMesh->ClearAllMeshSections();
    WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterMesh->SetGenerateOverlapEvents(false);
    WaterMesh->SetCanEverAffectNavigation(false);
    SetActorLocation(Surface.LocationCm);
    SetActorRotation(Surface.Rotation);

    UMaterialInterface* Material = LoadObject<UMaterialInterface>(
        nullptr, *ToObjectPath(WaterMaterialPath));
    if (!Material)
    {
        return false;
    }

    constexpr int32 AngularSegments = 128;
    constexpr int32 RadialSegments = 12;
    const float RadiusX = FMath::Max(100.0f, FMath::Abs(Surface.Scale.X) * 50.0f);
    const float RadiusY = FMath::Max(100.0f, FMath::Abs(Surface.Scale.Y) * 50.0f);
    // The presentation plane sits just above the authoritative cylinder top to avoid z-fighting.
    const float SurfaceZ = FMath::Abs(Surface.Scale.Z) * 50.0f + 1.5f;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FColor> Colors;
    TArray<FProcMeshTangent> Tangents;
    Vertices.Reserve(1 + RadialSegments * (AngularSegments + 1));
    Triangles.Reserve(AngularSegments * (1 + 2 * (RadialSegments - 1)) * 3);

    Vertices.Add(FVector(0.0f, 0.0f, SurfaceZ));
    Normals.Add(FVector::UpVector);
    UVs.Add(FVector2D(0.5f, 0.5f));
    Colors.Add(FColor(0, 0, 0, 255));
    Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));

    const auto RingVertex = [=](int32 Ring, int32 Segment)
    {
        return 1 + (Ring - 1) * (AngularSegments + 1) + Segment;
    };

    for (int32 Ring = 1; Ring <= RadialSegments; ++Ring)
    {
        const float RadiusAlpha = static_cast<float>(Ring) / RadialSegments;
        const float Shallow = FMath::SmoothStep(0.42f, 1.0f, RadiusAlpha);
        const float Shore = FMath::SmoothStep(0.87f, 1.0f, RadiusAlpha);
        for (int32 Segment = 0; Segment <= AngularSegments; ++Segment)
        {
            const float Angle = 2.0f * PI * static_cast<float>(Segment) / AngularSegments;
            const float UnitX = FMath::Cos(Angle);
            const float UnitY = FMath::Sin(Angle);
            const float AngularWave = 0.5f + 0.5f * FMath::Sin(Angle * 3.0f + RadiusAlpha * 4.0f);
            Vertices.Add(FVector(UnitX * RadiusX * RadiusAlpha,
                UnitY * RadiusY * RadiusAlpha, SurfaceZ));
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(0.5f + 0.5f * UnitX * RadiusAlpha,
                0.5f + 0.5f * UnitY * RadiusAlpha));
            Colors.Add(FColor(
                static_cast<uint8>(FMath::RoundToInt(Shallow * 255.0f)),
                static_cast<uint8>(FMath::RoundToInt(Shore * 255.0f)),
                static_cast<uint8>(FMath::RoundToInt(AngularWave * 255.0f)), 255));
            Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
        }
    }

    for (int32 Segment = 0; Segment < AngularSegments; ++Segment)
    {
        Triangles.Add(0);
        Triangles.Add(RingVertex(1, Segment));
        Triangles.Add(RingVertex(1, Segment + 1));
    }
    for (int32 Ring = 2; Ring <= RadialSegments; ++Ring)
    {
        for (int32 Segment = 0; Segment < AngularSegments; ++Segment)
        {
            const int32 InnerA = RingVertex(Ring - 1, Segment);
            const int32 InnerB = RingVertex(Ring - 1, Segment + 1);
            const int32 OuterA = RingVertex(Ring, Segment);
            const int32 OuterB = RingVertex(Ring, Segment + 1);
            Triangles.Add(InnerA);
            Triangles.Add(OuterA);
            Triangles.Add(OuterB);
            Triangles.Add(InnerA);
            Triangles.Add(OuterB);
            Triangles.Add(InnerB);
        }
    }

    WaterMesh->CreateMeshSection(
        0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    WaterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterMesh->SetGenerateOverlapEvents(false);
    WaterMesh->SetCanEverAffectNavigation(false);

    UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
    if (!Dynamic)
    {
        WaterMesh->ClearAllMeshSections();
        return false;
    }
    Dynamic->SetScalarParameterValue(TEXT("WaveAmplitudeCm"), 3.5f);
    Dynamic->SetScalarParameterValue(TEXT("WaveSpeed"), 0.16f);
    Dynamic->SetScalarParameterValue(TEXT("WaveTiling"), 14.0f);
    Dynamic->SetScalarParameterValue(TEXT("AngularWaveMix"), 2.6f);
    Dynamic->SetScalarParameterValue(TEXT("NormalStrength"), 0.065f);
    Dynamic->SetScalarParameterValue(TEXT("AmbientColorLift"), 0.12f);
    Dynamic->SetVectorParameterValue(TEXT("DeepWaterColor"),
        FLinearColor(0.006f, 0.040f, 0.055f, 1.0f));
    Dynamic->SetVectorParameterValue(TEXT("ShallowWaterColor"),
        FLinearColor(0.012f, 0.075f, 0.060f, 1.0f));
    Dynamic->SetVectorParameterValue(TEXT("ShoreTint"),
        FLinearColor(0.004f, 0.010f, 0.008f, 1.0f));
    WaterMesh->SetMaterial(0, Dynamic);

    TriangleCount = Triangles.Num() / 3;
    bReady = TriangleCount == AngularSegments * (1 + 2 * (RadialSegments - 1));
    if (!bReady)
    {
        WaterMesh->ClearAllMeshSections();
        TriangleCount = 0;
    }
    return bReady;
}

bool ADiscGolfWaterPresentationActor::IsCollisionInvariant() const
{
    return WaterMesh
        && WaterMesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision
        && !WaterMesh->GetGenerateOverlapEvents()
        && !WaterMesh->CanEverAffectNavigation();
}
