#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DiscGolfTypes.h"
#include "DiscActor.generated.h"

class UStaticMeshComponent;
class UDiscFlightComponent;
class USpringArmComponent;
class UCameraComponent;
class UPrimitiveComponent;
class AWindDirector;
class ADiscActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDiscSettledSignature, ADiscActor*, Disc, FVector, FinalLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscHoledOutSignature, ADiscActor*, Disc);

UCLASS()
class DISCGOLFTOUR_API ADiscActor : public AActor
{
    GENERATED_BODY()

public:
    ADiscActor();
    virtual void BeginPlay() override;

    UPROPERTY(BlueprintAssignable) FDiscSettledSignature OnDiscSettled;
    UPROPERTY(BlueprintAssignable) FDiscHoledOutSignature OnDiscHoledOut;

    UFUNCTION(BlueprintCallable) void InitializeDisc(const FResolvedDiscDefinition& InDisc, AWindDirector* InWindDirector);
    UFUNCTION(BlueprintCallable) void Throw(const FThrowRelease& Release);
    UFUNCTION(BlueprintCallable) void HoleOut();
    void ResolveBasketContact(const FBasketContactEvaluation& Evaluation, const FVector& CaptureWorldLocationCm);

    UFUNCTION(BlueprintPure) UDiscFlightComponent* GetFlightComponent() const { return FlightComponent; }
    UFUNCTION(BlueprintPure) FResolvedDiscDefinition GetResolvedDisc() const { return ResolvedDisc; }

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> DiscMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDiscFlightComponent> FlightComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> ChaseArm;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> ChaseCamera;
    FResolvedDiscDefinition ResolvedDisc;

    UFUNCTION() void HandleFlightSettled(FDiscFlightTelemetry Telemetry);
    UFUNCTION() void HandleDiscOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComponent,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);
};
