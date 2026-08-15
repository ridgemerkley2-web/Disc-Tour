#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DiscGolfUIFlowSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FDGOnUIRouteChanged,
    FName, ActiveRoute,
    int32, StackDepth
);

UCLASS(BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfUIFlowSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|UI")
    TArray<FName> RouteStack;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|UI")
    FDGOnUIRouteChanged OnRouteChanged;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|UI")
    void PushRoute(FName Route);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|UI")
    bool PopRoute();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|UI")
    void ResetToRoute(FName Route);

    UFUNCTION(BlueprintPure, Category="Disc Golf|UI")
    FName GetActiveRoute() const;
};
