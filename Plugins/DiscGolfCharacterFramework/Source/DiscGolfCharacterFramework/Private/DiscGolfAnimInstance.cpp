#include "DiscGolfAnimInstance.h"
#include "DiscGolfThrowComponent.h"
#include "DiscGolfCharacterProfile.h"
#include "GameFramework/Actor.h"

void UDiscGolfAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    AActor* Owner = GetOwningActor();
    if (!Owner)
    {
        bThrowActive = false;
        bDiscReleased = false;
        ThrowPhase = EDGThrowPhase::Idle;
        return;
    }

    const UDiscGolfThrowComponent* ThrowComponent = Owner->FindComponentByClass<UDiscGolfThrowComponent>();
    if (!ThrowComponent)
    {
        bThrowActive = false;
        bDiscReleased = false;
        ThrowPhase = EDGThrowPhase::Idle;
        return;
    }

    ThrowIntent = ThrowComponent->CurrentIntent;
    ThrowPhase = ThrowComponent->CurrentPhase;
    bThrowActive = ThrowComponent->bThrowActive;
    bDiscReleased = ThrowComponent->bDiscReleased;

    if (ThrowComponent->CharacterProfile)
    {
        BodyProfile = ThrowComponent->CharacterProfile->Body;
        ThrowStyle = ThrowComponent->CharacterProfile->ThrowStyle;
        Handedness = ThrowComponent->CharacterProfile->Handedness;
    }
}
