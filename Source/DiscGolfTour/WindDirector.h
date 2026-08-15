#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WindDirector.generated.h"

class ADiscGolfWindZoneActor;

UCLASS()
class DISCGOLFTOUR_API AWindDirector : public AActor
{
    GENERATED_BODY()

public:
    AWindDirector();
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wind") FVector BaseWindMps = FVector(2.5f, 0.8f, 0.0f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wind") float GustAmplitudeMps = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wind") float GustFrequencyHz = 0.12f;

    UFUNCTION(BlueprintPure, Category="Wind") FVector GetWindMpsAt(const FVector& WorldLocation) const;
    UFUNCTION(BlueprintCallable, Category="Wind") void RefreshCourseZones();
    UFUNCTION(BlueprintPure, Category="Wind") FName GetActiveZoneIdAt(const FVector& WorldLocation) const;
    UFUNCTION(BlueprintPure, Category="Wind") int32 GetCourseZoneCount() const { return CourseZones.Num(); }

private:
    float SimTime = 0.0f;
    UPROPERTY() TArray<TWeakObjectPtr<ADiscGolfWindZoneActor>> CourseZones;
};
