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
struct DISCGOLFRUNTIMEFOUNDATION_API FDGCourseZoneDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ZoneId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGCourseZoneType ZoneType = EDGCourseZoneType::Rough;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FVector> PolygonPointsCm;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PenaltyStrokes = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAffectsVegetation = true;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGMandoDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName MandoId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector GatePointACm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector GatePointBCm = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector RequiredPassDirection = FVector::ForwardVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MissPenaltyStrokes = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DropZoneIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGHoleDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HoleId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 HoleNumber = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Par = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PublishedDistanceM = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ElevationChangeM = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform TeeTransform = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FTransform BasketTransform = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTransform> DropZoneTransforms;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGMandoDefinition> Mandos;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FDGCourseZoneDefinition> Zones;
};

USTRUCT(BlueprintType)
struct DISCGOLFRUNTIMEFOUNDATION_API FDGCourseValidationIssue
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDGValidationSeverity Severity = EDGValidationSeverity::Info;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FName Code = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 HoleNumber = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Message;
};
