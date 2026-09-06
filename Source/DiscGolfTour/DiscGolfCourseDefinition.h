#pragma once

#include "CoreMinimal.h"
#include "DiscGolfTypes.h"
#include "DiscGolfCourseDefinition.generated.h"

UENUM(BlueprintType)
enum class EDiscGolfPrimitiveShape : uint8
{
    Box,
    Cylinder,
    Sphere
};

UENUM(BlueprintType)
enum class EDiscGolfCameraAnchorMode : uint8
{
    Launch,
    Fairway,
    Finish
};

UENUM(BlueprintType)
enum class EDiscGolfShotRouteType : uint8
{
    Primary,
    RiskReward,
    Bailout
};

USTRUCT(BlueprintType)
struct FDiscGolfBlockoutSurfaceDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SurfaceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ECourseSurfaceType SurfaceType = ECourseSurfaceType::Fairway;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDiscGolfPrimitiveShape Shape = EDiscGolfPrimitiveShape::Box;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Scale = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FDiscGolfTreeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeightScale = 1.0f;
};

/** Authored, quality-invariant collision fixture. DenseGrass is an overlap volume; other types block. */
USTRUCT(BlueprintType)
struct FDiscGolfCollisionFixtureDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName FixtureId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDiscGolfFixtureType FixtureType = EDiscGolfFixtureType::Unknown;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDiscGolfPrimitiveShape Shape = EDiscGolfPrimitiveShape::Box;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Scale = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FDiscGolfLandingZoneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ZoneId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ExtentCm = FVector(1200.0f, 700.0f, 40.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
};

/** Designer-authored strategy corridor. It is metadata and never collision or flight authority. */
USTRUCT(BlueprintType)
struct FDiscGolfShotRouteDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RouteId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDiscGolfShotRouteType RouteType = EDiscGolfShotRouteType::Primary;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ShotIntent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LandingZoneId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TargetStrokes = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RiskRating = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RewardRating = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CorridorWidthCm = 1200.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> WaypointsCm;
};

USTRUCT(BlueprintType)
struct FDiscGolfCameraAnchorDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AnchorId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDiscGolfCameraAnchorMode Mode = EDiscGolfCameraAnchorMode::Launch;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float FieldOfViewDeg = 60.0f;
};

USTRUCT(BlueprintType)
struct FDiscGolfSpectatorBoundaryDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BoundaryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ExtentCm = FVector(1000.0f, 40.0f, 80.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
};

USTRUCT(BlueprintType)
struct FDiscGolfWindZoneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ZoneId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector LocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector ExtentCm = FVector(1500.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BaseWindScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector AdditiveWindMps = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FDiscGolfHoleBlockoutDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CourseId = TEXT("PineRidgeChampionship");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LayoutId = TEXT("Championship");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HoleNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText HoleName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Par = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector TeeLocationCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector BasketLocationCm = FVector(11000.0f, 800.0f, 0.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfBlockoutSurfaceDefinition> Surfaces;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfTreeDefinition> Trees;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfCollisionFixtureDefinition> CollisionFixtures;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfLandingZoneDefinition> LandingZones;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfShotRouteDefinition> ShotRoutes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfCameraAnchorDefinition> CameraAnchors;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfSpectatorBoundaryDefinition> SpectatorBoundaries;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfWindZoneDefinition> WindZones;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> FlyoverPointsCm;
};

USTRUCT(BlueprintType)
struct FDiscGolfCourseManifestHoleEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HoleNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DefinitionFile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector WorldOriginCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float WorldYawDeg = 0.0f;
};

USTRUCT(BlueprintType)
struct FDiscGolfCourseManifestDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SchemaVersion = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CourseId = TEXT("PineRidgeChampionship");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName LayoutId = TEXT("Championship");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDiscGolfCourseManifestHoleEntry> Holes;
};

/** Result of attempting to load the authored Pine Ridge manifest or hole definition. */
enum class EDiscGolfAuthoredCourseDataState : uint8
{
    Valid,
    Missing,
    Invalid
};

/** Runtime action selected after applying the build's authored-course data policy. */
enum class EDiscGolfAuthoredCourseLoadAction : uint8
{
    UseAuthoredData,
    UseSourceFallback,
    FailClosed
};

namespace DiscGolfCourseDefinition
{
    /** Shipping must never mask missing or invalid authored course data with source fallbacks. */
    DISCGOLFTOUR_API EDiscGolfAuthoredCourseLoadAction ResolveAuthoredCourseLoadAction(
        EDiscGolfAuthoredCourseDataState DataState,
        bool bIsShippingBuild);
    DISCGOLFTOUR_API FDiscGolfCourseManifestDefinition PineRidgeCourseFallback();
    DISCGOLFTOUR_API FDiscGolfHoleBlockoutDefinition PineRidgeHole1Fallback();
    DISCGOLFTOUR_API FDiscGolfHoleBlockoutDefinition PineRidgeHole2Fallback();
    DISCGOLFTOUR_API FDiscGolfHoleBlockoutDefinition PineRidgeHole3Fallback();
    DISCGOLFTOUR_API bool LoadPineRidgeCourseManifest(
        FDiscGolfCourseManifestDefinition& OutManifest,
        FString& OutSource,
        FString& OutError);
    DISCGOLFTOUR_API bool LoadPineRidgeHole(
        int32 HoleNumber,
        FDiscGolfHoleBlockoutDefinition& OutDefinition,
        FString& OutSource,
        FString& OutError);
    DISCGOLFTOUR_API bool LoadPineRidgeHole1(
        FDiscGolfHoleBlockoutDefinition& OutDefinition,
        FString& OutSource,
        FString& OutError);
    DISCGOLFTOUR_API bool ParseManifestJson(
        const FString& Json,
        FDiscGolfCourseManifestDefinition& OutManifest,
        FString& OutError);
    DISCGOLFTOUR_API bool ValidateManifest(
        const FDiscGolfCourseManifestDefinition& Manifest,
        FString& OutError);
    DISCGOLFTOUR_API bool PlaceHoleInCourse(
        const FDiscGolfHoleBlockoutDefinition& LocalDefinition,
        const FDiscGolfCourseManifestHoleEntry& ManifestEntry,
        FDiscGolfHoleBlockoutDefinition& OutWorldDefinition,
        FString& OutError);
    DISCGOLFTOUR_API bool ParseJson(
        const FString& Json,
        FDiscGolfHoleBlockoutDefinition& OutDefinition,
        FString& OutError);
    DISCGOLFTOUR_API bool Validate(
        const FDiscGolfHoleBlockoutDefinition& Definition,
        FString& OutError);
    /** Wind-zone actor identities share one persistent course world across holes. */
    DISCGOLFTOUR_API bool ValidateCourseWindZoneIdentities(
        const TArray<FDiscGolfHoleBlockoutDefinition>& Definitions,
        FString& OutError);
    DISCGOLFTOUR_API float MeasuredDistanceFeet(const FDiscGolfHoleBlockoutDefinition& Definition);
}
