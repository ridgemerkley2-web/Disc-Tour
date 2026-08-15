#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfThrowComponent.generated.h"

class UDiscGolfCharacterProfile;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDGOnDiscRelease, FDGReleaseData, ReleaseData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDGOnThrowPhaseChanged, EDGThrowPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDGOnThrowFinished);

UCLASS(ClassGroup=(DiscGolf), meta=(BlueprintSpawnableComponent))
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfThrowComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDiscGolfThrowComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Disc Golf|Character")
    TObjectPtr<UDiscGolfCharacterProfile> CharacterProfile;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    FDGThrowIntent CurrentIntent;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    EDGThrowPhase CurrentPhase = EDGThrowPhase::Idle;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    bool bThrowActive = false;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    bool bDiscReleased = false;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Throw")
    FDGOnDiscRelease OnDiscRelease;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Throw")
    FDGOnThrowPhaseChanged OnThrowPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category="Disc Golf|Throw")
    FDGOnThrowFinished OnThrowFinished;

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Throw")
    void SetThrowIntent(const FDGThrowIntent& NewIntent);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Throw")
    void BeginThrow();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Throw")
    void CancelThrow();

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Throw")
    void NotifyThrowPhase(EDGThrowPhase NewPhase);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Throw")
    void NotifyDiscRelease(USkeletalMeshComponent* MeshComp);

    UFUNCTION(BlueprintCallable, Category="Disc Golf|Throw")
    void NotifyThrowFinished();

    UFUNCTION(BlueprintPure, Category="Disc Golf|Throw")
    FName GetActiveDiscGripBone() const;

private:
    FVector LastGripWorldLocation = FVector::ZeroVector;
    double LastGripSampleTimeSeconds = -1.0;
};
