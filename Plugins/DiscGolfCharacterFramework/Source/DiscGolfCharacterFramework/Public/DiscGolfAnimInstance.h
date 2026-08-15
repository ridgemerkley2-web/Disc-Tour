#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DiscGolfCharacterTypes.h"
#include "DiscGolfAnimInstance.generated.h"

UCLASS(Blueprintable, BlueprintType)
class DISCGOLFCHARACTERFRAMEWORK_API UDiscGolfAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Character")
    FDGBodyProfile BodyProfile;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Character")
    FDGThrowStyle ThrowStyle;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Character")
    EDGHandedness Handedness = EDGHandedness::Right;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    FDGThrowIntent ThrowIntent;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    EDGThrowPhase ThrowPhase = EDGThrowPhase::Idle;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    bool bThrowActive = false;

    UPROPERTY(BlueprintReadOnly, Category="Disc Golf|Throw")
    bool bDiscReleased = false;
};
