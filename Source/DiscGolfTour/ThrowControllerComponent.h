#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfTypes.h"
#include "ThrowControllerComponent.generated.h"

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFTOUR_API UThrowControllerComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UThrowControllerComponent();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    void AdjustPower(float AxisValue, float DeltaSeconds);
    void AdjustHyzer(float AxisValue, float DeltaSeconds);
    void AdjustNose(float AxisValue, float DeltaSeconds);
    void ToggleThrowStyle();
    void CancelTiming();
    void SetShotContext(EDiscShotContext NewContext, float DistanceToBasketMeters);

    bool HandleThrowPress(FName MoldId, EDiscPlastic Plastic, const FVector& Direction, FThrowCommand& OutCommand);

    UFUNCTION(BlueprintPure) float GetPower01() const { return Power01; }
    UFUNCTION(BlueprintPure) float GetHyzerDeg() const { return HyzerDeg; }
    UFUNCTION(BlueprintPure) float GetNoseDeg() const { return NoseDeg; }
    UFUNCTION(BlueprintPure) float GetTimingNeedle01() const { return TimingNeedle01; }
    UFUNCTION(BlueprintPure) float GetIdealTiming01() const { return IdealTiming01; }
    UFUNCTION(BlueprintPure) float GetTimingMissSpan01() const { return TimingMissSpan01; }
    UFUNCTION(BlueprintPure) bool IsTimingActive() const { return bTimingActive; }
    UFUNCTION(BlueprintPure) EThrowStyle GetThrowStyle() const { return ThrowStyle; }
    UFUNCTION(BlueprintPure) EDiscShotContext GetShotContext() const { return ShotContext; }
    UFUNCTION(BlueprintPure) float GetLaunchAngleDeg() const { return LaunchAngleDeg; }
    UFUNCTION(BlueprintPure) float GetPuttDistanceMeters() const { return PuttDistanceMeters; }
    UFUNCTION(BlueprintPure) float GetRecommendedPower01() const { return RecommendedPower01; }
    UFUNCTION(BlueprintPure) bool IsPutting() const { return ShotContext != EDiscShotContext::Drive; }

private:
    UPROPERTY(EditAnywhere, Category="Throw") float Power01 = 0.82f;
    UPROPERTY(EditAnywhere, Category="Throw") float HyzerDeg = 3.0f;
    UPROPERTY(EditAnywhere, Category="Throw") float NoseDeg = 1.0f;
    UPROPERTY(EditAnywhere, Category="Throw") float LaunchAngleDeg = 7.0f;
    UPROPERTY(EditAnywhere, Category="Throw") float TimingCycleSeconds = 1.30f;
    UPROPERTY(EditAnywhere, Category="Throw") float IdealTiming01 = 0.82f;
    UPROPERTY(EditAnywhere, Category="Throw") float TimingMissSpan01 = 0.18f;
    UPROPERTY(EditAnywhere, Category="Throw") EThrowStyle ThrowStyle = EThrowStyle::Backhand;
    UPROPERTY(VisibleAnywhere, Category="Throw") EDiscShotContext ShotContext = EDiscShotContext::Drive;

    bool bTimingActive = false;
    float TimingElapsed = 0.0f;
    float TimingNeedle01 = 0.0f;
    float PuttDistanceMeters = 0.0f;
    float RecommendedPower01 = 0.82f;
};
