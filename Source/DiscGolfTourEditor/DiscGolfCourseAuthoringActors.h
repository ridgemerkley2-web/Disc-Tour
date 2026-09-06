#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.h"
#include "GameFramework/Actor.h"
#include "DiscGolfCourseAuthoringActors.generated.h"

class UArrowComponent;
class UBillboardComponent;
class USceneComponent;
class USplineComponent;

class ADiscGolfBasketAuthoringActor;
class ADiscGolfDropZoneAuthoringActor;
class ADiscGolfGameplayZoneAuthoringActor;
class ADiscGolfMandoGateAuthoringActor;
class ADiscGolfTeeAuthoringActor;

/** Value-only marker record used by deterministic editor conversion and tests. */
struct DISCGOLFTOUREDITOR_API FDiscGolfAuthoringMarkerRecord
{
    FName StableId = NAME_None;
    FTransform WorldTransform = FTransform::Identity;
};

/** Value-only mando record; the root resolves DropZoneId after stable sorting. */
struct DISCGOLFTOUREDITOR_API FDiscGolfAuthoringMandoRecord
{
    FName StableId = NAME_None;
    FVector GatePointAWorldCm = FVector::ZeroVector;
    FVector GatePointBWorldCm = FVector::ZeroVector;
    FVector RequiredPassDirectionWorld = FVector::ForwardVector;
    int32 MissPenaltyStrokes = 1;
    FName DropZoneId = NAME_None;
};

/** Value-only polygon/spline record used to keep conversion independent of Actors and Worlds. */
struct DISCGOLFTOUREDITOR_API FDiscGolfAuthoringZoneRecord
{
    FName StableId = NAME_None;
    EDGCourseZoneType ZoneType = EDGCourseZoneType::Rough;
    TArray<FVector> PolygonPointsWorldCm;
    int32 PenaltyStrokes = 0;
    bool bAffectsVegetation = true;
    bool bIsClosedLoop = true;
};

/** Complete value snapshot of one explicitly wired authoring root. */
struct DISCGOLFTOUREDITOR_API FDiscGolfHoleAuthoringSnapshot
{
    FName HoleId = NAME_None;
    int32 HoleNumber = 1;
    int32 Par = 3;
    float PublishedDistanceM = 0.0f;
    float ElevationChangeM = 0.0f;
    FTransform CourseOriginWorldTransform = FTransform::Identity;
    FDiscGolfAuthoringMarkerRecord Tee;
    FDiscGolfAuthoringMarkerRecord Basket;
    TArray<FDiscGolfAuthoringMarkerRecord> DropZones;
    TArray<FDiscGolfAuthoringMandoRecord> Mandos;
    TArray<FDiscGolfAuthoringZoneRecord> Zones;
};

/** Pure conversion functions shared by the placed actors and focused automation tests. */
namespace DiscGolfCourseAuthoring
{
    DISCGOLFTOUREDITOR_API FTransform ToCourseRelativeTransform(
        const FTransform& WorldTransform,
        const FTransform& CourseOriginWorldTransform);

    DISCGOLFTOUREDITOR_API FDGCourseZoneDefinition ToCourseZoneDefinition(
        const FDiscGolfAuthoringZoneRecord& Record,
        const FTransform& CourseOriginWorldTransform);

    DISCGOLFTOUREDITOR_API FDGMandoDefinition ToMandoDefinition(
        const FDiscGolfAuthoringMandoRecord& Record,
        const FTransform& CourseOriginWorldTransform,
        int32 DropZoneIndex);

    /**
     * Converts one immutable snapshot. Stable IDs are mandatory and unique per authored type.
     * Drop zones, mandos, and zones are sorted by ID before conversion, so Actor order is inert.
     */
    DISCGOLFTOUREDITOR_API bool ToHoleDefinition(
        const FDiscGolfHoleAuthoringSnapshot& Snapshot,
        FDGHoleDefinition& OutDefinition,
        FString& OutError);

    /** Bounds in course-origin-relative centimeters across all gameplay-defining geometry. */
    DISCGOLFTOUREDITOR_API FBox CalculateCourseRelativeBounds(const FDGHoleDefinition& Definition);
}

/** Common explicit identity for editor markers. Identity does not derive from Actor names. */
UCLASS(Abstract, BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfAuthoringMarkerActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfAuthoringMarkerActor();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Authoring")
    FName StableId = NAME_None;

    FDiscGolfAuthoringMarkerRecord MakeMarkerRecord() const;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    TObjectPtr<USceneComponent> AuthoringRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    TObjectPtr<UBillboardComponent> MarkerBillboard;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    TObjectPtr<UArrowComponent> ForwardArrow;
};

UCLASS(BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfTeeAuthoringActor final : public ADiscGolfAuthoringMarkerActor
{
    GENERATED_BODY()

public:
    ADiscGolfTeeAuthoringActor();
};

UCLASS(BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfBasketAuthoringActor final : public ADiscGolfAuthoringMarkerActor
{
    GENERATED_BODY()

public:
    ADiscGolfBasketAuthoringActor();
};

UCLASS(BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfDropZoneAuthoringActor final : public ADiscGolfAuthoringMarkerActor
{
    GENERATED_BODY()

public:
    ADiscGolfDropZoneAuthoringActor();
};

/** Visible two-post gate whose local points make resizing independent from world placement. */
UCLASS(BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfMandoGateAuthoringActor final : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfMandoGateAuthoringActor();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Authoring")
    FName StableId = TEXT("Mando_01");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    FVector GatePointALocalCm = FVector(0.0f, -500.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    FVector GatePointBLocalCm = FVector(0.0f, 500.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    FVector RequiredPassDirectionLocal = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring", meta=(ClampMin="0"))
    int32 MissPenaltyStrokes = 1;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Authoring")
    TObjectPtr<ADiscGolfDropZoneAuthoringActor> MissDropZone;

    FDiscGolfAuthoringMandoRecord MakeMandoRecord() const;

private:
    UPROPERTY(VisibleAnywhere, Category="Disc Golf|Authoring")
    TObjectPtr<USceneComponent> AuthoringRoot;

    UPROPERTY(VisibleAnywhere, Category="Disc Golf|Authoring")
    TObjectPtr<USplineComponent> GateSpline;

    UPROPERTY(VisibleAnywhere, Category="Disc Golf|Authoring")
    TObjectPtr<UArrowComponent> RequiredDirectionArrow;
};

/** Editable closed polygon or open spline converted to course-relative zone points. */
UCLASS(BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfGameplayZoneAuthoringActor final : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfGameplayZoneAuthoringActor();
    virtual void OnConstruction(const FTransform& Transform) override;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Authoring")
    FName StableId = TEXT("Zone_01");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    EDGCourseZoneType ZoneType = EDGCourseZoneType::Rough;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    int32 PenaltyStrokes = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    bool bAffectsVegetation = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    bool bClosedLoop = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Disc Golf|Authoring")
    TObjectPtr<USplineComponent> ZoneSpline;

    FDiscGolfAuthoringZoneRecord MakeZoneRecord() const;

private:
    UPROPERTY(VisibleAnywhere, Category="Disc Golf|Authoring")
    TObjectPtr<USceneComponent> AuthoringRoot;
};

/** Hole metadata and explicit typed references; conversion is read-only and never exports assets. */
UCLASS(BlueprintType)
class DISCGOLFTOUREDITOR_API ADiscGolfHoleAuthoringActor final : public AActor
{
    GENERATED_BODY()

public:
    ADiscGolfHoleAuthoringActor();

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Hole")
    FName HoleId = TEXT("Hole_01");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Hole", meta=(ClampMin="1"))
    int32 HoleNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Hole", meta=(ClampMin="1", ClampMax="12"))
    int32 Par = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Hole", meta=(ClampMin="0.0"))
    float PublishedDistanceM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Disc Golf|Hole")
    float ElevationChangeM = 0.0f;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Hole")
    TObjectPtr<ADiscGolfTeeAuthoringActor> Tee;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Hole")
    TObjectPtr<ADiscGolfBasketAuthoringActor> Basket;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Hole")
    TArray<TObjectPtr<ADiscGolfDropZoneAuthoringActor>> DropZones;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Hole")
    TArray<TObjectPtr<ADiscGolfMandoGateAuthoringActor>> Mandos;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Disc Golf|Hole")
    TArray<TObjectPtr<ADiscGolfGameplayZoneAuthoringActor>> Zones;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Hole Authoring")
    bool BuildHoleDefinition(FDGHoleDefinition& OutDefinition, FString& OutError) const;

    FDiscGolfHoleAuthoringSnapshot MakeSnapshot(FString& OutError) const;

private:
    UPROPERTY(VisibleAnywhere, Category="Disc Golf|Authoring")
    TObjectPtr<USceneComponent> CourseOrigin;

    UPROPERTY(VisibleAnywhere, Category="Disc Golf|Authoring")
    TObjectPtr<UBillboardComponent> HoleBillboard;
};
