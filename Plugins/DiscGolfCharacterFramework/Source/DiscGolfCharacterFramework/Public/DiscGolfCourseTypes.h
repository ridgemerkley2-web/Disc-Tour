#pragma once

#include "CoreMinimal.h"
#include "DiscGolfCourseTypes.generated.h"

UENUM(BlueprintType)
enum class EDGCourseZoneType : uint8
{
    TeeSafety,
    FairwayPrimary,
    FairwaySecondary,
    Rough,
    DeepRough,
    Green,
    OutOfBounds,
    WaterHazard,
    Spectator,
    NoSpawn
};

UENUM(BlueprintType)
enum class EDGValidationSeverity : uint8
{
    Info,
    Warning,
    Error
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGCourseZoneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    FName ZoneId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    EDGCourseZoneType ZoneType = EDGCourseZoneType::Rough;

    // Stored relative to the course origin/authoring actor.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    TArray<FVector> PolygonPointsCm;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    int32 PenaltyStrokes = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Zone")
    bool bAffectsVegetation = true;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGMandoDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mando")
    FName MandoId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mando")
    FVector GatePointACm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mando")
    FVector GatePointBCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mando")
    FVector RequiredPassDirection = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mando")
    int32 MissPenaltyStrokes = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mando")
    int32 DropZoneIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGHoleDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    FName HoleId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    int32 HoleNumber = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    int32 Par = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    FTransform TeeTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    FTransform BasketTransform = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    TArray<FTransform> DropZoneTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    TArray<FDGMandoDefinition> Mandos;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    TArray<FDGCourseZoneDefinition> Zones;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    float PublishedDistanceM = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hole")
    float ElevationChangeM = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGCourseValidationIssue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    EDGValidationSeverity Severity = EDGValidationSeverity::Info;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FName Code = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    int32 HoleNumber = 0;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FString Message;
};
