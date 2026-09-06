#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfAnimInstance.generated.h"

UCLASS(Transient, Blueprintable)
class DISCGOLFRUNTIMEFOUNDATION_API UDiscGolfAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGBodyProfile BodyProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDGThrowStyle ThrowStyle;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EDGHandedness Handedness = EDGHandedness::Right;

    /** Read-only bridge from the authoritative character throw component. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDGThrowIntent ThrowIntent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) EDGThrowPhase ThrowPhase = EDGThrowPhase::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bThrowActive = false;
};
