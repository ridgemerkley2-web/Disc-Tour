#pragma once

#include "CoreMinimal.h"
#include "DiscGolfDiscTypes.generated.h"

UENUM(BlueprintType)
enum class EDGDiscClass : uint8
{
    Putter,
    Approach,
    Midrange,
    FairwayDriver,
    DistanceDriver
};

UENUM(BlueprintType)
enum class EDGDiscWearStage : uint8
{
    New,
    Seasoned,
    Worn,
    VeryWorn
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGDiscFlightNumbers
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc", meta=(ClampMin="1.0", ClampMax="15.0"))
    float Speed = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc", meta=(ClampMin="1.0", ClampMax="7.0"))
    float Glide = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc", meta=(ClampMin="-5.0", ClampMax="1.0"))
    float Turn = -1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc", meta=(ClampMin="0.0", ClampMax="5.0"))
    float Fade = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGDiscGeometry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry", meta=(ClampMin="20.0", ClampMax="30.0"))
    float DiameterCm = 21.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry", meta=(ClampMin="1.0", ClampMax="3.5"))
    float HeightCm = 1.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry", meta=(ClampMin="0.5", ClampMax="3.0"))
    float RimWidthCm = 1.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry", meta=(ClampMin="0.8", ClampMax="2.5"))
    float RimDepthCm = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Geometry", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float Dome01 = 0.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGDiscAerodynamics
{
    GENERATED_BODY()

    // Calibration-facing coefficients. The existing flight model remains authoritative.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float LiftSlope = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float ZeroLiftAngleDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float BaseDragCoefficient = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float InducedDragFactor = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float PitchingMomentCoefficient = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float SpinDamping = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float HighSpeedTurnResponse = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    float LowSpeedFadeResponse = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aerodynamics")
    bool bCalibrated = false;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGPlasticPerformance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plastic", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Grip = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plastic", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Durability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plastic", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Stiffness = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Plastic", meta=(ClampMin="-1.0", ClampMax="1.0"))
    float StabilityOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground")
    float GroundFriction = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground")
    float SkipMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground")
    float RollingResistance = 1.0f;
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGDiscInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    FGuid InstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    FName DiscDefinitionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    FName PlasticId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    float MassGrams = 175.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Wear01 = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    FLinearColor Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    FName StampId = TEXT("premium_default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    FString Nickname;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Disc")
    bool bFavorite = false;

    EDGDiscWearStage GetWearStage() const
    {
        if (Wear01 < 0.20f) return EDGDiscWearStage::New;
        if (Wear01 < 0.50f) return EDGDiscWearStage::Seasoned;
        if (Wear01 < 0.80f) return EDGDiscWearStage::Worn;
        return EDGDiscWearStage::VeryWorn;
    }
};

USTRUCT(BlueprintType)
struct DISCGOLFCHARACTERFRAMEWORK_API FDGDiscBagLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Bag")
    FName BagEquipmentId = TEXT("premium_bag_default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Bag")
    int32 Capacity = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Bag")
    TArray<FDGDiscInstance> Discs;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Bag")
    FGuid SelectedDiscInstanceId;
};
